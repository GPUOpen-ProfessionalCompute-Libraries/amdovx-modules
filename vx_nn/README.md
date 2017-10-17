# OpenVX Neural Network Extension Library (vx_nn)
vx_nn is an OpenVX Neural Network extension module. This implementation supports only floating-point tensor datatype and does not support 8-bit and 16-bit fixed-point datatypes specified in the OpenVX specification.

### Pre-requisites :
1. Ubuntu 16.04 with ROCm enabled platform.
   For further details visit the following [site](https://rocm.github.io/ROCmInstall.html) on how to install ROCm.
2. CMake 2.8 or newer [download](https://cmake.org/download/).
3. Install the [Protobuf](https://github.com/google/protobuf) library.
3. Build and install MIOpen and it's dependents listed below.
   - [rocm-cmake](https://github.com/RadeonOpenCompute/rocm-cmake)
   - [MIOpenGemm](https://github.com/ROCmSoftwarePlatform/MIOpenGEMM)
   - [MIOpen](https://github.com/ROCmSoftwarePlatform/MIOpen)

### Build Instructions :
1. Install the above pre-requisites 
2. Follow the build instructions from [../README.md](../README.md)

After the make is successful, the executables will be present in build/bin folder. 

### List of supported neural network layers:
Layer name | Function|Kernel name
------|---------------|------------
Convolution Layer|vxConvolutionLayer|org.khronos.nn_extension.convolution_layer
Activation Layer|vxActivationLayer|org.khronos.nn_extension.activation_layer
Pooling Layer|vxPoolingLayer|org.khronos.nn_extension.pooling_layer
LRN Layer|vxNormalizationLayer|org.khronos.nn_extension.normalization_layer
Fully Connected Layer|vxFullyConnectedLayer|org.khronos.nn_extension.fully_connected_layer
Softmax Layer|vxSoftmaxLayer|org.khronos.nn_extension.softmax_layer
Deconvolution Layer|vxDeconvolutionLayer|org.khronos.nn_extension.deconvolution_layer
Elementwise Add Layer|vxElementwiseLayer|com.amd.nn_extension.elementwise_layer
Batch Normalization Layer|vxBatchNormalizationLayer|com.amd.nn_extension.batch_norm_layer
Concat Layer|vxConcatLayer|com.amd.nn_extension.concat_layer
Slice Layer|vxSliceLayer|com.amd.nn_extension.slice_layer
Copy Layer|vxCopyLayer|com.amd.nn_extension.copy_layer

### List of supported tensor operations:
Tensor Operation | Function | Kernel Name
-----------------|----------|------------
Tensor add|vxTensorAddNode |org.khronos.openvx.tensor_add
Tensor subtract|vxTensorSubtractNode|org.khronos.openvx.tensor_subtract
Tensor multiply|vxTensorMultiplyNode|org.khronos.openvx.tensor_multiply
Tensor convert depth node|vxTensorConvertDepthNode|org.khronos.openvx.tensor_convert_depth
Image to Tensor convert node|vxConvertImageToTensorNode|com.amd.nn_extension.convert_image_to_tensor
Tensor to Image convert node|vxConvertTensorToImageNode|com.amd.nn_extension.tensorToImage

### Example

```
Usage:
  % inference_generator [options] <net.caffemodel> [n c H W [type fixed-point-position [convert-policy round-policy]]]
    options:
      --[no-]virtual-buffers    - do/don't use virtual buffers (default: ON)
      --[no-]generate-gdf       - do/don't generate RunVX GDF with weight/bias initialization (default: ON)
      --[no-]generate-vx-code   - do/don't generate OpenVX C Code with weight/bias initialization (default: OFF)
      --output-dir <folder>     - specify output folder for weights/biases, GDF, and OpenVX C Code (default: current)
      --flags <int>             - specify custom flags (default: 0)

```

Here is an example to generate a quick OpenVX prototype using GDF from a pre-trained Caffe model using CIFAR 10 dataset:

```
% mkdir -o example-gdf/weights
% mkdir -o example-gdf/bias
% inference_generator --generate-gdf --output-dir example-gdf cifar10_quick_iter_4000.caffemodel 1 3 32 32
% cd example-gdf
% PATH=$PATH:path-to-build-bin
% LD_LIBRARY_PATH=$LD_LIBRARY_PATH:path-to-build-bin
% cp .../input.f32 .
% runvx -frames:2 net.gdf

```

Here is another example that generates C++ code using OpenVX API for the same:

```
% mkdir -o example-c/weights
% mkdir -o example-c/bias
% inference_generator --generate-vx-code --output-dir example-c cifar10_quick_iter_4000.caffemodel 1 3 32 32
% ls example-c
main.cpp
net.cpp
net.h

```

To convert an image to a tensor of type float32, one can use the below GDF with RunVX:

```
import vx_nn

data input  = image:32,32,RGB2
data output = tensor:4,{32,32,3,1},VX_TYPE_FLOAT32,0
data a = scalar:FLOAT32,1.0
data b = scalar:FLOAT32,0.0
data reverse_channel_order = scalar:BOOL,0
read input input.png
node com.amd.nn_extension.convert_image_to_tensor input output a b reverse_channel_order
write output input.f32

```
