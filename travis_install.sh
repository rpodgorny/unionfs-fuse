#!/bin/sh

FUSE_VER=3.10.3
wget https://github.com/libfuse/libfuse/releases/download/fuse-${FUSE_VER}/fuse-${FUSE_VER}.tar.gz
tar xzf fuse-${FUSE_VER}.tar.gz
cd fuse-${FUSE_VER}
mkdir build
cd build
meson ..
ninja
sudo ninja install
test -e /usr/local/lib/pkgconfig || sudo mkdir /usr/local/lib/pkgconfig
sudo mv /usr/local/lib/*/pkgconfig/* /usr/local/lib/pkgconfig/
