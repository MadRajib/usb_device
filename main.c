#include <errno.h>
#include <stdio.h>
#include <errno.h>
#include "usb.h"

int main(int argc, char **argv) {

	int ret;

	if (argc != 2) {
		printf("ffs directory not specified!\n");
		return 1;
	}

	ret = usb_init(argv[1]);
	if (ret) {
		fprintf(stderr, "USB init failed : %d\n", errno);
	}

	usb_chat_loop();

	return 0;
}
