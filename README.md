# AMD OpenVX modules (amdovx-modules)
The OpenVX framework provides a mechanism to add new vision functions to OpenVX by 3rd party vendors. This project has below OpenVX modules and utilities to complement AMD OpenVX [amdovx-core](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-core) project.
* [vx_loomsl](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-modules/tree/master/vx_loomsl/README.md): Radeon LOOM stitching library for live 360 degree video applications
* [loom_shell](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-modules/tree/master/utils/loom_shell/README.md): an interpreter to prototype 360 degree video stitching applications using a script
* [vx_ext_cv](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-modules/tree/master/vx_ext_cv/README.md): OpenVX module that implemented a mechanism to access OpenCV functionality as OpenVX kernels

## Pre-requisites
* CPU: SSE4.1 or above CPU, 64-bit
* GPU: Radeon W9100 16GB or above (required for vx_loomsl library)
  * DRIVER: AMD Radeon Crimson Edition - use the latest version
  * AMD APP SDK 3.0 [download](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/).
* Download [amdovx-core](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-core) project at the same level folder as amdovx-modules build folder

## Build using Visual Studio Professional 2013 on 64-bit Windows 10/8.1/7
* Use amdovx-modules/loom.sln to build for x64 platform

## Build using CMake on Linux (Ubuntu 15.10 64-bit)
* Install CMake 2.8 or newer [download](http://cmake.org/download/).
* Use CMake to configure and generate Makefile

## Build OpenCV extension module (vx_ext_cv)
* Install [OpenCV 3.0](https://github.com/opencv/opencv/releases/tag/3.0.0).
* Set OpenCV_DIR environment variable to OpenCV/build folder
* Use amdovx-modules/vx_ext_cv/amd_ext_cv.sln
