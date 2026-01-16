cmake_minimum_required(VERSION 3.20)
project(winsoftphone C)

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

target_include_directories(winsoftphone PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${PJSIP_DIR}/include
)

target_link_directories(winsoftphone PRIVATE ${PJSIP_DIR}/lib)

target_link_libraries(winsoftphone PRIVATE
  pjsua
  pjsip-ua
  pjsip
  pjmedia
  pjmedia-audiodev
  pjmedia-codec
  pjlib
  pjlib-util
  ws2_32
  winmm
  comctl32
  shell32
  ole32
  uuid
)
