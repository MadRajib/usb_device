CFLAGS= -Wall -O2 -c
LDFLAGS=

OBJS=main.o usb.o
all: usb_demon

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
usb_demon: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	rm -rf *.o

install : stage
	install -d  $(DESTDIR)/bin/
	install -m 755 ./usb_demon $(DESTDIR)/bin/
	install -d  $(DESTDIR)/etc/systemd/system/
	install -m 755 ./systemd_files/usb-daemon.service $(DESTDIR)/etc/systemd/system/
	install -m 755 ./systemd_files/usb-gadget_config.service $(DESTDIR)/etc/systemd/system/
	install -m 755 ./systemd_files/dev-ffs_usb.mount $(DESTDIR)/etc/systemd/system/
	install -m 755 ./systemd_files/enable_usb.service $(DESTDIR)/etc/systemd/system/
	install -d $(DESTDIR)/etc/initscripts/
	install -m 755 ./systemd_files/start_usb $(DESTDIR)/etc/initscripts/
	install -m 755 ./systemd_files/enable_usb $(DESTDIR)/etc/initscripts/

clean:
	rm -rf usb_demon *.o

