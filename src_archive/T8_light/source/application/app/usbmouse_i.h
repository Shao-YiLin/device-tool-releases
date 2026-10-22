/********************************************************************************************************
 * @file	usbmouse_i.h
 *
 * @brief	This is the header file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	2020.06
 *
 * @par     Copyright (c) 2020, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *          
 *          Redistribution and use in source and binary forms, with or without
 *          modification, are permitted provided that the following conditions are met:
 *          
 *              1. Redistributions of source code must retain the above copyright
 *              notice, this list of conditions and the following disclaimer.
 *          
 *              2. Unless for usage inside a TELINK integrated circuit, redistributions 
 *              in binary form must reproduce the above copyright notice, this list of 
 *              conditions and the following disclaimer in the documentation and/or other
 *              materials provided with the distribution.
 *          
 *              3. Neither the name of TELINK, nor the names of its contributors may be 
 *              used to endorse or promote products derived from this software without 
 *              specific prior written permission.
 *          
 *              4. This software, with or without modification, must only be used with a
 *              TELINK integrated circuit. All other usages are subject to written permission
 *              from TELINK and different commercial license may apply.
 *
 *              5. Licensee shall be solely responsible for any claim to the extent arising out of or 
 *              relating to such deletion(s), modification(s) or alteration(s).
 *         
 *          THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *          ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *          WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *          DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
 *          DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *          (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *          LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *          ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *          (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *          SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *         
 *******************************************************************************************************/

#pragma once

#include "usbmouse.h"
#include "application/usbstd/usbhw.h"
#include "application/usbstd/usbhw_i.h"
#include "application/usbstd/usb.h"
#include "vendor/common/default_config.h"


/** HID class report descriptor. This is a special descriptor constructed with values from the
 *  USBIF HID class specification to describe the reports and capabilities of the HID device. This
 *  descriptor is parsed by the host and its contents used to determine what data (and in what encoding)
 *  the device will send, and what it may be sent back from the host. Refer to the HID specification for
 *  more details on HID report descriptors.
 */
/* debug note: */
/*
 0x85, 0x01, //Report ID (1) keyboard
 0x85, 0x02, //report ID 02 mouse
 */
