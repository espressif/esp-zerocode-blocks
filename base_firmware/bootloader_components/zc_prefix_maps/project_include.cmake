# The bootloader sub-build gets the app's prefix maps.
get_filename_component(_zc_project "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${_zc_project}/cmake/prefix_maps.cmake")
zc_add_prefix_maps("${_zc_project}")
