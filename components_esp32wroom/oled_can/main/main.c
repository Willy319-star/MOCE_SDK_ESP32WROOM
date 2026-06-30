#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
#define OLED_REFRESH_MS         5000

#define CAN_ID_OLED_CLEAR       0x410U
#define CAN_ID_OLED_TEXT        0x411U
#define CAN_ID_GATEWAY_ACK      0x500U
#define OLED_TEXT_PAGE          2U
#define OLED_TEXT_COL           0U
#define OLED_FONT_WIDTH         6U
#define OLED_TEXT_MAX_PER_FRAME 6U

static const char *TAG = "esp32wroom_oled";

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

static void format_can_data(const can_rx_item_t *item, char *buffer, size_t buffer_size)
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

static void log_rx_frame(const can_rx_item_t *item)
{
    char data_text[3 * TWAI_FRAME_MAX_LEN] = {0};
    format_can_data(item, data_text, sizeof(data_text));
    ESP_LOGI(TAG, "RX id=0x%03" PRIX32 " dlc=%u data=[%s]",
             item->header.id, item->header.dlc, data_text);
    printf("can_rx id=0x%03" PRIX32 " dlc=%u data=[%s]\r\n",
           item->header.id, item->header.dlc, data_text);
}

static void drain_rx_queue(void)
{
    can_rx_item_t item = {0};
    while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE) {
        log_rx_frame(&item);
    }
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

    if (!done.success) {
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " failed on bus", id);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TX id=0x%03" PRIX32 " OK", done.id);
    return ESP_OK;
}

static bool wait_for_gateway_ack(uint16_t source_id, uint8_t code)
{
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(50)) != pdTRUE) {
            continue;
        }

        if (!item.header.ide && !item.header.rtr &&
            item.header.id == CAN_ID_GATEWAY_ACK && item.header.dlc >= 4 &&
            item.data[0] == code &&
            ((uint16_t)item.data[1] | ((uint16_t)item.data[2] << 8)) == source_id) {
            ESP_LOGI(TAG, "ACK code=0x%02X source=0x%03X result=%u",
                     code, source_id, item.data[3]);
            printf("gateway_ack code=0x%02X source=0x%03X result=%u\r\n",
                   code, source_id, item.data[3]);
            return item.data[3] != 0;
        }

        log_rx_frame(&item);
    }

    ESP_LOGW(TAG, "ACK timeout code=0x%02X source=0x%03X", code, source_id);
    return false;
}

static bool send_oled_clear(twai_node_handle_t node)
{
    uint8_t data[1] = {0};

    if (send_can_frame(node, CAN_ID_OLED_CLEAR, data, 0) != ESP_OK) {
        return false;
    }
    return wait_for_gateway_ack(CAN_ID_OLED_CLEAR, 0x10U);
}

static bool send_oled_text_chunk(twai_node_handle_t node, uint8_t page, uint8_t col,
                                 const char *text, size_t len)
{
    uint8_t data[8] = {0};

    data[0] = page;
    data[1] = col;
    memcpy(&data[2], text, len);

    if (send_can_frame(node, CAN_ID_OLED_TEXT, data, (uint8_t)(len + 2U)) != ESP_OK) {
        return false;
    }
    return wait_for_gateway_ack(CAN_ID_OLED_TEXT, 0x11U);
}

static bool display_message(twai_node_handle_t node, const char *message)
{
    const size_t message_len = strlen(message);
    bool ok = send_oled_clear(node);

    for (size_t offset = 0; offset < message_len; offset += OLED_TEXT_MAX_PER_FRAME) {
        size_t chunk_len = message_len - offset;
        if (chunk_len > OLED_TEXT_MAX_PER_FRAME) {
            chunk_len = OLED_TEXT_MAX_PER_FRAME;
        }

        uint8_t col = (uint8_t)(OLED_TEXT_COL + offset * OLED_FONT_WIDTH);
        ok = send_oled_text_chunk(node, OLED_TEXT_PAGE, col, &message[offset], chunk_len) && ok;
    }

    return ok;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM OLED over CH32 CAN gateway example");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("oled gateway demo: bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d text=moceai666\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, sending OLED commands to CH32 gateway");
    printf("twai started, sending OLED commands to CH32 gateway\r\n");

    while (true) {
        bool ok = display_message(node, "moceai666");
        ESP_LOGI(TAG, "OLED gateway update %s", ok ? "OK" : "FAILED");
        printf("oled_gateway_update %s\r\n", ok ? "OK" : "FAILED");
        drain_rx_queue();
        vTaskDelay(pdMS_TO_TICKS(OLED_REFRESH_MS));
    }
}
