#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_BITRATE                 50000
#define CAN_BITRATE_TEXT            "50 kbit/s"
#define CAN_RX_QUEUE_LEN            32
#define CAN_TX_DONE_QUEUE_LEN       8
#define CAN_TX_QUEUE_DEPTH          8
#define CAN_CMD_TIMEOUT_MS          600
#define MAIN_LOOP_WAIT_MS           50
#define SERVO_CMD_PERIOD_MS         1000
#define STATUS_PRINT_PERIOD_MS      3000

#define DEVICE_TYPE_SERVO           0x03U
#define NODE_ID_MAX                 10U
#define SERVO_CHANNEL_COUNT         4U

#define CAN_ID_SERVO_CMD(node_id)   (0x400U + (uint32_t)(node_id))
#define CAN_ID_ACK(node_id)         (0x500U + (uint32_t)(node_id))
#define CAN_ID_HELLO(node_id)       (0x700U + (uint32_t)(node_id))

static const char *TAG = "servo_test0";

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
    uint8_t node_id;
    uint8_t device_type;
    uint8_t fw_version;
    uint8_t capability_flags;
    TickType_t last_seen_tick;
} node_info_t;

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;
static node_info_t s_nodes[NODE_ID_MAX + 1U];

static void put_u16_le(uint8_t *data, uint8_t offset, uint16_t value)
{
    data[offset] = (uint8_t)(value & 0xFFU);
    data[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
}

static const char *device_type_name(uint8_t type)
{
    return type == DEVICE_TYPE_SERVO ? "SERVO" : "UNKNOWN";
}

static uint8_t find_servo_node(void)
{
    for (uint8_t id = 1U; id <= NODE_ID_MAX; ++id) {
        if (s_nodes[id].online && s_nodes[id].device_type == DEVICE_TYPE_SERVO) {
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
            .bitrate = CAN_BITRATE,
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

static void handle_hello(const can_rx_item_t *item)
{
    if (item->header.dlc < 4U) {
        return;
    }

    uint8_t node_id = item->data[1];
    if (node_id == 0U || node_id > NODE_ID_MAX || item->header.id != CAN_ID_HELLO(node_id)) {
        return;
    }

    bool first_seen = !s_nodes[node_id].online;
    s_nodes[node_id].online = true;
    s_nodes[node_id].node_id = node_id;
    s_nodes[node_id].device_type = item->data[0];
    s_nodes[node_id].fw_version = item->data[2];
    s_nodes[node_id].capability_flags = item->data[3];
    s_nodes[node_id].last_seen_tick = xTaskGetTickCount();

    if (first_seen) {
        ESP_LOGI(TAG, "HELLO node=%u type=%s(0x%02X) fw=%u cap=0x%02X",
                 node_id,
                 device_type_name(item->data[0]),
                 item->data[0],
                 item->data[2],
                 item->data[3]);
        printf("hello node=%u type=%s fw=%u cap=0x%02X\r\n",
               node_id,
               device_type_name(item->data[0]),
               item->data[2],
               item->data[3]);
    }
}

static void process_rx_frame(const can_rx_item_t *item)
{
    if (item->header.ide || item->header.rtr) {
        return;
    }

    if (item->header.id >= CAN_ID_HELLO(1U) && item->header.id <= CAN_ID_HELLO(NODE_ID_MAX)) {
        handle_hello(item);
    }
}

static bool wait_for_servo_ack(uint8_t node_id, uint8_t channel)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS);
    uint32_t ack_id = CAN_ID_ACK(node_id);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }

        process_rx_frame(&item);

        if (!item.header.ide && !item.header.rtr &&
            item.header.id == ack_id &&
            item.header.dlc >= 4 &&
            item.data[0] == channel &&
            item.data[2] == node_id &&
            item.data[3] == DEVICE_TYPE_SERVO) {
            ESP_LOGI(TAG, "ACK node=%u ch=%u result=%u", node_id, channel, item.data[1]);
            return item.data[1] != 0U;
        }
    }

    ESP_LOGW(TAG, "ACK timeout node=%u ch=%u", node_id, channel);
    return false;
}

static bool send_servo_angle(twai_node_handle_t node, uint8_t node_id,
                             uint8_t channel, uint16_t angle)
{
    uint8_t data[8] = {0};

    if (channel >= SERVO_CHANNEL_COUNT) {
        return false;
    }
    if (angle > 180U) {
        angle = 180U;
    }

    data[0] = channel;
    put_u16_le(data, 1U, angle);

    if (send_can_frame(node, CAN_ID_SERVO_CMD(node_id), data, sizeof(data)) != ESP_OK) {
        return false;
    }
    return wait_for_servo_ack(node_id, channel);
}

static void print_node_summary(void)
{
    printf("nodes:");
    for (uint8_t id = 1U; id <= NODE_ID_MAX; ++id) {
        if (s_nodes[id].online) {
            printf(" [%u %s]", id, device_type_name(s_nodes[id].device_type));
        }
    }
    printf("\r\n");
}

void app_main(void)
{
    static const uint16_t sweep_angles[] = {0U, 90U, 180U, 90U};
    size_t sweep_index = 0U;
    TickType_t last_servo_tick = 0;
    TickType_t last_status_tick = 0;

    ESP_LOGI(TAG, "ESP32-WROOM servo_test0");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("\r\n==== ESP32-WROOM servo_test0 ====\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("protocol: hello=0x700+node servo_cmd=0x400+node ack=0x500+node\r\n");
    printf("CH32 default: NODE_ID=3 ch0=PA6 ch1=PA7 ch2=PB6 ch3=PB7\r\n\r\n");

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, waiting for CH32 servo HELLO");
    printf("twai started, waiting for CH32 servo HELLO\r\n");

    while (true) {
        can_rx_item_t item = {0};
        while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE) {
            process_rx_frame(&item);
        }

        TickType_t now = xTaskGetTickCount();
        uint8_t servo_node = find_servo_node();
        if (servo_node != 0U &&
            (now - last_servo_tick) >= pdMS_TO_TICKS(SERVO_CMD_PERIOD_MS)) {
            last_servo_tick = now;
            uint16_t angle = sweep_angles[sweep_index];
            sweep_index = (sweep_index + 1U) % (sizeof(sweep_angles) / sizeof(sweep_angles[0]));

            bool ok[SERVO_CHANNEL_COUNT] = {0};
            for (uint8_t ch = 0U; ch < SERVO_CHANNEL_COUNT; ++ch) {
                ok[ch] = send_servo_angle(node, servo_node, ch, angle);
                vTaskDelay(pdMS_TO_TICKS(30));
            }

            ESP_LOGI(TAG, "servo node=%u angle=%u ch0=%s ch1=%s ch2=%s ch3=%s",
                     servo_node, angle,
                     ok[0] ? "OK" : "FAILED",
                     ok[1] ? "OK" : "FAILED",
                     ok[2] ? "OK" : "FAILED",
                     ok[3] ? "OK" : "FAILED");
            printf("cmd servo node=%u angle=%u ch0=%s ch1=%s ch2=%s ch3=%s\r\n",
                   servo_node, angle,
                   ok[0] ? "OK" : "FAILED",
                   ok[1] ? "OK" : "FAILED",
                   ok[2] ? "OK" : "FAILED",
                   ok[3] ? "OK" : "FAILED");
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
