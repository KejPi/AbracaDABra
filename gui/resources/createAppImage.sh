#! /bin/bash

APPDIR=AppDir
ICON_DIR=$APPDIR/usr/share/icons/hicolor/512x512/apps
RESOURCES_DIR=$PWD/gui/resources
VER=`git describe`

echo "Building application version: $VER"

# Build 
if [ -d build ]; then
	rm -Rf build
fi
mkdir build
cd build
#cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DUSE_SYSTEM_RTLSDR=OFF -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
make install DESTDIR=$APPDIR

# Dirty hack - copying binary to be able to deploy library correctly, otherwise it does not work
rm -Rf $APPDIR/usr/lib/*
cp -f gui/AbracaDABra $APPDIR/usr/bin/

QMAKE=$(which qmake6) VERSION=$VER linuxdeploy --appdir $APPDIR -d ${RESOURCES_DIR}/AbracaDABra.desktop -i $ICON_DIR/AbracaDABra.png --plugin qt --output appimage


