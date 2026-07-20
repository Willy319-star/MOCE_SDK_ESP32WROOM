#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "board.h"
#include "ch32_motor_gateway_final.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_BITRATE_TEXT           "50 kbit/s"
#define CAN_RX_QUEUE_LEN           32
#define CAN_TX_DONE_QUEUE_LEN      8
#define CAN_TX_QUEUE_DEPTH         8
#define CAN_CMD_TIMEOUT_MS         600
#define MAIN_LOOP_WAIT_MS          50
#define MOTOR_CMD_PERIOD_MS        1000
#define STATUS_PRINT_PERIOD_MS     3000

static const char *TAG = "motor_test0_final";

typedef struct {
    twai_frame_header_t header;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} can_rx_item_t;

typedef struct {
    bool success;
    uint32_t id;
} can_tx_done_item_t;

typedef struct {
    QueueHandle_t rx_queue;
    QueueHandle_t tx_done_queue;
} can_context_t;

typedef struct {
    bool online;
    ch32_motor_hello_t hello;
    TickType_t last_seen_tick;
} motor_node_state_t;

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;
static motor_node_state_t s_motor_nodes[CH32_MOTOR_NODE_ID_MAX + 1U];

static uint8_t find_motor_node(void)
{
    for (uint8_t id = 1U; id <= CH32_MOTOR_NODE_ID_MAX; ++id) {
        if (s_motor_nodes[id].online) {
            return id;
        }
    }
    return 0U;
}

static bool IRAM_ATTR on_rx_done(twai_node_handle_t handle,
                                 const twai_rx_done_event_data_t *edata,
                                 void *user_ctx)
{
    (void)edata;
    can_context_t *ctx = (can_context_t *)user_ctx;
    BaseType_t woken = pdFALSE;
    can_rx_item_t item = {0};
    twai_frame_t frame = {
        .buffer = item.data,
        .buffer_len = sizeof(item.data),
    };

    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        item.header = frame.header;
        if (xQueueSendFromISR(ctx->rx_queue, &item, &woken) != pdTRUE) {
            ESP_EARLY_LOGW(TAG, "RX queue full, dropping frame");
        }
    }

    return woken == pdTRUE;
}

static bool IRAM_ATTR on_tx_done(twai_node_handle_t handle,
                                 const twai_tx_done_event_data_t *edata,
                                 void *user_ctx)
{
    (void)handle;
    can_context_t *ctx = (can_context_t *)user_ctx;
    BaseType_t woken = pdFALSE;
    can_tx_done_item_t item = {
        .success = edata->is_tx_success,
        .id = edata->done_tx_frame != NULL ? edata->done_tx_frame->header.id : 0,
    };

    (void)xQueueSendFromISR(ctx->tx_done_queue, &item, &woken);
    return woken == pdTRUE;
}

static bool IRAM_ATTR on_error(twai_node_handle_t handle,
                               const twai_error_event_data_t *edata,
                               void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGW(TAG, "TWAI bus error flags=0x%" PRIx32, edata->err_flags.val);
    return false;
}

static bool IRAM_ATTR on_state_change(twai_node_handle_t handle,
                                      const twai_state_change_event_data_t *edata,
                                      void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGI(TAG, "TWAI state %d -> %d", edata->old_sta, edata->new_sta);
    return false;
}

static twai_node_handle_t start_can_node(void)
{
    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = BOARD_CAN_TX_GPIO,
            .rx = BOARD_CAN_RX_GPIO,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = CH32_MOTOR_CAN_BITRATE,
            .sp_permill = 800,
        },
        .timestamp_resolution_hz = 1000000,
        .fail_retry_cnt = 3,
        .tx_queue_depth = CAN_TX_QUEUE_DEPTH,
    };

    twai_node_handle_t node = NULL;
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node));

    twai_mask_filter_config_t accept_all_standard = {
        .id = 0,
        .mask = 0,
        .is_ext = false,
        .no_fd = true,
    };
    ESP_ERROR_CHECK(twai_node_config_mask_filter(node, 0, &accept_all_standard));

    twai_event_callbacks_t callbacks = {
        .on_rx_done = on_rx_done,
        .on_tx_done = on_tx_done,
        .on_error = on_error,
        .on_state_change = on_state_change,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, &s_can_ctx));
    ESP_ERROR_CHECK(twai_node_enable(node));
    return node;
}

