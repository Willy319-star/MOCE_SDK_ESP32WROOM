#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "ch32_mpu6050_gateway.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_RX_QUEUE_LEN             48U
#define CAN_TX_DONE_QUEUE_LEN        4U
#define CAN_TX_QUEUE_DEPTH           4U
#define CAN_TIMEOUT_MS               1000U

#define CH32_NODE_ID                 CH32_MPU6050_DEFAULT_NODE_ID
#define MPU6050_I2C_ADDR             CH32_MPU6050_DEFAULT_I2C_ADDRESS

#define MPU6050_REG_SMPLRT_DIV       0x19U
#define MPU6050_REG_CONFIG           0x1AU
#define MPU6050_REG_GYRO_CONFIG      0x1BU
#define MPU6050_REG_ACCEL_CONFIG     0x1CU
#define MPU6050_REG_ACCEL_XOUT_H     0x3BU
#define MPU6050_REG_PWR_MGMT_1       0x6BU
#define MPU6050_REG_WHO_AM_I         0x75U
#define MPU6050_BURST_LEN            14U
#define MPU6050_SAMPLE_PERIOD_MS     500U

#define MPU6050_ACCEL_LSB_PER_G      16384.0f
#define MPU6050_GYRO_LSB_PER_DPS     131.0f
#define STANDARD_GRAVITY_MS2         9.80665f

