# Radeon Loom Stitching Library(vx_loomsl)
Radeon Loom Stitching Library (beta preview) is a highly optimized library for 360 degree video stitching applications.

This software is provided under a MIT-style license,  see the file COPYRIGHT.txt for details.

#### Features
* Real-time live 360 degree video stitching 
* Easy to use API for fast prototyping
* The code is highly optimized for AMD Pro Graphics
* Supports upto 31 cameras

Refer to live_stitch_api.h for details of the API.

## Build Instructions

#### Pre-requisites
* CMake 2.8 or newer [download](http://cmake.org/download/).
* CPU: SSE4.1 or above CPU, 64-bit.
* GPU: Radeon W9100 16GB or above
  * DRIVER: AMD Radeon Crimson Edition - use the latest version
  * AMD APP SDK 3.0 [download](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/).

#### Build using Visual Studio Professional 2013 on 64-bit Windows 10/8.1/7
* Use amdovx-modules\loom.sln to build for x64 platform
