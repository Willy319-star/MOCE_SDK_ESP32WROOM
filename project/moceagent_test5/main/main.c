#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "driver_button.h"
#include "driver_encoder.h"
#include "driver_led.h"
#include "driver_motor.h"
#include "driver_mpu6050.h"
#include "driver_oled.h"
#include "driver_tb6612.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"
#include "service_device.h"
#include "service_pid.h"

#define APP_LOOP_PERIOD_MS                 100
#define APP_PID_PERIOD_S                   0.10f
#define APP_TOF_NEAR_MM                    300
#define APP_TOF_VERY_NEAR_MM               180
#define APP_TOF_FAR_MM                     1200
#define APP_TILT_ENTER_DEG                 18.0f
#define APP_TILT_PROTECT_DEG               30.0f
#define APP_GYRO_SHAKE_DPS                 180.0f
#define APP_STALL_SPEED_THRESHOLD          5
#define APP_STALL_COUNT_LIMIT              8
#define APP_SELF_CHECK_COUNT_LIMIT         20

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

typedef enum {
    APP_MOTION_STOP = 0,
    APP_MOTION_FORWARD,
    APP_MOTION_BACKWARD,
    APP_MOTION_TURN_LEFT,
    APP_MOTION_TURN_RIGHT,
} app_motion_t;

typedef struct {
    app_state_t state;
    app_state_t prev_state;
    app_motion_t motion;
    uint16_t target_speed;
    uint16_t current_speed;
    uint16_t tof_distance_mm;
    float pitch_deg;
    float roll_deg;
    float gyro_mag_dps;
    int left_encoder_count;
    int right_encoder_count;
    int left_encoder_prev;
    int right_encoder_prev;
    int stall_counter;
    bool tof_ok;
    bool imu_ok;
    bool enc_ok;
    bool motor_ok;
    bool tts_ok;
    bool ready_to_move;
    bool obstacle_near;
    bool obstacle_very_near;
    bool user_near;
    bool tilt_protect;
    bool shake_protect;
    bool motor_stall;
} app_context_t;

static const char *TAG = "moceagent_test5";

static service_pid_t s_pid_left;
static service_pid_t s_pid_right;
static app_context_t s_ctx;

static float app_absf(float v)
{
    return (v < 0.0f) ? -v : v;
}

static void app_show_status(const char *line1, const char *line2)
{
    driver_oled_clear();
    driver_oled_draw_string(0, 0, line1, true);
    driver_oled_draw_string(0, 16, line2, true);
    driver_oled_flush();
}

static void app_speak_state(const char *text)
{
    if (driver_tw_tts_is_initialized()) {
        (void)driver_tw_tts_speak(text);
    }
}

static void app_set_motion_stop(void)
{
    (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, 0);
    (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, 0);
    (void)driver_tb6612_coast(DRIVER_TB6612_MOTOR_LEFT);
    (void)driver_tb6612_coast(DRIVER_TB6612_MOTOR_RIGHT);
    s_ctx.motion = APP_MOTION_STOP;
    s_ctx.target_speed = 0;
}

static void app_update_encoders(void)
{
    (void)driver_encoder_get_count(DRIVER_ENCODER_LEFT, &s_ctx.left_encoder_count);
    (void)driver_encoder_get_count(DRIVER_ENCODER_RIGHT, &s_ctx.right_encoder_count);
}

