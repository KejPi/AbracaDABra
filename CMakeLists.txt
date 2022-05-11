cmake_minimum_required(VERSION 3.5)
include (CheckLibraryExists)
include (CheckSymbolExists)
if(NOT WIN32)
    include (FindPkgConfig)
endif()

set(TARGET AbracaDABra)

project(${TARGET}
    VERSION 0.99
    LANGUAGES CXX)

# Select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

#########################################################
## OPTIONAL SWITCHES

# AAC decoder - default is FAAD2 but FDK-AAC can be used optionally
option (USE_FDK_AAC           "Compile with fdk-aac library from Franhofer instead of faad2" OFF)

# Options to force using libs build manually and installed in ${CMAKE_SOURCE_DIR}/../../dab-libs
option (USE_SYSTEM_RTLSDR     "Use system provided rtl-sdr"     OFF)
option (USE_SYSTEM_LIBUSB     "Use system provided libusb"      OFF)
option (USE_SYSTEM_FAAD       "Use system provided libfaad"     OFF)
option (USE_SYSTEM_FDKAAC     "Use system provided libfdk-aac"  OFF)
option (USE_SYSTEM_MPG123     "Use system provided libmpg123"   OFF)
option (USE_SYSTEM_PORTAUDIO  "Use system provided portaudio"   OFF)

option(APPLE_BUILD_X86_64     "Build x86_64 binary for Apple"   OFF)

if (APPLE_BUILD_X86_64)
    set(CMAKE_OSX_ARCHITECTURES "x86_64")

    # libs need to be either universal or built separately for x86_64 (this case)
    set(USE_SYSTEM_RTLSDR    OFF)
    set(USE_SYSTEM_LIBUSB    OFF)
    set(USE_SYSTEM_FAAD      OFF)
    set(USE_SYSTEM_FDKAAC    OFF)
    set(USE_SYSTEM_MPG123    OFF)
    set(USE_SYSTEM_PORTAUDIO OFF)
endif()

#########################################################
## VERSION
set(PROJECT_VERSION_RELEASE OFF)
#string(TIMESTAMP PROJECT_BUILD_DATE "%Y-%m-%d %H:%M")
execute_process(COMMAND git log --pretty=format:%h -n 1
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE PROJECT_GIT_REV
                ERROR_QUIET)

# Check whether we got any revision
if ("${PROJECT_GIT_REV}" STREQUAL "")
    set(PROJECT_GIT_REV "N/A")
endif()

#########################################################
## DAB library (backend)
if(EXISTS "${PROJECT_SOURCE_DIR}/dabsdr/CMakeLists.txt")
    set(DABSDR_DIR "${PROJECT_SOURCE_DIR}/dabsdr")

    # dab library source code available
    add_subdirectory(${DABSDR_DIR} dabsdr)

    set(DAB_LINK_LIBRARIES dabsdr)
    set(DAB_INCLUDE_DIRS ${DABSDR_DIR}/src ${PROJECT_BINARY_DIR}/dabsdr/src)
else()
    # dab library
    string(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_HOST_SYSTEM_PROCESSOR}" PLATFORM)
    set(DABSDR_DIR "${PROJECT_SOURCE_DIR}/lib")

    find_library(DAB_LINK_LIBRARIES dabsdr PATHS ${DABSDR_DIR}/${PLATFORM} NO_DEFAULT_PATH)
    find_path(DAB_INCLUDE_DIRS dabsdr.h PATHS ${DABSDR_DIR}/${PLATFORM} NO_DEFAULT_PATH)
    if (DAB_LINK_LIBRARIES)
        message (STATUS "libdabsdr found: ${DAB_LINK_LIBRARIES}")
    else()
        message (FATAL_ERROR "libdabsdr not found.")
    endif()
endif()
###

#########################################################
## External libs
set (DAB_LIBS_DIR "${PROJECT_SOURCE_DIR}/../${TARGET}-libs")


