PREFIX=/usr/local
BINDIR=/bin
SBINDIR=/sbin

build:
	$(MAKE) -C src/

clean:
	$(MAKE) -C src/ clean

install: build
	install -d $(DESTDIR)$(PREFIX)$(BINDIR)
	install -d $(DESTDIR)$(PREFIX)$(SBINDIR)
	install -d $(DESTDIR)$(PREFIX)/share/man/man8
	install -m 0755 src/unionfs-fuse $(DESTDIR)$(PREFIX)$(BINDIR)
	install -m 0755 src/unionfs-fuse-ctl $(DESTDIR)$(PREFIX)$(BINDIR)
	install -m 0755 mount.unionfs-fuse $(DESTDIR)$(PREFIX)$(SBINDIR)
	install -m 0644 man/unionfs-fuse.8 $(DESTDIR)$(PREFIX)/share/man/man8/