static void app_update_sensors(void)
{
    driver_tof2000c_vl53l0x_result_t tof = {0};
    driver_mpu6050_data_t imu = {0};

    if (driver_tof2000c_vl53l0x_read_continuous(&tof) == ESP_OK && !tof.timeout) {
        s_ctx.tof_distance_mm = tof.distance_mm;
        s_ctx.tof_ok = true;
    } else {
        s_ctx.tof_ok = false;
    }

    if (driver_mpu6050_read(&imu) == ESP_OK) {
        s_ctx.pitch_deg = atan2f(imu.accel_x_g, sqrtf(imu.accel_y_g * imu.accel_y_g + imu.accel_z_g * imu.accel_z_g)) * 57.29578f;
        s_ctx.roll_deg = atan2f(imu.accel_y_g, sqrtf(imu.accel_x_g * imu.accel_x_g + imu.accel_z_g * imu.accel_z_g)) * 57.29578f;
        s_ctx.gyro_mag_dps = fabsf(imu.gyro_x_dps) + fabsf(imu.gyro_y_dps) + fabsf(imu.gyro_z_dps);
        s_ctx.imu_ok = true;
    } else {
        s_ctx.imu_ok = false;
    }

    app_update_encoders();

    s_ctx.obstacle_very_near = s_ctx.tof_ok && (s_ctx.tof_distance_mm <= APP_TOF_VERY_NEAR_MM);
    s_ctx.obstacle_near = s_ctx.tof_ok && (s_ctx.tof_distance_mm <= APP_TOF_NEAR_MM);
    s_ctx.user_near = s_ctx.tof_ok && (s_ctx.tof_distance_mm <= APP_TOF_FAR_MM);
    s_ctx.tilt_protect = (app_absf(s_ctx.pitch_deg) >= APP_TILT_PROTECT_DEG) || (app_absf(s_ctx.roll_deg) >= APP_TILT_PROTECT_DEG);
    s_ctx.shake_protect = s_ctx.gyro_mag_dps >= APP_GYRO_SHAKE_DPS;
}

static void app_update_motor_health(void)
{
    int left_delta = s_ctx.left_encoder_count - s_ctx.left_encoder_prev;
    int right_delta = s_ctx.right_encoder_count - s_ctx.right_encoder_prev;

    s_ctx.left_encoder_prev = s_ctx.left_encoder_count;
    s_ctx.right_encoder_prev = s_ctx.right_encoder_count;

    if (s_ctx.state == APP_STATE_MOVE && s_ctx.target_speed > 0) {
        if (app_absf((float)left_delta) < APP_STALL_SPEED_THRESHOLD &&
            app_absf((float)right_delta) < APP_STALL_SPEED_THRESHOLD) {
            s_ctx.stall_counter++;
        } else {
            s_ctx.stall_counter = 0;
        }
    } else {
        s_ctx.stall_counter = 0;
    }

    s_ctx.motor_stall = (s_ctx.stall_counter >= APP_STALL_COUNT_LIMIT);
}

static void app_apply_pid_motion(int16_t base_speed)
{
    float left_feedback = (float)(s_ctx.left_encoder_count - s_ctx.left_encoder_prev);
    float right_feedback = (float)(s_ctx.right_encoder_count - s_ctx.right_encoder_prev);
    float left_out = service_pid_compute(&s_pid_left, (float)base_speed, left_feedback, APP_PID_PERIOD_S);
    float right_out = service_pid_compute(&s_pid_right, (float)base_speed, right_feedback, APP_PID_PERIOD_S);

    (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, (int16_t)left_out);
    (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, (int16_t)right_out);
    (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, (int16_t)left_out);
    (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, (int16_t)right_out);
}

static void app_state_transition(app_state_t next)
{
    if (s_ctx.state == next) {
        return;
    }

    s_ctx.prev_state = s_ctx.state;
    s_ctx.state = next;

    switch (next) {
    case APP_STATE_SELF_CHECK:
        app_speak_state("self check");
        app_show_status("SELF CHECK", "checking...");
        break;
    case APP_STATE_IDLE:
        app_speak_state("stand by");
        app_show_status("IDLE", "waiting user");
        break;
    case APP_STATE_INTERACT:
        app_speak_state("hello");
        app_show_status("INTERACT", "user near");
        break;
    case APP_STATE_MOVE:
        app_speak_state("moving");
        app_show_status("MOVE", "pid control");
        break;
    case APP_STATE_AVOID:
        app_speak_state("obstacle ahead");
        app_show_status("AVOID", "avoid obstacle");
        break;
    case APP_STATE_PROTECT:
        app_speak_state("protection mode");
        app_show_status("PROTECT", "motor stopped");
        break;
    case APP_STATE_ERROR:
        app_speak_state("system error");
        app_show_status("ERROR", "check system");
        break;
    case APP_STATE_INIT:
    default:
        break;
    }
}

