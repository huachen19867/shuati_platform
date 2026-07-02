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

find_package(OpenSSL REQUIRED)

add_library(shuati_core
  src/app/app_config.cpp
  src/app/server.cpp
  src/auth/auth_service.cpp
  src/auth/password_hasher.cpp
  src/auth/role.cpp
  src/auth/session_manager.cpp
  src/auth/user_repository.cpp
  src/common/config.cpp
  src/common/crypto.cpp
  src/common/json_response.cpp
  src/common/logging.cpp
  src/judge/judge_types.cpp
  src/judge/local_cpp_runner.cpp
  src/judge/submission_service.cpp
  src/problem/problem_service.cpp
  src/problem/testcase_service.cpp
)

target_include_directories(shuati_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib
)

target_link_libraries(shuati_core
  PUBLIC
    OpenSSL::Crypto
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
    tests/unit/app_config_test.cpp
    tests/unit/auth_service_test.cpp
    tests/unit/connection_pool_test.cpp
    tests/unit/config_test.cpp
    tests/unit/json_response_test.cpp
    tests/unit/local_cpp_runner_test.cpp
    tests/unit/logging_test.cpp
    tests/unit/problem_service_test.cpp
    tests/unit/role_test.cpp
    tests/unit/server_test.cpp
    tests/unit/session_manager_test.cpp
    tests/unit/submission_service_test.cpp
    tests/unit/testcase_service_test.cpp
  )

  target_link_libraries(shuati_tests
    PRIVATE
      shuati_core
      GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(shuati_tests)
endif()
