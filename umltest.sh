#!/bin/bash

CURDIR="$(pwd)"
VIRTUALENV_PATH="$(realpath ~/virtualenv)"
echo "VIRTUALENV_PATH: ${VIRTUALENV_PATH}"

cat > umltest.inner.sh <<EOF
#!/bin/sh

# Ensure that we really start on a clean state. The following commands are allowed to
# fail
umount union
rm -r ro1 ro2 rw1 rw2 union

# Perform the actual test setup and run the tests
(
	set -e -x
        # source python3.5, test.py depends on python >= 3.3 (PermissionError)
	. "${VIRTUALENV_PATH}"/python3.5/bin/activate
	insmod /usr/lib/uml/modules/\`uname -r\`/kernel/fs/fuse/fuse.ko
	cd "$CURDIR"
	python3 --version
        # sleep if it fails to allow writing stuff
	RUNNING_ON_TRAVIS_CI= python3 test.py
	echo Success
)
echo "\$?" > "$CURDIR"/umltest.status
halt -f
EOF

chmod +x umltest.inner.sh

/usr/bin/linux.uml init="${CURDIR}"/umltest.inner.sh rootfstype=hostfs rw

RESULT=$(<"${CURDIR}"/umltest.status)

exit $RESULT
