#define MAX_BREATH_COLOUR 8
enum GRADUAL_TYPE
{
	NO_GRADUAL,
	BREATH,
	GRADIENT
};

enum LIGHT_STATE
{
	STATE_OFF,
	STATE_ON,
	STATE_UP,	//该状态在gradual_flag == GRADIENT不使用
	STATE_DOWN,	//该状态在gradual_flag == GRADIENT不使用
	STATE_SWITCH,//该状态只有在gradual_flag == GRADIENT才使用
};

typedef struct
{
	unsigned char polarity;
	unsigned short pwm_channel;
	unsigned int gpio_point;
}gpio_and_pwm_channel;

typedef struct
{
	gpio_and_pwm_channel gpio_r;
	gpio_and_pwm_channel gpio_g;
	gpio_and_pwm_channel gpio_b;
}int_pwm_channel;

typedef struct
{
	short led_r;
	short led_g;
	short led_b;
}rgb_duty_cycle;

typedef struct{
	unsigned short onTime_ms;
	unsigned short offTime_ms;
	unsigned short changeTime_ms;
	rgb_duty_cycle on_colour[MAX_BREATH_COLOUR];	//能存储的颜色个数为MAX_BREATH_COLOUR - 1，最后一个数组成员作保留
	unsigned char colour_len;
	unsigned char gradual_flag;	//Whether to turn on the light gradient
	unsigned char repeatCount;	//0xff special for long on(offTime_ms=0)/long off(onTime_ms=0) must(gradual_flag == NO_GRADUAL)
	unsigned char priority;	//0x00 < 0x01 < 0x02 < 0x04 < 0x08 < 0x10 < 0x20 < 0x40 < 0x80
} rgb_cfg;

typedef struct{
	unsigned char isOn;
	unsigned char colour_index;
	unsigned char turnon_flag;
	int_pwm_channel gpio_param;
	rgb_duty_cycle current_colour;
	rgb_cfg rgb_cfg_info;
	unsigned int startTick[4];
}rgb_base_param;

struct rgb_base_class
{
	rgb_base_param rgb_param;
	void (*init)(struct rgb_base_class *rgb_handle, int_pwm_channel gpio_param);
	unsigned int (*set)(struct rgb_base_class *rgb_handle, rgb_cfg cfg);
	void (*proc)(struct rgb_base_class *rgb_handle, unsigned char turnon_flag);
};

void rgb_base_construct(struct rgb_base_class *lm);