//static const USB_Descriptor_HIDReport_Datatype_t mouse_report_desc[] = {
//#if 0
//
//	HID_RPT_USAGE_PAGE(8, 0x01), /* Generic Desktop */
//
//    HID_RPT_USAGE(8, 0x02)		, /* Mouse */
//
//    HID_RPT_COLLECTION(8, 0x01)		, /* Application */
//
//    HID_RPT_REPORT_ID(8, USB_HID_MOUSE)		, /*Report ID*/
//
//    HID_RPT_USAGE_PAGE(8, 0x09)		, /* Button */
//
//	// 1 is mouse left button,2 is mouse right button,3 is central buuton
//    HID_RPT_USAGE_MINIMUM(8, 0x01)		,
//
//    HID_RPT_USAGE_MAXIMUM(8, 0x05),
//
//    HID_RPT_LOGICAL_MINIMUM(8, 0x00)		,
//
//    HID_RPT_LOGICAL_MAXIMUM(8, 0x01),
//
//	HID_RPT_REPORT_SIZE(8, 0x01),
//
//    HID_RPT_REPORT_COUNT(8, 0x05), /* debug note: 3->5*/
//
//    HID_RPT_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
//
//	HID_RPT_REPORT_SIZE(8, 0x03), /* debug note: 5->3*/
//
//    HID_RPT_REPORT_COUNT(8, 0x01),
//
//    HID_RPT_INPUT(8, HID_IOF_CONSTANT),
//
//    HID_RPT_USAGE(8, 0x01), /* Pointer */
//
//    HID_RPT_COLLECTION(8, 0x00), /* Physical */
//
//    HID_RPT_USAGE_PAGE(8, 0x01),  /* Generic Desktop */
//
//    HID_RPT_USAGE(8, 0x30), /* Usage X */
//
//    HID_RPT_USAGE(8, 0x31), /* Usage Y */
//
//    HID_RPT_LOGICAL_MINIMUM(8, 0x81), //     LOGICAL_MINIMUM (-127)
//    HID_RPT_LOGICAL_MAXIMUM(8, 0x7f), //     LOGICAL_MAXIMUM (127)
//
//    HID_RPT_REPORT_SIZE(8, 0x08),
//    HID_RPT_REPORT_COUNT(8, 0x02),
//
//    HID_RPT_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),
//
//    HID_RPT_USAGE(8, 0x38),  /* Usage Wheel */
//    HID_RPT_LOGICAL_MINIMUM(8, 0x81),	 //LOGICAL_MINIMUM (-127)
//    HID_RPT_LOGICAL_MAXIMUM(8, 0x7f),	 //LOGICAL_MAXIMUM (127)
//    HID_RPT_REPORT_SIZE(8, 0x08),		 //REPORT_SIZE (8)
//    HID_RPT_REPORT_COUNT(8, 0x01),		//REPORT_COUNT (1)
//    HID_RPT_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),
//
//    HID_RPT_END_COLLECTION(0),
//    HID_RPT_END_COLLECTION(0),
//
//    HID_RPT_USAGE_PAGE(8, 0x0c),             //global, usage page (follow 1 bytes) consumer page
//    HID_RPT_USAGE(8, 0x01),                  //local,  usage ID 01  Consumer Control
//    HID_RPT_COLLECTION(8, 0x01),             //main, collection
//    HID_RPT_REPORT_ID(8, USB_HID_KB_MEDIA),  //Report ID
//    HID_RPT_REPORT_SIZE(8, 0x10),            //global, report size 16 bits
//    HID_RPT_REPORT_COUNT(8, 0x02),           //global, report count 2
//    HID_RPT_LOGICAL_MINIMUM(8, 0x01),        //global, min  0x01
//    HID_RPT_LOGICAL_MAXIMUM(16, 0x02ff),     //global, max  0x028c
//    HID_RPT_USAGE_MINIMUM(8, 0x01),          //local, min   0x01
//    HID_RPT_USAGE_MAXIMUM(16, 0x02ff),       //local, max    0x28c
//    HID_RPT_INPUT(8, HID_IOF_ABSOLUTE),      //main,  input data varible, absolute
//    HID_RPT_END_COLLECTION(0),               //main, end collection
//
//	HID_RPT_USAGE_PAGE(8, 0x01),             //gobal,  USAGE_PAGE 1 (Generic Desktop)
//	HID_RPT_USAGE(8, 0x80),                  //local, usage ID 0x80 system control
//	HID_RPT_COLLECTION(8, 0x01),             //main conllection
//    HID_RPT_REPORT_ID(8, USB_HID_KB_SYS),    //Report ID
//	HID_RPT_REPORT_SIZE(8, 0x01),            //global, report size 2
//	HID_RPT_REPORT_COUNT(8, 0x03),           //report count  1
//	HID_RPT_LOGICAL_MINIMUM(8, 0x00),        //global min 01
//    HID_RPT_LOGICAL_MAXIMUM(8, 0x01),        //gobal, max 3
//	HID_RPT_USAGE(8, 0x81),					//local usage ID 0x81 system power down
//    HID_RPT_USAGE(8, 0x82),                  //local usage ID 0x82 system sleep
//    HID_RPT_USAGE(8, 0x83),                  //local usage ID 0x83 system wakeup
//    HID_RPT_INPUT(8, HID_IOF_ABSOLUTE|HID_IOF_NO_PREFERRED_STATE|HID_IOF_NULLSTATE),
//    HID_RPT_REPORT_SIZE(8, 0x05),            //global report size 6
//	HID_RPT_REPORT_COUNT(8, 0x01),			//report count	1
//    HID_RPT_INPUT(8, HID_IOF_CONSTANT|HID_IOF_VARIABLE),
//    HID_RPT_END_COLLECTION(0),               //end of collection
//
//#else
//
//	0x05, 0x01, // USAGE_PAGE (Generic Desktop)
//
//	0x09, 0x02, // USAGE (Mouse)
//
//	0xa1, 0x01, // COLLECTION (Application)
//
//	0x85, USB_HID_MOUSE, //report ID 01
//
//	0x09, 0x01, //   USAGE (Pointer)
//
//	0xa1, 0x00, //   COLLECTION (Physical)
//
//	0x05, 0x09, //     USAGE_PAGE (Button)
//
//	// 1 is mouse left button,2 is mouse right button,3 is central buuton
//	0x19, 0x01, //     USAGE_MINIMUM (Button 1)
//
//	0x29, 0x05, //     USAGE_MAXIMUM (Button 5)
//
//	0x15, 0x00, //     LOGICAL_MINIMUM (0)
//
//	0x25, 0x01, //     LOGICAL_MAXIMUM (1)
//
//	0x95, 0x05, //     REPORT_COUNT (3)
//
//	0x75, 0x01, //     REPORT_SIZE (1)
//
//	0x81, 0x02, //     INPUT (Data,Var,Abs)
//
//	0x95, 0x01, //     REPORT_COUNT (1)
//
//	0x75, 0x03, //     REPORT_SIZE (3)
//
//	0x81, 0x01, //     INPUT (Cnst,Var,Abs)
//
//	0x05, 0x01, //     USAGE_PAGE (Generic Desktop)
//
//	0x09, 0x30, //     USAGE (X)
//
//	0x09, 0x31, //     USAGE (Y)
//
//	0x15, 0x81, //     LOGICAL_MINIMUM (-127)
//	0x25, 0x7f, //     LOGICAL_MAXIMUM (127)
//
//	0x75, 0x08, //     REPORT_SIZE (16)
//
//	0x95, 0x02, //     REPORT_COUNT (2)
//
//	0x81, 0x06, //     INPUT (Data,Var,Rel)
//
//	0x09, 0x38, //     USAGE (Wheel)
//	0x15, 0x81, //LOGICAL_MINIMUM (-127)
//	0x25, 0x7f, //LOGICAL_MAXIMUM (127)
//	0x75, 0x08, //REPORT_SIZE (16)
//	0x95, 0x01, //REPORT_COUNT (1)
//	0x81, 0x06, //INPUT (Data,Var,Rel)
//
//	0xc0, //   END_COLLECTION
//	0xc0, // END_COLLECTION
//
//#if (ONEKEY_WEB==0)
//	// begin of media key
//	0x05,0x0c,     //global, usage page (follow 1 bytes) consumer page
//	0x09,0x01,     //local,  usage ID 01  Consumer Control
//	0xA1,0x01,     //main, collection
//	0x85,USB_HID_KB_MEDIA,     //global, report ID 0x03
//	0x75,0x10,     //global, report size 16 bits
//	0x95,0x02,     //global, report count 2
//	0x15,0x01,     //global, min  0x01
//	0x26,0x9c,0x02,  //global, max  0x29c
//#if CHIP_EOP_ERROR
//	0x19,0x01,     //local, min   0x01
//	0x2a,0xff,0x02,  //local, max    0x2ff
//#else
//	0x19,0x01,     //local, min   0x01
//	0x2a,0x8c,0x02,  //local, max    0x28c
//#endif
//	0x81,0x00,     //main,  input data varible, absolute
//	0xc0,        //main, end collection
//
//	0x05,0x01,     //gobal,  USAGE_PAGE 1 (Generic Desktop)
//	0x09,0x80,     //local, usage ID 0x80 system control
//	0xa1,0x01,     //main conllection
//	0x85,USB_HID_KB_SYS,     //global report ID 0x4
//	0x75,0x02,     //global, report size 2
//	0x95,0x01,     //report count  1
//	0x15,0x01,     //global min 01
//	0x25,0x03,     //gobal, max 3
//	0x09,0x82,     //local usage ID 0x82 system sleep
//	0x09,0x81,     //local usage ID 0x81 system power down
//	0x09,0x83,     //local usage ID 0x83 system wakeup
//	0x81,0x60,     //main, input data, var, abs, No Prefer, NULL state
//	0x75,0x06,     //global report size 6
//	0x81,0x03,     //main input, constant, array
//	0xc0,        //end of collection
//	// end of media key
//#endif
//#endif
//    //need Jensen's help: report ID 5
//    HID_RPT_USAGE_PAGE(8, 0x01),     //global,  USAGE_PAGE 1 (Generic Desktop)
//	0x09,0x00,     //usage undefined
//	0xa1,0x01,     //main collection
//	0x85,0x05,     //global report ID 0x5
//	0x06,0x00,0xff,  //global usage page
//	0x09,0x01,     //local,  usage ID 01  Consumer Control
//	0x15,0x81,     //global min 81
//	0x25,0x7f,     //global, max 7f
//	0x75,0x08,     //global, report size 8
//	0x95,0x07,     //report count  7
//	0xb1,0x02,     //feature (data, var, abs)
//	HID_RPT_END_COLLECTION(0),         //main, end collection
//};

