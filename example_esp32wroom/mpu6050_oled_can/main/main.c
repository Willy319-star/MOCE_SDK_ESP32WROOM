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

#define CAN_BITRATE              50000
#define CAN_BITRATE_TEXT         "50 kbit/s"
#define CAN_RX_QUEUE_LEN         48
#define CAN_TX_QUEUE_DEPTH       4
#define CAN_TX_DONE_QUEUE_LEN    4
#define CAN_RX_WAIT_MS           1000
#define CAN_CMD_TIMEOUT_MS       300
#define CAN_STATUS_PERIOD_TICKS  5

#define CAN_ID_MPU_ACCEL         0x350U
#define CAN_ID_MPU_GYRO          0x351U
#define CAN_ID_MPU_TEMP          0x352U
#define CAN_ID_OLED_CLEAR        0x410U
#define CAN_ID_OLED_TEXT         0x411U
#define CAN_ID_GATEWAY_ACK       0x500U
#define MPU_CAN_DLC              8U

#define OLED_FONT_WIDTH          6U
#define OLED_TEXT_MAX_PER_FRAME  6U
#define OLED_UPDATE_MIN_MS       1000U

#define MPU6050_ACCEL_LSB_PER_G  16384.0f
#define MPU6050_GYRO_LSB_PER_DPS 131.0f
#define STANDARD_GRAVITY_MS2     9.80665f

static const char *TAG = "mpu6050_oled_can";

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

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;
static mpu6050_sample_t s_mpu_sample;
static TickType_t s_last_oled_update_tick;

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_i16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
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

static void reset_mpu_sample(uint16_t sample_count)
{
    memset(&s_mpu_sample, 0, sizeof(s_mpu_sample));
    s_mpu_sample.sample_count = sample_count;
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
        if (xQueueSendFromISR(ctx->rx_queue, &item, &woken) != pdTRUE) {
            ESP_EARLY_LOGW(TAG, "RX queue full, dropping CAN frame");
        }
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

    if (!done.success) {
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " failed on bus", id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static bool is_gateway_ack(const can_rx_item_t *item, uint16_t source_id, uint8_t code)
{
    return !item->header.ide && !item->header.rtr &&
           item->header.id == CAN_ID_GATEWAY_ACK &&
           item->header.dlc >= 4 &&
           item->data[0] == code &&
           read_u16_le(&item->data[1]) == source_id;
}

static bool wait_for_gateway_ack(uint16_t source_id, uint8_t code)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }
        if (is_gateway_ack(&item, source_id, code)) {
            return item.data[3] != 0;
        }
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

    /*
     * The CH32 gateway ACKs every OLED command, but MPU6050 frames are also
     * arriving periodically on the same CAN queue. Waiting for every text ACK
     * makes a full-screen refresh fragile at 50 kbit/s, so text refreshes use
     * CAN-level TX success and a short pacing delay instead.
     */
    vTaskDelay(pdMS_TO_TICKS(8));
    return true;
}

static bool send_oled_line(twai_node_handle_t node, uint8_t page, const char *text)
{
    size_t len = strlen(text);
    bool ok = true;

    for (size_t off = 0; off < len; off += OLED_TEXT_MAX_PER_FRAME) {
        size_t chunk_len = len - off;
        if (chunk_len > OLED_TEXT_MAX_PER_FRAME) {
            chunk_len = OLED_TEXT_MAX_PER_FRAME;
        }
        ok = send_oled_text_chunk(node, page, (uint8_t)(off * OLED_FONT_WIDTH),
                                  &text[off], chunk_len) && ok;
    }

    return ok;
}

static void format_signed_scaled(char *out, size_t out_size, const char *label,
                                 float value, float scale)
{
    int scaled = (int)(value * scale);
    char sign = 'P';

    if (scaled < 0) {
        sign = 'N';
        scaled = -scaled;
    }

    if (scaled > 999) {
        scaled = 999;
    }

    snprintf(out, out_size, "%s%c%03d", label, sign, scaled);
}

static void format_temp(char *out, size_t out_size, float temp_c)
{
    int whole = (int)temp_c;

    if (whole < 0) {
        whole = 0;
    } else if (whole > 99) {
        whole = 99;
    }

    snprintf(out, out_size, "T%02dC  ", whole);
}

