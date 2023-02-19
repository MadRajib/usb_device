#ifdef  _DEFAULT_SOURCE 
#undef  _DEFAULT_SOURCE
#endif
#define _DEFAULT_SOURCE /* for endian.h */

#include <errno.h>
#include <fcntl.h>
#include <endian.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <linux/usb/functionfs.h>
#include "usb.h"

/******************** Little Endian Handling ********************************/

/*
 * cpu_to_le16/32 are used when initializing structures, a context where a
 * function call is not allowed. To solve this, we code cpu_to_le16/32 in a way
 * that allows them to be used when initializing structures.
 */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(x)  (x)
#define cpu_to_le32(x)  (x)
#else
#define cpu_to_le16(x)  ((((x) >> 8) & 0xffu) | (((x) & 0xffu) << 8))
#define cpu_to_le32(x)  \
	((((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >>  8) | \
	(((x) & 0x0000ff00u) <<  8) | (((x) & 0x000000ffu) << 24))
#endif

#define le32_to_cpu(x)  le32toh(x)
#define le16_to_cpu(x)  le16toh(x)
/******************** Descriptors and Strings *******************************/


static const struct {
	struct usb_functionfs_descs_head_v2 header;
	__le32 fs_count;
	__le32 hs_count;
	struct {
		struct usb_interface_descriptor intf;
		struct usb_endpoint_descriptor_no_audio sink;
		struct usb_endpoint_descriptor_no_audio source;
	} __attribute__((packed)) fs_descs, hs_descs;
} __attribute__((packed)) descriptors = {
	.header = {
		.magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
		.flags = cpu_to_le32(FUNCTIONFS_HAS_FS_DESC |
				     FUNCTIONFS_HAS_HS_DESC ),
		.length = cpu_to_le32(sizeof descriptors),
	},
	.fs_count = cpu_to_le32(3),
	.fs_descs = {
		.intf = {
			.bLength = sizeof descriptors.fs_descs.intf,
			.bDescriptorType = USB_DT_INTERFACE,
			.bNumEndpoints = 2,
			.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
			.iInterface = 1,
		},
		.sink = {
			.bLength = sizeof descriptors.fs_descs.sink,
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 1 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			/* .wMaxPacketSize = autoconfiguration (kernel) */
		},
		.source = {
			.bLength = sizeof descriptors.fs_descs.source,
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 2 | USB_DIR_OUT,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			/* .wMaxPacketSize = autoconfiguration (kernel) */
		},
	},
	.hs_count = cpu_to_le32(3),
	.hs_descs = {
		.intf = {
			.bLength = sizeof descriptors.fs_descs.intf,
			.bDescriptorType = USB_DT_INTERFACE,
			.bNumEndpoints = 2,
			.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
			.iInterface = 1,
		},
		.sink = {
			.bLength = sizeof descriptors.hs_descs.sink,
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 1 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			.wMaxPacketSize = cpu_to_le16(512),
		},
		.source = {
			.bLength = sizeof descriptors.hs_descs.source,
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 2 | USB_DIR_OUT,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			.wMaxPacketSize = cpu_to_le16(512),
			.bInterval = 1, /* NAK every 1 uframe */
		},
	},
};

#define STR_INTERFACE_ "Source/Sink"

static const struct {
	struct usb_functionfs_strings_head header;
	struct {
		__le16 code;
		const char str1[sizeof STR_INTERFACE_];
	} __attribute__((packed)) lang0;
} __attribute__((packed)) strings = {
	.header = {
		.magic = cpu_to_le32(FUNCTIONFS_STRINGS_MAGIC),
		.length = cpu_to_le32(sizeof strings),
		.str_count = cpu_to_le32(1),
		.lang_count = cpu_to_le32(1),
	},
	.lang0 = {
		cpu_to_le16(0x0409), /* en-us */
		STR_INTERFACE_,
	},
};

static int init_ep0(const char *ffs_path, int *ep_nodes){

	char *ep_path;
	int ret;

	ep_path = malloc(strlen(ffs_path) + 4 /* "/ep#" */ + 1 /* '\0' */);
	if(ep_path == NULL) {
		fprintf(stderr, "malloc error: %d\n", errno);
		exit(1);
	}

	sprintf(ep_path, "%s/ep0", ffs_path);
	ep_nodes[0] = open(ep_path, O_RDWR);
	if(ep_nodes[0] < 0) {
		fprintf(stderr, "couldnot open ep0: %d\n", errno);
		ret = -errno;
		goto error;
	}

	ret = write(ep_nodes[0], &descriptors, sizeof(descriptors));
	if (ret < 0) {
		fprintf(stderr, "couldn't write descriptors to ep0: %d\n", errno);
		ret = -errno;
		goto error;
	}

	ret = write(ep_nodes[0], &strings, sizeof(strings));
	if (ret < 0) {
		fprintf(stderr, "couldn't write strings to ep0: %d\n", errno);
		ret = -errno;
		goto error;
	}

	sprintf(ep_path, "%s/ep1", ffs_path);
	ep_nodes[1] = open(ep_path, O_RDWR);
	if (ep_nodes[1] < 0) {
		fprintf(stderr, "couldn't open ep1 :%d \n", errno);
		ret = -errno;
		goto error;
	}


	sprintf(ep_path, "%s/ep2", ffs_path);
	ep_nodes[2] = open(ep_path, O_RDWR);
	if (ep_nodes[2] < 0) {
		fprintf(stderr, "couldn't open ep2 :%d \n", errno);
		ret = -errno;
		goto error;
	}
	

error:
	free(ep_path);
	return ret;
}

int usb_init(const char *ffs_path) {

	int ret;
	int ep_nodes[3];

	ret = init_ep0(ffs_path, ep_nodes);

	return ret;
}
