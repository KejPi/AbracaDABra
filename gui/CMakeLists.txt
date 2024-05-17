# Debug options
option (TII_SPECTRUM_PLOT "Enable spectrum plot for TII"  OFF)


#########################################################
## Pictures
list (APPEND RESOURCES resources.qrc)
qt_add_resources (RCC_SOURCES ${RESOURCES})

# Set some Win32 Specific Settings
if(WIN32)
    set(GUI_TYPE WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/appIcon.rc")
    set(APP_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    set_source_files_properties(${APP_RESOURCE_WINDOWS} PROPERTIES LANGUAGE RC)
endif(WIN32)

# Set some Apple MacOS Specific settings
if (APPLE)
    if (APPLE_APP_BUNDLE)
        set(GUI_TYPE MACOSX_BUNDLE)

        # NOTE: Don't include the path in MACOSX_BUNDLE_ICON_FILE -- this is
        # the property added to Info.plist
        set(MACOSX_BUNDLE_ICON_FILE appIcon.icns)

        # And this part tells CMake where to find and install the file itself
        set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/resources/appIcon.icns)
        set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES
               MACOSX_PACKAGE_LOCATION "Resources")
    endif(APPLE_APP_BUNDLE)

    set(APPLE_SOURCES
        mac.mm
        mac.h)
endif(APPLE)

configure_file (config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")

if (HAVE_AIRSPY)
    set(AIRSPY_SOURCES input/airspyinput.h input/airspyinput.cpp)
endif(HAVE_AIRSPY)
if (HAVE_SOAPYSDR)
    set(SOAPYSDR_SOURCES input/soapysdrinput.h input/soapysdrinput.cpp)
endif(HAVE_SOAPYSDR)
if (HAVE_PORTAUDIO)
    set(PORTAUDIO_SOURCES audiooutputpa.h audiooutputpa.cpp)
endif(HAVE_PORTAUDIO)
if (HAVE_QCUSTOMPLOT)
    set(QCUSTOMPLOT_SOURCES
        snrplotdialog.h snrplotdialog.cpp snrplotdialog.ui
    )
endif(HAVE_QCUSTOMPLOT)


#########################################################
## SOURCES & HEADERS
add_executable(${TARGET}
    ${GUI_TYPE}
    ${RCC_SOURCES}
    ${APP_ICON_MACOSX}
    ${APP_ICON_RESOURCE_WINDOWS}
    ${APP_RESOURCE_WINDOWS}
    ${CMAKE_CURRENT_BINARY_DIR}/config.h    
    ${PORTAUDIO_SOURCES}
    ${APPLE_SOURCES}
    ${QCUSTOMPLOT_SOURCES}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    dabtables.h
    dabtables.cpp
    radiocontrol.h
    radiocontrol.cpp
    audiodecoder.h
    audiodecoder.cpp
    audiofifo.h
    audiofifo.cpp
    audiooutput.h
    audiooutputqt.h
    audiooutputqt.cpp
    dabchannellistmodel.h
    dabchannellistmodel.cpp
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
    slproxymodel.h
    slproxymodel.cpp
    setupdialog.h
    setupdialog.cpp
    setupdialog.ui
    bandscandialog.h
    bandscandialog.cpp
    bandscandialog.ui
    logdialog.h
    logdialog.cpp
    logdialog.ui
    logmodel.h
    logmodel.cpp

    ensembleinfodialog.h
    ensembleinfodialog.cpp
    ensembleinfodialog.ui

    # Input devices
    ${AIRSPY_SOURCES}
    ${SOAPYSDR_SOURCES}
    input/inputdevice.h
    input/inputdevice.cpp
    input/inputdevicesrc.h
    input/inputdevicesrc.cpp
    input/inputdevicerecorder.h
    input/inputdevicerecorder.cpp
    input/rawfileinput.h
    input/rawfileinput.cpp
    input/rtlsdrinput.h
    input/rtlsdrinput.cpp
    input/rtltcpinput.h
    input/rtltcpinput.cpp

    # User applications
    data/mscdatagroup.h
    data/mscdatagroup.cpp
    data/dldecoder.h
    data/dldecoder.cpp
    data/motdecoder.h
    data/motdecoder.cpp
    data/motobject.h
    data/motobject.cpp
    data/userapplication.h
    data/userapplication.cpp
    data/slideshowapp.h
    data/slideshowapp.cpp
    data/spiapp.h
    data/spiapp.cpp

    catslsdialog.h
    catslsdialog.cpp
    catslsdialog.ui

    aboutdialog.h
    aboutdialog.cpp
    aboutdialog.ui

    metadatamanager.h
    metadatamanager.cpp

    # customized widgets
    widgets/slsview.h
    widgets/slsview.cpp
    widgets/clickablelabel.h
    widgets/clickablelabel.cpp
    widgets/elidedlabel.h
    widgets/elidedlabel.cpp

    # EPG related classes
    epg/epgmodel.h
    epg/epgmodel.cpp
    epg/epgmodelitem.h
    epg/epgmodelitem.cpp
    epg/epgdialog.h
    epg/epgdialog.cpp
    epg/epgdialog.ui
    epg/epgproxymodel.h
    epg/epgproxymodel.cpp
    epg/epgtime.h
    epg/epgtime.cpp

    # audio recording features
    audiorec/audiorecorder.h
    audiorec/audiorecorder.cpp
    audiorec/audiorecscheduledialog.h
    audiorec/audiorecscheduledialog.cpp
    audiorec/audiorecscheduledialog.ui
    audiorec/audiorecschedulemodel.h
    audiorec/audiorecschedulemodel.cpp
    audiorec/audiorecscheduleitem.h
    audiorec/audiorecscheduleitem.cpp
    audiorec/audiorecitemdialog.h
    audiorec/audiorecitemdialog.cpp
    audiorec/audiorecitemdialog.ui
    audiorec/audiorecmanager.h
    audiorec/audiorecmanager.cpp

    # TII decoder
    tii/tiidialog.h tii/tiidialog.cpp
    tii/tiitablemodel.h tii/tiitablemodel.cpp
    tii/tiitablemodelitem.h tii/tiitablemodelitem.cpp
    tii/txdataitem.h tii/txdataitem.cpp
    tii/txdataloader.h tii/txdataloader.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/audiorec)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/epg)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/input)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tii)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/widgets)