static const char *TAG = "mpu6050_ch32_gateway";

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
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temperature;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_raw_t;

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;
static uint8_t s_next_request_id = 1U;
static uint32_t s_sample_count;

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
            .bitrate = CH32_MPU6050_CAN_BITRATE,
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
            .id = CH32_MPU6050_CAN_ID_I2C_CMD(CH32_NODE_ID),
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
        done.id != CH32_MPU6050_CAN_ID_I2C_CMD(CH32_NODE_ID)) {
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
                     "Waiting for CH32 MPU6050 node %u "
                     "(HELLO id 0x%03X)",
                     CH32_NODE_ID,
                     (unsigned)CH32_MPU6050_CAN_ID_HELLO(
                         CH32_NODE_ID));
            continue;
        }
        if (item.header.ide || item.header.rtr) {
            continue;
        }

        ch32_mpu6050_hello_t hello = {0};
        if (ch32_mpu6050_parse_hello(
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

static bool parse_target_ack(const can_rx_item_t *item,
                             uint8_t command,
                             bool *result)
{
    if (item->header.ide || item->header.rtr) {
        return false;
    }

    ch32_mpu6050_ack_t ack = {0};
    if (!ch32_mpu6050_parse_ack(
            item->header.id, item->data, item->header.dlc,
            CH32_NODE_ID, command, &ack)) {
        return false;
    }
    *result = ack.result;
    return true;
}

static bool wait_for_ack(uint8_t command)
{
    TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(CAN_TIMEOUT_MS);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        bool result = false;
        if (xQueueReceive(s_rx_queue, &item,
                          pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }
        if (parse_target_ack(&item, command, &result)) {
            return result;
        }
    }

    ESP_LOGW(TAG, "ACK timeout for command 0x%02X", command);
    return false;
}

static bool gateway_simple_command(twai_node_handle_t node,
                                   uint8_t command,
                                   const uint8_t *data,
                                   uint8_t len)
{
    if (send_can_frame(node, data, len) != ESP_OK) {
        return false;
    }
    return wait_for_ack(command);
}

static bool gateway_probe(twai_node_handle_t node)
{
    uint8_t data[2] = {0};
    if (!ch32_mpu6050_build_probe(MPU6050_I2C_ADDR, data)) {
        return false;
    }
    return gateway_simple_command(
        node, CH32_MPU6050_I2C_CMD_PROBE, data, sizeof(data));
}

static bool gateway_write_reg(twai_node_handle_t node,
                              uint8_t reg,
                              uint8_t value)
{
    uint8_t data[5] = {0};
    if (!ch32_mpu6050_build_write_reg(
            MPU6050_I2C_ADDR, reg, value, data)) {
        return false;
    }
    return gateway_simple_command(
        node, CH32_MPU6050_I2C_CMD_WRITE_REG,
        data, sizeof(data));
}

static uint8_t next_request_id(void)
{
    uint8_t request_id = s_next_request_id++;
    if (s_next_request_id == 0U) {
        s_next_request_id = 1U;
    }
    return request_id;
}

static bool gateway_read_regs(twai_node_handle_t node,
                              uint8_t reg,
                              uint8_t *out,
                              uint8_t len)
{
    uint8_t request_id = next_request_id();
    uint8_t command[5] = {0};
    bool received[32] = {false};
    bool read_done = false;
    bool read_ok = false;
    bool ack_received = false;
    bool ack_ok = false;
    uint8_t reported_count = 0U;
    TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(CAN_TIMEOUT_MS);

    if (out == NULL || len == 0U || len > sizeof(received) ||
        !ch32_mpu6050_build_read_regs(
            MPU6050_I2C_ADDR, reg, len, request_id, command)) {
        return false;
    }
    memset(out, 0, len);

    if (send_can_frame(node, command, sizeof(command)) != ESP_OK) {
        return false;
    }

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item,
                          pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }
        if (item.header.ide || item.header.rtr) {
            continue;
        }

        ch32_mpu6050_read_chunk_t chunk = {0};
        if (ch32_mpu6050_parse_read_chunk(
                item.header.id, item.data, item.header.dlc,
                CH32_NODE_ID, &chunk) &&
            chunk.request_id == request_id &&
            (uint16_t)chunk.offset + chunk.length <= len) {
            memcpy(&out[chunk.offset], chunk.data, chunk.length);
            for (uint8_t i = 0U; i < chunk.length; ++i) {
                received[(uint8_t)(chunk.offset + i)] = true;
            }
        }

        ch32_mpu6050_read_done_t done = {0};
        if (ch32_mpu6050_parse_read_done(
                item.header.id, item.data, item.header.dlc,
                CH32_NODE_ID, &done) &&
            done.request_id == request_id &&
            done.i2c_address == MPU6050_I2C_ADDR &&
            done.reg == reg &&
            done.requested_length == len) {
            read_done = true;
            read_ok = done.result;
            reported_count = done.processed_length;
        }

        bool this_ack_ok = false;
        if (parse_target_ack(
                &item, CH32_MPU6050_I2C_CMD_READ_REGS,
                &this_ack_ok)) {
            ack_received = true;
            ack_ok = this_ack_ok;
        }

        if (read_done && ack_received) {
            break;
        }
    }

    if (!read_done || !read_ok ||
        !ack_received || !ack_ok ||
        reported_count != len) {
        ESP_LOGW(TAG,
                 "Read reg 0x%02X failed: done=%u read_ok=%u "
                 "ack=%u ack_ok=%u bytes=%u/%u",
                 reg, read_done, read_ok, ack_received, ack_ok,
                 reported_count, len);
        return false;
    }

    for (uint8_t i = 0U; i < len; ++i) {
        if (!received[i]) {
            ESP_LOGW(TAG,
                     "Read reg 0x%02X missing byte offset %u",
                     reg, i);
            return false;
        }
    }
    return true;
}

