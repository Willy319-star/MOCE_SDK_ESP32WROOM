#pragma once

#include "driver/ledc.h"

#define BOARD_LED_GPIO               2

#define BOARD_LED_PWM_MODE           LEDC_LOW_SPEED_MODE
#define BOARD_LED_PWM_TIMER          LEDC_TIMER_0
#define BOARD_LED_PWM_CHANNEL        LEDC_CHANNEL_0
#define BOARD_LED_PWM_DUTY_RES       LEDC_TIMER_10_BIT
#define BOARD_LED_PWM_FREQUENCY_HZ   5000


/* Button */
#define BOARD_BUTTON_GPIO            0
#define BOARD_BUTTON_ACTIVE_LEVEL    0   /* 0: 按下为低电平, 1: 按下为高电平 */
#define BOARD_BUTTON_USE_PULLUP      1
#define BOARD_BUTTON_USE_PULLDOWN    0

/* 时序参数 */
#define BOARD_BUTTON_DEBOUNCE_MS     30
#define BOARD_BUTTON_LONG_PRESS_MS   1000