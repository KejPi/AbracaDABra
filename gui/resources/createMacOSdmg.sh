#! /bin/bash

VERSION=`git describe`

DIR=`pwd`

function createDmg () {
	APP_NAME="AbracaDABra"
	DMG_FILE_NAME="${APP_NAME}.dmg"
	VOLUME_NAME="${APP_NAME} Installer"
	SOURCE_FOLDER_PATH="gui/Release/"

	# Since create-dmg does not clobber, be sure to delete previous DMG
	[[ -f "${DMG_FILE_NAME}" ]] && rm "${DMG_FILE_NAME}"

	# Create the DMG
    # --background "installer_background.png" \
	create-dmg \
	  --volname "${VOLUME_NAME}" \
	  --window-pos 200 120 \
	  --window-size 640 440 \
      --background ../gui/resources/macos_dmg_bg.png \
	  --icon-size 100 \
	  --icon "${APP_NAME}.app" 140 250 \
	  --hide-extension "${APP_NAME}.app" \
	  --app-drop-link 500 250 \
	  --no-internet-enable \
	  "${DMG_FILE_NAME}" \
	  "${SOURCE_FOLDER_PATH}"
}

### AARCH64

BUILD_DIR=build-aarch64
if [ -d $BUILD_DIR ]; then
	rm -Rf $BUILD_DIR
fi
mkdir $BUILD_DIR

cmake -G Xcode -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake -DEXTERNAL_LIBS_DIR=../AbracaDABra-libs-aarch64
cmake --build $BUILD_DIR --target ALL_BUILD --config Release

cd $BUILD_DIR/gui/Release
$QT_PATH/bin/macdeployqt AbracaDABra.app -codesign="-" # -dmg

cd ../..
createDmg
cd $DIR

### Intel

BUILD_DIR=build-x86_64
if [ -d $BUILD_DIR ]; then
	rm -Rf $BUILD_DIR
fi
mkdir $BUILD_DIR

cmake -G Xcode -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DAPPLE_BUILD_X86_64=ON -DAIRSPY=ON -DSOAPYSDR=ON -DPROJECT_VERSION_RELEASE=ON -DCMAKE_PREFIX_PATH=$QT_PATH_6_4/lib/cmake -DEXTERNAL_LIBS_DIR=../AbracaDABra-libs-x86
cmake --build $BUILD_DIR --target ALL_BUILD --config Release

cd $BUILD_DIR/gui/Release
$QT_PATH_6_4/bin/macdeployqt AbracaDABra.app -codesign="-" # -dmg

cd ../..
createDmg
cd $DIR

# Move files to release folder
mkdir release
mv build-aarch64/AbracaDABra.dmg release/AbracaDABra-$VERSION-AppleSilicon.dmg
mv build-x86_64/AbracaDABra.dmg release/AbracaDABra-$VERSION-Intel.dmg



