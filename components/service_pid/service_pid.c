#include "service_pid.h"

#include <stddef.h>

esp_err_t service_pid_init(service_pid_t *pid, const service_pid_config_t *config)
{
    if (pid == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    pid->config = *config;
    pid->integral = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->prev_error = 0.0f;
    pid->first_run = true;

    return ESP_OK;
}

float service_pid_compute(service_pid_t *pid, float setpoint, float measurement, float dt)
{
    if (pid == NULL || dt <= 0.0f) {
        return 0.0f;
    }

    float error = setpoint - measurement;

    /* Proportional */
    float p_term = pid->config.kp * error;

    /* Integral with anti-windup via clamping */
    pid->integral += pid->config.ki * error * dt;
    if (pid->integral > pid->config.integral_max) {
        pid->integral = pid->config.integral_max;
    } else if (pid->integral < -pid->config.integral_max) {
        pid->integral = -pid->config.integral_max;
    }
    float i_term = pid->integral;

    /* Derivative on measurement to avoid derivative kick */
    float d_term = 0.0f;
    if (!pid->first_run) {
        d_term = -pid->config.kd * (measurement - pid->prev_measurement) / dt;
    }

    pid->prev_measurement = measurement;
    pid->prev_error = error;
    pid->first_run = false;

    float output = p_term + i_term + d_term;

    /* Clamp output */
    if (output > pid->config.output_max) {
        output = pid->config.output_max;
    } else if (output < pid->config.output_min) {
        output = pid->config.output_min;
    }

    return output;
}

void service_pid_reset(service_pid_t *pid)
{
    if (pid == NULL) return;

    pid->integral = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->prev_error = 0.0f;
    pid->first_run = true;
}

void service_pid_set_gains(service_pid_t *pid, float kp, float ki, float kd)
{
    if (pid == NULL) return;

    pid->config.kp = kp;
    pid->config.ki = ki;
    pid->config.kd = kd;
}

void service_pid_get_gains(const service_pid_t *pid, float *kp, float *ki, float *kd)
{
    if (pid == NULL) return;

    if (kp) *kp = pid->config.kp;
    if (ki) *ki = pid->config.ki;
    if (kd) *kd = pid->config.kd;
}
