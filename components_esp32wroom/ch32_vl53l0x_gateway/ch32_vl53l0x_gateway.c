#include "ch32_vl53l0x_gateway.h"

#include <string.h>

#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define TAG "ch32_vl53l0x"

#define CAN_ID_STATUS(node)      (0x100U + (uint32_t)(node))
#define CAN_ID_CMD(node)         (0x200U + (uint32_t)(node))
#define CAN_ID_ACK(node)         (0x500U + (uint32_t)(node))
#define CAN_ID_HELLO(node)       (0x700U + (uint32_t)(node))

#define CAN_RX_QUEUE_LEN         16U
#define CAN_TX_DONE_QUEUE_LEN    8U
#define CAN_TX_QUEUE_DEPTH       8U

#define CMD_SCAN                 0x01U
#define CMD_PROBE                0x02U
#define CMD_WRITE_REG            0x03U
#define CMD_READ_REGS            0x04U

#define STATUS_PROBE             0x02U
#define STATUS_WRITE             0x03U
#define STATUS_READ              0x04U

#define REG_SYSRANGE_START       0x00U
#define REG_RESULT_RANGE_STATUS  0x14U
#define REG_MODEL_ID             0xC0U
#define BRIDGE_MAX_READ_BYTES    3U

typedef struct {
    uint8_t reg;
    uint8_t value;
} reg_write_t;

typedef struct {
    twai_frame_header_t header;
    uint8_t data[8];
} can_rx_item_t;

typedef struct {
    bool success;
    uint32_t id;
} can_tx_done_item_t;

typedef struct {
    QueueHandle_t rx_queue;
    QueueHandle_t tx_done_queue;
} can_context_t;

static const reg_write_t k_init_table[] = {
    {0x88, 0x00}, {0x80, 0x01}, {0xFF, 0x01}, {0x00, 0x00},
    {0x91, 0x3C}, {0x00, 0x01}, {0xFF, 0x00}, {0x80, 0x00},
    {0x60, 0x00}, {0x44, 0x00}, {0x01, 0xFF}, {0x80, 0x01},
    {0xFF, 0x01}, {0x00, 0x00}, {0x91, 0x3C}, {0x00, 0x01},
    {0xFF, 0x00}, {0x80, 0x00},
};

static ch32_vl53l0x_config_t s_cfg;
static ch32_vl53l0x_status_t s_status;
static twai_node_handle_t s_node;
static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;

static uint64_t deadline_us(uint32_t timeout_ms)
{
    return (uint64_t)esp_timer_get_time() + ((uint64_t)timeout_ms * 1000ULL);
}

static bool before_deadline(uint64_t deadline)
{
    return (uint64_t)esp_timer_get_time() < deadline;
}

static bool IRAM_ATTR on_rx_done(twai_node_handle_t handle,
                                 const twai_rx_done_event_data_t *edata,
                                 void *user_ctx)
{
    (void)edata;
    can_context_t *ctx = (can_context_t *)user_ctx;
    BaseType_t woken = pdFALSE;
    can_rx_item_t item = { 0 };
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
    (void)edata;
    (void)user_ctx;
    return false;
}

static esp_err_t send_cmd(const uint8_t *data, uint8_t len)
{
    if (s_node == NULL || data == NULL || len == 0 || len > 8) {
        return ESP_ERR_INVALID_ARG;
    }

    twai_frame_t frame = {
        .header = {
            .id = CAN_ID_CMD(s_cfg.ch32_node_id),
            .dlc = len,
        },
        .buffer = (uint8_t *)data,
        .buffer_len = len,
    };

    esp_err_t err = twai_node_transmit(s_node, &frame, pdMS_TO_TICKS(s_cfg.command_timeout_ms));
    if (err != ESP_OK) {
        s_status.error_count++;
        s_status.last_result = CH32_VL53L0X_RESULT_COMM_FAIL;
        return err;
    }

    can_tx_done_item_t done = { 0 };
    if (xQueueReceive(s_tx_done_queue, &done, pdMS_TO_TICKS(s_cfg.command_timeout_ms)) != pdTRUE || !done.success) {
        s_status.error_count++;
        s_status.last_result = CH32_VL53L0X_RESULT_COMM_FAIL;
        return ESP_FAIL;
    }

    s_status.can_tx_count++;
    return ESP_OK;
}

