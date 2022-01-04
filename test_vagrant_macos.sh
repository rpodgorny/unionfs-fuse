#!/bin/sh
set -e -x

trap "vagrant destroy --force; rm -rf Vagrantfile" SIGINT SIGTERM ERR EXIT

rm -rf Vagrantfile
vagrant init rpodgorny/mymacos
vagrant up

vagrant upload $(pwd) xxx

# TODO: get rid of source .bashrc
echo "
set -e -x
source .bashrc

cd xxx

rm -rf build
mkdir build
cd build
cmake ..
make

python3 ../test_all.py
" | vagrant ssh

#vagrant destroy --force
