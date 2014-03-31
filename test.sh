#!/bin/bash

set -v
set -e

rm -rf original union working-copy
mkdir original union working-copy original/play-dir original/del-dir
echo v1 > original/file
echo v1 > original/play-with-me
echo v1 > original/delete-me

src/unionfs -d -o cow,stats working-copy=rw:original=ro union >unionfs.log 2>&1 &
trap 'if [ "$(ls union)" ]; then fusermount -u union; fi; rm -rf union original working-copy' EXIT
sleep 1

[ "$(cat union/file)" = "v1" ]

echo "v2" > union/file
[ "$(cat union/file)" = "v2" ]

echo "v2" > union/play-with-me
[ "$(cat union/play-with-me)" = "v2" ]

[ -f union/play-with-me ]
rm union/play-with-me
[ ! -f union/play-with-me ]

[ -f union/delete-me ]
rm union/delete-me
[ ! -f union/delete-me ]

[ "$(ls union/play-dir)" = "" ]
echo "fool" > union/play-dir/foo
[ "$(ls union/play-dir)" = "foo" ]
rm union/play-dir/foo
[ "$(ls union/play-dir)" = "" ]

[ -d union/play-dir ]
rmdir union/play-dir
[ ! -d union/play-dir ]

[ -d union/del-dir ]
rmdir union/del-dir
[ ! -d union/del-dir ]

! echo v1 > union/del-dir/foo

[ ! -d union/del-dir ]
mkdir union/del-dir
[ ! -f union/del-dir/foo ]
echo v1 > union/del-dir/foo
[ -f union/del-dir/foo ]
rm union/del-dir/foo
[ -d union/del-dir ]
rmdir union/del-dir
[ ! -d union/del-dir ]

# rmdir() test
set +e
set +v
rc=0
mkdir original/testdir
touch original/testdir/testfile
mkdir working-copy/testdir
rmdir union/testdir 2>/dev/null
if [ $? -eq 0 ]; then
	echo "rmdir succeeded, although it must not"
	rc=$(($rc + $?))
fi
rm union/testdir/testfile
rc=$(($rc + $?))
rmdir union/testdir/
rc=$(($rc + $?))
if [ $rc -ne 0 ]; then
	echo "rmdir test failed"
	exit 1
else
	echo "rmdir test passed"
fi
set -e


set +e
set +v
getfattr union/stats
if [ $? -eq 0 ]; then
	echo "getfattr did not fail for stats, that's weird"
	rc=$?
fi
set -e


fusermount -u union

[ "$(cat original/file)" = "v1" ]
[ "$(cat original/play-with-me)" = "v1" ]
[ "$(cat original/delete-me)" = "v1" ]
[ -d original/play-dir ]
[ -d original/del-dir ]
[ "$(cat working-copy/file)" = "v2" ]

echo "ALL TEST PASSED"
