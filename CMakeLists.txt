cmake_minimum_required(VERSION 2.6)

include(CheckCSourceCompiles)
include(CheckCSourceRuns)

project(vncbenchtools)

set(ENCODER_OPTIONS
  "tight-1.1, tight-1.3.9, turbo-0.4, turbo-0.5, turbo-1.0, turbo-1.1, tiger-1.1, tiger-1.2, h264")

set(ENCODER "" CACHE STRING
  "Which Tight/Turbo/Tiger encoder to use (one of ${ENCODER_OPTIONS})")

set(DECODER_OPTIONS
  "tight-1.3.9, turbo-0.4, turbo-0.5, turbo-1.0, tiger-1.1, tiger-1.2")

set(DECODER "" CACHE STRING
  "Which Tight/Turbo/Tiger decoder to use (one of ${DECODER_OPTIONS})")

if(NOT ENCODER)
  message(FATAL_ERROR "ENCODER must be set to one of ${ENCODER_OPTIONS}")
endif()

if(NOT DECODER)
  message(FATAL_ERROR "DECODER must be set to one of ${DECODER_OPTIONS}")
endif()

message(STATUS "Using ${ENCODER} encoder")
message(STATUS "Using ${DECODER} decoder")

math(EXPR BITS "${CMAKE_SIZEOF_VOID_P} * 8")
message(STATUS "${BITS}-bit build")

set(SOURCES compare-encodings.c misc.c hextile.c zlib.c zrle.c
  zrleoutstream.c zrlepalettehelper.c translate.c)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(LINK_LIBRARIES z)

if(ENCODER MATCHES tig* OR DECODER MATCHES tig* OR ENCODER MATCHES h264)
  # Check for libjpeg
  find_package(JPEG REQUIRED)

  if(ENCODER MATCHES tiger-* OR DECODER MATCHES tiger-*)
    # Warn if it doesn't seem to be the accelerated libjpeg that's found
    set(CMAKE_REQUIRED_LIBRARIES ${JPEG_LIBRARIES})
    set(CMAKE_REQUIRED_FLAGS -I${JPEG_INCLUDE_DIR})

    set(JPEG_TEST_SOURCE "\n
      #include <stdio.h>\n
      #include <jpeglib.h>\n
      int main(void) {\n
        struct jpeg_compress_struct cinfo;\n
        struct jpeg_error_mgr jerr;\n
        cinfo.err=jpeg_std_error(&jerr);\n
        jpeg_create_compress(&cinfo);\n
        cinfo.input_components = 3;\n
        jpeg_set_defaults(&cinfo);\n
        cinfo.in_color_space = JCS_EXT_RGB;\n
        jpeg_default_colorspace(&cinfo);\n
        return 0;\n
      }")

    if(CMAKE_CROSSCOMPILING)
      check_c_source_compiles("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
    else()
      check_c_source_runs("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
    endif()

    set(CMAKE_REQUIRED_LIBRARIES)
    set(CMAKE_REQUIRED_FLAGS)
    set(CMAKE_REQUIRED_DEFINITIONS)

    if(NOT FOUND_LIBJPEG_TURBO)
      message(STATUS "WARNING: You are not using libjpeg-turbo. Performance will suffer.")
    endif()
  endif()

  set(LINK_LIBRARIES ${LINK_LIBRARIES} ${JPEG_LIBRARIES})
  include_directories(${JPEG_INCLUDE_DIR})
endif()

if((ENCODER MATCHES turbo-* OR DECODER MATCHES turbo-*)
  AND NOT ENCODER MATCHES h264)
  include(cmakescripts/FindTurboJPEG.cmake)
  set(LINK_LIBRARIES ${LINK_LIBRARIES} ${TJPEG_LIBRARY})
endif()

if(ENCODER MATCHES turbo-1.* OR DECODER MATCHES turbo-1.*)
  set(LINK_LIBRARIES ${LINK_LIBRARIES} pthread)
endif()

if(ENCODER MATCHES h264)
  if(BITS EQUAL 64)
    set(DEFAULT_X264_DIR /opt/x264/linux64)
  else()
    set(DEFAULT_X264_DIR /opt/x264/linux)
  endif()
  set(X264_DIR ${DEFAULT_X264_DIR} CACHE PATH
    "Directory in which libx264 is installed (default: ${DEFAULT_X264_DIR})")
  message(STATUS "X264_DIR = ${X264_DIR}")
  include_directories(${X264_DIR}/include ${CMAKE_SOURCE_DIR}/flv
    ${CMAKE_SOURCE_DIR}/turbojpeg)
  add_definitions(-DH264)
  set(LINK_LIBRARIES ${LINK_LIBRARIES} ${X264_DIR}/lib/libx264.a m)
  set(SOURCES ${SOURCES} flv/flv.c flv/flv_bytestream.c turbojpeg/turbojpeg.c
    turbojpeg/jdatadst-tj.c turbojpeg/jdatasrc-tj.c)
endif()

if(ENCODER MATCHES tiger-*)
  file(REMOVE tiger.cxx)
  add_custom_command(OUTPUT tiger.cxx
    COMMAND ln -fs ${CMAKE_SOURCE_DIR}/${ENCODER}.cxx tiger.cxx)
  add_definitions(-DHAVE_VSNPRINTF -DHAVE_SNPRINTF)
  include_directories(${CMAKE_SOURCE_DIR}/${ENCODER})
  set(SOURCES ${SOURCES} ${CMAKE_BINARY_DIR}/tiger.cxx)
  add_subdirectory(${ENCODER})
  set(LINK_LIBRARIES tigervnc ${LINK_LIBRARIES})
else()
  file(REMOVE tight.c)
  add_custom_command(OUTPUT tight.c
    COMMAND ln -fs ${CMAKE_SOURCE_DIR}/${ENCODER}.c tight.c)
  set(SOURCES ${SOURCES} ${CMAKE_BINARY_DIR}/tight.c)
endif()

if(DECODER MATCHES tiger-*)
  file(REMOVE tigerd.cxx)
  add_custom_command(OUTPUT tigerd.cxx
    COMMAND ln -fs ${CMAKE_SOURCE_DIR}/${DECODER}d.cxx tigerd.cxx)
  add_definitions(-DTIGERD -DHAVE_VSNPRINTF -DHAVE_SNPRINTF)
  include_directories(${CMAKE_SOURCE_DIR}/${DECODER})
  set_source_files_properties(compare-encodings.c PROPERTIES
    OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/tigerd.cxx
    LANGUAGE CXX)
  if(NOT DECODER STREQUAL ENCODER)
    add_subdirectory(${DECODER})
  endif()
  set(LINK_LIBRARIES tigervnc ${LINK_LIBRARIES})
else()
  file(REMOVE tightd.c)
  add_custom_command(OUTPUT tightd.c
    COMMAND ln -fs ${CMAKE_SOURCE_DIR}/${DECODER}d.c tightd.c)
  set(DEPENDENCIES tightd.c)
  set_source_files_properties(compare-encodings.c PROPERTIES
    OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/tightd.c)
endif()

add_executable(compare-encodings ${SOURCES})
target_link_libraries(compare-encodings ${LINK_LIBRARIES})