#########################################################
## RTL-SDR
if (USE_SYSTEM_RTLSDR)
    find_library (RTL_SDR_LINK_LIBRARIES rtlsdr)
    find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h)
    if (NOT WIN32)
        pkg_search_module (RTL_SDR rtlsdr)
    endif()
    if (NOT RTL_SDR_LINK_LIBRARIES)
        # not found is system -> trying ${DAB_LIBS_DIR}
        message (WARNING "librtlsdr not found in system, searching in ${DAB_LIBS_DIR}")

        find_library(RTL_SDR_LINK_LIBRARIES rtlsdr PATHS ${DAB_LIBS_DIR}/lib)
        find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h PATHS ${DAB_LIBS_DIR}/include)

        if (RTL_SDR_LINK_LIBRARIES)
            message (STATUS "librtlsdr found: ${RTL_SDR_LINK_LIBRARIES}")
        else(RTL_SDR_LINK_LIBRARIES)
            message (FATAL "librtlsdr not found. Build from source and install to: ${DAB_LIBS_DIR}")
        endif()
    endif()
else(USE_SYSTEM_RTLSDR)
    find_library(RTL_SDR_LINK_LIBRARIES rtlsdr PATHS ${DAB_LIBS_DIR}/lib NO_DEFAULT_PATH)
    find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h PATHS ${DAB_LIBS_DIR}/include NO_DEFAULT_PATH)
    if (RTL_SDR_LINK_LIBRARIES)
        message (STATUS "librtlsdr found: ${RTL_SDR_LINK_LIBRARIES}")
    else()
        message (FATAL_ERROR "librtlsdr not found. Build from source and install to: ${DAB_LIBS_DIR}")
    endif()
endif (USE_SYSTEM_RTLSDR)

#########################################################
## LIBUSB
if (USE_SYSTEM_LIBUSB)
    find_library (LIBUSB_LINK_LIBRARIES usb-1.0)
    if (NOT WIN32)
        pkg_search_module (LIBUSB usb-1.0)
    endif()
    if (NOT LIBUSB_LINK_LIBRARIES)
        # not found is system -> trying ${DAB_LIBS_DIR}
        message (WARNING "libusb-1.0 not found in system, searching in ${DAB_LIBS_DIR}")

        find_library(LIBUSB_LINK_LIBRARIES usb-1.0 PATHS ${DAB_LIBS_DIR}/lib)

        if (LIBUSB_LINK_LIBRARIES)
            message (STATUS "libusb-1.0 found: ${LIBUSB_LINK_LIBRARIES}")
        else(LIBUSB_LINK_LIBRARIES)
            message (FATAL "libusb-1.0 not found. Build from source and install to: ${DAB_LIBS_DIR}")
        endif()
    endif()
else(USE_SYSTEM_LIBUSB)
    find_library(LIBUSB_LINK_LIBRARIES usb-1.0 PATHS ${DAB_LIBS_DIR}/lib NO_DEFAULT_PATH)
    if (LIBUSB_LINK_LIBRARIES)
        message (STATUS "libusb-1.0 found: ${LIBUSB_LINK_LIBRARIES}")
    else()
        message (FATAL_ERROR "libusb-1.0 not found. Build from source and install to: ${DAB_LIBS_DIR}")
    endif()
endif (USE_SYSTEM_LIBUSB)

