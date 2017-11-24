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

static vx_status VX_CALLBACK validateTensorUpsamling(vx_node node, const vx_reference parameters[], vx_uint32 num, vx_meta_format metas[])
{

    // check tensor dims.
    vx_enum type;
    vx_size num_dims;
    vx_size input_dims[4],  output_dims[4];
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    if (num_dims != 4) return VX_ERROR_INVALID_DIMENSION;
    if (type != VX_TYPE_FLOAT32) return VX_ERROR_INVALID_TYPE;
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DIMS, input_dims, sizeof(input_dims)));

 

    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    if (num_dims != 4) return VX_ERROR_INVALID_DIMENSION;
    if (type != VX_TYPE_FLOAT32) return VX_ERROR_INVALID_TYPE;
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_DIMS, output_dims, sizeof(output_dims)));

    if (output_dims[1] != 2*input_dims[1]) return VX_ERROR_INVALID_DIMENSION;
    if (output_dims[0] != 2*input_dims[0]) return VX_ERROR_INVALID_DIMENSION;

    // output tensor configuration
    type = VX_TYPE_FLOAT32;
    num_dims = 4;
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[1], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[1], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[1], VX_TENSOR_DIMS, output_dims, sizeof(output_dims)));
    return VX_SUCCESS;
}

//! \brief The kernel target support callback.
static vx_status VX_CALLBACK query_target_support(vx_graph graph, vx_node node,
    vx_bool use_opencl_1_2,              // [input]  false: OpenCL driver is 2.0+; true: OpenCL driver is 1.2
    vx_uint32& supported_target_affinity // [output] must be set to AGO_TARGET_AFFINITY_CPU or AGO_TARGET_AFFINITY_GPU or (AGO_TARGET_AFFINITY_CPU | AGO_TARGET_AFFINITY_GPU)
)
{
    supported_target_affinity = AGO_TARGET_AFFINITY_GPU;
    return VX_SUCCESS;
}

//! \brief The OpenCL code generator callback.
static vx_status VX_CALLBACK opencl_codegen(
    vx_node node,                                  // [input] node
    const vx_reference parameters[],               // [input] parameters
    vx_uint32 num,                                 // [input] number of parameters
    bool opencl_load_function,                     // [input]  false: normal OpenCL kernel; true: reserved
    char opencl_kernel_function_name[64],          // [output] kernel_name for clCreateKernel()
    std::string& opencl_kernel_code,               // [output] string for clCreateProgramWithSource()
    std::string& opencl_build_options,             // [output] options for clBuildProgram()
    vx_uint32& opencl_work_dim,                    // [output] work_dim for clEnqueueNDRangeKernel()
    vx_size opencl_global_work[],                  // [output] global_work[] for clEnqueueNDRangeKernel()
    vx_size opencl_local_work[],                   // [output] local_work[] for clEnqueueNDRangeKernel()
    vx_uint32& opencl_local_buffer_usage_mask,     // [output] reserved: must be ZERO
    vx_uint32& opencl_local_buffer_size_in_bytes   // [output] reserved: must be ZERO
)
{
    //get tensor dimensions
    vx_size input_dims[4], output_dims[4];
    vx_size num_of_dims;
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_NUMBER_OF_DIMS, &num_of_dims, sizeof(num_of_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DIMS, input_dims, sizeof(input_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[1], VX_TENSOR_DIMS, output_dims, sizeof(output_dims)));

#if ENABLE_DEBUG_PRINT_DIMS
    std::cout << "tensor_sub input " << input_dims[3] << " " << input_dims[2] << " " << input_dims[1] << " " << input_dims[0] << " ";
    std::cout << "tensor_sub output " << output_dims[3] << " " << output_dims[2] << " " << output_dims[1] << " " << output_dims[0] << std::endl;
#endif

    strcpy(opencl_kernel_function_name, "tensor_upsample");

    vx_uint32 input_dim_size = input_dims[0] * input_dims[1] * input_dims[2] * input_dims[3];

    opencl_work_dim = 3;
    opencl_global_work[0] = output_dims[0];
    opencl_global_work[1] = output_dims[1];
    opencl_global_work[2] = output_dims[2] * output_dims[3];

    // Setting variables required by the interface
    opencl_local_buffer_usage_mask = 0;
    opencl_local_buffer_size_in_bytes = 0;

    if (num_of_dims == 4) {
        char item[8192];
        sprintf(item,
                "__kernel void %s(__global float * in, uint in_offset, uint4 i0_stride, __global float * out, uint out_offset, uint4 o0_stride) \n"
                "{ \n"
                "     uint output_x = get_global_id(0);\n"
                "     uint output_y = get_global_id(1);\n"
                "     uint output_n = get_global_id(2);\n"
                "     if(output_x < %ld  && output_y < %ld  && output_n < %ld) {\n"
                "       uint input_x = output_x / 2;\n"
                "       uint input_y = output_y / 2;\n"
                "       uint input_n = output_n;"
                "       out[out_offset + output_n*(o0_stride.s2/o0_stride.s0) + output_y*(o0_stride.s2/o0_stride.s1) + output_x] = in[in_offset + input_n*(i0_stride.s2/i0_stride.s0) + input_y*(i0_stride.s2/i0_stride.s1) + input_x];\n"
                "     }\n"
                " }\n",opencl_kernel_function_name, output_dims[0], output_dims[1], output_dims[2] * output_dims[3]);



        opencl_kernel_code = item;
    }

    return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK host_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num) {
    return VX_ERROR_NOT_IMPLEMENTED;
}

//! \brief The kernel publisher.
vx_status publishTensorUpsample(vx_context context) {

    vx_kernel kernel = vxAddUserKernel(context, "org.khronos.openvx.tensor_upsample", VX_KERNEL_UPSAMPLE_LAYER_AMD, host_kernel, 2, validateTensorUpsamling, nullptr, nullptr);
    ERROR_CHECK_OBJECT(kernel);

    amd_kernel_query_target_support_f query_target_support_f = query_target_support;
    amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = opencl_codegen;
    ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
    ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

    //set kernel parameters.
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED));
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED));

    //finalize and release kernel object.
    ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
    ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

    return VX_SUCCESS;
}

VX_API_ENTRY vx_node VX_API_CALL vxTensorUpsampleNode(vx_graph graph, vx_tensor input, vx_tensor output)
{
    vx_node node = NULL;
    vx_context context = vxGetContext((vx_reference)graph);
    if (vxGetStatus((vx_reference)context) == VX_SUCCESS) {
        vx_reference params[] = {
            (vx_reference)input,
            (vx_reference)output
        };
        node = createNode(graph, VX_KERNEL_UPSAMPLE_LAYER_AMD, params, sizeof(params) / sizeof(params[0]));
    }
    return node;
}


