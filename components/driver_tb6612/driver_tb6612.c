#include "driver_tb6612.h"

#include <stdbool.h>
#include <stddef.h>

#include "board.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "driver_tb6612";

static const int s_in1_gpios[DRIVER_TB6612_MOTOR_MAX] = {
    BOARD_MOTOR_LEFT_IN1_GPIO,
    BOARD_MOTOR_RIGHT_IN1_GPIO,
};

static const int s_in2_gpios[DRIVER_TB6612_MOTOR_MAX] = {
    BOARD_MOTOR_LEFT_IN2_GPIO,
    BOARD_MOTOR_RIGHT_IN2_GPIO,
};

static const ledc_channel_t s_pwm_channels[DRIVER_TB6612_MOTOR_MAX] = {
    BOARD_MOTOR_LEFT_PWM_CHANNEL,
    BOARD_MOTOR_RIGHT_PWM_CHANNEL,
};

static const int s_pwm_gpios[DRIVER_TB6612_MOTOR_MAX] = {
    BOARD_MOTOR_LEFT_PWM_GPIO,
    BOARD_MOTOR_RIGHT_PWM_GPIO,
};

static bool s_inited = false;

static uint32_t motor_max_duty(void)
{
    return bsp_pwm_max_duty(BOARD_MOTOR_PWM_DUTY_RES);
}

static bool motor_id_is_valid(driver_tb6612_motor_t motor)
{
    return motor >= DRIVER_TB6612_MOTOR_LEFT && motor < DRIVER_TB6612_MOTOR_MAX;
}

esp_err_t driver_tb6612_init(driver_tb6612_motor_t motor)
{
    if (!motor_id_is_valid(motor)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_inited) {
        bsp_pwm_timer_config_t timer_config = {
            .speed_mode = BOARD_MOTOR_PWM_MODE,
            .duty_resolution = BOARD_MOTOR_PWM_DUTY_RES,
            .timer_num = BOARD_MOTOR_PWM_TIMER,
            .frequency_hz = BOARD_MOTOR_PWM_FREQUENCY_HZ,
        };

        esp_err_t err = bsp_pwm_timer_init(&timer_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "pwm timer init failed: %s", esp_err_to_name(err));
            return err;
        }
        s_inited = true;
    }

    bsp_pwm_channel_config_t channel_config = {
        .gpio_num = s_pwm_gpios[motor],
        .speed_mode = BOARD_MOTOR_PWM_MODE,
        .channel = s_pwm_channels[motor],
        .timer_num = BOARD_MOTOR_PWM_TIMER,
        .duty = 0,
    };

    esp_err_t err = bsp_pwm_channel_init(&channel_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pwm channel init motor %d failed: %s", motor, esp_err_to_name(err));
        return err;
    }

    bsp_gpio_config_t gpio_config = {
        .pin = s_in1_gpios[motor],
        .mode = GPIO_MODE_OUTPUT,
        .pull_up = false,
        .pull_down = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = bsp_gpio_config(&gpio_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IN1 gpio config failed: %s", esp_err_to_name(err));
        return err;
    }

    gpio_config.pin = s_in2_gpios[motor];
    err = bsp_gpio_config(&gpio_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IN2 gpio config failed: %s", esp_err_to_name(err));
        return err;
    }

    bsp_gpio_set_level(s_in1_gpios[motor], 0);
    bsp_gpio_set_level(s_in2_gpios[motor], 0);

    ESP_LOGI(TAG, "motor %d initialized (PWM GPIO %d, IN1 %d, IN2 %d)",
             motor, s_pwm_gpios[motor], s_in1_gpios[motor], s_in2_gpios[motor]);
    return ESP_OK;
}

esp_err_t driver_tb6612_set_speed(driver_tb6612_motor_t motor, int16_t speed)
{
    if (!motor_id_is_valid(motor)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;

    if (speed > 0) {
        bsp_gpio_set_level(s_in1_gpios[motor], 1);
        bsp_gpio_set_level(s_in2_gpios[motor], 0);
        uint32_t duty = (uint32_t)speed * motor_max_duty() / 100;
        return bsp_pwm_set_duty(BOARD_MOTOR_PWM_MODE, s_pwm_channels[motor], duty);
    } else if (speed < 0) {
        bsp_gpio_set_level(s_in1_gpios[motor], 0);
        bsp_gpio_set_level(s_in2_gpios[motor], 1);
        uint32_t duty = (uint32_t)(-speed) * motor_max_duty() / 100;
        return bsp_pwm_set_duty(BOARD_MOTOR_PWM_MODE, s_pwm_channels[motor], duty);
    } else {
        bsp_gpio_set_level(s_in1_gpios[motor], 0);
        bsp_gpio_set_level(s_in2_gpios[motor], 0);
        return bsp_pwm_set_duty(BOARD_MOTOR_PWM_MODE, s_pwm_channels[motor], 0);
    }
}

esp_err_t driver_tb6612_set_pwm_raw(driver_tb6612_motor_t motor, uint32_t duty)
{
    if (!motor_id_is_valid(motor)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t max = motor_max_duty();
    if (duty > max) duty = max;

    if (duty > 0) {
        bsp_gpio_set_level(s_in1_gpios[motor], 1);
        bsp_gpio_set_level(s_in2_gpios[motor], 0);
    } else {
        bsp_gpio_set_level(s_in1_gpios[motor], 0);
        bsp_gpio_set_level(s_in2_gpios[motor], 0);
    }

    return bsp_pwm_set_duty(BOARD_MOTOR_PWM_MODE, s_pwm_channels[motor], duty);
}

esp_err_t driver_tb6612_brake(driver_tb6612_motor_t motor)
{
    if (!motor_id_is_valid(motor)) {
        return ESP_ERR_INVALID_ARG;
    }

    bsp_gpio_set_level(s_in1_gpios[motor], 1);
    bsp_gpio_set_level(s_in2_gpios[motor], 1);
    return bsp_pwm_set_duty(BOARD_MOTOR_PWM_MODE, s_pwm_channels[motor], motor_max_duty());
}

esp_err_t driver_tb6612_coast(driver_tb6612_motor_t motor)
{
    if (!motor_id_is_valid(motor)) {
        return ESP_ERR_INVALID_ARG;
    }

    bsp_gpio_set_level(s_in1_gpios[motor], 0);
    bsp_gpio_set_level(s_in2_gpios[motor], 0);
    return bsp_pwm_set_duty(BOARD_MOTOR_PWM_MODE, s_pwm_channels[motor], 0);
}
