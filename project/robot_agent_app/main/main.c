#include <stdio.h>
#include <string.h>
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
#include "driver_servo.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"
#include "service_device.h"
#include "service_pid.h"
#include "service_wifi.h"

static const char *TAG = "robot_agent_app";

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
    bool tof_ok;
    bool mpu_ok;
    bool encoder_ok;
    bool motor_ok;
    bool tts_ok;
    bool oled_ok;
} app_health_t;

typedef struct {
    float distance_mm;
    float pitch_deg;
    float roll_deg;
    int left_encoder;
    int right_encoder;
    bool user_near;
    bool obstacle_close;
    bool tilted;
    bool shaken;
    bool motor_stall;
} app_sensor_fusion_t;

static service_pid_t s_left_pid;
static service_pid_t s_right_pid;
static app_state_t s_state = APP_STATE_INIT;
static app_state_t s_last_announced_state = APP_STATE_ERROR;
static app_health_t s_health;
static bool s_tts_enabled = false;
static bool s_oled_enabled = false;
static uint32_t s_loop_count = 0;
static int s_last_left_encoder = 0;
static int s_last_right_encoder = 0;

static const char *app_state_name(app_state_t state)
{
    switch (state) {
    case APP_STATE_INIT: return "INIT";
    case APP_STATE_SELF_CHECK: return "SELF_CHECK";
    case APP_STATE_IDLE: return "IDLE";
    case APP_STATE_INTERACT: return "INTERACT";
    case APP_STATE_MOVE: return "MOVE";
    case APP_STATE_AVOID: return "AVOID";
    case APP_STATE_PROTECT: return "PROTECT";
    case APP_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

static void app_speak_once(app_state_t state, const char *text)
{
    if (!s_tts_enabled || s_last_announced_state == state) {
        return;
    }
    if (driver_tw_tts_speak(text) == ESP_OK) {
        s_last_announced_state = state;
    }
}

static void app_set_motors_stop(bool brake)
{
    if (brake) {
        (void)driver_motor_brake(DRIVER_MOTOR_LEFT);
        (void)driver_motor_brake(DRIVER_MOTOR_RIGHT);
    } else {
        (void)driver_motor_coast(DRIVER_MOTOR_LEFT);
        (void)driver_motor_coast(DRIVER_MOTOR_RIGHT);
    }
}

static void app_update_oled(const app_sensor_fusion_t *fusion)
{
    if (!s_oled_enabled) {
        return;
    }

    char line0[32];
    char line1[32];
    char line2[32];
    char line3[32];

    snprintf(line0, sizeof(line0), "S:%s", app_state_name(s_state));
    snprintf(line1, sizeof(line1), "D:%4.0fmm P:%5.1f", fusion->distance_mm, fusion->pitch_deg);
    snprintf(line2, sizeof(line2), "R:%5.1f L:%d", fusion->roll_deg, fusion->left_encoder);
    snprintf(line3, sizeof(line3), "R:%d %s", fusion->right_encoder, fusion->tilted ? "TILT" : "OK");

    driver_oled_clear();
    driver_oled_draw_string(0, 0, line0, true);
    driver_oled_draw_string(0, 16, line1, true);
    driver_oled_draw_string(0, 32, line2, true);
    driver_oled_draw_string(0, 48, line3, true);
    (void)driver_oled_flush();
}

static bool app_init_oled(void)
{
    if (driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306) != ESP_OK) {
        ESP_LOGW(TAG, "OLED init failed");
        return false;
    }
    s_oled_enabled = true;
    return true;
}

static bool app_init_tof(void)
{
    if (driver_tof2000c_vl53l0x_init_default() != ESP_OK) {
        ESP_LOGW(TAG, "TOF init failed");
        return false;
    }
    return true;
}

static bool app_init_mpu(void)
{
    if (driver_mpu6050_init_default() != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 init failed");
        return false;
    }
    return true;
}

static bool app_init_tts(void)
{
    if (driver_tw_tts_init_default() != ESP_OK) {
        ESP_LOGW(TAG, "TTS init failed");
        return false;
    }
    s_tts_enabled = true;
    return true;
}

static bool app_init_motors(void)
{
    driver_motor_config_t cfg = {
        .ppr = DRIVER_MOTOR_PPR_DEFAULT,
    };

    esp_err_t err = driver_motor_init(DRIVER_MOTOR_LEFT, &cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Left motor init failed: %s", esp_err_to_name(err));
        return false;
    }
    err = driver_motor_init(DRIVER_MOTOR_RIGHT, &cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Right motor init failed: %s", esp_err_to_name(err));
        return false;
    }
    app_set_motors_stop(true);
    return true;
}

static bool app_init_pid(void)
{
    service_pid_config_t pid_cfg = {
        .kp = 1.2f,
        .ki = 0.05f,
        .kd = 0.02f,
        .output_min = -100.0f,
        .output_max = 100.0f,
        .integral_max = 80.0f,
    };

    if (service_pid_init(&s_left_pid, &pid_cfg) != ESP_OK) {
        return false;
    }
    if (service_pid_init(&s_right_pid, &pid_cfg) != ESP_OK) {
        return false;
    }
    return true;
}

static void app_collect_fusion(app_sensor_fusion_t *fusion)
{
    driver_tof2000c_vl53l0x_result_t tof = {0};
    driver_mpu6050_data_t mpu = {0};
    int left_count = 0;
    int right_count = 0;

    fusion->distance_mm = 9999.0f;
    fusion->pitch_deg = 0.0f;
    fusion->roll_deg = 0.0f;
    fusion->left_encoder = s_last_left_encoder;
    fusion->right_encoder = s_last_right_encoder;
    fusion->user_near = false;
    fusion->obstacle_close = false;
    fusion->tilted = false;
    fusion->shaken = false;
    fusion->motor_stall = false;

    if (driver_tof2000c_vl53l0x_read_single(&tof) == ESP_OK && !tof.timeout) {
        fusion->distance_mm = (float)tof.distance_mm;
        fusion->user_near = (tof.distance_mm > 0 && tof.distance_mm < 700);
        fusion->obstacle_close = (tof.distance_mm > 0 && tof.distance_mm < 200);
    }

    if (driver_mpu6050_read(&mpu) == ESP_OK) {
        fusion->pitch_deg = atan2f(mpu.accel_x_g, sqrtf(mpu.accel_y_g * mpu.accel_y_g + mpu.accel_z_g * mpu.accel_z_g)) * 57.29578f;
        fusion->roll_deg = atan2f(mpu.accel_y_g, mpu.accel_z_g) * 57.29578f;
        fusion->tilted = fabsf(fusion->pitch_deg) > 35.0f || fabsf(fusion->roll_deg) > 35.0f;
        fusion->shaken = fabsf(mpu.gyro_x_dps) > 150.0f || fabsf(mpu.gyro_y_dps) > 150.0f || fabsf(mpu.gyro_z_dps) > 150.0f;
    }

    if (driver_encoder_get_count(DRIVER_ENCODER_LEFT, &left_count) == ESP_OK &&
        driver_encoder_get_count(DRIVER_ENCODER_RIGHT, &right_count) == ESP_OK) {
        fusion->left_encoder = left_count;
        fusion->right_encoder = right_count;
        fusion->motor_stall = (abs(left_count - s_last_left_encoder) < 2) && (abs(right_count - s_last_right_encoder) < 2);
        s_last_left_encoder = left_count;
        s_last_right_encoder = right_count;
    }
}

static void app_update_state(const app_sensor_fusion_t *fusion)
{
    switch (s_state) {
    case APP_STATE_INIT:
        s_state = APP_STATE_SELF_CHECK;
        break;

    case APP_STATE_SELF_CHECK:
        if (s_health.tof_ok && s_health.mpu_ok && s_health.encoder_ok && s_health.motor_ok && s_health.tts_ok) {
            s_state = APP_STATE_IDLE;
        } else {
            s_state = APP_STATE_ERROR;
        }
        break;

    case APP_STATE_IDLE:
        if (fusion->tilted || fusion->shaken) {
            s_state = APP_STATE_PROTECT;
        } else if (fusion->obstacle_close) {
            s_state = APP_STATE_AVOID;
        } else if (fusion->user_near) {
            s_state = APP_STATE_INTERACT;
        } else if (s_loop_count % 40 == 0) {
            s_state = APP_STATE_MOVE;
        }
        break;

    case APP_STATE_INTERACT:
        if (fusion->tilted || fusion->shaken) {
            s_state = APP_STATE_PROTECT;
        } else if (fusion->obstacle_close) {
            s_state = APP_STATE_AVOID;
        } else if (!fusion->user_near) {
            s_state = APP_STATE_IDLE;
        }
        break;

    case APP_STATE_MOVE:
        if (fusion->tilted || fusion->shaken || fusion->motor_stall) {
            s_state = APP_STATE_PROTECT;
        } else if (fusion->obstacle_close) {
            s_state = APP_STATE_AVOID;
        } else if (!fusion->user_near && (s_loop_count % 25 == 0)) {
            s_state = APP_STATE_IDLE;
        }
        break;

    case APP_STATE_AVOID:
        if (fusion->tilted || fusion->shaken) {
            s_state = APP_STATE_PROTECT;
        } else if (!fusion->obstacle_close && fusion->user_near) {
            s_state = APP_STATE_INTERACT;
        } else if (!fusion->obstacle_close) {
            s_state = APP_STATE_MOVE;
        }
        break;

    case APP_STATE_PROTECT:
        if (!fusion->tilted && !fusion->shaken && !fusion->motor_stall) {
            s_state = APP_STATE_IDLE;
        }
        break;

    case APP_STATE_ERROR:
    default:
        break;
    }
}

static void app_apply_state(const app_sensor_fusion_t *fusion)
{
    switch (s_state) {
    case APP_STATE_INIT:
        app_set_motors_stop(true);
        driver_led_set(1);
        break;

    case APP_STATE_SELF_CHECK:
        app_set_motors_stop(true);
        driver_led_set_brightness(50);
        break;

    case APP_STATE_IDLE:
        app_set_motors_stop(false);
        driver_led_set_brightness(10);
        break;

    case APP_STATE_INTERACT:
        app_set_motors_stop(false);
        driver_led_set_brightness(30);
        if (s_last_announced_state != APP_STATE_INTERACT) {
            app_speak_once(APP_STATE_INTERACT, "欢迎使用桌面伴侣");
        }
        break;

    case APP_STATE_MOVE: {
        int16_t base_speed = 35;
        int16_t left_speed = base_speed;
        int16_t right_speed = base_speed;
        if (fusion->user_near) {
            base_speed = 25;
            left_speed = base_speed;
            right_speed = base_speed;
        }
        if (fusion->motor_stall) {
            app_set_motors_stop(true);
            break;
        }
        (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, left_speed);
        (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, right_speed);
        driver_led_set_brightness(80);
        break;
    }

    case APP_STATE_AVOID:
        (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, -20);
        (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, -20);
        driver_led_set_brightness(60);
        if (s_last_announced_state != APP_STATE_AVOID) {
            app_speak_once(APP_STATE_AVOID, "前方有障碍，正在避障");
        }
        break;

    case APP_STATE_PROTECT:
        app_set_motors_stop(true);
        driver_led_set(0);
        if (s_last_announced_state != APP_STATE_PROTECT) {
            app_speak_once(APP_STATE_PROTECT, "设备状态异常，已停止运动");
        }
        break;

    case APP_STATE_ERROR:
    default:
        app_set_motors_stop(true);
        driver_led_set(0);
        if (s_last_announced_state != APP_STATE_ERROR) {
            app_speak_once(APP_STATE_ERROR, "系统故障，请复位");
        }
        break;
    }
}

static void app_init_peripherals(void)
{
    service_device_init();
    driver_led_init();
    driver_button_init();

    s_health.tof_ok = app_init_tof();
    s_health.mpu_ok = app_init_mpu();
    s_health.oled_ok = app_init_oled();
    s_health.tts_ok = app_init_tts();
    s_health.motor_ok = app_init_motors();
    s_health.encoder_ok = (driver_encoder_init(DRIVER_ENCODER_LEFT) == ESP_OK) &&
                          (driver_encoder_init(DRIVER_ENCODER_RIGHT) == ESP_OK);
    s_health.encoder_ok = s_health.encoder_ok && (driver_encoder_reset(DRIVER_ENCODER_LEFT) == ESP_OK) &&
                          (driver_encoder_reset(DRIVER_ENCODER_RIGHT) == ESP_OK);
    (void)driver_servo_init();
    s_health.encoder_ok = s_health.encoder_ok && (driver_servo_set_angle(DRIVER_SERVO_0, 90) == ESP_OK);
    s_health.encoder_ok = s_health.encoder_ok && (driver_servo_set_angle(DRIVER_SERVO_1, 90) == ESP_OK);

    ESP_LOGI(TAG, "Init result: TOF=%d MPU=%d OLED=%d TTS=%d MOTOR=%d ENC=%d",
             s_health.tof_ok, s_health.mpu_ok, s_health.oled_ok, s_health.tts_ok,
             s_health.motor_ok, s_health.encoder_ok);
}

static void app_state_voice_and_log(void)
{
    if (s_last_announced_state == s_state) {
        return;
    }

    ESP_LOGI(TAG, "State -> %s", app_state_name(s_state));

    switch (s_state) {
    case APP_STATE_INIT:
        app_speak_once(APP_STATE_INIT, "系统初始化中");
        break;
    case APP_STATE_SELF_CHECK:
        app_speak_once(APP_STATE_SELF_CHECK, "系统自检中");
        break;
    case APP_STATE_IDLE:
        app_speak_once(APP_STATE_IDLE, "进入待机");
        break;
    case APP_STATE_INTERACT:
        app_speak_once(APP_STATE_INTERACT, "检测到用户靠近");
        break;
    case APP_STATE_MOVE:
        app_speak_once(APP_STATE_MOVE, "开始移动");
        break;
    case APP_STATE_AVOID:
        app_speak_once(APP_STATE_AVOID, "正在避障");
        break;
    case APP_STATE_PROTECT:
        app_speak_once(APP_STATE_PROTECT, "进入保护状态");
        break;
    case APP_STATE_ERROR:
    default:
        app_speak_once(APP_STATE_ERROR, "进入故障状态");
        break;
    }
}

void app_main(void)
{
    app_init_peripherals();
    s_state = APP_STATE_SELF_CHECK;
    s_last_announced_state = APP_STATE_ERROR;

    while (1) {
        driver_button_process();

        app_sensor_fusion_t fusion;
        app_collect_fusion(&fusion);

        if (s_state == APP_STATE_SELF_CHECK) {
            app_state_voice_and_log();
            app_update_state(&fusion);
        } else {
            app_update_state(&fusion);
        }

        app_state_voice_and_log();
        app_apply_state(&fusion);
        app_update_oled(&fusion);

        if (driver_button_get_event() == DRIVER_BUTTON_EVENT_LONG_PRESS) {
            s_state = APP_STATE_MOVE;
            s_last_announced_state = APP_STATE_ERROR;
        }

        s_loop_count++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}