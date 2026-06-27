#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "driver_button.h"
#include "driver_encoder.h"
#include "driver_motor.h"
#include "driver_mpu6050.h"
#include "driver_oled.h"
#include "driver_tb6612.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"
#include "service_device.h"
#include "service_pid.h"

static const char *TAG = "510demo";

typedef enum {
    APP_STATE_INIT = 0,
    APP_STATE_SELF_CHECK,
    APP_STATE_IDLE,
    APP_STATE_INTERACT,
    APP_STATE_MOVE,
    APP_STATE_AVOID,
    APP_STATE_PROTECT,
    APP_STATE_ERROR,
} app_state_t;

typedef struct {
    app_state_t state;
    app_state_t last_state;

    bool mpu_ok;
    bool tof_ok;
    bool oled_ok;
    bool tts_ok;
    bool motor_ok;
    bool encoder_ok;

    float pitch_deg;
    float roll_deg;
    float tilt_abs_deg;

    uint16_t tof_distance_mm;
    uint8_t tof_range_status;
    bool tof_timeout;

    int encoder_left;
    int encoder_right;
    int encoder_left_prev;
    int encoder_right_prev;

    bool user_near;
    bool obstacle_near;
    bool pose_abnormal;
    bool motor_stall;
    bool movement_requested;
    bool moving_forward;
    bool moving_reverse;
    bool was_lifted;
} app_context_t;

static app_context_t s_ctx;

static service_pid_t s_pid_left;
static service_pid_t s_pid_right;

static void oled_show_line(int y, const char *text)
{
    driver_oled_draw_string(0, y, text, true);
}

static const char *state_to_string(app_state_t state)
{
    switch (state) {
    case APP_STATE_INIT: return "INIT";
    case APP_STATE_SELF_CHECK: return "SELFCHK";
    case APP_STATE_IDLE: return "IDLE";
    case APP_STATE_INTERACT: return "INTERACT";
    case APP_STATE_MOVE: return "MOVE";
    case APP_STATE_AVOID: return "AVOID";
    case APP_STATE_PROTECT: return "PROTECT";
    case APP_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

static void speak_once_on_state_change(app_state_t prev, app_state_t next)
{
    if (!s_ctx.tts_ok || prev == next) {
        return;
    }

    switch (next) {
    case APP_STATE_SELF_CHECK:
        driver_tw_tts_speak("system self check");
        break;
    case APP_STATE_IDLE:
        driver_tw_tts_speak("enter standby");
        break;
    case APP_STATE_INTERACT:
        driver_tw_tts_speak("user detected");
        break;
    case APP_STATE_MOVE:
        driver_tw_tts_speak("moving");
        break;
    case APP_STATE_AVOID:
        driver_tw_tts_speak("obstacle ahead");
        break;
    case APP_STATE_PROTECT:
        driver_tw_tts_speak("protection mode");
        break;
    case APP_STATE_ERROR:
        driver_tw_tts_speak("system error");
        break;
    default:
        break;
    }
}

static void show_status(void)
{
    char line[32];

    driver_oled_clear();

    snprintf(line, sizeof(line), "ST:%s", state_to_string(s_ctx.state));
    oled_show_line(0, line);

    snprintf(line, sizeof(line), "TOF:%u%s", s_ctx.tof_distance_mm, s_ctx.tof_timeout ? " T" : "");
    oled_show_line(12, line);

    snprintf(line, sizeof(line), "MPU:P%.1f R%.1f", s_ctx.pitch_deg, s_ctx.roll_deg);
    oled_show_line(24, line);

    snprintf(line, sizeof(line), "ENC:%d/%d", s_ctx.encoder_left, s_ctx.encoder_right);
    oled_show_line(36, line);

    snprintf(line, sizeof(line), "M:%s", s_ctx.moving_forward ? "F" : (s_ctx.moving_reverse ? "R" : "S"));
    oled_show_line(48, line);

    driver_oled_flush();
}

static void motor_stop_all(void)
{
    driver_motor_brake(DRIVER_MOTOR_LEFT);
    driver_motor_brake(DRIVER_MOTOR_RIGHT);
    driver_tb6612_brake(DRIVER_TB6612_MOTOR_LEFT);
    driver_tb6612_brake(DRIVER_TB6612_MOTOR_RIGHT);
}

static void motor_run_forward(int16_t speed)
{
    driver_motor_set_speed(DRIVER_MOTOR_LEFT, speed);
    driver_motor_set_speed(DRIVER_MOTOR_RIGHT, speed);
    driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, speed);
    driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, speed);
}

static void motor_run_reverse(int16_t speed)
{
    driver_motor_set_speed(DRIVER_MOTOR_LEFT, -speed);
    driver_motor_set_speed(DRIVER_MOTOR_RIGHT, -speed);
    driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, -speed);
    driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, -speed);
}

