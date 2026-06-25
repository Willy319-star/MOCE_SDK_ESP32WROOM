#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DRV8833_PWM_MODE          LEDC_HIGH_SPEED_MODE
#define DRV8833_PWM_TIMER         LEDC_TIMER_2
#define DRV8833_PWM_DUTY_RES      LEDC_TIMER_10_BIT
#define DRV8833_PWM_FREQ_HZ       20000
#define DRV8833_MAX_DUTY          ((1U << 10) - 1U)

/*
 * ESP32-WROOM control port 2 is used because PWM_A1..A4 are GPIO36/39/34/35
 * and those pins are input-only on ESP32.
 * This mapping matches the first working demo wiring.
 */
#define DRV8833_AIN1_GPIO         BOARD_PWM_B1_GPIO
#define DRV8833_AIN2_GPIO         BOARD_PWM_B2_GPIO
#define DRV8833_BIN1_GPIO         BOARD_PWM_B3_GPIO
#define DRV8833_BIN2_GPIO         BOARD_PWM_B4_GPIO

#define DRV8833_AIN1_CHANNEL      LEDC_CHANNEL_0
#define DRV8833_AIN2_CHANNEL      LEDC_CHANNEL_1
#define DRV8833_BIN1_CHANNEL      LEDC_CHANNEL_2
#define DRV8833_BIN2_CHANNEL      LEDC_CHANNEL_3

#define MOTOR_RUN_DUTY_PERCENT    50
#define MOTOR_STAGE_DELAY_MS      1000
#define MOTOR_STATUS_PERIOD_MS    1000

static const char *TAG = "esp32wroom_motor";

typedef enum {
    MOTOR_A = 0,
    MOTOR_B,
} motor_id_t;

typedef struct {
    const char *name;
    int in1_gpio;
    int in2_gpio;
    ledc_channel_t in1_channel;
    ledc_channel_t in2_channel;
} drv8833_motor_t;

static const drv8833_motor_t s_motors[] = {
    [MOTOR_A] = {
        .name = "A",
        .in1_gpio = DRV8833_AIN1_GPIO,
        .in2_gpio = DRV8833_AIN2_GPIO,
        .in1_channel = DRV8833_AIN1_CHANNEL,
        .in2_channel = DRV8833_AIN2_CHANNEL,
    },
    [MOTOR_B] = {
        .name = "B",
        .in1_gpio = DRV8833_BIN1_GPIO,
        .in2_gpio = DRV8833_BIN2_GPIO,
        .in1_channel = DRV8833_BIN1_CHANNEL,
        .in2_channel = DRV8833_BIN2_CHANNEL,
    },
};

static uint32_t percent_to_duty(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    return (DRV8833_MAX_DUTY * percent) / 100U;
}

static esp_err_t pwm_set(ledc_channel_t channel, uint32_t duty)
{
    ESP_RETURN_ON_ERROR(ledc_set_duty(DRV8833_PWM_MODE, channel, duty), TAG, "set duty failed");
    return ledc_update_duty(DRV8833_PWM_MODE, channel);
}

static esp_err_t motor_coast(motor_id_t motor)
{
    const drv8833_motor_t *m = &s_motors[motor];

    ESP_RETURN_ON_ERROR(pwm_set(m->in1_channel, 0), TAG, "motor %s IN1 coast failed", m->name);
    ESP_RETURN_ON_ERROR(pwm_set(m->in2_channel, 0), TAG, "motor %s IN2 coast failed", m->name);

    ESP_LOGI(TAG, "motor %s coast", m->name);
    printf("motor %s coast\r\n", m->name);
    return ESP_OK;
}

