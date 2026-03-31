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

if (HAVE_FAAD)
    set(FAAD_SOURCES audiodecoderfaad.cpp audiodecoderfaad.h)
endif(HAVE_FAAD)
if (HAVE_FDKAAC)
    set(FDKAAC_SOURCES audiodecoderfdkaac.cpp audiodecoderfdkaac.h)
endif(HAVE_FDKAAC)
if (HAVE_AIRSPY)
    set(AIRSPY_SOURCES input/airspyinput.h input/airspyinput.cpp)
endif(HAVE_AIRSPY)
if (HAVE_SOAPYSDR)
    set(SOAPYSDR_SOURCES input/soapysdrinput.h input/soapysdrinput.cpp input/sdrplayinput.h input/sdrplayinput.cpp)
endif(HAVE_SOAPYSDR)
if (HAVE_PORTAUDIO)
    set(PORTAUDIO_SOURCES audiooutputpa.h audiooutputpa.cpp)
endif(HAVE_PORTAUDIO)


#########################################################
## SOURCES & HEADERS
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

qt_add_executable(${TARGET} MANUAL_FINALIZATION
    ${GUI_TYPE}
    ${RCC_SOURCES}
    ${APP_ICON_MACOSX}
    ${APP_ICON_RESOURCE_WINDOWS}
    ${APP_RESOURCE_WINDOWS}
    ${CMAKE_CURRENT_BINARY_DIR}/config.h    
    ${PORTAUDIO_SOURCES}
    ${APPLE_SOURCES}
    ${FAAD_SOURCES}
    ${FDKAAC_SOURCES}
    main.cpp
    application.cpp
    application.h

    dabtables.h
    dabtables.cpp
    radiocontrol.h
    radiocontrol.cpp
    audiodecoder.h
    audiodecoder.cpp
    audiodecoder.h

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

    settings.h

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
    input/rarttcpinput.h
    input/rarttcpinput.cpp

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

    updatechecker.h updatechecker.cpp
    metadatamanager.h
    metadatamanager.cpp

    appversion.h

    # TII
    tii/txdataitem.h tii/txdataitem.cpp
    tii/txdataloader.h tii/txdataloader.cpp
    tii/txlocallist.h tii/txlocallist.cpp
    tii/txtablemodelitem.cpp tii/txtablemodelitem.h

    # EPG
    epg/epgmodelitem.h
    epg/epgmodelitem.cpp
    epg/epgtime.h
    epg/epgtime.cpp

    # Audiorec
    audiorec/audiorecscheduleitem.h
    audiorec/audiorecscheduleitem.cpp
    audiorec/audiorecorder.h
    audiorec/audiorecorder.cpp
)

target_link_libraries(AbracaDABra PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(AbracaDABra PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(AbracaDABra PRIVATE Qt${QT_VERSION_MAJOR}::Core)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/audiorec)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/epg)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/data)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/charts)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/input)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tii)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/scanner)

# QML
if(QT_VERSION_MINOR GREATER_EQUAL 8)
    qt_policy(SET QTP0004 NEW)
endif()

