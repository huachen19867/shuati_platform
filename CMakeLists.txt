cmake_minimum_required(VERSION 3.22)

project(
  shuati_platform
  VERSION 0.1.0
  DESCRIPTION "A lightweight C++ online judge platform"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SHUATI_BUILD_TESTS "Build unit tests" ON)

add_library(shuati_core
  src/common/config.cpp
)

target_include_directories(shuati_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib
)

add_executable(shuati_server
  src/app/main.cpp
)

target_link_libraries(shuati_server
  PRIVATE
    shuati_core
)

if(SHUATI_BUILD_TESTS)
  include(CTest)
  find_package(GTest REQUIRED)

  add_executable(shuati_tests
    tests/unit/config_test.cpp
  )

  target_link_libraries(shuati_tests
    PRIVATE
      shuati_core
      GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(shuati_tests)
endif()
