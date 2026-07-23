#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "board.h"
#include "ch32_oled_gateway.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_RX_QUEUE_LEN            32U
#define CAN_TX_DONE_QUEUE_LEN       4U
#define CAN_TX_QUEUE_DEPTH          4U
#define CAN_TIMEOUT_MS              1000U

#define CH32_NODE_ID                CH32_OLED_DEFAULT_NODE_ID
#define OLED_I2C_ADDR               CH32_OLED_DEFAULT_I2C_ADDRESS
#define OLED_RAW_DATA_MAX           4U

static const char *TAG = "oled_ch32_gateway";

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
    uint8_t len;
    uint8_t data[4];
} oled_command_group_t;

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;

static bool IRAM_ATTR on_rx_done(
    twai_node_handle_t handle,
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
            ESP_EARLY_LOGW(TAG, "RX queue full, dropping CAN frame");
        }
    }

    return woken == pdTRUE;
}

static bool IRAM_ATTR on_tx_done(
    twai_node_handle_t handle,
    const twai_tx_done_event_data_t *edata,
    void *user_ctx)
{
    (void)handle;
    can_context_t *ctx = (can_context_t *)user_ctx;
    BaseType_t woken = pdFALSE;
    can_tx_done_item_t item = {
        .success = edata->is_tx_success,
        .id = edata->done_tx_frame != NULL
                  ? edata->done_tx_frame->header.id
                  : 0U,
    };

    (void)xQueueSendFromISR(ctx->tx_done_queue, &item, &woken);
    return woken == pdTRUE;
}

static bool IRAM_ATTR on_error(
    twai_node_handle_t handle,
    const twai_error_event_data_t *edata,
    void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGW(TAG, "TWAI bus error flags=0x%" PRIx32,
                   edata->err_flags.val);
    return false;
}

static bool IRAM_ATTR on_state_change(
    twai_node_handle_t handle,
    const twai_state_change_event_data_t *edata,
    void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGI(TAG, "TWAI state %d -> %d",
                   edata->old_sta, edata->new_sta);
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
            .bitrate = CH32_OLED_CAN_BITRATE,
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
    ESP_ERROR_CHECK(
        twai_node_config_mask_filter(node, 0, &accept_all_standard));

    twai_event_callbacks_t callbacks = {
        .on_rx_done = on_rx_done,
        .on_tx_done = on_tx_done,
        .on_error = on_error,
        .on_state_change = on_state_change,
    };
    ESP_ERROR_CHECK(
        twai_node_register_event_callbacks(node, &callbacks, &s_can_ctx));
    ESP_ERROR_CHECK(twai_node_enable(node));
    return node;
}

static esp_err_t send_can_frame(twai_node_handle_t node,
                                const uint8_t *data,
                                uint8_t len)
{
    twai_frame_t frame = {
        .header = {
            .id = CH32_OLED_CAN_ID_I2C_CMD(CH32_NODE_ID),
            .dlc = len,
        },
        .buffer = (uint8_t *)data,
        .buffer_len = len,
    };

    esp_err_t result = twai_node_transmit(
        node, &frame, pdMS_TO_TICKS(CAN_TIMEOUT_MS));
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "CAN command submit failed: %s",
                 esp_err_to_name(result));
        return result;
    }

    can_tx_done_item_t done = {0};
    if (xQueueReceive(s_tx_done_queue, &done,
                      pdMS_TO_TICKS(CAN_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "CAN command completion timeout");
        return ESP_ERR_TIMEOUT;
    }
    if (!done.success ||
        done.id != CH32_OLED_CAN_ID_I2C_CMD(CH32_NODE_ID)) {
        ESP_LOGW(TAG, "CAN command transmission failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void wait_for_gateway_hello(void)
{
    while (true) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item,
                          pdMS_TO_TICKS(CAN_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGI(TAG,
                     "Waiting for CH32 OLED node %u "
                     "(HELLO id 0x%03X)",
                     CH32_NODE_ID,
                     (unsigned)CH32_OLED_CAN_ID_HELLO(CH32_NODE_ID));
            continue;
        }

        if (item.header.ide || item.header.rtr) {
            continue;
        }

        ch32_oled_hello_t hello = {0};
        if (ch32_oled_parse_hello(
                item.header.id, item.data, item.header.dlc,
                CH32_NODE_ID, &hello)) {
            ESP_LOGI(TAG,
                     "CH32 node %u online, firmware=%u "
                     "capabilities=0x%02X",
                     hello.node_id, hello.fw_version,
                     hello.capability_flags);
            return;
        }
    }
}

static bool wait_for_gateway_ack(uint8_t command)
{
    TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(CAN_TIMEOUT_MS);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item,
                          pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }
        if (item.header.ide || item.header.rtr) {
            continue;
        }

        ch32_oled_ack_t ack = {0};
        if (ch32_oled_parse_ack(
                item.header.id, item.data, item.header.dlc,
                CH32_NODE_ID, command, &ack)) {
            return ack.result;
        }
    }

    ESP_LOGW(TAG, "ACK timeout for command 0x%02X", command);
    return false;
}

