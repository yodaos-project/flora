CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
project(flora)
set(VERSION 0.1)

if (CROSS_COMPILE)
  include (toolchain.cmake)
endif()

set(CMAKE_CXX_STANDARD 11)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  set(COMPILER_IS_CLANG TRUE)
else()
  set(COMPILER_IS_CLANG FALSE)
endif()

option(BUILD_DEBUG "debug or release" OFF)
option(BUILD_TEST "build tests" OFF)
option(BUILD_STATIC "build static libraries" OFF)
option(BUILD_SHARED "build shared libraries" OFF)
option(USE_EPOLL "use epoll in flora service" OFF)
option(TRACE_COMMANDS "service print detail of req/resp commands" ON)
if (NOT SVC_LOGLEVEL)
  set(SVC_LOGLEVEL 2)
endif()
if (NOT CLI_LOGLEVEL)
  set(CLI_LOGLEVEL 2)
endif()
if (NOT BUILD_STATIC AND NOT BUILD_SHARED)
  set(BUILD_SHARED ON)
endif()

if (USE_EPOLL)
  set(EPOLL_MACRO "-DFLORA_USE_EPOLL")
endif()
if (TRACE_COMMANDS)
  set(TRACE_MACRO "-DTRACE_COMMANDS")
endif()

find_package(mutils REQUIRED)
find_package(caps REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (BUILD_DEBUG)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

set(flora_svc_SOURCES
  src/common.cpp
  src/svc.cpp
)
set (flora_cli_SOURCES
  src/cli.cpp
  src/common.cpp
)
if (BUILD_STATIC)
add_library(flora-svc-static STATIC
  ${flora_svc_SOURCES}
)
set_target_properties(flora-svc-static PROPERTIES
  OUTPUT_NAME flora-svc
  POSITION_INDEPENDENT_CODE ON
)
target_include_directories(flora-svc-static PRIVATE
  include
)
target_compile_options(flora-svc-static PRIVATE
  -DROKID_LOG_ENABLED=${SVC_LOGLEVEL}
  ${EPOLL_MACRO}
  ${TRACE_MACRO}
)

add_library(flora-cli-static STATIC
  ${flora_cli_SOURCES}
)
set_target_properties(flora-cli-static PROPERTIES
  OUTPUT_NAME flora-cli
  POSITION_INDEPENDENT_CODE ON
)
target_include_directories(flora-cli-static PRIVATE
  include
)
target_compile_options(flora-cli-static PRIVATE
  -DROKID_LOG_ENABLED=${CLI_LOGLEVEL}
)
install(TARGETS flora-svc-static flora-cli-static
  EXPORT floraConfig
  ARCHIVE DESTINATION lib
)
endif(BUILD_STATIC)

if (BUILD_SHARED)
add_library(flora-svc SHARED
  ${flora_svc_SOURCES}
)
target_include_directories(flora-svc PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(flora-svc
  Threads::Threads
  rokid::mutils
  rokid::caps
)
target_compile_options(flora-svc PRIVATE
  -DROKID_LOG_ENABLED=${SVC_LOGLEVEL}
  ${EPOLL_MACRO}
  ${TRACE_MACRO}
)
set_target_properties(flora-svc PROPERTIES
  PUBLIC_HEADER "include/flora-svc.h;include/flora-defs.h"
)

add_library(flora-cli SHARED
  ${flora_cli_SOURCES}
)
target_include_directories(flora-cli PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(flora-cli
  Threads::Threads
  rokid::mutils
  rokid::caps
)
target_compile_options(flora-cli PRIVATE -DROKID_LOG_ENABLED=${CLI_LOGLEVEL})
set_target_properties(flora-cli PROPERTIES
  PUBLIC_HEADER "include/flora-cli.h;include/flora-defs.h"
)
install(TARGETS flora-svc flora-cli
  EXPORT floraConfig
  LIBRARY DESTINATION lib
  PUBLIC_HEADER DESTINATION include
)
#export(TARGETS flora-svc flora-cli
#  NAMESPACE rokid::
#  FILE "${CMAKE_CURRENT_BINARY_DIR}/floraConfig.cmake"
#)
install(EXPORT floraConfig
  NAMESPACE rokid::
  DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/flora"
)
endif(BUILD_SHARED)

# unit-tests
if (BUILD_TEST)
find_package(GTest REQUIRED)
file(GLOB tests_sources tests/*.cpp)
add_executable(flora-test ${tests_sources})
target_include_directories(flora-test PRIVATE
  include
  ${GTEST_INCLUDE_DIRS}
)
if (BUILD_SHARED)
target_link_libraries(flora-test
  flora-cli
  flora-svc
  rokid::mutils
  rokid::caps
  ${GTEST_LIBRARY}
)
else(BUILD_SHARED)
target_link_libraries(flora-test
  flora-cli-static
  flora-svc-static
  rokid::mutils
  rokid::caps
  ${GTEST_LIBRARY}
)
endif(BUILD_SHARED)
endif(BUILD_TEST)
