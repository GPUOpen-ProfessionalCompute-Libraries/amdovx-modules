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
project (camera_parameter_detector)

set (CMAKE_CXX_STANDARD 11)

find_package(OpenCV REQUIRED)

list(APPEND SOURCES
	src/apply_color_correction.cpp
	src/calculate_color_correction.cpp
	src/color_correction_matrix.cpp
	src/inputparameters.cpp
	src/rect.cpp
	src/rectangular_list.cpp
	src/Source.cpp
	src/utils.cpp
)

add_executable(camera_parameter_detector ${SOURCES})

include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(camera_parameter_detector ${OpenCV_LIBRARIES})
target_compile_definitions(camera_parameter_detector PUBLIC USE_OPENCV_CONTRIB=0)

if( POLICY CMP0054 )
  cmake_policy( SET CMP0054 OLD )
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()