static const USB_Descriptor_HIDReport_Datatype_t mouse_report_desc[] =
{
#if 1//keyboard report in  25 bytes
		0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
		0x09, 0x06,                    // USAGE (Keyboard)
		0xA1, 0x01,                    // COLLECTION (Application)
		0x85, HID_REPORT_ID_KEYBOARD_INPUT,  // Report Id
		0x95, 0x68,                    //   REPORT_COUNT (6)
		0x75, 0x01,                    //   REPORT_SIZE (8)
		0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
		0x25, 0x01,                    //   LOGICAL_MAXIMUM (255)
		0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
		0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
		0x29, 0x67,                    //   USAGE_MAXIMUM (Keyboard Application)
		0x81, 0x02,                    //   INPUT (Data,Ary,Abs)
		0xc0,
#endif

#if 1//mouse report in  66 bytes

		0x05, 0x01,  // Usage Page (Generic Desktop)
		0x09, 0x02,  // Usage (Mouse)
		0xA1, 0x01,  // Collection (Application)
		0x85, HID_REPORT_ID_MOUSE_INPUT,  // Report Id

		0x09, 0x01,  //   Usage (Pointer)
		0xA1, 0x00,  //   Collection (Physical)
		0x05, 0x09,  // 	Usage Page (Buttons)
		0x19, 0x01,  // 	Usage Minimum (01) - Button 1
		0x29, 0x03,  // 	Usage Maximum (03) - Button 3
		0x15, 0x00,  // 	Logical Minimum (0)
		0x25, 0x01,  // 	Logical Maximum (1)
		0x75, 0x01,  // 	Report Size (1)
		0x95, 0x05,  // 	Report Count (3)
		0x81, 0x02,  // 	Input (Data, Variable, Absolute) - Button states
		0x75, 0x03,  // 	Report Size (5)
		0x95, 0x01,  // 	Report Count (1)
		0x81, 0x01,  // 	Input (Constant) - Padding or Reserved bits

		0x05,0x01,		//	Usage Page (Generic Desktop Control)
		0x09,0x30,	 // Usage (X)
		0x09,0x31,	 // Usage (Y)

		0x16,0x01,0x80, //	LOGICAL_MINIMUM(0)
		0x26,0xff,0x7f,
		0x75,0x10,	//	Report Size (16)
		0x95,0x02,	//	Report Count (2)
		0x81,0x06,	//	Input (Data, Variable, Relative)


	//	0x05,0x01,			//	Usage Page (Generic Desktop Control)  //for win7
		0x09,0x38,			//	Usage (Wheel)
		0x15,0x81,			//	Logical Minimum (-4)
		0x25,0x7F,			//	Logical Maximum (3)
		0x75,0x08,			//	Report Size (3)
		0x95,0x01,			//	Report Count (1)
		0x81,0x06,			//	Input (Data, Variable, Relative)

		0xC0,		 //   End Collection
		0xC0,		 // End Collection

#endif


#if 1//CONSUMER_REPORT_BYTE_AAA  25bytes

	//consumer report in
	0x05, 0x0C,   // Usage Page (Consumer)
	0x09, 0x01,   // Usage (Consumer Control)
	0xA1, 0x01,   // Collection (Application)
	0x85, HID_REPORT_ID_CONSUME_CONTROL_INPUT,   //     Report Id
	0x75,0x10,     //global, report size 16 bits
	0x95,0x01,     //global, report count 1
	0x15,0x01,     //global, min  0x01
	0x26,0x8c,0x02,  //global, max  0x28c
	0x19,0x01,     //local, min   0x01
	0x2a,0x8c,0x02,  //local, max    0x28c
	0x81,0x00,     //main,  input data varible, absolute
	0xc0,        //main, end collection

#endif
#if 0//SYSTEM_KEY_FUN_ENABLE_AAA  29 bytes

	0x05,0x01, 		//Usage Page (Generic Desktop Control)
	0x09,0x80,			//Usage (SYSTEM CONTROL)
	0xA1,0x01,	//Collection (Application)

	0x85,HID_REPORT_ID_SYSTEM_INPUT,		//Report ID (ACPI)

	0x25,0x01,			//	Logical Maximum (1)
	0x15,0x00,			//	Logical Minimum (0)
	0x75,0x01,			//	Report Size


	0x09,0x82, 	//	USAGE SYSTEM SLEEP
	0x09,0x81,	//	USAGE SYSTEM POWER DOWN
	0x09,0x83, 	//	USAGE SYSTEM WAKE UP

	0x95,0x03,			//	REPORT_COUNT (03H)
	0x81,0x02,			//	INPUT (DATA, VAR)
	0x95,0x05,			//	Report Count (05)

	0x81,0x01,			//	Input (CONSTANT)
	0xC0,				//	END COLLECTION

#endif
#if 0  //24
	0x05,0x01,    // USAGE_PAGE (Generic Desktop)
	0x09,0x00,	  //usage undefined
	0xa1,0x01,	  //main collection
	0x85,HID_REPORT_ID_USER_FEATURE,	  //global report ID 0x5
	0x06,0x00,0xff,	//global usage page
	0x09,0x01,	  //local,	usage ID 01  Consumer Control
	0x15,0x81,	  //global min 81
	0x25,0x7f,	  //global, max 7f
	0x75,0x08,	  //global, report size 8
	0x95,0x07,	  //report count  7
	0xb1,0x02,	  //feature (data, var, abs)
	0xc0, //main, end collection
#endif
    //need Jensen's help: report ID 5
#if 0 //24

		0x05,0x01,	  // USAGE_PAGE (Generic Desktop)
		0x09,0x00,	  //usage undefined
		0xa1,0x01,	  //main collection
		0x85,HID_REPORT_ID_SPP_INPUT,	  //global report ID 0x5
		0x06,0x00,0xff, //global usage page
		0x09,0x01,	  //local,	usage ID 01  Consumer Control
		0x15,0x81,	  //global min 81
		0x25,0x7f,	  //global, max 7f
		0x75,0x08,	  //global, report size 8
		0x95,0x07,	  //report count  7
		0x81,0x02,	  //feature (data, var, abs)
		0xc0, //main, end collection

#endif
};

static inline u8* usbmouse_get_report_desc(void) {
	return (u8*) (mouse_report_desc);
}

static inline u16 usbmouse_get_report_desc_size(void) {
	return sizeof(mouse_report_desc);
}

