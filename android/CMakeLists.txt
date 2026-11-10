cmake_minimum_required(VERSION 3.18)
project(axeolon CXX C)

# The Android version: its own renderer, touch controls and Pocket Edition-style menus (src/),
# on top of the game code it shares with the 3DS build (../source).
add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

file(GLOB PLATFORM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
# the whole game, shared with the 3DS build: the same main loop, menus, HUD, world, mobs, sound
# logic... Only the 3DS renderer is swapped for src/RendererGL.cpp; libctru, citro3d and citro2d
# are stood in for by shim/ and src/
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../source)
file(GLOB_RECURSE SHARED_SRC ${SHARED}/*.cpp)
list(REMOVE_ITEM SHARED_SRC ${SHARED}/render/Renderer.cpp ${SHARED}/render/PlaceholderAtlas.cpp)
add_library(axeolon SHARED ${PLATFORM_SRC} ${SHARED_SRC} ${SHARED}/audio/stb_vorbis.c)
target_include_directories(axeolon PRIVATE shim src third_party ../source)
target_compile_options(axeolon PRIVATE -O2 -ffp-contract=off -Wall -Wno-unused-function -Wno-unused-variable
    -Wno-unused-but-set-variable -Wno-missing-braces -Wno-sign-compare -Wno-unused-private-field)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
target_link_libraries(axeolon native_app_glue android EGL GLESv2 aaudio log m z)
