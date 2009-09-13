PREFIX=/usr/local
BINDIR=/bin

build:
	$(MAKE) -C src/

clean:
	$(MAKE) -C src/ clean

install: build
	install -d $(DESTDIR)$(PREFIX)$(BINDIR)
	install -m 0755 src/unionfs $(DESTDIR)$(PREFIX)$(BINDIR)
	install -m 0644 man/unionfs-fuse.8 $(DESTDIR)$(PREFIX)/share/man/man8/
