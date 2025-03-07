#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "fmlistInterface::fmlistInterface" for configuration "Release"
set_property(TARGET fmlistInterface::fmlistInterface APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(fmlistInterface::fmlistInterface PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "Qt6::Core;Qt6::Gui;Qt6::Network"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/darwin_arm64/libfmlistInterface.2.0.0.dylib"
  IMPORTED_SONAME_RELEASE "@rpath/libfmlistInterface.2.dylib"
  )

list(APPEND _cmake_import_check_targets fmlistInterface::fmlistInterface )
list(APPEND _cmake_import_check_files_for_fmlistInterface::fmlistInterface "${_IMPORT_PREFIX}/darwin_arm64/libfmlistInterface.2.0.0.dylib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
