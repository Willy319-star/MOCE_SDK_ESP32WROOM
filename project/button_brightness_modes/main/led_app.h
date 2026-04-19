#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_APP_MODE_SOLID_ON = 0,
    LED_APP_MODE_SLOW_BLINK,
    LED_APP_MODE_FAST_BLINK,
    LED_APP_MODE_BREATH,
    LED_APP_MODE_MAX,
} led_app_mode_t;

typedef enum {
    LED_APP_BRIGHTNESS_20 = 0,
    LED_APP_BRIGHTNESS_50,
    LED_APP_BRIGHTNESS_100,
    LED_APP_BRIGHTNESS_MAX,
} led_app_brightness_t;

void led_app_init(void);
void led_app_process(void);

#ifdef __cplusplus
}
#endif
