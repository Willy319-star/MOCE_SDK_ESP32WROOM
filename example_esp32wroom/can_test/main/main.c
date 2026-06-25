#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_TEST_BITRATE        50000
#define CAN_TEST_ID             0x321U
#define CAN_TEST_PERIOD_MS      1000
#define CAN_GPIO_SETTLE_US      2000
#define CAN_RX_QUEUE_LEN        8
#define CAN_TX_DONE_QUEUE_LEN   4

static const char *TAG = "esp32_can_test";

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
} can_test_context_t;

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_test_context_t s_can_ctx;

static bool run_transceiver_gpio_test(void)
{
    bool ok = true;

    ESP_ERROR_CHECK(gpio_reset_pin(BOARD_CAN_TX_GPIO));
    ESP_ERROR_CHECK(gpio_reset_pin(BOARD_CAN_RX_GPIO));
    ESP_ERROR_CHECK(gpio_set_direction(BOARD_CAN_TX_GPIO, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(BOARD_CAN_RX_GPIO, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(BOARD_CAN_RX_GPIO, GPIO_FLOATING));

    ESP_LOGI(TAG, "physical transceiver test: TXD=GPIO%d RXD=GPIO%d",
             BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("physical transceiver test: TXD=GPIO%d RXD=GPIO%d\r\n",
           BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);

    for (int i = 0; i < 5; ++i) {
        ESP_ERROR_CHECK(gpio_set_level(BOARD_CAN_TX_GPIO, 1));
        esp_rom_delay_us(CAN_GPIO_SETTLE_US);
        int recessive_rx = gpio_get_level(BOARD_CAN_RX_GPIO);

        ESP_ERROR_CHECK(gpio_set_level(BOARD_CAN_TX_GPIO, 0));
        esp_rom_delay_us(CAN_GPIO_SETTLE_US);
        int dominant_rx = gpio_get_level(BOARD_CAN_RX_GPIO);

        ESP_LOGI(TAG, "gpio loop #%d: TXD high -> RXD=%d, TXD low -> RXD=%d",
                 i + 1, recessive_rx, dominant_rx);
        printf("gpio_loop #%d txd_high_rxd=%d txd_low_rxd=%d\r\n",
               i + 1, recessive_rx, dominant_rx);

        if (recessive_rx != 1 || dominant_rx != 0) {
            ok = false;
        }
    }

    ESP_ERROR_CHECK(gpio_set_level(BOARD_CAN_TX_GPIO, 1));
    ESP_LOGI(TAG, "physical transceiver test result: %s", ok ? "PASS" : "FAIL");
    printf("physical transceiver test result: %s\r\n", ok ? "PASS" : "FAIL");
    return ok;
}

static bool IRAM_ATTR on_rx_done(twai_node_handle_t handle,
                                 const twai_rx_done_event_data_t *edata,
                                 void *user_ctx)
{
    (void)edata;
    can_test_context_t *ctx = (can_test_context_t *)user_ctx;
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
    can_test_context_t *ctx = (can_test_context_t *)user_ctx;
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
    ESP_EARLY_LOGW(TAG, "TWAI error flags=0x%" PRIx32, edata->err_flags.val);
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

static void format_frame_data(const can_rx_item_t *item, char *buffer, size_t buffer_size)
{
    size_t used = 0;
    uint16_t len = item->header.dlc <= TWAI_FRAME_MAX_LEN ? item->header.dlc : TWAI_FRAME_MAX_LEN;

    for (uint16_t i = 0; i < len && used < buffer_size; ++i) {
        int written = snprintf(buffer + used, buffer_size - used, "%02X%s",
                               item->data[i], (i + 1U < len) ? " " : "");
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }

    if (buffer_size > 0 && used == 0) {
        buffer[0] = '\0';
    }
}

static void print_frame(const can_rx_item_t *item, uint32_t rx_count)
{
    char data_text[3 * TWAI_FRAME_MAX_LEN] = {0};
    format_frame_data(item, data_text, sizeof(data_text));

    ESP_LOGI(TAG, "RX #%" PRIu32 " id=0x%03" PRIX32 " dlc=%u data=[%s]",
             rx_count, item->header.id, item->header.dlc, data_text);
    printf("can_rx #%" PRIu32 " id=0x%03" PRIX32 " dlc=%u data=[%s]\r\n",
           rx_count, item->header.id, item->header.dlc, data_text);
}

static twai_node_handle_t start_twai_node(void)
{
    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = BOARD_CAN_TX_GPIO,
            .rx = BOARD_CAN_RX_GPIO,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = CAN_TEST_BITRATE,
            .sp_permill = 800,
        },
        .timestamp_resolution_hz = 1000000,
        .fail_retry_cnt = 3,
        .tx_queue_depth = 4,
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

void app_main(void)
{
    printf("esp32 can_test start\r\n");
    ESP_LOGI(TAG, "ESP32-WROOM external CAN transceiver test");
    ESP_LOGI(TAG, "CAN_TX=GPIO%d CAN_RX=GPIO%d bitrate=%d", BOARD_CAN_TX_GPIO,
             BOARD_CAN_RX_GPIO, CAN_TEST_BITRATE);

    bool gpio_test_ok = run_transceiver_gpio_test();
    printf("note: GPIO test uses TXD/RXD through the CAN transceiver; TWAI test needs bus ACK for TX_OK.\r\n");

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_twai_node();
    ESP_LOGI(TAG, "TWAI started in normal mode");
    printf("twai normal mode started, bitrate=%d, physical_gpio_test=%s\r\n",
           CAN_TEST_BITRATE, gpio_test_ok ? "PASS" : "FAIL");

    uint32_t tx_count = 0;
    uint32_t rx_count = 0;

    while (true) {
        uint8_t data[8] = {
            'M', 'O', 'C', 'E',
            (uint8_t)(tx_count & 0xFFU),
            (uint8_t)((tx_count >> 8U) & 0xFFU),
            0x32,
            0x57,
        };
        twai_frame_t tx_frame = {
            .header = {
                .id = CAN_TEST_ID,
                .dlc = sizeof(data),
            },
            .buffer = data,
            .buffer_len = sizeof(data),
        };

        esp_err_t tx_ret = twai_node_transmit(node, &tx_frame, 100);
        if (tx_ret == ESP_OK) {
            can_tx_done_item_t done = {0};
            if (xQueueReceive(s_tx_done_queue, &done, pdMS_TO_TICKS(300)) == pdTRUE) {
                ESP_LOGI(TAG, "TX request #%" PRIu32 " id=0x%03" PRIX32 " done=%s",
                         tx_count, done.id, done.success ? "OK" : "FAILED");
                printf("can_tx #%" PRIu32 " id=0x%03" PRIX32 " done=%s\r\n",
                       tx_count, done.id, done.success ? "OK" : "FAILED");
            } else {
                ESP_LOGW(TAG, "TX request #%" PRIu32 " queued, no tx_done callback yet", tx_count);
                printf("can_tx #%" PRIu32 " queued no_tx_done_yet\r\n", tx_count);
            }
        } else {
            ESP_LOGW(TAG, "twai_node_transmit failed: %s", esp_err_to_name(tx_ret));
            printf("can_tx #%" PRIu32 " submit_failed=%s\r\n", tx_count, esp_err_to_name(tx_ret));
        }

        can_rx_item_t rx_item = {0};
        while (xQueueReceive(s_rx_queue, &rx_item, 0) == pdTRUE) {
            ++rx_count;
            print_frame(&rx_item, rx_count);
        }

        twai_node_status_t status = {0};
        twai_node_record_t record = {0};
        (void)twai_node_get_info(node, &status, &record);
        ESP_LOGI(TAG, "status state=%d tx_err=%u rx_err=%u bus_err=%" PRIu32,
                 status.state, status.tx_error_count, status.rx_error_count, record.bus_err_num);
        printf("can_status state=%d tx_err=%u rx_err=%u bus_err=%" PRIu32 "\r\n",
               status.state, status.tx_error_count, status.rx_error_count, record.bus_err_num);

        ++tx_count;
        vTaskDelay(pdMS_TO_TICKS(CAN_TEST_PERIOD_MS));
    }
}
