#include <inttypes.h>
#include <stdbool.h>
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
#define CAN_RX_QUEUE_LEN        32
#define CAN_RX_WAIT_MS          1000
#define CAN_STATUS_PERIOD_TICKS 5

#define CAN_ID_MPU_ACCEL        0x350U
#define CAN_ID_MPU_GYRO         0x351U
#define CAN_ID_MPU_TEMP         0x352U
#define MPU_CAN_DLC             8U

#define MPU6050_ACCEL_LSB_PER_G 16384.0f
#define MPU6050_GYRO_LSB_PER_DPS 131.0f
#define STANDARD_GRAVITY_MS2    9.80665f

static const char *TAG = "esp32wroom_mpu6050";

typedef struct {
    twai_frame_header_t header;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} can_rx_item_t;

typedef struct {
    uint16_t sample_count;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temperature_raw;
    bool has_accel;
    bool has_gyro;
    bool has_temp;
} mpu6050_sample_t;

static QueueHandle_t s_can_rx_queue;
static mpu6050_sample_t s_mpu_sample;

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_i16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}

static void reset_mpu_sample(uint16_t sample_count)
{
    memset(&s_mpu_sample, 0, sizeof(s_mpu_sample));
    s_mpu_sample.sample_count = sample_count;
}

static float accel_raw_to_g(int16_t raw)
{
    return (float)raw / MPU6050_ACCEL_LSB_PER_G;
}

static float gyro_raw_to_dps(int16_t raw)
{
    return (float)raw / MPU6050_GYRO_LSB_PER_DPS;
}

