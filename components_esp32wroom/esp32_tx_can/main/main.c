#include <inttypes.h>
#include <stdio.h>

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_BITRATE          50000
#define CAN_BITRATE_TEXT     "50 kbit/s"
#define CAN_TEST_ID          0x123U
#define CAN_TX_PERIOD_MS     1000
#define CAN_TX_QUEUE_DEPTH   4
#define CAN_TX_DONE_QUEUE_LEN 4

static const char *TAG = "esp32_tx_can";

typedef struct {
    bool success;
    uint32_t id;
} can_tx_done_item_t;

static QueueHandle_t s_tx_done_queue;

static bool IRAM_ATTR can_tx_done_callback(twai_node_handle_t handle,
                                           const twai_tx_done_event_data_t *edata,
                                           void *user_ctx)
{
    (void)handle;
    QueueHandle_t tx_done_queue = (QueueHandle_t)user_ctx;
    BaseType_t woken = pdFALSE;

    can_tx_done_item_t item = {
        .success = edata->is_tx_success,
        .id = edata->done_tx_frame != NULL ? edata->done_tx_frame->header.id : 0,
    };

    (void)xQueueSendFromISR(tx_done_queue, &item, &woken);
    return woken == pdTRUE;
}

static bool IRAM_ATTR can_error_callback(twai_node_handle_t handle,
                                         const twai_error_event_data_t *edata,
                                         void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGW(TAG, "TWAI bus error flags=0x%" PRIx32, edata->err_flags.val);
    return false;
}

static bool IRAM_ATTR can_state_change_callback(twai_node_handle_t handle,
                                                const twai_state_change_event_data_t *edata,
                                                void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    const char *state_names[] = {
        "error_active",
        "error_warning",
        "error_passive",
        "bus_off",
    };

    if (edata->old_sta <= TWAI_ERROR_BUS_OFF && edata->new_sta <= TWAI_ERROR_BUS_OFF) {
        ESP_EARLY_LOGI(TAG, "TWAI state: %s -> %s",
                       state_names[edata->old_sta], state_names[edata->new_sta]);
    }
    return false;
}

static void log_status(twai_node_handle_t can_node)
{
    twai_node_status_t status = {0};
    twai_node_record_t record = {0};

    (void)twai_node_get_info(can_node, &status, &record);
    ESP_LOGI(TAG, "status state=%d tx_err=%u rx_err=%u bus_err=%" PRIu32,
             status.state, status.tx_error_count, status.rx_error_count, record.bus_err_num);
    printf("esp32_tx_can status state=%d tx_err=%u rx_err=%u bus_err=%" PRIu32 "\r\n",
           status.state, status.tx_error_count, status.rx_error_count, record.bus_err_num);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM CAN TX example");
    ESP_LOGI(TAG, "bitrate=%s tx_gpio=%d rx_gpio=%d id=0x%03X period=%dms",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO,
             CAN_TEST_ID, CAN_TX_PERIOD_MS);
    printf("esp32_tx_can ready: bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d id=0x%03X period=%dms\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO,
           CAN_TEST_ID, CAN_TX_PERIOD_MS);

    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK(s_tx_done_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);

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

    twai_node_handle_t can_node = NULL;
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &can_node));

    twai_event_callbacks_t callbacks = {
        .on_tx_done = can_tx_done_callback,
        .on_error = can_error_callback,
        .on_state_change = can_state_change_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(can_node, &callbacks, s_tx_done_queue));

    ESP_ERROR_CHECK(twai_node_enable(can_node));
    ESP_LOGI(TAG, "TWAI driver started, sending CAN frames...");
    printf("esp32_tx_can started, sending CAN frames...\r\n");

    uint32_t tx_count = 0;

    while (true) {
        uint8_t data[8] = {
            0x01, 0x02, 0x03, 0x04,
            0x05, 0x06, 0x07, 0x08,
        };
        twai_frame_t tx_frame = {
            .header = {
                .id = CAN_TEST_ID,
                .dlc = sizeof(data),
            },
            .buffer = data,
            .buffer_len = sizeof(data),
        };

        esp_err_t ret = twai_node_transmit(can_node, &tx_frame, 100);
        if (ret == ESP_OK) {
            can_tx_done_item_t done = {0};
            if (xQueueReceive(s_tx_done_queue, &done, pdMS_TO_TICKS(500)) == pdTRUE) {
                ESP_LOGI(TAG, "#%" PRIu32 " id=0x%03" PRIX32 " %s data=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                         tx_count, done.id, done.success ? "TX OK" : "TX FAILED",
                         data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
                printf("can_tx #%" PRIu32 " id=0x%03" PRIX32 " %s data=[%02X %02X %02X %02X %02X %02X %02X %02X]\r\n",
                       tx_count, done.id, done.success ? "OK" : "FAILED",
                       data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
            } else {
                ESP_LOGW(TAG, "#%" PRIu32 " TX queued but no tx_done callback", tx_count);
                printf("can_tx #%" PRIu32 " queued no_tx_done\r\n", tx_count);
            }
        } else {
            ESP_LOGW(TAG, "#%" PRIu32 " transmit submit failed: %s", tx_count, esp_err_to_name(ret));
            printf("can_tx #%" PRIu32 " submit_failed=%s\r\n", tx_count, esp_err_to_name(ret));
        }

        log_status(can_node);
        ++tx_count;
        vTaskDelay(pdMS_TO_TICKS(CAN_TX_PERIOD_MS));
    }
}
