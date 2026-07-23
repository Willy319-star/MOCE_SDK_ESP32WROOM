#include "ch32_syn6288_gateway.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ch32_syn6288_protocol.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define RX_QUEUE_DEPTH          16U
#define TX_DONE_QUEUE_DEPTH     4U
#define TWAI_TX_QUEUE_DEPTH     8U
#define TWAI_TX_TIMEOUT_MS      500U
#define TWAI_RX_POLL_MS         20U
#define TWAI_SAMPLE_POINT       800U
#define TWAI_HARDWARE_RETRIES   3

static const char *TAG = "ch32_syn6288";

typedef struct {
    twai_frame_header_t header;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} gateway_rx_item_t;

typedef struct {
    bool success;
    uint32_t id;
} gateway_tx_done_item_t;

struct ch32_syn6288_gateway {
    twai_node_handle_t node;
    QueueHandle_t rx_queue;
    QueueHandle_t tx_done_queue;
    SemaphoreHandle_t transfer_mutex;
    ch32_syn6288_gateway_config_t config;
    uint8_t next_transfer_id;
};

static bool IRAM_ATTR gateway_on_rx_done(
    twai_node_handle_t node,
    const twai_rx_done_event_data_t *event,
    void *user_context)
{
    (void)event;
    ch32_syn6288_gateway_handle_t gateway = user_context;
    BaseType_t task_woken = pdFALSE;
    gateway_rx_item_t item = {0};
    twai_frame_t frame = {
        .buffer = item.data,
        .buffer_len = sizeof(item.data),
    };

    if (twai_node_receive_from_isr(node, &frame) == ESP_OK) {
        item.header = frame.header;
        (void)xQueueSendFromISR(gateway->rx_queue, &item, &task_woken);
    }
    return task_woken == pdTRUE;
}

static bool IRAM_ATTR gateway_on_tx_done(
    twai_node_handle_t node,
    const twai_tx_done_event_data_t *event,
    void *user_context)
{
    (void)node;
    ch32_syn6288_gateway_handle_t gateway = user_context;
    BaseType_t task_woken = pdFALSE;
    gateway_tx_done_item_t item = {
        .success = event->is_tx_success,
        .id = event->done_tx_frame != NULL
                  ? event->done_tx_frame->header.id
                  : UINT32_MAX,
    };

    (void)xQueueSendFromISR(gateway->tx_done_queue, &item, &task_woken);
    return task_woken == pdTRUE;
}

static bool IRAM_ATTR gateway_on_error(
    twai_node_handle_t node,
    const twai_error_event_data_t *event,
    void *user_context)
{
    (void)node;
    (void)user_context;
    ESP_EARLY_LOGW(TAG, "TWAI error flags=0x%" PRIx32, event->err_flags.val);
    return false;
}

static bool IRAM_ATTR gateway_on_state_change(
    twai_node_handle_t node,
    const twai_state_change_event_data_t *event,
    void *user_context)
{
    (void)node;
    (void)user_context;
    ESP_EARLY_LOGI(TAG, "TWAI state %d -> %d",
                   event->old_sta, event->new_sta);
    return false;
}

static esp_err_t gateway_send_can_frame(
    ch32_syn6288_gateway_handle_t gateway,
    uint32_t id,
    uint8_t *data,
    uint8_t len)
{
    twai_frame_t frame = {
        .header = {
            .id = id,
            .dlc = len,
        },
        .buffer = data,
        .buffer_len = len,
    };
    gateway_tx_done_item_t completed = {0};

    esp_err_t result = twai_node_transmit(
        gateway->node, &frame, pdMS_TO_TICKS(TWAI_TX_TIMEOUT_MS));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "CAN submit id=0x%03" PRIX32 " failed: %s",
                 id, esp_err_to_name(result));
        return result;
    }

    while (xQueueReceive(gateway->tx_done_queue, &completed,
                         pdMS_TO_TICKS(TWAI_TX_TIMEOUT_MS)) == pdTRUE) {
        if (completed.id == id) {
            if (!completed.success) {
                ESP_LOGE(TAG,
                         "CAN bus TX id=0x%03" PRIX32
                         " failed after %d retries",
                         id, TWAI_HARDWARE_RETRIES);
                return ESP_FAIL;
            }
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "CAN TX completion timeout id=0x%03" PRIX32, id);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t gateway_wait_ack(
    ch32_syn6288_gateway_handle_t gateway,
    uint8_t expected_phase,
    uint16_t expected_source_id,
    uint8_t expected_transfer_id,
    uint32_t timeout_ms,
    ch32_syn6288_ack_t *out_ack)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        TickType_t remaining = deadline - xTaskGetTickCount();
        TickType_t wait = pdMS_TO_TICKS(TWAI_RX_POLL_MS);
        gateway_rx_item_t item = {0};
        ch32_syn6288_ack_t ack = {0};

        if (wait > remaining) {
            wait = remaining;
        }
        if (xQueueReceive(gateway->rx_queue, &item, wait) != pdTRUE) {
            continue;
        }
        if (item.header.ide || item.header.rtr ||
            item.header.id != CH32_SYN6288_CAN_ID_ACK ||
            item.header.dlc != 8U ||
            !ch32_syn6288_protocol_parse_ack(
                item.data, item.header.dlc, &ack)) {
            continue;
        }
        if (ack.phase != expected_phase ||
            ack.source_id != expected_source_id ||
            ack.transfer_id != expected_transfer_id) {
            continue;
        }

        ESP_LOGI(TAG,
                 "ACK phase=0x%02X source=0x%03X result=%u "
                 "transfer=%u detail=%u processed=%u",
                 ack.phase, ack.source_id, ack.result, ack.transfer_id,
                 ack.detail, ack.processed_len);
        if (out_ack != NULL) {
            *out_ack = ack;
        }
        return ack.result == 1U ? ESP_OK : ESP_FAIL;
    }

    ESP_LOGE(TAG,
             "ACK timeout phase=0x%02X source=0x%03X "
             "transfer=%u timeout=%" PRIu32 "ms",
             expected_phase, expected_source_id,
             expected_transfer_id, timeout_ms);
    return ESP_ERR_TIMEOUT;
}

