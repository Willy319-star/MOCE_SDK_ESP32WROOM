#include <inttypes.h>
#include <stdio.h>

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "vc02_direct_uart_final.h"

#define VC02_STATUS_PERIOD_MS 3000
#define VC02_READ_TIMEOUT_MS  200

static const char *TAG = "vc02_direct_test0_final";
static vc02_direct_uart_final_t s_vc02;

static void print_raw(const uint8_t *data, uint8_t len)
{
    printf("[");
    for (uint8_t i = 0U; i < len; ++i) {
        printf("%s%02X", i == 0U ? "" : " ", data[i]);
    }
    printf("]");
}

static void action_voice_wakeup(const vc02_direct_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    printf("ACTION %s: wakeup detected, system is ready for the next voice command raw=",
           vc02_direct_cmd_name(event->cmd));
    print_raw(event->raw, event->raw_len);
    printf("\r\n");
}

static void action_slot_demo(const vc02_direct_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    printf("ACTION_SLOT %s: %s -> replace this callback with a selected module API raw=",
           vc02_direct_action_slot(event->cmd),
           event->info != NULL ? event->info->meaning : "unknown");
    print_raw(event->raw, event->raw_len);
    printf("\r\n");
}

static void register_actions(void)
{
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_WAKEUP, action_voice_wakeup, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_OLED_CLEAR, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_MOTOR_START, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_OLED_REFRESH, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_SERVO_START, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_MPU6050_SHOW, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_SYN_STOP, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_SYN_START, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_MOTOR_STOP, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_SERVO_STOP, action_slot_demo, NULL);
    vc02_direct_register_action(&s_vc02, VC02_DIRECT_CMD_VL53L0X_SHOW, action_slot_demo, NULL);
}

static void print_event(const vc02_direct_event_t *event)
{
    printf("VC02_EVENT name=%s uart=\"%s\" meaning=\"%s\" slot=%s raw=",
           event->info->name,
           event->info->vc02_uart_text,
           event->info->meaning,
           event->info->action_slot);
    print_raw(event->raw, event->raw_len);
    printf("\r\n");
}

void app_main(void)
{
    vc02_direct_uart_final_config_t cfg = vc02_direct_uart_final_default_config();
    cfg.uart_port = (uart_port_t)BOARD_UART_PORT;
    cfg.tx_gpio = BOARD_UART_TX_GPIO;
    cfg.rx_gpio = BOARD_UART_RX_GPIO;
    cfg.baud_rate = VC02_DIRECT_UART_DEFAULT_BAUD;
    cfg.rx_buffer_size = BOARD_UART_RX_BUFFER_SIZE > 512 ? BOARD_UART_RX_BUFFER_SIZE : 512;
    cfg.tx_buffer_size = 0;

    printf("\n==== ESP32-WROOM direct UART -> VC02 action mapping demo v2 ====\n");
    printf("VC02 UART direct wiring: ESP32 GPIO%d TX -> VC02 RX, ESP32 GPIO%d RX <- VC02 TX\n",
           cfg.tx_gpio, cfg.rx_gpio);
    printf("VC02 UART config: UART%d baud=%" PRIu32 " 8N1\n",
           (int)cfg.uart_port, cfg.baud_rate);
    printf("Recognized commands: wakeup, OLED clear/refresh, motor start/stop, servo start/stop, MPU6050 show, voice broadcast start/stop, VL53L0X show\n");
    printf("Reset reason: cpu0=%d cpu1=%d\n\n", esp_reset_reason(), esp_reset_reason());

    esp_err_t err = vc02_direct_uart_final_init(&s_vc02, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "VC02 UART init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "VC02 UART init OK, waiting for UART bytes");
    register_actions();

    TickType_t last_status = 0;
    uint32_t last_status_chunks = 0;
    uint32_t last_status_matched = 0;
    uint32_t last_status_no_match = 0;

    while (true) {
        vc02_direct_event_t event = {0};
        err = vc02_direct_read_event(&s_vc02, &event, VC02_READ_TIMEOUT_MS);
        if (err == ESP_OK) {
            print_event(&event);
            if (!vc02_direct_dispatch(&s_vc02, &event)) {
                printf("VC02_EVENT %s has no callback registered\r\n", vc02_direct_cmd_name(event.cmd));
            }
        } else if (err == ESP_ERR_NOT_FOUND) {
            printf("VC02_RX no_match chunks=%lu bytes=%lu ascii_window=\"%s\"\r\n",
                   (unsigned long)s_vc02.rx_chunks,
                   (unsigned long)s_vc02.rx_bytes,
                   s_vc02.ascii_window);
        } else if (err != ESP_ERR_TIMEOUT) {
            printf("VC02_UART_READ result=%s\r\n", esp_err_to_name(err));
        }

        TickType_t now = xTaskGetTickCount();
        bool status_changed = s_vc02.rx_chunks != last_status_chunks ||
                              s_vc02.matched_events != last_status_matched ||
                              s_vc02.no_match_chunks != last_status_no_match;
        if (status_changed && (now - last_status) >= pdMS_TO_TICKS(VC02_STATUS_PERIOD_MS)) {
            printf("VC02_DIRECT_STATUS uart_ready=%d uart=UART%d tx_gpio=%d rx_gpio=%d baud=%" PRIu32
                   " chunks=%lu bytes=%lu matched=%lu dispatched=%lu no_match=%lu ascii_window=\"%s\"\r\n",
                   s_vc02.uart_ready ? 1 : 0,
                   (int)s_vc02.config.uart_port,
                   s_vc02.config.tx_gpio,
                   s_vc02.config.rx_gpio,
                   s_vc02.config.baud_rate,
                   (unsigned long)s_vc02.rx_chunks,
                   (unsigned long)s_vc02.rx_bytes,
                   (unsigned long)s_vc02.matched_events,
                   (unsigned long)s_vc02.dispatched_events,
                   (unsigned long)s_vc02.no_match_chunks,
                   s_vc02.ascii_window);
            last_status_chunks = s_vc02.rx_chunks;
            last_status_matched = s_vc02.matched_events;
            last_status_no_match = s_vc02.no_match_chunks;
            last_status = now;
        }
    }
}
