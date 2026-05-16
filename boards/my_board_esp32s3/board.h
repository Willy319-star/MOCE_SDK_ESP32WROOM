#pragma once

#include "driver/i2c_types.h"
#include "driver/ledc.h"

/*
 * Default ESP32-S3 board profile.
 *
 * Many ESP32-S3 dev boards use a WS2812 RGB LED instead of a plain GPIO LED.
 * If your board has that kind of LED, replace the LED BSP with an RMT/led_strip
 * implementation or connect an external LED to BOARD_LED_GPIO.
 *
 * ESP32-S3-CAM camera connector reservation:
 * GPIO4/5/6/7/8/9/10/11/12/13/15/16/17/18 are used by the camera module.
 * GPIO19/20 are the native USB D+/D- pins. Keep SDK peripheral defaults away
 * from these pins so the camera and USB Serial/JTAG can coexist.
 */
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
#define BOARD_SERVO_GPIO_0           1
#define BOARD_SERVO_GPIO_1           14

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
#define BOARD_I2C_SDA_GPIO                   47
#define BOARD_I2C_SCL_GPIO                   21
#define BOARD_I2C_FREQUENCY_HZ               400000
#define BOARD_I2C_TIMEOUT_MS                 1000
#define BOARD_I2C_GLITCH_IGNORE_CNT          7
#define BOARD_I2C_TRANS_QUEUE_DEPTH          0
#define BOARD_I2C_ENABLE_INTERNAL_PULLUP     1
#define BOARD_I2C_MAX_REGISTER_WRITE_LEN     32

/* UART for external serial modules such as TW-TTS */
#define BOARD_UART_PORT                      1
#define BOARD_UART_TX_GPIO                   38
#define BOARD_UART_RX_GPIO                   39
#define BOARD_UART_BAUD_RATE                 9600
#define BOARD_UART_RX_BUFFER_SIZE            256
#define BOARD_UART_TX_BUFFER_SIZE            0
#define BOARD_UART_EVENT_QUEUE_SIZE          0


/* =========================
 * TB6612 Motor Driver
 * ========================= */

/* Left motor */
#define BOARD_MOTOR_LEFT_PWM_GPIO        40
#define BOARD_MOTOR_LEFT_IN1_GPIO        41
#define BOARD_MOTOR_LEFT_IN2_GPIO        42

/* Right motor */
#define BOARD_MOTOR_RIGHT_PWM_GPIO       45
#define BOARD_MOTOR_RIGHT_IN1_GPIO       46
#define BOARD_MOTOR_RIGHT_IN2_GPIO       48


/* Motor PWM */
#define BOARD_MOTOR_PWM_MODE             LEDC_LOW_SPEED_MODE
#define BOARD_MOTOR_PWM_TIMER            LEDC_TIMER_2

#define BOARD_MOTOR_LEFT_PWM_CHANNEL     LEDC_CHANNEL_3
#define BOARD_MOTOR_RIGHT_PWM_CHANNEL    LEDC_CHANNEL_4

#define BOARD_MOTOR_PWM_DUTY_RES         LEDC_TIMER_10_BIT
#define BOARD_MOTOR_PWM_FREQUENCY_HZ     20000


/* =========================
 * Encoder
 * ========================= */

/* Left encoder */
#define BOARD_ENCODER_LEFT_A_GPIO        35
#define BOARD_ENCODER_LEFT_B_GPIO        36

/* Right encoder */
#define BOARD_ENCODER_RIGHT_A_GPIO       37
#define BOARD_ENCODER_RIGHT_B_GPIO       43
