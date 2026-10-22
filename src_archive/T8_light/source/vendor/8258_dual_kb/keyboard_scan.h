/*
 * @Descripttion:Function declarations and key-value definitions for the NKRO scanner.
 * Each time a keystroke changes, a 15-byte array is output.
 * Each bit of output data is mapped to a key value.  True means the button is pressed, and false means the button is not pressed. 
 * The key value represents the position of the key in the output array.
 * @version: 1.0
 * @Author: xdhuang
 * @Date: 2021-05-25 09:21:33
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 10:55:48
 * @Copyright: Hefei Madao Information Technology Co., Ltd
 */

#ifndef _KEYBOARD_SCAN_H_
#define _KEYBOARD_SCAN_H_

#include "common/types.h"
#include "AAA_public_config.h"

#define SCAN_KEYBOARD_SIZE 15

#define TOTAL_ROW 14	    // 行数*8=可容纳按键数=112>=87
#define TOTAL_COL 8         // 列固定为8

//-------------------- key value conversion define ------------------------------
#define VK_VALUE(n)  (1 << ((n - 1) & 0x07))
#define VK_ADDR(n)  ((n - 1) >> 3)
#define ARRAY_MS_TO_VK(buf,n) buf[VK_ADDR(n)] |= VK_VALUE(n)
//----------------------------------------------------------------------------

//extern u8 matix_window[TOTAL_ROW][TOTAL_COL];

typedef void (*scan_callback)(u8 *data, u8 size);
typedef void (*keyboard_up_callback)(u8);

struct keyboard_scan_manager
{
    void (*loop_scan)();
    void (*init)(scan_callback cb, keyboard_up_callback keyboard_up_cb);
    void (*set_wakeup)(u8 enable);
    void (*set_scan_data)(u8 *data, u8 num);/* 设置扫描值 */
};

struct keyboard_scan_manager *scan_manager_instance();


/* * * * * keycode * * * * */
#define MS_NULL     0
#define MS_Voice	1
#define MS_Fn		2
#define MS_BAT		3
#define MS_WHL		4
#define MS_MIC		5

#define MS_LCtrl	9
#define MS_LShift	10
#define MS_LAlt		11
#define MS_LWin		12
#define MS_RCtrl	13
#define MS_RShift	14
#define MS_RAlt		15
#define MS_RWin		16

#define MS_A		21
#define MS_B		22
#define MS_C		23
#define MS_D		24

#define MS_E		25
#define MS_F		26
#define MS_G		27
#define MS_H		28
#define MS_I		29
#define MS_J		30
#define MS_K		31
#define MS_L		32

#define MS_M		33
#define MS_N		34
#define MS_O		35
#define MS_P		36
#define MS_Q		37
#define MS_R		38
#define MS_S		39
#define MS_T		40

#define MS_U		41
#define MS_V		42
#define MS_W		43
#define MS_X		44
#define MS_Y		45
#define MS_Z		46
#define MS_1		47
#define MS_2		48

#define MS_3		49
#define MS_4		50
#define MS_5		51
#define MS_6		52
#define MS_7		53
#define MS_8		54
#define MS_9		55
#define MS_0		56

#define MS_Enter	57
#define MS_Esc		58
#define MS_Back		59
#define MS_Tab		60
#define MS_Space	61
#define MS_Jianhao	62
#define MS_Denghao	63
#define MS_LKuohao	64

#define MS_RKuohao	65
#define MS_Xiegang	66
#define MS_Europe1	67
#define MS_Fenhao	68
#define MS_Yinhao	69
#define MS_Dunhao	70
#define MS_Douhao	71
#define MS_Juhao	72

#define MS_Wenhao	73
#define MS_Caps		74
#define MS_F1		75
#define MS_F2		76
#define MS_F3		77
#define MS_F4		78
#define MS_F5		79
#define MS_F6		80

#define MS_F7		81
#define MS_F8		82
#define MS_F9		83
#define MS_F10		84
#define MS_F11		85
#define MS_F12		86
#define MS_Print	87
#define MS_Scroll	88

#define MS_Pause	89
#define MS_Insert	90
#define MS_Home		91
#define MS_Pgup		92
#define MS_Delete	93
#define MS_End		94
#define MS_Pgdn		95
#define MS_Right	96

#define MS_Left		97
#define MS_Down		98
#define MS_Up		99
#define MS_NumLock	100
#define MP_Chuhao	101
#define MP_Cenghao	102
#define MP_Jianhao	103
#define MP_Jiahao	104

#define MP_Enter	105
#define MP_1		106
#define MP_2		107
#define MP_3		108
#define MP_4		109
#define MP_5		110
#define MP_6		111
#define MP_7		112

#define MP_8		113
#define MP_9		114
#define MP_0		115
#define MP_Dian		116
#define MS_Europe2	117
#define MS_App		118
#define MS_Power	119
#define MS_Equal	120

#endif
