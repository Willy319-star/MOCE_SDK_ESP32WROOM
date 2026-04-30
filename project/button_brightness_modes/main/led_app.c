#include "led_app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_button.h"
#include "bsp_led.h"

static const char *TAG = "button_brightness";

#define SLOW_BLINK_HALF_PERIOD_MS   500U
#define FAST_BLINK_HALF_PERIOD_MS   100U
#define BREATHE_STEP_INTERVAL_MS    20U
#define BREATHE_PHASE_MAX           100U

typedef struct {
    led_app_mode_t mode;
    led_app_brightness_t brightness;
    bool blink_on;
    uint8_t breathe_phase;
    int8_t breathe_direction;
    TickType_t last_update_tick;
} led_app_state_t;

typedef struct {
    led_app_brightness_t level;
    uint8_t percent;
} brightness_config_t;

static const brightness_config_t s_brightness_configs[] = {
    { LED_APP_BRIGHTNESS_20, 20U },
    { LED_APP_BRIGHTNESS_50, 50U },
    { LED_APP_BRIGHTNESS_100, 100U },
};

static led_app_state_t s_app;

static const char *mode_to_string(led_app_mode_t mode)
{
    switch (mode) {
    case LED_APP_MODE_SOLID_ON:
        return "SOLID_ON";
    case LED_APP_MODE_SLOW_BLINK:
        return "SLOW_BLINK";
    case LED_APP_MODE_FAST_BLINK:
        return "FAST_BLINK";
    case LED_APP_MODE_BREATHE:
        return "BREATHE";
    default:
        return "UNKNOWN";
    }
}

static const char *brightness_to_string(led_app_brightness_t brightness)
{
    switch (brightness) {
    case LED_APP_BRIGHTNESS_20:
        return "20%";
    case LED_APP_BRIGHTNESS_50:
        return "50%";
    case LED_APP_BRIGHTNESS_100:
        return "100%";
    default:
        return "UNKNOWN";
    }
}

static uint8_t current_brightness_percent(void)
{
    for (size_t i = 0; i < (sizeof(s_brightness_configs) / sizeof(s_brightness_configs[0])); i++) {
        if (s_brightness_configs[i].level == s_app.brightness) {
            return s_brightness_configs[i].percent;
        }
    }

    return 50U;
}

static uint8_t eased_breathe_percent(uint8_t phase)
{
    uint32_t t = phase;
    uint32_t eased = 3U * t * t - (2U * t * t * t) / BREATHE_PHASE_MAX;

    return (uint8_t)((eased + (BREATHE_PHASE_MAX / 2U)) / BREATHE_PHASE_MAX);
}

static void set_led_output(bool on)
{
    bsp_led_set_brightness(on ? current_brightness_percent() : 0U);
}

static void reset_dynamic_state(void)
{
    s_app.blink_on = false;
    s_app.breathe_phase = 0;
    s_app.breathe_direction = 1;
    s_app.last_update_tick = xTaskGetTickCount();
}

static void apply_mode_start_state(void)
{
    reset_dynamic_state();

    switch (s_app.mode) {
    case LED_APP_MODE_SOLID_ON:
        bsp_led_set_brightness(current_brightness_percent());
        break;

    case LED_APP_MODE_SLOW_BLINK:
    case LED_APP_MODE_FAST_BLINK:
    case LED_APP_MODE_BREATHE:
        bsp_led_set_brightness(0U);
        break;

    default:
        break;
    }
}

static void log_state(const char *reason)
{
    ESP_LOGI(TAG, "%s: mode=%s, brightness=%s",
             reason,
             mode_to_string(s_app.mode),
             brightness_to_string(s_app.brightness));
}

static void next_mode(void)
{
    s_app.mode = (led_app_mode_t)((s_app.mode + 1) % LED_APP_MODE_MAX);
    apply_mode_start_state();
    log_state("mode changed");
}

static void next_brightness(void)
{
    s_app.brightness = (led_app_brightness_t)((s_app.brightness + 1) % LED_APP_BRIGHTNESS_MAX);

    if (s_app.mode == LED_APP_MODE_SOLID_ON || s_app.blink_on) {
        bsp_led_set_brightness(current_brightness_percent());
    }

    log_state("brightness changed");
}

static void process_button(void)
{
    bsp_button_process();

    switch (bsp_button_get_event()) {
    case BSP_BUTTON_EVENT_SHORT_PRESS:
        next_mode();
        break;

    case BSP_BUTTON_EVENT_LONG_PRESS:
        next_brightness();
        break;

    default:
        break;
    }
}

static void process_blink(uint32_t half_period_ms)
{
    TickType_t now = xTaskGetTickCount();

    if ((now - s_app.last_update_tick) < pdMS_TO_TICKS(half_period_ms)) {
        return;
    }

    s_app.last_update_tick = now;
    s_app.blink_on = !s_app.blink_on;
    set_led_output(s_app.blink_on);
}

static void process_breathe(void)
{
    TickType_t now = xTaskGetTickCount();
    uint8_t max_brightness = current_brightness_percent();

    if ((now - s_app.last_update_tick) < pdMS_TO_TICKS(BREATHE_STEP_INTERVAL_MS)) {
        return;
    }

    s_app.last_update_tick = now;

    if (s_app.breathe_direction > 0) {
        if (s_app.breathe_phase < BREATHE_PHASE_MAX) {
            s_app.breathe_phase++;
        } else {
            s_app.breathe_direction = -1;
            s_app.breathe_phase--;
        }
    } else {
        if (s_app.breathe_phase > 0) {
            s_app.breathe_phase--;
        } else {
            s_app.breathe_direction = 1;
            s_app.breathe_phase++;
        }
    }

    uint32_t brightness = eased_breathe_percent(s_app.breathe_phase) * max_brightness;
    bsp_led_set_brightness((uint8_t)((brightness + 50U) / 100U));
}

static void process_led(void)
{
    switch (s_app.mode) {
    case LED_APP_MODE_SOLID_ON:
        break;

    case LED_APP_MODE_SLOW_BLINK:
        process_blink(SLOW_BLINK_HALF_PERIOD_MS);
        break;

    case LED_APP_MODE_FAST_BLINK:
        process_blink(FAST_BLINK_HALF_PERIOD_MS);
        break;

    case LED_APP_MODE_BREATHE:
        process_breathe();
        break;

    default:
        break;
    }
}

void led_app_init(void)
{
    s_app.mode = LED_APP_MODE_SOLID_ON;
    s_app.brightness = LED_APP_BRIGHTNESS_50;
    apply_mode_start_state();
    log_state("initial state");
}

void led_app_process(void)
{
    process_button();
    process_led();
}
