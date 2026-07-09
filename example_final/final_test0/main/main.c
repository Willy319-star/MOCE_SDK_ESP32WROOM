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

#define CAN_BITRATE                  50000
#define CAN_BITRATE_TEXT             "50 kbit/s"
#define CAN_RX_QUEUE_LEN             64
#define CAN_TX_QUEUE_DEPTH           8
#define CAN_TX_DONE_QUEUE_LEN        8
#define CAN_CMD_TIMEOUT_MS           500
#define MAIN_LOOP_WAIT_MS            50

#define DEVICE_TYPE_I2C              0x01U
#define DEVICE_TYPE_MOTOR            0x02U
#define DEVICE_TYPE_SERVO            0x03U

#define I2C_SCAN_CMD                 0x01U
#define I2C_WRITE_CMD                0x02U
#define I2C_READ_CMD                 0x03U
#define I2C_STATUS_SCAN              0x01U

#define MOTOR_DUTY_PERMILLE          500U
#define MOTOR_DIR_FORWARD            0U

#define NODE_ID_MAX                  10U
#define SERVO_CHANNEL_COUNT          4U
#define MOTOR_CHANNEL_COUNT          2U

#define I2C_SCAN_PERIOD_MS           5000U
#define MOTOR_CMD_PERIOD_MS          1000U
#define SERVO_CMD_PERIOD_MS          1000U
#define STATUS_PRINT_PERIOD_MS       5000U

#define CAN_ID_STATUS(node_id)        (0x100U + (uint32_t)(node_id))
#define CAN_ID_I2C_CMD(node_id)       (0x200U + (uint32_t)(node_id))
#define CAN_ID_MOTOR_CMD(node_id)     (0x300U + (uint32_t)(node_id))
#define CAN_ID_SERVO_CMD(node_id)     (0x400U + (uint32_t)(node_id))
#define CAN_ID_ACK(node_id)           (0x500U + (uint32_t)(node_id))
#define CAN_ID_HELLO(node_id)         (0x700U + (uint32_t)(node_id))

static const char *TAG = "final_test0";

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
    bool online;
    uint8_t node_id;
    uint8_t device_type;
    uint8_t fw_version;
    uint8_t capability_flags;
    TickType_t last_seen_tick;
} node_info_t;

static QueueHandle_t s_rx_queue;
static QueueHandle_t s_tx_done_queue;
static can_context_t s_can_ctx;
static node_info_t s_nodes[NODE_ID_MAX + 1U];

