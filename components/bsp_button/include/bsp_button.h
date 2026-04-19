#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_BUTTON_EVENT_NONE = 0,
    BSP_BUTTON_EVENT_PRESS,
    BSP_BUTTON_EVENT_RELEASE,
    BSP_BUTTON_EVENT_SHORT_PRESS,
    BSP_BUTTON_EVENT_LONG_PRESS,
} bsp_button_event_t;

void bsp_button_init(void);

/* 周期调用，用于按键状态更新与事件检测 */
void bsp_button_process(void);

/* 获取并清除当前事件 */
bsp_button_event_t bsp_button_get_event(void);

/* 当前是否按下 */
bool bsp_button_is_pressed(void);

#ifdef __cplusplus
}
#endif