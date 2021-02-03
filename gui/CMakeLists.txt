cmake_minimum_required(VERSION 3.5)
include (CheckLibraryExists)
include (CheckSymbolExists)
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
option (USE_SYSTEM_PORTAUDIO "Use system provided portaudio" ON)


### DAB library
# This has to be before Qt stuff
set(BACKEND_DIR "${PROJECT_SOURCE_DIR}/../backend")
set(BACKEND_INCLUDE_DIRS "${BACKEND_DIR}")
add_subdirectory(${BACKEND_DIR} backend)
###


## External libs
set (EXT_LIBS_DIR "${CMAKE_SOURCE_DIR}/../../dab_libs")

if(WIN32)
   #set (LIBUSB_WIN_PREBUILD ON)
endif(WIN32)

if (LIBUSB_WIN_PREBUILD)
    set (LIBUSB_PREFIX "${EXT_LIBS_DIR}/libusb")
else(LIBUSB_WIN_PREBUILD)
    set (LIBUSB_PREFIX "${EXT_LIBS_DIR}/libusb-prefix")
endif(LIBUSB_WIN_PREBUILD)


set (RTLSDR_PREFIX "${EXT_LIBS_DIR}/rtlsdr-prefix")
set (FAAD2_PREFIX "${EXT_LIBS_DIR}/faad2-prefix")
set (MPG123_PREFIX "${EXT_LIBS_DIR}/mpg123-prefix")
set (PORTAUDIO_PREFIX "${EXT_LIBS_DIR}/portaudio-prefix")


if (USE_SYSTEM_RTLSDR)
    find_library (RTL_SDR_LINK_LIBRARIES rtlsdr)
    find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h)
    if (NOT WIN32)
        pkg_search_module (RTL_SDR librtlsdr)
    endif()
    if (NOT RTL_SDR_LINK_LIBRARIES)
        message (WARNING "librtlsdr not found. Build from source and install to: ${RTLSDR_PREFIX}")
        set (USE_SYSTEM_RTLSDR OFF)
    endif ()
endif ()

if (USE_SYSTEM_LIBUSB)
    find_library (LIBUSB_LINK_LIBRARIES usb-1.0)
    if (NOT WIN32)
        pkg_search_module (LIBUSB libusb-1.0)
    endif()
    if (NOT LIBUSB_LINK_LIBRARIES)
        if (LIBUSB_WIN_PREBUILD)
            message (WARNING "libusb-1.0 not found. Download libusb prebuild libs for Win64 and copy to: ${LIBUSB_PREFIX}")
        else (LIBUSB_WIN_PREBUILD)
            message (WARNING "libusb-1.0 not found. Build from source and install to: ${LIBUSB_PREFIX}")
        endif (LIBUSB_WIN_PREBUILD)
        set (USE_SYSTEM_LIBUSB OFF)
    endif ()
endif ()

if (USE_SYSTEM_FAAD)
    find_library (LIBFAAD2_LINK_LIBRARIES faad)
    find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h)
    if (NOT LIBFAAD2_LINK_LIBRARIES)
        message (WARNING "libfaad not found. Build from source and install to: ${FAAD2_PREFIX}")
        set (USE_SYSTEM_FAAD OFF)
    endif ()
endif ()

if (USE_SYSTEM_MPG123)
    find_library (LIBMPG123_LINK_LIBRARIES mpg123)
    find_path(LIBMPG123_INCLUDE_DIRS mpg123.h)
    if (NOT WIN32)
        pkg_search_module(LIBMPG123 mpg123)
    endif()
    if (NOT LIBMPG123_LINK_LIBRARIES)
        message (WARNING "libmpg123 not found. Build from source and install to: ${MPG123_PREFIX}")
        set (USE_SYSTEM_MPG123 OFF)
    endif ()
endif ()

