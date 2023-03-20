#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dabsdr::dabsdr" for configuration "Release"
set_property(TARGET dabsdr::dabsdr APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(dabsdr::dabsdr PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/linux_aarch64/libdabsdr.so.2.1.2"
  IMPORTED_SONAME_RELEASE "libdabsdr.so.2"
  )

list(APPEND _cmake_import_check_targets dabsdr::dabsdr )
list(APPEND _cmake_import_check_files_for_dabsdr::dabsdr "${_IMPORT_PREFIX}/linux_aarch64/libdabsdr.so.2.1.2" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
