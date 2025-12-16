cmake_minimum_required(VERSION 3.10)
project(simbricks-base C)

# Create a static library for the simbricks C base code
add_library(simbricks-base STATIC
  if.c
)

# Add the parent directory (lib/simbricks) as an include directory
# so that if.h can find its dependencies like "simbricks/base/proto.h"
target_include_directories(simbricks-base PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
