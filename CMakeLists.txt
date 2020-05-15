# CMake is used to export libraries related to the P4 IR. It currently cannot
# be used to build the entire PDPI project.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(src)