static void put_u16_le(uint8_t *data, uint8_t offset, uint16_t value)
{
    data[offset] = (uint8_t)(value & 0xFFU);
    data[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static const char *device_type_name(uint8_t type)
{
    switch (type) {
    case DEVICE_TYPE_I2C:
        return "I2C";
    case DEVICE_TYPE_MOTOR:
        return "MOTOR";
    case DEVICE_TYPE_SERVO:
        return "SERVO";
    default:
        return "UNKNOWN";
    }
}

static uint8_t find_node_by_type(uint8_t device_type)
{
    for (uint8_t id = 1U; id <= NODE_ID_MAX; ++id) {
        if (s_nodes[id].online && s_nodes[id].device_type == device_type) {
            return id;
        }
    }
    return 0U;
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
            ESP_EARLY_LOGW(TAG, "RX queue full, dropping frame");
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
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " queued but not completed", id);
        return ESP_ERR_TIMEOUT;
    }

    if (!done.success) {
        ESP_LOGW(TAG, "TX id=0x%03" PRIX32 " failed on bus", id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void handle_hello(const can_rx_item_t *item)
{
    if (item->header.dlc < 4U) {
        return;
    }

    uint8_t node_id = item->data[1];
    if (node_id == 0U || node_id > NODE_ID_MAX || item->header.id != CAN_ID_HELLO(node_id)) {
        return;
    }

    bool first_seen = !s_nodes[node_id].online;
    s_nodes[node_id].online = true;
    s_nodes[node_id].node_id = node_id;
    s_nodes[node_id].device_type = item->data[0];
    s_nodes[node_id].fw_version = item->data[2];
    s_nodes[node_id].capability_flags = item->data[3];
    s_nodes[node_id].last_seen_tick = xTaskGetTickCount();

    if (first_seen) {
        ESP_LOGI(TAG, "HELLO node=%u type=%s(0x%02X) fw=%u cap=0x%02X",
                 node_id,
                 device_type_name(item->data[0]),
                 item->data[0],
                 item->data[2],
                 item->data[3]);
        printf("hello node=%u type=%s fw=%u cap=0x%02X\r\n",
               node_id,
               device_type_name(item->data[0]),
               item->data[2],
               item->data[3]);
    }
}

static void handle_status(const can_rx_item_t *item)
{
    if (item->header.dlc < 2U) {
        return;
    }

    uint8_t node_id = (uint8_t)(item->header.id - 0x100U);
    if (node_id == 0U || node_id > NODE_ID_MAX) {
        return;
    }

    if (item->data[0] == I2C_STATUS_SCAN) {
        uint8_t found_count = item->data[1];
        ESP_LOGI(TAG, "I2C scan result node=%u found=%u first=[0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X]",
                 node_id, found_count,
                 item->data[2], item->data[3], item->data[4],
                 item->data[5], item->data[6], item->data[7]);
        printf("i2c node=%u found=%u addr:", node_id, found_count);
        for (uint8_t i = 0U; i < 6U && i < found_count; ++i) {
            printf(" 0x%02X", item->data[2U + i]);
        }
        printf("\r\n");
    }
}

static void log_ack(const can_rx_item_t *item)
{
    uint8_t node_id = (uint8_t)(item->header.id - 0x500U);
    if (item->header.dlc < 4U || node_id == 0U || node_id > NODE_ID_MAX) {
        return;
    }

    ESP_LOGI(TAG, "ACK node=%u type=%s cmd=0x%02X result=%u",
             node_id,
             device_type_name(item->data[3]),
             item->data[0],
             item->data[1]);
}

static void process_rx_frame(const can_rx_item_t *item)
{
    if (item->header.ide || item->header.rtr) {
        return;
    }

    if (item->header.id >= CAN_ID_HELLO(1U) && item->header.id <= CAN_ID_HELLO(NODE_ID_MAX)) {
        handle_hello(item);
    } else if (item->header.id >= CAN_ID_STATUS(1U) && item->header.id <= CAN_ID_STATUS(NODE_ID_MAX)) {
        handle_status(item);
    } else if (item->header.id >= CAN_ID_ACK(1U) && item->header.id <= CAN_ID_ACK(NODE_ID_MAX)) {
        log_ack(item);
    }
}

static bool wait_for_ack(uint8_t node_id, uint8_t command_type)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CAN_CMD_TIMEOUT_MS);
    uint32_t ack_id = CAN_ID_ACK(node_id);
    bool seen_ack = false;

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        can_rx_item_t item = {0};
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }

        process_rx_frame(&item);

        if (!item.header.ide && !item.header.rtr &&
            item.header.id == ack_id &&
            item.header.dlc >= 4 &&
            item.data[0] == command_type &&
            item.data[2] == node_id) {
            seen_ack = true;
            return item.data[1] != 0U;
        }
    }

    ESP_LOGW(TAG, "ACK %s node=%u cmd=0x%02X",
             seen_ack ? "negative" : "timeout", node_id, command_type);
    return false;
}

static bool send_i2c_scan(twai_node_handle_t node, uint8_t node_id)
{
    uint8_t data[8] = {0};
    data[0] = I2C_SCAN_CMD;

    if (send_can_frame(node, CAN_ID_I2C_CMD(node_id), data, sizeof(data)) != ESP_OK) {
        return false;
    }
    return wait_for_ack(node_id, I2C_SCAN_CMD);
}

static bool send_motor_duty(twai_node_handle_t node, uint8_t node_id,
                            uint8_t channel, uint16_t duty_permille, uint8_t direction)
{
    uint8_t data[8] = {0};
    data[0] = channel;
    put_u16_le(data, 1U, duty_permille);
    data[3] = direction;

    if (send_can_frame(node, CAN_ID_MOTOR_CMD(node_id), data, sizeof(data)) != ESP_OK) {
        return false;
    }
    return wait_for_ack(node_id, channel);
}

static bool send_servo_angle(twai_node_handle_t node, uint8_t node_id,
                             uint8_t channel, uint16_t angle)
{
    uint8_t data[8] = {0};
    data[0] = channel;
    put_u16_le(data, 1U, angle);

    if (send_can_frame(node, CAN_ID_SERVO_CMD(node_id), data, sizeof(data)) != ESP_OK) {
        return false;
    }
    return wait_for_ack(node_id, channel);
}

static void print_node_summary(void)
{
    printf("nodes:");
    for (uint8_t id = 1U; id <= NODE_ID_MAX; ++id) {
        if (s_nodes[id].online) {
            printf(" [%u %s]", id, device_type_name(s_nodes[id].device_type));
        }
    }
    printf("\r\n");
}

