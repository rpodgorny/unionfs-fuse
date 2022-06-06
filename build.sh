#!/bin/bash -ex

rm -rf build
rm -rf output

if [ "" != "`which ninja`" ]; then
    cmake -GNinja -H. -Bbuild -DCMAKE_INSTALL_PREFIX=output
else
    cmake -H. -Bbuild -DCMAKE_INSTALL_PREFIX=output
fi

cmake --build build --target install 