static bool gateway_command(twai_node_handle_t node,
                            uint8_t command,
                            const uint8_t *frame_data,
                            uint8_t frame_len)
{
    if (send_can_frame(node, frame_data, frame_len) != ESP_OK) {
        return false;
    }
    return wait_for_gateway_ack(command);
}

static bool gateway_probe(twai_node_handle_t node)
{
    uint8_t data[2] = {0};
    if (!ch32_oled_build_probe(OLED_I2C_ADDR, data)) {
        return false;
    }
    return gateway_command(
        node, CH32_OLED_I2C_CMD_PROBE, data, sizeof(data));
}

static bool gateway_write_raw(twai_node_handle_t node,
                              const uint8_t *payload,
                              uint8_t payload_len)
{
    uint8_t data[CH32_OLED_CAN_FRAME_MAX_DLC] = {0};
    uint8_t dlc = 0U;

    if (!ch32_oled_build_write_raw(
            OLED_I2C_ADDR, payload, payload_len, data, &dlc)) {
        return false;
    }
    return gateway_command(
        node, CH32_OLED_I2C_CMD_WRITE_RAW, data, dlc);
}

static bool oled_write(twai_node_handle_t node,
                       uint8_t control,
                       const uint8_t *data,
                       size_t len)
{
    size_t offset = 0U;

    while (offset < len) {
        uint8_t payload[CH32_OLED_WRITE_RAW_MAX_PAYLOAD] = {
            control, 0U, 0U, 0U, 0U
        };
        size_t chunk_len = len - offset;
        if (chunk_len > OLED_RAW_DATA_MAX) {
            chunk_len = OLED_RAW_DATA_MAX;
        }
        memcpy(&payload[1], &data[offset], chunk_len);
        if (!gateway_write_raw(
                node, payload, (uint8_t)(chunk_len + 1U))) {
            return false;
        }
        offset += chunk_len;
    }

    return true;
}

static bool oled_commands(twai_node_handle_t node,
                          const uint8_t *commands,
                          size_t len)
{
    return oled_write(node, 0x00U, commands, len);
}

static bool oled_data(twai_node_handle_t node,
                      const uint8_t *data,
                      size_t len)
{
    return oled_write(node, 0x40U, data, len);
}

static bool oled_set_position(twai_node_handle_t node,
                              uint8_t page,
                              uint8_t column)
{
    const uint8_t commands[] = {
        (uint8_t)(0xB0U | (page & 0x07U)),
        (uint8_t)(column & 0x0FU),
        (uint8_t)(0x10U | ((column >> 4U) & 0x0FU)),
    };
    return oled_commands(node, commands, sizeof(commands));
}

