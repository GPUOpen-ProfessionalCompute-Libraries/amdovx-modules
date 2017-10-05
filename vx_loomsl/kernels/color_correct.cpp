/*
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

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

#define _CRT_SECURE_NO_WARNINGS
#include "color_correct.h"
// Transformation Function
std::string GetTransformationFunctions16bit(){
	std::string output =
		"uint4 RGBTran_16(uint src0, uint src1, uint src2, float4 r4, float4 g4, float4 b4) {\n"
		"  uint4 output;\n"
		"  float3 fin, fout;\n"
		"  fin = amd_unpackA(src0, src1);\n"
		"  fout.s0 = mad(fin.s0, r4.s0, mad(fin.s1, r4.s1, mad(fin.s2, r4.s2, r4.s3)));\n"
		"  fout.s1 = mad(fin.s0, g4.s0, mad(fin.s1, g4.s1, mad(fin.s2, g4.s2, g4.s3)));\n"
		"  fout.s2 = mad(fin.s0, b4.s0, mad(fin.s1, b4.s1, mad(fin.s2, b4.s2, b4.s3)));\n"
		"  output.s0 = amd_pack15(fout.s0,fout.s1);\n"
		"  output.s1 = amd_pack15(fout.s2,0);\n"
		"  fin = amd_unpackB(src1, src2);\n"
		"  fout.s0 = mad(fin.s0, r4.s0, mad(fin.s1, r4.s1, mad(fin.s2, r4.s2, r4.s3)));\n"
		"  fout.s1 = mad(fin.s0, g4.s0, mad(fin.s1, g4.s1, mad(fin.s2, g4.s2, g4.s3)));\n"
		"  fout.s2 = mad(fin.s0, b4.s0, mad(fin.s1, b4.s1, mad(fin.s2, b4.s2, b4.s3)));\n"
		"  output.s1 += (((uint)clamp(fout.s0,0.0f,32767.0f))<<16);\n"
		"  output.s2 = amd_pack15(fout.s1,fout.s2);\n"
		"  return output;\n"
		"}\n"
		"\n";
	return output;
}
std::string create_amd_unpackab()
{
	std::string output =
		"float3 amd_unpackA(uint src0, uint src1)\n"
		"{\n"
		"  return (float3)((float)(src0 & 0x7fff),(float)((src0 >> 16) & 0x7fff), (float)(src1 & 0x7fff));\n"
		"}\n"
		"float3 amd_unpackB(uint src0, uint src1)\n"
		"{\n"
		"  return (float3)((float)((src0 >> 16) & 0x7fff), (float)(src1 & 0x7fff), (float)((src1 >> 16) & 0x7fff));\n"
		"}\n"
		"\n";
	return output;
}
std::string create_amd_pack15()
{
	std::string output =
		"uint amd_pack15(float src0, float src1)\n"
		"{\n"
		"  return ( ( ( (uint) clamp(src1,0.0f,32767.0f))<<16) + (uint) clamp(src0,0.0f,32767.0f) );\n"
		"}\n"
		"\n";
	return output;
}

//! \brief The input validator callback.
static vx_status VX_CALLBACK color_correct_input_validator(vx_node node, vx_uint32 index)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	// get reference for parameter at specified index
	vx_reference ref = avxGetNodeParamRef(node, index);
	ERROR_CHECK_OBJECT(ref);
	// validate each parameter
	if (index == 0)
	{ // object of SCALAR type
		vx_enum itemtype = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)ref, VX_SCALAR_ATTRIBUTE_TYPE, &itemtype, sizeof(itemtype)));
		ERROR_CHECK_STATUS(vxReleaseScalar((vx_scalar *)&ref));
		if (itemtype == VX_TYPE_UINT32) {
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_correct num_cameras scalar type should be a UINT32\n");
		}
	}
	else if (index == 1)
	{ // array object for gains
		vx_enum itemtype = VX_TYPE_INVALID;
		vx_size capacity = 0;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)ref, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &itemtype, sizeof(itemtype)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)ref, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (itemtype != VX_TYPE_FLOAT32) {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_correct gains array type should be float32\n");
		}
		else if (capacity == 0) {
			status = VX_ERROR_INVALID_DIMENSION;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_correct gains array capacity should be positive\n");
		}
		else {
			status = VX_SUCCESS;
		}
		ERROR_CHECK_STATUS(vxReleaseArray((vx_array *)&ref));
	}
	else if (index == 2)
	{ // image of format RGB2 or RGB4
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_RGB || format == VX_DF_IMAGE_RGB4_AMD) {
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_correct doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	else if (index == 4)
	{ // object of SCALAR type
		vx_enum itemtype = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)ref, VX_SCALAR_ATTRIBUTE_TYPE, &itemtype, sizeof(itemtype)));
		ERROR_CHECK_STATUS(vxReleaseScalar((vx_scalar *)&ref));
		if (itemtype == VX_TYPE_UINT32) {
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_correct num_camera_columns scalar type should be a UINT32\n");
		}
	}
	return status;
}

//! \brief The output validator callback.
static vx_status VX_CALLBACK color_correct_output_validator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 3)
	{ // image of format RGB2 or RGB4
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 2);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 input_width = 0, input_height = 0;
		vx_df_image input_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &input_format, sizeof(input_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		image = (vx_image)avxGetNodeParamRef(node, index);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 output_width = 0, output_height = 0;
		vx_df_image output_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		if (input_width != output_width || input_height != output_height)
		{
			status = VX_ERROR_INVALID_DIMENSION;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert doesn't support input & output image with different dimensions\n");
			return status;
		}
		if (input_format == VX_DF_IMAGE_RGB && output_format != VX_DF_IMAGE_RGB) {
			output_format = VX_DF_IMAGE_RGB;
		}
		else if (input_format == VX_DF_IMAGE_RGB4_AMD && output_format != VX_DF_IMAGE_RGB4_AMD) {
			output_format = VX_DF_IMAGE_RGB4_AMD;
		}
		// set output image meta data
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		status = VX_SUCCESS;
	}
	return status;
}

//! \brief The kernel target support callback.
static vx_status VX_CALLBACK color_correct_query_target_support(vx_graph graph, vx_node node,
	vx_bool use_opencl_1_2,              // [input]  false: OpenCL driver is 2.0+; true: OpenCL driver is 1.2
	vx_uint32& supported_target_affinity // [output] must be set to AGO_TARGET_AFFINITY_CPU or AGO_TARGET_AFFINITY_GPU or (AGO_TARGET_AFFINITY_CPU | AGO_TARGET_AFFINITY_GPU)
	)
{
	supported_target_affinity = AGO_TARGET_AFFINITY_GPU;
	return VX_SUCCESS;
}

//! \brief The OpenCL code generator callback.
static vx_status VX_CALLBACK color_correct_opencl_codegen(
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
	// get input and output image configurations
	vx_uint32 num_cameras = 0, num_camera_columns = 1;
	vx_scalar scalar = (vx_scalar)avxGetNodeParamRef(node, 0);			// input scalar - num cameras
	ERROR_CHECK_OBJECT(scalar);
	ERROR_CHECK_STATUS(vxReadScalarValue(scalar, &num_cameras));
	ERROR_CHECK_STATUS(vxReleaseScalar(&scalar));

	vx_size num_gains;
	vx_array gains = (vx_array)avxGetNodeParamRef(node, 1);	            // input array - gains
	ERROR_CHECK_STATUS(vxQueryArray(gains, VX_ARRAY_ATTRIBUTE_CAPACITY, &num_gains, sizeof(num_gains)));

	vx_uint32 input_width = 0, input_height = 0, output_width = 0, output_height = 0;
	vx_df_image input_format = VX_DF_IMAGE_VIRT, output_format = VX_DF_IMAGE_VIRT;
	vx_image image = (vx_image)avxGetNodeParamRef(node, 2);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &input_format, sizeof(input_format)));
	image = (vx_image)avxGetNodeParamRef(node, 3);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));

	vx_scalar s_num_camera_columns = (vx_scalar)parameters[4];
	if (s_num_camera_columns) {
		// read num_camera_columns
		ERROR_CHECK_STATUS(vxReadScalarValue(s_num_camera_columns, &num_camera_columns));
	}
	vx_uint32 num_camera_rows = num_cameras / num_camera_columns;

	// set kernel configuration
	vx_uint32 work_items[2];
	//work_items[0] = (input_width + (3*num_camera_columns -1)) / (4*num_camera_columns);
	//work_items[1] = (input_height + (num_cameras / num_camera_columns) - 1) / (num_cameras / num_camera_columns);
	//work_items[2] = num_cameras;
	work_items[0] = input_width / 4;
	work_items[1] = input_height;
	strcpy(opencl_kernel_function_name, "color_correct");
	opencl_work_dim = 2;
	opencl_local_work[0] = 16;
	opencl_local_work[1] = 4;
	opencl_global_work[0] = (work_items[0] + opencl_local_work[0] - 1) & ~(opencl_local_work[0] - 1);
	opencl_global_work[1] = (work_items[1] + opencl_local_work[1] - 1) & ~(opencl_local_work[1] - 1);

	// kernel header and reading
	opencl_kernel_code =
		"#pragma OPENCL EXTENSION cl_amd_media_ops : enable\n"
		"#pragma OPENCL EXTENSION cl_amd_media_ops2 : enable\n";
	
	//Define help functions for 16 bit
	if (input_format == VX_DF_IMAGE_RGB4_AMD){ 
		opencl_kernel_code += create_amd_unpackab(); 
		opencl_kernel_code += create_amd_pack15();
		opencl_kernel_code += GetTransformationFunctions16bit();
	}
	else{
		opencl_kernel_code +=
			"float4 amd_unpack(uint src)\n"
			"{\n"
			"	return (float4)(amd_unpack0(src), amd_unpack1(src), amd_unpack2(src), amd_unpack3(src));\n"
			"}\n";
	}

	char item[8192];
	sprintf(item,
		"__kernel __attribute__((reqd_work_group_size(%d, %d, 1)))\n" // opencl_local_work[0], opencl_local_work[1]
		"void %s(\n" // opencl_kernel_function_name
		"        uint num_cameras,\n"
		"        __global uchar * pG_buf, uint pG_offs, uint pG_num,\n"
		"	     uint pRGB_in_width, uint pRGB_in_height, __global uchar * pRGB_in_buf, uint pRGB_in_stride, uint pRGB_in_offset,\n"
		"	     uint pRGB_out_width, uint pRGB_out_height, __global uchar * pRGB_out_buf, uint pRGB_out_stride, uint pRGB_out_offset"
		, (int)opencl_local_work[0], (int)opencl_local_work[1], opencl_kernel_function_name);
	opencl_kernel_code += item;
	if (s_num_camera_columns) {
		opencl_kernel_code +=
			",\n"
			"        uint num_camera_columns";
	}
	
	sprintf(item,
		")\n"
		"{\n"
		"  int gx = get_global_id(0)<<2;\n"
		"  int gy = get_global_id(1);\n"
		"  if ((gx < (%d<<2)) && (gy < %d)) {\n" // work_items[0], work_items[1]
		"  int cam_rows = gy / %d;\n" // input_height / num_camera_rows
		"  int cam_id = gx / %d + cam_rows * %d;\n"// input_width / num_camera_columns, num_camera_columns
		, work_items[0], work_items[1], input_height / num_camera_rows, input_width / num_camera_columns, num_camera_columns);
	opencl_kernel_code += item;

	opencl_kernel_code +=
		"    pG_buf += pG_offs;\n"
		"    __global float4 * pg = (__global float4 *)pG_buf; pg += cam_id*3;\n"
		"    float4 r4 = pg[0], g4 = pg[1], b4 = pg[2];\n";

	if (input_format == VX_DF_IMAGE_RGB){
		opencl_kernel_code +=
			"  uint4 L0;\n"
			"  pRGB_in_buf += pRGB_in_offset + (gy * pRGB_in_stride) + (gx * 3);\n"
			"  L0 = *(__global uint4 *) pRGB_in_buf;\n"
			"  uint4 pRGB0;\n"
			"  float4 fin1, fin2, fout;\n"
			"  fin1 = amd_unpack(L0.s0);\n"
			"  fin2 = amd_unpack(L0.s1);\n"
			"  fout.s0 = mad(fin1.s0, r4.s0, mad(fin1.s1, r4.s1, mad(fin1.s2, r4.s2, r4.s3)));\n"
			"  fout.s1 = mad(fin1.s0, g4.s0, mad(fin1.s1, g4.s1, mad(fin1.s2, g4.s2, g4.s3)));\n"
			"  fout.s2 = mad(fin1.s0, b4.s0, mad(fin1.s1, b4.s1, mad(fin1.s2, b4.s2, b4.s3)));\n"
			"  fout.s3 = mad(fin1.s3, r4.s0, mad(fin2.s0, r4.s1, mad(fin2.s1, r4.s2, r4.s3)));\n"
			"  pRGB0.s0 = amd_pack(fout);\n"
			"  fout.s0 = mad(fin1.s3, g4.s0, mad(fin2.s0, g4.s1, mad(fin2.s1, g4.s2, g4.s3)));\n"
			"  fout.s1 = mad(fin1.s3, b4.s0, mad(fin2.s0, b4.s1, mad(fin2.s1, b4.s2, b4.s3)));\n"
			"  fin1 = amd_unpack(L0.s2);\n"
			"  fout.s2 = mad(fin2.s2, r4.s0, mad(fin2.s3, r4.s1, mad(fin1.s0, r4.s2, r4.s3)));\n"
			"  fout.s3 = mad(fin2.s2, g4.s0, mad(fin2.s3, g4.s1, mad(fin1.s0, g4.s2, g4.s3)));\n"
			"  pRGB0.s1 = amd_pack(fout);\n"
			"  fout.s0 = mad(fin2.s2, b4.s0, mad(fin2.s3, b4.s1, mad(fin1.s0, b4.s2, b4.s3)));\n"
			"  fout.s1 = mad(fin1.s1, r4.s0, mad(fin1.s2, r4.s1, mad(fin1.s3, r4.s2, r4.s3)));\n"
			"  fout.s2 = mad(fin1.s1, g4.s0, mad(fin1.s2, g4.s1, mad(fin1.s3, g4.s2, g4.s3)));\n"
			"  fout.s3 = mad(fin1.s1, b4.s0, mad(fin1.s2, b4.s1, mad(fin1.s3, b4.s2, b4.s3)));\n"
			"  pRGB0.s2 = amd_pack(fout);\n"
			"  pRGB_out_buf += pRGB_out_offset + (gy * pRGB_out_stride) + (gx * 3);\n"
			"  *(__global uint2 *) pRGB_out_buf    = pRGB0.s01;\n"
			"  *(__global uint  *)&pRGB_out_buf[8] = pRGB0.s2;\n"
			"  }\n"
			"}\n";
	}
	else{ // if (input_format == VX_DF_IMAGE_RGB4_AMD){
		opencl_kernel_code +=
			"  uint4 L0;\n"
			"  uint2 L1;\n"
			"  pRGB_in_buf += pRGB_in_offset + (gy * pRGB_in_stride) + (gx * 6);\n"
			"  L0 = *(__global uint4 *) pRGB_in_buf;\n"
			"  L1 = *(__global uint2 *)&pRGB_in_buf[16];\n"
			"  uint4 out;\n"
			"  out = RGBTran_16(L0.s0, L0.s1, L0.s2, r4, g4, b4); L0.s012 = out.s012;\n"
			"  out = RGBTran_16(L0.s3, L1.s0, L1.s1, r4, g4, b4); L0.s3   = out.s0;   L1 = out.s12;\n"
			"  pRGB_out_buf += pRGB_out_offset + (gy * pRGB_out_stride) + (gx * 6);\n"
			"  *(__global uint4 *) pRGB_out_buf = L0;\n"
			"  *(__global uint2 *)&pRGB_out_buf[16] = L1;\n"
			"  }\n"
			"}\n";
	}	

	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK color_correct_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	return VX_ERROR_NOT_SUPPORTED;
}

//! \brief The kernel publisher.
vx_status color_correct_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddKernel(context, "com.amd.loomsl.color_correct",
		AMDOVX_KERNEL_STITCHING_COLOR_CORRECT,
		color_correct_kernel,
		5,
		color_correct_input_validator,
		color_correct_output_validator,
		nullptr,
		nullptr);
	ERROR_CHECK_OBJECT(kernel);
	amd_kernel_query_target_support_f query_target_support_f = color_correct_query_target_support;
	amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = color_correct_opencl_codegen;
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_INPUT,  VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_INPUT,  VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 3, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 4, VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL));

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}