# QML
set_source_files_properties(qml/EPGColors.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/epg.qml PROPERTIES QT_RESOURCE_ALIAS epg.qml)
set_source_files_properties(qml/EPGItem.qml PROPERTIES QT_RESOURCE_ALIAS EPGItem.qml)
set_source_files_properties(qml/EPGSwitch.qml PROPERTIES QT_RESOURCE_ALIAS EPGSwitch.qml)
set_source_files_properties(qml/EPGTabBar.qml PROPERTIES QT_RESOURCE_ALIAS EPGTabBar.qml)
set_source_files_properties(qml/EPGTabButton.qml PROPERTIES QT_RESOURCE_ALIAS EPGTabButton.qml)
set_source_files_properties(qml/EPGButton.qml PROPERTIES QT_RESOURCE_ALIAS EPGButton.qml)
set_source_files_properties(qml/EPGColors.qml PROPERTIES QT_RESOURCE_ALIAS EPGColors.qml)
set_source_files_properties(qml/EPGHorizontalShadow.qml PROPERTIES QT_RESOURCE_ALIAS EPGHorizontalShadow.qml)
set_source_files_properties(qml/EPGScrollableText.qml PROPERTIES QT_RESOURCE_ALIAS EPGScrollableText.qml)

if(HAVE_QTLOCATION)
    set(MAP_QML_FILE qml/map.qml)
else()
    set(MAP_QML_FILE qml/map_blank.qml)
endif()
set_source_files_properties(${MAP_QML_FILE} PROPERTIES QT_RESOURCE_ALIAS map.qml)
set_source_files_properties(qml/TransmitterMarker.qml PROPERTIES QT_RESOURCE_ALIAS TransmitterMarker.qml)
set_source_files_properties(qml/TIITableView.qml PROPERTIES QT_RESOURCE_ALIAS TIITableView.qml)


