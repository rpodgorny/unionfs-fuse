#!/bin/sh
set -e -x

PKGS="fuse3 libfuse3-dev"
CMAKE_CMD="cmake .."

# uncomment this for libfuse2
#PKGS="fuse libfuse-dev"
#CMAKE_CMD="cmake .. -DWITH_LIBFUSE3=FALSE"

trap "vagrant destroy --force; rm -rf Vagrantfile" SIGINT SIGTERM ERR EXIT

rm -rf Vagrantfile
#vagrant init ubuntu/bionic64
#vagrant init --box-version 20220324.0.0 ubuntu/focal64
vagrant init --box-version 20220405.0.0 ubuntu/jammy64
#vagrant box update
vagrant up

echo "
set -e -x

uname -a

sudo apt-get update -y
sudo apt-get install -y gcc make pkg-config cmake ${PKGS}

sudo apt-get install -y python3 python3-pip
sudo pip install pytest
" | vagrant ssh

echo "
set -e -x

cp -av /vagrant /var/tmp/xxx
cd /var/tmp/xxx

rm -rf build
mkdir build
cd build
${CMAKE_CMD}
make

sudo modprobe fuse
python3 ../test_all.py
" | vagrant ssh

#vagrant destroy --force
