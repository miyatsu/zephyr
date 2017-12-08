if (NOT DEFINED BOARD)
	set(BOARD stm32f4_explo)
endif()

set(CONF_FILE "prj_${BOARD}.conf")

set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dts.overlay")

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

assert_exists(DTC_OVERLAY_FILE)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
