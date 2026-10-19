cmake_minimum_required(VERSION 3.16)
project(quant_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")  # SPEC section 13 asks for -O2 (CMake's default is -O3)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(CURL REQUIRED)

file(GLOB_RECURSE QUANT_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
list(FILTER QUANT_SOURCES EXCLUDE REGEX "src/main\.cpp$")

add_library(quant_core STATIC ${QUANT_SOURCES})
target_include_directories(quant_core PUBLIC include third_party)
target_link_libraries(quant_core PUBLIC CURL::libcurl)

add_executable(quant_engine src/main.cpp)
target_link_libraries(quant_engine PRIVATE quant_core)

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)
add_executable(quant_tests ${TEST_SOURCES})
target_include_directories(quant_tests PRIVATE tests)
target_link_libraries(quant_tests PRIVATE quant_core)
# Tests read fixtures by relative path, so run from the project root.
add_test(NAME quant_tests COMMAND quant_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Windows/MinGW: put the runtime DLLs next to the executables so they start from any shell.
if(WIN32)
  get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  foreach(exe quant_engine quant_tests)
    add_custom_command(TARGET ${exe} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:${exe}> -DDEST=$<TARGET_FILE_DIR:${exe}>
              -DSEARCH=${MINGW_BIN_DIR} -DOBJDUMP=${CMAKE_OBJDUMP}
              -P ${CMAKE_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake
      VERBATIM)
  endforeach()
endif()
