#!/bin/bash

CURDIR="`pwd`"

cat > umltest.inner.sh <<EOF
#!/bin/sh
(
	set -e
	set -x
	insmod /usr/lib/uml/modules/\`uname -r\`/kernel/fs/fuse/fuse.ko
	cd "$CURDIR"
	python3 test.py
	echo Success
)
echo "\$?" > "$CURDIR"/umltest.status
halt -f
EOF

chmod +x umltest.inner.sh

/usr/bin/linux.uml init=`pwd`/umltest.inner.sh rootfstype=hostfs rw

exit $(<umltest.status)
