#!/bin/bash
set -v
set -e
rm -rf original base working-copy
mkdir original base working-copy original/play-dir original/del-dir
echo v1 > original/file
echo v1 > original/play-with-me
echo v1 > original/delete-me

./unionfs -o cow working-copy=rw:original=ro base
trap 'if [ "$(ls base)" ]; then umount base; fi; rm -rf base original working-copy' EXIT
sleep 1

[ "$(cat base/file)" = "v1" ]

echo "v2" > base/file
[ "$(cat base/file)" = "v2" ]

echo "v2" > base/play-with-me
[ "$(cat base/play-with-me)" = "v2" ]

[ -f base/play-with-me ]
rm base/play-with-me
[ ! -f base/play-with-me ]

[ -f base/delete-me ]
rm base/delete-me
[ ! -f base/delete-me ]

[ "$(ls base/play-dir)" = "" ]
echo "fool" > base/play-dir/foo
[ "$(ls base/play-dir)" = "foo" ]
rm base/play-dir/foo
[ "$(ls base/play-dir)" = "" ]

[ -d base/play-dir ]
rmdir base/play-dir
[ ! -d base/play-dir ]

[ -d base/del-dir ]
rmdir base/del-dir
[ ! -d base/del-dir ]

! echo v1 > base/del-dir/foo

[ ! -d base/del-dir ]
mkdir base/del-dir
[ ! -f base/del-dir/foo ]
echo v1 > base/del-dir/foo
[ -f base/del-dir/foo ]
rm base/del-dir/foo
[ -d base/del-dir ]
rmdir base/del-dir
[ ! -d base/del-dir ]

fusermount -u base

[ "$(cat original/file)" = "v1" ]
[ "$(cat original/play-with-me)" = "v1" ]
[ "$(cat original/delete-me)" = "v1" ]
[ -d original/play-dir ]
[ -d original/del-dir ]
[ "$(cat working-copy/file)" = "v2" ]

echo "ALL TEST PASSED"
