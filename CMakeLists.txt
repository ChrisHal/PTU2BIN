cmake_minimum_required (VERSION 3.10)

project ("PTU2BIN")
option(DOPERFORMANCEANALYSIS "enable timing of execution time" OFF)

add_subdirectory ("PTU2BIN")
add_subdirectory ("convertPTUs")
