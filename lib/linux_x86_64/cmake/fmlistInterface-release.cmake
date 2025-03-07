#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "fmlistInterface::fmlistInterface" for configuration "Release"
set_property(TARGET fmlistInterface::fmlistInterface APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(fmlistInterface::fmlistInterface PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "Qt6::Core;Qt6::Gui;Qt6::Network"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/linux_x86_64/libfmlistInterface.so.2.0.0"
  IMPORTED_SONAME_RELEASE "libfmlistInterface.so.2"
  )

list(APPEND _IMPORT_CHECK_TARGETS fmlistInterface::fmlistInterface )
list(APPEND _IMPORT_CHECK_FILES_FOR_fmlistInterface::fmlistInterface "${_IMPORT_PREFIX}/linux_x86_64/libfmlistInterface.so.2.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
