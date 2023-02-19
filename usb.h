#ifndef USB_H_
#define USB_H_

#include <unistd.h>
#include <stdint.h>

#define EXIT_COMMAND "\\exit"
#define MAX_LINE_LENGTH 1024*8
#define EP_IN_IDX 1
#define EP_OUT_IDX 2

typedef struct{
	uint16_t len;
	char buf[MAX_LINE_LENGTH];
}__attribute__((__packed__)) msg_t;

int usb_init(const char *);
int usb_recv_msg(int ep, msg_t *msg);
int usb_send_msg(int ep, msg_t *);
void usb_chat_loop();

#endif /*USB_H_*/