static void app_self_check(void)
{
    bool tof_ok = false;
    bool imu_ok = false;
    bool enc_ok = false;
    bool motor_ok = false;
    bool tts_ok = false;

    for (int i = 0; i < APP_SELF_CHECK_COUNT_LIMIT; ++i) {
        driver_tof2000c_vl53l0x_result_t tof = {0};
        uint8_t who = 0;
        if (driver_tof2000c_vl53l0x_read_continuous(&tof) == ESP_OK || driver_tof2000c_vl53l0x_probe(DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT) == ESP_OK) {
            tof_ok = true;
        }
        if (driver_mpu6050_read_who_am_i(&who) == ESP_OK && who == DRIVER_MPU6050_WHO_AM_I_VALUE) {
            imu_ok = true;
        }
        int count = 0;
        if (driver_encoder_get_count(DRIVER_ENCODER_LEFT, &count) == ESP_OK &&
            driver_encoder_get_count(DRIVER_ENCODER_RIGHT, &count) == ESP_OK) {
            enc_ok = true;
        }
        if (driver_motor_get_ppr(DRIVER_MOTOR_LEFT) > 0 && driver_motor_get_ppr(DRIVER_MOTOR_RIGHT) > 0) {
            motor_ok = true;
        }
        if (driver_tw_tts_is_initialized()) {
            tts_ok = true;
        }

        if (tof_ok && imu_ok && enc_ok && motor_ok && tts_ok) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    s_ctx.tof_ok = tof_ok;
    s_ctx.imu_ok = imu_ok;
    s_ctx.enc_ok = enc_ok;
    s_ctx.motor_ok = motor_ok;
    s_ctx.tts_ok = tts_ok;

    if (tof_ok && imu_ok && enc_ok && motor_ok && tts_ok) {
        app_state_transition(APP_STATE_IDLE);
    } else {
        ESP_LOGE(TAG, "self check failed: tof=%d imu=%d enc=%d motor=%d tts=%d", tof_ok, imu_ok, enc_ok, motor_ok, tts_ok);
        app_state_transition(APP_STATE_ERROR);
    }
}

static void app_enter_protect_if_needed(void)
{
    if (s_ctx.tilt_protect || s_ctx.shake_protect || s_ctx.motor_stall || !s_ctx.tof_ok || !s_ctx.imu_ok) {
        app_set_motion_stop();
        app_state_transition(APP_STATE_PROTECT);
    }
}

static void app_state_run(void)
{
    switch (s_ctx.state) {
    case APP_STATE_INIT:
        app_state_transition(APP_STATE_SELF_CHECK);
        break;

    case APP_STATE_SELF_CHECK:
        app_self_check();
        break;

    case APP_STATE_IDLE:
        if (s_ctx.tilt_protect || s_ctx.shake_protect) {
            app_state_transition(APP_STATE_PROTECT);
        } else if (s_ctx.user_near) {
            app_state_transition(APP_STATE_INTERACT);
        } else if (s_ctx.motion != APP_MOTION_STOP) {
            app_state_transition(APP_STATE_MOVE);
        }
        break;

    case APP_STATE_INTERACT:
        if (s_ctx.tilt_protect || s_ctx.shake_protect) {
            app_state_transition(APP_STATE_PROTECT);
        } else if (s_ctx.obstacle_near) {
            app_state_transition(APP_STATE_AVOID);
        } else if (s_ctx.motion != APP_MOTION_STOP) {
            app_state_transition(APP_STATE_MOVE);
        } else {
            app_state_transition(APP_STATE_IDLE);
        }
        break;

    case APP_STATE_MOVE:
        if (s_ctx.tilt_protect || s_ctx.shake_protect || s_ctx.motor_stall) {
            app_state_transition(APP_STATE_PROTECT);
        } else if (!s_ctx.tof_ok || !s_ctx.imu_ok) {
            app_state_transition(APP_STATE_ERROR);
        } else if (s_ctx.obstacle_near) {
            app_state_transition(APP_STATE_AVOID);
        } else {
            int16_t base_speed = 50;
            if (s_ctx.obstacle_very_near) {
                base_speed = 25;
            }
            app_apply_pid_motion(base_speed);
        }
        break;

    case APP_STATE_AVOID:
        app_set_motion_stop();
        if (s_ctx.obstacle_very_near) {
            (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, -20);
            (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, 20);
            vTaskDelay(pdMS_TO_TICKS(300));
            app_set_motion_stop();
        }
        if (s_ctx.tilt_protect || s_ctx.shake_protect || s_ctx.motor_stall) {
            app_state_transition(APP_STATE_PROTECT);
        } else if (!s_ctx.obstacle_near) {
            app_state_transition(APP_STATE_MOVE);
        } else {
            app_state_transition(APP_STATE_IDLE);
        }
        break;

    case APP_STATE_PROTECT:
        app_set_motion_stop();
        if (!s_ctx.tilt_protect && !s_ctx.shake_protect && !s_ctx.motor_stall) {
            app_state_transition(APP_STATE_IDLE);
        }
        break;

    case APP_STATE_ERROR:
        app_set_motion_stop();
        break;

    default:
        app_state_transition(APP_STATE_ERROR);
        break;
    }
}

void app_main(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = APP_STATE_INIT;

    service_device_init();

    driver_led_init();
    driver_button_init();

    (void)driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);
    (void)driver_oled_clear_screen();
    app_show_status("BOOT", "initializing...");

    (void)driver_tof2000c_vl53l0x_init_default();
    (void)driver_mpu6050_init_default();
    (void)driver_tw_tts_init_default();

    driver_motor_config_t motor_cfg = {
        .ppr = DRIVER_MOTOR_PPR_DEFAULT,
    };
    (void)driver_motor_init(DRIVER_MOTOR_LEFT, &motor_cfg);
    (void)driver_motor_init(DRIVER_MOTOR_RIGHT, &motor_cfg);
    (void)driver_tb6612_init(DRIVER_TB6612_MOTOR_LEFT);
    (void)driver_tb6612_init(DRIVER_TB6612_MOTOR_RIGHT);

    (void)driver_encoder_init(DRIVER_ENCODER_LEFT);
    (void)driver_encoder_init(DRIVER_ENCODER_RIGHT);
    (void)driver_encoder_reset(DRIVER_ENCODER_LEFT);
    (void)driver_encoder_reset(DRIVER_ENCODER_RIGHT);

    service_pid_config_t pid_cfg = {
        .kp = 1.2f,
        .ki = 0.08f,
        .kd = 0.02f,
        .output_min = -100.0f,
        .output_max = 100.0f,
        .integral_max = 200.0f,
    };
    (void)service_pid_init(&s_pid_left, &pid_cfg);
    (void)service_pid_init(&s_pid_right, &pid_cfg);

    app_set_motion_stop();
    driver_led_set_brightness(20);
    app_speak_state("power on complete");

    app_state_transition(APP_STATE_SELF_CHECK);

    while (1) {
        driver_button_process();
        driver_button_event_t evt = driver_button_get_event();
        if (evt == DRIVER_BUTTON_EVENT_SHORT_PRESS) {
            if (s_ctx.state == APP_STATE_IDLE || s_ctx.state == APP_STATE_INTERACT) {
                s_ctx.motion = APP_MOTION_FORWARD;
                s_ctx.target_speed = 50;
                app_state_transition(APP_STATE_MOVE);
            } else {
                app_set_motion_stop();
                app_state_transition(APP_STATE_IDLE);
            }
        } else if (evt == DRIVER_BUTTON_EVENT_LONG_PRESS) {
            app_set_motion_stop();
            app_state_transition(APP_STATE_PROTECT);
        }

        app_update_sensors();
        app_update_motor_health();
        app_enter_protect_if_needed();
        app_state_run();

        switch (s_ctx.state) {
        case APP_STATE_IDLE:
            driver_led_set_brightness(10);
            break;
        case APP_STATE_INTERACT:
            driver_led_set_brightness(30);
            break;
        case APP_STATE_MOVE:
            driver_led_set_brightness(60);
            break;
        case APP_STATE_AVOID:
            driver_led_set_brightness(80);
            break;
        case APP_STATE_PROTECT:
        case APP_STATE_ERROR:
            driver_led_set_brightness(100);
            break;
        default:
            break;
        }

        char line1[32];
        char line2[32];
        snprintf(line1, sizeof(line1), "S:%d D:%u", (int)s_ctx.state, (unsigned)s_ctx.tof_distance_mm);
        snprintf(line2, sizeof(line2), "P:%.1f R:%.1f", s_ctx.pitch_deg, s_ctx.roll_deg);
        app_show_status(line1, line2);

        vTaskDelay(pdMS_TO_TICKS(APP_LOOP_PERIOD_MS));
    }
}