static bool rx_frame(can_rx_item_t *item, uint32_t wait_ms)
{
    if (xQueueReceive(s_rx_queue, item, pdMS_TO_TICKS(wait_ms)) == pdTRUE) {
        s_status.can_rx_count++;
        return true;
    }
    return false;
}

static bool bridge_probe(uint8_t addr7)
{
    uint8_t cmd[2] = { CMD_PROBE, addr7 };
    if (send_cmd(cmd, sizeof(cmd)) != ESP_OK) {
        return false;
    }

    uint64_t deadline = deadline_us(s_cfg.command_timeout_ms);
    while (before_deadline(deadline)) {
        can_rx_item_t frame = { 0 };
        if (!rx_frame(&frame, 20)) {
            continue;
        }
        if (frame.header.id == CAN_ID_ACK(s_cfg.ch32_node_id) && frame.header.dlc >= 3 && frame.data[1] == CMD_PROBE) {
            if (frame.data[2] != 0) {
                return false;
            }
        }
        if (frame.header.id == CAN_ID_STATUS(s_cfg.ch32_node_id) && frame.header.dlc >= 4 &&
            frame.data[0] == STATUS_PROBE && frame.data[1] == addr7) {
            return frame.data[2] != 0;
        }
    }
    return false;
}

static bool bridge_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t cmd[5] = { CMD_WRITE_REG, s_cfg.tof_addr7, reg, 1U, value };
    if (send_cmd(cmd, sizeof(cmd)) != ESP_OK) {
        return false;
    }

    uint64_t deadline = deadline_us(s_cfg.command_timeout_ms);

    while (before_deadline(deadline)) {
        can_rx_item_t frame = { 0 };
        if (!rx_frame(&frame, 20)) {
            continue;
        }
        if (frame.header.id == CAN_ID_STATUS(s_cfg.ch32_node_id) && frame.header.dlc >= 5 &&
            frame.data[0] == STATUS_WRITE && frame.data[1] == s_cfg.tof_addr7 && frame.data[2] == reg) {
            return frame.data[4] != 0;
        }
    }
    return false;
}

static bool bridge_read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }

    uint8_t done = 0;
    while (done < len) {
        uint8_t chunk = (uint8_t)(len - done);
        if (chunk > BRIDGE_MAX_READ_BYTES) {
            chunk = BRIDGE_MAX_READ_BYTES;
        }

        uint8_t this_reg = (uint8_t)(reg + done);
        uint8_t cmd[4] = { CMD_READ_REGS, s_cfg.tof_addr7, this_reg, chunk };
        if (send_cmd(cmd, sizeof(cmd)) != ESP_OK) {
            return false;
        }

        uint64_t deadline = deadline_us(s_cfg.command_timeout_ms);

        bool got = false;
        while (before_deadline(deadline)) {
            can_rx_item_t frame = { 0 };
            if (!rx_frame(&frame, 20)) {
                continue;
            }
            if (frame.header.id == CAN_ID_STATUS(s_cfg.ch32_node_id) && frame.header.dlc >= (uint8_t)(5 + chunk) &&
                frame.data[0] == STATUS_READ && frame.data[1] == s_cfg.tof_addr7 && frame.data[2] == this_reg &&
                frame.data[3] == chunk && frame.data[4] != 0) {
                memcpy(&data[done], &frame.data[5], chunk);
                got = true;
                break;
            }
        }

        if (!got) {
            return false;
        }
        done = (uint8_t)(done + chunk);
    }

    return true;
}

static ch32_vl53l0x_result_t set_result(ch32_vl53l0x_result_t result)
{
    s_status.last_result = result;
    if (result != CH32_VL53L0X_RESULT_OK && result != CH32_VL53L0X_RESULT_OUT_OF_RANGE) {
        s_status.error_count++;
        s_status.state = CH32_VL53L0X_STATE_ERROR;
    }
    return result;
}

void ch32_vl53l0x_default_config(ch32_vl53l0x_config_t *config)
{
    if (config == NULL) {
        return;
    }
    config->can_tx_gpio = BOARD_CAN_TX_GPIO;
    config->can_rx_gpio = BOARD_CAN_RX_GPIO;
    config->ch32_node_id = CH32_VL53L0X_DEFAULT_NODE_ID;
    config->tof_addr7 = CH32_VL53L0X_DEFAULT_I2C_ADDR7;
    config->command_timeout_ms = 300U;
    config->gateway_wait_ms = 1500U;
}

