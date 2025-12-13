cmake_minimum_required(VERSION 3.10)
project(insta360-gstreamer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

set(INSTA360_SDK_DIR ${PROJECT_SOURCE_DIR})

# Find OpenCV
find_package(OpenCV REQUIRED)

# Find FFmpeg (libavformat, libavcodec, libswscale) and GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT libavformat REQUIRED)
pkg_check_modules(AVCODEC libavcodec REQUIRED)
pkg_check_modules(AVUTIL libavutil REQUIRED)
pkg_check_modules(SWSCALE libswscale REQUIRED)
pkg_check_modules(GST gstreamer-1.0 REQUIRED)
pkg_check_modules(GST_APP gstreamer-app-1.0 REQUIRED)
pkg_check_modules(GST_VIDEO gstreamer-video-1.0 REQUIRED)
pkg_check_modules(GST_RTP gstreamer-rtp-1.0 REQUIRED)

# For string formatting using fmt::format
find_package(fmt)

if(NOT EXISTS ${INSTA360_SDK_DIR})
  message(FATAL_ERROR "Error: INSTA360_SDK_DIR ${INSTA360_SDK_DIR} does not exist.")
else()
  message(STATUS "INSTA360_SDK_DIR: " ${INSTA360_SDK_DIR})
endif()

# Include directories
include_directories(
  "${INSTA360_SDK_DIR}/include/"
  ${GST_INCLUDE_DIRS}
  ${GST_APP_INCLUDE_DIRS}
  ${GST_VIDEO_INCLUDE_DIRS}
  ${GST_RTP_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${AVFORMAT_INCLUDE_DIRS}
  ${AVCODEC_INCLUDE_DIRS}
  ${AVUTIL_INCLUDE_DIRS}
  ${SWSCALE_INCLUDE_DIRS}
)

# Compile here
add_executable(streamer src/streamer.cpp)
target_link_libraries(
  streamer
  fmt::fmt
  "${INSTA360_SDK_DIR}/lib/libCameraSDK.so"
  "/usr/lib/x86_64-linux-gnu/libudev.so"
  ${GST_LIBRARIES}
  ${GST_APP_LIBRARIES}
  ${GST_VIDEO_LIBRARIES}
  ${GST_RTP_LIBRARIES}
  ${OpenCV_LIBS}
  ${AVFORMAT_LIBRARIES}
  ${AVCODEC_LIBRARIES}
  ${AVUTIL_LIBRARIES}
  ${SWSCALE_LIBRARIES}
)