esp_err_t ch32_syn6288_gateway_create(
    const ch32_syn6288_gateway_config_t *config,
    ch32_syn6288_gateway_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config != NULL && out_handle != NULL,
                        ESP_ERR_INVALID_ARG, TAG,
                        "config/out_handle is null");
    ESP_RETURN_ON_FALSE(
        config->can_tx_gpio >= 0 &&
            config->can_rx_gpio >= 0 &&
            config->bitrate > 0U &&
            config->start_ack_timeout_ms > 0U &&
            config->final_ack_timeout_ms > 0U,
        ESP_ERR_INVALID_ARG, TAG, "invalid gateway configuration");

    *out_handle = NULL;
    ch32_syn6288_gateway_handle_t gateway = calloc(1, sizeof(*gateway));
    ESP_RETURN_ON_FALSE(gateway != NULL, ESP_ERR_NO_MEM, TAG,
                        "gateway allocation failed");

    gateway->config = *config;
    gateway->rx_queue =
        xQueueCreate(RX_QUEUE_DEPTH, sizeof(gateway_rx_item_t));
    gateway->tx_done_queue =
        xQueueCreate(TX_DONE_QUEUE_DEPTH, sizeof(gateway_tx_done_item_t));
    gateway->transfer_mutex = xSemaphoreCreateMutex();
    if (gateway->rx_queue == NULL ||
        gateway->tx_done_queue == NULL ||
        gateway->transfer_mutex == NULL) {
        (void)ch32_syn6288_gateway_delete(gateway);
        return ESP_ERR_NO_MEM;
    }

    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = config->can_tx_gpio,
            .rx = config->can_rx_gpio,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = config->bitrate,
            .sp_permill = TWAI_SAMPLE_POINT,
        },
        .timestamp_resolution_hz = 1000000U,
        .fail_retry_cnt = TWAI_HARDWARE_RETRIES,
        .tx_queue_depth = TWAI_TX_QUEUE_DEPTH,
        .flags.no_receive_rtr = true,
    };
    esp_err_t result =
        twai_new_node_onchip(&node_config, &gateway->node);
    if (result != ESP_OK) {
        (void)ch32_syn6288_gateway_delete(gateway);
        return result;
    }

    twai_mask_filter_config_t accept_all_standard = {
        .id = 0U,
        .mask = 0U,
        .is_ext = false,
        .no_fd = true,
    };
    result = twai_node_config_mask_filter(
        gateway->node, 0U, &accept_all_standard);
    if (result != ESP_OK) {
        (void)ch32_syn6288_gateway_delete(gateway);
        return result;
    }

    twai_event_callbacks_t callbacks = {
        .on_rx_done = gateway_on_rx_done,
        .on_tx_done = gateway_on_tx_done,
        .on_error = gateway_on_error,
        .on_state_change = gateway_on_state_change,
    };
    result = twai_node_register_event_callbacks(
        gateway->node, &callbacks, gateway);
    if (result == ESP_OK) {
        result = twai_node_enable(gateway->node);
    }
    if (result != ESP_OK) {
        (void)ch32_syn6288_gateway_delete(gateway);
        return result;
    }

    gateway->next_transfer_id = 1U;
    *out_handle = gateway;
    ESP_LOGI(TAG,
             "TWAI ready bitrate=%" PRIu32
             " TX=GPIO%d RX=GPIO%d retries=%d",
             config->bitrate, config->can_tx_gpio,
             config->can_rx_gpio, TWAI_HARDWARE_RETRIES);
    return ESP_OK;
}

