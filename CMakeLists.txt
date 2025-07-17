cmake_minimum_required(VERSION 3.16)
project(NoctUI LANGUAGES C)

set(CMAKE_OSX_ARCHITECTURES arm64)

# GTK4
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# NoctLang
set(USE_TRANSLATION OFF CACHE BOOL "Disable translation" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/NoctLang)

# Library source files.
set(SOURCES
  src/main.c
)

# Add NoctUI library.
add_executable(noctui ${SOURCES})

# GTK4
target_include_directories(noctui PRIVATE ${GTK4_INCLUDE_DIRS})
target_link_directories(noctui PRIVATE ${GTK4_LIBRARY_DIRS})
target_link_libraries(noctui PRIVATE ${GTK4_LIBRARIES})

# NoctLang
target_include_directories(noctui PRIVATE {$CMAKE_CURRENT_SOURCE_DIR}/NoctLang/include)
target_link_libraries(noctui PRIVATE noctlang)

# Debug Configuration.
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_RELEASE}   -O0 -g3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} -O0 -g3")
