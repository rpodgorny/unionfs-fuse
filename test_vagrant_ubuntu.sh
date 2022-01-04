#!/bin/sh
set -e -x
rm -rf Vagrantfile

trap "vagrant destroy --force; rm -rf Vagrantfile" SIGINT SIGTERM ERR EXIT

#vagrant init ubuntu/hirsute64
vagrant init ubuntu/focal64
vagrant up

echo "
set -e -x

uname -a

sudo apt-get update -y
sudo apt-get install -y pkg-config fuse3

sudo modprobe fuse

sudo apt-get install -y gcc make pkg-config cmake libfuse3-dev

sudo apt-get install -y python3 python3-pip
sudo pip install pytest

cp -av /vagrant /var/tmp/xxx
cd /var/tmp/xxx

mkdir build
cd build
cmake ..
make

python3 ../test_all.py
" | vagrant ssh

#vagrant destroy --force