static esp_err_t motor_brake(motor_id_t motor)
{
    const drv8833_motor_t *m = &s_motors[motor];

    ESP_RETURN_ON_ERROR(pwm_set(m->in1_channel, DRV8833_MAX_DUTY), TAG, "motor %s IN1 brake failed", m->name);
    ESP_RETURN_ON_ERROR(pwm_set(m->in2_channel, DRV8833_MAX_DUTY), TAG, "motor %s IN2 brake failed", m->name);

    ESP_LOGI(TAG, "motor %s brake", m->name);
    printf("motor %s brake\r\n", m->name);
    return ESP_OK;
}

static esp_err_t motor_set_speed(motor_id_t motor, int8_t speed_percent)
{
    const drv8833_motor_t *m = &s_motors[motor];
    bool reverse = false;
    uint8_t abs_speed = 0;

    if (speed_percent < 0) {
        reverse = true;
        abs_speed = (uint8_t)(-speed_percent);
    } else {
        abs_speed = (uint8_t)speed_percent;
    }

    if (abs_speed > 100) {
        abs_speed = 100;
    }

    const uint32_t duty = percent_to_duty(abs_speed);
    if (reverse) {
        ESP_RETURN_ON_ERROR(pwm_set(m->in1_channel, 0), TAG, "motor %s reverse IN1 failed", m->name);
        ESP_RETURN_ON_ERROR(pwm_set(m->in2_channel, duty), TAG, "motor %s reverse IN2 failed", m->name);
    } else {
        ESP_RETURN_ON_ERROR(pwm_set(m->in1_channel, duty), TAG, "motor %s forward IN1 failed", m->name);
        ESP_RETURN_ON_ERROR(pwm_set(m->in2_channel, 0), TAG, "motor %s forward IN2 failed", m->name);
    }

    ESP_LOGI(TAG, "motor %s speed=%d%% duty=%" PRIu32, m->name, speed_percent, duty);
    printf("motor %s speed=%d%% duty=%" PRIu32 "\r\n", m->name, speed_percent, duty);
    return ESP_OK;
}

