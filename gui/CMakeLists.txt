cmake_minimum_required(VERSION 3.5)
include (CheckLibraryExists)
include (CheckSymbolExists)
#include (ExternalProject)
if(NOT WIN32)
include (FindPkgConfig)
endif()

set(TARGET AbracaDABra)

project(${TARGET} LANGUAGES CXX)

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/../install/")

option (USE_SYSTEM_RTLSDR "Use system provided rtl-sdr" ON)
option (USE_SYSTEM_LIBUSB "Use system provided libusb" ON)
option (USE_SYSTEM_FAAD "Use system provided libfaad" ON)
option (USE_SYSTEM_MPG123 "Use system provided libmpg123" ON)
option (USE_SYSTEM_RTAUDIO "Use system provided rtaudio" ON)
option (USE_SYSTEM_PORTAUDIO "Use system provided portaudio" ON)


### DAB library
# This hes to be before Qt stuff
set(BACKEND_DIR "${PROJECT_SOURCE_DIR}/../backend")
set(BACKEND_INCLUDE_DIRS "${BACKEND_DIR}")
add_subdirectory(${BACKEND_DIR} backend)
###


## External libs
if(WIN32)
   #set (LIBUSB_WIN_PREBUILD ON)
endif(WIN32)

set (EXT_LIBS_DIR "${CMAKE_SOURCE_DIR}/../../dab_libs")

if (LIBUSB_WIN_PREBUILD)
    set (LIBUSB_PREFIX "${EXT_LIBS_DIR}/libusb")
else(LIBUSB_WIN_PREBUILD)
    set (LIBUSB_PREFIX "${EXT_LIBS_DIR}/libusb-prefix")
endif(LIBUSB_WIN_PREBUILD)

set (RTLSDR_PREFIX "${EXT_LIBS_DIR}/rtlsdr-prefix")
set (FAAD2_PREFIX "${EXT_LIBS_DIR}/faad2-prefix")
set (MPG123_PREFIX "${EXT_LIBS_DIR}/mpg123-prefix")
set (RTAUDIO_PREFIX "${EXT_LIBS_DIR}/rtaudio-prefix")
set (PORTAUDIO_PREFIX "${EXT_LIBS_DIR}/portaudio-prefix")


if (USE_SYSTEM_RTLSDR)
    find_library (RTL_SDR_LIBRARIES rtlsdr)
    if (NOT WIN32)
        pkg_search_module (RTL_SDR librtlsdr)
    endif()
    find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h)
    if (NOT RTL_SDR_LIBRARIES)
        message (WARNING "librtlsdr not found. Build from source and install to: ${RTLSDR_PREFIX}")
        set (USE_SYSTEM_RTLSDR OFF)
    endif ()
endif ()
if (USE_SYSTEM_LIBUSB)
    find_library (LIBUSB_LIBRARIES usb-1.0)
    if (NOT WIN32)
        pkg_search_module (LIBUSB libusb-1.0)
    endif()
    if (NOT LIBUSB_LIBRARIES)
        if (LIBUSB_WIN_PREBUILD)
            message (WARNING "libusb-1.0 not found. Download libusb prebuild libs for Win64 and copy to: ${LIBUSB_PREFIX}")
        else (LIBUSB_WIN_PREBUILD)
            message (WARNING "libusb-1.0 not found. Build from source and install to: ${LIBUSB_PREFIX}")
        endif (LIBUSB_WIN_PREBUILD)
        set (USE_SYSTEM_LIBUSB OFF)
    endif ()
endif ()
if (USE_SYSTEM_FAAD)
    find_library (LIBFAAD2_LIBRARIES faad)    
    if (NOT WIN32)
        pkg_search_module(LIBFAAD2 faad)
        #find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h)
    endif()
    if (NOT LIBFAAD2_LIBRARIES)
        message (WARNING "libfaad not found. Build from source and install to: ${FAAD2_PREFIX}")
        set (USE_SYSTEM_FAAD OFF)
    endif ()
endif ()
if (USE_SYSTEM_MPG123)
    find_library (LIBMPG123_LIBRARIES mpg123)
    if (NOT WIN32)
        pkg_search_module(LIBMPG123 mpg123)
        #find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h)
    endif()
    if (NOT LIBMPG123_LIBRARIES)
        message (WARNING "libmpg123 not found. Build from source and install to: ${MPG123_PREFIX}")
        set (USE_SYSTEM_MPG123 OFF)
    endif ()
