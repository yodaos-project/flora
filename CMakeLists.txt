CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
project(flora)
set(VERSION 0.1)

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

set(CMAKE_CXX_FLAGS "-pthread -fPIC -std=c++11")
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
#	src/*.h
#	src/*.cpp
#)
set(flora_svc_SOURCES
	include/flora-svc.h
	include/flora-cli.h
	src/defs.h
	src/adap.h
	src/tcp-adap.h
	src/tcp-adap.cc
	src/poll.cc
	src/tcp-poll.h
	src/tcp-poll.cc
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
	src/cli.h
	src/cli.cc
	src/conn.h
	src/tcp-conn.h
	src/tcp-conn.cc
	src/defs.h
	src/ser-helper.h
	src/ser-helper.cc
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
endif(BUILD_TEST)
