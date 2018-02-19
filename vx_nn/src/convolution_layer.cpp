/*
Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "kernels.h"

struct ConvolutionLayerLocalData {
    NeuralNetworkCommonHandle * handle;
    float alpha;
    float beta;
    miopenTensorDescriptor_t input_desc;
    cl_mem input_mem;
    miopenTensorDescriptor_t weight_desc;
    cl_mem weight_mem;
    miopenConvolutionDescriptor_t conv_desc;
    miopenConvFwdAlgorithm_t algo;
    miopenTensorDescriptor_t output_desc;
    cl_mem output_mem;
    cl_mem workspace;
    size_t workspace_size;
    miopenTensorDescriptor_t bias_desc;
    cl_mem bias_mem;
};

static vx_status VX_CALLBACK validateConvolutionLayer(vx_node node, const vx_reference parameters[], vx_uint32 num, vx_meta_format metas[])
{
    // check scalar type
    vx_enum type;
    ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[3], VX_SCALAR_TYPE, &type, sizeof(type)));
    if(type != VX_TYPE_NN_CONVOLUTION_PARAMS) return ERRMSG(VX_ERROR_INVALID_TYPE, "validate: conv: #3 type=%d (must be CONV_PARAMS)\n", type);

    // check tensor dimensions
    vx_size num_dims;
    vx_size input_dims[4], weights_dims[4], output_dims[4];
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    if(num_dims != 4) return ERRMSG(VX_ERROR_INVALID_DIMENSION, "validate: conv: #0 num_dims=%ld (must be 4)\n", num_dims);
    if(type != VX_TYPE_FLOAT32) return ERRMSG(VX_ERROR_INVALID_TYPE, "validate: conv: #0 type=%d (must be float)\n", type);
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DIMS, input_dims, sizeof(input_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    if(num_dims != 4) return ERRMSG(VX_ERROR_INVALID_DIMENSION, "validate: conv: #1 num_dims=%ld (must be 4)\n", num_dims);
    if(type != VX_TYPE_FLOAT32) return ERRMSG(VX_ERROR_INVALID_TYPE, "validate: conv: #1 type=%d (must be float)\n", type);
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_DIMS, weights_dims, sizeof(weights_dims)));
    if(parameters[2]) {
        ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[2], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
        ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[2], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
        if(num_dims != 1 && num_dims != 2) return ERRMSG(VX_ERROR_INVALID_DIMENSION, "validate: conv: #2 num_dims=%ld (must be 1 or 2)\n", num_dims);
        if(type != VX_TYPE_FLOAT32) return ERRMSG(VX_ERROR_INVALID_TYPE, "validate: conv: #2 type=%d (must be float)\n", type);
        vx_size bias_dims[2] = { 0, 1 };
        ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[2], VX_TENSOR_DIMS, bias_dims, num_dims*sizeof(bias_dims[0])));
        if(bias_dims[0] != weights_dims[3] || bias_dims[1] != 1) return ERRMSG(VX_ERROR_INVALID_DIMENSION, "validate: conv: bias[%ldx%ld] weights[%ldx%ldx%ldx%ld]\n", bias_dims[1], bias_dims[0], weights_dims[3], weights_dims[2], weights_dims[1], weights_dims[0]);
    }
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[4], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[4], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    if(num_dims != 4) return ERRMSG(VX_ERROR_INVALID_DIMENSION, "validate: conv: #4 num_dims=%ld (must be 4)\n", num_dims);
    if(type != VX_TYPE_FLOAT32) return ERRMSG(VX_ERROR_INVALID_TYPE, "validate: conv: #4 type=%d (must be float)\n", type);
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[4], VX_TENSOR_DIMS, output_dims, sizeof(output_dims)));

    if(output_dims[3] != input_dims[3] || input_dims[2] != weights_dims[2] || output_dims[2] != weights_dims[3])
        return ERRMSG(VX_ERROR_INVALID_DIMENSION, "validate: conv: input[%ldx%ldx%ldx%ld] weights[%ldx%ldx%ldx%ld] output[%ldx%ldx%ldx%ld]\n",
            input_dims[3], input_dims[2], input_dims[1], input_dims[0],
            weights_dims[3], weights_dims[2], weights_dims[1], weights_dims[0],
            output_dims[3], output_dims[2], output_dims[1], output_dims[0]);

    // output tensor configuration
    type = VX_TYPE_FLOAT32;
    num_dims = 4;
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[4], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[4], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[4], VX_TENSOR_DIMS, output_dims, sizeof(output_dims)));
    return VX_SUCCESS;
}

static vx_status VX_CALLBACK processConvolutionLayer(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
    ConvolutionLayerLocalData * data= NULL;
    ERROR_CHECK_STATUS(vxQueryNode(node, VX_NODE_LOCAL_DATA_PTR, &data, sizeof(data)));

    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_BUFFER_OPENCL, &data->input_mem, sizeof(data->input_mem)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[4], VX_TENSOR_BUFFER_OPENCL, &data->output_mem, sizeof(data->output_mem)));

    //ConvolutionForward.
    ERROR_CHECK_MIOPEN_STATUS(miopenConvolutionForward(data->handle->miopen_handle, &data->alpha, data->input_desc, data->input_mem,
                                                       data->weight_desc,data->weight_mem,data->conv_desc,data->algo,&data->beta, data->output_desc, data->output_mem, data->workspace, data->workspace_size));
    //Convolution Forward Bias.
	if(parameters[2]) {
		ERROR_CHECK_MIOPEN_STATUS(miopenConvolutionForwardBias(data->handle->miopen_handle, &data->alpha, data->bias_desc, data->bias_mem,
                                                           &data->beta, data->output_desc, data->output_mem));
    }

    return VX_SUCCESS;
}

static vx_status VX_CALLBACK initializeConvolutionLayer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    ConvolutionLayerLocalData * data = new ConvolutionLayerLocalData;
    memset(data, 0, sizeof(*data));
    ERROR_CHECK_STATUS(createGraphHandle(node, &data->handle));

    //convolution params.
    vx_nn_convolution_params_t params;
    ERROR_CHECK_STATUS(vxCopyScalar((vx_scalar)parameters[3], &params, VX_READ_ONLY, VX_MEMORY_TYPE_HOST));

    vx_size pad_h, pad_w;
    vx_size dilation_w, dilation_h;
    vx_enum downscale_size_rounding, overflow_policy, rounding_policy;

    pad_h = params.padding_y; pad_w = params.padding_x;
    downscale_size_rounding = params.down_scale_size_rounding;
    overflow_policy = params.overflow_policy;
    rounding_policy = params.rounding_policy;
    dilation_h = params.dilation_y + 1;
    dilation_w = params.dilation_x + 1;
    miopenConvolutionMode_t mode = miopenConvolution;

    vx_size input_dims[4], weights_dims[4], output_dims[4], bias_dims[2] = { 0, 1 };
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DIMS, input_dims, sizeof(input_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_DIMS, weights_dims, sizeof(weights_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[4], VX_TENSOR_DIMS, output_dims, sizeof(output_dims)));
    if(parameters[2]) {
        vx_size num_dims;
        ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[2], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(vx_size)));
        ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[2], VX_TENSOR_DIMS, bias_dims, num_dims * sizeof(vx_size)));
    }

    vx_size stride_h, stride_w;
    vx_size kernel_h, kernel_w;

    kernel_h = weights_dims[1];
    kernel_w = weights_dims[0];
    stride_w = (output_dims[0] > 1) ? ((input_dims[0] + 2 * pad_w - kernel_w - (kernel_w - 1) * (dilation_w - 1) + ((output_dims[0] - 1) / 2)) / (output_dims[0] - 1)) : 1;
    stride_h = (output_dims[1] > 1) ? ((input_dims[1] + 2 * pad_h - kernel_h - (kernel_h - 1) * (dilation_h - 1) + ((output_dims[1] - 1) / 2)) / (output_dims[1] - 1)) : 1;

    //input, weight and output descriptors.
    ERROR_CHECK_MIOPEN_STATUS(miopenCreateTensorDescriptor(&data->input_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenCreateTensorDescriptor(&data->weight_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenCreateTensorDescriptor(&data->output_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenCreateTensorDescriptor(&data->bias_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenSet4dTensorDescriptor(data->input_desc, miopenFloat, input_dims[3], input_dims[2], input_dims[1], input_dims[0]));
    ERROR_CHECK_MIOPEN_STATUS(miopenSet4dTensorDescriptor(data->weight_desc, miopenFloat, weights_dims[3], weights_dims[2], weights_dims[1], weights_dims[0]));
    ERROR_CHECK_MIOPEN_STATUS(miopenSet4dTensorDescriptor(data->output_desc, miopenFloat, output_dims[3], output_dims[2], output_dims[1], output_dims[0]));
    if(parameters[2]) {
        ERROR_CHECK_MIOPEN_STATUS(miopenSet4dTensorDescriptor(data->bias_desc, miopenFloat, 1, bias_dims[0], 1, 1));
    }

    //Convolution Descriptor.
    ERROR_CHECK_MIOPEN_STATUS(miopenCreateConvolutionDescriptor(&data->conv_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenInitConvolutionDescriptor(data->conv_desc, mode, pad_h, pad_w, stride_h, stride_w, dilation_h, dilation_w));

    //Memory Declaration.
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_BUFFER_OPENCL, &data->input_mem, sizeof(data->input_mem)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[4], VX_TENSOR_BUFFER_OPENCL, &data->output_mem, sizeof(data->output_mem)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_BUFFER_OPENCL, &data->weight_mem, sizeof(data->weight_mem)));
    if(parameters[2]) {
        ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[2], VX_TENSOR_BUFFER_OPENCL, &data->bias_mem, sizeof(data->bias_mem)));
    }

    //Workspace Size.
    ERROR_CHECK_MIOPEN_STATUS(miopenConvolutionForwardGetWorkSpaceSize(data->handle->miopen_handle, data->weight_desc, data->input_desc, data->conv_desc, data->output_desc, &data->workspace_size ));
    if (data->workspace_size > 0) {
        vx_context   vxContext = vxGetContext((vx_reference)node);
        cl_context context;
        ERROR_CHECK_STATUS(vxQueryContext(vxContext, VX_CONTEXT_ATTRIBUTE_AMD_OPENCL_CONTEXT, &context, sizeof(context)));
        data->workspace_size = (data->workspace_size + 3) & ~3;
        data->workspace = clCreateBuffer(context, CL_MEM_READ_WRITE, data->workspace_size, NULL, NULL);
        if (!data->workspace) {
            return VX_FAILURE;
        }
        cl_float pattern = 0;
        cl_int err = clEnqueueFillBuffer(data->handle->cmdq, data->workspace, &pattern, sizeof(cl_float), 0, data->workspace_size, 0, NULL, NULL);
        if(err) return VX_FAILURE;
    }

    data->alpha = 1;
    data->beta = 0;

    //Finding best Convolution Algorithm.
    miopenConvAlgoPerf_t perf;
    int algo_count;
    ERROR_CHECK_MIOPEN_STATUS(miopenFindConvolutionForwardAlgorithm(data->handle->miopen_handle, data->input_desc, data->input_mem, data->weight_desc, data->weight_mem,
                                                                    data->conv_desc, data->output_desc, data->output_mem, 1, &algo_count, &perf, data->workspace, data->workspace_size, data->handle->exhaustiveSearch));
    data->algo = perf.fwd_algo;

#if ENABLE_DEBUG_PRINT_DIMS
    std::cout << "conv input " << input_dims[0] << " " << input_dims[1] << " " << input_dims[2] << " " << input_dims[3] << " ";
    std::cout << "weights " << weights_dims[0] << " " << weights_dims[1] << " "<< weights_dims[2] <<" " <<  weights_dims[3] << " ";
    std::cout << "bias " << bias_dims[0] << " ";
    std::cout << "stride " << stride_h << " " << stride_w << " " << "pad " << pad_h << " " << pad_w;
    std::cout << " output " << output_dims[0] << " " << output_dims[1] << " " << output_dims[2] << " " << output_dims[3] << std::endl;
#endif

    ERROR_CHECK_STATUS(vxSetNodeAttribute(node, VX_NODE_LOCAL_DATA_PTR, &data, sizeof(data)));

    return VX_SUCCESS;
}

static vx_status VX_CALLBACK uninitializeConvolutionLayer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    ConvolutionLayerLocalData * data = NULL;
    ERROR_CHECK_STATUS(vxQueryNode(node, VX_NODE_LOCAL_DATA_PTR, &data, sizeof(data)));
    if(data->workspace && clReleaseMemObject(data->workspace) != 0) return VX_FAILURE;
    ERROR_CHECK_MIOPEN_STATUS(miopenDestroyConvolutionDescriptor(data->conv_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenDestroyTensorDescriptor(data->input_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenDestroyTensorDescriptor(data->output_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenDestroyTensorDescriptor(data->weight_desc));
    ERROR_CHECK_MIOPEN_STATUS(miopenDestroyTensorDescriptor(data->bias_desc));
    if (data) {
        ERROR_CHECK_STATUS(releaseGraphHandle(node, data->handle));
        delete data;
    }
    return VX_SUCCESS;
}

vx_status publishConvolutionLayer(vx_context context)
{
    // add kernel to the context with callbacks
    vx_kernel kernel = vxAddUserKernel(context, "org.khronos.nn_extension.convolution_layer", VX_KERNEL_CONVOLUTION_LAYER, processConvolutionLayer, 5, validateConvolutionLayer, initializeConvolutionLayer, uninitializeConvolutionLayer);
    ERROR_CHECK_OBJECT(kernel);

    // enable OpenCL buffer access since the kernel_f callback uses OpenCL buffers instead of host accessible buffers
    vx_bool enableBufferAccess = vx_true_e;
    ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_BUFFER_ACCESS_ENABLE, &enableBufferAccess, sizeof(enableBufferAccess)));

    // set kernel parameters
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED));
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED));
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_OPTIONAL));
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 3, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED));
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 4, VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED));

    // finalize and release kernel object
    ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
    ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

    return VX_SUCCESS;
}

VX_API_ENTRY vx_node VX_API_CALL vxConvolutionLayer(vx_graph graph, vx_tensor inputs, vx_tensor weights, vx_tensor biases,
                                                    const vx_nn_convolution_params_t *convolution_params, vx_size size_of_convolution_params, vx_tensor outputs)
{
    vx_node node = NULL;
    vx_context context = vxGetContext((vx_reference)graph);
    if(vxGetStatus((vx_reference)context) == VX_SUCCESS) {
        vx_scalar conv_params = vxCreateScalarWithSize(context, VX_TYPE_NN_CONVOLUTION_PARAMS, convolution_params, size_of_convolution_params);
        if(vxGetStatus((vx_reference)conv_params) == VX_SUCCESS) {
            vx_reference params[] = {
                (vx_reference)inputs,
                (vx_reference)weights,
                (vx_reference)biases,
                (vx_reference)conv_params,
                (vx_reference)outputs
            };
            node = createNode(graph, VX_KERNEL_CONVOLUTION_LAYER, params, sizeof(params)/sizeof(params[0]));
            vxReleaseScalar(&conv_params);
        }
    }
    return node;
}
