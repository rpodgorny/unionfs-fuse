PREFIX=/usr/local
BINDIR=/bin
SBINDIR=/sbin

build:
	$(MAKE) -C src/

build_coverage:
	CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage" \
	       LDFLAGS="-lgcov -coverage" $(MAKE) -C src/

clean: clean_coverage
	$(MAKE) -C src/ clean

test: clean build
	python3 -m pytest

test_coverage: clean build_coverage coverage
	python3 -m pytest
	(cd src && gcovr -r . --html -o ../coverage/index.html --html-details)
	(cd src && gcovr -r .)

clean_coverage:
	rm -rf coverage
	rm -rf src/*.gcda
	rm -rf src/*.gcno

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
