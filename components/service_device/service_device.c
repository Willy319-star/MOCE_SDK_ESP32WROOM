#include "service_device.h"
#include "driver_led.h"
#include "driver_button.h"

void service_device_init(void)
{
    driver_led_init();
    driver_button_init();
}