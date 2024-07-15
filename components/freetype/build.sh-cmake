#! /usr/bin/env bash
#
# Freetype build tool. 
#
# This script will configure and compile the Freetype library. 
# This must be done once when a new version of the library
# is retrieved or any change in the configuration of this library is performed.
#
# Before running this script, please do have esp-idf definitions exported to the shell:
#
# like so:
#
# $ . ~/esp/esp-idf/export.sh
#

rm -rf esp32s3
mkdir esp32s3

# make clean
# cd freetype-2.13.2
# ./configure --host=xtensa-esp32s3-elf --prefix=$PWD/../esp32s3 --disable-shared --enable-static \
#   --with-librsvg=no \
#   --with-old-mac-fonts=no \
#   --with-fsspec=no \
#   --with-fsref=no \
#   --with-quickdraw-toolbox=no \
#   --with-quickdraw-carbon=no \
#   --with-ats=no \
#   CFLAGS="-mlongcalls"
# make



cd freetype
rm -rf build || true
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$IDF_PATH/tools/cmake/toolchain-esp32s3.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-mlongcalls -DHAVE_FSSPEC=0 -DHAVE_FSREF=0 -DHAVE_QUICKDRAW_TOOLBOX=0 -DHAVE_QUICKDRAW_CARBON=0 -DHAVE_ATS=0" \
  -DCMAKE_INSTALL_PREFIX=../../esp32s3 \
  -DFT_REQUIRE_PNG=OFF \
  -DFT_REQUIRE_ZLIB=OFF \
  -DFT_DISABLE_BZIP2=ON \
  -DFT_DISABLE_BROTLI=ON \
  -DFT_DISABLE_HARFBUZZ=ON \
  ..
make -j
make install
