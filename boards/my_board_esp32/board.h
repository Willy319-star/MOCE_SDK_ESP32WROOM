#pragma once

#include "hal/i2c_types.h"
#include "hal/ledc_types.h"

#define BOARD_LED_GPIO               2

#define BOARD_LED_PWM_MODE           LEDC_LOW_SPEED_MODE
#define BOARD_LED_PWM_TIMER          LEDC_TIMER_0
#define BOARD_LED_PWM_CHANNEL        LEDC_CHANNEL_0
#define BOARD_LED_PWM_DUTY_RES       LEDC_TIMER_10_BIT
#define BOARD_LED_PWM_FREQUENCY_HZ   5000

/* Button */
#define BOARD_BUTTON_GPIO            0
#define BOARD_BUTTON_ACTIVE_LEVEL    0   /* 0: pressed low, 1: pressed high */
#define BOARD_BUTTON_USE_PULLUP      1
#define BOARD_BUTTON_USE_PULLDOWN    0

/* Timing */
#define BOARD_BUTTON_DEBOUNCE_MS     30
#define BOARD_BUTTON_LONG_PRESS_MS   1000

/* Servo */
#define BOARD_SERVO_COUNT            2
#define BOARD_SERVO_GPIO_0           4
#define BOARD_SERVO_GPIO_1           5

#define BOARD_SERVO_PWM_MODE         LEDC_LOW_SPEED_MODE
#define BOARD_SERVO_PWM_TIMER        LEDC_TIMER_1
#define BOARD_SERVO_PWM_CHANNEL_0    LEDC_CHANNEL_1
#define BOARD_SERVO_PWM_CHANNEL_1    LEDC_CHANNEL_2
#define BOARD_SERVO_PWM_DUTY_RES     LEDC_TIMER_14_BIT
#define BOARD_SERVO_PWM_FREQUENCY_HZ 50

#define BOARD_SERVO_MIN_PULSE_US     500
#define BOARD_SERVO_CENTER_PULSE_US  1500
#define BOARD_SERVO_MAX_PULSE_US     2500
#define BOARD_SERVO_MIN_ANGLE_DEG    0
#define BOARD_SERVO_MAX_ANGLE_DEG    180

/* I2C master bus for displays, IMUs, and other sensors */
#define BOARD_OLED_I2C_ADDRESS                0x3C

#define BOARD_I2C_PORT                       I2C_NUM_0
#define BOARD_I2C_SDA_GPIO                   21
#define BOARD_I2C_SCL_GPIO                   22
#define BOARD_I2C_FREQUENCY_HZ               400000
#define BOARD_I2C_TIMEOUT_MS                 1000
#define BOARD_I2C_GLITCH_IGNORE_CNT          7
#define BOARD_I2C_TRANS_QUEUE_DEPTH          0
#define BOARD_I2C_ENABLE_INTERNAL_PULLUP     1
#define BOARD_I2C_MAX_REGISTER_WRITE_LEN     32

/* UART for external serial modules such as TW-TTS */
#define BOARD_UART_PORT                      1
#define BOARD_UART_TX_GPIO                   17
#define BOARD_UART_RX_GPIO                   16
#define BOARD_UART_BAUD_RATE                 9600
#define BOARD_UART_RX_BUFFER_SIZE            256
#define BOARD_UART_TX_BUFFER_SIZE            0
#define BOARD_UART_EVENT_QUEUE_SIZE          0
