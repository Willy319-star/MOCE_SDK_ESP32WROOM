#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "ch32_vc02_gateway_final.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define CAN_RX_QUEUE_LEN       32
#define CAN_TX_DONE_QUEUE_LEN  8
#define CAN_TX_QUEUE_DEPTH     8
#define CAN_TX_TIMEOUT_MS      600
#define PING_PERIOD_MS         2000
#define STATUS_PERIOD_MS       3000
#define LOOP_WAIT_MS           50

static const char *TAG = "vc02_test0_final";

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
static ch32_vc02_driver_t s_vc02;

static void print_raw(const uint8_t *data, uint8_t len)
{
    printf("[");
    for (uint8_t i = 0U; i < len; ++i) {
        printf("%s%02X", i == 0U ? "" : " ", data[i]);
    }
    printf("]");
}

static void action_voice_wakeup(const ch32_vc02_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    printf("ACTION %s: wakeup event received, keep system ready for the next command raw=",
           ch32_vc02_cmd_name(event->cmd));
    print_raw(event->raw, event->raw_len);
    printf("\r\n");
}

static void action_slot_demo(const ch32_vc02_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    printf("ACTION_SLOT %s: %s -> replace this callback with a real module function raw=",
           ch32_vc02_action_slot(event->cmd),
           event->info != NULL ? event->info->meaning : "unknown");
    print_raw(event->raw, event->raw_len);
    printf("\r\n");
}

static void register_vc02_actions(void)
{
    /*
     * This table is the important VC02 integration pattern.
     *
     * VC02 firmware sends fixed UART bytes.
     * The driver turns those bytes into semantic commands.
     * A final product should connect commands to other module drivers by
     * replacing action_slot_demo with functions such as:
     *
     *   oled_gateway_clear_screen
     *   motor_gateway_start
     *   servo_gateway_stop
     *   tof_gateway_show_latest_distance
     *
     * Keep parsing logic in the driver; only change callback registration here.
     */
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_WAKEUP, action_voice_wakeup, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_OLED_CLEAR, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_MOTOR_START, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_OLED_REFRESH, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_SERVO_START, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_MPU6050_SHOW, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_SYN_STOP, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_SYN_START, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_MOTOR_STOP, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_SERVO_STOP, action_slot_demo, NULL);
    ch32_vc02_register_action(&s_vc02, CH32_VC02_CMD_VL53L0X_SHOW, action_slot_demo, NULL);
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
            .bitrate = CH32_VC02_CAN_BITRATE_HZ,
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
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, &s_can_ctx));
    ESP_ERROR_CHECK(twai_node_enable(node));
    return node;
}

static esp_err_t send_ping(twai_node_handle_t node, uint8_t seq)
{
    uint8_t data[8] = {0};
    if (!ch32_vc02_build_ping(seq, data)) {
        return ESP_FAIL;
    }

    twai_frame_t frame = {
        .header = {
            .id = CH32_VC02_CAN_ID_PING,
            .dlc = sizeof(data),
        },
        .buffer = data,
        .buffer_len = sizeof(data),
    };

    esp_err_t err = twai_node_transmit(node, &frame, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS));
    if (err != ESP_OK) {
        return err;
    }

    can_tx_done_item_t done = {0};
    if (xQueueReceive(s_tx_done_queue, &done, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return done.success ? ESP_OK : ESP_FAIL;
}

static void process_vc02_uart_frame(const can_rx_item_t *item)
{
    ch32_vc02_event_t event = {0};
    if (ch32_vc02_feed_bytes(&s_vc02, item->data, item->header.dlc, &event)) {
        printf("VC02_EVENT name=%s uart=\"%s\" meaning=\"%s\" slot=%s raw=",
               event.info->name,
               event.info->vc02_uart_text,
               event.info->meaning,
               event.info->action_slot);
        print_raw(event.raw, event.raw_len);
        printf("\r\n");

        if (!ch32_vc02_dispatch(&s_vc02, &event)) {
            printf("VC02_EVENT %s has no callback registered\r\n", ch32_vc02_cmd_name(event.cmd));
        }
    } else {
        printf("VC02_RX no_match chunks=%lu bytes=%lu ascii_window=\"%s\"\r\n",
               (unsigned long)s_vc02.rx_chunks,
               (unsigned long)s_vc02.rx_bytes,
               s_vc02.ascii_window);
    }
}

static void process_rx_frame(const can_rx_item_t *item)
{
    if (item == NULL || item->header.ide || item->header.rtr) {
        return;
    }

    if (item->header.id == CH32_VC02_CAN_ID_UART_RX) {
        process_vc02_uart_frame(item);
    } else if (item->header.id == CH32_VC02_CAN_ID_STATUS) {
        printf("VC02_BRIDGE_STATUS raw=");
        print_raw(item->data, item->header.dlc);
        printf("\r\n");
    } else if (item->header.id == CH32_VC02_CAN_ID_ACK) {
        printf("VC02_BRIDGE_ACK raw=");
        print_raw(item->data, item->header.dlc);
        printf("\r\n");
    }
}

void app_main(void)
{
    printf("\n==== ESP32-WROOM -> CH32 UART bridge -> VC02 action mapping demo ====\n");
    printf("ESP32 CAN: TX=GPIO%d RX=GPIO%d bitrate=50 kbit/s\n",
           BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    printf("VC02 UART is on CH32 side. Expected CH32 UART baud=%u\n", CH32_VC02_UART_BAUD);
    printf("CAN IDs: ping=0x%03X status=0x%03X uart_rx=0x%03X ack=0x%03X\n\n",
           CH32_VC02_CAN_ID_PING,
           CH32_VC02_CAN_ID_STATUS,
           CH32_VC02_CAN_ID_UART_RX,
           CH32_VC02_CAN_ID_ACK);

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_item_t));
    s_tx_done_queue = xQueueCreate(CAN_TX_DONE_QUEUE_LEN, sizeof(can_tx_done_item_t));
    ESP_ERROR_CHECK((s_rx_queue == NULL || s_tx_done_queue == NULL) ? ESP_ERR_NO_MEM : ESP_OK);

    s_can_ctx.rx_queue = s_rx_queue;
    s_can_ctx.tx_done_queue = s_tx_done_queue;

    ch32_vc02_init(&s_vc02);
    register_vc02_actions();

    twai_node_handle_t node = start_can_node();
    ESP_LOGI(TAG, "TWAI started, waiting for VC02 bytes forwarded by CH32");

    TickType_t last_ping = 0;
    TickType_t last_status = 0;
    uint8_t seq = 0;

    while (true) {
        can_rx_item_t item = {0};
        while (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(LOOP_WAIT_MS)) == pdTRUE) {
            process_rx_frame(&item);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_ping) >= pdMS_TO_TICKS(PING_PERIOD_MS)) {
            esp_err_t err = send_ping(node, seq++);
            printf("VC02_PING result=%s\r\n", esp_err_to_name(err));
            last_ping = now;
        }

        if ((now - last_status) >= pdMS_TO_TICKS(STATUS_PERIOD_MS)) {
            printf("VC02_DRIVER_STATUS chunks=%lu bytes=%lu matched=%lu dispatched=%lu no_match=%lu ascii_window=\"%s\"\r\n",
                   (unsigned long)s_vc02.rx_chunks,
                   (unsigned long)s_vc02.rx_bytes,
                   (unsigned long)s_vc02.matched_events,
                   (unsigned long)s_vc02.dispatched_events,
                   (unsigned long)s_vc02.no_match_chunks,
                   s_vc02.ascii_window);
            last_status = now;
        }
    }
}
