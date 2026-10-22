/*
 * @Descripttion: 
 * @version: 
 * @Author: xdhuang
 * @Date: 2021-05-25 09:21:22
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 10:14:00
 * @Copyright: Hefei Madao Information Technology Co., Ltd
 */
#include "keyboard_scan.h"
//#include "drivers.h"
#include "../../common/string.h"

static scan_callback scan_cb = NULL;
static keyboard_up_callback keyboard_up = NULL;

static u8 arrKeyboardBuf[SCAN_KEYBOARD_SIZE];

static struct
{
    u8 state;
    u8 data[TOTAL_ROW];
}current_scanbuf;

// 通过更改此处修改按键映射
#define KEYMAP(\
	    k51,   k72, k73, k74,   k75, k76, k77, k80,   k81, k82, k83, k84, k85,   k86, k87, k90,                      \
	    k65, k36, k37, k40, k41, k42, k43, k44, k45, k46, k47, k55, k56,  k52,   k91, k92, k93,  ka3, ka4, ka5, ka6, \
	    k53 , k24, k32, k10, k25, k27, k34, k30, k14, k22, k23, k57, k60, k61,   k94, k95, k96,  kb7, kc0, kc1, ka7, \
	    k71  , k04, k26, k07, k11, k12, k13, k15, k16, k17, k63, k64,     k50,                   kb4, kb5, kb6,      \
	    kd1    , k35, k33, k06, k31, k05, k21, k20, k66, k67, k70,        kd5,        ka2,       kb1, kb2, kb3, kb0, \
	    kd0,  kd3,  kd2,               k54,             kd6,  kc7,  kc5,  kd4,   ka0, ka1, k97,     kc2,   kc3       \
)\
{\
        { MS_NULL, MS_NULL, MS_NULL, MS_NULL, k04, k05, k06, k07 }, \
        { k10, k11, k12, k13, k14, k15, k16, k17 }, \
        { k20, k21, k22, k23, k24, k25, k26, k27 }, \
        { k30, k31, k32, k33, k34, k35, k36, k37 }, \
        { k40, k41, k42, k43, k44, k45, k46, k47 }, \
        { k50, k51, k52, k53, k54, k55, k56, k57 }, \
        { k60, k61, MS_NULL, k63, k64, k65, k66, k67 }, \
        { k70, k71, k72, k73, k74, k75, k76, k77 }, \
        { k80, k81, k82, k83, k84, k85, k86, k87 }, \
        { k90, k91, k92, k93, k94, k95, k96, k97 }, \
        { ka0, ka1, ka2, ka3, ka4, ka5, ka6, ka7 }, \
        { kb0, kb1, kb2, kb3, kb4, kb5, kb6, kb7 }, \
        { kc0, kc1, kc2, kc3, MS_NULL, kc5, MS_NULL, kc7 }, \
        { kd0, kd1, kd2, kd3, kd4, kd5, kd6, MS_NULL }, \
}
_attribute_data_retention_user static u8 matix_window[TOTAL_ROW][TOTAL_COL] = KEYMAP(\
		MS_Esc,    MS_F1, MS_F2, MS_F3, MS_F4,    MS_F5, MS_F6, MS_F7, MS_F8,     MS_F9, MS_F10, MS_F11, MS_F12,  MS_Print , MS_Scroll ,MS_Pause,  \
		MS_Dunhao,	MS_1, MS_2, MS_3, MS_4, MS_5, MS_6, MS_7, MS_8, MS_9, MS_0, MS_Jianhao, MS_Denghao, MS_Back,  MS_Insert, MS_Home   , MS_Pgup,  MS_NumLock, MP_Chuhao, MP_Cenghao, MP_Jianhao,\
		MS_Tab , MS_Q, MS_W, MS_E, MS_R, MS_T, MS_Y, MS_U, MS_I, MS_O, MS_P, MS_LKuohao, MS_RKuohao, MS_Xiegang,  MS_Delete, MS_End    , MS_Pgdn,  MP_7,       MP_8,      MP_9,       MP_Jiahao, \
		MS_Caps    , MS_A, MS_S, MS_D, MS_F, MS_G, MS_H, MS_J, MS_K, MS_L, MS_Fenhao, MS_Yinhao,       MS_Enter,                                   MP_4,       MP_5,      MP_6,                  \
		MS_LShift     , MS_Z, MS_X, MS_C, MS_V, MS_B, MS_N, MS_M, MS_Douhao, MS_Juhao, MS_Wenhao,     MS_RShift,             MS_Up     ,           MP_1,       MP_2,      MP_3,       MP_Enter,  \
		MS_LCtrl, MS_LWin, MS_LAlt,                 MS_Space,                MS_RAlt, MS_Fn, MS_Voice, MS_RCtrl,  MS_Left  , MS_Down   , MS_Right,       MP_0,            MP_Dian                \
);

