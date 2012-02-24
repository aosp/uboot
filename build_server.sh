#!/bin/bash
# This build script is used by the build server.
# It's supposed to be executed in the directory where it lives.

set -x -e

if [ -z $BUILD_CONFIG ]; then
  echo "Please set BUILD_CONFIG!"
  exit 2
fi

if [ -z $BUILD_OUTPUT ]; then
  echo "Please set output dir BUILD_OUTPUT!"
  exit 2
fi

if [ -z $BUILD_JOBS ]; then
  BUILD_JOBS=10
fi

FILES_TO_ARCHIVE="u-boot.bin"
if [ "$BUILD_CONFIG" = "omap4_tungsten_config" ]; then
FILES_TO_ARCHIVE="u-boot.img MLO spl/u-boot-spl.bin"
fi

export ARCH=arm
export CROSS_COMPILE=arm-eabi-

make clean
make $BUILD_CONFIG
make -j $BUILD_JOBS
mkdir -p $BUILD_OUTPUT
cp -f $FILES_TO_ARCHIVE $BUILD_OUTPUT

if [ "$BUILD_CONFIG" = "omap4_tungsten_config" ]; then
make clean
make MFG=1 $BUILD_CONFIG
make MFG=1 -j $BUILD_JOBS
cp -f u-boot.img $BUILD_OUTPUT/factory_u-boot.img

make clean
make FORCE_FBT=1 $BUILD_CONFIG
make FORCE_FBT=1 -j $BUILD_JOBS
cp -f u-boot.bin $BUILD_OUTPUT/3RD.unsigned
fi


