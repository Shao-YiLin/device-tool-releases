#ifndef CommonDefine_H
#define CommonDefine_H

#include<stdio.h>
#include<stdint.h>

#define DEBUG

#ifdef DEBUG
#define log_d  printf
#else
#define log_d  
#endif // DEBUG

#ifndef NULL
#define NULL 0
#endif // !NULL

#define true 1
#define false 0

#define USB_VID 0x2480
#define USB_PRODUCT_ID 0x6698
#define USB_INTERFACE 1

typedef uint8_t bool;

typedef  uint8_t u8;
typedef  uint32_t u32;

#define alignment_up(a, size)   ((a+size-1) & (~ (size-1)))
#define MBLUE_ALIGN(a)		alignment_up(a, 16)  

#endif // !1