void app_main(void)
{
    static const uint16_t servo_angles[] = {0U, 90U, 180U, 90U};
    size_t servo_angle_index = 0U;
    TickType_t last_i2c_scan_tick = 0;
    TickType_t last_motor_tick = 0;
    TickType_t last_servo_tick = 0;
    TickType_t last_status_tick = 0;

    ESP_LOGI(TAG, "ESP32-WROOM final_test0");
    ESP_LOGI(TAG, "bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d",
             CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("\r\n==== ESP32-WROOM final_test0 ====\r\n");
    printf("CAN bitrate=%s CAN_TX=GPIO%d CAN_RX=GPIO%d\r\n",
           CAN_BITRATE_TEXT, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("protocol: hello=0x700+node ack=0x500+node status=0x100+node\r\n");
    printf("commands: i2c=0x200+node motor=0x300+node servo=0x400+node\r\n\r\n");

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);
    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, waiting for CH32 HELLO frames");
    printf("twai started, waiting for CH32 HELLO frames\r\n");

    while (true) {
        can_rx_item_t item = {0};
        while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE) {
            process_rx_frame(&item);
        }

        TickType_t now = xTaskGetTickCount();

        uint8_t i2c_node = find_node_by_type(DEVICE_TYPE_I2C);
        if (i2c_node != 0U &&
            (now - last_i2c_scan_tick) >= pdMS_TO_TICKS(I2C_SCAN_PERIOD_MS)) {
            last_i2c_scan_tick = now;
            bool ok = send_i2c_scan(node, i2c_node);
            ESP_LOGI(TAG, "I2C scan node=%u %s", i2c_node, ok ? "OK" : "FAILED");
            printf("cmd i2c_scan node=%u %s\r\n", i2c_node, ok ? "OK" : "FAILED");
        }

        uint8_t motor_node = find_node_by_type(DEVICE_TYPE_MOTOR);
        if (motor_node != 0U &&
            (now - last_motor_tick) >= pdMS_TO_TICKS(MOTOR_CMD_PERIOD_MS)) {
            last_motor_tick = now;
            bool ch0_ok = send_motor_duty(node, motor_node, 0U, MOTOR_DUTY_PERMILLE, MOTOR_DIR_FORWARD);
            vTaskDelay(pdMS_TO_TICKS(20));
            bool ch1_ok = send_motor_duty(node, motor_node, 1U, MOTOR_DUTY_PERMILLE, MOTOR_DIR_FORWARD);
            ESP_LOGI(TAG, "motor node=%u duty=%u ch0=%s ch1=%s",
                     motor_node, MOTOR_DUTY_PERMILLE,
                     ch0_ok ? "OK" : "FAILED",
                     ch1_ok ? "OK" : "FAILED");
            printf("cmd motor node=%u duty=%u ch0=%s ch1=%s\r\n",
                   motor_node, MOTOR_DUTY_PERMILLE,
                   ch0_ok ? "OK" : "FAILED",
                   ch1_ok ? "OK" : "FAILED");
        }

        uint8_t servo_node = find_node_by_type(DEVICE_TYPE_SERVO);
        if (servo_node != 0U &&
            (now - last_servo_tick) >= pdMS_TO_TICKS(SERVO_CMD_PERIOD_MS)) {
            last_servo_tick = now;
            uint16_t angle = servo_angles[servo_angle_index];
            servo_angle_index = (servo_angle_index + 1U) %
                                (sizeof(servo_angles) / sizeof(servo_angles[0]));

            bool ok[SERVO_CHANNEL_COUNT] = {0};
            for (uint8_t ch = 0U; ch < SERVO_CHANNEL_COUNT; ++ch) {
                ok[ch] = send_servo_angle(node, servo_node, ch, angle);
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            ESP_LOGI(TAG, "servo node=%u angle=%u ch0=%s ch1=%s ch2=%s ch3=%s",
                     servo_node, angle,
                     ok[0] ? "OK" : "FAILED",
                     ok[1] ? "OK" : "FAILED",
                     ok[2] ? "OK" : "FAILED",
                     ok[3] ? "OK" : "FAILED");
            printf("cmd servo node=%u angle=%u ch0=%s ch1=%s ch2=%s ch3=%s\r\n",
                   servo_node, angle,
                   ok[0] ? "OK" : "FAILED",
                   ok[1] ? "OK" : "FAILED",
                   ok[2] ? "OK" : "FAILED",
                   ok[3] ? "OK" : "FAILED");
        }

        if ((now - last_status_tick) >= pdMS_TO_TICKS(STATUS_PRINT_PERIOD_MS)) {
            last_status_tick = now;
            twai_node_status_t status = {0};
            (void)twai_node_get_info(node, &status, NULL);
            ESP_LOGI(TAG, "status state=%d tx_err=%u rx_err=%u", status.state,
                     status.tx_error_count, status.rx_error_count);
            print_node_summary();
        }

        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_WAIT_MS));
    }
}
