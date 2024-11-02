#! /bin/bash

APPDIR=AppDir
ICON_DIR=$APPDIR/usr/share/icons/hicolor/512x512/apps
RESOURCES_DIR=$PWD/gui/resources
QML_DIR=$PWD/gui/qml
VER=`git describe`

if [ -z ${QT_PATH+x} ]; then
	QMAKE=$(which qmake6)
else
	QMAKE=$QT_PATH/bin/qmake
fi

echo "Building application version: $VER"

# Build 
if [ -d build ]; then
	rm -Rf build
fi
mkdir build
cd build
#cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib

if [ -z ${QT_PATH+x} ]; then
	# cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DUSE_SYSTEM_RTLSDR=OFF -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_INSTALL_PREFIX=/usr	
	cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DUSE_SYSTEM_RTLSDR=OFF -DCMAKE_INSTALL_PREFIX=/usr
else 
	cmake .. -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DUSE_SYSTEM_RTLSDR=OFF -DUSE_SYSTEM_QCUSTOMPLOT=OFF -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake
fi

make -j$(nproc)
make install DESTDIR=$APPDIR

# Dirty hack - copying binary to be able to deploy library correctly, otherwise it does not work
rm -Rf $APPDIR/usr/lib/*
cp -f gui/AbracaDABra $APPDIR/usr/bin/

if [ -z ${QT_PATH+x} ]; then
	QML_SOURCES_PATHS=$QML_DIR QMAKE=$(which qmake6) VERSION=$VER EXTRA_QT_PLUGINS=location \
		linuxdeploy --appdir $APPDIR -d ${RESOURCES_DIR}/AbracaDABra.desktop -i $ICON_DIR/AbracaDABra.png --plugin qt --output appimage	
else
	QML_SOURCES_PATHS=$QML_DIR QMAKE=$QT_PATH/bin/qmake VERSION=$VER EXTRA_QT_PLUGINS=location \
		linuxdeploy --appdir $APPDIR -d ${RESOURCES_DIR}/AbracaDABra.desktop -i $ICON_DIR/AbracaDABra.png --plugin qt --output appimage
fi


