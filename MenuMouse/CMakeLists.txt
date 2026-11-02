cmake_minimum_required(VERSION 3.20)
project(MenuMouse LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "EDF 4.1 is 64-bit; configure with -A x64")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_library(minhook STATIC
	third_party/minhook/src/buffer.c
	third_party/minhook/src/hook.c
	third_party/minhook/src/trampoline.c
	third_party/minhook/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC third_party/minhook/include)

add_library(MenuMouse SHARED
	src/dllmain.cpp
	src/config.cpp
	src/log.cpp
	src/sigscan.cpp
	src/hooks.cpp
	src/ui_cursor.cpp
)
target_link_libraries(MenuMouse PRIVATE minhook user32)
target_compile_definitions(MenuMouse PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(MenuMouse PRIVATE /W4 /permissive-)