if (USE_SYSTEM_PORTAUDIO)
    find_library (PORTAUDIO_LINK_LIBRARIES portaudio)
    find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h)
    if (NOT WIN32)
        pkg_search_module(PORTAUDIO portaudio-2.0)
    endif()
    if (NOT PORTAUDIO_LINK_LIBRARIES)
        message (WARNING "portaudio not found. Build from source and install to: ${PORTAUDIO_PREFIX}")
        set (USE_SYSTEM_PORTAUDIO OFF)
    endif ()
endif ()


if (NOT USE_SYSTEM_LIBUSB)
    add_library (libusb STATIC IMPORTED)
    set (LIBUSB_LINK_LIBRARIES libusb)

    if (LIBUSB_WIN_PREBUILD)
        set_property (TARGET libusb PROPERTY IMPORTED_LOCATION "${LIBUSB_PREFIX}/MinGW64/static/libusb-1.0.a")

    else (LIBUSB_WIN_PREBUILD)
        set_property (TARGET libusb PROPERTY IMPORTED_LOCATION "${LIBUSB_PREFIX}/lib/libusb-1.0.a")
    endif (LIBUSB_WIN_PREBUILD)
endif ()

if (NOT USE_SYSTEM_RTLSDR)   
    add_library (rtlsdr STATIC IMPORTED)
    set_property (TARGET rtlsdr PROPERTY IMPORTED_LOCATION "${RTLSDR_PREFIX}/lib/librtlsdr_static.a")
    add_dependencies (rtlsdr rtlsdr_external)

    set (RTL_SDR_INCLUDE_DIRS "${RTLSDR_PREFIX}/include")
    set (RTL_SDR_LINK_LIBRARIES rtlsdr ${LIBUSB_LINK_LIBRARIES})
endif ()

if (NOT USE_SYSTEM_FAAD)
    add_library (faad2 STATIC IMPORTED)
    set_property (TARGET faad2 PROPERTY IMPORTED_LOCATION "${FAAD2_PREFIX}/lib/libfaad.a")
    add_dependencies (faad2 faad2_external)

    set (LIBFAAD2_INCLUDE_DIRS "${FAAD2_PREFIX}/include")
    set (LIBFAAD2_LINK_LIBRARIES faad2)
endif()

if (NOT USE_SYSTEM_MPG123)
    add_library (mpg123 STATIC IMPORTED)
    set_property (TARGET mpg123 PROPERTY IMPORTED_LOCATION "${MPG123_PREFIX}/libmpg123-0.dll")
    add_dependencies (mpg123 mpg123_external)

    set (LIBMPG123_INCLUDE_DIRS "${MPG123_PREFIX}")
    set (LIBMPG123_LINK_LIBRARIES mpg123)
endif()

if (NOT USE_SYSTEM_PORTAUDIO)
    add_library (portaudio STATIC IMPORTED)
    set_property (TARGET portaudio PROPERTY IMPORTED_LOCATION "${PORTAUDIO_PREFIX}/lib/libportaudio.dll")
    add_dependencies (portaudio portaudio_external)

    set (PORTAUDIO_INCLUDE_DIRS "${PORTAUDIO_PREFIX}/include/")
    set (PORTAUDIO_LINK_LIBRARIES portaudio)
endif()


## QT creator CMAKEFILE

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

### DAB library
include_directories ( ${BACKEND_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE dab)

# FAAD
include_directories ( ${LIBFAAD2_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${LIBFAAD2_LINK_LIBRARIES}" )

# MPG123
include_directories ( ${LIBMPG123_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${LIBMPG123_LINK_LIBRARIES}" )

# RTLSDR
include_directories ( ${RTL_SDR_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${RTL_SDR_LINK_LIBRARIES}" )

# PORTAUDIO
include_directories ( ${PORTAUDIO_INCLUDE_DIRS} )
target_link_libraries(${PROJECT_NAME} PRIVATE "${PORTAUDIO_LINK_LIBRARIES}" )

include_directories ( ${CMAKE_SOURCE_DIR} )
target_link_libraries(AbracaDABra PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)

# Set a custom plist file for the app bundle
if(APPLE)
    set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in)
endif(APPLE)
