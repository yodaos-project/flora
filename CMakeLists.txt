CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
project(flora)
set(VERSION 0.1)

if (CROSS_COMPILE)
  include (toolchain.cmake)
endif()

set(CMAKE_CXX_STANDARD 11)

option(BUILD_TEST "build tests" OFF)
option(USE_EPOLL "use epoll in flora service" ON)
option(TRACE_COMMANDS "service print detail of req/resp commands" ON)
option(BUILD_SHARED_LIBS "build shared libs" ON)
if (NOT SVC_LOGLEVEL)
  set(SVC_LOGLEVEL 2)
endif()
if (NOT CLI_LOGLEVEL)
  set(CLI_LOGLEVEL 2)
endif()
if (NOT USE_EPOLL)
  set(EPOLL_MACRO "-DFLORA_NO_EPOLL")
endif()
if (TRACE_COMMANDS)
  set(TRACE_MACRO "-DTRACE_COMMANDS")
endif()

find_package(mutils REQUIRED)
find_package(caps REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(flora_svc_SOURCES
  src/common.cpp
  src/svc.cpp
)
set (flora_cli_SOURCES
  src/cli.cpp
  src/common.cpp
)

add_library(flora-svc
  ${flora_svc_SOURCES}
)
target_include_directories(flora-svc INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_include_directories(flora-svc PRIVATE include)
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
if (NOT BUILD_SHARED_LIBS)
  set_target_properties(flora-svc PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

add_library(flora-cli
  ${flora_cli_SOURCES}
)
target_include_directories(flora-cli INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_include_directories(flora-cli PRIVATE include)
target_link_libraries(flora-cli
  Threads::Threads
  rokid::mutils
  rokid::caps
)
target_compile_options(flora-cli PRIVATE
  -DROKID_LOG_ENABLED=${CLI_LOGLEVEL}
  ${EPOLL_MACRO}
)
set_target_properties(flora-cli PROPERTIES
  PUBLIC_HEADER "include/flora-cli.h;include/flora-defs.h"
)
if (NOT BUILD_SHARED_LIBS)
  set_target_properties(flora-cli PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
install(TARGETS flora-svc flora-cli
  EXPORT floraConfig
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include
)
install(EXPORT floraConfig
  NAMESPACE rokid::
  DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/flora"
)

# unit-tests
if (BUILD_TEST)
find_package(GTest REQUIRED)
file(GLOB tests_sources tests/*.cpp)
add_executable(flora-test ${tests_sources})
target_link_libraries(flora-test
  flora-cli
  flora-svc
  rokid::mutils
  rokid::caps
  GTest::gtest
)
endif(BUILD_TEST)
