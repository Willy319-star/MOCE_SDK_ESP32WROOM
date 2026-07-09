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

#define CAN_BITRATE             50000
#define CAN_BITRATE_TEXT        "50 kbit/s"
#define CAN_TX_QUEUE_DEPTH      4
#define CAN_RX_QUEUE_LEN        8
#define CAN_TX_DONE_QUEUE_LEN   4
#define CAN_CMD_TIMEOUT_MS      500
#define SERVO_CMD_PERIOD_MS     1000
#define CAN_RESTART_DELAY_MS    200

#define CAN_ID_SERVO_SET        0x430U
#define CAN_ID_GATEWAY_ACK      0x500U
#define GATEWAY_ACK_SERVO_SET   0x30U

#define SERVO_CHANNEL_COUNT     4U
#define SERVO_MIN_ANGLE         0U
#define SERVO_MID_ANGLE         90U
#define SERVO_MAX_ANGLE         180U

static const char *TAG = "esp32wroom_servo_can";

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

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void put_u16_le(uint8_t *data, uint8_t offset, uint16_t value)
{
    data[offset] = (uint8_t)(value & 0xFFU);
    data[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
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
        (void)xQueueSendFromISR(ctx->rx_queue, &item, &woken);
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

static twai_node_handle_t restart_can_node(twai_node_handle_t node)
{
    ESP_LOGW(TAG, "restarting TWAI node to clear stuck TX frames");

    if (node != NULL) {
        (void)twai_node_disable(node);
        (void)twai_node_delete(node);
    }

    if (s_rx_queue != NULL) {
        xQueueReset(s_rx_queue);
    }
    if (s_tx_done_queue != NULL) {
        xQueueReset(s_tx_done_queue);
    }

    vTaskDelay(pdMS_TO_TICKS(CAN_RESTART_DELAY_MS));
    return start_can_node();
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
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " queued but not completed; check CAN ACK/wiring", id);
        return ESP_ERR_TIMEOUT;
    }

    return done.success ? ESP_OK : ESP_FAIL;
}

static bool wait_for_servo_ack(uint8_t channel)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(50)) != pdTRUE) {
            continue;
        }

        if (!item.header.ide && !item.header.rtr &&
            item.header.id == CAN_ID_GATEWAY_ACK &&
            item.header.dlc >= 4 &&
            item.data[0] == GATEWAY_ACK_SERVO_SET &&
            read_u16_le(&item.data[1]) == CAN_ID_SERVO_SET) {
            ESP_LOGI(TAG, "servo channel %u gateway result=%u", channel, item.data[3]);
            return item.data[3] != 0U;
        }
    }

    ESP_LOGW(TAG, "servo channel %u gateway ACK timeout", channel);
    return false;
}

static bool send_servo_angle(twai_node_handle_t node, uint8_t channel, uint16_t angle)
{
    uint8_t data[3] = {0};

    if (channel >= SERVO_CHANNEL_COUNT) {
        return false;
    }
    if (angle > SERVO_MAX_ANGLE) {
        angle = SERVO_MAX_ANGLE;
    }

    data[0] = channel;
    put_u16_le(data, 1U, angle);

    if (send_can_frame(node, CAN_ID_SERVO_SET, data, sizeof(data)) != ESP_OK) {
        return false;
    }

    return wait_for_servo_ack(channel);
}

static bool send_all_servo_angles(twai_node_handle_t node, uint8_t angle)
{
    bool ok[SERVO_CHANNEL_COUNT] = {0};
    bool all_ok = true;

    for (uint8_t channel = 0U; channel < SERVO_CHANNEL_COUNT; ++channel) {
        ok[channel] = send_servo_angle(node, channel, angle);
        all_ok = all_ok && ok[channel];
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "servo angle %u: ch0=%s ch1=%s ch2=%s ch3=%s",
             angle,
             ok[0] ? "OK" : "FAILED",
             ok[1] ? "OK" : "FAILED",
             ok[2] ? "OK" : "FAILED",
             ok[3] ? "OK" : "FAILED");
    printf("servo_can angle=%u ch0=%s ch1=%s ch2=%s ch3=%s\r\n",
           angle,
           ok[0] ? "OK" : "FAILED",
           ok[1] ? "OK" : "FAILED",
           ok[2] ? "OK" : "FAILED",
           ok[3] ? "OK" : "FAILED");
    return all_ok;
}

void app_main(void)
{
    static const uint8_t sweep_angles[] = {
        SERVO_MIN_ANGLE,
        SERVO_MID_ANGLE,
        SERVO_MAX_ANGLE,
        SERVO_MID_ANGLE,
    };
    size_t sweep_index = 0U;

    ESP_LOGI(TAG, "ESP32-WROOM servo CAN demo");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("\r\n==== ESP32-WROOM servo CAN demo ====\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("CH32 servo reference: 20ms period, 1000..2000us pulse, 0..180 deg\r\n");
    printf("Servo channels: ch0=PA6 ch1=PA7 ch2=PB6 ch3=PB7\r\n");
    printf("CAN protocol: id=0x%03X data[0]=channel data[1]=angle ack=0x%02X\r\n\r\n",
           CAN_ID_SERVO_SET, GATEWAY_ACK_SERVO_SET);

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, sending servo angle commands");

    while (true) {
        bool ok = send_all_servo_angles(node, sweep_angles[sweep_index]);
        if (!ok) {
            node = restart_can_node(node);
        }
        sweep_index = (sweep_index + 1U) % (sizeof(sweep_angles) / sizeof(sweep_angles[0]));
        vTaskDelay(pdMS_TO_TICKS(SERVO_CMD_PERIOD_MS));
    }
}
