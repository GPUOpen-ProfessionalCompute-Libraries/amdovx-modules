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
project(inference_generator)

set (CMAKE_CXX_STANDARD 11)

find_package(Protobuf REQUIRED)

file( DOWNLOAD  "https://raw.githubusercontent.com/BVLC/caffe/master/src/caffe/proto/caffe.proto" "${CMAKE_SOURCE_DIR}/proto/caffe.proto")
execute_process( COMMAND protoc -I=${CMAKE_SOURCE_DIR}/proto/ --cpp_out=${CMAKE_SOURCE_DIR}/proto/ ${CMAKE_SOURCE_DIR}/proto/caffe.proto)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/bin CACHE PATH "Directory where inference generator.exe files are generated." FORCE)

include_directories(${CMAKE_SOURCE_DIR}/proto )

list(APPEND SOURCES
	src/inference_generator.cpp
	proto/caffe.pb.cc
    )

add_executable(${PROJECT_NAME} ${SOURCES} )
target_link_libraries(${PROJECT_NAME} ${PROTOBUF_LIBRARIES})

list(APPEND SOURCE_TEST_1
	src/bin2proto.cpp
	proto/caffe.pb.cc
    )

add_executable(bin2proto ${SOURCE_TEST_1} )
target_link_libraries(bin2proto ${PROTOBUF_LIBRARIES})

list(APPEND SOURCE_TEST_2
	src/proto2bin.cpp
	proto/caffe.pb.cc
    )

add_executable(proto2bin ${SOURCE_TEST_2} )
target_link_libraries(proto2bin ${PROTOBUF_LIBRARIES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/weights)
file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/bias)
