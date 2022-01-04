#!/bin/sh
set -e -x

# libfuse3 is disabled for macos since there's no support in macfuse
# see https://github.com/osxfuse/osxfuse/issues/390

trap "vagrant destroy --force; rm -rf Vagrantfile" SIGINT SIGTERM ERR EXIT

rm -rf Vagrantfile
vagrant init rpodgorny/mymacos
vagrant up

vagrant upload $(pwd) xxx/

# TODO: get rid of source .bashrc
echo "
set -e -x
source .bashrc

uname -a

cd xxx

rm -rf build
mkdir build
cd build
cmake .. -DWITH_LIBFUSE3=FALSE
make

python3 ../test_all.py
" | vagrant ssh

#vagrant destroy --force
