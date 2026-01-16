cmake_minimum_required(VERSION 3.20)
project(winsoftphone C)

if(NOT PJSIP_DIR AND DEFINED ENV{PJSIP_DIR})
  set(PJSIP_DIR "$ENV{PJSIP_DIR}")
endif()

if(NOT PJSIP_DIR)
  message(FATAL_ERROR "PJSIP_DIR is required (root with headers/libs).")
endif()

if(NOT OPENSSL_ROOT_DIR AND DEFINED ENV{OPENSSL_ROOT_DIR})
  set(OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT_DIR}")
endif()
if(NOT OPENSSL_ROOT_DIR AND DEFINED ENV{VCPKG_ROOT})
  set(OPENSSL_ROOT_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows")
endif()
if(OPENSSL_ROOT_DIR)
  list(APPEND CMAKE_PREFIX_PATH "${OPENSSL_ROOT_DIR}")
endif()

find_package(OpenSSL REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(winsoftphone
  src/main.c
  src/app.c
  src/ui.c
  src/sip.c
  src/config.c
  src/contacts.c
  src/history.c
  src/storage.c
  src/json_utils.c
  src/jsmn.c
)

set(PJSIP_INCLUDE_DIRS "")
set(PJSIP_LIB_DIRS "")

set(PJSIP_INCLUDE_CANDIDATES
  ${PJSIP_DIR}/include
  ${PJSIP_DIR}/pjsip/include
  ${PJSIP_DIR}/pjsip/include/pjsua-lib
  ${PJSIP_DIR}/pjlib/include
  ${PJSIP_DIR}/pjlib-util/include
  ${PJSIP_DIR}/pjnath/include
  ${PJSIP_DIR}/pjmedia/include
)

foreach(dir IN LISTS PJSIP_INCLUDE_CANDIDATES)
  if(EXISTS "${dir}")
    list(APPEND PJSIP_INCLUDE_DIRS "${dir}")
  endif()
endforeach()

if(PJSIP_INCLUDE_DIRS STREQUAL "")
  message(FATAL_ERROR "Could not locate PJSIP headers under ${PJSIP_DIR}.")
endif()

set(PJSIP_LIB_CANDIDATES
  ${PJSIP_DIR}/lib
  ${PJSIP_DIR}/pjsip/lib
  ${PJSIP_DIR}/pjlib/lib
  ${PJSIP_DIR}/pjlib-util/lib
  ${PJSIP_DIR}/pjnath/lib
  ${PJSIP_DIR}/pjmedia/lib
)

foreach(dir IN LISTS PJSIP_LIB_CANDIDATES)
  if(EXISTS "${dir}")
    list(APPEND PJSIP_LIB_DIRS "${dir}")
  endif()
endforeach()

if(PJSIP_LIB_DIRS STREQUAL "")
  message(FATAL_ERROR "Could not locate PJSIP libraries under ${PJSIP_DIR}.")
endif()

target_include_directories(winsoftphone PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${PJSIP_INCLUDE_DIRS}
)

target_link_directories(winsoftphone PRIVATE ${PJSIP_LIB_DIRS})

target_link_libraries(winsoftphone PRIVATE
  pjsua-lib
  pjsip-ua
  pjsip-simple
  pjsip
  pjnath
  pjmedia
  pjmedia-audiodev
  pjmedia-codec
  pjlib
  pjlib-util
  OpenSSL::SSL
  OpenSSL::Crypto
  ws2_32
  winmm
  comctl32
  shell32
  ole32
  uuid
)

if(DEFINED ENV{VCPKG_ROOT})
  set(_VCPKG_LIB_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows/lib")
endif()
find_library(SAMPLERATE_LIBRARY NAMES samplerate HINTS ${_VCPKG_LIB_DIR})
if(SAMPLERATE_LIBRARY)
  target_link_libraries(winsoftphone PRIVATE ${SAMPLERATE_LIBRARY})
endif()