static void update_sensor_fusion(void)
{
    driver_mpu6050_data_t mpu = {0};
    driver_tof2000c_vl53l0x_result_t tof = {0};
    int count = 0;

    if (driver_mpu6050_read(&mpu) == ESP_OK) {
        s_ctx.mpu_ok = true;
        s_ctx.pitch_deg = atan2f(mpu.accel_x_g, sqrtf(mpu.accel_y_g * mpu.accel_y_g + mpu.accel_z_g * mpu.accel_z_g)) * 57.29578f;
        s_ctx.roll_deg = atan2f(mpu.accel_y_g, mpu.accel_z_g) * 57.29578f;
        s_ctx.tilt_abs_deg = fabsf(s_ctx.pitch_deg) > fabsf(s_ctx.roll_deg) ? fabsf(s_ctx.pitch_deg) : fabsf(s_ctx.roll_deg);
    } else {
        s_ctx.mpu_ok = false;
    }

    if (driver_tof2000c_vl53l0x_read_continuous(&tof) == ESP_OK) {
        s_ctx.tof_ok = true;
        s_ctx.tof_distance_mm = tof.distance_mm;
        s_ctx.tof_range_status = tof.range_status;
        s_ctx.tof_timeout = tof.timeout;
    } else {
        s_ctx.tof_ok = false;
        s_ctx.tof_timeout = true;
    }

    if (driver_encoder_get_count(DRIVER_ENCODER_LEFT, &count) == ESP_OK) {
        s_ctx.encoder_ok = true;
        s_ctx.encoder_left = count;
    } else {
        s_ctx.encoder_ok = false;
    }

    if (driver_encoder_get_count(DRIVER_ENCODER_RIGHT, &count) == ESP_OK) {
        s_ctx.encoder_ok = s_ctx.encoder_ok && true;
        s_ctx.encoder_right = count;
    } else {
        s_ctx.encoder_ok = false;
    }

    s_ctx.user_near = (s_ctx.tof_ok && !s_ctx.tof_timeout && s_ctx.tof_distance_mm > 0 && s_ctx.tof_distance_mm < 500);
    s_ctx.obstacle_near = (s_ctx.tof_ok && !s_ctx.tof_timeout && s_ctx.tof_distance_mm > 0 && s_ctx.tof_distance_mm < 200);
    s_ctx.pose_abnormal = (!s_ctx.mpu_ok) || (s_ctx.tilt_abs_deg > 35.0f);
    s_ctx.motor_stall = false;

    if (s_ctx.encoder_left == s_ctx.encoder_left_prev && s_ctx.encoder_right == s_ctx.encoder_right_prev && s_ctx.moving_forward) {
        s_ctx.motor_stall = true;
    }

    s_ctx.encoder_left_prev = s_ctx.encoder_left;
    s_ctx.encoder_right_prev = s_ctx.encoder_right;
}

static void transition_to(app_state_t next)
{
    if (s_ctx.state == next) {
        return;
    }

    app_state_t prev = s_ctx.state;
    s_ctx.last_state = prev;
    s_ctx.state = next;
    ESP_LOGI(TAG, "state %s -> %s", state_to_string(prev), state_to_string(next));
    speak_once_on_state_change(prev, next);
}

static void process_button(void)
{
    driver_button_process();
    switch (driver_button_get_event()) {
    case DRIVER_BUTTON_EVENT_SHORT_PRESS:
        s_ctx.movement_requested = true;
        s_ctx.moving_forward = !s_ctx.moving_forward;
        s_ctx.moving_reverse = !s_ctx.moving_forward;
        ESP_LOGI(TAG, "button short press: movement_requested=%d forward=%d", (int)s_ctx.movement_requested, (int)s_ctx.moving_forward);
        break;
    case DRIVER_BUTTON_EVENT_LONG_PRESS:
        s_ctx.movement_requested = false;
        s_ctx.moving_forward = false;
        s_ctx.moving_reverse = false;
        ESP_LOGI(TAG, "button long press: stop request");
        break;
    default:
        break;
    }
}

static void app_self_check(void)
{
    s_ctx.mpu_ok = driver_mpu6050_probe(DRIVER_MPU6050_I2C_ADDR_DEFAULT) == ESP_OK;
    s_ctx.tof_ok = driver_tof2000c_vl53l0x_probe(DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT) == ESP_OK;
    s_ctx.oled_ok = driver_oled_is_initialized();
    s_ctx.tts_ok = driver_tw_tts_is_initialized();
    s_ctx.motor_ok = true;
    s_ctx.encoder_ok = true;

    if (driver_encoder_reset(DRIVER_ENCODER_LEFT) != ESP_OK) {
        s_ctx.encoder_ok = false;
    }
    if (driver_encoder_reset(DRIVER_ENCODER_RIGHT) != ESP_OK) {
        s_ctx.encoder_ok = false;
    }

    if (driver_motor_reset_encoder(DRIVER_MOTOR_LEFT) != ESP_OK) {
        s_ctx.motor_ok = false;
    }
    if (driver_motor_reset_encoder(DRIVER_MOTOR_RIGHT) != ESP_OK) {
        s_ctx.motor_ok = false;
    }

    if (s_ctx.mpu_ok && s_ctx.tof_ok && s_ctx.oled_ok && s_ctx.tts_ok && s_ctx.motor_ok && s_ctx.encoder_ok) {
        transition_to(APP_STATE_IDLE);
    } else {
        ESP_LOGE(TAG, "self check failed mpu=%d tof=%d oled=%d tts=%d motor=%d enc=%d",
                 s_ctx.mpu_ok, s_ctx.tof_ok, s_ctx.oled_ok, s_ctx.tts_ok, s_ctx.motor_ok, s_ctx.encoder_ok);
        transition_to(APP_STATE_ERROR);
    }
}

