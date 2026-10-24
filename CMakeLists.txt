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
	${CORE}/environ/ConfigManager
	${CORE}/msg
	${CORE}/msg/win32
	${CORE}/utils
	${CORE}/utils/win32
	${CORE}/extension
	${CORE}/sound
	${CORE}/sound/win32
	${CORE}/visual
	${CORE}/visual/win32
	${CORE}/visual/gl
	${CORE}/visual/ogl
	${CORE}/visual/darwin
	${CORE}/movie
)

# The old sources predate the warnings clang grew since; errors stay errors.
add_compile_options(-w)
add_compile_definitions(
	KRKR_DESKTOP_UNPACK
	TJS_TEXT_OUT_CRLF
	__STDC_CONSTANT_MACROS
	USE_UNICODE_FSTRING
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ONIG REQUIRED oniguruma)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(LZ4 REQUIRED liblz4)

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

# ---- krkrbase: the archive and storage layer ----------------------------------
# The files of src/core/base that only need the system: the storage interface,
# the archive readers (XP3, TAR), the streams and the character set.  The
# runtime pieces (EventIntf, SystemIntf, SysInitIntf, ScriptMgnIntf) belong to
# the window layer and wait for it.  libarchive's header layout differs from
# the Android one the sources expect, so platform/compat holds the shim.
file(GLOB KRKRBASE_SRC
	${CORE}/base/BinaryStream.cpp
	${CORE}/base/CharacterSet.cpp
	${CORE}/base/PluginIntf.cpp
	${CORE}/base/StorageIntf.cpp
	${CORE}/base/TARArchive.cpp
	${CORE}/base/TextStream.cpp

	${CORE}/base/UtilStreams.cpp
	${CORE}/base/XP3Archive.cpp
	${CORE}/msg/MsgIntf.cpp
	${CORE}/utils/DebugIntf.cpp
	${CORE}/utils/*.cpp
	${CORE}/utils/win32/DebugImpl.cpp
	${CORE}/utils/win32/ThreadImpl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform/darwin/Platform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform/darwin_host.cpp
	${CORE}/utils/win32/ThreadImpl.cpp
)
file(GLOB KRKRBASE_ENC ${CORE}/utils/encoding/*.c)
add_library(krkrbase STATIC ${KRKRBASE_SRC} ${KRKRBASE_ENC})
target_include_directories(krkrbase PUBLIC ${KRKR_INCLUDES} ${CORE}/environ
	${CMAKE_CURRENT_SOURCE_DIR}/platform/darwin
	${CMAKE_CURRENT_SOURCE_DIR}/platform
	${CMAKE_CURRENT_SOURCE_DIR}/platform/compat
	/opt/homebrew/include)
find_package(OpenMP)
target_link_libraries(krkrbase PUBLIC tjs2 z archive)
if(OpenMP_CXX_FOUND)
	target_link_libraries(krkrbase PUBLIC OpenMP::OpenMP_CXX)
else()
	# without OpenMP the parallel-for runs on one thread, which is correct,
	# just slower: USING_THREADPOOL11 is off and omp pragmas are ignored
	message(STATUS "OpenMP not found: TVPExecThreadTask runs single-threaded")
endif()

# ---- krkrvis: the software blitters --------------------------------------
# visual/tvpgl.cpp is the engine's whole pixel pipeline as function pointers
# (12 600 lines), assigned to the generic C implementations in TVPInitTVPGL and
# initialised by the tables in visual/gl/blend_function.cpp.  The SIMD dispatch
# header the Android build generates has no desktop counterpart, which is why
# platform/compat/tvpgl_route.h exists; the C path is what runs here.
#
# The image decoders and the bitmap classes sit on top of this and need the
# platform bitmap layer (visual/win32/LayerBitmapImpl.cpp and the window), which
# is the next piece of work.
add_library(krkrvis STATIC
	${CORE}/visual/tvpgl.cpp
	${CORE}/visual/gl/blend_function.cpp
	${CORE}/visual/LoadPNG.cpp
	${CORE}/visual/LoadTLG.cpp
	${CORE}/visual/FontSystem.cpp
	${CORE}/visual/FreeType.cpp
	${CORE}/visual/FreeTypeFontRasterizer.cpp
	${CORE}/visual/PrerenderedFont.cpp
	${CORE}/visual/CharacterData.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform/darwin/darwin_font.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform/darwin/darwin_config.cpp
	${CORE}/visual/LayerBitmapIntf.cpp
	${CORE}/visual/darwin/LayerBitmapImpl.cpp
	${CORE}/visual/RenderManager.cpp
	${CORE}/visual/ComplexRect.cpp
	${CORE}/visual/win32/BitmapInfomation.cpp
	${CORE}/visual/win32/BitmapBitsAlloc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/platform/compat/renderer/CCTexture2D.cpp
	${CORE}/visual/argb.cpp
	${CORE}/visual/darwin/ResampleImage_Soft.cpp)
target_include_directories(krkrvis PUBLIC ${KRKR_INCLUDES}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)
target_link_libraries(krkrvis PUBLIC krkrbase ${PNG_LIBRARIES})
target_link_directories(krkrvis PUBLIC ${PNG_LIBRARY_DIRS})
target_include_directories(krkrvis PUBLIC ${PNG_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${LZ4_INCLUDE_DIRS}
	/opt/homebrew/opt/opencv@4/include/opencv4 /opt/homebrew/include
	${CMAKE_CURRENT_SOURCE_DIR}/platform/compat)
target_link_directories(krkrvis PUBLIC ${LZ4_LIBRARY_DIRS})
# Homebrew carries OpenCV 5 and OpenCV 4 side by side and both install
# libopencv_*.dylib; the render manager is written against the 4.x API
# (getAffineTransform, getPerspectiveTransform with three arguments), so the
# 4.x libraries are named by path rather than searched for.
set(OPENCV4_LIB /opt/homebrew/opt/opencv@4/lib)
target_link_libraries(krkrvis PUBLIC ${LZ4_LIBRARIES} swscale
	${OPENCV4_LIB}/libopencv_core.dylib
	${OPENCV4_LIB}/libopencv_imgproc.dylib)
target_link_directories(krkrvis PUBLIC /opt/homebrew/lib ${OPENCV4_LIB})
target_link_libraries(krkrvis PUBLIC ${FREETYPE_LIBRARIES})
target_link_directories(krkrvis PUBLIC ${FREETYPE_LIBRARY_DIRS})

add_executable(krkrbitmap tools/krkrbitmap.cpp)
target_link_libraries(krkrbitmap PRIVATE krkrvis ${SDL2_LIBRARIES})
target_link_directories(krkrbitmap PRIVATE ${SDL2_LIBRARY_DIRS})
target_include_directories(krkrbitmap PRIVATE ${KRKR_INCLUDES} ${SDL2_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)

add_executable(krkrscene tools/krkrscene.cpp)
target_link_libraries(krkrscene PRIVATE krkrvis ${SDL2_LIBRARIES})
target_link_directories(krkrscene PRIVATE ${SDL2_LIBRARY_DIRS})
target_include_directories(krkrscene PRIVATE ${KRKR_INCLUDES} ${SDL2_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)

add_executable(krkrfont tools/krkrfont.cpp)
target_link_libraries(krkrfont PRIVATE krkrvis ${SDL2_LIBRARIES})
target_link_directories(krkrfont PRIVATE ${SDL2_LIBRARY_DIRS})
target_include_directories(krkrfont PRIVATE ${KRKR_INCLUDES} ${SDL2_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)

add_executable(krkrview tools/krkrview.cpp)
target_link_libraries(krkrview PRIVATE krkrvis ${SDL2_LIBRARIES})
target_link_directories(krkrview PRIVATE ${SDL2_LIBRARY_DIRS})
target_include_directories(krkrview PRIVATE ${KRKR_INCLUDES} ${SDL2_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)

add_executable(krkrblit tools/krkrblit.cpp)
target_link_libraries(krkrblit PRIVATE krkrvis)
target_include_directories(krkrblit PRIVATE ${KRKR_INCLUDES}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)

# ---- krkrscript: a TJS2 host with the storage layer attached -----------------
add_executable(krkrscript tools/krkrscript.cpp)
target_link_libraries(krkrscript PRIVATE krkrbase)
target_include_directories(krkrscript PRIVATE ${KRKR_INCLUDES}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)

# ---- krkrpack: list and extract an XP3 archive -------------------------------
add_executable(krkrpack tools/krkrpack.cpp)
target_link_libraries(krkrpack PRIVATE krkrbase)
target_include_directories(krkrpack PRIVATE ${KRKR_INCLUDES}
	${CMAKE_CURRENT_SOURCE_DIR}/platform)

# ---- tjsc: run a .tjs script -------------------------------------------------
add_executable(tjsc tools/tjsc.cpp)
target_link_libraries(tjsc PRIVATE tjs2)
target_include_directories(tjsc PRIVATE ${KRKR_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/platform)