static bool update_oled(twai_node_handle_t node, const mpu6050_sample_t *sample)
{
    char line[8][8] = {0};
    float ax_g = accel_raw_to_g(sample->accel_x);
    float ay_g = accel_raw_to_g(sample->accel_y);
    float az_g = accel_raw_to_g(sample->accel_z);
    float gx_dps = gyro_raw_to_dps(sample->gyro_x);
    float gy_dps = gyro_raw_to_dps(sample->gyro_y);
    float gz_dps = gyro_raw_to_dps(sample->gyro_z);
    float temp_c = temp_raw_to_c(sample->temperature_raw);
    bool ok = true;

    snprintf(line[0], sizeof(line[0]), "MPU%03u", (unsigned)(sample->sample_count % 1000U));
    format_signed_scaled(line[1], sizeof(line[1]), "AX", ax_g, 100.0f);
    format_signed_scaled(line[2], sizeof(line[2]), "AY", ay_g, 100.0f);
    format_signed_scaled(line[3], sizeof(line[3]), "AZ", az_g, 100.0f);
    format_signed_scaled(line[4], sizeof(line[4]), "GX", gx_dps, 1.0f);
    format_signed_scaled(line[5], sizeof(line[5]), "GY", gy_dps, 1.0f);
    format_signed_scaled(line[6], sizeof(line[6]), "GZ", gz_dps, 1.0f);
    format_temp(line[7], sizeof(line[7]), temp_c);

    for (uint8_t page = 0; page < 8U; ++page) {
        ok = send_oled_line(node, page, line[page]) && ok;
    }

    return ok;
}

static void print_sample(const mpu6050_sample_t *sample)
{
    float ax_g = accel_raw_to_g(sample->accel_x);
    float ay_g = accel_raw_to_g(sample->accel_y);
    float az_g = accel_raw_to_g(sample->accel_z);
    float gx_dps = gyro_raw_to_dps(sample->gyro_x);
    float gy_dps = gyro_raw_to_dps(sample->gyro_y);
    float gz_dps = gyro_raw_to_dps(sample->gyro_z);
    float temp_c = temp_raw_to_c(sample->temperature_raw);

    ESP_LOGI(TAG,
             "#%u accel[g]=(% .3f,% .3f,% .3f) accel[m/s2]=(% .2f,% .2f,% .2f) gyro[dps]=(% .2f,% .2f,% .2f) temp=% .2fC",
             sample->sample_count,
             ax_g, ay_g, az_g,
             ax_g * STANDARD_GRAVITY_MS2,
             ay_g * STANDARD_GRAVITY_MS2,
             az_g * STANDARD_GRAVITY_MS2,
             gx_dps, gy_dps, gz_dps,
             temp_c);

    printf("mpu6050 #%u ax=% .3fg ay=% .3fg az=% .3fg gx=% .2fdps gy=% .2fdps gz=% .2fdps temp=% .2fC\r\n",
           sample->sample_count,
           ax_g, ay_g, az_g,
           gx_dps, gy_dps, gz_dps,
           temp_c);
}

static bool handle_mpu6050_frame(const can_rx_item_t *item, mpu6050_sample_t *complete)
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

    if (s_mpu_sample.has_accel && s_mpu_sample.has_gyro && s_mpu_sample.has_temp) {
        *complete = s_mpu_sample;
        memset(&s_mpu_sample, 0, sizeof(s_mpu_sample));
        return true;
    }

    return false;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM MPU6050 OLED CAN gateway demo");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("\r\n==== ESP32-WROOM MPU6050 OLED CAN demo ====\r\n");
    printf("MPU6050 and OLED are handled by CH32 over I2C; ESP32 uses CAN\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, waiting for MPU6050 frames and updating OLED");
    printf("twai started, waiting for MPU6050 frames and updating OLED\r\n");

    (void)send_oled_clear(node);
    (void)send_oled_line(node, 0, "WAIT");
    (void)send_oled_line(node, 1, "MPU6050");

    uint32_t idle_count = 0;

    while (true) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(CAN_RX_WAIT_MS)) == pdTRUE) {
            mpu6050_sample_t complete = {0};
            idle_count = 0;
            if (handle_mpu6050_frame(&item, &complete)) {
                TickType_t now = xTaskGetTickCount();
                print_sample(&complete);
                if ((now - s_last_oled_update_tick) >= pdMS_TO_TICKS(OLED_UPDATE_MIN_MS)) {
                    bool ok = update_oled(node, &complete);
                    s_last_oled_update_tick = now;
                    ESP_LOGI(TAG, "OLED update %s", ok ? "OK" : "FAILED");
                }
            }
        } else {
            idle_count++;
            if ((idle_count % CAN_STATUS_PERIOD_TICKS) == 0) {
                twai_node_status_t status = {0};
                (void)twai_node_get_info(node, &status, NULL);
                ESP_LOGI(TAG, "waiting for CAN frames, state=%d tx_err=%u rx_err=%u",
                         status.state, status.tx_error_count, status.rx_error_count);
                printf("waiting for CAN frames, state=%d tx_err=%u rx_err=%u\r\n",
                       status.state, status.tx_error_count, status.rx_error_count);
            }
        }
    }
}