endif ()
if (USE_SYSTEM_RTAUDIO)
    find_library (RTAUDIO_LIBRARIES rtaudio)
    if (NOT WIN32)
        pkg_search_module(RTAUDIO rtaudio)
        #find_path(RTAUDIO_INCLUDE_DIRS rtaudio/RtAudio.h)
    endif()
    if (NOT RTAUDIO_LIBRARIES)
        message (WARNING "rtaudio not found. Build from source and install to: ${RTAUDIO_PREFIX}")
        set (USE_SYSTEM_RTAUDIO OFF)
    endif ()
endif ()
if (USE_SYSTEM_PORTAUDIO)
    find_library (PORTAUDIO_LIBRARIES portaudio)
    if (NOT WIN32)
        pkg_search_module(PORTAUDIO portaudio)
        #find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h)
    endif()
    if (NOT PORTAUDIO_LIBRARIES)
        message (WARNING "portaudio not found. Build from source and install to: ${PORTAUDIO_PREFIX}")
        set (USE_SYSTEM_PORTAUDIO OFF)
    endif ()
endif ()

if (NOT USE_SYSTEM_LIBUSB)
    add_library (libusb STATIC IMPORTED)
    set (LIBUSB_LIBRARIES libusb)

    if (LIBUSB_WIN_PREBUILD)
        set_property (TARGET libusb PROPERTY IMPORTED_LOCATION "${LIBUSB_PREFIX}/MinGW64/static/libusb-1.0.a")

        set (RTLSDR_CMAKE_ARGS
            -DLIBUSB_LIBRARIES:STRING=${LIBUSB_PREFIX}/MinGW64/static/libusb-1.0.a
            -DLIBUSB_LIBRARIES:STRING=${LIBUSB_PREFIX}/lib/libusb-1.0.a
            )
    else (LIBUSB_WIN_PREBUILD)
        set_property (TARGET libusb PROPERTY IMPORTED_LOCATION "${LIBUSB_PREFIX}/lib/libusb-1.0.a")

        set (RTLSDR_CMAKE_ARGS
            -DLIBUSB_INCLUDE_DIR:STRING=${LIBUSB_PREFIX}/include/libusb-1.0/
            -DLIBUSB_LIBRARIES:STRING=${LIBUSB_PREFIX}/lib/libusb-1.0.a
            )
    endif (LIBUSB_WIN_PREBUILD)
endif ()

if (NOT USE_SYSTEM_RTLSDR)   
    add_library (rtlsdr STATIC IMPORTED)
    set_property (TARGET rtlsdr PROPERTY IMPORTED_LOCATION "${RTLSDR_PREFIX}/lib/librtlsdr_static.a")
    add_dependencies (rtlsdr rtlsdr_external)

    set (RTL_SDR_INCLUDE_DIRS "${RTLSDR_PREFIX}/include")
    set (RTL_SDR_LIBRARIES rtlsdr ${LIBUSB_LIBRARIES})
    set (RTL_SDR_BUILTIN rtlsdr)
endif ()


if (NOT USE_SYSTEM_FAAD)
    add_library (faad2 STATIC IMPORTED)
    set_property (TARGET faad2 PROPERTY IMPORTED_LOCATION "${FAAD2_PREFIX}/lib/libfaad.a")
    add_dependencies (faad2 faad2_external)

    set (LIBFAAD2_INCLUDE_DIRS "${FAAD2_PREFIX}/include")
    set (LIBFAAD2_LIBRARIES faad2)
    set (FAAD2_BUILTIN faad2)
endif()

if (NOT USE_SYSTEM_MPG123)
    add_library (mpg123 STATIC IMPORTED)
    set_property (TARGET mpg123 PROPERTY IMPORTED_LOCATION "${MPG123_PREFIX}/lib/libmpg123.a")
    add_dependencies (mpg123 mpg123_external)

    set (LIBMPG123_INCLUDE_DIRS "${MPG123_PREFIX}/include")
    set (LIBMPG123_LIBRARIES mpg123)
    set (MPG123_BUILTIN mpg123)
endif()

