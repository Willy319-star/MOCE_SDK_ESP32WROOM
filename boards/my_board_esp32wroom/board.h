#pragma once

#include "hal/i2c_types.h"
#include "hal/ledc_types.h"

/*
 * ESP32-WROOM-32E-N4 board profile.
 *
 * Pin map follows the schematic labels:
 * - TXD0/RXD0 are reserved for programming/log serial.
 * - TXD1/RXD1 are routed to the external serial header.
 * - GPIO21/GPIO22 are the I2C0 bus.
 * - GPIO23/19/18/15/13 are the SPI bus.
 * - GPIO5/GPIO4 are the CAN transceiver TX/RX signals.
 */

/* On-board controllable LED */
#define BOARD_LED_GPIO               12

#define BOARD_LED_PWM_MODE           LEDC_HIGH_SPEED_MODE
#define BOARD_LED_PWM_TIMER          LEDC_TIMER_0
#define BOARD_LED_PWM_CHANNEL        LEDC_CHANNEL_0
#define BOARD_LED_PWM_DUTY_RES       LEDC_TIMER_10_BIT
#define BOARD_LED_PWM_FREQUENCY_HZ   5000

/* Boot button / IO0 */
#define BOARD_BUTTON_GPIO            0
#define BOARD_BUTTON_ACTIVE_LEVEL    0   /* 0: pressed low, 1: pressed high */
#define BOARD_BUTTON_USE_PULLUP      1
#define BOARD_BUTTON_USE_PULLDOWN    0

/* Timing */
#define BOARD_BUTTON_DEBOUNCE_MS     30
#define BOARD_BUTTON_LONG_PRESS_MS   1000

/* PWM header nets from the schematic */
#define BOARD_PWM_A1_GPIO            36  /* input-only on ESP32 */
#define BOARD_PWM_A2_GPIO            39  /* input-only on ESP32 */
#define BOARD_PWM_A3_GPIO            34  /* input-only on ESP32 */
#define BOARD_PWM_A4_GPIO            35  /* input-only on ESP32 */
#define BOARD_PWM_B1_GPIO            32
#define BOARD_PWM_B2_GPIO            33
#define BOARD_PWM_B3_GPIO            25
#define BOARD_PWM_B4_GPIO            26

/* Servo defaults use output-capable PWM header pins. */
#define BOARD_SERVO_COUNT            2
#define BOARD_SERVO_GPIO_0           BOARD_PWM_B1_GPIO
#define BOARD_SERVO_GPIO_1           BOARD_PWM_B2_GPIO

#define BOARD_SERVO_PWM_MODE         LEDC_HIGH_SPEED_MODE
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

/* I2C0 bus for the external I2C connectors */
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

/* UART1 external serial header */
#define BOARD_UART_PORT                      1
#define BOARD_UART_TX_GPIO                   17
#define BOARD_UART_RX_GPIO                   16
#define BOARD_UART_BAUD_RATE                 9600
#define BOARD_UART_RX_BUFFER_SIZE            256
#define BOARD_UART_TX_BUFFER_SIZE            0
#define BOARD_UART_EVENT_QUEUE_SIZE          0

/* SPI headers */
#define BOARD_SPI_MOSI_GPIO                  23
#define BOARD_SPI_MISO_GPIO                  19
#define BOARD_SPI_SCK_GPIO                   18
#define BOARD_SPI_CS0_GPIO                   15
#define BOARD_SPI_CS1_GPIO                   13

/* CAN transceiver */
#define BOARD_CAN_TX_GPIO                    5
#define BOARD_CAN_RX_GPIO                    4

/* Extra GPIO header pins */
#define BOARD_GPIO_IO27                      27
#define BOARD_GPIO_IO14                      14
#define BOARD_GPIO_IO2                       2

/* =========================
 * TB6612 Motor Driver
 * =========================
 *
 * This ESP32-WROOM schematic does not include a dedicated TB6612 circuit.
 * These defaults map the SDK motor BSP onto available output-capable header
 * pins so motor examples can still be wired externally.
 */

/* Left motor */
#define BOARD_MOTOR_LEFT_PWM_GPIO        BOARD_PWM_B1_GPIO
#define BOARD_MOTOR_LEFT_IN1_GPIO        BOARD_PWM_B2_GPIO
#define BOARD_MOTOR_LEFT_IN2_GPIO        BOARD_PWM_B3_GPIO

/* Right motor */
#define BOARD_MOTOR_RIGHT_PWM_GPIO       BOARD_PWM_B4_GPIO
#define BOARD_MOTOR_RIGHT_IN1_GPIO       BOARD_GPIO_IO27
#define BOARD_MOTOR_RIGHT_IN2_GPIO       BOARD_GPIO_IO14

/* Motor PWM */
#define BOARD_MOTOR_PWM_MODE             LEDC_HIGH_SPEED_MODE
#define BOARD_MOTOR_PWM_TIMER            LEDC_TIMER_2

#define BOARD_MOTOR_LEFT_PWM_CHANNEL     LEDC_CHANNEL_3
#define BOARD_MOTOR_RIGHT_PWM_CHANNEL    LEDC_CHANNEL_4

#define BOARD_MOTOR_PWM_DUTY_RES         LEDC_TIMER_10_BIT
#define BOARD_MOTOR_PWM_FREQUENCY_HZ     20000

/* =========================
 * Encoder
 * =========================
 *
 * The PWM_A header nets are input-capable and suitable for encoder inputs.
 */

/* Left encoder */
#define BOARD_ENCODER_LEFT_A_GPIO        BOARD_PWM_A1_GPIO
#define BOARD_ENCODER_LEFT_B_GPIO        BOARD_PWM_A2_GPIO

/* Right encoder */
#define BOARD_ENCODER_RIGHT_A_GPIO       BOARD_PWM_A3_GPIO
#define BOARD_ENCODER_RIGHT_B_GPIO       BOARD_PWM_A4_GPIO
