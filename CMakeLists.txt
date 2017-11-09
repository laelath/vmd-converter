cmake_minimum_required (VERSION 2.7)
project (VMD-Converter)

include_directories (include)

file (GLOB SOURCES *.c)
add_executable (vmd-converter ${SOURCES})

target_link_libraries (vmd-converter)
