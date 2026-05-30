#include <stdio.h>
#include <string.h>
#include <math.h>

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "service_device.h"
#include "service_pid.h"

#include "driver_button.h"
#include "driver_encoder.h"
#include "driver_mpu6050.h"
#include "driver_oled.h"
#include "driver_motor.h"
#include "driver_tb6612.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"

#define APP_LOOP_PERIOD_MS              100
#define APP_TELEMETRY_PERIOD_MS         300
#define APP_BOOT_WAIT_MS                1500
#define APP_SELF_CHECK_TICKS            20
#define APP_MOVE_SPEED_DEFAULT          220
#define APP_MOVE_SPEED_SLOW            120
#define APP_TURN_SPEED                  180
#define APP_DISTANCE_NEAR_MM            450
#define APP_DISTANCE_OBSTACLE_MM        250
#define APP_DISTANCE_CRITICAL_MM        160
#define APP_TILT_LIMIT_DEG              28.0f
#define APP_LIFT_ACCEL_LIMIT_G          0.45f
#define APP_STALL_DELTA_COUNT           3
#define APP_STALL_TICKS                 6
#define APP_PID_DT_S                    0.10f

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
    uint16_t tof_distance_mm;
    uint8_t tof_status;
    bool tof_timeout;
    driver_mpu6050_data_t mpu;
    int encoder_left;
    int encoder_right;
    bool encoder_ok;
    bool motor_ok;
} app_sensor_data_t;

static const char *TAG = "robot_agent";

static service_pid_t s_pid_left;
static service_pid_t s_pid_right;
static app_state_t s_state = APP_STATE_INIT;
static app_state_t s_last_spoken_state = APP_STATE_ERROR;
static int s_stall_ticks = 0;
static int s_prev_left_count = 0;
static int s_prev_right_count = 0;
static bool s_have_prev_count = false;
static bool s_initialized = false;

static void app_stop_motors(void)
{
    (void)driver_motor_brake(DRIVER_MOTOR_LEFT);
    (void)driver_motor_brake(DRIVER_MOTOR_RIGHT);
    (void)driver_tb6612_brake(DRIVER_TB6612_MOTOR_LEFT);
    (void)driver_tb6612_brake(DRIVER_TB6612_MOTOR_RIGHT);
    (void)driver_motor_set_pwm_raw(DRIVER_MOTOR_LEFT, 0);
    (void)driver_motor_set_pwm_raw(DRIVER_MOTOR_RIGHT, 0);
}

static void app_oled_show(const app_sensor_data_t *s, app_state_t state)
{
    char line0[24];
    char line1[24];
    char line2[24];
    char line3[24];
    char line4[24];

    snprintf(line0, sizeof(line0), "State:%d", (int)state);
    snprintf(line1, sizeof(line1), "TOF:%umm", (unsigned int)s->tof_distance_mm);
    snprintf(line2, sizeof(line2), "A:%.1fG", (double)s->mpu.accel_z_g);
    snprintf(line3, sizeof(line3), "GY:%.1f", (double)s->mpu.gyro_z_dps);
    snprintf(line4, sizeof(line4), "L:%d R:%d", s->encoder_left, s->encoder_right);

    driver_oled_clear();
    driver_oled_draw_string(0, 0, line0, true);
    driver_oled_draw_string(0, 12, line1, true);
    driver_oled_draw_string(0, 24, line2, true);
    driver_oled_draw_string(0, 36, line3, true);
    driver_oled_draw_string(0, 48, line4, true);
    (void)driver_oled_flush();
}

static void app_speak_state_once(app_state_t state)
{
    if (s_last_spoken_state == state) {
        return;
    }

    switch (state) {
    case APP_STATE_SELF_CHECK:
        (void)driver_tw_tts_speak("系统自检中");
        break;
    case APP_STATE_IDLE:
        (void)driver_tw_tts_speak("进入待机");
        break;
    case APP_STATE_INTERACT:
        (void)driver_tw_tts_speak("检测到用户靠近");
        break;
    case APP_STATE_MOVE:
        (void)driver_tw_tts_speak("开始移动");
        break;
    case APP_STATE_AVOID:
        (void)driver_tw_tts_speak("前方有障碍，正在避障");
        break;
    case APP_STATE_PROTECT:
        (void)driver_tw_tts_speak("进入保护状态");
        break;
    case APP_STATE_ERROR:
        (void)driver_tw_tts_speak("系统故障，请检查");
        break;
    case APP_STATE_INIT:
    default:
        break;
    }

    s_last_spoken_state = state;
}

