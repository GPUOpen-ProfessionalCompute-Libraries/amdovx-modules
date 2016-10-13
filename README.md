# AMD OpenVX modules (amdovx-modules)
Each subfolder contains a separate OpenVX module. Look into the subfolder for more details of individual module pre-requisites and special build setup requirements.

## List of modules
* **vx_loomsl**: Radeon Loom Stitching library for 360 degree video stitching applications.
* **utils/loom_shell**: an interpreter to prototype 360 degree video stitching applications using a script
* **vx_ext_cv**: OpenVX module that implemented a mechanism to access OpenCV functionality as OpenVX kernels

## Build Instructions

#### Pre-requisites
* CMake 2.8 or newer [download](http://cmake.org/download/).
* Download [amdovx-core](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-core) project at the same level folder as amdovx-modules build folder

#### Build using Visual Studio Professional 2013 on 64-bit Windows 10/8.1/7
* Use amdovx-modules/loom.sln to build for x64 platform

#### Build using CMake on Linux (Ubuntu 15.10 64-bit)
* Use CMake to configure and generate Makefile

#### Build OpenCV extension module (vx_ext_cv)
* Install [OpenCV 3.0](http://opencv.org/downloads.html).
* Set OpenCV_DIR environment variable to OpenCV/build folder
* Use amdovx-modules/vx_ext_cv/amd_ext_cv.sln
