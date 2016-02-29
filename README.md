# AMD OpenVX modules (amdovx-modules)
Each subfolder contains a separate OpenVX module. Look into the subfolder for more details of individual module pre-requisites and special build setup requirements.

The current release verion is 0.9 (beta preview).

## List of modules
* **vx_ext_cv**: OpenVX module that implemented a mechanism to access OpenCV functionality as OpenVX kernels

## Build Instructions

#### Pre-requisites
* OpenCV 3.0 [download](http://opencv.org/downloads.html).
* CMake 3.1 or newer [download](http://cmake.org/download/).
* Download and build [amdovx-core](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-core) project at the same level folder as amdovx-modules build folder
* OpenCV_DIR environment variable should point to OpenCV/build folder

#### Build using Visual Studio Professional 2013 on 64-bit Windows 10/8.1/7
* Use amdovx-modules/amdovx-solution.sln to build for x64 platform

#### Build using CMake on Linux (Ubuntu 15.10 64-bit)
* Use CMake to configure and generate Makefile
