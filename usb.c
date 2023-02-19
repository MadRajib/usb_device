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
#include <stdint.h>
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

msg_t M;
int ep_nodes[3];

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

static int usb_handle_ep0(int ep0, int *connected)
{
	struct usb_functionfs_event event;
	int ret;

	ret = read(ep0, &event, sizeof(event));
	if (!ret) {
		fprintf(stderr, "unable to read event from ep0 %d\n", errno);
		return -EIO;
	}

	switch (event.type) {
	case FUNCTIONFS_SETUP:
		/* stall for all setuo requests */
		if (event.u.setup.bRequestType & USB_DIR_IN)
			ret = write(ep0, NULL, 0);
		else
			ret = read(ep0, NULL, 0);
		break;

	case FUNCTIONFS_ENABLE:
		*connected = 1;
		break;

	case FUNCTIONFS_DISABLE:
		*connected = 0;
		break;

	default:
		break;
	}

	return 0;
}

int usb_recv_msg(int ep, msg_t *msg) {
	int ret;
	int len;

	ret = read(ep, &msg->len, sizeof(msg->len));
	if (ret < sizeof(msg->len)) {
		fprintf(stderr,"Unable to receive length %d \n", errno);
		return ret;
	}

	len = le16toh(msg->len) - 2;

	ret = read(ep, msg->buf, len);
	if (ret < len) {
		fprintf(stderr,"Unable to receive buff %d \n", errno);
		return ret;
	}

	return ret;
}

int usb_send_msg(int ep, msg_t *msg) {
	
	int len;
	int ret;

	len = strlen(msg->buf) + 1;
	msg->len = htole16(len + 2);

	ret = write(ep, &msg->len, sizeof(msg->len));
	if (ret < 0) {
		fprintf(stderr, "Unable to send lenght to ep: %d\n", errno);
		return ret;
	}

	ret = write(ep, msg->buf, len);
	if (ret < 0) {
		fprintf(stderr, "Unable to send buf to ep: %d\n", errno);
		return ret;
	}

	return ret;
}

void usb_chat_loop()
{
	int connected = 0;
	int len;
	char *buf;
	int ret;

	printf("Waiting for connection...\n");

	while (1) {
		printf("Waiting for connection...\n");

		while (!connected) {
			ret = usb_handle_ep0(ep_nodes[0], &connected);
			if (ret < 0) {
				fprintf(stderr, " usb_handle_ep0 %d\n", errno);
				exit(1);
			}
				
		}

		printf("Chat started.\n");
		while (connected) {
			ret = usb_recv_msg(ep_nodes[EP_OUT_IDX], &M);
			if (ret < 0) {
				if (ret == -ECONNRESET) {
					printf("Connection lost.");
					break;
				}
				return;
			}

			printf("host> %s\n", M.buf);

			printf("device> ");
			buf = fgets(M.buf, sizeof(M.buf), stdin);
			if (!buf) {
				fprintf(stderr, "I/O error in fgets %d\n", errno);
				return;
			}

			len = strlen(buf);
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';

			if (!strcmp(EXIT_COMMAND, M.buf))
				break;

			ret = usb_send_msg(ep_nodes[EP_IN_IDX], &M);
			if (ret < 0) {
				if (ret == -ECONNRESET) {
					printf("Connection lost.");
					break;
				}
				return;
			}

		}

	}
}


int usb_init(const char *ffs_path) {

	int ret;

	ret = init_ep0(ffs_path, ep_nodes);

	return ret;
}
