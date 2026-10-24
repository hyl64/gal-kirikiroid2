cmake_minimum_required(VERSION 3.20)
project(krkr2-mac CXX C)

# Kirikiroid2 (zeas2/Kirikiroid2) built natively for macOS arm64.
#
# The engine upstream is an Android/cocos2d-x application.  This tree brings
# it up on the desktop a layer at a time: the TJS2 script engine first (the
# language the KAG scenarios are written in), then the archive layer, then
# the runtime.  Each step is a real target that builds and runs on its own
# instead of one all-or-nothing port.

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES arm64)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

# the include set the Android.mk of the core asks for, minus the Android and
# cocos2d-x directories a desktop build does not have
set(KRKR_INCLUDES
	${CORE}/tjs2
	${CORE}/base
	${CORE}/base/win32
	${CORE}/environ
	${CORE}/environ/win32
	${CORE}/msg
	${CORE}/msg/win32
	${CORE}/utils
	${CORE}/utils/win32
	${CORE}/extension
	${CORE}/sound
	${CORE}/sound/win32
	${CORE}/visual
	${CORE}/visual/win32
	${CORE}/movie
)

# The old sources predate the warnings clang grew since; errors stay errors.
add_compile_options(-w)
add_compile_definitions(
	TJS_TEXT_OUT_CRLF
	__STDC_CONSTANT_MACROS
	USE_UNICODE_FSTRING
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ONIG REQUIRED oniguruma)

# ---- TJS2: the script engine -------------------------------------------------
# Everything under src/core/tjs2; it needs the message catalogue and a handful
# of helpers from base/ and utils/ that the linker will name if we miss them.
file(GLOB TJS2_SRC ${CORE}/tjs2/*.cpp)
list(APPEND TJS2_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/platform/darwin_platform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform/darwin_system.cpp)
add_library(tjs2 STATIC ${TJS2_SRC})
target_include_directories(tjs2 PUBLIC ${KRKR_INCLUDES} ${ONIG_INCLUDE_DIRS})
target_link_libraries(tjs2 PUBLIC ${ONIG_LIBRARIES})
target_link_directories(tjs2 PUBLIC ${ONIG_LIBRARY_DIRS})

# ---- tjsc: run a .tjs script -------------------------------------------------
add_executable(tjsc tools/tjsc.cpp)
target_link_libraries(tjsc PRIVATE tjs2)
target_include_directories(tjsc PRIVATE ${KRKR_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/platform)