static esp_err_t app_init_all(void)
{
    esp_err_t ret;

    service_device_init();
    driver_button_init();

    ret = driver_tof2000c_vl53l0x_init_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TOF init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_mpu6050_init_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_tw_tts_init_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_motor_init(DRIVER_MOTOR_LEFT, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Left motor init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_motor_init(DRIVER_MOTOR_RIGHT, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Right motor init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_tb6612_init(DRIVER_TB6612_MOTOR_LEFT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TB6612 left init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_tb6612_init(DRIVER_TB6612_MOTOR_RIGHT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TB6612 right init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_encoder_init(DRIVER_ENCODER_LEFT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Left encoder init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = driver_encoder_init(DRIVER_ENCODER_RIGHT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Right encoder init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    app_stop_motors();
    ESP_LOGI(TAG, "All peripherals initialized");
    return ESP_OK;
}

static bool app_collect_sensors(app_sensor_data_t *s)
{
    driver_tof2000c_vl53l0x_result_t tof = {0};
    esp_err_t ret;

    ret = driver_tof2000c_vl53l0x_read_single(&tof);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TOF read failed: %s", esp_err_to_name(ret));
        s->tof_timeout = true;
        s->tof_distance_mm = 0;
    } else {
        s->tof_timeout = tof.timeout;
        s->tof_distance_mm = tof.distance_mm;
        s->tof_status = tof.range_status;
    }

    ret = driver_mpu6050_read(&s->mpu);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 read failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = driver_encoder_get_count(DRIVER_ENCODER_LEFT, &s->encoder_left);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Left encoder read failed: %s", esp_err_to_name(ret));
        s->encoder_ok = false;
    }
    ret = driver_encoder_get_count(DRIVER_ENCODER_RIGHT, &s->encoder_right);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Right encoder read failed: %s", esp_err_to_name(ret));
        s->encoder_ok = false;
    }

    if (s->encoder_ok) {
        s->motor_ok = true;
    }

    return true;
}

static bool app_is_tilted(const app_sensor_data_t *s)
{
    float ax = fabsf(s->mpu.accel_x_g);
    float ay = fabsf(s->mpu.accel_y_g);
    float az = fabsf(s->mpu.accel_z_g);
    float tilt_score = ax + ay;

    return (tilt_score > 1.1f) || (az < 0.6f) || (ax > APP_LIFT_ACCEL_LIMIT_G) || (ay > APP_LIFT_ACCEL_LIMIT_G);
}

static bool app_is_near_user(const app_sensor_data_t *s)
{
    return (!s->tof_timeout && s->tof_distance_mm > 0 && s->tof_distance_mm <= APP_DISTANCE_NEAR_MM);
}

static bool app_is_obstacle(const app_sensor_data_t *s)
{
    return (!s->tof_timeout && s->tof_distance_mm > 0 && s->tof_distance_mm <= APP_DISTANCE_OBSTACLE_MM);
}

static bool app_is_critical_obstacle(const app_sensor_data_t *s)
{
    return (!s->tof_timeout && s->tof_distance_mm > 0 && s->tof_distance_mm <= APP_DISTANCE_CRITICAL_MM);
}

static bool app_detect_stall(const app_sensor_data_t *s)
{
    int dleft = 0;
    int dright = 0;

    if (!s_have_prev_count) {
        s_prev_left_count = s->encoder_left;
        s_prev_right_count = s->encoder_right;
        s_have_prev_count = true;
        return false;
    }

    dleft = abs(s->encoder_left - s_prev_left_count);
    dright = abs(s->encoder_right - s_prev_right_count);
    s_prev_left_count = s->encoder_left;
    s_prev_right_count = s->encoder_right;

    if (dleft <= APP_STALL_DELTA_COUNT && dright <= APP_STALL_DELTA_COUNT) {
        s_stall_ticks++;
    } else {
        s_stall_ticks = 0;
    }

    return (s_stall_ticks >= APP_STALL_TICKS);
}

static void app_update_oled_and_log(const app_sensor_data_t *s)
{
    ESP_LOGI(TAG, "state=%d tof=%umm accel=(%.2f,%.2f,%.2f) gyro=(%.1f,%.1f,%.1f) enc=(%d,%d)",
             (int)s_state,
             (unsigned int)s->tof_distance_mm,
             (double)s->mpu.accel_x_g, (double)s->mpu.accel_y_g, (double)s->mpu.accel_z_g,
             (double)s->mpu.gyro_x_dps, (double)s->mpu.gyro_y_dps, (double)s->mpu.gyro_z_dps,
             s->encoder_left, s->encoder_right);
    app_oled_show(s, s_state);
}

static void app_run_state_machine(void)
{
    app_sensor_data_t s = {
        .encoder_ok = true,
        .motor_ok = true,
    };

    if (!app_collect_sensors(&s)) {
        s_state = APP_STATE_ERROR;
    }

    switch (s_state) {
    case APP_STATE_INIT:
        if (app_init_all() == ESP_OK) {
            s_state = APP_STATE_SELF_CHECK;
        } else {
            s_state = APP_STATE_ERROR;
        }
        break;

    case APP_STATE_SELF_CHECK:
        app_speak_state_once(APP_STATE_SELF_CHECK);
        if (driver_tof2000c_vl53l0x_is_initialized() &&
            driver_mpu6050_is_initialized() &&
            driver_oled_is_initialized() &&
            driver_tw_tts_is_initialized()) {
            s_state = APP_STATE_IDLE;
            ESP_LOGI(TAG, "Self check passed");
        } else {
            s_state = APP_STATE_ERROR;
        }
        break;

    case APP_STATE_IDLE:
        if (app_is_critical_obstacle(&s) || app_is_tilted(&s)) {
            s_state = APP_STATE_PROTECT;
        } else if (app_is_obstacle(&s)) {
            s_state = APP_STATE_AVOID;
        } else if (app_is_near_user(&s)) {
            s_state = APP_STATE_INTERACT;
        }
        break;

    case APP_STATE_INTERACT:
        app_speak_state_once(APP_STATE_INTERACT);
        if (app_is_tilted(&s)) {
            s_state = APP_STATE_PROTECT;
        } else if (app_is_obstacle(&s)) {
            s_state = APP_STATE_AVOID;
        } else {
            s_state = APP_STATE_MOVE;
        }
        break;

    case APP_STATE_MOVE: {
        int16_t left_speed = APP_MOVE_SPEED_DEFAULT;
        int16_t right_speed = APP_MOVE_SPEED_DEFAULT;

        if (app_is_tilted(&s)) {
            s_state = APP_STATE_PROTECT;
            break;
        }
        if (app_is_critical_obstacle(&s)) {
            s_state = APP_STATE_PROTECT;
            break;
        }
        if (app_is_obstacle(&s)) {
            s_state = APP_STATE_AVOID;
            break;
        }

        if (app_detect_stall(&s)) {
            ESP_LOGW(TAG, "Possible motor stall detected");
            s_state = APP_STATE_PROTECT;
            break;
        }

        if (s.tof_distance_mm <= APP_DISTANCE_NEAR_MM) {
            left_speed = APP_MOVE_SPEED_SLOW;
            right_speed = APP_MOVE_SPEED_SLOW;
        }

        (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, left_speed);
        (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, right_speed);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, left_speed);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, right_speed);
        break;
    }

    case APP_STATE_AVOID:
        app_speak_state_once(APP_STATE_AVOID);
        (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, -APP_TURN_SPEED);
        (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, APP_TURN_SPEED);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, -APP_TURN_SPEED);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, APP_TURN_SPEED);
        if (!app_is_obstacle(&s)) {
            s_state = APP_STATE_MOVE;
        }
        break;

    case APP_STATE_PROTECT:
        app_speak_state_once(APP_STATE_PROTECT);
        app_stop_motors();
        if (!app_is_tilted(&s) && !app_is_critical_obstacle(&s)) {
            s_state = APP_STATE_IDLE;
        }
        break;

    case APP_STATE_ERROR:
    default:
        app_speak_state_once(APP_STATE_ERROR);
        app_stop_motors();
        break;
    }

    app_update_oled_and_log(&s);
}

