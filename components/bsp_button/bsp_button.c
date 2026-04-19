#include "bsp_button.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "board.h"

static const char *TAG = "bsp_button";

typedef struct {
    bool stable_pressed;
    bool last_raw_pressed;
    int64_t last_change_ms;
    int64_t press_start_ms;
    bool long_press_reported;
    bsp_button_event_t pending_event;
} bsp_button_ctx_t;

static bsp_button_ctx_t s_btn = {0};
static bool s_inited = false;

static int64_t button_now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static bool button_raw_is_pressed(void)
{
    int level = gpio_get_level(BOARD_BUTTON_GPIO);
    return (level == BOARD_BUTTON_ACTIVE_LEVEL);
}

void bsp_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = BOARD_BUTTON_USE_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = BOARD_BUTTON_USE_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed");
        return;
    }

    bool raw = button_raw_is_pressed();
    int64_t now = button_now_ms();

    s_btn.stable_pressed = raw;
    s_btn.last_raw_pressed = raw;
    s_btn.last_change_ms = now;
    s_btn.press_start_ms = 0;
    s_btn.long_press_reported = false;
    s_btn.pending_event = BSP_BUTTON_EVENT_NONE;

    s_inited = true;

    ESP_LOGI(TAG, "button initialized on GPIO %d", BOARD_BUTTON_GPIO);
}

void bsp_button_process(void)
{
    if (!s_inited) {
        return;
    }

    int64_t now = button_now_ms();
    bool raw = button_raw_is_pressed();

    if (raw != s_btn.last_raw_pressed) {
        s_btn.last_raw_pressed = raw;
        s_btn.last_change_ms = now;
    }

    if (raw != s_btn.stable_pressed) {
        if ((now - s_btn.last_change_ms) >= BOARD_BUTTON_DEBOUNCE_MS) {
            s_btn.stable_pressed = raw;

            if (raw) {
                s_btn.press_start_ms = now;
                s_btn.long_press_reported = false;
                s_btn.pending_event = BSP_BUTTON_EVENT_PRESS;
            } else {
                s_btn.pending_event = BSP_BUTTON_EVENT_RELEASE;

                if (!s_btn.long_press_reported &&
                    (now - s_btn.press_start_ms) < BOARD_BUTTON_LONG_PRESS_MS) {
                    s_btn.pending_event = BSP_BUTTON_EVENT_SHORT_PRESS;
                }
            }
        }
    }

    if (s_btn.stable_pressed && !s_btn.long_press_reported) {
        if ((now - s_btn.press_start_ms) >= BOARD_BUTTON_LONG_PRESS_MS) {
            s_btn.long_press_reported = true;
            s_btn.pending_event = BSP_BUTTON_EVENT_LONG_PRESS;
        }
    }
}

bsp_button_event_t bsp_button_get_event(void)
{
    bsp_button_event_t ev = s_btn.pending_event;
    s_btn.pending_event = BSP_BUTTON_EVENT_NONE;
    return ev;
}

bool bsp_button_is_pressed(void)
{
    if (!s_inited) {
        return false;
    }
    return s_btn.stable_pressed;
}