#########################################################
## FDK-AAC
if (USE_FDK_AAC)
    if (USE_SYSTEM_FDKAAC)
        find_library (LIBFDKAAC_LINK_LIBRARIES fdk-aac)
        find_path(LIBFDKAAC_INCLUDE_DIRS fdk-aac/aacdecoder_lib.h)
        if (NOT WIN32)
            pkg_search_module (LIBFDKAAC fdk-aac)
        endif()
        if (NOT LIBFDKAAC_LINK_LIBRARIES)
            # not found is system -> trying ${DAB_LIBS_DIR}
            message (WARNING "libfdk-aac not found in system, searching in ${DAB_LIBS_DIR}")

            find_library(LIBFDKAAC_LINK_LIBRARIES fdk-aac PATHS ${DAB_LIBS_DIR}/lib)
            find_path(LIBFDKAAC_INCLUDE_DIRS fdk-aac/aacdecoder_lib.h PATHS ${DAB_LIBS_DIR}/include)

            if (LIBFDKAAC_LINK_LIBRARIES)
                message (STATUS "libfdk-aac found: ${LIBFDKAAC_LINK_LIBRARIES}")
                message (STATUS "libfdk-aac used instead of libfaad2 ${USE_FDK_AAC}")
                set(AUDIO_DECODER_USE_FDKAAC ON)
            else(LIBFDKAAC_LINK_LIBRARIES)
                message (WARNING "libfdk-aac not found. Build from source and install to: ${DAB_LIBS_DIR}. Trying to use faad2 instead.")
                set (USE_FDK_AAC OFF)
            endif()
        endif()
    else(USE_SYSTEM_FDKAAC)
        find_library(LIBFDKAAC_LINK_LIBRARIES fdk-aac PATHS ${DAB_LIBS_DIR}/lib NO_DEFAULT_PATH)
        find_path(LIBFDKAAC_INCLUDE_DIRS fdk-aac/aacdecoder_lib.h PATHS ${DAB_LIBS_DIR}/include NO_DEFAULT_PATH)
        if (LIBFDKAAC_LINK_LIBRARIES)
            message (STATUS "libfdk-aac found: ${LIBFDKAAC_LINK_LIBRARIES}")
            message (STATUS "libfdk-aac used instead of libfaad2 ${USE_FDK_AAC}")
            set(AUDIO_DECODER_USE_FDKAAC ON)
        else()
            message (WARNING "libfdk-aac not found. Build from source and install to: ${DAB_LIBS_DIR}. Trying to use faad2 instead.")
            set (USE_FDK_AAC OFF)
        endif()
    endif (USE_SYSTEM_FDKAAC)
endif (USE_FDK_AAC)

#########################################################
## FAAD2
# this can be fallback when FDK AAC not found
if (NOT USE_FDK_AAC)
    if (USE_SYSTEM_FAAD)
        find_library (LIBFAAD2_LINK_LIBRARIES faad)
        find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h)
        if (NOT WIN32)
            pkg_search_module (LIBFAAD2 faad)
        endif()
        if (NOT LIBFAAD2_LINK_LIBRARIES)
            # not found is system -> trying ${DAB_LIBS_DIR}
            message (WARNING "libfaad2 not found in system, searching in ${DAB_LIBS_DIR}")

            find_library(LIBFAAD2_LINK_LIBRARIES faad PATHS ${DAB_LIBS_DIR}/lib)
            find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h PATHS ${DAB_LIBS_DIR}/include)

            if (LIBFAAD2_LINK_LIBRARIES)
                message (STATUS "libfaad2 found: ${LIBFAAD2_LINK_LIBRARIES}")
            else(LIBFAAD2_LINK_LIBRARIES)
                message (FATAL "libfaad2 not found. Build from source and install to: ${DAB_LIBS_DIR}")
            endif()
        endif()
    else(USE_SYSTEM_FAAD)
        find_library(LIBFAAD2_LINK_LIBRARIES faad PATHS ${DAB_LIBS_DIR}/lib NO_DEFAULT_PATH)
        find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h PATHS ${DAB_LIBS_DIR}/include NO_DEFAULT_PATH)
        if (LIBFAAD2_LINK_LIBRARIES)
            message (STATUS "libfaad found: ${LIBFAAD2_LINK_LIBRARIES}")
        else()
            message (FATAL_ERROR "libfaad not found. Build from source and install to: ${DAB_LIBS_DIR}")
        endif()
    endif (USE_SYSTEM_FAAD)
endif(NOT USE_FDK_AAC)

