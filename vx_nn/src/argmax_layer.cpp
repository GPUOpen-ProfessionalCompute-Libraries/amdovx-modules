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

static vx_status VX_CALLBACK validateKernel(vx_node node, const vx_reference parameters[], vx_uint32 num, vx_meta_format metas[])
{
    // check input configuration
    vx_enum type;
    vx_size num_dims, input_dims[4] = { 1, 1, 1, 1 };
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DATA_TYPE, &type, sizeof(type)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    if (num_dims < 2)
        return VX_ERROR_INVALID_DIMENSION;
    if (type != VX_TYPE_FLOAT32)
        return VX_ERROR_INVALID_TYPE;
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DIMS, input_dims, sizeof(input_dims[0])*num_dims));
    if (input_dims[3] != 1)
        return VX_ERROR_INVALID_DIMENSION;
    vx_df_image format;
    ERROR_CHECK_STATUS(vxQueryImage((vx_image)parameters[1], VX_IMAGE_FORMAT, &format, sizeof(format)));
    if(format == VX_DF_IMAGE_U8 && input_dims[2] > 255)
        return VX_ERROR_INVALID_FORMAT;
    if(format == VX_DF_IMAGE_VIRT)
        format = (input_dims[2] < 256) ? VX_DF_IMAGE_U8 : VX_DF_IMAGE_U16;

    // set output image configuration
    vx_uint32 width = (vx_uint32)input_dims[0];
    vx_uint32 height = (vx_uint32)input_dims[1];
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[1], VX_IMAGE_WIDTH, &width, sizeof(width)));
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[1], VX_IMAGE_HEIGHT, &height, sizeof(height)));
    ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[1], VX_IMAGE_FORMAT, &format, sizeof(format)));
    if(parameters[2]) {
        ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[2], VX_IMAGE_WIDTH, &width, sizeof(width)));
        ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[2], VX_IMAGE_HEIGHT, &height, sizeof(height)));
        ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[2], VX_IMAGE_FORMAT, &format, sizeof(format)));
    }

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
    // get configuration
    vx_size num_dims, input_dims[4] = { 1, 1, 1, 1 };
    vx_df_image format;
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_NUMBER_OF_DIMS, &num_dims, sizeof(num_dims)));
    ERROR_CHECK_STATUS(vxQueryTensor((vx_tensor)parameters[0], VX_TENSOR_DIMS, input_dims, sizeof(input_dims[0])*num_dims));
    ERROR_CHECK_STATUS(vxQueryImage((vx_image)parameters[1], VX_IMAGE_FORMAT, &format, sizeof(format)));

    // compute global work
    vx_uint32 width_div_4 = (input_dims[0] + 3) / 4;
    opencl_work_dim = 3;
    opencl_local_work[0] = 8;
    opencl_local_work[1] = 8;
    opencl_local_work[2] = 1;
    opencl_global_work[0] = (width_div_4  + opencl_local_work[0] - 1) & ~(opencl_local_work[0] - 1);
    opencl_global_work[1] = (input_dims[1] + opencl_local_work[1] - 1) & ~(opencl_local_work[1] - 1);
    opencl_global_work[2] = 1;

    // generate OpenCL C code
    strcpy(opencl_kernel_function_name, "argmax");
    char item[8192];
    if(parameters[2]) {
        sprintf(item,
            "#pragma opencl extension cl_amd_media_ops : enable\n"
            "__kernel __attribute__((reqd_work_group_size(%ld, %ld, 1)))\n" // opencl_local_work[0] opencl_local_work[1]
            "void %s(__global uchar * i0_buf, uint i0_offset, uint4 i0_stride, uint o0_width, uint o0_height, __global uchar * o0_buf, uint o0_stride, uint o0_offset, uint o1_width, uint o1_height, __global uchar * o1_buf, uint o1_stride, uint o1_offset)\n"
            "{\n"
            "    uint x = get_global_id(0) * 4;\n"
            "    uint y = get_global_id(1);\n"
            "    if(x < %ld && y < %ld) {\n"
            "        i0_buf += i0_offset + y * i0_stride.s1 + x * i0_stride.s0;\n"
            "        uint4 cmax, cmax1;\n"
            "        float4 f, fmax, fmax1;\n"
            "        fmax = *(__global float4 *)i0_buf;\n"
            "        i0_buf += i0_stride.s2; f = *(__global float4 *)i0_buf;\n"
            "        cmax1.s0 = (f.s0 > fmax.s0) ? 0 : 1;\n"
            "         cmax.s0 = (f.s0 > fmax.s0) ? 1 : 0;\n"
            "        fmax1.s0 = (f.s0 > fmax.s0) ? fmax.s0 :    f.s0;\n"
            "         fmax.s0 = (f.s0 > fmax.s0) ?    f.s0 : fmax.s0;\n"
            "        cmax1.s1 = (f.s1 > fmax.s1) ? 0 : 1;\n"
            "         cmax.s1 = (f.s1 > fmax.s1) ? 1 : 0;\n"
            "        fmax1.s1 = (f.s1 > fmax.s1) ? fmax.s1 :    f.s1;\n"
            "         fmax.s1 = (f.s1 > fmax.s1) ?    f.s1 : fmax.s1;\n"
            "        cmax1.s2 = (f.s2 > fmax.s2) ? 0 : 1;\n"
            "         cmax.s2 = (f.s2 > fmax.s2) ? 1 : 0;\n"
            "        fmax1.s2 = (f.s2 > fmax.s2) ? fmax.s2 :    f.s2;\n"
            "         fmax.s2 = (f.s2 > fmax.s2) ?    f.s2 : fmax.s2;\n"
            "        cmax1.s3 = (f.s3 > fmax.s3) ? 0 : 1;\n"
            "         cmax.s3 = (f.s3 > fmax.s3) ? 1 : 0;\n"
            "        fmax1.s3 = (f.s3 > fmax.s3) ? fmax.s3 :    f.s3;\n"
            "         fmax.s3 = (f.s3 > fmax.s3) ?    f.s3 : fmax.s3;\n"
            "        for(uint c = 2; c < %ld; c++) {\n"
            "            i0_buf += i0_stride.s2; f = *(__global float4 *)i0_buf;\n"
            "            cmax1.s0 = (f.s0 > fmax.s0) ? cmax.s0 : ((f.s0 > fmax1.s0) ? c    : cmax1.s0);\n"
            "            fmax1.s0 = (f.s0 > fmax.s0) ? fmax.s0 : ((f.s0 > fmax1.s0) ? f.s0 : fmax1.s0);\n"
            "            cmax.s0  = (f.s0 > fmax.s0) ? c    : cmax.s0;\n"
            "            fmax.s0  = (f.s0 > fmax.s0) ? f.s0 : fmax.s0;\n"
            "            cmax1.s1 = (f.s1 > fmax.s1) ? cmax.s1 : ((f.s1 > fmax1.s1) ? c    : cmax1.s1);\n"
            "            fmax1.s1 = (f.s1 > fmax.s1) ? fmax.s1 : ((f.s1 > fmax1.s1) ? f.s1 : fmax1.s1);\n"
            "            cmax.s1  = (f.s1 > fmax.s1) ? c    : cmax.s1;\n"
            "            fmax.s1  = (f.s1 > fmax.s1) ? f.s1 : fmax.s1;\n"
            "            cmax1.s2 = (f.s2 > fmax.s2) ? cmax.s2 : ((f.s2 > fmax1.s2) ? c    : cmax1.s2);\n"
            "            fmax1.s2 = (f.s2 > fmax.s2) ? fmax.s2 : ((f.s2 > fmax1.s2) ? f.s2 : fmax1.s2);\n"
            "            cmax.s2  = (f.s2 > fmax.s2) ? c    : cmax.s2;\n"
            "            fmax.s2  = (f.s2 > fmax.s2) ? f.s2 : fmax.s2;\n"
            "            cmax1.s3 = (f.s3 > fmax.s3) ? cmax.s3 : ((f.s3 > fmax1.s3) ? c    : cmax1.s3);\n"
            "            fmax1.s3 = (f.s3 > fmax.s3) ? fmax.s3 : ((f.s3 > fmax1.s3) ? f.s3 : fmax1.s3);\n"
            "            cmax.s3  = (f.s3 > fmax.s3) ? c    : cmax.s3;\n"
            "            fmax.s3  = (f.s3 > fmax.s3) ? f.s3 : fmax.s3;\n"
            "        }\n"
            , opencl_local_work[0], opencl_local_work[1], opencl_kernel_function_name, input_dims[0], input_dims[1], input_dims[2]);
        opencl_kernel_code = item;
    }
    else {
        sprintf(item,
            "#pragma opencl extension cl_amd_media_ops : enable\n"
            "__kernel __attribute__((reqd_work_group_size(%ld, %ld, 1)))\n" // opencl_local_work[0] opencl_local_work[1]
            "void %s(__global uchar * i0_buf, uint i0_offset, uint4 i0_stride, uint o0_width, uint o0_height, __global uchar * o0_buf, uint o0_stride, uint o0_offset)\n"
            "{\n"
            "    uint x = get_global_id(0) * 4;\n"
            "    uint y = get_global_id(1);\n"
            "    if(x < %ld && y < %ld) {\n"
            "        i0_buf += i0_offset + y * i0_stride.s1 + x * i0_stride.s0;\n"
            "        uint4 cmax = (uint4)0;\n"
            "        float4 fmax = *(__global float4 *)i0_buf;\n"
            "        for(uint c = 1; c < %ld; c++) {\n"
            "            i0_buf += i0_stride.s2;\n"
            "            float4 f = *(__global float4 *)i0_buf;\n"
            "            cmax.s0 = (f.s0 > fmax.s0) ? c    : cmax.s0;\n"
            "            fmax.s0 = (f.s0 > fmax.s0) ? f.s0 : fmax.s0;\n"
            "            cmax.s1 = (f.s1 > fmax.s1) ? c    : cmax.s1;\n"
            "            fmax.s1 = (f.s1 > fmax.s1) ? f.s1 : fmax.s1;\n"
            "            cmax.s2 = (f.s2 > fmax.s2) ? c    : cmax.s2;\n"
            "            fmax.s2 = (f.s2 > fmax.s2) ? f.s2 : fmax.s2;\n"
            "            cmax.s3 = (f.s3 > fmax.s3) ? c    : cmax.s3;\n"
            "            fmax.s3 = (f.s3 > fmax.s3) ? f.s3 : fmax.s3;\n"
            "        }\n"
            , opencl_local_work[0], opencl_local_work[1], opencl_kernel_function_name, input_dims[0], input_dims[1], input_dims[2]);
        opencl_kernel_code = item;
    }
    if(format == VX_DF_IMAGE_U8) {
        opencl_kernel_code +=
            "        uint imax = cmax.s0 + (cmax.s1 << 8) + (cmax.s2 << 16) + (cmax.s3 << 24);\n"
            "        *(__global uint *)&o0_buf[o0_offset + y * o0_stride + x] = imax;\n";
        if(parameters[2]) {
            opencl_kernel_code +=
                "        uint imax1 = cmax1.s0 + (cmax1.s1 << 8) + (cmax1.s2 << 16) + (cmax1.s3 << 24);\n"
                "        *(__global uint *)&o1_buf[o1_offset + y * o1_stride + x] = imax1;\n";
        }
    }
    else if(format == VX_DF_IMAGE_U16) {
        opencl_kernel_code +=
            "        uint2 imax;\n"
            "        imax.s0 = cmax.s0 + (cmax.s1 << 16);\n"
            "        imax.s1 = cmax.s2 + (cmax.s3 << 16);\n"
            "        *(__global uint2 *)&o0_buf[o0_offset + y * o0_stride + x * 2] = imax;\n";
        if(parameters[2]) {
            opencl_kernel_code +=
                "        uint2 imax1;\n"
                "        imax1.s0 = cmax1.s0 + (cmax1.s1 << 16);\n"
                "        imax1.s1 = cmax1.s2 + (cmax1.s3 << 16);\n"
                "        *(__global uint2 *)&o1_buf[o1_offset + y * o1_stride + x * 2] = imax1;\n";
        }
    }
    opencl_kernel_code +=
        "    }\n"
        "}\n";

