#ifndef LED_MANAGER_H_
#define LED_MANAGER_H_

#include "tl_common.h"


//led management
typedef struct{
	unsigned short onTime_ms;
	unsigned short offTime_ms;

	unsigned char  repeatCount;  //0xff special for long on(offTime_ms=0)/long off(onTime_ms=0)
	unsigned char  priority;     //0x00 < 0x01 < 0x02 < 0x04 < 0x08 < 0x10 < 0x20 < 0x40 < 0x80
} gpio_cfg;

typedef struct {
	unsigned char  isOn;
	unsigned char  polar;
	unsigned char  repeatCount;
	unsigned char  priority;


	unsigned short onTime_ms;
	unsigned short offTime_ms;

	unsigned int gpio_port;
	unsigned int startTick;
}device_gpio_param;

struct gpio_manager{

    device_gpio_param gpio_param;

    void (*init)(struct gpio_manager *lm, u32 gpio, u8 polarity);
    int (*set)(struct gpio_manager *lm, gpio_cfg cfg);
    int (*proc)(struct gpio_manager *lm);
};

void gpio_manager_construct(struct gpio_manager *lm);

#endif
