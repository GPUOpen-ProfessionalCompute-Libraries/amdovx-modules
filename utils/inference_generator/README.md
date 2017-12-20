# Inference Generator
Convert a pre-trained CAFFE model into a C library for use by applications.
* Extract neural network model from `deploy.prototxt`
  * generate C code that instantiates OpenVX kernels from [vx_nn](../../vx_nn/README.md) module
  * generate build scripts that package C code into a library
  * the generated C code or library can be easily integrated into an application for running inference
* Extract weights and biases from `weights.caffemodel` into separates folders for use by the C library during initialization
* Also generate a GDF for quick prototyping and kernel debugging

The generated C code will have two functions in `annmodule.h`:
````
void annGetTensorDimensions(
        vx_size dimInput[4],    // input tensor dimensions
        vx_size dimOutput[4]    // output tensor dimensions
    );

vx_graph annCreateGraph(
        vx_context context,     // OpenVX context
        vx_tensor input,        // input tensor
        vx_tensor output,       // output tensor
        const char * dataFolder // folder with weights and biases
    );
````
* `annGetTensorDimensions`: allows an application to query dimensions of input and output tensors
* `annCreateGraph`: creates and initializes a graph with trained neural network for inference

## Command-line Usage
````
  % inference_generator
        [options]
        <net.prototxt|net.caffemodel>
        [n c H W [type fixed-point-position [convert-policy round-policy]]]
````
option|description
------|-----------
--[no-]virtual-buffers  | do/don't use virtual buffers (default: ON)
--[no-]generate-gdf     | do/don't generate RunVX GDF with weight/bias initialization (default: ON)
--[no-]generate-vx-code | do/don't generate OpenVX C Code with weight/bias initialization (default: ON)
--output-dir <folder>   | specify output folder for weights/biases, GDF, and OpenVX C Code (default: current)
--flags <int>           | specify custom flags (default: 0)

## Example
Make sure that all executables and libraries are in `PATH` and `LD_LIBRARY_PATH` environment variables.
````
% export PATH=$PATH:/opt/rocm/bin
% export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/rocm/lib
````

Below log outlines a simple use-case with inference generator.
````
% inference_generator weights.caffemodel 1 3 32 32
% inference_generator deploy.prototxt 1 3 32 32
% ls
CMakeLists.txt   annmodule.txt   cmake              weights
annmodule.cpp    anntest.cpp     deploy.prototxt    weights.caffemodel
annmodule.h      bias            net.gdf
% mkdir build
% cd build
% cmake ..
% make
% cd ..
% ls build
CMakeCache.txt  Makefile        cmake_install.cmake
CMakeFiles      anntest         libannmodule.so
% ./build/anntest
OK: annGetTensorDimensions() => [input 32x32x3x32] [output 1x1x10x32]
````
The `anntest.cpp` is a simple program to initialize and run neural network using the `annmodule` library.
