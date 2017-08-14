# OpenVX Neural Network Extension Library (vx_nn)
vx_nn is an OpenVX Neural Network extension module. This implementation supports only floating-point tensor datatypes and does not support 8-bit and 16-bit fixed-point types specified in the OpenVX specification.

### Pre-requisites :
1. Ubuntu 16.04 with ROCm enabled platform.
   For further details visit the following [site](https://rocm.github.io/ROCmInstall.html) on how to install ROCm.
2. ROCm cmake modules can be installed from [here](https://github.com/RadeonOpenCompute/rocm-cmake).
3. This implementation is dependent on [MIOpen](https://github.com/ROCmSoftwarePlatform/MIOpen) and [MIOpenGemm](https://github.com/ROCmSoftwarePlatform/MIOpenGEMM). 
4. Install the [Protobuf](https://github.com/google/protobuf) library to build inference_generator that generates a GDF that can be used for inference.
5. CMake 2.8 or newer [download](https://cmake.org/download/).
6. [Optional] OpenCV 3.0 [download](http://docs.opencv.org/3.0-beta/doc/tutorials/introduction/linux_install/linux_install.html)

Please find the install instructions on above dependencies on their respective repositories.

### Build Instructions :
1.  Clone the repository [amdovx-modules](https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-modules).
2.  mkdir build
3.  cd build
4.  cmake -DCMAKE_BUILD_TYPE=Release ../amdovx-modules/ [Release Mode]                
      [**OR**]                                                                                                                           
    cmake -DCMAKE_BUILD_TYPE=Debug ../amdovx-modules/ [Debug Mode]
5.  make

After the build is successful, the executables will be present in bin folder. 

### List of supported layers:
Layer Supported | Function|Kernel name
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

### How to run using runvx:
1. Create a GDF that contains the network definition.
2. [Optional] inference_generator can be used to generate a GDF.
3. Currently the implementation supports floating point tensors. Hence, additional pre-processing step is requrired to convert an image to floating point tensor which can be given as an input.
4. % runvx[.exe] file net.gdf