static int16_t read_i16_be(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static bool mpu6050_init(twai_node_handle_t node)
{
    uint8_t who_am_i = 0U;

    if (!gateway_write_reg(
            node, MPU6050_REG_PWR_MGMT_1, 0x00U)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!gateway_write_reg(
            node, MPU6050_REG_SMPLRT_DIV, 0x07U) ||
        !gateway_write_reg(
            node, MPU6050_REG_CONFIG, 0x03U) ||
        !gateway_write_reg(
            node, MPU6050_REG_GYRO_CONFIG, 0x00U) ||
        !gateway_write_reg(
            node, MPU6050_REG_ACCEL_CONFIG, 0x00U)) {
        return false;
    }

    if (!gateway_read_regs(
            node, MPU6050_REG_WHO_AM_I, &who_am_i, 1U)) {
        return false;
    }
    if (who_am_i != 0x68U) {
        ESP_LOGW(TAG, "Unexpected MPU6050 WHO_AM_I=0x%02X",
                 who_am_i);
        return false;
    }

    ESP_LOGI(TAG, "MPU6050 initialized, WHO_AM_I=0x%02X",
             who_am_i);
    return true;
}

static bool mpu6050_read(twai_node_handle_t node,
                         mpu6050_raw_t *raw)
{
    uint8_t data[MPU6050_BURST_LEN] = {0};

    if (raw == NULL ||
        !gateway_read_regs(
            node, MPU6050_REG_ACCEL_XOUT_H,
            data, sizeof(data))) {
        return false;
    }

    raw->accel_x = read_i16_be(&data[0]);
    raw->accel_y = read_i16_be(&data[2]);
    raw->accel_z = read_i16_be(&data[4]);
    raw->temperature = read_i16_be(&data[6]);
    raw->gyro_x = read_i16_be(&data[8]);
    raw->gyro_y = read_i16_be(&data[10]);
    raw->gyro_z = read_i16_be(&data[12]);
    return true;
}

static void print_sample(const mpu6050_raw_t *raw)
{
    float ax_g =
        (float)raw->accel_x / MPU6050_ACCEL_LSB_PER_G;
    float ay_g =
        (float)raw->accel_y / MPU6050_ACCEL_LSB_PER_G;
    float az_g =
        (float)raw->accel_z / MPU6050_ACCEL_LSB_PER_G;
    float gx_dps =
        (float)raw->gyro_x / MPU6050_GYRO_LSB_PER_DPS;
    float gy_dps =
        (float)raw->gyro_y / MPU6050_GYRO_LSB_PER_DPS;
    float gz_dps =
        (float)raw->gyro_z / MPU6050_GYRO_LSB_PER_DPS;
    float temp_c =
        ((float)raw->temperature / 340.0f) + 36.53f;

    ESP_LOGI(TAG,
             "#%" PRIu32
             " accel[g]=(% .3f,% .3f,% .3f) "
             "accel[m/s2]=(% .2f,% .2f,% .2f) "
             "gyro[dps]=(% .2f,% .2f,% .2f) temp=% .2fC",
             ++s_sample_count,
             ax_g, ay_g, az_g,
             ax_g * STANDARD_GRAVITY_MS2,
             ay_g * STANDARD_GRAVITY_MS2,
             az_g * STANDARD_GRAVITY_MS2,
             gx_dps, gy_dps, gz_dps, temp_c);

    printf("mpu6050 #%-5" PRIu32
           " ax=% .3fg ay=% .3fg az=% .3fg "
           "gx=% .2fdps gy=% .2fdps gz=% .2fdps "
           "temp=% .2fC\r\n",
           s_sample_count,
           ax_g, ay_g, az_g,
           gx_dps, gy_dps, gz_dps, temp_c);
}

void app_main(void)
{
    ESP_LOGI(TAG,
             "ESP32 -> CAN -> CH32 node %u -> MPU6050",
             CH32_NODE_ID);
    ESP_LOGI(TAG,
             "CAN 50 kbit/s TX=GPIO%d RX=GPIO%d, "
             "MPU6050 address=0x%02X",
             BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO,
             MPU6050_I2C_ADDR);

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
                 "MPU6050 not responding at address "
                 "0x%02X; retrying",
                 MPU6050_I2C_ADDR);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (!mpu6050_init(node)) {
        ESP_LOGW(TAG,
                 "MPU6050 initialization failed; retrying");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        mpu6050_raw_t raw = {0};
        vTaskDelayUntil(
            &last_wake,
            pdMS_TO_TICKS(MPU6050_SAMPLE_PERIOD_MS));
        if (mpu6050_read(node, &raw)) {
            print_sample(&raw);
        } else {
            ESP_LOGW(TAG, "MPU6050 sample read failed");
        }
    }
}

