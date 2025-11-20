#!/bin/bash
set -e
source setup.sh
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
