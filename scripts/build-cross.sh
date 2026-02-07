#!/bin/bash
BUILD_DIR="build-cross"
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
echo "Cross-compilation complete. Binary: $BUILD_DIR/SquareLine_Project"
