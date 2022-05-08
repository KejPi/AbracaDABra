
#########################################################
## Pictures
list (APPEND RESOURCES resources.qrc)
qt_add_resources (RCC_SOURCES ${RESOURCES})

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

configure_file (config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")

#########################################################
## SOURCES & HEADERS
add_executable(${TARGET}
    ${GUI_TYPE}
    ${RCC_SOURCES}
    ${APP_ICON_MACOSX}
    ${APP_ICON_RESOURCE_WINDOWS}
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    clickablelabel.h
    clickablelabel.cpp
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
    rtltcpinput.h
    rtltcpinput.cpp
    rarttcpinput.h
    rarttcpinput.cpp
    mscdatagroup.h
    mscdatagroup.cpp
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
    servicelistid.h
    servicelist.h
    servicelist.cpp
    ensemblelistitem.h
    ensemblelistitem.cpp
    servicelistitem.h
    servicelistitem.cpp

    slmodel.h
    slmodel.cpp
    slmodelitem.h
    slmodelitem.cpp
    sltreemodel.h
    sltreemodel.cpp
    setupdialog.h
    setupdialog.cpp
    setupdialog.ui
    bandscandialog.h
    bandscandialog.cpp
    bandscandialog.ui

    ensembleinfodialog.h
    ensembleinfodialog.cpp
    ensembleinfodialog.ui

    userapplication.h
    userapplication.cpp
    slideshowapp.h
    slideshowapp.cpp
    spiapp.h
    spiapp.cpp

    slsview.h
    slsview.cpp

    catslsdialog.h
    catslsdialog.cpp
    catslsdialog.ui

    aboutdialog.h
    aboutdialog.cpp
    aboutdialog.ui
  )

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#add_definitions(-fsanitize=undefined)
#target_link_options(${TARGET} PUBLIC -fsanitize=undefined)
#add_definitions(-fsanitize=thread)
#target_link_options(${TARGET} PUBLIC -fsanitize=thread)


# config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

### DAB library
include_directories ( ${DAB_INCLUDE_DIRS} )
target_link_libraries(${TARGET} PRIVATE ${DAB_LINK_LIBRARIES})

# AAC libs
if (USE_FDK_AAC)
    # FDKAAC
    include_directories ( ${LIBFDKAAC_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${LIBFDKAAC_LINK_LIBRARIES}" )
else (USE_FDK_AAC)
    # FAAD
    include_directories ( ${LIBFAAD2_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${LIBFAAD2_LINK_LIBRARIES}" )
endif (USE_FDK_AAC)

# MPG123
include_directories ( ${LIBMPG123_INCLUDE_DIRS} )
target_link_libraries(${TARGET} PRIVATE "${LIBMPG123_LINK_LIBRARIES}" )

# RTLSDR
include_directories ( ${RTL_SDR_INCLUDE_DIRS} )
target_link_libraries(${TARGET} PRIVATE "${RTL_SDR_LINK_LIBRARIES}" )

# PORTAUDIO
include_directories ( ${PORTAUDIO_INCLUDE_DIRS} )
target_link_libraries(${TARGET} PRIVATE "${PORTAUDIO_LINK_LIBRARIES}" )

# Set some Win32 Specific Settings
if(WIN32)
    # required fro sockets
    target_link_libraries(${TARGET} PRIVATE ws2_32)
endif(WIN32)

include_directories ( ${CMAKE_SOURCE_DIR} )
target_link_libraries(${TARGET} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)

# Set a custom plist file for the app bundle
if(APPLE)
    set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in)
endif(APPLE)
