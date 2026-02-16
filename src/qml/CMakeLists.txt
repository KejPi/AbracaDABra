# QML
set_source_files_properties(EPGColors.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

if(HAVE_QTLOCATION)
    set(MAP_QML_FILE map.qml)
else()
    set(MAP_QML_FILE map_blank.qml)
endif()


qt_add_qml_module(QmlUI
    PLUGIN_TARGET QmlUI
    URI abracaComponents
    VERSION 1.0
    RESOURCES
        images/map_marker_3.png
        images/map_marker_4.png
        images/map_marker_5.png
        images/Europe_blank_laea_location_map.svg
        images/sort-up.svg
        images/sort-down.svg
    QML_FILES
        epg.qml
        EPGItem.qml
        EPGSwitch.qml
        EPGTabBar.qml
        EPGTabButton.qml
        EPGButton.qml
        EPGColors.qml
        EPGHorizontalShadow.qml
        EPGScrollableText.qml

        ${MAP_QML_FILE}
        TransmitterMarker.qml
        TIITableView.qml
)