static esp_err_t send_can_frame(twai_node_handle_t node, uint32_t id,
                                const uint8_t *data, uint8_t len)
{
    twai_frame_t frame = {
        .header = {
            .id = id,
            .dlc = len,
        },
        .buffer = (uint8_t *)data,
        .buffer_len = len,
    };

    esp_err_t ret = twai_node_transmit(node, &frame, pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " submit failed: %s", id, esp_err_to_name(ret));
        return ret;
    }

    can_tx_done_item_t done = {0};
    if (xQueueReceive(s_tx_done_queue, &done, pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " queued but not completed", id);
        return ESP_ERR_TIMEOUT;
    }

    if (!done.success) {
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " failed on bus", id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void process_rx_frame(const can_rx_item_t *item)
{
    if (item->header.ide || item->header.rtr) {
        return;
    }

    ch32_motor_hello_t hello = {0};
    if (ch32_motor_parse_hello(item->header.id, item->data, item->header.dlc, &hello)) {
        bool first_seen = !s_motor_nodes[hello.node_id].online;
        s_motor_nodes[hello.node_id].online = true;
        s_motor_nodes[hello.node_id].hello = hello;
        s_motor_nodes[hello.node_id].last_seen_tick = xTaskGetTickCount();

        if (first_seen) {
            ESP_LOGI(TAG, "HELLO motor node=%u fw=%u cap=0x%02X",
                     hello.node_id, hello.fw_version, hello.capability_flags);
            printf("hello motor node=%u fw=%u cap=0x%02X\r\n",
                   hello.node_id, hello.fw_version, hello.capability_flags);
        }
    }
}

static bool wait_for_motor_ack(uint8_t node_id, uint8_t channel)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }

        process_rx_frame(&item);

        ch32_motor_ack_t ack = {0};
        if (ch32_motor_parse_ack(item.header.id, item.data, item.header.dlc,
                                 node_id, channel, &ack)) {
            ESP_LOGI(TAG, "ACK motor node=%u ch=%u result=%u",
                     ack.node_id, ack.channel, ack.result ? 1U : 0U);
            return ack.result;
        }
    }

    ESP_LOGW(TAG, "ACK timeout motor node=%u ch=%u", node_id, channel);
    return false;
}

static bool send_motor_duty(twai_node_handle_t node, uint8_t node_id,
                            uint8_t channel, uint16_t duty_permille, uint8_t direction)
{
    uint8_t data[CH32_MOTOR_FRAME_DLC] = {0};
    if (!ch32_motor_build_set_duty_cmd(channel, duty_permille, direction, data)) {
        return false;
    }

    if (send_can_frame(node, CH32_MOTOR_CAN_ID_CMD(node_id), data, sizeof(data)) != ESP_OK) {
        return false;
    }
    return wait_for_motor_ack(node_id, channel);
}

static void print_node_summary(void)
{
    printf("motor nodes:");
    for (uint8_t id = 1U; id <= CH32_MOTOR_NODE_ID_MAX; ++id) {
        if (s_motor_nodes[id].online) {
            printf(" [%u fw=%u cap=0x%02X]", id,
                   s_motor_nodes[id].hello.fw_version,
                   s_motor_nodes[id].hello.capability_flags);
        }
    }
    printf("\r\n");
}

void app_main(void)
{
    static const uint16_t motor_test_duty = 1000U;
    TickType_t last_motor_tick = 0;
    TickType_t last_status_tick = 0;

    ESP_LOGI(TAG, "ESP32-WROOM motor_test0_final");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("\r\n==== ESP32-WROOM motor_test0_final ====\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("gateway only: ESP32 sends CAN commands, CH32 generates motor PWM\r\n");
    printf("protocol: hello=0x700+node motor_cmd=0x300+node ack=0x500+node\r\n");
    printf("CH32 default: NODE_ID=2 ch0=PA7 PWM+PA5 DIR ch1=PA6 PWM+PA4 DIR\r\n\r\n");

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, waiting for CH32 motor HELLO");
    printf("twai started, waiting for CH32 motor HELLO\r\n");

    while (true) {
        can_rx_item_t item = {0};
        while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE) {
            process_rx_frame(&item);
        }

        TickType_t now = xTaskGetTickCount();
        uint8_t motor_node = find_motor_node();
        if (motor_node != 0U &&
            (now - last_motor_tick) >= pdMS_TO_TICKS(MOTOR_CMD_PERIOD_MS)) {
            last_motor_tick = now;
            uint16_t duty = motor_test_duty;

            bool ch0_ok = send_motor_duty(node, motor_node, 0U, duty, CH32_MOTOR_DIR_FORWARD);
            vTaskDelay(pdMS_TO_TICKS(30));
            bool ch1_ok = send_motor_duty(node, motor_node, 1U, duty, CH32_MOTOR_DIR_FORWARD);

            ESP_LOGI(TAG, "motor node=%u duty=%u dir=0 ch0=%s ch1=%s",
                     motor_node, duty,
                     ch0_ok ? "OK" : "FAILED",
                     ch1_ok ? "OK" : "FAILED");
            printf("cmd motor node=%u duty=%u dir=0 ch0=%s ch1=%s\r\n",
                   motor_node, duty,
                   ch0_ok ? "OK" : "FAILED",
                   ch1_ok ? "OK" : "FAILED");
        }

        if ((now - last_status_tick) >= pdMS_TO_TICKS(STATUS_PRINT_PERIOD_MS)) {
            last_status_tick = now;
            twai_node_status_t status = {0};
            (void)twai_node_get_info(node, &status, NULL);
            ESP_LOGI(TAG, "status state=%d tx_err=%u rx_err=%u",
                     status.state, status.tx_error_count, status.rx_error_count);
            print_node_summary();
        }

        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_WAIT_MS));
    }
}
