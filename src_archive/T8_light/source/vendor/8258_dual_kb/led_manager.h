#ifndef LED_MANAGER_H_
#define LED_MANAGER_H_

#include "tl_common.h"


//led management
typedef struct{
	unsigned short onTime_ms;
	unsigned short offTime_ms;

	unsigned char  repeatCount;  //0xff special for long on(offTime_ms=0)/long off(onTime_ms=0)
	unsigned char  priority;     //0x00 < 0x01 < 0x02 < 0x04 < 0x08 < 0x10 < 0x20 < 0x40 < 0x80
} led_cfg;

typedef struct {
	unsigned char  isOn;
	unsigned char  polar;
	unsigned char  repeatCount;
	unsigned char  priority;


	unsigned short onTime_ms;
	unsigned short offTime_ms;

	unsigned int gpio_led;
	unsigned int startTick;
}device_led_param;

struct led_manager{

    device_led_param led_param;

    void (*init)(struct led_manager *lm, u32 gpio, u8 polarity);
    int (*set)(struct led_manager *lm, led_cfg cfg);
    int (*proc)(struct led_manager *lm);
};

void led_manager_construct(struct led_manager *lm);

#endif