static void app_state_machine(void)
{
    update_sensor_fusion();
    process_button();

    if (s_ctx.state == APP_STATE_INIT) {
        transition_to(APP_STATE_SELF_CHECK);
        return;
    }

    if (s_ctx.state == APP_STATE_SELF_CHECK) {
        app_self_check();
        return;
    }

    if (s_ctx.pose_abnormal || s_ctx.tof_timeout) {
        transition_to(APP_STATE_PROTECT);
    } else if (s_ctx.obstacle_near) {
        transition_to(APP_STATE_AVOID);
    } else if (s_ctx.user_near && !s_ctx.moving_forward && !s_ctx.moving_reverse) {
        transition_to(APP_STATE_INTERACT);
    } else if (s_ctx.movement_requested) {
        transition_to(APP_STATE_MOVE);
    } else {
        transition_to(APP_STATE_IDLE);
    }

    switch (s_ctx.state) {
    case APP_STATE_IDLE:
        motor_stop_all();
        break;
    case APP_STATE_INTERACT:
        motor_stop_all();
        break;
    case APP_STATE_MOVE: {
        float setpoint = 20.0f;
        float left_out = service_pid_compute(&s_pid_left, setpoint, (float)s_ctx.encoder_left, 0.1f);
        float right_out = service_pid_compute(&s_pid_right, setpoint, (float)s_ctx.encoder_right, 0.1f);
        int16_t speed_l = (int16_t)left_out;
        int16_t speed_r = (int16_t)right_out;
        if (s_ctx.moving_reverse) {
            speed_l = -speed_l;
            speed_r = -speed_r;
        }
        driver_motor_set_speed(DRIVER_MOTOR_LEFT, speed_l);
        driver_motor_set_speed(DRIVER_MOTOR_RIGHT, speed_r);
        driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, speed_l);
        driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, speed_r);
        break;
    }
    case APP_STATE_AVOID:
        motor_stop_all();
        if (s_ctx.tts_ok) {
            driver_tw_tts_speak("avoid obstacle");
        }
        break;
    case APP_STATE_PROTECT:
        motor_stop_all();
        if (s_ctx.tts_ok) {
            driver_tw_tts_speak("protection stop");
        }
        break;
    case APP_STATE_ERROR:
        motor_stop_all();
        break;
    default:
        break;
    }

    show_status();
}

static void init_peripherals(void)
{
    service_device_init();
    driver_button_init();

    driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);
    driver_oled_set_power(true);
    driver_oled_clear_screen();

    driver_mpu6050_init_default();
    driver_tof2000c_vl53l0x_init_default();
    driver_tw_tts_init_default();

    driver_encoder_init(DRIVER_ENCODER_LEFT);
    driver_encoder_init(DRIVER_ENCODER_RIGHT);

    driver_motor_init(DRIVER_MOTOR_LEFT, &(driver_motor_config_t){ .ppr = DRIVER_MOTOR_PPR_DEFAULT });
    driver_motor_init(DRIVER_MOTOR_RIGHT, &(driver_motor_config_t){ .ppr = DRIVER_MOTOR_PPR_DEFAULT });
    driver_tb6612_init(DRIVER_TB6612_MOTOR_LEFT);
    driver_tb6612_init(DRIVER_TB6612_MOTOR_RIGHT);

    service_pid_init(&s_pid_left, &(service_pid_config_t){ .kp = 2.0f, .ki = 0.1f, .kd = 0.02f, .output_min = -100.0f, .output_max = 100.0f, .integral_max = 50.0f });
    service_pid_init(&s_pid_right, &(service_pid_config_t){ .kp = 2.0f, .ki = 0.1f, .kd = 0.02f, .output_min = -100.0f, .output_max = 100.0f, .integral_max = 50.0f });

    motor_stop_all();
}

void app_main(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = APP_STATE_INIT;
    s_ctx.last_state = APP_STATE_INIT;

    init_peripherals();
    transition_to(APP_STATE_SELF_CHECK);

    while (1) {
        app_state_machine();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}