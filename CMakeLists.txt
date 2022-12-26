cmake_minimum_required (VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project(sandbox LANGUAGES CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0) # sudo apt-get install libgtk-3-dev
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_BASE REQUIRED gstreamer-plugins-base-1.0)

add_definitions(${GTK2_CFLAGS_OTHER})

add_executable(sandbox sandbox.cpp)

target_include_directories(sandbox PRIVATE ${GTK3_INCLUDE_DIRS} ${GST_INCLUDE_DIRS} ${GST_BASE_INCLUDE_DIRS})
target_link_directories(sandbox PRIVATE ${GTK3_LIBRARY_DIRS} ${GST_LIBRARY_DIRS} ${GST_BASE_LIBRARY_DIRS})
target_link_libraries(sandbox PRIVATE ${GTK3_LIBRARIES} ${GST_LIBRARIES} gstvideo-1.0)