if (NOT USE_SYSTEM_RTAUDIO)
    add_library (rtaudio STATIC IMPORTED)
    set_property (TARGET rtaudio PROPERTY IMPORTED_LOCATION "${RTAUDIO_PREFIX}/lib/librtaudio.dll.a")
    add_dependencies (rtaudio rtaudio_external)

    set (RTAUDIO_INCLUDE_DIRS "${RTAUDIO_PREFIX}/include/")
    set (RTAUDIO_LIBRARIES rtaudio)
    set (RTAUDIO_BUILTIN rtaudio)
endif()

if (NOT USE_SYSTEM_PORTAUDIO)
    add_library (portaudio STATIC IMPORTED)
    set_property (TARGET portaudio PROPERTY IMPORTED_LOCATION "${PORTAUDIO_PREFIX}/lib/libportaudio.dll.a")
    add_dependencies (portaudio portaudio_external)

    set (PORTAUDIO_INCLUDE_DIRS "${PORTAUDIO_PREFIX}/include/")
    set (PORTAUDIO_LIBRARIES portaudio)
    set (PORTAUDIO_BUILTIN portaudio)
endif()


## QT creator CMAKEFILE

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Multimedia REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Multimedia REQUIRED)

## Pictures
list (APPEND RESOURCES resources.qrc)
qt5_add_resources (RCC_SOURCES ${RESOURCES})

#add_executable(dabGui MACOSX_BUNDLE main.cpp ${APP_ICON_MACOSX})

# Set some Win32 Specific Settings
if(WIN32)
    set(GUI_TYPE WIN32)

    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/appIcon.rc")
endif(WIN32)
# Set some Apple MacOS Specific settings
if (APPLE)
    set(GUI_TYPE MACOSX_BUNDLE)

    # NOTE: Don't include the path in MACOSX_BUNDLE_ICON_FILE -- this is
    # the property added to Info.plist
    set(MACOSX_BUNDLE_ICON_FILE appIcon.icns)

    # And this part tells CMake where to find and install the file itself
    set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/resources/appIcon.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES
           MACOSX_PACKAGE_LOCATION "Resources")
endif(APPLE)

if(ANDROID)
  add_library(${TARGET} SHARED
    ${RCC_SOURCES}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    dabtables.h
    dabtables.cpp
    radiocontrol.h
    radiocontrol.cpp
    inputdevice.h
    inputdevice.cpp
    rawfileinput.h
    rawfileinput.cpp
    rtlsdrinput.h
    rtlsdrinput.cpp
    dldecoder.h
    dldecoder.cpp
    motdecoder.h
    motdecoder.cpp
    motobject.h
    motobject.cpp
    audiodecoder.h
    audiodecoder.cpp
    audiofifo.h
    audiofifo.cpp
    audiooutput.h
    audiooutput.cpp
    setupdialog.h
    setupdialog.cpp
    setupdialog.ui
  )
else()
  add_executable(${TARGET}
    ${GUI_TYPE}
    ${RCC_SOURCES}
    ${APP_ICON_MACOSX}
    ${APP_ICON_RESOURCE_WINDOWS}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    dabtables.h
    dabtables.cpp
    radiocontrol.h
    radiocontrol.cpp
    inputdevice.h
    inputdevice.cpp
    rawfileinput.h
    rawfileinput.cpp
    rtlsdrinput.h
    rtlsdrinput.cpp
    dldecoder.h
    dldecoder.cpp
    motdecoder.h
    motdecoder.cpp
    motobject.h
    motobject.cpp
    audiodecoder.h
    audiodecoder.cpp
    audiofifo.h
    audiofifo.cpp
    audiooutput.h
    audiooutput.cpp
    setupdialog.h
    setupdialog.cpp
    setupdialog.ui
  )
endif()

### DAB library
include_directories ( ${BACKEND_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE dab)
###

# FAAD
include_directories ( ${LIBFAAD2_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${LIBFAAD2_LIBRARIES}" )

# MPG123
include_directories ( ${LIBMPG123_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${LIBMPG123_LIBRARIES}" )

# RTLSDR
include_directories ( ${RTL_SDR_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${RTL_SDR_LIBRARIES}" )

# RTAUDIO
include_directories ( ${RTAUDIO_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${RTAUDIO_LIBRARIES}" )

# PORTAUDIO
include_directories ( ${PORTAUDIO_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${PORTAUDIO_LIBRARIES}" )

include_directories ( ${CMAKE_SOURCE_DIR} )
target_link_libraries(AbracaDABra PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)

# Set a custom plist file for the app bundle
if(APPLE)
    set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in)
endif(APPLE)
