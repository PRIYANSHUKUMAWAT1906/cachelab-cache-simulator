cmake_minimum_required(VERSION 3.14)
project(cachelab VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CACHELAB_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(CACHELAB_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

add_library(cachelab_core STATIC
  src/cache_simulator.cpp
  src/replacement_policy.cpp
  src/trace_reader.cpp
  src/statistics.cpp
  src/benchmark.cpp)
target_include_directories(cachelab_core PUBLIC include)

add_executable(cachelab src/main.cpp)
target_link_libraries(cachelab PRIVATE cachelab_core)

enable_testing()
foreach(t test_cache test_policies test_trace_reader test_benchmark)
  add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE cachelab_core)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