esp_err_t ch32_vl53l0x_init(const ch32_vl53l0x_config_t *config)
{
    ch32_vl53l0x_config_t local;
    if (config == NULL) {
        ch32_vl53l0x_default_config(&local);
        config = &local;
    }
    s_cfg = *config;
    memset(&s_status, 0, sizeof(s_status));
    s_status.state = CH32_VL53L0X_STATE_RESET;
    s_status.ch32_node_id = s_cfg.ch32_node_id;
    s_status.tof_addr7 = s_cfg.tof_addr7;
    s_status.last_result = CH32_VL53L0X_RESULT_NOT_READY;

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    if (s_rx_queue == NULL || s_tx_done_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = s_cfg.can_tx_gpio,
            .rx = s_cfg.can_rx_gpio,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = CH32_VL53L0X_CAN_BITRATE_HZ,
            .sp_permill = 800,
        },
        .timestamp_resolution_hz = 1000000,
        .fail_retry_cnt = 3,
        .tx_queue_depth = CAN_TX_QUEUE_DEPTH,
    };

    ESP_RETURN_ON_ERROR(twai_new_node_onchip(&node_config, &s_node), TAG, "twai_new_node_onchip failed");

    twai_mask_filter_config_t accept_all = {
        .id = 0,
        .mask = 0,
        .is_ext = false,
        .no_fd = true,
    };
    ESP_RETURN_ON_ERROR(twai_node_config_mask_filter(s_node, 0, &accept_all), TAG, "twai_node_config_mask_filter failed");

    twai_event_callbacks_t callbacks = {
        .on_rx_done = on_rx_done,
        .on_tx_done = on_tx_done,
        .on_error = on_error,
    };
    ESP_RETURN_ON_ERROR(twai_node_register_event_callbacks(s_node, &callbacks, &s_can_ctx), TAG, "twai_node_register_event_callbacks failed");
    ESP_RETURN_ON_ERROR(twai_node_enable(s_node), TAG, "twai_node_enable failed");

    s_status.state = CH32_VL53L0X_STATE_CAN_READY;
    return ESP_OK;
}

