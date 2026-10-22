/*
 * RGB_ins1_manager.h
 *
 *  Created on: 2022-7-8
 *      Author: xdhuang
 */

#ifndef RGB_INS1_MANAGER_H_
#define RGB_INS1_MANAGER_H_
#include "RGB_base.h"

typedef struct
{
	unsigned char rgb_red;
	unsigned char rgb_green;
	unsigned char rgb_blue;
}rgb_data;

typedef struct
{
	rgb_data colour_value;
	unsigned short onTime_ms;
	unsigned short offTime_ms;
	unsigned char  repeatCount;
	unsigned char  priority;
}single_cfg;

struct RGB_manager
{
	struct rgb_base_class rgb_handle;
	void (* init)(struct RGB_manager *mg);
	/**
	* @brief 用于设置呼吸灯灯效，呼吸灯效果为逐渐点亮再逐渐熄灭，再切换到下一个颜色依次轮询
	* priority：灯光的优先级，高优先级的灯光可以打断低优先级的灯光
	* count：灯效的执行次数，oxff为一直循环执行
	* on_time：灯光持续点亮的时间(ms)
	* off_time：灯光持续关闭的时间(ms)
	* change_time：灯光由关闭变为点亮或由点亮变为关闭过程的持续时间(ms)
	* colour：灯光的颜色，为RGB数组类型，最多可设七组
	* len：colour灯光颜色的个数，最大值为7，最小值为1
	*/
	void (* set_breathing_colour)(struct RGB_manager *mg, unsigned char priority, unsigned char count, unsigned short on_time, unsigned short off_time, unsigned short change_time, rgb_data *colour, unsigned char len);
	/**
	* @brief 用于设置流水灯灯效，流水灯效果为各个灯光直接轮询切换
	* priority：灯光的优先级，高优先级的灯光可以打断低优先级的灯光
	* count：灯效的执行次数，oxff为一直循环执行
	* on_time：灯光持续点亮的时间(ms)
	* change_time：灯光由当前颜色变换为下一个颜色过程的持续时间(ms)
	* colour：灯光的颜色，为RGB数组类型，最多可设七组
	* len：colour灯光颜色的个数，最大值为7，最小值为1
	*/
	void (* set_gradient_colour)(struct RGB_manager *mg, unsigned char priority, unsigned char count, unsigned short on_time, unsigned short change_time, rgb_data *colour, unsigned char len);
	/**
	* @brief 获取当前光效剩余的执行次数
	* return：count
	*/
	unsigned char (* get_repeatCount)(struct RGB_manager *mg);
	/**
	* @brief 设置单色灯光
	* cfg：配置参数详见“RGB_config_file.h”
	*/
	unsigned int (* set_single_colour)(struct RGB_manager *mg, single_cfg cfg);
	void (* loop)(struct RGB_manager *mg, unsigned char turnon_flag);
};

struct RGB_manager *rgb_front();
struct RGB_manager *rgb_back();

#endif /* RGB_INS_MANAGER_H_ */
