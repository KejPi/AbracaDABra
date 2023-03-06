#! /bin/bash

APPDIR=AppDir
ICON_DIR=$APPDIR/usr/share/icons/hicolor/512x512/apps
RESOURCES_DIR=$PWD/gui/resources
#ICON_SRC=${RESOURCES_DIR}/appIcon-linux.png	

# Build 
if [ -d build ]; then
	rm -Rf build
fi
mkdir build
cd build
cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
make install DESTDIR=$APPDIR

# Prepare directory structure
#mkdir -p ./$APPDIR/usr/bin/
#cp gui/AbracaDABra $APPDIR/usr/bin/

#mkdir -p ./$APPDIR/usr/lib/
# libdabsdr will be copied automatically

# create icon
#mkdir -p $ICON_DIR
#convert $ICON_SRC -resize 512 $ICON_DIR/AbracaDABra.png

QMAKE=`which qmake6` VERSION=$1 linuxdeploy --appdir $APPDIR -d ${RESOURCES_DIR}/AbracaDABra.desktop -i $ICON_DIR/AbracaDABra.png --plugin qt --output appimage


