cmake_minimum_required(VERSION 3.10)

# set the project name
project(PTU2BIN)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if(DOPERFORMANCEANALYSIS)
add_compile_definitions(DOPERFORMANCEANALYSIS)
endif()
# add the executable
add_executable(PTU2BIN PTU2BIN.cpp export_igor_ibw.cpp export_igor_ibw.h PTUFileHeader.cpp PTUFileHEader.h RecordBuffer.h TTTRRecordProcessor.cpp TTTRRecordProcessor.h external/cxxopts.hpp)


