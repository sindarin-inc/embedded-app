#! /usr/bin/bash
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
cd freetype

if [ ! -f builds/unix/configure ]; then
    ./autogen.sh
fi

cp ../modules.cfg .

./configure --host=xtensa-esp32s3-elf --prefix=$PWD/../esp32s3 --disable-shared --enable-static \
  --with-png=no \
  --with-zlib=no \
  --with-bzip2=no \
  --with-brotli=no \
  --with-harfbuzz=no \
  --with-librsvg=no \
  --with-old-mac-fonts=no \
  --with-fsspec=no \
  --with-fsref=no \
  --with-quickdraw-toolbox=no \
  --with-quickdraw-carbon=no \
  --with-ats=no \
  CFLAGS="-mlongcalls"

make
make install
make clean
cd ..
