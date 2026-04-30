#include "led_effects.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_led.h"

static const char *TAG = "led_effects";

#define DEMO_AUTO_SWITCH_MS        6000U
#define BREATHE_STEP_INTERVAL_MS   20U
#define BREATHE_PHASE_MAX          100U

typedef struct {
    led_effect_t effect;
    uint32_t half_period_ms;
} blink_config_t;

static const blink_config_t s_blink_configs[] = {
    { LED_EFFECT_BLINK_1HZ, 500U },
    { LED_EFFECT_BLINK_2HZ, 250U },
    { LED_EFFECT_BLINK_5HZ, 100U },
};

static led_effect_t s_current_effect = LED_EFFECT_OFF;
static bool s_led_on = false;
static TickType_t s_last_update_tick = 0;
static TickType_t s_last_effect_switch_tick = 0;
static uint8_t s_breathe_phase = 0;
static int8_t s_breathe_direction = 1;

static const char *effect_to_string(led_effect_t effect)
{
    switch (effect) {
    case LED_EFFECT_OFF:
        return "OFF";
    case LED_EFFECT_ON:
        return "ON";
    case LED_EFFECT_BLINK_1HZ:
        return "BLINK_1HZ";
    case LED_EFFECT_BLINK_2HZ:
        return "BLINK_2HZ";
    case LED_EFFECT_BLINK_5HZ:
        return "BLINK_5HZ";
    case LED_EFFECT_BREATHE:
        return "BREATHE";
    default:
        return "UNKNOWN";
    }
}

static bool get_blink_half_period(led_effect_t effect, uint32_t *half_period_ms)
{
    for (size_t i = 0; i < (sizeof(s_blink_configs) / sizeof(s_blink_configs[0])); i++) {
        if (s_blink_configs[i].effect == effect) {
            *half_period_ms = s_blink_configs[i].half_period_ms;
            return true;
        }
    }

    return false;
}

static uint8_t smooth_brightness_percent(uint8_t phase)
{
    uint32_t t = phase;

    /* Smoothstep easing: 3t^2 - 2t^3, scaled to 0..100. */
    return (uint8_t)((3U * t * t - 2U * t * t * t / BREATHE_PHASE_MAX + 50U) /
                     BREATHE_PHASE_MAX);
}

static void apply_effect_start_state(led_effect_t effect)
{
    s_last_update_tick = xTaskGetTickCount();

    switch (effect) {
    case LED_EFFECT_OFF:
        s_led_on = false;
        bsp_led_set(0);
        break;

    case LED_EFFECT_ON:
        s_led_on = true;
        bsp_led_set(1);
        break;

    case LED_EFFECT_BLINK_1HZ:
    case LED_EFFECT_BLINK_2HZ:
    case LED_EFFECT_BLINK_5HZ:
        s_led_on = false;
        bsp_led_set(0);
        break;

    case LED_EFFECT_BREATHE:
        s_breathe_phase = 0;
        s_breathe_direction = 1;
        bsp_led_set_brightness(0);
        break;

    default:
        break;
    }
}

static void process_blink(TickType_t now)
{
    uint32_t half_period_ms = 0;

    if (!get_blink_half_period(s_current_effect, &half_period_ms)) {
        return;
    }

    if ((now - s_last_update_tick) >= pdMS_TO_TICKS(half_period_ms)) {
        s_last_update_tick = now;
        s_led_on = !s_led_on;
        bsp_led_set(s_led_on ? 1 : 0);
    }
}

static void process_breathe(TickType_t now)
{
    if ((now - s_last_update_tick) < pdMS_TO_TICKS(BREATHE_STEP_INTERVAL_MS)) {
        return;
    }

    s_last_update_tick = now;

    if (s_breathe_direction > 0) {
        if (s_breathe_phase < BREATHE_PHASE_MAX) {
            s_breathe_phase++;
        } else {
            s_breathe_direction = -1;
            s_breathe_phase--;
        }
    } else {
        if (s_breathe_phase > 0) {
            s_breathe_phase--;
        } else {
            s_breathe_direction = 1;
            s_breathe_phase++;
        }
    }

    bsp_led_set_brightness(smooth_brightness_percent(s_breathe_phase));
}

void led_effects_init(void)
{
    s_current_effect = LED_EFFECT_OFF;
    s_last_effect_switch_tick = xTaskGetTickCount();
    apply_effect_start_state(s_current_effect);

    ESP_LOGI(TAG, "initial effect -> %s", effect_to_string(s_current_effect));
}

void led_effects_set(led_effect_t effect)
{
    if (effect >= LED_EFFECT_MAX) {
        ESP_LOGW(TAG, "ignore invalid effect: %d", (int)effect);
        return;
    }

    s_current_effect = effect;
    s_last_effect_switch_tick = xTaskGetTickCount();
    apply_effect_start_state(s_current_effect);

    ESP_LOGI(TAG, "effect -> %s", effect_to_string(s_current_effect));
}

void led_effects_next(void)
{
    led_effect_t next = (led_effect_t)((s_current_effect + 1) % LED_EFFECT_MAX);
    led_effects_set(next);
}

void led_effects_process(void)
{
    TickType_t now = xTaskGetTickCount();

    if ((now - s_last_effect_switch_tick) >= pdMS_TO_TICKS(DEMO_AUTO_SWITCH_MS)) {
        led_effects_next();
        return;
    }

    switch (s_current_effect) {
    case LED_EFFECT_BLINK_1HZ:
    case LED_EFFECT_BLINK_2HZ:
    case LED_EFFECT_BLINK_5HZ:
        process_blink(now);
        break;

    case LED_EFFECT_BREATHE:
        process_breathe(now);
        break;

    case LED_EFFECT_OFF:
    case LED_EFFECT_ON:
    default:
        break;
    }
}