static void app_task(void *arg)
{
    (void)arg;
    app_state_t last_state = APP_STATE_INIT;

    ESP_LOGI(TAG, "boot");
    app_stop_motors();
    vTaskDelay(pdMS_TO_TICKS(APP_BOOT_WAIT_MS));

    while (1) {
        if (s_state != last_state) {
            ESP_LOGI(TAG, "state change %d -> %d", (int)last_state, (int)s_state);
            last_state = s_state;
        }

        if (s_state == APP_STATE_SELF_CHECK) {
            app_speak_state_once(APP_STATE_SELF_CHECK);
        }

        app_run_state_machine();

        if (s_state == APP_STATE_IDLE) {
            app_speak_state_once(APP_STATE_IDLE);
        }

        vTaskDelay(pdMS_TO_TICKS(APP_LOOP_PERIOD_MS));
    }
}

void app_main(void)
{
    service_pid_config_t pid_cfg = {
        .kp = 0.8f,
        .ki = 0.02f,
        .kd = 0.05f,
        .output_min = -255.0f,
        .output_max = 255.0f,
        .integral_max = 120.0f,
    };

    ESP_ERROR_CHECK(service_pid_init(&s_pid_left, &pid_cfg));
    ESP_ERROR_CHECK(service_pid_init(&s_pid_right, &pid_cfg));

    s_state = APP_STATE_INIT;
    s_last_spoken_state = APP_STATE_ERROR;

    xTaskCreate(app_task, "robot_agent_task", 6144, NULL, 5, NULL);
}