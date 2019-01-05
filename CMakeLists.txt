CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
project(flora)
set(VERSION 0.1)
set(CMAKE_CXX_STANDARD 11)

option(USE_GCC "force use gcc" OFF)
if (USE_GCC)
SET(CMAKE_C_COMPILER gcc)
SET(CMAKE_CXX_COMPILER g++)
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  set(COMPILER_IS_CLANG TRUE)
else()
  set(COMPILER_IS_CLANG FALSE)
endif()
include (${CUSTOM_CMAKE_MODULES}/common.mk)

option(BUILD_DEBUG "debug or release" OFF)
option(BUILD_TEST "build unit-tests" OFF)

findPackage(mutils REQUIRED
  HINTS ${mutilsPrefix}
  INC_PATH_SUFFIX log caps misc
  HEADERS rlog.h caps.h uri.h
  SHARED_LIBS rlog caps
  STATIC_LIBS misc
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
if (NOT COMPILER_IS_CLANG)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
if (BUILD_DEBUG)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -DFLORA_DEBUG")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os")
  set(CMAKE_SHARED_LINKER_FLAGS "-s")
  set(CMAKE_EXE_LINKER_FLAGS "-s")
endif()
if (CROSS_COMPILE_CXXFLAGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CROSS_COMPILE_CXXFLAGS}")
endif()
set(LogLevel 2)
if (LOG_ENABLED STREQUAL "verbose")
  set(LogLevel 0)
elseif (LOG_ENABLED STREQUAL "debug")
  set(LogLevel 1)
elseif (LOG_ENABLED STREQUAL "info")
  set(LogLevel 2)
elseif (LOG_ENABLED STREQUAL "warn")
  set(LogLevel 3)
elseif (LOG_ENABLED STREQUAL "error")
  set(LogLevel 4)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROKID_LOG_ENABLED=${LogLevel}")

#file(GLOB_RECURSE UWS_SOURCES 
# src/*.h
# src/*.cpp
#)
set(flora_svc_SOURCES
  include/flora-svc.h
  include/flora-cli.h
  src/defs.h
  src/adap.h
  src/sock-adap.h
  src/sock-adap.cc
  src/poll.cc
  src/sock-poll.h
  src/sock-poll.cc
  src/disp.h
  src/disp.cc
  src/reply-mgr.h
  src/reply-mgr.cc
  src/ser-helper.h
  src/ser-helper.cc
)
add_library(flora-svc SHARED
  ${flora_svc_SOURCES}
)
target_include_directories(flora-svc
  PRIVATE
    ${mutils_INCLUDE_DIRS}
  PUBLIC
    include
)
target_link_libraries(flora-svc
  ${mutils_LIBRARIES}
)

set(flora_cli_SOURCES
  include/flora-cli.h
  include/flora-agent.h
  src/cli.h
  src/cli.cc
  src/conn.h
  src/sock-conn.h
  src/sock-conn.cc
  src/defs.h
  src/ser-helper.h
  src/ser-helper.cc
  src/flora-agent.cc
)
add_library(flora-cli SHARED
  ${flora_cli_SOURCES}
)
target_include_directories(flora-cli
  PRIVATE
    ${mutils_INCLUDE_DIRS}
  PUBLIC
    include
)
target_link_libraries(flora-cli
  ${mutils_LIBRARIES}
)

# install
install(TARGETS flora-svc flora-cli
  LIBRARY DESTINATION lib
)
file(GLOB flora_headers include/*.h)
install(FILES ${flora_headers} DESTINATION include)

# unit-tests
if (BUILD_TEST)
file(GLOB flora_unit_test_SOURCES
  unit-tests/*.h
  unit-tests/*.cc
)
add_executable(flora-test
  ${flora_unit_test_SOURCES}
)
target_include_directories(flora-test
  PRIVATE include ${mutils_INCLUDE_DIRS}
)
target_link_libraries(flora-test
  flora-cli
  flora-svc
  ${mutils_LIBRARIES}
)
set_target_properties(flora-test PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS flora-test RUNTIME DESTINATION bin)

file(GLOB flora_demo_SOURCES
  examples/*.h
  examples/*.cc
)
add_executable(flora-demo
  ${flora_demo_SOURCES}
)
target_include_directories(flora-demo
  PRIVATE include ${mutils_INCLUDE_DIRS}
)
target_link_libraries(flora-demo
  flora-cli
  flora-svc
  ${mutils_LIBRARIES}
)
endif(BUILD_TEST)
