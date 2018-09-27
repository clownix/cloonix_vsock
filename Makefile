PREFIX = /usr
SUBDIRS = libmdl srv cli

define SYSTEMD_UNIT_FILE
[Unit]
Description=virtio-vsock host/guest use for distant shell
After=systemd-modules-load.service

[Service]
Type=forking
ExecStart=$(PREFIX)/bin/vsock_srv 7777
Restart=always

[Install]
WantedBy=multi-user.target
endef

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(addprefix clean_, $(SUBDIRS))

$(addprefix clean_, $(SUBDIRS)): clean_%:
	$(MAKE) -C $* clean

export SYSTEMD_UNIT_FILE
install:
	install -vD cli/vsock_cli $(PREFIX)/bin/vsock_cli
	install -vD srv/vsock_srv $(PREFIX)/bin/vsock_srv
	mkdir -p $(PREFIX)/lib/systemd/system
	echo "$$SYSTEMD_UNIT_FILE" > $(PREFIX)/lib/systemd/system/vsock_srv.service

.PHONY: all $(SUBDIRS) clean $(addprefix clean_, $(SUBDIRS)) install
