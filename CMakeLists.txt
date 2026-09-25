cmake_minimum_required(VERSION 3.10)

if(DEFINED ENV{VITASDK})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
endif()

project(PSVitaman C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")
add_definitions(-D__psp2__ -D__VITA__)

include_directories(
  src
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Parse .env if present
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.env" DOTENV_LINES)
  foreach(LINE ${DOTENV_LINES})
    string(REGEX REPLACE "#.*$" "" CLEAN_LINE "${LINE}")
    string(STRIP "${CLEAN_LINE}" CLEAN_LINE)
    if(CLEAN_LINE MATCHES "^([A-Za-z0-9_]+)=(.*)$")
      set(ENV_KEY "${CMAKE_MATCH_1}")
      set(ENV_VAL "${CMAKE_MATCH_2}")
      string(REGEX REPLACE "^[\"']|[\"']$" "" ENV_VAL "${ENV_VAL}")
      set(DOTENV_${ENV_KEY} "${ENV_VAL}")
    endif()
  endforeach()
endif()

# Resolve Spotify Client ID: CMake option/def > ENV var > .env file
if(DEFINED DEFAULT_SPOTIFY_CLIENT_ID AND NOT "${DEFAULT_SPOTIFY_CLIENT_ID}" STREQUAL "")
  set(CFG_SPOTIFY_CLIENT_ID "${DEFAULT_SPOTIFY_CLIENT_ID}")
elseif(DEFINED ENV{SPOTIFY_CLIENT_ID} AND NOT "$ENV{SPOTIFY_CLIENT_ID}" STREQUAL "")
  set(CFG_SPOTIFY_CLIENT_ID "$ENV{SPOTIFY_CLIENT_ID}")
elseif(DEFINED DOTENV_SPOTIFY_CLIENT_ID AND NOT "${DOTENV_SPOTIFY_CLIENT_ID}" STREQUAL "")
  set(CFG_SPOTIFY_CLIENT_ID "${DOTENV_SPOTIFY_CLIENT_ID}")
else()
  set(CFG_SPOTIFY_CLIENT_ID "")
endif()

if(NOT "${CFG_SPOTIFY_CLIENT_ID}" STREQUAL "")
  add_definitions(-DDEFAULT_SPOTIFY_CLIENT_ID="${CFG_SPOTIFY_CLIENT_ID}")
  message(STATUS "Configuring PSVitaman with embedded Spotify Client ID: ${CFG_SPOTIFY_CLIENT_ID}")
endif()

# Optional Client Secret
if(DEFINED DEFAULT_SPOTIFY_CLIENT_SECRET AND NOT "${DEFAULT_SPOTIFY_CLIENT_SECRET}" STREQUAL "")
  set(CFG_SPOTIFY_CLIENT_SECRET "${DEFAULT_SPOTIFY_CLIENT_SECRET}")
elseif(DEFINED ENV{SPOTIFY_CLIENT_SECRET} AND NOT "$ENV{SPOTIFY_CLIENT_SECRET}" STREQUAL "")
  set(CFG_SPOTIFY_CLIENT_SECRET "$ENV{SPOTIFY_CLIENT_SECRET}")
elseif(DEFINED DOTENV_SPOTIFY_CLIENT_SECRET AND NOT "${DOTENV_SPOTIFY_CLIENT_SECRET}" STREQUAL "")
  set(CFG_SPOTIFY_CLIENT_SECRET "${DOTENV_SPOTIFY_CLIENT_SECRET}")
else()
  set(CFG_SPOTIFY_CLIENT_SECRET "")
endif()

if(NOT "${CFG_SPOTIFY_CLIENT_SECRET}" STREQUAL "")
  add_definitions(-DDEFAULT_SPOTIFY_CLIENT_SECRET="${CFG_SPOTIFY_CLIENT_SECRET}")
endif()

# Optional Refresh Token
if(DEFINED DEFAULT_SPOTIFY_REFRESH_TOKEN AND NOT "${DEFAULT_SPOTIFY_REFRESH_TOKEN}" STREQUAL "")
  set(CFG_SPOTIFY_REFRESH_TOKEN "${DEFAULT_SPOTIFY_REFRESH_TOKEN}")
elseif(DEFINED ENV{SPOTIFY_REFRESH_TOKEN} AND NOT "$ENV{SPOTIFY_REFRESH_TOKEN}" STREQUAL "")
  set(CFG_SPOTIFY_REFRESH_TOKEN "$ENV{SPOTIFY_REFRESH_TOKEN}")
elseif(DEFINED DOTENV_SPOTIFY_REFRESH_TOKEN AND NOT "${DOTENV_SPOTIFY_REFRESH_TOKEN}" STREQUAL "")
  set(CFG_SPOTIFY_REFRESH_TOKEN "${DOTENV_SPOTIFY_REFRESH_TOKEN}")
else()
  set(CFG_SPOTIFY_REFRESH_TOKEN "")
endif()

if(NOT "${CFG_SPOTIFY_REFRESH_TOKEN}" STREQUAL "")
  add_definitions(-DDEFAULT_SPOTIFY_REFRESH_TOKEN="${CFG_SPOTIFY_REFRESH_TOKEN}")
endif()

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
  src/http_server.c
  src/qrcodegen.c
  src/compat_openssl.c
  src/logger.c
  src/error.c
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
  zstd
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