#if ENABLE_DEBUG_PRINT_DIMS
    std::cout << "KERNEL argmax_layer output " << input_dims[0] << "x" << input_dims[1] << " " << std::endl;
#endif

    return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK host_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
    return VX_ERROR_NOT_IMPLEMENTED;
}

//! \brief The kernel publisher.
vx_status publishArgmaxLayer(vx_context context)
{
    vx_kernel kernel = vxAddUserKernel(context, "com.amd.nn_extension.argmax_layer", VX_KERNEL_ARGMAX_LAYER_AMD, host_kernel, 3, validateKernel, nullptr, nullptr);
    ERROR_CHECK_OBJECT(kernel);

    amd_kernel_query_target_support_f query_target_support_f = query_target_support;
    amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = opencl_codegen;
    ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
    ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

    // set kernel parameters.
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED));
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
    ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_OPTIONAL));

    // finalize and release kernel object.
    ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
    ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

    return VX_SUCCESS;
}

VX_API_ENTRY vx_node VX_API_CALL vxArgmaxLayerNode(vx_graph graph, vx_tensor input, vx_image output0, vx_image output1)
{
    vx_node node = NULL;
    vx_context context = vxGetContext((vx_reference)graph);
    if (vxGetStatus((vx_reference)context) == VX_SUCCESS) {
        vx_reference params[] = {
            (vx_reference)input,
            (vx_reference)output0,
            (vx_reference)output1
        };
        node = createNode(graph, VX_KERNEL_ARGMAX_LAYER_AMD, params, sizeof(params) / sizeof(params[0]));
    }
    return node;
}