static bool oled_init(twai_node_handle_t node)
{
    static const oled_command_group_t init_groups[] = {
        {1U, {0xAEU}},
        {2U, {0x20U, 0x02U}},
        {1U, {0xB0U}},
        {1U, {0xC8U}},
        {1U, {0x00U}},
        {1U, {0x10U}},
        {1U, {0x40U}},
        {2U, {0x81U, 0x7FU}},
        {1U, {0xA1U}},
        {1U, {0xA6U}},
        {2U, {0xA8U, 0x3FU}},
        {1U, {0xA4U}},
        {2U, {0xD3U, 0x00U}},
        {2U, {0xD5U, 0x80U}},
        {2U, {0xD9U, 0xF1U}},
        {2U, {0xDAU, 0x12U}},
        {2U, {0xDBU, 0x40U}},
        {2U, {0x8DU, 0x14U}},
        {1U, {0xAFU}},
    };

    for (size_t i = 0U;
         i < (sizeof(init_groups) / sizeof(init_groups[0]));
         ++i) {
        if (!oled_commands(
                node, init_groups[i].data, init_groups[i].len)) {
            ESP_LOGW(TAG,
                     "SSD1315 initialization failed "
                     "at command group %u",
                     (unsigned)i);
            return false;
        }
    }
    return true;
}

static bool oled_clear(twai_node_handle_t node)
{
    static const uint8_t zeros[OLED_RAW_DATA_MAX] = {0};

    for (uint8_t page = 0U; page < 8U; ++page) {
        if (!oled_set_position(node, page, 0U)) {
            return false;
        }
        for (uint8_t column = 0U;
             column < 128U;
             column += OLED_RAW_DATA_MAX) {
            if (!oled_data(node, zeros, sizeof(zeros))) {
                return false;
            }
        }
    }
    return true;
}

static const uint8_t *font_glyph(char c)
{
    static const uint8_t blank[5] =
        {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    static const uint8_t upper_h[5] =
        {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU};
    static const uint8_t upper_w[5] =
        {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU};
    static const uint8_t lower_e[5] =
        {0x38U, 0x54U, 0x54U, 0x54U, 0x18U};
    static const uint8_t lower_l[5] =
        {0x00U, 0x41U, 0x7FU, 0x40U, 0x00U};
    static const uint8_t lower_o[5] =
        {0x38U, 0x44U, 0x44U, 0x44U, 0x38U};
    static const uint8_t lower_r[5] =
        {0x7CU, 0x08U, 0x04U, 0x04U, 0x08U};
    static const uint8_t lower_d[5] =
        {0x38U, 0x44U, 0x44U, 0x48U, 0x7FU};

    switch (c) {
    case 'H':
        return upper_h;
    case 'W':
        return upper_w;
    case 'e':
        return lower_e;
    case 'l':
        return lower_l;
    case 'o':
        return lower_o;
    case 'r':
        return lower_r;
    case 'd':
        return lower_d;
    default:
        return blank;
    }
}

static bool oled_show_hello_world(twai_node_handle_t node)
{
    static const char text[] = "Hello World";
    uint8_t pixels[(sizeof(text) - 1U) * 6U] = {0};

    for (size_t i = 0U; i < (sizeof(text) - 1U); ++i) {
        const uint8_t *glyph = font_glyph(text[i]);
        memcpy(&pixels[i * 6U], glyph, 5U);
    }

    if (!oled_set_position(node, 3U, 31U)) {
        return false;
    }
    return oled_data(node, pixels, sizeof(pixels));
}

void app_main(void)
{
    ESP_LOGI(TAG,
             "ESP32 -> CAN -> CH32 node %u -> SSD1315 OLED",
             CH32_NODE_ID);
    ESP_LOGI(TAG,
             "CAN 50 kbit/s TX=GPIO%d RX=GPIO%d, "
             "OLED address=0x%02X",
             BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO, OLED_I2C_ADDR);

    s_rx_queue =
        xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue =
        xQueueCreate(CAN_TX_DONE_QUEUE_LEN,
                     sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK(
        (s_rx_queue == NULL || s_tx_done_queue == NULL)
            ? ESP_ERR_NO_MEM
            : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    wait_for_gateway_hello();

    while (!gateway_probe(node)) {
        ESP_LOGW(TAG,
                 "SSD1315 not responding at 7-bit address "
                 "0x%02X; retrying",
                 OLED_I2C_ADDR);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "SSD1315 detected");

    while (!oled_init(node) ||
           !oled_clear(node) ||
           !oled_show_hello_world(node)) {
        ESP_LOGW(TAG,
                 "OLED update failed; retrying complete initialization");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "OLED now displays: Hello World");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

