# OpenVX Neural Network Extension Library (vx_nn)
vx_nn is an OpenVX Neural Network extension module. This implementation supports only floating-point tensor datatypes and does not support 8-bit and 16-bit fixed-point types specified in the OpenVX specification.

### Pre-requisites :
1. Ubuntu 16.04 with ROCm enabled platform.
   For further details visit the following [site](https://rocm.github.io/ROCmInstall.html) on how to install ROCm.
2. ROCm cmake modules can be installed from [here](https://github.com/RadeonOpenCompute/rocm-cmake).
3. This implementation is dependent on [MIOpen](https://github.com/ROCmSoftwarePlatform/MIOpen) and [MIOpenGemm](https://github.com/ROCmSoftwarePlatform/MIOpenGEMM). 
4. [Optional] Install the [Protobuf](https://github.com/google/protobuf) library to build [inference_generator](https://github.com/lcskrishna/amdovx-modules/tree/miopen/utils/inference_generator) that generates a GDF that can be used for inference. Make sure protoc is properly installed to run this tool.
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

### Example using CIFAR 10 caffemodel :

To use the inference_generator tool follow proper syntax as follows:

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

To generate a GDF for CIFAR 10 caffemodel run the following command, make sure to create a folder name specified and the weights and bias folders where all the weights will be extracted.

```
% inference_generator[.exe] --no-virtual-buffers --output-dir example cifar10_quick_iter_4000.caffemodel 1 3 32 32

```

This creates weights and biases of each layer which are present in the respective folders and the is GDF generated as follows :

File **net.gdf** :

```
import vx_nn
data data = tensor:4,{32,32,3,1},VX_TYPE_FLOAT32,0
read data input.f32
data conv1 = tensor:4,{32,32,32,1},VX_TYPE_FLOAT32,0
data conv1_W = tensor:4,{5,5,3,32},VX_TYPE_FLOAT32,0
init conv1_W weights/conv1.f32
data conv1_B = tensor:1,{32},VX_TYPE_FLOAT32,0
init conv1_B bias/conv1.f32
data conv1_params =  scalar:VX_TYPE_NN_CONV_PARAMS,{2,2,VX_CONVERT_POLICY_SATURATE,VX_ROUND_POLICY_TO_NEAREST_EVEN,VX_NN_DS_SIZE_ROUNDING_FLOOR,0,0}
node org.khronos.nn_extension.convolution_layer data conv1_W conv1_B conv1_params conv1

data pool1 = tensor:4,{16,16,32,1},VX_TYPE_FLOAT32,0
data pool1_type =  scalar:VX_TYPE_ENUM,VX_NN_POOLING_MAX
data pool1_kernel_w = scalar:VX_TYPE_SIZE,3
data pool1_kernel_h = scalar:VX_TYPE_SIZE,3
data pool1_pad_w = scalar:VX_TYPE_SIZE,0
data pool1_pad_h = scalar:VX_TYPE_SIZE,0
data pool1_roundPolicy =  scalar:VX_TYPE_ENUM,VX_ROUND_POLICY_TO_NEAREST_EVEN
node org.khronos.nn_extension.pooling_layer conv1 pool1_type pool1_kernel_w pool1_kernel_h pool1_pad_w pool1_pad_h pool1_roundPolicy pool1

data relu1 = tensor:4,{16,16,32,1},VX_TYPE_FLOAT32,0
data relu1_mode =  scalar:VX_TYPE_ENUM,VX_NN_ACTIVATION_RELU
data relu1_param_a = scalar:VX_TYPE_FLOAT32,0
data relu1_param_b = scalar:VX_TYPE_FLOAT32,0
node org.khronos.nn_extension.activation_layer pool1 relu1_mode relu1_param_a relu1_param_b relu1

data conv2 = tensor:4,{16,16,32,1},VX_TYPE_FLOAT32,0
data conv2_W = tensor:4,{5,5,32,32},VX_TYPE_FLOAT32,0
init conv2_W weights/conv2.f32
data conv2_B = tensor:1,{32},VX_TYPE_FLOAT32,0
init conv2_B bias/conv2.f32
data conv2_params =  scalar:VX_TYPE_NN_CONV_PARAMS,{2,2,VX_CONVERT_POLICY_SATURATE,VX_ROUND_POLICY_TO_NEAREST_EVEN,VX_NN_DS_SIZE_ROUNDING_FLOOR,0,0}
node org.khronos.nn_extension.convolution_layer relu1 conv2_W conv2_B conv2_params conv2

data relu2 = tensor:4,{16,16,32,1},VX_TYPE_FLOAT32,0
data relu2_mode =  scalar:VX_TYPE_ENUM,VX_NN_ACTIVATION_RELU
data relu2_param_a = scalar:VX_TYPE_FLOAT32,0
data relu2_param_b = scalar:VX_TYPE_FLOAT32,0
node org.khronos.nn_extension.activation_layer conv2 relu2_mode relu2_param_a relu2_param_b relu2

data pool2 = tensor:4,{8,8,32,1},VX_TYPE_FLOAT32,0
data pool2_type =  scalar:VX_TYPE_ENUM,VX_NN_POOLING_AVG
data pool2_kernel_w = scalar:VX_TYPE_SIZE,3
data pool2_kernel_h = scalar:VX_TYPE_SIZE,3
data pool2_pad_w = scalar:VX_TYPE_SIZE,0
data pool2_pad_h = scalar:VX_TYPE_SIZE,0
data pool2_roundPolicy =  scalar:VX_TYPE_ENUM,VX_ROUND_POLICY_TO_NEAREST_EVEN
node org.khronos.nn_extension.pooling_layer relu2 pool2_type pool2_kernel_w pool2_kernel_h pool2_pad_w pool2_pad_h pool2_roundPolicy pool2

data conv3 = tensor:4,{8,8,64,1},VX_TYPE_FLOAT32,0
data conv3_W = tensor:4,{5,5,32,64},VX_TYPE_FLOAT32,0
init conv3_W weights/conv3.f32
data conv3_B = tensor:1,{64},VX_TYPE_FLOAT32,0
init conv3_B bias/conv3.f32
data conv3_params =  scalar:VX_TYPE_NN_CONV_PARAMS,{2,2,VX_CONVERT_POLICY_SATURATE,VX_ROUND_POLICY_TO_NEAREST_EVEN,VX_NN_DS_SIZE_ROUNDING_FLOOR,0,0}
node org.khronos.nn_extension.convolution_layer pool2 conv3_W conv3_B conv3_params conv3

data relu3 = tensor:4,{8,8,64,1},VX_TYPE_FLOAT32,0
data relu3_mode =  scalar:VX_TYPE_ENUM,VX_NN_ACTIVATION_RELU
data relu3_param_a = scalar:VX_TYPE_FLOAT32,0
data relu3_param_b = scalar:VX_TYPE_FLOAT32,0
node org.khronos.nn_extension.activation_layer conv3 relu3_mode relu3_param_a relu3_param_b relu3

data pool3 = tensor:4,{4,4,64,1},VX_TYPE_FLOAT32,0
data pool3_type =  scalar:VX_TYPE_ENUM,VX_NN_POOLING_AVG
data pool3_kernel_w = scalar:VX_TYPE_SIZE,3
data pool3_kernel_h = scalar:VX_TYPE_SIZE,3
data pool3_pad_w = scalar:VX_TYPE_SIZE,0
data pool3_pad_h = scalar:VX_TYPE_SIZE,0
data pool3_roundPolicy =  scalar:VX_TYPE_ENUM,VX_ROUND_POLICY_TO_NEAREST_EVEN
node org.khronos.nn_extension.pooling_layer relu3 pool3_type pool3_kernel_w pool3_kernel_h pool3_pad_w pool3_pad_h pool3_roundPolicy pool3

data ip1 = tensor:4,{1,1,64,1},VX_TYPE_FLOAT32,0
data ip1_W = tensor:4,{4,4,64,64},VX_TYPE_FLOAT32,0
init ip1_W weights/ip1.f32
data ip1_B = tensor:1,{64},VX_TYPE_FLOAT32,0
init ip1_B bias/ip1.f32
data ip1_convertPolicy =  scalar:VX_TYPE_ENUM,VX_CONVERT_POLICY_SATURATE
data ip1_roundPolicy = scalar:VX_TYPE_ENUM,VX_VX_ROUND_POLICY_TO_NEAREST_EVEN
node org.khronos.nn_extension.fully_connected_layer pool3 ip1_W ip1_B ip1_convertPolicy ip1_roundPolicy ip1

data ip2 = tensor:4,{1,1,10,1},VX_TYPE_FLOAT32,0
data ip2_W = tensor:4,{1,1,64,10},VX_TYPE_FLOAT32,0
init ip2_W weights/ip2.f32
data ip2_B = tensor:1,{10},VX_TYPE_FLOAT32,0
init ip2_B bias/ip2.f32
data ip2_convertPolicy =  scalar:VX_TYPE_ENUM,VX_CONVERT_POLICY_SATURATE
data ip2_roundPolicy = scalar:VX_TYPE_ENUM,VX_VX_ROUND_POLICY_TO_NEAREST_EVEN
node org.khronos.nn_extension.fully_connected_layer ip1 ip2_W ip2_B ip2_convertPolicy ip2_roundPolicy ip2

data label = tensor:4,{1,1,10,1},VX_TYPE_FLOAT32,0
node org.khronos.nn_extension.softmax_layer ip2 label
write label output.f32
```



To convert a caffemodel file to tensor of type float32, convert_image_to_tensor kernel comes handy.
Here is a simple script that shows how to convert an image to a tensor.

```
% runvx[.exe] image_to_tensor.gdf
```

File **image_to_tensor.gdf** :


```
import vx_nn

data input  = image:32,32,RGB2
data output = tensor:4,{32,32,3,1},VX_TYPE_FLOAT32,0
data flip = scalar:UINT32,1
read input input.png
node com.amd.nn_extension.convert_image_to_tensor input output flip
write output input.f32

```

Now using the above tensor created and net.gdf file run the inference using the following command:

```
% runvx [.exe] net.gdf
```

This dumps the output as output.f32 in the respective folder. 
To view the octal dump of the dumped output use the following command:

```
% od -t f4 output.f32
```


