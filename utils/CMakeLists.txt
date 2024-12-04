###############################################################################
#
#    RELIANOID reverse proxy Load Balancer Software License
#    This file is part of the RELIANOID reverse proxy Load Balancer solution
#
#    Copyright (C) RELIANOID
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Affero General Public License as
#    published by the Free Software Foundation, either version 3 of the
#    License, or any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Affero General Public License for more details.
#
#    You should have received a copy of the GNU Affero General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
###############################################################################
cmake_minimum_required(VERSION 3.6)
set(CMAKE_CXX_STANDARD 17)

if (EXISTS CMakeLists.txt)
    message(FATAL_ERROR"Please create a cmake build directory, running cmake form source tree root is not allowed\n")
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

set(CMAKE_ENABLE_EXPORTS TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MAINFOLDER ${PROJECT_SOURCE_DIR})

set(PROJECT_VERSION "0.0.1")
set(PROJECT_DESCRIPTION "RELIANOID core utils library")
set(PROJECT_CODENAME "utils")

# get current git info
if (EXISTS "${PROJECT_SOURCE_DIR}/.git/HEAD")
    file(READ "${PROJECT_SOURCE_DIR}/.git/HEAD"
        PROJECT_SOURCE_VERSION)
    if ("${PROJECT_SOURCE_VERSION}" MATCHES "^ref:")
        string(REGEX REPLACE "^ref: *([^ \n\r]*).*" "\\1"
            PROJECT_GIT_REF "${PROJECT_SOURCE_VERSION}")
        file(READ "${PROJECT_SOURCE_DIR}/.git/${PROJECT_GIT_REF}"
            PROJECT_SOURCE_VERSION)
    endif ()
    string(STRIP "${PROJECT_SOURCE_VERSION}"
        PROJECT_SOURCE_VERSION)
endif ()

if (UNIQUE_BUILD_DIRECTORY)
	set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/build/bin")
endif ()

set(sources
	## sources
	src/u_log.c
	src/u_network.c
	src/u_backtrace.c
	src/u_http.c
	src/u_sbuffer.c
	src/u_string.c

	## libs
	include/u_log.h
	include/u_common.h
	include/u_backtrace.h
	include/u_environment.h
	include/u_http.h
	include/u_time.h
	include/u_network.h
	include/u_sbuffer.h
	include/u_string.h
)

include_directories(${PROJECT_SOURCE_DIR}/utils/include)

add_library(utils ${sources})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -pipe -Wno-unused-function -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -g -O3")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS} -Os")

include_directories(${PROJECT_SOURCE_DIR}/include)