static float temp_raw_to_c(int16_t raw)
{
    return ((float)raw / 340.0f) + 36.53f;
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

static void log_unhandled_can_frame(const can_rx_item_t *item, uint32_t frame_count)
{
    char data_text[3 * TWAI_FRAME_MAX_LEN] = {0};
    format_can_data(item, data_text, sizeof(data_text));

    ESP_LOGI(TAG, "CAN #%" PRIu32 " id=0x%03" PRIX32 " dlc=%u data=[%s]",
             frame_count, item->header.id, item->header.dlc, data_text);
    printf("can_rx #%" PRIu32 " id=0x%03" PRIX32 " dlc=%u data=[%s]\r\n",
           frame_count, item->header.id, item->header.dlc, data_text);
}

static void print_mpu_sample_if_complete(void)
{
    if (!s_mpu_sample.has_accel || !s_mpu_sample.has_gyro || !s_mpu_sample.has_temp) {
        return;
    }

    float ax_g = accel_raw_to_g(s_mpu_sample.accel_x);
    float ay_g = accel_raw_to_g(s_mpu_sample.accel_y);
    float az_g = accel_raw_to_g(s_mpu_sample.accel_z);
    float gx_dps = gyro_raw_to_dps(s_mpu_sample.gyro_x);
    float gy_dps = gyro_raw_to_dps(s_mpu_sample.gyro_y);
    float gz_dps = gyro_raw_to_dps(s_mpu_sample.gyro_z);
    float temp_c = temp_raw_to_c(s_mpu_sample.temperature_raw);

    ESP_LOGI(TAG,
             "#%u accel[g]=(% .3f,% .3f,% .3f) accel[m/s2]=(% .2f,% .2f,% .2f) gyro[dps]=(% .2f,% .2f,% .2f) temp=% .2fC",
             s_mpu_sample.sample_count,
             ax_g, ay_g, az_g,
             ax_g * STANDARD_GRAVITY_MS2,
             ay_g * STANDARD_GRAVITY_MS2,
             az_g * STANDARD_GRAVITY_MS2,
             gx_dps, gy_dps, gz_dps,
             temp_c);

    printf("mpu6050 #%u accel_g ax=% .3f ay=% .3f az=% .3f | accel_ms2 ax=% .2f ay=% .2f az=% .2f | gyro_dps gx=% .2f gy=% .2f gz=% .2f | temp=% .2fC\r\n",
           s_mpu_sample.sample_count,
           ax_g, ay_g, az_g,
           ax_g * STANDARD_GRAVITY_MS2,
           ay_g * STANDARD_GRAVITY_MS2,
           az_g * STANDARD_GRAVITY_MS2,
           gx_dps, gy_dps, gz_dps,
           temp_c);

    memset(&s_mpu_sample, 0, sizeof(s_mpu_sample));
}

static bool handle_mpu6050_frame(const can_rx_item_t *item)
{
    if (item->header.ide || item->header.rtr || item->header.dlc != MPU_CAN_DLC) {
        return false;
    }

    if (item->header.id != CAN_ID_MPU_ACCEL &&
        item->header.id != CAN_ID_MPU_GYRO &&
        item->header.id != CAN_ID_MPU_TEMP) {
        return false;
    }

    uint16_t sample_count = read_u16_le(&item->data[0]);
    if (s_mpu_sample.sample_count != sample_count ||
        (!s_mpu_sample.has_accel && !s_mpu_sample.has_gyro && !s_mpu_sample.has_temp)) {
        reset_mpu_sample(sample_count);
    }

    switch (item->header.id) {
    case CAN_ID_MPU_ACCEL:
        s_mpu_sample.accel_x = read_i16_le(&item->data[2]);
        s_mpu_sample.accel_y = read_i16_le(&item->data[4]);
        s_mpu_sample.accel_z = read_i16_le(&item->data[6]);
        s_mpu_sample.has_accel = true;
        break;

    case CAN_ID_MPU_GYRO:
        s_mpu_sample.gyro_x = read_i16_le(&item->data[2]);
        s_mpu_sample.gyro_y = read_i16_le(&item->data[4]);
        s_mpu_sample.gyro_z = read_i16_le(&item->data[6]);
        s_mpu_sample.has_gyro = true;
        break;

    case CAN_ID_MPU_TEMP:
        s_mpu_sample.temperature_raw = read_i16_le(&item->data[2]);
        s_mpu_sample.has_temp = true;
        break;

    default:
        return false;
    }

    print_mpu_sample_if_complete();
    return true;
}

static bool IRAM_ATTR can_rx_done_callback(twai_node_handle_t handle,
                                           const twai_rx_done_event_data_t *edata,
                                           void *user_ctx)
{
    (void)edata;
    QueueHandle_t rx_queue = (QueueHandle_t)user_ctx;
    BaseType_t woken = pdFALSE;

    can_rx_item_t item = {0};
    twai_frame_t frame = {
        .buffer = item.data,
        .buffer_len = sizeof(item.data),
    };

    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        item.header = frame.header;
        if (xQueueSendFromISR(rx_queue, &item, &woken) != pdTRUE) {
            ESP_EARLY_LOGW(TAG, "RX queue full, dropping CAN frame");
        }
    }

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
        .fail_retry_cnt = -1,
        .tx_queue_depth = 1,
    };

    twai_node_handle_t node = NULL;
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node));

    twai_mask_filter_config_t accept_mpu6050 = {
        .id = CAN_ID_MPU_ACCEL,
        .mask = 0x7FC,
        .is_ext = false,
        .no_fd = true,
    };
    ESP_ERROR_CHECK(twai_node_config_mask_filter(node, 0, &accept_mpu6050));

    twai_event_callbacks_t callbacks = {
        .on_rx_done = can_rx_done_callback,
        .on_error = can_error_callback,
        .on_state_change = can_state_change_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, s_can_rx_queue));
    ESP_ERROR_CHECK(twai_node_enable(node));
    return node;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM MPU6050 over CH32 CAN gateway demo");
    ESP_LOGI(TAG, "CAN bitrate=%s tx_gpio=%d rx_gpio=%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("\r\n==== ESP32-WROOM MPU6050 CAN gateway demo ====\r\n");
    printf("MPU6050 -> CH32 I2C, CH32 -> ESP32 CAN\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("Waiting for CH32 frames: 0x350 accel, 0x351 angular velocity, 0x352 temperature\r\n\r\n");

    s_can_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    ESP_ERROR_CHECK(s_can_rx_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    twai_node_handle_t can_node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, waiting for MPU6050 frames from CH32 gateway");
    printf("twai started, waiting for MPU6050 frames from CH32 gateway\r\n");

    uint32_t frame_count = 0;
    uint32_t idle_count = 0;

    while (true) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_can_rx_queue, &item, pdMS_TO_TICKS(CAN_RX_WAIT_MS)) == pdTRUE) {
            idle_count = 0;
            frame_count++;
            if (!handle_mpu6050_frame(&item)) {
                log_unhandled_can_frame(&item, frame_count);
            }
        } else {
            idle_count++;
            if ((idle_count % CAN_STATUS_PERIOD_TICKS) == 0) {
                twai_node_status_t status = {0};
                (void)twai_node_get_info(can_node, &status, NULL);
                ESP_LOGI(TAG, "no MPU6050 CAN frame yet, state=%d tx_err=%u rx_err=%u",
                         status.state, status.tx_error_count, status.rx_error_count);
                printf("waiting for MPU6050 CAN frames, state=%d tx_err=%u rx_err=%u\r\n",
                       status.state, status.tx_error_count, status.rx_error_count);
            }
        }
    }
}
