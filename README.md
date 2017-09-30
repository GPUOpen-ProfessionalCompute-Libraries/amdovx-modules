# AMD OpenVX modules (amdovx-modules)
The OpenVX framework provides a mechanism to add new vision functions to OpenVX by 3rd party vendors. This project has below OpenVX modules and utilities to complement AMD OpenVX [amdovx-core](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-core) project.
* [vx_nn](vx_nn/README.md): OpenVX neural network module that was built on top of [MIOpen](https://github.com/ROCmSoftwarePlatform/MIOpen)
* [vx_loomsl](vx_loomsl/README.md): Radeon LOOM stitching library for live 360 degree video applications
* [loom_shell](utils/loom_shell/README.md): an interpreter to prototype 360 degree video stitching applications using a script
* [vx_opencv](vx_opencv/README.md): OpenVX module that implemented a mechanism to access OpenCV functionality as OpenVX kernels

## Pre-requisites
* CPU: SSE4.1 or above CPU, 64-bit
* GPU: Radeon Professional Graphics Cards or Vega Family of Products (16GB required for vx_loomsl library)
  * Windows: install the latest drivers and OpenCL SDK [download](https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/releases)
  * Linux: install [ROCm](https://rocm.github.io/ROCmInstall.html)
* OpenCV 3 (optional) [download](https://github.com/opencv/opencv/releases) for vx_opencv
  * Set OpenCV_DIR environment variable to OpenCV/build folder
* [vx_nn](vx_nn/README.md) dependencies on ROCm platform (optional)
  * [rocm-cmake](https://github.com/RadeonOpenCompute/rocm-cmake), [MIOpenGEMM](https://github.com/ROCmSoftwarePlatform/MIOpenGEMM), [MIOpen](https://github.com/ROCmSoftwarePlatform/MIOpen): requires build and install
  * [protobuf](https://github.com/google/protobuf): install `libprotobuf-dev` and `protobuf-compiler`
* git clone this project using `--recursive` option so that correct branch of the [amdovx-core](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-core) project is cloned automatically in the deps folder.

## Build using Visual Studio Professional 2013 on 64-bit Windows 10/8.1/7
* Use amdovx-modules/loom.sln to build for x64 platform

## Build using CMake on Linux (Ubuntu 16.04 64-bit) with ROCm
* Install CMake 2.8 or newer [download](http://cmake.org/download/).
* Use CMake to configure and generate Makefile