qt6_add_qml_module(${TARGET}
    URI app.qmlcomponents
    QML_FILES
        qml/epg.qml
        qml/EPGItem.qml
        qml/EPGSwitch.qml
        qml/EPGTabBar.qml
        qml/EPGTabButton.qml
        qml/EPGButton.qml
        qml/EPGColors.qml
        qml/EPGHorizontalShadow.qml
        qml/EPGScrollableText.qml

        ${MAP_QML_FILE}
        qml/TransmitterMarker.qml
        qml/TIITableView.qml
    RESOURCES
        resources/map_marker_3.png
        resources/map_marker_4.png
        resources/map_marker_5.png
        resources/Europe_blank_laea_location_map.svg
        resources/sort-up.svg
        resources/sort-down.svg
    VERSION 1.0
    RESOURCE_PREFIX "/"
)

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
if (HAVE_FDKAAC)
    # FDKAAC
    include_directories ( ${LIBFDKAAC_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${LIBFDKAAC_LINK_LIBRARIES}" )
else (HAVE_FDKAAC)
    # FAAD
    include_directories ( ${LIBFAAD2_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${LIBFAAD2_LINK_LIBRARIES}" )
endif (HAVE_FDKAAC)

# MPG123
include_directories ( ${LIBMPG123_INCLUDE_DIRS} )
target_link_libraries(${TARGET} PRIVATE "${LIBMPG123_LINK_LIBRARIES}" )

# RTLSDR
include_directories ( ${RTL_SDR_INCLUDE_DIRS} )
target_link_libraries(${TARGET} PRIVATE "${RTL_SDR_LINK_LIBRARIES}" )

# PORTAUDIO
if (USE_PORTAUDIO)
    include_directories ( ${PORTAUDIO_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${PORTAUDIO_LINK_LIBRARIES}" )
endif(USE_PORTAUDIO)

# AIRSPY
if (HAVE_AIRSPY)
    # AIRSPY
    include_directories ( ${AIRSPY_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${AIRSPY_LINK_LIBRARIES}" )
endif(HAVE_AIRSPY)

# SOAPYSDR
if (HAVE_SOAPYSDR)
    # SOAPYSDR
    include_directories ( ${SOAPYSDR_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${SOAPYSDR_LINK_LIBRARIES}" )
endif(HAVE_SOAPYSDR)

# QCUSTOMPLOT
if (HAVE_QCUSTOMPLOT)
    # QCUSTOMPLOT
    add_compile_definitions(QCUSTOMPLOT_USE_LIBRARY)
    include_directories ( ${QCUSTOMPLOT_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${QCUSTOMPLOT_LINK_LIBRARIES}" )
endif(HAVE_QCUSTOMPLOT)


# Set some Win32 Specific Settings
if(WIN32)
    # required fro sockets
    target_link_libraries(${TARGET} PRIVATE ws2_32)
endif(WIN32)

include_directories ( ${CMAKE_SOURCE_DIR} )
target_link_libraries(${TARGET} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::Positioning
)
if(HAVE_QTLOCATION)
    target_link_libraries(${TARGET} PRIVATE
        Qt${QT_VERSION_MAJOR}::Location
    )
endif(HAVE_QTLOCATION)

set(TSFILES
    l10n/${TARGET}_cs.ts
    l10n/${TARGET}_de.ts
    l10n/${TARGET}_pl.ts
)
qt_add_translations(${TARGET} TS_FILES ${TSFILES}
    LUPDATE_OPTIONS "-noobsolete"
)

# Set a custom plist file for the app bundle
if(APPLE AND APPLE_APP_BUNDLE)
    set_target_properties(${TARGET} PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}_${PROJECT_GIT_REV}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE TRUE
    )
    if (NOT CMAKE_GENERATOR STREQUAL "Xcode")
        # Need to sign application for location permissions to work
        add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND codesign -s - ${TARGET}.app)
    endif()
endif(APPLE AND APPLE_APP_BUNDLE)
