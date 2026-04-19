#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_EFFECT_SOLID_ON = 0,
    LED_EFFECT_SOLID_OFF,
    LED_EFFECT_BLINK,
    LED_EFFECT_BREATH,
} led_effect_mode_t;

typedef enum {
    LED_BLINK_FREQ_1HZ = 1,
    LED_BLINK_FREQ_2HZ = 2,
    LED_BLINK_FREQ_5HZ = 5,
} led_blink_freq_t;

void led_effects_apply_solid(int on, uint32_t hold_ms);
void led_effects_blink(led_blink_freq_t freq_hz, uint32_t duration_ms);
void led_effects_breath(uint32_t duration_ms);

const char *led_effects_mode_name(led_effect_mode_t mode);

#ifdef __cplusplus
}
#endif