esp_err_t ch32_syn6288_gateway_send_raw(
    ch32_syn6288_gateway_handle_t gateway,
    const uint8_t *frame,
    size_t frame_len)
{
    ESP_RETURN_ON_FALSE(gateway != NULL && frame != NULL,
                        ESP_ERR_INVALID_ARG, TAG,
                        "gateway/frame is null");
    ESP_RETURN_ON_FALSE(
        frame_len >= 1U &&
            frame_len <= CH32_SYN6288_MAX_FRAME_SIZE,
        ESP_ERR_INVALID_SIZE, TAG,
        "frame length must be 1..4096");
    if (xSemaphoreTake(gateway->transfer_mutex, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_OK;
    uint8_t transfer_id = gateway->next_transfer_id++;
    uint8_t can_data[8] = {0};
    uint16_t crc = ch32_syn6288_protocol_crc16(frame, frame_len);
    size_t fragment_count =
        ch32_syn6288_protocol_chunk_count(frame_len);
    twai_node_status_t status = {0};

    xQueueReset(gateway->rx_queue);
    xQueueReset(gateway->tx_done_queue);
    ch32_syn6288_protocol_build_start(
        transfer_id, (uint16_t)frame_len, crc, can_data);

    ESP_LOGI(TAG,
             "transfer=%u length=%u fragments=%u crc=0x%04X",
             transfer_id, (unsigned)frame_len,
             (unsigned)fragment_count, crc);
    result = gateway_send_can_frame(
        gateway, CH32_SYN6288_CAN_ID_START, can_data, 8U);
    if (result != ESP_OK) {
        goto done;
    }
    result = gateway_wait_ack(
        gateway, CH32_SYN6288_ACK_PHASE_START,
        CH32_SYN6288_CAN_ID_START, transfer_id,
        gateway->config.start_ack_timeout_ms, NULL);
    if (result != ESP_OK) {
        goto done;
    }

    for (size_t offset = 0U, sequence = 0U;
         offset < frame_len;
         offset += CH32_SYN6288_DATA_PAYLOAD_SIZE, ++sequence) {
        size_t payload_len = frame_len - offset;
        if (payload_len > CH32_SYN6288_DATA_PAYLOAD_SIZE) {
            payload_len = CH32_SYN6288_DATA_PAYLOAD_SIZE;
        }
        size_t dlc = ch32_syn6288_protocol_build_data(
            transfer_id, (uint16_t)sequence,
            &frame[offset], payload_len, can_data);
        result = gateway_send_can_frame(
            gateway, CH32_SYN6288_CAN_ID_DATA,
            can_data, (uint8_t)dlc);
        if (result != ESP_OK) {
            goto done;
        }
    }

    result = gateway_wait_ack(
        gateway, CH32_SYN6288_ACK_PHASE_COMPLETE,
        CH32_SYN6288_CAN_ID_DATA, transfer_id,
        gateway->config.final_ack_timeout_ms, NULL);
    if (result == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG,
                 "final ACK timeout: application replay "
                 "is intentionally disabled");
    }

done:
    if (twai_node_get_info(gateway->node, &status, NULL) == ESP_OK) {
        ESP_LOGI(TAG, "TWAI state=%d tx_err=%u rx_err=%u",
                 status.state, status.tx_error_count,
                 status.rx_error_count);
    }
    xSemaphoreGive(gateway->transfer_mutex);
    return result;
}

esp_err_t ch32_syn6288_gateway_delete(
    ch32_syn6288_gateway_handle_t gateway)
{
    if (gateway == NULL) {
        return ESP_OK;
    }

    esp_err_t result = ESP_OK;
    if (gateway->node != NULL) {
        esp_err_t disable_result =
            twai_node_disable(gateway->node);
        if (disable_result != ESP_OK &&
            disable_result != ESP_ERR_INVALID_STATE) {
            result = disable_result;
        }
        esp_err_t delete_result =
            twai_node_delete(gateway->node);
        if (delete_result != ESP_OK) {
            result = delete_result;
        }
    }
    if (gateway->transfer_mutex != NULL) {
        vSemaphoreDelete(gateway->transfer_mutex);
    }
    if (gateway->tx_done_queue != NULL) {
        vQueueDelete(gateway->tx_done_queue);
    }
    if (gateway->rx_queue != NULL) {
        vQueueDelete(gateway->rx_queue);
    }
    free(gateway);
    return result;
}

