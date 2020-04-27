/**
 * usb_cdc_defines USB CDC Type Definitions
 *
*/
#ifndef __HID_H
#define __HID_H

#include "libmaple_types.h"


#define USB_CLASS_HID		0x03



#define HID_REQ_GET_REPORT 				0x01
#define HID_REQ_GET_IDLE				0x02
#define HID_REQ_GET_PROTOCOL			0x03
#define HID_REQ_SET_REPORT				0x09
#define HID_REQ_SET_IDLE				0x0A
#define HID_REQ_SET_PROTOCOL			0x0B



/* Table 15: Class-Specific Descriptor Header Format */
typedef struct usb_hid_descriptor {
	  uint8_t bLength;
	  uint8_t bDescriptorType;
	  uint16_t bcdHID;
	  uint8_t bCountryCode;
	  uint8_t bNumDescriptors;
	  uint8_t bDescriptorType0;
	  uint16_t wItemLength;
} __attribute((packed)) usb_hid_descriptor;


extern const uint8_t hid_report[];
extern const int hid_report_size;

#endif
