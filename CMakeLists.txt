cmake_minimum_required(VERSION 2.6)
project(INFINIPIC)

# All header files are specified relative to base dir.
include_directories(${PROJECT_SOURCE_DIR})

# Set some basic build flags.
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -std=c++0x")

############################################################
# Configure required libraries.

# Boost.
find_package(Boost COMPONENTS filesystem system REQUIRED)

# OpenCV.
find_package(OpenCV REQUIRED)

# Protobuf library
include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})

set(APP_LIBRARIES
  gflags
  GL
  GLU
  X11
  ${Boost_LIBRARIES}
  ${OpenCV_LIBS}
  ${PROTOBUF_LIBRARIES}
)

############################################################
# Build targets

set(INFINIPIC_SRCS
  infinipic.cc
  recordio.cc
  window.cc
)
add_executable(infinipic ${INFINIPIC_SRCS})
target_link_libraries(infinipic ${APP_LIBRARIES})
