cmake_minimum_required(VERSION 3.20.0)

# Zephyr Utilities: retain automatic overlays and add this application's editable overlay.
if(NOT DEFINED ZEPHYR_UTILS_DEFAULT_OVERLAY)
  if(DEFINED DTC_OVERLAY_FILE)
    set(ZEPHYR_UTILS_DEFAULT_OVERLAY OFF CACHE BOOL "Include the generated default application overlay")
  else()
    set(ZEPHYR_UTILS_DEFAULT_OVERLAY ON CACHE BOOL "Include the generated default application overlay")
  endif()
endif()
if(ZEPHYR_UTILS_DEFAULT_OVERLAY AND "${BOARD}" STREQUAL "esp32s3_zero/esp32s3/procpu")
  list(PREPEND EXTRA_DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/overlays/esp32s3_zero_esp32s3_procpu.overlay")
endif()
# End Zephyr Utilities default overlay.


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zero_blink LANGUAGES C)

# Protobuf contract (copied from documentation/spec/motion.proto and
# documentation/scripts/contract/motion.options), encoded with nanopb.
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)
zephyr_nanopb_sources(app proto/motion.proto)

target_sources(app PRIVATE
  src/main.c
  src/imu.c
  src/motion_codec.c
  src/udp_sender.c
  src/wifi.c
)
