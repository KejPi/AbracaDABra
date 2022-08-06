#! /bin/bash

APPDIR=AppDir
RESOURCES_DIR=$PWD/gui/resources
ICON_SRC=${RESOURCES_DIR}/appIcon-linux.png	

# Build 
if [ -d build ]; then
	rm -Rf build
fi
mkdir build
cd build
cmake .. -DAIRSPY=ON
make

# Prepare directory structure
mkdir -p ./$APPDIR/usr/bin/
cp gui/AbracaDABra $APPDIR/usr/bin/

mkdir -p ./$APPDIR/usr/lib/
# libsdrdab will be copied automatically

# create icon
ICON_DIR=$APPDIR/usr/share/icons/hicolor/512x512/apps
mkdir -p $ICON_DIR
convert $ICON_SRC -resize 512 $ICON_DIR/AbracaDABra.png

VERSION=$1 linuxdeploy-x86_64.AppImage --appdir $APPDIR -d ${RESOURCES_DIR}/AbracaDABra.desktop -i $ICON_DIR/AbracaDABra.png --plugin qt --output appimage


