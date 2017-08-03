# Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#  
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#  
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


cmake_minimum_required (VERSION 2.8)
project (loom_shell)

set (CMAKE_CXX_STANDARD 11)

find_package(OpenVX REQUIRED)
find_package(OpenCL QUIET)

include_directories(${CMAKE_SOURCE_DIR}/vx_loomsl ${OpenVX_INCLUDE_DIRS})

list(APPEND SOURCES
	loom_shell_util.cpp
	loom_shell.cpp
	help.cpp
)

add_executable(loom_shell ${SOURCES})

target_link_libraries(loom_shell vx_loomsl ${OpenVX_LIBRARIES})

if (OpenCL_FOUND)
	include_directories(${OpenCL_INCLUDE_DIRS} ${OpenCL_INCLUDE_DIRS}/Headers)
	target_link_libraries(loom_shell ${OpenCL_LIBRARIES})
endif(OpenCL_FOUND)

if( POLICY CMP0054 )
  cmake_policy( SET CMP0054 OLD )
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

set_target_properties(loom_shell PROPERTIES
				OUTPUT_NAME loom_shell
				DEBUG_POSTFIX "d"
				)

install ( TARGETS loom_shell
          RUNTIME DESTINATION bin
          ARCHIVE DESTINATION lib
          LIBRARY DESTINATION lib )