// 设置扫描值
static void _set_scan_data(u8 *data, u8 num)
{
	memcpy(current_scanbuf.data,data,num);
	current_scanbuf.state = 1;
}

// 设置休眠唤醒机制
static void set_suspend_wakeup_level(u8 en)
{

	if(gpio_read(GPIO_SN32_IRQ))
		cpu_set_gpio_wakeup(GPIO_SN32_IRQ, 0, en);
	else cpu_set_gpio_wakeup(GPIO_SN32_IRQ, 1, en);

}

// 保存按键键值到数组arrKeyboardBuf
static void _save_keycode_result(u8 key_code)
{
	if(key_code == 0)
		return;
	ARRAY_MS_TO_VK(arrKeyboardBuf,key_code);
}


static u8 keboard_buf_empty[TOTAL_ROW] = {0};

static void matrix_scan_loop()
{
	u8 row,col;
	// 如果有按键事件来临
	if(current_scanbuf.state)
	{
		current_scanbuf.state = 0;
		memset(arrKeyboardBuf, 0x00, SCAN_KEYBOARD_SIZE);// 先清零
		if (!memcmp(current_scanbuf.data, keboard_buf_empty, TOTAL_ROW))
		{
//			printf("[2]KeyUp\n");
			if (keyboard_up != NULL)
			{
				keyboard_up(1);// 表明所有的按键弹起
			}
		}
		else
		{
//			printf("[2]KeyDown\n");
			if (keyboard_up != NULL)
			{
				keyboard_up(0);// 表明有按键按下
			}
		}
		for (row = 0; row < TOTAL_ROW; row++)
		{
			for (col = 0; col < TOTAL_COL; col++)
			{
				if (current_scanbuf.data[row] & (1 << col))// 如果按下
				{
					  _save_keycode_result(matix_window[row][col]);// 保存
				}
			}
		}
		memset(current_scanbuf.data, 0, TOTAL_ROW);// 按键清0
		// 通过回调函数返回数据
		if (NULL != scan_cb)
		{
			scan_cb(arrKeyboardBuf, sizeof(arrKeyboardBuf));
		}
	}
}

static void _init(scan_callback cb, keyboard_up_callback keyboard_up_cb)
{
	scan_cb = cb;
	keyboard_up = keyboard_up_cb;
    current_scanbuf.state = 0;
	memset(current_scanbuf.data, 0, TOTAL_ROW);
}

// 设置唤醒源（中断）
static void _set_wakeup(u8 enable)
{
	set_suspend_wakeup_level(enable);
}

static struct keyboard_scan_manager *instance;
static struct keyboard_scan_manager _key_data =
{
	.loop_scan = matrix_scan_loop,
	.init = _init,
	.set_wakeup = _set_wakeup,
	.set_scan_data = _set_scan_data
};

struct keyboard_scan_manager *scan_manager_instance()
{
	if (NULL == instance)
		instance = &_key_data;
	return instance;
}

