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
#define MOTOR_CMD_PERIOD_MS     1000

#define CAN_ID_PWM_SET          0x420U
#define CAN_ID_GATEWAY_ACK      0x500U
#define GATEWAY_ACK_PWM_SET     0x20U

#define MOTOR_A_CHANNEL         0U
#define MOTOR_B_CHANNEL         1U
#define MOTOR_DUTY_100_PERCENT  1000U

static const char *TAG = "esp32wroom_motor_can";

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

static void put_u16_le(uint8_t *data, uint8_t offset, uint16_t value)
{
    data[offset] = (uint8_t)(value & 0xFFU);
    data[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
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
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " queued, no tx_done callback", id);
        return ESP_ERR_TIMEOUT;
    }

    return done.success ? ESP_OK : ESP_FAIL;
}

static bool wait_for_pwm_ack(uint8_t channel)
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
            item.data[0] == GATEWAY_ACK_PWM_SET &&
            read_u16_le(&item.data[1]) == CAN_ID_PWM_SET) {
            ESP_LOGI(TAG, "motor channel %u gateway result=%u", channel, item.data[3]);
            return item.data[3] != 0U;
        }
    }

    ESP_LOGW(TAG, "motor channel %u gateway ACK timeout", channel);
    return false;
}

static bool send_motor_duty(twai_node_handle_t node, uint8_t channel, uint16_t duty_permille)
{
    uint8_t data[3] = {0};
    data[0] = channel;
    put_u16_le(data, 1U, duty_permille);

    if (send_can_frame(node, CAN_ID_PWM_SET, data, sizeof(data)) != ESP_OK) {
        return false;
    }

    return wait_for_pwm_ack(channel);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM motor CAN demo");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d duty=%u/1000",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO, MOTOR_DUTY_100_PERCENT);
    printf("\r\n==== ESP32-WROOM motor CAN demo ====\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("CH32 map: channel0 motorA PA4 PWM PA5 DIR low\r\n");
    printf("CH32 map: channel1 motorB PA6 TIM3_CH1 PWM PA7 DIR low\r\n");
    printf("Sending motor A and B duty 100%% every %d ms\r\n\r\n", MOTOR_CMD_PERIOD_MS);

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, sending motor commands");

    while (true) {
        bool motor_a_ok = send_motor_duty(node, MOTOR_A_CHANNEL, MOTOR_DUTY_100_PERCENT);
        vTaskDelay(pdMS_TO_TICKS(20));
        bool motor_b_ok = send_motor_duty(node, MOTOR_B_CHANNEL, MOTOR_DUTY_100_PERCENT);

        ESP_LOGI(TAG, "motor A 100%% %s, motor B 100%% %s",
                 motor_a_ok ? "OK" : "FAILED",
                 motor_b_ok ? "OK" : "FAILED");
        printf("motor_can duty: A=100%% %s B=100%% %s\r\n",
               motor_a_ok ? "OK" : "FAILED",
               motor_b_ok ? "OK" : "FAILED");

        vTaskDelay(pdMS_TO_TICKS(MOTOR_CMD_PERIOD_MS));
    }
}
