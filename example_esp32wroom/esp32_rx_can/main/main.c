#include <inttypes.h>
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

#define MPU_CAN_ID_ACCEL        0x350
#define MPU_CAN_ID_GYRO         0x351
#define MPU_CAN_ID_TEMP         0x352
#define MPU_CAN_DLC             8

static const char *TAG = "esp32_rx_can";

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
    char tag[5];
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

static void format_can_data(const can_rx_item_t *item, char *buffer, size_t buffer_size)
{
    size_t used = 0;
    const uint16_t len = item->header.dlc <= TWAI_FRAME_MAX_LEN ? item->header.dlc : TWAI_FRAME_MAX_LEN;

    for (uint16_t i = 0; i < len && used < buffer_size; i++) {
        const int written = snprintf(buffer + used, buffer_size - used, "%02X%s",
                                     item->data[i], (i + 1U < len) ? " " : "");
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }

    if (used == 0 && buffer_size > 0) {
        buffer[0] = '\0';
    }
}

static void log_can_frame(const can_rx_item_t *item, uint32_t frame_count)
{
    char data_text[3 * 8] = {0};
    format_can_data(item, data_text, sizeof(data_text));

    const char *id_type = item->header.ide ? "ext" : "std";
    const char *frame_type = item->header.rtr ? "remote" : "data";

    ESP_LOGI(TAG,
             "#%" PRIu32 " %s %s id=0x%08" PRIX32 " dlc=%u ts=%" PRIu64 " data=[%s]",
             frame_count,
             id_type,
             frame_type,
             item->header.id,
             item->header.dlc,
             item->header.timestamp,
             item->header.rtr ? "" : data_text);

    printf("can_rx #%" PRIu32 " %s %s id=0x%08" PRIX32 " dlc=%u ts=%" PRIu64 " data=[%s]\r\n",
           frame_count,
           id_type,
           frame_type,
           item->header.id,
           item->header.dlc,
           item->header.timestamp,
           item->header.rtr ? "" : data_text);
}

static void print_mpu_sample_if_complete(void)
{
    if (!s_mpu_sample.has_accel || !s_mpu_sample.has_gyro || !s_mpu_sample.has_temp) {
        return;
    }

    const float temp_c = ((float)s_mpu_sample.temperature_raw / 340.0f) + 36.53f;
    ESP_LOGI(TAG,
             "MPU6050 count=%u ax=%d ay=%d az=%d gx=%d gy=%d gz=%d temp_raw=%d temp=%.2fC tag=%s",
             s_mpu_sample.sample_count,
             s_mpu_sample.accel_x,
             s_mpu_sample.accel_y,
             s_mpu_sample.accel_z,
             s_mpu_sample.gyro_x,
             s_mpu_sample.gyro_y,
             s_mpu_sample.gyro_z,
             s_mpu_sample.temperature_raw,
             temp_c,
             s_mpu_sample.tag);

    printf("MPU6050 count=%u ax=%d ay=%d az=%d gx=%d gy=%d gz=%d temp_raw=%d temp=%.2fC tag=%s\r\n",
           s_mpu_sample.sample_count,
           s_mpu_sample.accel_x,
           s_mpu_sample.accel_y,
           s_mpu_sample.accel_z,
           s_mpu_sample.gyro_x,
           s_mpu_sample.gyro_y,
           s_mpu_sample.gyro_z,
           s_mpu_sample.temperature_raw,
           temp_c,
           s_mpu_sample.tag);

    memset(&s_mpu_sample, 0, sizeof(s_mpu_sample));
}

static bool handle_mpu6050_frame(const can_rx_item_t *item)
{
    if (item->header.ide || item->header.rtr || item->header.dlc != MPU_CAN_DLC) {
        return false;
    }

    if (item->header.id != MPU_CAN_ID_ACCEL &&
        item->header.id != MPU_CAN_ID_GYRO &&
        item->header.id != MPU_CAN_ID_TEMP) {
        return false;
    }

    const uint16_t sample_count = read_u16_le(&item->data[0]);
    if (s_mpu_sample.sample_count != sample_count ||
        (!s_mpu_sample.has_accel && !s_mpu_sample.has_gyro && !s_mpu_sample.has_temp)) {
        reset_mpu_sample(sample_count);
    }

    switch (item->header.id) {
    case MPU_CAN_ID_ACCEL:
        s_mpu_sample.accel_x = read_i16_le(&item->data[2]);
        s_mpu_sample.accel_y = read_i16_le(&item->data[4]);
        s_mpu_sample.accel_z = read_i16_le(&item->data[6]);
        s_mpu_sample.has_accel = true;
        break;

    case MPU_CAN_ID_GYRO:
        s_mpu_sample.gyro_x = read_i16_le(&item->data[2]);
        s_mpu_sample.gyro_y = read_i16_le(&item->data[4]);
        s_mpu_sample.gyro_z = read_i16_le(&item->data[6]);
        s_mpu_sample.has_gyro = true;
        break;

    case MPU_CAN_ID_TEMP:
        s_mpu_sample.temperature_raw = read_i16_le(&item->data[2]);
        memcpy(s_mpu_sample.tag, &item->data[4], 4);
        s_mpu_sample.tag[4] = '\0';
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

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM CAN RX example");
    ESP_LOGI(TAG, "bitrate=%s tx_gpio=%d rx_gpio=%d", CAN_BITRATE_TEXT,
             BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("esp32_rx_can ready: bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);

    s_can_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    ESP_ERROR_CHECK(s_can_rx_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);

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

    twai_node_handle_t can_node = NULL;
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &can_node));

    twai_mask_filter_config_t accept_all_standard = {
        .id = 0,
        .mask = 0,
        .is_ext = false,
        .no_fd = true,
    };
    ESP_ERROR_CHECK(twai_node_config_mask_filter(can_node, 0, &accept_all_standard));

    twai_event_callbacks_t callbacks = {
        .on_rx_done = can_rx_done_callback,
        .on_error = can_error_callback,
        .on_state_change = can_state_change_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(can_node, &callbacks, s_can_rx_queue));

    ESP_ERROR_CHECK(twai_node_enable(can_node));

    ESP_LOGI(TAG, "TWAI driver started, waiting for CAN frames...");
    printf("esp32_rx_can started, waiting for CAN frames...\r\n");

    uint32_t frame_count = 0;
    uint32_t idle_count = 0;

    while (true) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_can_rx_queue, &item, pdMS_TO_TICKS(CAN_RX_WAIT_MS)) == pdTRUE) {
            idle_count = 0;
            frame_count++;
            if (!handle_mpu6050_frame(&item)) {
                log_can_frame(&item, frame_count);
            }
        } else {
            idle_count++;
            if ((idle_count % CAN_STATUS_PERIOD_TICKS) == 0) {
                twai_node_status_t status = {0};
                (void)twai_node_get_info(can_node, &status, NULL);
                ESP_LOGI(TAG, "no CAN frame yet, state=%d tx_err=%u rx_err=%u",
                         status.state, status.tx_error_count, status.rx_error_count);
                printf("esp32_rx_can listening, no frame yet\r\n");
            }
        }
    }
}
