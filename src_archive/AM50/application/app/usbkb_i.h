/********************************************************************************************************
 * @file     usbkb_i.h 
 *
 * @brief    for TLSR chips
 *
 * @author	 public@telink-semi.com;
 * @date     Sep. 30, 2010
 *
 * @par      Copyright (c) Telink Semiconductor (Shanghai) Co., Ltd.
 *           All rights reserved.
 *           
 *			 The information contained herein is confidential and proprietary property of Telink 
 * 		     Semiconductor (Shanghai) Co., Ltd. and is available under the terms 
 *			 of Commercial License Agreement between Telink Semiconductor (Shanghai) 
 *			 Co., Ltd. and the licensee in separate contract or the terms described here-in. 
 *           This heading MUST NOT be removed from this file.
 *
 * 			 Licensees are granted free, non-transferable use of the information in this 
 *			 file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided. 
 *           
 *******************************************************************************************************/

#pragma once

#include "usbkb.h"
#include "../usbstd/usbhw.h"
#include "../usbstd/usbhw_i.h"
#include "../../common/config/user_config.h"

/** HID class report descriptor. This is a special descriptor constructed with values from the
 *  USBIF HID class specification to describe the reports and capabilities of the HID device. This
 *  descriptor is parsed by the host and its contents used to determine what data (and in what encoding)
 *  the device will send, and what it may be sent back from the host. Refer to the HID specification for
 *  more details on HID report descriptors.
 */
static const USB_Descriptor_HIDReport_Datatype_t keyboard_report_desc[] = {
    HID_DESCRIPTOR_KEYBOARD(KEYBOARD_REPORT_KEY_MAX),
};

static inline u8* usbkb_get_report_desc(void) {
	return (u8*) (keyboard_report_desc);
}

static inline u16 usbkb_get_report_desc_size(void) {
	return sizeof(keyboard_report_desc);
}


static const USB_Descriptor_HIDReport_Datatype_t Madao_report_desc[] = {
		0x05, 0x8c, // USAGE_PAGE (ST Page)
		0x09, 0x01, // USAGE (Demo Kit)
		0xa1, 0x01, // COLLECTION (Application)
		// The Input report
		0x09,0x01, // USAGE ID - Vendor defined
		0x15,0x00, // LOGICAL_MINIMUM (0)
		0x26,0x00, 0xFF, // LOGICAL_MAXIMUM (255)
		0x75,0x08, // REPORT_SIZE (8bit)
		0x95,0x40, // REPORT_COUNT (64Byte)
		0x81,0x02, // INPUT (Data,Var,Abs)
		// The Output report
		0x09,0x02, // USAGE ID - Vendor defined
		0x15,0x00, // LOGICAL_MINIMUM (0)
		0x26,0x00,0xFF, // LOGICAL_MAXIMUM (255)
		0x75,0x08, // REPORT_SIZE (8bit)
		0x95,0x40, // REPORT_COUNT (64Byte)
		0x91,0x02, // OUTPUT (Data,Var,Abs)
		0x09,0x04, // USAGE ID - Vendor defined
		0xb1,0x82, //FEATURE (Data,Var,Abs,Vol)
		0xc0 // END_COLLECTION
};

static inline u8* usbMadao_get_report_desc(void) {
	return (u8*)(Madao_report_desc);
	//return (u8*) (keyboard_report_desc);
}

static inline u16 usbMadao_get_report_desc_size(void) {
	return sizeof(Madao_report_desc);
	//return sizeof(keyboard_report_desc);
}