set(QML_SINGLETONS
    qml/UI.qml
)
set_source_files_properties(${QML_SINGLETONS}
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(${TARGET}
    URI abracaComponents
    VERSION 1.0
    RESOURCES
        qml/images/map_marker_3.png
        qml/images/map_marker_4.png
        qml/images/map_marker_5.png
        qml/images/sort-up.svg
        qml/images/sort-down.svg
        qml/images/reload.svg
        qml/images/close.svg
        qml/images/icon-cat-sls.svg
        qml/images/icon-others.svg
        qml/images/icon-settings.svg
        qml/images/chevron-left.svg
        qml/images/chevron-right.svg
        qml/images/chevron-down.svg
        qml/images/icon-multi-ens.svg
        qml/images/star.svg
        qml/images/starEmpty.svg
        qml/images/announcement-active.svg
        qml/images/announcement-suspended.svg
        qml/images/volume-0.svg
        qml/images/volume-1.svg
        qml/images/volume-2.svg
        qml/images/volume-3.svg        
        qml/images/diagonal-line-pattern.svg
        qml/images/alert.svg
        qml/images/icon-recording.svg
        qml/images/trayIcon.svg
        qml/images/icon-service.svg
        qml/images/icon-info.svg
        qml/images/icon-signal-overview.svg
        qml/images/icon-scanner.svg
        qml/images/icon-tii.svg
        qml/images/icon-epg.svg
        qml/images/check.svg
        qml/images/icon-off.svg
        qml/images/icon-about.svg
        qml/images/icon-log.svg
        qml/images/icon-bandscan.svg
        qml/images/icon-list.svg
        qml/images/icon-list-clear.svg
        qml/images/icon-list-export.svg
        qml/images/icon-rec-schedule.svg
        qml/images/icon-rec-start.svg
        qml/images/icon-dock.svg
        qml/images/icon-undock.svg
        qml/images/icon-audio.svg
        qml/images/icon-config.svg
        qml/images/icon-drag.svg
        qml/images/icon-loop-file.svg
        qml/images/placeholder-undocked.svg
        qml/images/icon-sidebar-left.svg
        qml/images/icon-cross.svg

        resources/appIcon.png
    QML_FILES
        ${QML_SINGLETONS}
        qml/Main.qml
        qml/UndockablePage.qml
        qml/DetachedWindow.qml
        qml/DynamicPage.qml

        qml/ServiceList.qml
        qml/SignalState.qml
        qml/Navigation.qml        
        qml/ServiceView.qml        
        qml/ContentView.qml
        qml/EnsembleInfoView.qml
        qml/EnsembleConfigDialog.qml
        qml/EnsembleConfigDrawer.qml
        qml/SubchannelsView.qml
        qml/ServicePanel.qml
        qml/StatusBar.qml
        qml/SPIProgress.qml
        qml/SLSView.qml
        qml/CatSLS.qml
        qml/CatSLSView.qml
        qml/AppLog.qml
        qml/AppLogView.qml
        qml/About.qml
        qml/AboutView.qml
        qml/BandScan.qml
        qml/MessageBox.qml
        qml/DabSignal.qml
        qml/DabSignalView.qml
        qml/UpdateDialog.qml        
        qml/SystemTray.qml

        qml/audiorec/AudioRecordingStatus.qml
        qml/audiorec/AudioRecording.qml
        qml/audiorec/AudioRecordingView.qml
        qml/audiorec/AudioRecordingItemDialog.qml

        qml/tii/TII.qml
        qml/tii/TIIView.qml
        qml/tii/TIIMap.qml
        qml/tii/TransmitterMarker.qml
        qml/tii/Scanner.qml
        qml/tii/ScannerView.qml
        qml/tii/ScannerSetupDialog.qml
        qml/tii/ScannerSetupDrawer.qml
        qml/tii/ChannelSelectionDialog.qml

        qml/epg/EPG.qml
        qml/epg/EPGView.qml
        qml/epg/EPGItem.qml

        qml/controls/AbracaButton.qml
        qml/controls/AbracaImgButton.qml
        qml/controls/AbracaLabel.qml
        qml/controls/AbracaComboBox.qml
        qml/controls/AbracaCheckBox.qml
        qml/controls/AbracaSwitch.qml
        qml/controls/AbracaSlider.qml
        qml/controls/AbracaProgressBar.qml
        qml/controls/AbracaRadioButton.qml
        qml/controls/AbracaGroupBox.qml
        qml/controls/AbracaScrollIndicator.qml
        qml/controls/AbracaPane.qml
        qml/controls/AbracaTabBar.qml
        qml/controls/AbracaTabButton.qml
        qml/controls/AbracaItemDelegate.qml
        qml/controls/AbracaColorizedImage.qml
        qml/controls/AbracaDrawer.qml
        qml/controls/AbracaTextField.qml
        qml/controls/AbracaScrollBar.qml
        qml/controls/AbracaMenu.qml
        qml/controls/AbracaMenuItem.qml
        qml/controls/AbracaTextArea.qml
        qml/controls/AbracaToolTip.qml
        qml/controls/AbracaSpinBox.qml
        qml/controls/AbracaDoubleSpinBox.qml
        qml/controls/AbracaScrollableText.qml
        qml/controls/AbracaHorizontalShadow.qml
        qml/controls/AbracaContextMenu.qml
        qml/controls/AbracaBusyIndicator.qml
        qml/controls/AbracaTableView.qml
        qml/controls/AbracaDatePicker.qml
        qml/controls/AbracaTimePicker.qml
        qml/controls/AbracaRecordingButton.qml
        qml/controls/AbracaLine.qml
        qml/controls/AbracaMenuSeparator.qml
        qml/controls/AbracaDialog.qml
        qml/controls/AbracaSplitView.qml
        qml/controls/AbracaScrollView.qml
        qml/controls/AbracaMessage.qml

        qml/settings/Settings.qml
        qml/settings/SettingsView.qml
        qml/settings/DeviceSettings.qml
        qml/settings/RtlSdrDevice.qml
        qml/settings/RtlTcpDevice.qml
        qml/settings/SoapySdrDevice.qml
        qml/settings/AirspyDevice.qml
        qml/settings/RartTcpDevice.qml
        qml/settings/SdrPlayDevice.qml
        qml/settings/RawFileDevice.qml
        qml/settings/AudioSettings.qml
        qml/settings/AnnouncementSettings.qml
        qml/settings/UaSettings.qml
        qml/settings/TiiSettings.qml
        qml/settings/OtherSettings.qml

        qml/charts/ChartView.qml

    SOURCES
        itemmodel.h itemmodel.cpp
        charts/linechartitem.cpp
        charts/linechartitem.h
        charts/lineseries.h
        charts/markerline.h

        navigationmodel.h navigationmodel.cpp
        navigationproxymodel.h navigationproxymodel.cpp

        # TII decoder
        tii/txmapbackend.h tii/txmapbackend.cpp
        tii/tiibackend.cpp tii/tiibackend.h
        tii/tiitablecolssettingsmodel.h tii/tiitablecolssettingsmodel.cpp

        # Scanner tool
        tii/scannerbackend.cpp tii/scannerbackend.h
        tii/txtablemodel.cpp tii/txtablemodel.h
        tii/txtableproxymodel.h tii/txtableproxymodel.cpp

        # application.h application.cpp
        uicontrolprovider.h

        dlplusmodel.h dlplusmodel.cpp
        slsbackend.h slsbackend.cpp
        catslsbackend.cpp catslsbackend.h
        logbackend.cpp logbackend.h
        aboutui.h aboutui.cpp
        bandscanbackend.cpp bandscanbackend.h
        settingsbackend.cpp settingsbackend.h        
        soapysdrgainmodel.h soapysdrgainmodel.cpp

        messageboxbackend.h messageboxbackend.cpp
        contextmenumodel.h contextmenumodel.cpp

        signalbackend.h signalbackend.cpp

        # EPG related classes
        epg/epgbackend.h epg/epgbackend.cpp
        epg/epgmodel.h
        epg/epgmodel.cpp
        epg/epgproxymodel.h
        epg/epgproxymodel.cpp

        # audio recording features
        audiorec/audiorecschedulemodel.h
        audiorec/audiorecschedulemodel.cpp
        audiorec/audiorecmanager.h
        audiorec/audiorecmanager.cpp        

        slproxymodel.h
        slproxymodel.cpp
        slmodel.h
        slmodel.cpp
        slmodelitem.h
        slmodelitem.cpp
        sltreemodel.h
        sltreemodel.cpp

        logmodel.h
        logmodel.cpp

        ensembleinfobackend.h ensembleinfobackend.cpp
        ensemblesubchmodel.h ensemblesubchmodel.cpp
        androidfilehelper.h androidfilehelper.cpp
)

# add_subdirectory(qml)

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
endif (HAVE_FDKAAC)
if (HAVE_FAAD)
    # FAAD
    include_directories ( ${LIBFAAD2_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE "${LIBFAAD2_LINK_LIBRARIES}" )
endif (HAVE_FAAD)

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

# FMLIST
if (HAVE_FMLIST_INTERFACE)
    include_directories ( ${FMLIST_INCLUDE_DIRS} )
    target_link_libraries(${TARGET} PRIVATE ${FMLIST_LINK_LIBRARIES})
endif(HAVE_FMLIST_INTERFACE)

# Set some Win32 Specific Settings
if(WIN32)
    # required fro sockets
    target_link_libraries(${TARGET} PRIVATE ws2_32)
endif(WIN32)

include_directories ( ${CMAKE_SOURCE_DIR} )
target_link_libraries(${TARGET} PRIVATE
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::Positioning
    Qt${QT_VERSION_MAJOR}::Location
)
if(HAVE_QTWIDGETS)
    target_link_libraries(${TARGET} PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
    )
endif(HAVE_QTWIDGETS)

set(TSFILES
    l10n/${TARGET}_cs.ts
    l10n/${TARGET}_de.ts
    l10n/${TARGET}_pl.ts
    l10n/${TARGET}_nl.ts
)
qt_add_translations(${TARGET} TS_FILES ${TSFILES}
    LUPDATE_OPTIONS "-noobsolete"
)

# Set a custom plist file for the app bundle
if(APPLE AND APPLE_APP_BUNDLE)
    set_target_properties(${TARGET} PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_VERSION        "${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING  "${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
        MACOSX_BUNDLE TRUE
    )
    if (NOT CMAKE_GENERATOR STREQUAL "Xcode")
        # Need to sign application for location permissions to work
        add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND codesign -s - ${TARGET}.app)
    endif()
endif(APPLE AND APPLE_APP_BUNDLE)

# Android-specific settings
if(ANDROID)
    # Bundle external shared libraries into the APK
    set(ANDROID_EXTRA_LIBS "")

    # Add RTL-SDR library
    if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/librtlsdr.so")
        list(APPEND ANDROID_EXTRA_LIBS "${EXTERNAL_LIBS_DIR}/lib/librtlsdr.so")
    endif()
    
    # Add libusb library (required by RTL-SDR)
    if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/libusb-1.0.so")
        list(APPEND ANDROID_EXTRA_LIBS "${EXTERNAL_LIBS_DIR}/lib/libusb-1.0.so")
    endif()
    
    # Add other required libraries if they exist
    if(HAVE_FAAD AND EXISTS "${EXTERNAL_LIBS_DIR}/lib/libfaad.so")
        list(APPEND ANDROID_EXTRA_LIBS "${EXTERNAL_LIBS_DIR}/lib/libfaad.so")
    endif()
    
    if(HAVE_FDKAAC AND EXISTS "${EXTERNAL_LIBS_DIR}/lib/libfdk-aac.so")
        list(APPEND ANDROID_EXTRA_LIBS "${EXTERNAL_LIBS_DIR}/lib/libfdk-aac.so")
    endif()
    
    if(EXISTS "${EXTERNAL_LIBS_DIR}/lib/libmpg123.so")
        list(APPEND ANDROID_EXTRA_LIBS "${EXTERNAL_LIBS_DIR}/lib/libmpg123.so")
    endif()
    
    # Add Airspy library if enabled
    if(HAVE_AIRSPY AND EXISTS "${EXTERNAL_LIBS_DIR}/lib/libairspy.so")
        list(APPEND ANDROID_EXTRA_LIBS "${EXTERNAL_LIBS_DIR}/lib/libairspy.so")
    endif()
    
    if(ANDROID_EXTRA_LIBS)
        message(STATUS "Android extra libs: ${ANDROID_EXTRA_LIBS}")
        set_property(TARGET ${TARGET} PROPERTY QT_ANDROID_EXTRA_LIBS "${ANDROID_EXTRA_LIBS}")
    endif()
    
    # Set Android package properties
    # Calculate Android version code: (((MAJOR*100)+MINOR)*100+PATCH)*1000+GIT
    # This ensures the version code increases with every release
    math(EXPR ANDROID_VERSION_CODE "(((${VER_MAJOR} * 100) + ${VER_MINOR}) * 100 + ${VER_PATCH}) * 1000 + ${VER_GIT}")
    
    set_target_properties(${TARGET} PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_VERSION_NAME "${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
        QT_ANDROID_VERSION_CODE "${ANDROID_VERSION_CODE}"
    )
    
    message(STATUS "Android version: ${VER_MAJOR}.${VER_MINOR}.${VER_PATCH} (code: ${ANDROID_VERSION_CODE})")
    
    # Add OpenSSL libraries if ANDROID_OPENSSL_DIR is set
    # Must be called BEFORE qt_finalize_executable
    if(ANDROID_OPENSSL_DIR)
        add_android_openssl_libraries(${TARGET})
    endif()
    
    # For Android, qt_finalize_executable must be called from the same directory as the target
    qt_finalize_executable(${TARGET})
endif(ANDROID)
