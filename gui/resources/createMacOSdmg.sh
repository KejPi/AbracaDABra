#! /bin/bash

VERSION=`git describe`

DIR=`pwd`

### AARCH64

BUILD_DIR=build-aarch64
if [ -d $BUILD_DIR ]; then
	rm -Rf $BUILD_DIR
fi
mkdir $BUILD_DIR

# cd $BUILD_DIR
# cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DDAB_LIBS_DIR=../AbracaDABra-libs-aarch64
# make && cd gui
cmake -G Xcode -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DEXTERNAL_LIBS_DIR=../AbracaDABra-libs-aarch64
cmake --build $BUILD_DIR --target ALL_BUILD --config Release
cd $BUILD_DIR/gui/Release

$QT_PATH/bin/macdeployqt AbracaDABra.app -dmg -codesign="-"
cd $DIR

### Intel

BUILD_DIR=build-x86_64
if [ -d $BUILD_DIR ]; then
	rm -Rf $BUILD_DIR
fi
mkdir $BUILD_DIR

# cd $BUILD_DIR
# cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$$QT_PATH_6_4/lib/cmake -DDAB_LIBS_DIR=../AbracaDABra-libs-aarch64
# make && cd gui
cmake -G Xcode -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DAPPLE_BUILD_X86_64=ON -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$QT_PATH_6_4/lib/cmake -DEXTERNAL_LIBS_DIR=../AbracaDABra-libs-x86
cmake --build $BUILD_DIR --target ALL_BUILD --config Release
cd $BUILD_DIR/gui/Release

$QT_PATH_6_4/bin/macdeployqt AbracaDABra.app -dmg -codesign="-"
cd $DIR

# Move files to release folder
mkdir release
mv build-aarch64/gui/Release/AbracaDABra.dmg release/AbracaDABra-$VERSION-AppleSilicon.dmg
mv build-x86_64/gui/Release/AbracaDABra.dmg release/AbracaDABra-$VERSION-Intel.dmg

