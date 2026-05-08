cmake_minimum_required(VERSION 3.15)
include (CheckLibraryExists)
include (CheckSymbolExists)
if(NOT WIN32)
    include (FindPkgConfig)
endif()

# set(CMAKE_VERBOSE_MAKEFILE On)

#########################################################
## OPTIONAL SWITCHES

# AAC decoder - default is FAAD2 but FDK-AAC can be used optionally
option (USE_FDKAAC            "Compile with fdk-aac library from Franhofer instead of faad2" OFF)

# Audio output
option (USE_PORTAUDIO         "Compile with PortAudio library instead of Qt6 multimedia framework (better performance)" ON)

# Options to force using libs build manually and installed in ${CMAKE_SOURCE_DIR}/../../dab-libs
option (USE_SYSTEM_RTLSDR       "Use system provided rtl-sdr"         ON)
option (USE_SYSTEM_LIBUSB       "Use system provided libusb"          ON)
option (USE_SYSTEM_FAAD         "Use system provided libfaad"         ON)
option (USE_SYSTEM_FDKAAC       "Use system provided libfdk-aac"      ON)
option (USE_SYSTEM_MPG123       "Use system provided libmpg123"       ON)
option (USE_SYSTEM_PORTAUDIO    "Use system provided portaudio"       ON)
option (USE_SYSTEM_AIRSPY       "Use system provided airspy lib"      ON)
option (USE_SYSTEM_SOAPYSDR     "Use system provided SoapySDR lib"    ON)

# MacOS
option (APPLE_APP_BUNDLE      "Enable app bundle on macOS"            ON)
option (APPLE_BUILD_X86_64    "Build x86_64 binary for Apple"        OFF)

# Devices
option (AIRSPY                "Enable AirSpy devices"                OFF)
option (SOAPYSDR              "Enable Soapy SDR devices"             OFF)
option (RARTTCP               "Enable RaRT TCP device"               OFF)

# FMLIST
option (FMLIST                "Enable FMLIST interface"               ON)

# Android OpenSSL
set(ANDROID_OPENSSL_DIR "" CACHE PATH "Path to Android OpenSSL cmake directory (e.g., /path/to/android_openssl). If not set, will auto-derive from ANDROID_SDK_ROOT")

# Git submodule
option(GIT_SUBMODULE          "Check submodules during build"         ON)

# Apple specific
# if(APPLE)
#     # Qt 6.8.0 requires at least macOS 12 so this does not work
#     if(APPLE_BUILD_X86_64)
#         # Intel build
#         set(CMAKE_OSX_DEPLOYMENT_TARGET "11" CACHE STRING "Minimum OS X deployment version")
#     else()
#         # AARCH64 build
#         set(CMAKE_OSX_DEPLOYMENT_TARGET "11" CACHE STRING "Minimum OS X deployment version")
#     endif()
# endif()

## Project
set(TARGET AbracaDABra)

project(${TARGET} LANGUAGES CXX)

#########################################################
## VERSION
execute_process(COMMAND git describe
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE PROJECT_GIT_VERSION
                ERROR_QUIET)
# Check whether we got any revision
if ("${PROJECT_GIT_VERSION}" STREQUAL "")
    # load version FROM VERSION FILE
    file (STRINGS "APP_VERSION" PROJECT_GIT_VERSION)
endif()

# parse version
string(STRIP ${PROJECT_GIT_VERSION} PROJECT_GIT_VERSION)
if(PROJECT_GIT_VERSION MATCHES "^v([0-9]+).([0-9]+).([0-9]+)(-([0-9]+)-g[0-9A-Za-z]+)?" )
    set( VER_MAJOR "${CMAKE_MATCH_1}" )
    set( VER_MINOR "${CMAKE_MATCH_2}" )
    set( VER_PATCH "${CMAKE_MATCH_3}" )
    if(CMAKE_MATCH_5)
        set( VER_GIT "${CMAKE_MATCH_5}" )
    else()
        set( VER_GIT "0")
    endif()
    message( STATUS "Version: ${PROJECT_GIT_VERSION}" )
else()
    message( FATAL_ERROR "Version not valid: [${PROJECT_GIT_VERSION}]" )
endif()

#########################################################


# Select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

