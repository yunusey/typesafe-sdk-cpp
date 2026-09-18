cmake_minimum_required(VERSION 3.24)

project(
  typesafe-sdk-cpp
  VERSION 0.1.0
  DESCRIPTION "Unofficial C++23 SDK for the TypeSafe AI API"
  LANGUAGES CXX)

option(TYPESAFE_BUILD_EXAMPLES "Build the cookbook examples" ON)
option(TYPESAFE_BUILD_TESTS "Build the GTest suite" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      RelWithDebInfo
      CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)

add_library(
  typesafe_sdk
  src/error.cpp
  src/json.cpp
  src/config.cpp
  src/question.cpp
  src/answer.cpp
  src/retry.cpp
  src/request.cpp
  src/logging.cpp
  src/http.cpp
  src/client.cpp)
add_library(typesafe::sdk ALIAS typesafe_sdk)

target_include_directories(
  typesafe_sdk
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(typesafe_sdk PUBLIC nlohmann_json::nlohmann_json
                                          PRIVATE CURL::libcurl)

target_compile_features(typesafe_sdk PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(typesafe_sdk PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(TYPESAFE_BUILD_EXAMPLES OR TYPESAFE_BUILD_TESTS)
  add_subdirectory(testing)
endif()

if(TYPESAFE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(TYPESAFE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(
  TARGETS typesafe_sdk
  EXPORT typesafe-sdk-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
