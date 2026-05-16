#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_min;
    float output_max;
    float integral_max;
} service_pid_config_t;

typedef struct {
    service_pid_config_t config;
    float integral;
    float prev_measurement;
    float prev_error;
    bool first_run;
} service_pid_t;

esp_err_t service_pid_init(service_pid_t *pid, const service_pid_config_t *config);
float service_pid_compute(service_pid_t *pid, float setpoint, float measurement, float dt);
void service_pid_reset(service_pid_t *pid);
void service_pid_set_gains(service_pid_t *pid, float kp, float ki, float kd);
void service_pid_get_gains(const service_pid_t *pid, float *kp, float *ki, float *kd);

#ifdef __cplusplus
}
#endif