if(WIN32 OR (APPLE AND APPLE_APP_BUNDLE) OR ANDROID)
    # Apple, Win and Android do not use system libs
    # - M1 seems to have problems with that https://forum.qt.io/topic/135220/random-application-crash-on-apple-silicon-m1-qt-6-2-3/24
    # - Windows has always some problems
    # - Android requires cross-compiled libraries
    set(USE_SYSTEM_RTLSDR       OFF)
    set(USE_SYSTEM_LIBUSB       OFF)
    set(USE_SYSTEM_FAAD         OFF)
    set(USE_SYSTEM_FDKAAC       OFF)
    set(USE_SYSTEM_MPG123       OFF)
    set(USE_SYSTEM_PORTAUDIO    OFF)
    set(USE_SYSTEM_AIRSPY       OFF)
    set(USE_SYSTEM_SOAPYSDR     OFF)
endif()

# Android-specific settings
if(ANDROID)
    # Disable features not supported/needed on Android
    set(USE_PORTAUDIO OFF)
    set(SOAPYSDR OFF)
    set(RARTTCP OFF)
    
    # For Android cross-compilation, add external libs to CMAKE_FIND_ROOT_PATH
    # This allows find_library to locate Android ARM libraries
    list(APPEND CMAKE_FIND_ROOT_PATH ${EXTERNAL_LIBS_DIR})
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
endif()


if (APPLE_BUILD_X86_64)
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
endif()

#########################################################
## DAB library (backend)
if(EXISTS "${PROJECT_SOURCE_DIR}/dabsdr/CMakeLists.txt" AND NOT PROJECT_VERSION_RELEASE)
    message (STATUS "libdabsdr will be built from source")
    set(DABSDR_DIR "${PROJECT_SOURCE_DIR}/dabsdr")

    # dab library source code available
    add_subdirectory(${DABSDR_DIR} dabsdr)

    set(DAB_LINK_LIBRARIES dabsdr)
    set(DAB_INCLUDE_DIRS ${DABSDR_DIR}/src ${PROJECT_BINARY_DIR}/dabsdr/src)
else()
    # dab library
    if (APPLE)
        if(CMAKE_OSX_ARCHITECTURES MATCHES "^x86")
            string(TOLOWER "${CMAKE_SYSTEM_NAME}_x86_64" PLATFORM)
        elseif(CMAKE_OSX_ARCHITECTURES MATCHES "^arm")
            string(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_HOST_SYSTEM_PROCESSOR}" PLATFORM)
        else()
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
                string(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_HOST_SYSTEM_PROCESSOR}" PLATFORM)
            else()
                string(TOLOWER "${CMAKE_SYSTEM_NAME}_x86_64" PLATFORM)
            endif()
        endif()
    else()
        if (ANDROID)
            string(TOLOWER "android_arm64-v8a" PLATFORM)
        else()
            string(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_HOST_SYSTEM_PROCESSOR}" PLATFORM)
        endif()
    endif()
    set(DABSDR_DIR "${PROJECT_SOURCE_DIR}/lib")

    if (EXISTS "${DABSDR_DIR}/${PLATFORM}/cmake/dabsdr.cmake")
        message (STATUS "libdabsdr found: ${DABSDR_DIR}/${PLATFORM}/cmake/dabsdr.cmake")
        include(${DABSDR_DIR}/${PLATFORM}/cmake/dabsdr.cmake)
        set(DAB_LINK_LIBRARIES dabsdr::dabsdr)
    else()
        message (FATAL_ERROR "libdabsdr not found.")
    endif()
endif()
###

