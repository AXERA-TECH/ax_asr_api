#!/bin/bash
mkdir -p build_ax8850_aarch64 && cd build_ax8850_aarch64
cmake ..  \
  -DCHIP_AX8850=ON  \
  -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-none-linux-gnu.toolchain.cmake \
  -DCMAKE_INSTALL_PREFIX=../install/ax8850_aarch64 \
  -DCMAKE_BUILD_TYPE=Release  \
  $@
make -j4
make install