static esp_err_t drv8833_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = DRV8833_PWM_MODE,
        .duty_resolution = DRV8833_PWM_DUTY_RES,
        .timer_num = DRV8833_PWM_TIMER,
        .freq_hz = DRV8833_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "pwm timer init failed");

    const ledc_channel_config_t channels[] = {
        {
            .gpio_num = DRV8833_AIN1_GPIO,
            .speed_mode = DRV8833_PWM_MODE,
            .channel = DRV8833_AIN1_CHANNEL,
            .timer_sel = DRV8833_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
        {
            .gpio_num = DRV8833_AIN2_GPIO,
            .speed_mode = DRV8833_PWM_MODE,
            .channel = DRV8833_AIN2_CHANNEL,
            .timer_sel = DRV8833_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
        {
            .gpio_num = DRV8833_BIN1_GPIO,
            .speed_mode = DRV8833_PWM_MODE,
            .channel = DRV8833_BIN1_CHANNEL,
            .timer_sel = DRV8833_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
        {
            .gpio_num = DRV8833_BIN2_GPIO,
            .speed_mode = DRV8833_PWM_MODE,
            .channel = DRV8833_BIN2_CHANNEL,
            .timer_sel = DRV8833_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
    };

    for (size_t i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channels[i]), TAG, "pwm channel init failed");
    }

    gpio_set_drive_capability(DRV8833_AIN1_GPIO, GPIO_DRIVE_CAP_2);
    gpio_set_drive_capability(DRV8833_AIN2_GPIO, GPIO_DRIVE_CAP_2);
    gpio_set_drive_capability(DRV8833_BIN1_GPIO, GPIO_DRIVE_CAP_2);
    gpio_set_drive_capability(DRV8833_BIN2_GPIO, GPIO_DRIVE_CAP_2);

    ESP_LOGI(TAG, "DRV8833 PWM init done");
    ESP_LOGI(TAG, "Motor A: AIN1=PWM_B1/GPIO%d, AIN2=PWM_B2/GPIO%d",
             DRV8833_AIN1_GPIO, DRV8833_AIN2_GPIO);
    ESP_LOGI(TAG, "Motor B: BIN1=PWM_B3/GPIO%d, BIN2=PWM_B4/GPIO%d",
             DRV8833_BIN1_GPIO, DRV8833_BIN2_GPIO);
    printf("DRV8833 wiring: AIN1=PWM_B1 GPIO%d, AIN2=PWM_B2 GPIO%d, BIN1=PWM_B3 GPIO%d, BIN2=PWM_B4 GPIO%d\r\n",
           DRV8833_AIN1_GPIO, DRV8833_AIN2_GPIO, DRV8833_BIN1_GPIO, DRV8833_BIN2_GPIO);

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-WROOM DRV8833 motor driver example");
    printf("\r\n==== esp32wroom DRV8833 motor_driver ====\r\n");
    printf("PWM frequency=%d Hz, duty resolution=10-bit\r\n", DRV8833_PWM_FREQ_HZ);
    printf("Use ESP32-WROOM PWM_B header pins only; PWM_A pins are input-only.\r\n");
    printf("Sequence: A forward 1s, B forward 1s, wait 1s, then both forward at %d%% duty.\r\n",
           MOTOR_RUN_DUTY_PERCENT);

    ESP_ERROR_CHECK(drv8833_init());
    ESP_ERROR_CHECK(motor_coast(MOTOR_A));
    ESP_ERROR_CHECK(motor_coast(MOTOR_B));

    ESP_LOGI(TAG, "stage 1: motor A forward duty=%d%%", MOTOR_RUN_DUTY_PERCENT);
    printf("motor_driver stage1 A forward duty=%d%%\r\n", MOTOR_RUN_DUTY_PERCENT);
    ESP_ERROR_CHECK(motor_set_speed(MOTOR_A, MOTOR_RUN_DUTY_PERCENT));
    ESP_ERROR_CHECK(motor_coast(MOTOR_B));
    vTaskDelay(pdMS_TO_TICKS(MOTOR_STAGE_DELAY_MS));

    ESP_LOGI(TAG, "stage 2: motor B forward duty=%d%%", MOTOR_RUN_DUTY_PERCENT);
    printf("motor_driver stage2 B forward duty=%d%%\r\n", MOTOR_RUN_DUTY_PERCENT);
    ESP_ERROR_CHECK(motor_coast(MOTOR_A));
    ESP_ERROR_CHECK(motor_set_speed(MOTOR_B, MOTOR_RUN_DUTY_PERCENT));
    vTaskDelay(pdMS_TO_TICKS(MOTOR_STAGE_DELAY_MS));

    ESP_LOGI(TAG, "stage 3: wait after motor B");
    printf("motor_driver stage3 wait after B\r\n");
    ESP_ERROR_CHECK(motor_coast(MOTOR_A));
    ESP_ERROR_CHECK(motor_coast(MOTOR_B));
    vTaskDelay(pdMS_TO_TICKS(MOTOR_STAGE_DELAY_MS));

    ESP_LOGI(TAG, "stage 4: both motors forward duty=%d%%", MOTOR_RUN_DUTY_PERCENT);
    printf("motor_driver stage4 both forward duty=%d%%\r\n", MOTOR_RUN_DUTY_PERCENT);
    ESP_ERROR_CHECK(motor_set_speed(MOTOR_A, MOTOR_RUN_DUTY_PERCENT));
    ESP_ERROR_CHECK(motor_set_speed(MOTOR_B, MOTOR_RUN_DUTY_PERCENT));

    uint32_t counter = 0;
    while (true) {
        ESP_LOGI(TAG, "both motors forward duty=%d%% counter=%" PRIu32,
                 MOTOR_RUN_DUTY_PERCENT, counter);
        printf("motor_driver both forward duty=%d%% counter=%" PRIu32 "\r\n",
               MOTOR_RUN_DUTY_PERCENT, counter);
        counter++;
        vTaskDelay(pdMS_TO_TICKS(MOTOR_STATUS_PERIOD_MS));
    }
}