#########################################################
## FMLIST library (backend)
if(FMLIST)
  if(EXISTS "${PROJECT_SOURCE_DIR}/fmlist/CMakeLists.txt" AND NOT PROJECT_VERSION_RELEASE)
      message (STATUS "libfmlist will be built from source")
      set(FMLIST_DIR "${PROJECT_SOURCE_DIR}/fmlist")

      # fmlist library source code available
      add_subdirectory(${FMLIST_DIR} fmlist)

      set(FMLIST_LINK_LIBRARIES fmlistInterface)
      set(FMLIST_INCLUDE_DIRS ${FMLIST_DIR}/src ${PROJECT_BINARY_DIR}/fmlist/src)

      set(HAVE_FMLIST_INTERFACE ON)
  else()
      # fmlist library
      if (APPLE)
          if(CMAKE_OSX_ARCHITECTURES MATCHES "^x86")
              string(TOLOWER "${CMAKE_SYSTEM_NAME}_x86_64" PLATFORM)
          elseif(CMAKE_OSX_ARCHITECTURES MATCHES "^arm")
              string(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_HOST_SYSTEM_PROCESSOR}" PLATFORM)
          else()
              if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
                  string(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_HOST_SYSTEM_PROCESSOR}" PLATFORM)
              else()
                  string(TOLOWER "${CMAKE_SYSTEM_NAME}_x86_64" PLATFORM)
              endif()
          endif()
      else()
            if (ANDROID)
                string(TOLOWER "android_arm64-v8a" PLATFORM)
            else()
                string(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_HOST_SYSTEM_PROCESSOR}" PLATFORM)
            endif()
      endif()
      set(FMLIST_DIR "${PROJECT_SOURCE_DIR}/lib")

      if (EXISTS "${FMLIST_DIR}/${PLATFORM}/cmake/fmlistInterface.cmake")
          message (STATUS "libfmlist found: ${FMLIST_DIR}/${PLATFORM}/cmake/fmlistInterface.cmake")
          include(${FMLIST_DIR}/${PLATFORM}/cmake/fmlistInterface.cmake)
          set(FMLIST_LINK_LIBRARIES fmlistInterface::fmlistInterface)
          set(HAVE_FMLIST_INTERFACE ON)
      else()
          set(HAVE_FMLIST_INTERFACE OFF)
          message (STATUS "libfmlistInterface not found.")
      endif()
  endif()
else(FMLIST)
  set(HAVE_FMLIST_INTERFACE OFF)
  message (STATUS "libfmlistInterface disabled.")
endif()

###


#########################################################
## External libs
if (NOT EXTERNAL_LIBS_DIR)
    set (EXTERNAL_LIBS_DIR "${PROJECT_SOURCE_DIR}/../${TARGET}-libs")
endif()

message(STATUS "External libs directory: ${EXTERNAL_LIBS_DIR}")