esp_err_t ch32_vl53l0x_wait_gateway(uint32_t timeout_ms)
{
    uint64_t deadline = deadline_us(timeout_ms == 0 ? s_cfg.gateway_wait_ms : timeout_ms);
    while (before_deadline(deadline)) {
        can_rx_item_t frame = { 0 };
        if (!rx_frame(&frame, 50)) {
            continue;
        }
        if (frame.header.id == CAN_ID_HELLO(s_cfg.ch32_node_id)) {
            s_status.gateway_online = true;
            s_status.state = CH32_VL53L0X_STATE_GATEWAY_ONLINE;
            return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}

ch32_vl53l0x_result_t ch32_vl53l0x_probe(void)
{
    bool found = bridge_probe(s_cfg.tof_addr7);
    s_status.device_found = found;
    if (!found) {
        return set_result(CH32_VL53L0X_RESULT_ADDR_NOT_FOUND);
    }
    s_status.state = CH32_VL53L0X_STATE_DEVICE_FOUND;
    return set_result(CH32_VL53L0X_RESULT_OK);
}

ch32_vl53l0x_result_t ch32_vl53l0x_begin(void)
{
    ch32_vl53l0x_result_t probe = ch32_vl53l0x_probe();
    if (probe != CH32_VL53L0X_RESULT_OK) {
        return probe;
    }

    uint8_t model = 0;
    if (!bridge_read_regs(REG_MODEL_ID, &model, 1)) {
        return set_result(CH32_VL53L0X_RESULT_MODEL_ID_READ_FAIL);
    }
    s_status.model_id = model;
    if (model != CH32_VL53L0X_EXPECTED_MODEL_ID) {
        return set_result(CH32_VL53L0X_RESULT_MODEL_ID_MISMATCH);
    }

    for (size_t i = 0; i < sizeof(k_init_table) / sizeof(k_init_table[0]); ++i) {
        if (!bridge_write_reg(k_init_table[i].reg, k_init_table[i].value)) {
            return set_result(CH32_VL53L0X_RESULT_CONFIG_FAIL);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    s_status.initialized = true;
    s_status.state = CH32_VL53L0X_STATE_READY;
    return set_result(CH32_VL53L0X_RESULT_OK);
}

ch32_vl53l0x_result_t ch32_vl53l0x_read_distance(uint16_t *distance_mm, uint16_t *raw_distance_mm)
{
    if (distance_mm == NULL) {
        return set_result(CH32_VL53L0X_RESULT_BAD_ARG);
    }
    if (!s_status.initialized) {
        return set_result(CH32_VL53L0X_RESULT_NOT_READY);
    }

    if (!bridge_write_reg(REG_SYSRANGE_START, 0x01)) {
        return set_result(CH32_VL53L0X_RESULT_COMM_FAIL);
    }

    uint8_t status = 0;
    int ready_try = 0;
    for (; ready_try < 25; ++ready_try) {
        if (!bridge_read_regs(REG_RESULT_RANGE_STATUS, &status, 1)) {
            return set_result(CH32_VL53L0X_RESULT_COMM_FAIL);
        }
        if ((status & 0x01U) != 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (ready_try >= 25) {
        return set_result(CH32_VL53L0X_RESULT_MEASURE_TIMEOUT);
    }

    uint8_t buf[2] = { 0 };
    if (!bridge_read_regs((uint8_t)(REG_RESULT_RANGE_STATUS + 10U), buf, sizeof(buf))) {
        return set_result(CH32_VL53L0X_RESULT_COMM_FAIL);
    }
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    s_status.last_raw_distance_mm = raw;
    if (raw_distance_mm != NULL) {
        *raw_distance_mm = raw;
    }

    if (raw == 0 || raw == 0xFFFFU || raw > 2000U) {
        s_status.last_distance_mm = raw;
        *distance_mm = raw;
        return set_result(CH32_VL53L0X_RESULT_OUT_OF_RANGE);
    }

    s_status.last_distance_mm = raw;
    *distance_mm = raw;
    return set_result(CH32_VL53L0X_RESULT_OK);
}

void ch32_vl53l0x_get_status(ch32_vl53l0x_status_t *status)
{
    if (status != NULL) {
        *status = s_status;
    }
}

const char *ch32_vl53l0x_state_text(ch32_vl53l0x_state_t state)
{
    switch (state) {
    case CH32_VL53L0X_STATE_RESET: return "RESET";
    case CH32_VL53L0X_STATE_CAN_READY: return "CAN_READY";
    case CH32_VL53L0X_STATE_GATEWAY_ONLINE: return "GATEWAY_ONLINE";
    case CH32_VL53L0X_STATE_DEVICE_FOUND: return "DEVICE_FOUND";
    case CH32_VL53L0X_STATE_READY: return "READY";
    case CH32_VL53L0X_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

const char *ch32_vl53l0x_result_text(ch32_vl53l0x_result_t result)
{
    switch (result) {
    case CH32_VL53L0X_RESULT_OK: return "OK";
    case CH32_VL53L0X_RESULT_OUT_OF_RANGE: return "OUT_OF_RANGE";
    case CH32_VL53L0X_RESULT_ADDR_NOT_FOUND: return "ADDR_NOT_FOUND";
    case CH32_VL53L0X_RESULT_MODEL_ID_READ_FAIL: return "MODEL_ID_READ_FAIL";
    case CH32_VL53L0X_RESULT_MODEL_ID_MISMATCH: return "MODEL_ID_MISMATCH";
    case CH32_VL53L0X_RESULT_CONFIG_FAIL: return "CONFIG_FAIL";
    case CH32_VL53L0X_RESULT_START_TIMEOUT: return "START_TIMEOUT";
    case CH32_VL53L0X_RESULT_MEASURE_TIMEOUT: return "MEASURE_TIMEOUT";
    case CH32_VL53L0X_RESULT_COMM_FAIL: return "COMM_FAIL";
    case CH32_VL53L0X_RESULT_NOT_READY: return "NOT_READY";
    case CH32_VL53L0X_RESULT_BAD_ARG: return "BAD_ARG";
    default: return "UNKNOWN";
    }
}


