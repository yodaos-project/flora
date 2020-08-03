CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
project(flora)
set(VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  set(COMPILER_IS_CLANG TRUE)
else()
  set(COMPILER_IS_CLANG FALSE)
endif()
include (${CUSTOM_CMAKE_MODULES}/common.mk)

option(BUILD_DEBUG "debug or release" OFF)
option(BUILD_TEST "build tests" OFF)
option(BUILD_STATIC "build static libraries" OFF)
option(BUILD_SHARED "build shared libraries" OFF)
option(USE_EPOLL "use epoll in flora service" OFF)
option(TRACE_COMMANDS "service print detail of req/resp commands" ON)

if (NOT BUILD_STATIC AND NOT BUILD_SHARED)
  set(BUILD_SHARED ON)
endif()
function(parseLogLevel varName)
if (NOT ${varName})
  set(${varName} 2 PARENT_SCOPE)
else()
  unset(llv CACHE)
  if (${varName} STREQUAL "verbose")
    set(llv 0)
  elseif (${varName} STREQUAL "debug")
    set(llv 1)
  elseif (${varName} STREQUAL "info")
    set(llv 2)
  elseif (${varName} STREQUAL "warning")
    set(llv 3)
  elseif (${varName} STREQUAL "error")
    set(llv 4)
  elseif (${varName} STREQUAL "none")
    set(llv 5)
  else ()
    set(llv 2)
  endif()
  set(${varName} ${llv} PARENT_SCOPE)
endif()
endfunction(parseLogLevel)

parseLogLevel(SVC_LOGLEVEL)
parseLogLevel(CLI_LOGLEVEL)

if (USE_EPOLL)
  set(EPOLL_MACRO "-DFLORA_USE_EPOLL")
endif()
if (TRACE_COMMANDS)
  set(TRACE_MACRO "-DTRACE_COMMANDS")
endif()

findPackage(mutils REQUIRED
  HINTS ${mutilsPrefix}
  INC_PATH_SUFFIX include/mutils
  HEADERS rlog.h uri.h global-error.h thr-pool.h
  SHARED_LIBS mutils
)

findPackage(caps REQUIRED
  HINTS ${capsPrefix}
  INC_PATH_SUFFIX include/caps
  HEADERS caps.h
  SHARED_LIBS caps
)

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
  ${mutils_INCLUDE_DIRS}
  ${caps_INCLUDE_DIRS}
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
  ${mutils_INCLUDE_DIRS}
  ${caps_INCLUDE_DIRS}
  ${lizard_INCLUDE_DIRS}
)
target_compile_options(flora-cli-static PRIVATE
  -DROKID_LOG_ENABLED=${CLI_LOGLEVEL}
)
install(TARGETS flora-svc-static flora-cli-static
  ARCHIVE DESTINATION lib
)
endif(BUILD_STATIC)

if (BUILD_SHARED)
add_library(flora-svc SHARED
  ${flora_svc_SOURCES}
)
target_include_directories(flora-svc PRIVATE
  include
  ${mutils_INCLUDE_DIRS}
  ${caps_INCLUDE_DIRS}
)
target_link_libraries(flora-svc
  ${mutils_LIBRARIES}
  ${caps_LIBRARIES}
  Threads::Threads
)
target_compile_options(flora-svc PRIVATE
  -DROKID_LOG_ENABLED=${SVC_LOGLEVEL}
  ${EPOLL_MACRO}
  ${TRACE_MACRO}
)

add_library(flora-cli SHARED
  ${flora_cli_SOURCES}
)
target_include_directories(flora-cli PRIVATE
  include
  ${mutils_INCLUDE_DIRS}
  ${caps_INCLUDE_DIRS}
)
target_link_libraries(flora-cli
  ${mutils_LIBRARIES}
  ${caps_LIBRARIES}
  Threads::Threads
)
target_compile_options(flora-cli PRIVATE -DROKID_LOG_ENABLED=${CLI_LOGLEVEL})
install(TARGETS flora-svc flora-cli
  LIBRARY DESTINATION lib
)
endif(BUILD_SHARED)
file(GLOB flora_headers include/*.h)
install(FILES ${flora_headers} DESTINATION include)

# unit-tests
if (BUILD_TEST)
findPackage(gtest REQUIRED
  HINTS ${gtestPrefix}
  HEADERS gtest/gtest.h
  STATIC_LIBS gtest
)
file(GLOB tests_sources tests/*.cpp)
add_executable(flora-test ${tests_sources})
target_include_directories(flora-test PRIVATE
  include
  ${gtest_INCLUDE_DIRS}
  ${mutils_INCLUDE_DIRS}
  ${caps_INCLUDE_DIRS}
)
if (BUILD_SHARED)
target_link_libraries(flora-test
  flora-cli
  flora-svc
  ${mutils_LIBRARIES}
  ${caps_LIBRARIES}
  ${gtest_LIBRARIES}
)
else(BUILD_SHARED)
target_link_libraries(flora-test
  flora-cli-static
  flora-svc-static
  ${mutils_LIBRARIES}
  ${caps_LIBRARIES}
  ${gtest_LIBRARIES}
)
endif(BUILD_SHARED)
endif(BUILD_TEST)
