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

findPackage(mutils REQUIRED
  HINTS ${mutilsPrefix}
  INC_PATH_SUFFIX log caps misc
  HEADERS rlog.h caps.h uri.h
  SHARED_LIBS rlog caps
  STATIC_LIBS misc
)

findPackage(ncurses
  HINTS ${ncursesPrefix}
  HEADERS curses.h
  SHARED_LIBS ncurses
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
  src/ser-helper.h
  src/ser-helper.cc
)
add_library(flora-svc SHARED
  ${flora_svc_SOURCES}
)
target_include_directories(flora-svc PRIVATE
    include
    ${mutils_INCLUDE_DIRS}
)
target_link_libraries(flora-svc
  ${mutils_LIBRARIES}
)
target_compile_options(flora-svc PRIVATE -DROKID_LOG_ENABLED=${SVC_LOGLEVEL})

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
target_include_directories(flora-cli PRIVATE
    include
    ${mutils_INCLUDE_DIRS}
)
target_link_libraries(flora-cli
  ${mutils_LIBRARIES}
)
target_compile_options(flora-cli PRIVATE -DROKID_LOG_ENABLED=${CLI_LOGLEVEL})

if (ncurses_LIBRARIES)
file(GLOB monitor_SOURCES
  monitor/*.cc
  monitor/*.h
)
add_executable(flora-monitor ${monitor_SOURCES})
target_include_directories(flora-monitor PRIVATE
  include
  ${mutils_INCLUDE_DIRS}
  ${ncurses_INCLUDE_DIRS}
)
target_link_libraries(flora-monitor
  flora-cli
  ${mutils_LIBRARIES}
  ${ncurses_LIBRARIES}
)
install(TARGETS flora-monitor
  RUNTIME DESTINATION bin
)
endif(ncurses_LIBRARIES)

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
