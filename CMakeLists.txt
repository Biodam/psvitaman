cmake_minimum_required(VERSION 3.10)

if(DEFINED ENV{VITASDK})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
endif()

project(PSVitaman C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

include_directories(
  src
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(SOURCES
  src/main.c
  src/config.c
  src/ini.c
  src/cJSON.c
  src/utils.c
  src/spotify.c
  src/worker.c
  src/input.c
  src/ui.c
)

add_executable(psvitaman ${SOURCES})

target_link_libraries(psvitaman
  vita2d
  curl
  ssl
  crypto
  mbedtls
  png
  jpeg
  z
  m
  pthread
  SceAppMgr_stub
  SceCtrl_stub
  SceTouch_stub
  SceDisplay_stub
  SceGxm_stub
  SceSysmodule_stub
  SceNet_stub
  SceNetCtl_stub
  ScePgf_stub
  SceCommonDialog_stub
  ScePower_stub
)

vita_create_self(eboot.bin psvitaman UNSAFE)

vita_create_vpk(PSVitaman.vpk "SPOTMAN01" eboot.bin
  VERSION "01.00"
  NAME "PSVitaman"
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE assets/cacert.pem assets/cacert.pem
)
