cmake_minimum_required(VERSION 3.10)
project(simbricks-cxx CXX)

add_subdirectory(../base)

add_library(simbricks-cxx STATIC)

target_include_directories(simbricks-cxx PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_sources(simbricks-cxx PRIVATE
  communicator/shm_communicator.cc
)

target_link_libraries(simbricks-cxx PRIVATE simbricks-base)