#########################################################
## MPG123
if (USE_SYSTEM_MPG123)
    find_library (LIBMPG123_LINK_LIBRARIES mpg123)
    find_path(LIBMPG123_INCLUDE_DIRS mpg123.h)
    if (NOT WIN32)
        pkg_search_module (LIBMPG123 mpg123)
    endif()
    if (NOT LIBMPG123_LINK_LIBRARIES)
        # not found is system -> trying ${DAB_LIBS_DIR}
        message (WARNING "libmpg123 not found in system, searching in ${DAB_LIBS_DIR}")

        find_library(LIBMPG123_LINK_LIBRARIES mpg123 PATHS ${DAB_LIBS_DIR}/lib)
        find_path(LIBMPG123_INCLUDE_DIRS mpg123.h PATHS ${DAB_LIBS_DIR}/include)

        if (LIBMPG123_LINK_LIBRARIES)
            message (STATUS "libmpg123 found: ${LIBMPG123_LINK_LIBRARIES}")
        else(LIBMPG123_LINK_LIBRARIES)
            message (FATAL "libmpg123 not found. Build from source and install to: ${DAB_LIBS_DIR}")
        endif()
    endif()
else(USE_SYSTEM_MPG123)
    find_library(LIBMPG123_LINK_LIBRARIES mpg123 PATHS ${DAB_LIBS_DIR}/lib NO_DEFAULT_PATH)
    find_path(LIBMPG123_INCLUDE_DIRS mpg123.h PATHS ${DAB_LIBS_DIR}/include NO_DEFAULT_PATH)
    if (LIBMPG123_LINK_LIBRARIES)
        message (STATUS "libmpg123 found: ${LIBMPG123_LINK_LIBRARIES}")
    else()
        message (FATAL_ERROR "libmpg123 not found. Build from source and install to: ${DAB_LIBS_DIR}")
    endif()
endif (USE_SYSTEM_MPG123)

#########################################################
## PORTAUDIO
if (USE_SYSTEM_PORTAUDIO)
    find_library (PORTAUDIO_LINK_LIBRARIES portaudio)
    find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h)
    if (NOT WIN32)
        pkg_search_module(PORTAUDIO portaudio-2.0)
    endif()
    if (NOT PORTAUDIO_LINK_LIBRARIES)
        # not found is system -> trying ${DAB_LIBS_DIR}
        message (WARNING "portaudio not found in system, searching in ${DAB_LIBS_DIR}")

        find_library(PORTAUDIO_LINK_LIBRARIES portaudio PATHS ${DAB_LIBS_DIR}/lib)
        find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h PATHS ${DAB_LIBS_DIR}/include)

        if (PORTAUDIO_LINK_LIBRARIES)
            message (STATUS "libportaudio found: ${PORTAUDIO_LINK_LIBRARIES}")
        else(PORTAUDIO_LINK_LIBRARIES)
            message (FATAL "libportaudio not found. Build from source and install to: ${DAB_LIBS_DIR}")
        endif()
    endif()
else(USE_SYSTEM_PORTAUDIO)
    find_library(PORTAUDIO_LINK_LIBRARIES portaudio PATHS ${DAB_LIBS_DIR}/lib NO_DEFAULT_PATH)
    find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h PATHS ${DAB_LIBS_DIR}/include NO_DEFAULT_PATH)
    if (PORTAUDIO_LINK_LIBRARIES)
        message (STATUS "libportaudio found: ${PORTAUDIO_LINK_LIBRARIES}")
    else()
        message (FATAL_ERROR "libportaudio not found. Build from source and install to: ${DAB_LIBS_DIR}")
    endif()
endif (USE_SYSTEM_PORTAUDIO)

#########################################################
## QT creator CMAKEFILE
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 COMPONENTS Widgets Multimedia REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Multimedia REQUIRED)

#########################################################
## AbracaDABra GUI
add_subdirectory(gui)
