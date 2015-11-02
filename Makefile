PREFIX=/usr/local
BINDIR=/bin
SBINDIR=/sbin

build:
	$(MAKE) -C src/

build_coverage:
	CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage" \
	       LDFLAGS="-lgcov -coverage" $(MAKE) -C src/

clean:
	$(MAKE) -C src/ clean

test_coverage: clean build_coverage coverage
	./test.py
	(cd src && gcovr -r . --html -o ../coverage/coverage.html --html-details)

coverage:
	mkdir $@

install: build
	install -d $(DESTDIR)$(PREFIX)$(BINDIR)
	install -d $(DESTDIR)$(PREFIX)$(SBINDIR)
	install -d $(DESTDIR)$(PREFIX)/share/man/man8
	install -m 0755 src/unionfs $(DESTDIR)$(PREFIX)$(BINDIR)
	install -m 0755 src/unionfsctl $(DESTDIR)$(PREFIX)$(BINDIR)
	install -m 0755 mount.unionfs $(DESTDIR)$(PREFIX)$(SBINDIR)
	install -m 0644 man/unionfs.8 $(DESTDIR)$(PREFIX)/share/man/man8/
