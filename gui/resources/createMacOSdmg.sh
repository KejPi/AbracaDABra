#! /bin/bash

VERSION=$1

# Build 
if [ -d build-aarch64 ]; then
	rm -Rf build-aarch64
fi
mkdir build-aarch64
cd build-aarch64
cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DDAB_LIBS_DIR=../AbracaDABra-libs-aarch64
make && cd gui
#mv AbracaDABra.app AbracaDABra-$VERSION.app
#$QT_PATH/bin/macdeployqt AbracaDABra-$VERSION.app -dmg 
$QT_PATH/bin/macdeployqt AbracaDABra.app -dmg 
cd ../..

if [ -d build-x86_64 ]; then
	rm -Rf build-x86_64
fi
mkdir build-x86_64
cd build-x86_64
cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DAPPLE_BUILD_X86_64=ON -DDAB_LIBS_DIR=../AbracaDABra-libs-x86
make && cd gui 
#mv AbracaDABra.app AbracaDABra-$VERSION.app
#$QT_PATH/bin/macdeployqt AbracaDABra-$VERSION.app -dmg 
$QT_PATH/bin/macdeployqt AbracaDABra.app -dmg 

cd ../..

# Move files to release folder
mkdir release
mv build-aarch64/gui/AbracaDABra.dmg release/AbracaDABra-$VERSION-AppleSilicon.dmg
mv build-x86_64/gui/AbracaDABra.dmg release/AbracaDABra-$VERSION-Intel.dmg