#########################################################
## RTL-SDR
if (USE_SYSTEM_RTLSDR)
    find_library (RTL_SDR_LINK_LIBRARIES rtlsdr)
    find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h)
    if (NOT WIN32)
        pkg_search_module (RTL_SDR rtlsdr)
    endif()
    if (NOT RTL_SDR_LINK_LIBRARIES)
        # not found is system -> trying ${EXTERNAL_LIBS_DIR}
        message (WARNING "librtlsdr not found in system, searching in ${EXTERNAL_LIBS_DIR}")

        find_library(RTL_SDR_LINK_LIBRARIES rtlsdr PATHS ${EXTERNAL_LIBS_DIR}/lib)
        find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h PATHS ${EXTERNAL_LIBS_DIR}/include)

        if (RTL_SDR_LINK_LIBRARIES)
            message (STATUS "librtlsdr found: ${RTL_SDR_LINK_LIBRARIES}")
        else(RTL_SDR_LINK_LIBRARIES)
            message (FATAL "librtlsdr not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    endif()
else(USE_SYSTEM_RTLSDR)
    if(ANDROID)
        # For Android, directly set the library path since find_library may fail with cross-compiled libs
        if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/librtlsdr.so")
            set(RTL_SDR_LINK_LIBRARIES "${EXTERNAL_LIBS_DIR}/lib/librtlsdr.so")
            set(RTL_SDR_INCLUDE_DIRS "${EXTERNAL_LIBS_DIR}/include")
            message(STATUS "librtlsdr found (Android): ${RTL_SDR_LINK_LIBRARIES}")
        else()
            message(FATAL_ERROR "librtlsdr not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    else()
        find_library(RTL_SDR_LINK_LIBRARIES rtlsdr PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
        find_path(RTL_SDR_INCLUDE_DIRS rtl-sdr.h PATHS ${EXTERNAL_LIBS_DIR}/include NO_DEFAULT_PATH)
        if (RTL_SDR_LINK_LIBRARIES)
            message (STATUS "librtlsdr found: ${RTL_SDR_LINK_LIBRARIES}")
        else()
            message (FATAL_ERROR "librtlsdr not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
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
        # not found is system -> trying ${EXTERNAL_LIBS_DIR}
        message (WARNING "libusb-1.0 not found in system, searching in ${EXTERNAL_LIBS_DIR}")

        find_library(LIBUSB_LINK_LIBRARIES usb-1.0 PATHS ${EXTERNAL_LIBS_DIR}/lib)

        if (LIBUSB_LINK_LIBRARIES)
            message (STATUS "libusb-1.0 found: ${LIBUSB_LINK_LIBRARIES}")
        else(LIBUSB_LINK_LIBRARIES)
            message (FATAL "libusb-1.0 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    endif()
else(USE_SYSTEM_LIBUSB)
    if(ANDROID)
        # For Android, directly set the library path since find_library may fail with cross-compiled libs
        if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/libusb1.0.so")
            set(LIBUSB_LINK_LIBRARIES "${EXTERNAL_LIBS_DIR}/lib/libusb1.0.so")
            message(STATUS "libusb-1.0 found (Android): ${LIBUSB_LINK_LIBRARIES}")
        else()
            message(FATAL_ERROR "libusb-1.0 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    else()
        find_library(LIBUSB_LINK_LIBRARIES usb-1.0 PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
        if (LIBUSB_LINK_LIBRARIES)
            message (STATUS "libusb-1.0 found: ${LIBUSB_LINK_LIBRARIES}")
        else()
            message (FATAL_ERROR "libusb-1.0 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    endif()
endif (USE_SYSTEM_LIBUSB)

#########################################################
## FDK-AAC
if (USE_FDKAAC)
  if (USE_SYSTEM_FDKAAC)
      find_library (LIBFDKAAC_LINK_LIBRARIES fdk-aac)
      find_path(LIBFDKAAC_INCLUDE_DIRS fdk-aac/aacdecoder_lib.h)
      if (NOT WIN32)
          pkg_search_module (LIBFDKAAC fdk-aac)
      endif()
      if (NOT LIBFDKAAC_LINK_LIBRARIES)
          # not found is system -> trying ${EXTERNAL_LIBS_DIR}
          message (WARNING "libfdk-aac not found in system, searching in ${EXTERNAL_LIBS_DIR}")

          find_library(LIBFDKAAC_LINK_LIBRARIES fdk-aac PATHS ${EXTERNAL_LIBS_DIR}/lib)
          find_path(LIBFDKAAC_INCLUDE_DIRS fdk-aac/aacdecoder_lib.h PATHS ${EXTERNAL_LIBS_DIR}/include)

          if (LIBFDKAAC_LINK_LIBRARIES)
              message (STATUS "libfdk-aac found: ${LIBFDKAAC_LINK_LIBRARIES}")
              set(HAVE_FDKAAC ON)
          else(LIBFDKAAC_LINK_LIBRARIES)
              message (STATUS "libfdk-aac not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}. Trying to use faad2 instead.")
              set (HAVE_FDKAAC OFF)
          endif()
      else()
          # found in system
          set(HAVE_FDKAAC ON)
      endif()
  else(USE_SYSTEM_FDKAAC)
      find_library(LIBFDKAAC_LINK_LIBRARIES fdk-aac PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
      find_path(LIBFDKAAC_INCLUDE_DIRS fdk-aac/aacdecoder_lib.h PATHS ${EXTERNAL_LIBS_DIR}/include NO_DEFAULT_PATH)
      if (LIBFDKAAC_LINK_LIBRARIES)
          message (STATUS "libfdk-aac found: ${LIBFDKAAC_LINK_LIBRARIES}")
          set(HAVE_FDKAAC ON)
      else()
          message (STATUS "libfdk-aac not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}. Trying to use faad2 instead.")
          set (HAVE_FDKAAC OFF)
      endif()
  endif (USE_SYSTEM_FDKAAC)
else(USE_FDKAAC)
  set (HAVE_FDKAAC OFF)
endif(USE_FDKAAC)

#########################################################
## FAAD2
if (USE_SYSTEM_FAAD)
    find_library (LIBFAAD2_LINK_LIBRARIES faad)
    find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h)
    if (NOT WIN32)
        pkg_search_module (LIBFAAD2 faad)
    endif()
    if (NOT LIBFAAD2_LINK_LIBRARIES)
        # not found is system -> trying ${EXTERNAL_LIBS_DIR}
        message (WARNING "libfaad2 not found in system, searching in ${EXTERNAL_LIBS_DIR}")

        find_library(LIBFAAD2_LINK_LIBRARIES faad PATHS ${EXTERNAL_LIBS_DIR}/lib)
        find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h PATHS ${EXTERNAL_LIBS_DIR}/include)

        if (LIBFAAD2_LINK_LIBRARIES)
            message (STATUS "libfaad2 found: ${LIBFAAD2_LINK_LIBRARIES}")
            set (HAVE_FAAD ON)
        else(LIBFAAD2_LINK_LIBRARIES)
            message (STATUS "libfaad2 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
            set (HAVE_FAAD OFF)
        endif()
    else()
        set (HAVE_FAAD ON)
    endif()
else(USE_SYSTEM_FAAD)
    if(ANDROID)
        # For Android, directly set the library path since find_library may fail with cross-compiled libs
        if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/libfaad.so")
            set(LIBFAAD2_LINK_LIBRARIES "${EXTERNAL_LIBS_DIR}/lib/libfaad.so")
            set(LIBFAAD2_INCLUDE_DIRS "${EXTERNAL_LIBS_DIR}/include")
            message(STATUS "libfaad found (Android): ${LIBFAAD2_LINK_LIBRARIES}")
            set(HAVE_FAAD ON)
        else()
            message(STATUS "libfaad2 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
            set(HAVE_FAAD OFF)
        endif()
    else()
        find_library(LIBFAAD2_LINK_LIBRARIES faad PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
        find_path(LIBFAAD2_INCLUDE_DIRS neaacdec.h PATHS ${EXTERNAL_LIBS_DIR}/include NO_DEFAULT_PATH)
        if (LIBFAAD2_LINK_LIBRARIES)
            message (STATUS "libfaad found: ${LIBFAAD2_LINK_LIBRARIES}")
            set (HAVE_FAAD ON)
        else()
            message (STATUS "libfaad2 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
            set (HAVE_FAAD OFF)
        endif()
    endif()
endif (USE_SYSTEM_FAAD)

# Check if we have at least one audio decoder
if ((NOT HAVE_FAAD) AND (NOT HAVE_FDKAAC))
    message (FATAL_ERROR "Neither FAAD nor FDKAAC library found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
endif()

#########################################################
## MPG123
if (USE_SYSTEM_MPG123)
    find_library (LIBMPG123_LINK_LIBRARIES mpg123)
    find_path(LIBMPG123_INCLUDE_DIRS mpg123.h)
    if (NOT WIN32)
        pkg_search_module (LIBMPG123 mpg123)
    endif()
    if (NOT LIBMPG123_LINK_LIBRARIES)
        # not found is system -> trying ${EXTERNAL_LIBS_DIR}
        message (WARNING "libmpg123 not found in system, searching in ${EXTERNAL_LIBS_DIR}")

        find_library(LIBMPG123_LINK_LIBRARIES mpg123 PATHS ${EXTERNAL_LIBS_DIR}/lib)
        find_path(LIBMPG123_INCLUDE_DIRS mpg123.h PATHS ${EXTERNAL_LIBS_DIR}/include)

        if (LIBMPG123_LINK_LIBRARIES)
            message (STATUS "libmpg123 found: ${LIBMPG123_LINK_LIBRARIES}")
        else(LIBMPG123_LINK_LIBRARIES)
            message (FATAL "libmpg123 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    endif()
else(USE_SYSTEM_MPG123)
    if(ANDROID)
        # For Android, directly set the library path since find_library may fail with cross-compiled libs
        if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/libmpg123.so")
            set(LIBMPG123_LINK_LIBRARIES "${EXTERNAL_LIBS_DIR}/lib/libmpg123.so")
            set(LIBMPG123_INCLUDE_DIRS "${EXTERNAL_LIBS_DIR}/include")
            message(STATUS "libmpg123 found (Android): ${LIBMPG123_LINK_LIBRARIES}")
        else()
            message(FATAL_ERROR "libmpg123 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    else()
        find_library(LIBMPG123_LINK_LIBRARIES mpg123 PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
        find_path(LIBMPG123_INCLUDE_DIRS mpg123.h PATHS ${EXTERNAL_LIBS_DIR}/include NO_DEFAULT_PATH)
        if (LIBMPG123_LINK_LIBRARIES)
            message (STATUS "libmpg123 found: ${LIBMPG123_LINK_LIBRARIES}")
        else()
            message (FATAL_ERROR "libmpg123 not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}")
        endif()
    endif()
endif (USE_SYSTEM_MPG123)

#########################################################
## PORTAUDIO
if (USE_PORTAUDIO)
    if (USE_SYSTEM_PORTAUDIO)
        find_library (PORTAUDIO_LINK_LIBRARIES portaudio)
        find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h)
        if (NOT WIN32)
            pkg_search_module(PORTAUDIO portaudio-2.0)
        endif()
        if (NOT PORTAUDIO_LINK_LIBRARIES)
            # not found is system -> trying ${EXTERNAL_LIBS_DIR}
            message (WARNING "portaudio not found in system, searching in ${EXTERNAL_LIBS_DIR}")

            find_library(PORTAUDIO_LINK_LIBRARIES portaudio PATHS ${EXTERNAL_LIBS_DIR}/lib)
            find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h PATHS ${EXTERNAL_LIBS_DIR}/include)

            if (PORTAUDIO_LINK_LIBRARIES)
                message (STATUS "libportaudio found: ${PORTAUDIO_LINK_LIBRARIES}")
                set(HAVE_PORTAUDIO ON)
            else(PORTAUDIO_LINK_LIBRARIES)
                message (STATUS "libportaudio not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}. Using Qt6 multimedia framework instead.")
            endif()
        else()
            # found in system
            set(HAVE_PORTAUDIO ON)
        endif()
    else(USE_SYSTEM_PORTAUDIO)
        find_library(PORTAUDIO_LINK_LIBRARIES portaudio PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
        find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h PATHS ${EXTERNAL_LIBS_DIR}/include NO_DEFAULT_PATH)
        if (PORTAUDIO_LINK_LIBRARIES)
            message (STATUS "libportaudio found: ${PORTAUDIO_LINK_LIBRARIES}")
            set(HAVE_PORTAUDIO ON)
        else()
            message (STATUS "libportaudio not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}. Using Qt6 multimedia framework instead.")
        endif()
    endif (USE_SYSTEM_PORTAUDIO)
endif(USE_PORTAUDIO)

#########################################################
## AIRSPY
if (AIRSPY)
    if (USE_SYSTEM_AIRSPY)
        find_library (AIRSPY_LINK_LIBRARIES airspy)
        find_path(AIRSPY_INCLUDE_DIRS libairspy/airspy.h)
        if (NOT WIN32)
            pkg_search_module(AIRSPY airspy)
        endif()
        if (NOT AIRSPY_LINK_LIBRARIES)
            # not found is system -> trying ${EXTERNAL_LIBS_DIR}
            message (WARNING "airspy not found in system, searching in ${EXTERNAL_LIBS_DIR}")

            find_library(AIRSPY_LINK_LIBRARIES airspy PATHS ${EXTERNAL_LIBS_DIR}/lib)
            find_path(AIRSPY_INCLUDE_DIRS libairspy/airspy.h PATHS ${EXTERNAL_LIBS_DIR}/include)

            if (AIRSPY_LINK_LIBRARIES)
                message (STATUS "libairspy found: ${AIRSPY_LINK_LIBRARIES}")
                set(HAVE_AIRSPY ON)
            else(AIRSPY_LINK_LIBRARIES)
                message (STATUS "libairspy not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}.")
                set(HAVE_AIRSPY OFF)
            endif()
        else()
            # found in system
            set(HAVE_AIRSPY ON)
        endif()
    else(USE_SYSTEM_AIRSPY)
        if(ANDROID)
            # For Android, directly set the library path since find_library may fail with cross-compiled libs
            if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/libairspy.so")
                set(AIRSPY_LINK_LIBRARIES "${EXTERNAL_LIBS_DIR}/lib/libairspy.so")
                set(AIRSPY_INCLUDE_DIRS "${EXTERNAL_LIBS_DIR}/include")
                message(STATUS "libairspy found (Android): ${AIRSPY_LINK_LIBRARIES}")
                set(HAVE_AIRSPY ON)
            else()
                message(STATUS "libairspy not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}.")
                set(HAVE_AIRSPY OFF)
            endif()
        else()
            find_library(AIRSPY_LINK_LIBRARIES airspy PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
            find_path(AIRSPY_INCLUDE_DIRS libairspy/airspy.h PATHS ${EXTERNAL_LIBS_DIR}/include NO_DEFAULT_PATH)
            if (AIRSPY_LINK_LIBRARIES)
                message (STATUS "libairspy found: ${AIRSPY_LINK_LIBRARIES}")
                set(HAVE_AIRSPY ON)
            else()
                message (STATUS "libairspy not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}.")
                set(HAVE_AIRSPY OFF)
            endif()
        endif()
    endif (USE_SYSTEM_AIRSPY)
else(AIRSPY)
    set(HAVE_AIRSPY OFF)
endif(AIRSPY)

#########################################################
## SOAPYSDR
if (SOAPYSDR)
    if (USE_SYSTEM_SOAPYSDR)
        find_library (SOAPYSDR_LINK_LIBRARIES SoapySDR)
        find_path(SOAPYSDR_INCLUDE_DIRS SoapySDR/Device.hpp)
        if (NOT WIN32)
            pkg_search_module(SOAPYSDR SoapySDR)
        endif()
        if (NOT SOAPYSDR_LINK_LIBRARIES)
            # not found is system -> trying ${EXTERNAL_LIBS_DIR}
            message (WARNING "libSoapySDR not found in system, searching in ${EXTERNAL_LIBS_DIR}")

            find_library(SOAPYSDR_LINK_LIBRARIES SoapySDR PATHS ${EXTERNAL_LIBS_DIR}/lib)
            find_path(SOAPYSDR_INCLUDE_DIRS SoapySDR/Device.hpp PATHS ${EXTERNAL_LIBS_DIR}/include)

            if (SOAPYSDR_LINK_LIBRARIES)
                message (STATUS "libSoapySDR found: ${SOAPYSDR_LINK_LIBRARIES}")
                set(HAVE_SOAPYSDR ON)
            else(AIRSPY_LINK_LIBRARIES)
                message (STATUS "libSoapySDR not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}.")
                set(HAVE_SOAPYSDR OFF)
            endif()
        else()
            # found in system
            set(HAVE_SOAPYSDR ON)
        endif()
    else(USE_SYSTEM_SOAPYSDR)
        find_library(SOAPYSDR_LINK_LIBRARIES SoapySDR PATHS ${EXTERNAL_LIBS_DIR}/lib NO_DEFAULT_PATH)
        find_path(SOAPYSDR_INCLUDE_DIRS SoapySDR/Device.hpp PATHS ${EXTERNAL_LIBS_DIR}/include NO_DEFAULT_PATH)
        if (SOAPYSDR_LINK_LIBRARIES)
            message (STATUS "libSoapySDR found: ${SOAPYSDR_LINK_LIBRARIES}")
            set(HAVE_SOAPYSDR ON)
        else()
            message (STATUS "libSoapySDR not found. Build from source and install to: ${EXTERNAL_LIBS_DIR}.")
            set(HAVE_SOAPYSDR OFF)
        endif()
    endif (USE_SYSTEM_SOAPYSDR)
else(SOAPYSDR)
    set(HAVE_SOAPYSDR OFF)
endif(SOAPYSDR)

#########################################################
## RARTTCP
if (RARTTCP)
    # no specific check here
    set(HAVE_RARTTCP ON)
else(RARTTCP)
    set(HAVE_RARTTCP OFF)
endif(RARTTCP)

#########################################################
# QT
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 COMPONENTS Multimedia Svg Xml Network Quick Qml QuickControls2 Positioning Location Concurrent REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Multimedia Svg Xml Network Quick Qml QuickControls2 QmlWorkerScript Positioning Location Concurrent REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS LinguistTools)

set(HAVE_LINUX_DBUS OFF)

# Widgets and DBus
if(ANDROID)
    set(HAVE_QTWIDGETS OFF)
else()
    # this is required fro dock menu on macos
    set(HAVE_QTWIDGETS ON)
    find_package(QT NAMES Qt6 COMPONENTS Widgets REQUIRED)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)    

    if(UNIX AND NOT APPLE AND NOT ANDROID)
        find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS DBus)
        if(Qt${QT_VERSION_MAJOR}DBus_FOUND)
            set(HAVE_LINUX_DBUS ON)
        endif()
    endif()    
endif()

# Android OpenSSL integration (must be after Qt packages are found)
if(ANDROID)
    # Auto-derive ANDROID_OPENSSL_DIR from ANDROID_SDK_ROOT if not explicitly set
    if(NOT ANDROID_OPENSSL_DIR OR ANDROID_OPENSSL_DIR STREQUAL "")
        if(DEFINED ENV{ANDROID_SDK_ROOT})
            set(ANDROID_OPENSSL_DIR "$ENV{ANDROID_SDK_ROOT}/android_openssl")
        elseif(ANDROID_SDK_ROOT)
            set(ANDROID_OPENSSL_DIR "${ANDROID_SDK_ROOT}/android_openssl")
        endif()
    endif()
    
    if(ANDROID_OPENSSL_DIR AND EXISTS "${ANDROID_OPENSSL_DIR}/android_openssl.cmake")
        message(STATUS "Including Android OpenSSL from: ${ANDROID_OPENSSL_DIR}")
        # Ensure Qt6_VERSION is available for android_openssl.cmake version detection
        if(NOT DEFINED Qt6_VERSION AND DEFINED Qt6Core_VERSION)
            set(Qt6_VERSION ${Qt6Core_VERSION})
        endif()
        include(${ANDROID_OPENSSL_DIR}/android_openssl.cmake)
        message(STATUS "Android OpenSSL: Using SSL version ${ssl_version_dir} from ${ssl_root_path}")
    elseif(ANDROID_OPENSSL_DIR)
        message(WARNING "ANDROID_OPENSSL_DIR specified but android_openssl.cmake not found at: ${ANDROID_OPENSSL_DIR}")
    else()
        message(STATUS "ANDROID_OPENSSL_DIR not found. To enable SSL support, ensure android_openssl is in your ANDROID_SDK_ROOT or pass: -DANDROID_OPENSSL_DIR=/path/to/android_openssl")
    endif()
endif()

qt_standard_project_setup(REQUIRES 6.7)

#########################################################
## AbracaDABra GUI
add_subdirectory(src)

## This is required for localization permissions to work
## For Android, finalization is handled in src/CMakeLists.txt due to directory scope requirements
if(NOT ANDROID)
    qt_finalize_executable(${TARGET})
endif()

#########################################################
## Install
if(ANDROID)
    # Android uses APK deployment, no traditional install needed
    # The APK is built via androiddeployqt
elseif(APPLE AND APPLE_APP_BUNDLE)
    install(TARGETS ${TARGET} BUNDLE DESTINATION /Applications)
else ()
    install(TARGETS ${TARGET})
    #install(FILES ${qm_files} DESTINATION ${INSTALL_EXAMPLEDIR})

    if(UNIX AND NOT APPLE)        
        include(GNUInstallDirs)
        INSTALL (FILES ${PROJECT_SOURCE_DIR}/src/resources/AbracaDABra.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/ RENAME ${TARGET}.desktop)
        INSTALL (FILES ${PROJECT_SOURCE_DIR}/src/resources/appIcon-linux-512x512.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps/ RENAME ${TARGET}.png)
    endif()
endif()
if(NOT ANDROID AND (NOT EXISTS "${PROJECT_SOURCE_DIR}/dabsdr/CMakeLists.txt" OR PROJECT_VERSION_RELEASE))
    install(IMPORTED_RUNTIME_ARTIFACTS dabsdr::dabsdr)
endif()
if(NOT ANDROID AND HAVE_FMLIST_INTERFACE AND (NOT EXISTS "${PROJECT_SOURCE_DIR}/fmlist/CMakeLists.txt" OR PROJECT_VERSION_RELEASE))
    install(IMPORTED_RUNTIME_ARTIFACTS fmlistInterface::fmlistInterface)
endif()

#########################################################
# Uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

