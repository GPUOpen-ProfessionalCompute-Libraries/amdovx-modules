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
#include "color_convert.h"

//! \brief The input validator callback.
static vx_status VX_CALLBACK color_convert_general_input_validator(vx_node node, vx_uint32 index)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	// get reference for parameter at specified index
	vx_reference ref = avxGetNodeParamRef(node, index);
	ERROR_CHECK_OBJECT(ref);
	// validate each parameter
	if (index == 0)
	{ // image of format UYVY or YUYV or or V210 or V216 OR RGB2 or RGB4
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_UYVY || format == VX_DF_IMAGE_YUYV || format == VX_DF_IMAGE_V210_AMD || format == VX_DF_IMAGE_V216_AMD || format == VX_DF_IMAGE_RGB || format == VX_DF_IMAGE_RGB4_AMD) {
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	return status;
}

//! \brief The output validator callback.
static vx_status VX_CALLBACK color_convert_general_output_validator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 1)
	{ // image of format RGB or RGBX or RGB4 OR YUYV or UYVY or NV12 or IYUV or V210 or V216
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
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
		if ((output_width == (input_width / 2) && output_height == (input_height / 2)) && (input_format == VX_DF_IMAGE_V210_AMD)){ //No support of downscaling for this format
			// pick input dimensions as default
			output_width = input_width;
			output_height = input_height;
		}
		if ((input_format == VX_DF_IMAGE_UYVY || input_format == VX_DF_IMAGE_YUYV) && output_format != VX_DF_IMAGE_RGB && output_format != VX_DF_IMAGE_RGBX && output_format != VX_DF_IMAGE_RGB4_AMD) {
			// for 8bit YUV input take RGB as default output
			output_format = VX_DF_IMAGE_RGB;

		}
		if ((input_format == VX_DF_IMAGE_V210_AMD || input_format == VX_DF_IMAGE_V216_AMD) && output_format != VX_DF_IMAGE_RGB && output_format != VX_DF_IMAGE_RGBX && output_format != VX_DF_IMAGE_RGB4_AMD) {
			// for 16 bit YUV input take RGB4 as default output
			output_format = VX_DF_IMAGE_RGB4_AMD;
		}
		else if ((input_format == VX_DF_IMAGE_RGB) && (output_format != VX_DF_IMAGE_UYVY) && (output_format != VX_DF_IMAGE_YUYV) && output_format != VX_DF_IMAGE_V210_AMD && output_format != VX_DF_IMAGE_V216_AMD) {
			// for 8 bit RGB take UYVY as default output
			output_format = VX_DF_IMAGE_UYVY;
		}
		else if ((input_format == VX_DF_IMAGE_RGB4_AMD) && (output_format != VX_DF_IMAGE_UYVY) && (output_format != VX_DF_IMAGE_YUYV) && output_format != VX_DF_IMAGE_V210_AMD && output_format != VX_DF_IMAGE_V216_AMD) {
			// for 16 bit RGB take Y216 as default output
			output_format = VX_DF_IMAGE_V216_AMD;
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
static vx_status VX_CALLBACK color_convert_general_query_target_support(vx_graph graph, vx_node node,
	vx_bool use_opencl_1_2,              // [input]  false: OpenCL driver is 2.0+; true: OpenCL driver is 1.2
	vx_uint32& supported_target_affinity // [output] must be set to AGO_TARGET_AFFINITY_CPU or AGO_TARGET_AFFINITY_GPU or (AGO_TARGET_AFFINITY_CPU | AGO_TARGET_AFFINITY_GPU)
	)
{
	supported_target_affinity = AGO_TARGET_AFFINITY_GPU;
	return VX_SUCCESS;
}

//! \brief The OpenCL code generator callback.
static vx_status VX_CALLBACK color_convert_general_opencl_codegen(
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
	vx_uint32 input_width = 0, input_height = 0, output_width = 0, output_height = 0;
	vx_df_image input_format = VX_DF_IMAGE_VIRT, output_format = VX_DF_IMAGE_VIRT;
	vx_channel_range_e input_channel_range, output_channel_range;
	vx_color_space_e input_color_space, output_color_space;
	vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &input_format, sizeof(input_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &input_channel_range, sizeof(input_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &input_color_space, sizeof(input_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));
	image = (vx_image)avxGetNodeParamRef(node, 1);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &output_channel_range, sizeof(output_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &output_color_space, sizeof(output_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));

	// set kernel configuration
	vx_uint32 work_items[2];
	if (input_format == VX_DF_IMAGE_V210_AMD || output_format == VX_DF_IMAGE_V210_AMD){
		work_items[0] = (input_width + 11) / 12;
		work_items[1] = input_height;
		strcpy(opencl_kernel_function_name, "color_convert");
		opencl_work_dim = 2;
		opencl_local_work[0] = 16;
		opencl_local_work[1] = 4;
		opencl_global_work[0] = (work_items[0] + opencl_local_work[0] - 1) & ~(opencl_local_work[0] - 1);
		opencl_global_work[1] = (work_items[1] + opencl_local_work[1] - 1) & ~(opencl_local_work[1] - 1);
	}
	else{
		work_items[0] = (input_width + 7) / 8;
		work_items[1] = (input_height + 1) / 2;
		strcpy(opencl_kernel_function_name, "color_convert");
		opencl_work_dim = 2;
		opencl_local_work[0] = 16;
		opencl_local_work[1] = 4;
		opencl_global_work[0] = (work_items[0] + opencl_local_work[0] - 1) & ~(opencl_local_work[0] - 1);
		opencl_global_work[1] = (work_items[1] + opencl_local_work[1] - 1) & ~(opencl_local_work[1] - 1);
	}

	// kernel header and reading
	opencl_kernel_code =
		"#pragma OPENCL EXTENSION cl_amd_media_ops : enable\n"
		"#pragma OPENCL EXTENSION cl_amd_media_ops2 : enable\n";

	//Define help functions for 10 and 16 bit
	if (input_format == VX_DF_IMAGE_V210_AMD) opencl_kernel_code += Create_amd_unpack10();
	if (input_format == VX_DF_IMAGE_V216_AMD) opencl_kernel_code += Create_amd_unpack16();
	if (input_format == VX_DF_IMAGE_RGB4_AMD) opencl_kernel_code += Create_amd_unpackAB();
	if (output_format == VX_DF_IMAGE_V210_AMD) opencl_kernel_code += Create_amd_pack10();
	if (output_format == VX_DF_IMAGE_V216_AMD) opencl_kernel_code += Create_amd_pack16();
	if (output_format == VX_DF_IMAGE_RGB4_AMD) opencl_kernel_code += Create_amd_pack15();

	char item[8192];
	sprintf(item,
		"__kernel __attribute__((reqd_work_group_size(%d, %d, 1)))\n" // opencl_local_work[0], opencl_local_work[1]
		"void %s(\n" // opencl_kernel_function_name
		, (int)opencl_local_work[0], (int)opencl_local_work[1], opencl_kernel_function_name);
	opencl_kernel_code += item;
	if (input_format == VX_DF_IMAGE_RGB || input_format == VX_DF_IMAGE_RGB4_AMD){
		opencl_kernel_code +=
			"	uint pRGB_width, uint pRGB_height, __global uchar * pRGB_buf, uint pRGB_stride, uint pRGB_offset,\n";
	}
	else{ //input_format: UYVY, YUYV, V210, V216
		opencl_kernel_code +=
			"	uint p422_width, uint p422_height, __global uchar * p422_buf, uint p422_stride, uint p422_offset,\n";
	}
	if (output_format == VX_DF_IMAGE_RGB || output_format == VX_DF_IMAGE_RGBX || output_format == VX_DF_IMAGE_RGB4_AMD){
		opencl_kernel_code +=
			"	uint pRGB_width, uint pRGB_height, __global uchar * pRGB_buf, uint pRGB_stride, uint pRGB_offset)\n";
	}
	else{ //input_format: UYVY, YUYV, V210, V216
		opencl_kernel_code +=
			"	uint p422_width, uint p422_height, __global uchar * p422_buf, uint p422_stride, uint p422_offset)\n";
	}
	sprintf(item,
		"{\n"
		"  int gx = get_global_id(0);\n"
		"  int gy = get_global_id(1);\n"
		"  if ((gx < %d) && (gy < %d)) {\n" // work_items[0], work_items[1]
		, work_items[0], work_items[1]);
	opencl_kernel_code += item;

	// Input: UYVY
	if (input_format == VX_DF_IMAGE_UYVY){
		opencl_kernel_code += GetColorConversionTableForYUVInput(input_color_space);
		opencl_kernel_code += Read2x8PixelsFrom422buffer8bit();
		if (output_format == VX_DF_IMAGE_RGB)
		{
			opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertUYVYtoRGB2();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB2buffer() : Write1x4PixelsToRGB2buffer();
		}
		else if (output_format == VX_DF_IMAGE_RGBX)
		{
			opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertUYVYtoRGBX();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGBXbuffer() : Write1x4PixelsToRGBXbuffer();
		}
		else if (output_format == VX_DF_IMAGE_RGB4_AMD)
		{
			opencl_kernel_code += GetRangeConversionTableFor8bitTo15bit(input_channel_range);
			opencl_kernel_code += ConvertUYVYtoRGB4();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB4buffer() : Write1x4PixelsToRGB4buffer();
		}
	}

	// Input: YUYV
	else if (input_format == VX_DF_IMAGE_YUYV){
		opencl_kernel_code += GetColorConversionTableForYUVInput(input_color_space);
		opencl_kernel_code += Read2x8PixelsFrom422buffer8bit();
		if (output_format == VX_DF_IMAGE_RGB)
		{
			opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertYUYVtoRGB2();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB2buffer() : Write1x4PixelsToRGB2buffer();
		}
		else if (output_format == VX_DF_IMAGE_RGBX)
		{
			opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertYUYVtoRGBX();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGBXbuffer() : Write1x4PixelsToRGBXbuffer();
		}
		else if (output_format == VX_DF_IMAGE_RGB4_AMD)
		{
			opencl_kernel_code += GetRangeConversionTableFor8bitTo15bit(input_channel_range);
			opencl_kernel_code += ConvertYUYVtoRGB4();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB4buffer() : Write1x4PixelsToRGB4buffer();
		}
	}

	// Input: V210
	else if (input_format == VX_DF_IMAGE_V210_AMD){
		opencl_kernel_code += GetColorConversionTableForYUVInput(input_color_space);
		opencl_kernel_code += Read1x6PixelsFrom422buffer();
		if (output_format == VX_DF_IMAGE_RGB)
		{
			opencl_kernel_code += GetRangeConversionTableFor10bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertV210toRGB2();
			opencl_kernel_code += Write1x6PixelsToRGB2buffer();
		}
		else if (output_format == VX_DF_IMAGE_RGBX)
		{
			opencl_kernel_code += GetRangeConversionTableFor10bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertV210toRGBX();
			opencl_kernel_code += Write1x6PixelsToRGBXbuffer();
		}
		else if (output_format == VX_DF_IMAGE_RGB4_AMD)
		{
			opencl_kernel_code += GetRangeConversionTableFor10bitTo15bit(input_channel_range);
			opencl_kernel_code += ConvertV210toRGB4();
			opencl_kernel_code += Write1x6PixelsToRGB4buffer();
		}
	}

	// Input: V216
	else if (input_format == VX_DF_IMAGE_V216_AMD){
		opencl_kernel_code += GetColorConversionTableForYUVInput(input_color_space);
		opencl_kernel_code += Read2x8PixelsFrom422buffer16bit();
		if (output_format == VX_DF_IMAGE_RGB)
		{
			opencl_kernel_code += GetRangeConversionTableFor16bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertV216toRGB2();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB2buffer() : Write1x4PixelsToRGB2buffer();
		}
		else if (output_format == VX_DF_IMAGE_RGBX)
		{
			opencl_kernel_code += GetRangeConversionTableFor16bitTo8bit(input_channel_range);
			opencl_kernel_code += ConvertV216toRGBX();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGBXbuffer() : Write1x4PixelsToRGBXbuffer();
		}
		else if (output_format == VX_DF_IMAGE_RGB4_AMD)
		{
			opencl_kernel_code += GetRangeConversionTableFor16bitTo15bit(input_channel_range);
			opencl_kernel_code += ConvertV216toRGB4();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB4buffer() : Write1x4PixelsToRGB4buffer();
		}
	}

	// Input: RGB2
	else if (input_format == VX_DF_IMAGE_RGB){
		if (output_format == VX_DF_IMAGE_UYVY)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor8bitTo8bit(input_color_space, input_channel_range);
			opencl_kernel_code += Read2x8PixelsFromRGBbuffer8bit();
			opencl_kernel_code += ConvertRGB2toUYVY();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsTo422buffer8bit() : Write1x4PixelsTo422bufferUYVY();
		}
		else if (output_format == VX_DF_IMAGE_YUYV)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor8bitTo8bit(input_color_space, input_channel_range);
			opencl_kernel_code += Read2x8PixelsFromRGBbuffer8bit();
			opencl_kernel_code += ConvertRGB2toYUYV();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsTo422buffer8bit() : Write1x4PixelsTo422bufferYUYV();
		}
		else if (output_format == VX_DF_IMAGE_V210_AMD)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor8bitTo10bit(input_color_space, input_channel_range);
			opencl_kernel_code += ReadAndConvertRGB2toV210();
			opencl_kernel_code += Write1x6PixelsTo422buffer();
		}
		else if (output_format == VX_DF_IMAGE_V216_AMD)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor8bitTo16bit(input_color_space, input_channel_range);
			opencl_kernel_code += Read2x8PixelsFromRGBbuffer8bit();
			opencl_kernel_code += ConvertRGB2toV216();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsTo422buffer16bit() : Write1x4PixelsTo422buffer16bit();
		}
	}

	// Input: RGB4
	else if (input_format == VX_DF_IMAGE_RGB4_AMD){
		if (output_format == VX_DF_IMAGE_UYVY)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor15bitTo8bit(input_color_space, input_channel_range);
			opencl_kernel_code += Read2x8PixelsFromRGBbuffer16bit();
			opencl_kernel_code += ConvertRGB4toUYVY();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsTo422buffer8bit() : Write1x4PixelsTo422bufferUYVY();
		}
		else if (output_format == VX_DF_IMAGE_YUYV)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor15bitTo8bit(input_color_space, input_channel_range);
			opencl_kernel_code += Read2x8PixelsFromRGBbuffer16bit();
			opencl_kernel_code += ConvertRGB4toYUYV();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsTo422buffer8bit() : Write1x4PixelsTo422bufferYUYV();
		}
		else if (output_format == VX_DF_IMAGE_V210_AMD)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor15bitTo10bit(input_color_space, input_channel_range);
			opencl_kernel_code += ReadAndConvertRGB4toV210();
			opencl_kernel_code += Write1x6PixelsTo422buffer();
		}
		else if (output_format == VX_DF_IMAGE_V216_AMD)
		{
			opencl_kernel_code += GetColorRangeConversionTableFor15bitTo16bit(input_color_space, input_channel_range);
			opencl_kernel_code += Read2x8PixelsFromRGBbuffer16bit();
			opencl_kernel_code += ConvertRGB4toV216();
			opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsTo422buffer16bit() : Write1x4PixelsTo422buffer16bit();
		}
	}

	opencl_kernel_code +=
		"  }\n"
		"}\n";

	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK color_convert_general_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	return VX_ERROR_NOT_SUPPORTED;
}

//! \brief The kernel publisher.
vx_status color_convert_general_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddKernel(context, "com.amd.loomsl.color_convert_general",
		AMDOVX_KERNEL_STITCHING_COLOR_CONVERT_GENERAL,
		color_convert_general_kernel,
		2,
		color_convert_general_input_validator,
		color_convert_general_output_validator,
		nullptr,
		nullptr);
	ERROR_CHECK_OBJECT(kernel);
	amd_kernel_query_target_support_f query_target_support_f = color_convert_general_query_target_support;
	amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = color_convert_general_opencl_codegen;
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}

//! \brief The input validator callback.
static vx_status VX_CALLBACK color_convert_from_NV12_input_validator(vx_node node, vx_uint32 index)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	// get reference for parameter at specified index
	vx_reference ref = avxGetNodeParamRef(node, index);
	ERROR_CHECK_OBJECT(ref);
	// validate each parameter
	if (index == 0)
	{ // image of format NV12 > first plane U8
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_U8){
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	if (index == 1)
	{ // image of format NV12 > second plane U16
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_U16){
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	return status;
}

//! \brief The output validator callback.
static vx_status VX_CALLBACK color_convert_from_NV12_output_validator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 2)
	{ // image of format RGB or RGBX or RGB4
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 input_width = 0, input_height = 0;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		image = (vx_image)avxGetNodeParamRef(node, index);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 output_width = 0, output_height = 0;
		vx_df_image output_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		if (!(output_width == input_width && output_height == input_height) &&
			!(output_width == ((input_width + 1) / 2) && output_height == ((input_height + 1) / 2)))
		{
			// pick input dimensions as default
			output_width = input_width;
			output_height = input_height;
		}
		if (output_format != VX_DF_IMAGE_RGB && output_format != VX_DF_IMAGE_RGBX && output_format != VX_DF_IMAGE_RGB4_AMD) {
			// for 8bit YUV input take RGB as default output
			output_format = VX_DF_IMAGE_RGB;
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
static vx_status VX_CALLBACK color_convert_from_NV12_query_target_support(vx_graph graph, vx_node node,
	vx_bool use_opencl_1_2,              // [input]  false: OpenCL driver is 2.0+; true: OpenCL driver is 1.2
	vx_uint32& supported_target_affinity // [output] must be set to AGO_TARGET_AFFINITY_CPU or AGO_TARGET_AFFINITY_GPU or (AGO_TARGET_AFFINITY_CPU | AGO_TARGET_AFFINITY_GPU)
	)
{
	supported_target_affinity = AGO_TARGET_AFFINITY_GPU;
	return VX_SUCCESS;
}

//! \brief The OpenCL code generator callback.
static vx_status VX_CALLBACK color_convert_from_NV12_opencl_codegen(
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
	vx_uint32 input_width = 0, input_height = 0, output_width = 0, output_height = 0;
	vx_df_image input_format = VX_DF_IMAGE_VIRT, output_format = VX_DF_IMAGE_VIRT;
	vx_channel_range_e input_channel_range, output_channel_range;
	vx_color_space_e input_color_space, output_color_space;
	vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &input_format, sizeof(input_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &input_channel_range, sizeof(input_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &input_color_space, sizeof(input_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));
	image = (vx_image)avxGetNodeParamRef(node, 2);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &output_channel_range, sizeof(output_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &output_color_space, sizeof(output_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));

	// set kernel configuration
	vx_uint32 work_items[2];
	work_items[0] = (input_width + 7) / 8;
	work_items[1] = (input_height + 1) / 2;
	strcpy(opencl_kernel_function_name, "color_convert");
	opencl_work_dim = 2;
	opencl_local_work[0] = 16;
	opencl_local_work[1] = 4;
	opencl_global_work[0] = (work_items[0] + opencl_local_work[0] - 1) & ~(opencl_local_work[0] - 1);
	opencl_global_work[1] = (work_items[1] + opencl_local_work[1] - 1) & ~(opencl_local_work[1] - 1);

	// kernel header and reading
	opencl_kernel_code =
		"#pragma OPENCL EXTENSION cl_amd_media_ops : enable\n"
		"#pragma OPENCL EXTENSION cl_amd_media_ops2 : enable\n";

	//Define help functions for 10 and 16 bit
	if (input_format == VX_DF_IMAGE_RGB4_AMD) opencl_kernel_code += Create_amd_unpackAB();
	if (output_format == VX_DF_IMAGE_RGB4_AMD) opencl_kernel_code += Create_amd_pack15();

	char item[8192];
	sprintf(item,
		"__kernel __attribute__((reqd_work_group_size(%d, %d, 1)))\n" // opencl_local_work[0], opencl_local_work[1]
		"void %s(\n" // opencl_kernel_function_name
		, (int)opencl_local_work[0], (int)opencl_local_work[1], opencl_kernel_function_name);
	opencl_kernel_code += item;
	opencl_kernel_code +=
		"	uint pY_width, uint pY_height, __global uchar * pY_buf, uint pY_stride, uint pY_offset,\n    "
		"	uint pUV_width, uint pUV_height, __global uchar * pUV_buf, uint pUV_stride, uint pUV_offset,\n"
		"	uint pRGB_width, uint pRGB_height, __global uchar * pRGB_buf, uint pRGB_stride, uint pRGB_offset)\n";
	sprintf(item,
		"{\n"
		"  int gx = get_global_id(0);\n"
		"  int gy = get_global_id(1);\n"
		"  if ((gx < %d) && (gy < %d)) {\n" // work_items[0], work_items[1]
		, work_items[0], work_items[1]);
	opencl_kernel_code += item;

	// Input: NV12 only
	opencl_kernel_code += GetColorConversionTableForYUVInput(input_color_space);
	opencl_kernel_code += Read2x8PixelsFromYbufferAndUVbuffer();
	if (output_format == VX_DF_IMAGE_RGB)
	{
		opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
		opencl_kernel_code += ConvertNV12toRGB2();
		opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB2buffer() : Write1x4PixelsToRGB2buffer();
	}
	else if (output_format == VX_DF_IMAGE_RGBX)
	{
		opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
		opencl_kernel_code += ConvertNV12toRGBX();
		opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGBXbuffer() : Write1x4PixelsToRGBXbuffer();
	}
	else if (output_format == VX_DF_IMAGE_RGB4_AMD)
	{
		opencl_kernel_code += GetRangeConversionTableFor8bitTo15bit(input_channel_range);
		opencl_kernel_code += ConvertNV12toRGB4();
		opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB4buffer() : Write1x4PixelsToRGB4buffer();
	}
	opencl_kernel_code +=
		"  }\n"
		"}\n";

	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK color_convert_from_NV12_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	return VX_ERROR_NOT_SUPPORTED;
}

//! \brief The kernel publisher.
vx_status color_convert_from_NV12_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddKernel(context, "com.amd.loomsl.color_convert_from_NV12",
		AMDOVX_KERNEL_STITCHING_COLOR_CONVERT_FROM_NV12,
		color_convert_from_NV12_kernel,
		3,
		color_convert_from_NV12_input_validator,
		color_convert_from_NV12_output_validator,
		nullptr,
		nullptr);
	ERROR_CHECK_OBJECT(kernel);
	amd_kernel_query_target_support_f query_target_support_f = color_convert_from_NV12_query_target_support;
	amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = color_convert_from_NV12_opencl_codegen;
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}

//! \brief The input validator callback.
static vx_status VX_CALLBACK color_convert_from_IYUV_input_validator(vx_node node, vx_uint32 index)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	// get reference for parameter at specified index
	vx_reference ref = avxGetNodeParamRef(node, index);
	ERROR_CHECK_OBJECT(ref);
	// validate each parameter
	if (index == 0)
	{ // image of format IYUV > first plane U8
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_U8){
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	if (index == 1)
	{ // image of format IYUV > second plane U8
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_U8){
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	if (index == 2)
	{ // image of format IYUV > third plane U8
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_U8){
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	return status;
}

//! \brief The output validator callback.
static vx_status VX_CALLBACK color_convert_from_IYUV_output_validator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 3)
	{ // image of format RGB or RGBX or RGB4
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 input_width = 0, input_height = 0;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		image = (vx_image)avxGetNodeParamRef(node, index);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 output_width = 0, output_height = 0;
		vx_df_image output_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		if (!(output_width == input_width && output_height == input_height) &&
			!(output_width == ((input_width + 1) / 2) && output_height == ((input_height + 1) / 2)))
		{
			// pick input dimensions as default
			output_width = input_width;
			output_height = input_height;
		}
		if (output_format != VX_DF_IMAGE_RGB && output_format != VX_DF_IMAGE_RGBX && output_format != VX_DF_IMAGE_RGB4_AMD) {
			// for 8bit YUV input take RGB as default output
			output_format = VX_DF_IMAGE_RGB;
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
static vx_status VX_CALLBACK color_convert_from_IYUV_query_target_support(vx_graph graph, vx_node node,
	vx_bool use_opencl_1_2,              // [input]  false: OpenCL driver is 2.0+; true: OpenCL driver is 1.2
	vx_uint32& supported_target_affinity // [output] must be set to AGO_TARGET_AFFINITY_CPU or AGO_TARGET_AFFINITY_GPU or (AGO_TARGET_AFFINITY_CPU | AGO_TARGET_AFFINITY_GPU)
	)
{
	supported_target_affinity = AGO_TARGET_AFFINITY_GPU;
	return VX_SUCCESS;
}

//! \brief The OpenCL code generator callback.
static vx_status VX_CALLBACK color_convert_from_IYUV_opencl_codegen(
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
	vx_uint32 input_width = 0, input_height = 0, output_width = 0, output_height = 0;
	vx_df_image input_format = VX_DF_IMAGE_VIRT, output_format = VX_DF_IMAGE_VIRT;
	vx_channel_range_e input_channel_range, output_channel_range;
	vx_color_space_e input_color_space, output_color_space;
	vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &input_format, sizeof(input_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &input_channel_range, sizeof(input_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &input_color_space, sizeof(input_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));
	image = (vx_image)avxGetNodeParamRef(node, 3);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &output_channel_range, sizeof(output_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &output_color_space, sizeof(output_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));

	// set kernel configuration
	vx_uint32 work_items[2];
	work_items[0] = (input_width + 7) / 8;
	work_items[1] = (input_height + 1) / 2;
	strcpy(opencl_kernel_function_name, "color_convert");
	opencl_work_dim = 2;
	opencl_local_work[0] = 16;
	opencl_local_work[1] = 4;
	opencl_global_work[0] = (work_items[0] + opencl_local_work[0] - 1) & ~(opencl_local_work[0] - 1);
	opencl_global_work[1] = (work_items[1] + opencl_local_work[1] - 1) & ~(opencl_local_work[1] - 1);

	// kernel header and reading
	opencl_kernel_code =
		"#pragma OPENCL EXTENSION cl_amd_media_ops : enable\n"
		"#pragma OPENCL EXTENSION cl_amd_media_ops2 : enable\n";

	//Define help functions for 10 and 16 bit
	if (output_format == VX_DF_IMAGE_RGB4_AMD) opencl_kernel_code += Create_amd_pack15();

	char item[8192];
	sprintf(item,
		"__kernel __attribute__((reqd_work_group_size(%d, %d, 1)))\n" // opencl_local_work[0], opencl_local_work[1]
		"void %s(\n" // opencl_kernel_function_name
		, (int)opencl_local_work[0], (int)opencl_local_work[1], opencl_kernel_function_name);
	opencl_kernel_code += item;
	opencl_kernel_code +=
		"	uint pY_width, uint pY_height, __global uchar * pY_buf, uint pY_stride, uint pY_offset,\n    "
		"	uint pU_width, uint pU_height, __global uchar * pU_buf, uint pU_stride, uint pU_offset,\n"
		"	uint pV_width, uint pV_height, __global uchar * pV_buf, uint pV_stride, uint pV_offset,\n"
		"	uint pRGB_width, uint pRGB_height, __global uchar * pRGB_buf, uint pRGB_stride, uint pRGB_offset)\n";
	sprintf(item,
		"{\n"
		"  int gx = get_global_id(0);\n"
		"  int gy = get_global_id(1);\n"
		"  if ((gx < %d) && (gy < %d)) {\n" // work_items[0], work_items[1]
		, work_items[0], work_items[1]);
	opencl_kernel_code += item;

	// Input: NV12 only
	opencl_kernel_code += GetColorConversionTableForYUVInput(input_color_space);
	opencl_kernel_code += Read2x8PixelsFromYbufferAndUbufferAndVbuffer();
	if (output_format == VX_DF_IMAGE_RGB)
	{
		opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
		opencl_kernel_code += ConvertIYUVtoRGB2();
		opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB2buffer() : Write1x4PixelsToRGB2buffer();
	}
	else if (output_format == VX_DF_IMAGE_RGBX)
	{
		opencl_kernel_code += GetRangeConversionTableFor8bitTo8bit(input_channel_range);
		opencl_kernel_code += ConvertIYUVtoRGBX();
		opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGBXbuffer() : Write1x4PixelsToRGBXbuffer();
	}
	else if (output_format == VX_DF_IMAGE_RGB4_AMD)
	{
		opencl_kernel_code += GetRangeConversionTableFor8bitTo15bit(input_channel_range);
		opencl_kernel_code += ConvertIYUVtoRGB4();
		opencl_kernel_code += (input_width == output_width && input_height == output_height) ? Write2x8PixelsToRGB4buffer() : Write1x4PixelsToRGB4buffer();
	}

	opencl_kernel_code +=
		"  }\n"
		"}\n";

	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK color_convert_from_IYUV_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	return VX_ERROR_NOT_SUPPORTED;
}

//! \brief The kernel publisher.
vx_status color_convert_from_IYUV_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddKernel(context, "com.amd.loomsl.color_convert_from_IYUV",
		AMDOVX_KERNEL_STITCHING_COLOR_CONVERT_FROM_IYUV,
		color_convert_from_IYUV_kernel,
		4,
		color_convert_from_IYUV_input_validator,
		color_convert_from_IYUV_output_validator,
		nullptr,
		nullptr);
	ERROR_CHECK_OBJECT(kernel);
	amd_kernel_query_target_support_f query_target_support_f = color_convert_from_IYUV_query_target_support;
	amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = color_convert_from_IYUV_opencl_codegen;
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 3, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}

//! \brief The input validator callback.
static vx_status VX_CALLBACK color_convert_to_NV12_input_validator(vx_node node, vx_uint32 index)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	// get reference for parameter at specified index
	vx_reference ref = avxGetNodeParamRef(node, index);
	ERROR_CHECK_OBJECT(ref);
	// validate each parameter
	if (index == 0)
	{ // image of RGB2 or RGB4
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_RGB || format == VX_DF_IMAGE_RGB4_AMD) {
			status = VX_SUCCESS;
		}
		else {
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);
		}
		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	return status;
}

//! \brief The output validator callback.
static vx_status VX_CALLBACK color_convert_to_NV12_output_validator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 1)
	{ // image of format U8 as first plane of NV12
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 input_width = 0, input_height = 0;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		image = (vx_image)avxGetNodeParamRef(node, index);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 output_width = 0, output_height = 0;
		vx_df_image output_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		if (output_width != input_width || output_height != input_height)
		{
			// pick input dimensions as default
			output_width = input_width;
			output_height = input_height;
		}
		if (output_format != VX_DF_IMAGE_U8) {
			// for 8 bit RGB take UYVY as default output
			output_format = VX_DF_IMAGE_U8;
		}

		// set output image meta data
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		status = VX_SUCCESS;
	}
	if (index == 2)
	{ // image of format U16 as seconde plane of NV12
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 input_width = 0, input_height = 0;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		image = (vx_image)avxGetNodeParamRef(node, index);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 output_width = 0, output_height = 0;
		vx_df_image output_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		if (output_width * 2 != input_width || output_height * 2 != input_height)
		{
			// pick input dimensions as default
			output_width = (input_width + 1) / 2;
			output_height = (input_height + 1) / 2;
		}
		if (output_format != VX_DF_IMAGE_U16) {
			// for 8 bit RGB take UYVY as default output
			output_format = VX_DF_IMAGE_U16;
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
static vx_status VX_CALLBACK color_convert_to_NV12_query_target_support(vx_graph graph, vx_node node,
	vx_bool use_opencl_1_2,              // [input]  false: OpenCL driver is 2.0+; true: OpenCL driver is 1.2
	vx_uint32& supported_target_affinity // [output] must be set to AGO_TARGET_AFFINITY_CPU or AGO_TARGET_AFFINITY_GPU or (AGO_TARGET_AFFINITY_CPU | AGO_TARGET_AFFINITY_GPU)
	)
{
	supported_target_affinity = AGO_TARGET_AFFINITY_GPU;
	return VX_SUCCESS;
}

//! \brief The OpenCL code generator callback.
static vx_status VX_CALLBACK color_convert_to_NV12_opencl_codegen(
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
	vx_uint32 input_width = 0, input_height = 0, output_width = 0, output_height = 0;
	vx_df_image input_format = VX_DF_IMAGE_VIRT, output_format = VX_DF_IMAGE_VIRT;
	vx_channel_range_e input_channel_range, output_channel_range;
	vx_color_space_e input_color_space, output_color_space;
	vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &input_format, sizeof(input_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &input_channel_range, sizeof(input_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &input_color_space, sizeof(input_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));
	image = (vx_image)avxGetNodeParamRef(node, 1);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &output_channel_range, sizeof(output_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &output_color_space, sizeof(output_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));

	// set kernel configuration
	vx_uint32 work_items[2];
	work_items[0] = (input_width + 7) / 8;
	work_items[1] = (input_height + 1) / 2;
	strcpy(opencl_kernel_function_name, "color_convert");
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
	if (input_format == VX_DF_IMAGE_RGB4_AMD) opencl_kernel_code += Create_amd_unpackAB();

	char item[8192];
	sprintf(item,
		"__kernel __attribute__((reqd_work_group_size(%d, %d, 1)))\n" // opencl_local_work[0], opencl_local_work[1]
		"void %s(\n" // opencl_kernel_function_name
		, (int)opencl_local_work[0], (int)opencl_local_work[1], opencl_kernel_function_name);
	opencl_kernel_code += item;
	opencl_kernel_code +=
		"	uint pRGB_width, uint pRGB_height, __global uchar * pRGB_buf, uint pRGB_stride, uint pRGB_offset,\n"
		"	uint pY_width, uint pY_height, __global uchar * pY_buf, uint pY_stride, uint pY_offset,\n"
		"	uint pUV_width, uint pUV_height, __global uchar * pUV_buf, uint pUV_stride, uint pUV_offset)\n";
	sprintf(item,
		"{\n"
		"  int gx = get_global_id(0);\n"
		"  int gy = get_global_id(1);\n"
		"  if ((gx < %d) && (gy < %d)) {\n" // work_items[0], work_items[1]
		, work_items[0], work_items[1]);
	opencl_kernel_code += item;

	// Input: RGB2
	if (input_format == VX_DF_IMAGE_RGB){
		opencl_kernel_code += GetColorRangeConversionTableFor8bitTo8bit(input_color_space, input_channel_range);
		opencl_kernel_code += Read2x8PixelsFromRGBbuffer8bit();
		opencl_kernel_code += ConvertRGB2toNV12();
		opencl_kernel_code += Write2x8PixelsToYbufferAndUVbuffer();
	}

	// Input: RGB4
	else if (input_format == VX_DF_IMAGE_RGB4_AMD){
		opencl_kernel_code += GetColorRangeConversionTableFor15bitTo8bit(input_color_space, input_channel_range);
		opencl_kernel_code += Read2x8PixelsFromRGBbuffer16bit();
		opencl_kernel_code += ConvertRGB4toNV12();
		opencl_kernel_code += Write2x8PixelsToYbufferAndUVbuffer();
	}

	opencl_kernel_code +=
		"  }\n"
		"}\n";

	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK color_convert_to_NV12_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	return VX_ERROR_NOT_SUPPORTED;
}

//! \brief The kernel publisher.
vx_status color_convert_to_NV12_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddKernel(context, "com.amd.loomsl.color_convert_to_NV12",
		AMDOVX_KERNEL_STITCHING_COLOR_CONVERT_TO_NV12,
		color_convert_to_NV12_kernel,
		3,
		color_convert_to_NV12_input_validator,
		color_convert_to_NV12_output_validator,
		nullptr,
		nullptr);
	ERROR_CHECK_OBJECT(kernel);
	amd_kernel_query_target_support_f query_target_support_f = color_convert_to_NV12_query_target_support;
	amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = color_convert_to_NV12_opencl_codegen;
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}

//! \brief The input validator callback.
static vx_status VX_CALLBACK color_convert_to_IYUV_input_validator(vx_node node, vx_uint32 index)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	// get reference for parameter at specified index
	vx_reference ref = avxGetNodeParamRef(node, index);
	ERROR_CHECK_OBJECT(ref);
	// validate each parameter
	if (index == 0)
	{ // image of RGB2 or RGB4
		vx_df_image format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format)));
		if (format == VX_DF_IMAGE_RGB || format == VX_DF_IMAGE_RGB4_AMD) {
			status = VX_SUCCESS;
			status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: color_convert_general doesn't support input image format: %4.4s\n", &format);

		ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&ref));
	}
	return status;
}

//! \brief The output validator callback.
static vx_status VX_CALLBACK color_convert_to_IYUV_output_validator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 1)
	{ // image of format U8 as first, seconde or third plane of IYUV
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 input_width = 0, input_height = 0;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		image = (vx_image)avxGetNodeParamRef(node, index);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 output_width = 0, output_height = 0;
		vx_df_image output_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		if (output_width != input_width || output_height != input_height)
		{
			// pick input dimensions as default
			output_width = input_width;
			output_height = input_height;
		}
		if (output_format != VX_DF_IMAGE_U8) {
			// for 8 bit RGB take UYVY as default output
			output_format = VX_DF_IMAGE_U8;
		}

		// set output image meta data
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		status = VX_SUCCESS;
	}
	if (index == 2 || index == 3)
	{ // image of format U8 as first, seconde or third plane of IYUV
		// get image configuration
		vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 input_width = 0, input_height = 0;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		image = (vx_image)avxGetNodeParamRef(node, index);
		ERROR_CHECK_OBJECT(image);
		vx_uint32 output_width = 0, output_height = 0;
		vx_df_image output_format = VX_DF_IMAGE_VIRT;
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
		ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
		ERROR_CHECK_STATUS(vxReleaseImage(&image));
		if (output_width * 2 != input_width || output_height * 2 != input_height)
		{
			// pick input dimensions as default
			output_width = (input_width + 1) / 2;
			output_height = (input_height + 1) / 2;
		}
		if (output_format != VX_DF_IMAGE_U8) {
			// for 8 bit RGB take UYVY as default output
			output_format = VX_DF_IMAGE_U8;
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
static vx_status VX_CALLBACK color_convert_to_IYUV_query_target_support(vx_graph graph, vx_node node,
	vx_bool use_opencl_1_2,              // [input]  false: OpenCL driver is 2.0+; true: OpenCL driver is 1.2
	vx_uint32& supported_target_affinity // [output] must be set to AGO_TARGET_AFFINITY_CPU or AGO_TARGET_AFFINITY_GPU or (AGO_TARGET_AFFINITY_CPU | AGO_TARGET_AFFINITY_GPU)
	)
{
	supported_target_affinity = AGO_TARGET_AFFINITY_GPU;
	return VX_SUCCESS;
}

//! \brief The OpenCL code generator callback.
static vx_status VX_CALLBACK color_convert_to_IYUV_opencl_codegen(
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
	vx_uint32 input_width = 0, input_height = 0, output_width = 0, output_height = 0;
	vx_df_image input_format = VX_DF_IMAGE_VIRT, output_format = VX_DF_IMAGE_VIRT;
	vx_channel_range_e input_channel_range, output_channel_range;
	vx_color_space_e input_color_space, output_color_space;
	vx_image image = (vx_image)avxGetNodeParamRef(node, 0);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &input_width, sizeof(input_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &input_height, sizeof(input_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &input_format, sizeof(input_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &input_channel_range, sizeof(input_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &input_color_space, sizeof(input_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));
	image = (vx_image)avxGetNodeParamRef(node, 1);
	ERROR_CHECK_OBJECT(image);
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &output_width, sizeof(output_width)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &output_height, sizeof(output_height)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_RANGE, &output_channel_range, sizeof(output_channel_range)));
	ERROR_CHECK_STATUS(vxQueryImage(image, VX_IMAGE_ATTRIBUTE_SPACE, &output_color_space, sizeof(output_color_space)));
	ERROR_CHECK_STATUS(vxReleaseImage(&image));

	// set kernel configuration
	vx_uint32 work_items[2];
	work_items[0] = (input_width + 7) / 8;
	work_items[1] = (input_height + 1) / 2;
	strcpy(opencl_kernel_function_name, "color_convert");
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
	if (input_format == VX_DF_IMAGE_RGB4_AMD) opencl_kernel_code += Create_amd_unpackAB();

	char item[8192];
	sprintf(item,
		"__kernel __attribute__((reqd_work_group_size(%d, %d, 1)))\n" // opencl_local_work[0], opencl_local_work[1]
		"void %s(\n" // opencl_kernel_function_name
		, (int)opencl_local_work[0], (int)opencl_local_work[1], opencl_kernel_function_name);
	opencl_kernel_code += item;
	opencl_kernel_code +=
		"	uint pRGB_width, uint pRGB_height, __global uchar * pRGB_buf, uint pRGB_stride, uint pRGB_offset,\n"
		"	uint pY_width, uint pY_height, __global uchar * pY_buf, uint pY_stride, uint pY_offset,\n"
		"	uint pU_width, uint pU_height, __global uchar * pU_buf, uint pU_stride, uint pU_offset,\n"
		"	uint pV_width, uint pV_height, __global uchar * pV_buf, uint pV_stride, uint pV_offset)\n";
	sprintf(item,
		"{\n"
		"  int gx = get_global_id(0);\n"
		"  int gy = get_global_id(1);\n"
		"  if ((gx < %d) && (gy < %d)) {\n" // work_items[0], work_items[1]
		, work_items[0], work_items[1]);
	opencl_kernel_code += item;

	// Input: RGB2
	if (input_format == VX_DF_IMAGE_RGB){
		opencl_kernel_code += GetColorRangeConversionTableFor8bitTo8bit(input_color_space, input_channel_range);
		opencl_kernel_code += Read2x8PixelsFromRGBbuffer8bit();
		opencl_kernel_code += ConvertRGB2toIYUV();
		opencl_kernel_code += Write2x8PixelsToYbufferAndUbufferAndVbuffer();
	}

	// Input: RGB4
	else if (input_format == VX_DF_IMAGE_RGB4_AMD){
		opencl_kernel_code += GetColorRangeConversionTableFor15bitTo8bit(input_color_space, input_channel_range);
		opencl_kernel_code += Read2x8PixelsFromRGBbuffer16bit();
		opencl_kernel_code += ConvertRGB4toIYUV();
		opencl_kernel_code += Write2x8PixelsToYbufferAndUbufferAndVbuffer();
	}

	opencl_kernel_code +=
		"  }\n"
		"}\n";

	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK color_convert_to_IYUV_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	return VX_ERROR_NOT_SUPPORTED;
}

//! \brief The kernel publisher.
vx_status color_convert_to_IYUV_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddKernel(context, "com.amd.loomsl.color_convert_to_IYUV",
		AMDOVX_KERNEL_STITCHING_COLOR_CONVERT_TO_IYUV,
		color_convert_to_IYUV_kernel,
		4,
		color_convert_to_IYUV_input_validator,
		color_convert_to_IYUV_output_validator,
		nullptr,
		nullptr);
	ERROR_CHECK_OBJECT(kernel);
	amd_kernel_query_target_support_f query_target_support_f = color_convert_to_IYUV_query_target_support;
	amd_kernel_opencl_codegen_callback_f opencl_codegen_callback_f = color_convert_to_IYUV_opencl_codegen;
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_QUERY_TARGET_SUPPORT, &query_target_support_f, sizeof(query_target_support_f)));
	ERROR_CHECK_STATUS(vxSetKernelAttribute(kernel, VX_KERNEL_ATTRIBUTE_AMD_OPENCL_CODEGEN_CALLBACK, &opencl_codegen_callback_f, sizeof(opencl_codegen_callback_f)));

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 3, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}

// Input Color Conversions  ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string GetColorConversionTableForYUVInput(vx_color_space_e input_color_space)
{
	std::string output;
	if (input_color_space == VX_COLOR_SPACE_BT601_525 || input_color_space == VX_COLOR_SPACE_BT601_625) {
		output =
			"    float2 cR = (float2)( 0.0000f,  1.4030f);\n"
			"    float2 cG = (float2)(-0.3440f, -0.7140f);\n"
			"    float2 cB = (float2)( 1.7730f,  0.0000f);\n";
	}
	else if (input_color_space == VX_COLOR_SPACE_BT2020){
		output =
			"    float2 cR = (float2)( 0.0000f,  1.4746f);\n"
			"    float2 cG = (float2)(-0.16455f, -0.57135f);\n"
			"    float2 cB = (float2)( 1.8814f,  0.0000f);\n";
	}
	else{ // VX_COLOR_SPACE_BT709-5 Part 1: 60 Hz
		output =
			"    float2 cR = (float2)( 0.0000f,  1.5748f);\n"
			"    float2 cG = (float2)(-0.1873f, -0.4681f);\n"
			"    float2 cB = (float2)( 1.8556f,  0.0000f);\n";
	}
	return output;
}

//Input Range Conversion ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string GetRangeConversionTableFor8bitTo8bit(vx_channel_range_e input_channel_range){
	std::string output;
	if (input_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
		output =
			"    float4 r2f = (float4)(255.0f/219.0f, -16.0f*255.0f/219.0f, 255.0f/224.0f, -128.0f*255.0f/224.0f);\n";
	}
	else { // VX_CHANNEL_RANGE_FULL
		output =
			"    float4 r2f = (float4)(1.0f, 0.0f, 1.0f, -128.0f);\n";
	}
	return output;
}
std::string GetRangeConversionTableFor8bitTo15bit(vx_channel_range_e input_channel_range){
	std::string output;
	if (input_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
		output =
			"    float4 r2f = (float4)(32767.0f/219.0f, -16.0f*32767.0f/219.0f, 32767.0f/224.0f, -128.0f*32767.0f/224.0f);\n";
	}
	else { // VX_CHANNEL_RANGE_FULL
		output =
			"    float4 r2f = (float4)(32767.0f/255.0f, 0.0f, 32767.0f/255.0f, -128.0f*32767.0f/255.0f);\n";;
	}
	return output;
}
std::string GetRangeConversionTableFor10bitTo8bit(vx_channel_range_e input_channel_range){
	std::string output;
	if (input_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
		output =
			"    float4 r2f = (float4)(255.0f/(219.0f*4.0f), -16.0f*255.0f/219.0f, 255.0f/(224.0f*4.0f), -128.0f*255.0f/224.0f);\n";
	}
	else { // VX_CHANNEL_RANGE_FULL
		output =
			"    float4 r2f = (float4)(255.0f/1023.0f, 0.0f, 255.0f/1023.0f, -128.0f);\n";
	}
	return output;
}
std::string GetRangeConversionTableFor10bitTo15bit(vx_channel_range_e input_channel_range){
	std::string output;
	if (input_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
		output =
			"    float4 r2f = (float4)(32767.0f/(219.0f*4.0f), -16.0f*32767.0f/219.0f, 32767.0f/(224.0f*4.0f), -128.0f*32767.0f/224.0f);\n";
	}
	else { // VX_CHANNEL_RANGE_FULL
		output =
			"    float4 r2f = (float4)(32767.0f/1023.0f, 0.0f, 32767.0f/1023.0f, -128.0f*32767.0f/255.0f);\n";
	}
	return output;
}
std::string GetRangeConversionTableFor16bitTo8bit(vx_channel_range_e input_channel_range){
	std::string output;
	if (input_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
		output =
			"    float4 r2f = (float4)(255.0f/(219.0f*256.0f), -16.0f*255.0f/219.0f, 255.0f/(224.0f*256.0f), -128.0f*255.0f/224.0f);\n";
	}
	else { // VX_CHANNEL_RANGE_FULL
		output =
			"    float4 r2f = (float4)(255.0f/65535.0f, 0.0f, 255.0f/65535.0f, -128.0f);\n";
	}
	return output;
}
std::string GetRangeConversionTableFor16bitTo15bit(vx_channel_range_e input_channel_range){
	std::string output;
	if (input_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
		output =
			"    float4 r2f = (float4)(32767.0f/(219.0f*256.0f), -16.0f*32767.0f/219.0f, 32767.0f/(224.0f*256.0f), -128.0f*32767.0f/224.0f);\n";
	}
	else { // VX_CHANNEL_RANGE_FULL
		output =
			"    float4 r2f = (float4)(32767.0f/65535.0f, 0.0f, 32767.0f/65535.0f, -128.0f*32767.0f/255.0f);\n";
	}
	return output;
}

// Output Color and Range Conversion ------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string GetColorRangeConversionTableFor8bitTo8bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range){
	std::string output;
	if (output_color_space == VX_COLOR_SPACE_BT601_525 || output_color_space == VX_COLOR_SPACE_BT601_625) {
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)(0.2990f*219.0f/255.0f, 0.5870f*219.0f/255.0f, 0.1140f*219.0f/255.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.1690f*224.0f/255.0f, -0.3310f*224.0f/255.0f, 0.5f*224.0f/255.0f);\n"
				"    float3 cV = (float3)(0.5f*224.0f/255.0f, -0.4190f*224.0f/255.0f, -0.0810f*224.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(0.2990f, 0.5870f, 0.1140f,0.0f);\n"
				"    float3 cU = (float3)(-0.1690f, -0.3310f, 0.5f);\n"
				"    float3 cV = (float3)(0.5f, -0.4190f, -0.0810f);\n";
		}
	}
	else if (output_color_space == VX_COLOR_SPACE_BT2020){ // VX_COLOR_SPACE_BT2020
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)(  0.2627f*219.0f/255.0f,    0.678f*219.0f/255.0f,   0.0593f*219.0f/255.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.13963f*224.0f/255.0f, -0.36037f*224.0f/255.0f,      0.5f*224.0f/255.0f);\n"
				"    float3 cV = (float3)(     0.5f*224.0f/255.0f, -0.45979f*224.0f/255.0f, -0.04021f*224.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(0.2627f,  0.678f, 0.0593f,0.0f);\n"
				"    float3 cU = (float3)(-0.13963f, -0.36037f, 0.5f);\n"
				"    float3 cV = (float3)(0.5f, -0.45979f, -0.04021f);\n";
		}
	}
	else { // VX_COLOR_SPACE_BT709-5 Part 1: 60 Hz
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2126f*219.0f/255.0f,  0.7152f*219.0f/255.0f,  0.0722f*219.0f/255.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.1146f*224.0f/255.0f, -0.3854f*224.0f/255.0f,     0.5f*224.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f/255.0f, -0.4542f*224.0f/255.0f, -0.0458f*224.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(0.2126f, 0.7152f, 0.0722f,0.0f);\n"
				"    float3 cU = (float3)(-0.1146f, -0.3854f, 0.5f);\n"
				"    float3 cV = (float3)(0.5f, -0.4542f, -0.0458f);\n";
		}
	}
	return output;
}
std::string GetColorRangeConversionTableFor8bitTo10bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range){
	std::string output;
	if (output_color_space == VX_COLOR_SPACE_BT601_525 || output_color_space == VX_COLOR_SPACE_BT601_625) {
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2990f*219.0f*4.0f/255.0f,  0.5870f*219.0f*4.0f/255.0f,  0.1140f*219.0f*4.0f/255.0f,16.0f*4.0f);\n"
				"    float3 cU = (float3)(-0.1690f*224.0f*4.0f/255.0f, -0.3310f*224.0f*4.0f/255.0f,     0.5f*224.0f*4.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*4.0f/255.0f, -0.4190f*224.0f*4.0f/255.0f, -0.0810f*224.0f*4.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2990f*1023.0f/255.0f,  0.5870f*1023.0f/255.0f,  0.1140f*1023.0f/255.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1690f*1023.0f/255.0f, -0.3310f*1023.0f/255.0f,     0.5f*1023.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*1023.0f/255.0f, -0.4190f*1023.0f/255.0f, -0.0810f*1023.0f/255.0f);\n";
		}
	}
	else if (output_color_space == VX_COLOR_SPACE_BT2020){ // VX_COLOR_SPACE_BT2020
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)(  0.2627f*219.0f*4.0f/255.0f,    0.678f*219.0f*4.0f/255.0f,   0.0593f*219.0f*4.0f/255.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.13963f*224.0f*4.0f/255.0f, -0.36037f*224.0f*4.0f/255.0f,      0.5f*224.0f*4.0f/255.0f);\n"
				"    float3 cV = (float3)(     0.5f*224.0f*4.0f/255.0f, -0.45979f*224.0f*4.0f/255.0f, -0.04021f*224.0f*4.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(  0.2627f*1023.0f/255.0f,    0.678f*1023.0f/255.0f,   0.0593f*1023.0f/255.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.13963f*1023.0f/255.0f, -0.36037f*1023.0f/255.0f,      0.5f*1023.0f/255.0f);\n"
				"    float3 cV = (float3)(     0.5f*1023.0f/255.0f, -0.45979f*1023.0f/255.0f, -0.04021f*1023.0f/255.0f);\n";
		}
	}
	else { // VX_COLOR_SPACE_BT709-5 Part 1: 60 Hz
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2126f*219.0f*4.0f/255.0f,  0.7152f*219.0f*4.0f/255.0f,  0.0722f*219.0f*4.0f/255.0f,16.0f*4f);\n"
				"    float3 cU = (float3)(-0.1146f*224.0f*4.0f/255.0f, -0.3854f*224.0f*4.0f/255.0f,     0.5f*224.0f*4.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*4.0f/255.0f, -0.4542f*224.0f*4.0f/255.0f, -0.0458f*224.0f*4.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2126f*1023.0f/255.0f,  0.7152f*1023.0f/255.0f,  0.0722f*1023.0f/255.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1146f*1023.0f/255.0f, -0.3854f*1023.0f/255.0f,     0.5f*1023.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*1023.0f/255.0f, -0.4542f*1023.0f/255.0f, -0.0458f*1023.0f/255.0f);\n";
		}
	}
	return output;
}
std::string GetColorRangeConversionTableFor8bitTo16bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range){
	std::string output;
	if (output_color_space == VX_COLOR_SPACE_BT601_525 || output_color_space == VX_COLOR_SPACE_BT601_625) {
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2990f*219.0f*256.0f/255.0f,  0.5870f*219.0f*256.0f/255.0f,  0.1140f*219.0f*256.0f/255.0f,16.0f*256.0f);\n"
				"    float3 cU = (float3)(-0.1690f*224.0f*256.0f/255.0f, -0.3310f*224.0f*256.0f/255.0f,     0.5f*224.0f*256.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*256.0f/255.0f, -0.4190f*224.0f*256.0f/255.0f, -0.0810f*224.0f*256.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2990f*65535.0f/255.0f,  0.5870f*65535.0f/255.0f,  0.1140f*65535.0f/255.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1690f*65535.0f/255.0f, -0.3310f*65535.0f/255.0f,     0.5f*65535.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*65535.0f/255.0f, -0.4190f*65535.0f/255.0f, -0.0810f*65535.0f/255.0f);\n";
		}
	}
	else if (output_color_space == VX_COLOR_SPACE_BT2020){ // VX_COLOR_SPACE_BT2020
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)(  0.2627f*219.0f*256.0f/255.0f,    0.678f*219.0f*256.0f/255.0f,   0.0593f*219.0f*256.0f/255.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.13963f*224.0f*256.0f/255.0f, -0.36037f*224.0f*256.0f/255.0f,      0.5f*224.0f*256.0f/255.0f);\n"
				"    float3 cV = (float3)(     0.5f*224.0f*256.0f/255.0f, -0.45979f*224.0f*256.0f/255.0f, -0.04021f*224.0f*256.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(  0.2627f*65535.0f/255.0f,    0.678f*65535.0f/255.0f,   0.0593f*65535.0f/255.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.13963f*65535.0f/255.0f, -0.36037f*65535.0f/255.0f,      0.5f*65535.0f/255.0f);\n"
				"    float3 cV = (float3)(     0.5f*65535.0f/255.0f, -0.45979f*65535.0f/255.0f, -0.04021f*65535.0f/255.0f);\n";
		}
	}
	else { // VX_COLOR_SPACE_BT709-5 Part 1: 60 Hz
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2126f*219.0f*256.0f/255.0f,  0.7152f*219.0f*256.0f/255.0f,  0.0722f*219.0f*256.0f/255.0f,16.0f*4f);\n"
				"    float3 cU = (float3)(-0.1146f*224.0f*256.0f/255.0f, -0.3854f*224.0f*256.0f/255.0f,     0.5f*224.0f*256.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*256.0f/255.0f, -0.4542f*224.0f*256.0f/255.0f, -0.0458f*224.0f*256.0f/255.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2126f*65535.0f/255.0f,  0.7152f*65535.0f/255.0f,  0.0722f*65535.0f/255.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1146f*65535.0f/255.0f, -0.3854f*65535.0f/255.0f,     0.5f*65535.0f/255.0f);\n"
				"    float3 cV = (float3)(    0.5f*65535.0f/255.0f, -0.4542f*65535.0f/255.0f, -0.0458f*65535.0f/255.0f);\n";
		}
	}
	return output;
}
std::string GetColorRangeConversionTableFor15bitTo8bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range){
	std::string output;
	if (output_color_space == VX_COLOR_SPACE_BT601_525 || output_color_space == VX_COLOR_SPACE_BT601_625) {
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2990f*219.0f/32767.0f,  0.5870f*219.0f/32767.0f,  0.1140f*219.0f/32767.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.1690f*224.0f/32767.0f, -0.3310f*224.0f/32767.0f,     0.5f*224.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f/32767.0f, -0.4190f*224.0f/32767.0f, -0.0810f*224.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2990f*255.0f/32767.0f,  0.5870f*255.0f/32767.0f,  0.1140f*255.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1690f*255.0f/32767.0f, -0.3310f*255.0f/32767.0f,     0.5f*255.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*255.0f/32767.0f, -0.4190f*255.0f/32767.0f, -0.0810f*255.0f/32767.0f);\n";
		}
	}
	else if (output_color_space == VX_COLOR_SPACE_BT2020){ // VX_COLOR_SPACE_BT2020
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)(  0.2627f*219.0f/32767.0f,    0.678f*219.0f/32767.0f,   0.0593f*219.0f/32767.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.13963f*224.0f/32767.0f, -0.36037f*224.0f/32767.0f,      0.5f*224.0f/32767.0f);\n"
				"    float3 cV = (float3)(     0.5f*224.0f/32767.0f, -0.45979f*224.0f/32767.0f, -0.04021f*224.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(  0.2627f*255.0f/32767.0f,    0.678f*255.0f/32767.0f,   0.0593f*255.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.13963f*255.0f/32767.0f, -0.36037f*255.0f/32767.0f,      0.5f*255.0f/32767.0f);\n"
				"    float3 cV = (float3)(     0.5f*255.0f/32767.0f, -0.45979f*255.0f/32767.0f, -0.04021f*255.0f/32767.0f);\n";
		}
	}
	else { // VX_COLOR_SPACE_BT709-5 Part 1: 60 Hz
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2126f*219.0f/32767.0f,  0.7152f*219.0f/32767.0f,  0.0722f*219.0f/32767.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.1146f*224.0f/32767.0f, -0.3854f*224.0f/32767.0f,     0.5f*224.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f/32767.0f, -0.4542f*224.0f/32767.0f, -0.0458f*224.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2126f*255.0f/32767.0f,  0.7152f*255.0f/32767.0f,  0.0722f*255.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1146f*255.0f/32767.0f, -0.3854f*255.0f/32767.0f,     0.5f*255.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*255.0f/32767.0f, -0.4542f*255.0f/32767.0f, -0.0458f*255.0f/32767.0f);\n";
		}
	}
	return output;
}
std::string GetColorRangeConversionTableFor15bitTo10bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range){
	std::string output;
	if (output_color_space == VX_COLOR_SPACE_BT601_525 || output_color_space == VX_COLOR_SPACE_BT601_625) { // 15bit > 10bit
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2990f*219.0f*4.0f/32767.0f,  0.5870f*219.0f*4.0f/32767.0f,  0.1140f*219.0f*4.0f/32767.0f,16.0f*4.0f);\n"
				"    float3 cU = (float3)(-0.1690f*224.0f*4.0f/32767.0f, -0.3310f*224.0f*4.0f/32767.0f,     0.5f*224.0f*4.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*4.0f/32767.0f, -0.4190f*224.0f*4.0f/32767.0f, -0.0810f*224.0f*4.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2990f*1023.0f/32767.0f,  0.5870f*1023.0f/32767.0f,  0.1140f*1023.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1690f*1023.0f/32767.0f, -0.3310f*1023.0f/32767.0f,     0.5f*1023.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*1023.0f/32767.0f, -0.4190f*1023.0f/32767.0f, -0.0810f*1023.0f/32767.0f);\n";
		}
	}
	else if (output_color_space == VX_COLOR_SPACE_BT2020){ // VX_COLOR_SPACE_BT2020
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)(  0.2627f*219.0f*4.0f/32767.0f,    0.678f*219.0f*4.0f/32767.0f,   0.0593f*219.0f*4.0f/32767.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.13963f*224.0f*4.0f/32767.0f, -0.36037f*224.0f*4.0f/32767.0f,      0.5f*224.0f*4.0f/32767.0f);\n"
				"    float3 cV = (float3)(     0.5f*224.0f*4.0f/32767.0f, -0.45979f*224.0f*4.0f/32767.0f, -0.04021f*224.0f*4.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(  0.2627f*1023.0f/32767.0f,    0.678f*1023.0f/32767.0f,   0.0593f*1023.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.13963f*1023.0f/32767.0f, -0.36037f*1023.0f/32767.0f,      0.5f*1023.0f/32767.0f);\n"
				"    float3 cV = (float3)(     0.5f*1023.0f/32767.0f, -0.45979f*1023.0f/32767.0f, -0.04021f*1023.0f/32767.0f);\n";
		}
	}
	else { // VX_COLOR_SPACE_BT709-5 Part 1: 60 Hz
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2126f*219.0f*4.0f/32767.0f,  0.7152f*219.0f*4.0f/32767.0f,  0.0722f*219.0f*4.0f/32767.0f,16.0f*4.0f);\n"
				"    float3 cU = (float3)(-0.1146f*224.0f*4.0f/32767.0f, -0.3854f*224.0f*4.0f/32767.0f,     0.5f*224.0f*4.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*4.0f/32767.0f, -0.4542f*224.0f*4.0f/32767.0f, -0.0458f*224.0f*4.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2126f*1023.0f/32767.0f,  0.7152f*1023.0f/32767.0f,  0.0722f*1023.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1146f*1023.0f/32767.0f, -0.3854f*1023.0f/32767.0f,     0.5f*1023.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*1023.0f/32767.0f, -0.4542f*1023.0f/32767.0f, -0.0458f*1023.0f/32767.0f);\n";
		}
	}
	return output;
}
std::string GetColorRangeConversionTableFor15bitTo16bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range){
	std::string output;
	if (output_color_space == VX_COLOR_SPACE_BT601_525 || output_color_space == VX_COLOR_SPACE_BT601_625) {
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2990f*219.0f*256.0f/32767.0f,  0.5870f*219.0f*256.0f/32767.0f,  0.1140f*219.0f*256.0f/32767.0f,16.0f*256.0f);\n"
				"    float3 cU = (float3)(-0.1690f*224.0f*256.0f/32767.0f, -0.3310f*224.0f*256.0f/32767.0f,     0.5f*224.0f*256.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*256.0f/32767.0f, -0.4190f*224.0f*256.0f/32767.0f, -0.0810f*224.0f*256.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2990f*65535.0f/32767.0f,  0.5870f*65535.0f/32767.0f,  0.1140f*65535.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1690f*65535.0f/32767.0f, -0.3310f*65535.0f/32767.0f,     0.5f*65535.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*65535.0f/32767.0f, -0.4190f*65535.0f/32767.0f, -0.0810f*65535.0f/32767.0f);\n";
		}
	}
	else if (output_color_space == VX_COLOR_SPACE_BT2020){ // VX_COLOR_SPACE_BT2020
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)(  0.2627f*219.0f*256.0f/32767.0f,    0.678f*219.0f*256.0f/32767.0f,   0.0593f*219.0f*256.0f/32767.0f,16.0f);\n"
				"    float3 cU = (float3)(-0.13963f*224.0f*256.0f/32767.0f, -0.36037f*224.0f*256.0f/32767.0f,      0.5f*224.0f*256.0f/32767.0f);\n"
				"    float3 cV = (float3)(     0.5f*224.0f*256.0f/32767.0f, -0.45979f*224.0f*256.0f/32767.0f, -0.04021f*224.0f*256.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)(  0.2627f*65535.0f/32767.0f,    0.678f*65535.0f/32767.0f,   0.0593f*65535.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.13963f*65535.0f/32767.0f, -0.36037f*65535.0f/32767.0f,      0.5f*65535.0f/32767.0f);\n"
				"    float3 cV = (float3)(     0.5f*65535.0f/32767.0f, -0.45979f*65535.0f/32767.0f, -0.04021f*65535.0f/32767.0f);\n";
		}
	}
	else { // VX_COLOR_SPACE_BT709-5 Part 1: 60 Hz
		if (output_channel_range == VX_CHANNEL_RANGE_RESTRICTED) {
			output =
				"    float4 cY = (float4)( 0.2126f*219.0f*256.0f/32767.0f,  0.7152f*219.0f*256.0f/32767.0f,  0.0722f*219.0f*256.0f/32767.0f,16.0f*256.0f);\n"
				"    float3 cU = (float3)(-0.1146f*224.0f*256.0f/32767.0f, -0.3854f*224.0f*256.0f/32767.0f,     0.5f*224.0f*256.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*224.0f*256.0f/32767.0f, -0.4542f*224.0f*256.0f/32767.0f, -0.0458f*224.0f*256.0f/32767.0f);\n";
		}
		else { // VX_CHANNEL_RANGE_FULL
			output =
				"    float4 cY = (float4)( 0.2126f*65535.0f/32767.0f,  0.7152f*65535.0f/32767.0f,  0.0722f*65535.0f/32767.0f,0.0f);\n"
				"    float3 cU = (float3)(-0.1146f*65535.0f/32767.0f, -0.3854f*65535.0f/32767.0f,     0.5f*65535.0f/32767.0f);\n"
				"    float3 cV = (float3)(    0.5f*65535.0f/32767.0f, -0.4542f*65535.0f/32767.0f, -0.0458f*65535.0f/32767.0f);\n";
		}
	}
	return output;
}

// Read input from buffer -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string Read2x8PixelsFrom422buffer8bit(){
	std::string output =
		"    uint4 L0, L1;\n"
		"    p422_buf += p422_offset + (gy * p422_stride * 2) + (gx << 4);\n"
		"    L0 = *(__global uint4 *) p422_buf;\n"
		"    L1 = *(__global uint4 *)&p422_buf[p422_stride];\n";
	return output;
}
std::string Read2x8PixelsFromYbufferAndUVbuffer(){
	std::string output =
		"		pY_buf += pY_offset + (gy * pY_stride << 1) + (gx << 3);\n"
		"		pUV_buf += pUV_offset + (gy * pUV_stride) + (gx << 3);\n"
		"		uint2 pY0 = *(__global uint2 *) pY_buf;\n"
		"		uint2 pY1 = *(__global uint2 *)&pY_buf[pY_stride];\n"
		"		uint2 pUV = *(__global uint2 *) pUV_buf;\n";
	return output;
}
std::string Read2x8PixelsFromYbufferAndUbufferAndVbuffer(){
	std::string output =
		"    pY_buf += pY_offset + (gy * pY_stride << 1) + (gx << 3);\n"
		"    pU_buf += pU_offset + (gy * pU_stride) + (gx << 2);\n"
		"    pV_buf += pV_offset + (gy * pV_stride) + (gx << 2);\n"
		"    uint2 pY0 = *(__global uint2 *) pY_buf;\n"
		"    uint2 pY1 = *(__global uint2 *)&pY_buf[pY_stride];\n"
		"    uint pU = *(__global uint *) pU_buf;\n"
		"    uint pV = *(__global uint *) pV_buf;\n";
	return output;
}
std::string Read1x6PixelsFrom422buffer(){
	std::string output =
		"		uint8 L0;\n"
		"		p422_buf += p422_offset + (gy * p422_stride) + (gx << 5);\n"
		"		L0 = *(__global uint8 *) p422_buf;\n";
	return output;
}
std::string Read2x8PixelsFrom422buffer16bit(){
	std::string output =
		"    uint8 L0, L1;\n"
		"    p422_buf += p422_offset + (gy * p422_stride * 2) + (gx << 5);\n"
		"    L0 = *(__global uint8 *) p422_buf;\n"
		"    L1 = *(__global uint8 *)&p422_buf[p422_stride];\n";
	return output;
}
std::string Read2x8PixelsFromRGBbuffer8bit(){
	std::string output =
		"    uint8 L0, L1;\n"
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride * 2) + (gx * 24);\n"
		"    L0 = *(__global uint8 *) pRGB_buf;\n"
		"    L1 = *(__global uint8 *)&pRGB_buf[pRGB_stride];\n";
	return output;
}
std::string Read2x8PixelsFromRGBbuffer16bit(){
	std::string output =
		"    uint8 L0, L2;\n"
		"	 uint4 L1, L3;\n"
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride * 2) + (gx * 48);\n"
		"    L0 = *(__global uint8 *) pRGB_buf;\n"
		"    L1 = *(__global uint4 *)&pRGB_buf[32];\n"
		"    L2 = *(__global uint8 *)&pRGB_buf[pRGB_stride];\n"
		"    L3 = *(__global uint4 *)&pRGB_buf[pRGB_stride+32];\n";
	return output;
}
// Do Color Conversion --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string ConvertUYVYtoRGB2(){
	std::string output =
		"    uint8 pRGB0, pRGB1;\n"
		"    float4 rgbx; float y0, y1, u, v; rgbx.s3 = 0.0f;\n"
		"    u = mad(amd_unpack0(L0.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s0),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L0.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s1),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L0.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s2),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L0.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s3),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s7 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s0),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s1),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s2),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s3),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertUYVYtoRGBX(){
	std::string output =
		"    uint8 pRGB0, pRGB1;\n"
		"    float4 rgbx; float y0, y1, u, v;\n"
		"    u = mad(amd_unpack0(L0.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s0),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L0.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s1),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L0.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s2),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L0.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s3),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s7 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s0),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s1),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s2),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack0(L1.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s3),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertUYVYtoRGB4(){
	std::string output =
		"    uint8 pRGB0, pRGB1;\n"
		"    uint8 pRGB2, pRGB3;\n"
		"    float4 rgbx; float y0, y1, u, v; rgbx.s3 = 0.0f;\n"
		"    u = mad(amd_unpack0(L0.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s0),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB0.s0 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB0.s1 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB0.s1 += ((uint)rgbx.s0 << 16); pRGB0.s2 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack0(L0.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s1),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB0.s3 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB0.s4 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB0.s4 += ((uint)rgbx.s0 << 16); pRGB0.s5 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack0(L0.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s2),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB0.s6 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB0.s7 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB0.s7 += ((uint)rgbx.s0 << 16); pRGB1.s0 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack0(L0.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L0.s3),r2f.s0,r2f.s1); v = mad(amd_unpack2(L0.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L0.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB1.s1 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB1.s2 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB1.s2 += ((uint)rgbx.s0 << 16); pRGB1.s3 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack0(L1.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s0),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB2.s0 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB2.s1 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB2.s1 += ((uint)rgbx.s0 << 16); pRGB2.s2 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack0(L1.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s1),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB2.s3 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB2.s4 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB2.s4 += ((uint)rgbx.s0 << 16); pRGB2.s5 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack0(L1.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s2),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB2.s6 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB2.s7 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB2.s7 += ((uint)rgbx.s0 << 16); pRGB3.s0 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack0(L1.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack1(L1.s3),r2f.s0,r2f.s1); v = mad(amd_unpack2(L1.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack3(L1.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB3.s1 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB3.s2 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB3.s2 += ((uint)rgbx.s0 << 16); pRGB3.s3 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n";
	return output;
}
std::string ConvertYUYVtoRGB2(){
	std::string output =
		"    uint8 pRGB0, pRGB1;\n"
		"    float4 rgbx; float y0, y1, u, v; rgbx.s3 = 0.0f;\n"
		"    u = mad(amd_unpack1(L0.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s0),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L0.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s1),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L0.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s2),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L0.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s3),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB0.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB0.s7 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s0),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s1),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s2),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s3),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); pRGB1.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertYUYVtoRGBX(){
	std::string output =
		"    uint8 pRGB0, pRGB1;\n"
		"    float4 rgbx; float y0, y1, u, v;\n"
		"    u = mad(amd_unpack1(L0.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s0),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L0.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s1),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L0.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s2),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L0.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s3),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB0.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB0.s7 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s0),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s1 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s1),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s3 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s2),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s5 = amd_pack(rgbx);\n"
		"    u = mad(amd_unpack1(L1.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s3),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx.s3 = y0; pRGB1.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx.s3 = y1; pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertYUYVtoRGB4(){
	std::string output =
		"    uint8 pRGB0, pRGB2;\n"
		"    uint4 pRGB1, pRGB3;\n"
		"    float4 rgbx; float y0, y1, u, v; rgbx.s3 = 0.0f;\n"
		"    u = mad(amd_unpack1(L0.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s0),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB0.s0 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB0.s1 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB0.s1 += ((uint)rgbx.s0 << 16); pRGB0.s2 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack1(L0.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s1),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB0.s3 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB0.s4 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB0.s4 += ((uint)rgbx.s0 << 16); pRGB0.s5 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack1(L0.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s2),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB0.s6 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB0.s7 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB0.s7 += ((uint)rgbx.s0 << 16); pRGB1.s0 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack1(L0.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L0.s3),r2f.s0,r2f.s1); v = mad(amd_unpack3(L0.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L0.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB1.s1 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB1.s2 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB1.s2 += ((uint)rgbx.s0 << 16); pRGB1.s3 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack1(L1.s0),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s0),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s0),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s0),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB2.s0 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB2.s1 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB2.s1 += ((uint)rgbx.s0 << 16); pRGB2.s2 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack1(L1.s1),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s1),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s1),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s1),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB2.s3 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB2.s4 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB2.s4 += ((uint)rgbx.s0 << 16); pRGB2.s5 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack1(L1.s2),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s2),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s2),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s2),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB2.s6 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB2.s7 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB2.s7 += ((uint)rgbx.s0 << 16); pRGB3.s0 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n"
		"    u = mad(amd_unpack1(L1.s3),r2f.s2,r2f.s3); y0 = mad(amd_unpack0(L1.s3),r2f.s0,r2f.s1); v = mad(amd_unpack3(L1.s3),r2f.s2,r2f.s3); y1 = mad(amd_unpack2(L1.s3),r2f.s0,r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, v, y0); rgbx.s1 = mad(cG.s0, u, y0); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y0); rgbx = clamp (rgbx, 0, 32767); pRGB3.s1 = (((uint)rgbx.s1) << 16) + (uint)rgbx.s0; pRGB3.s2 = (uint)rgbx.s2;\n"
		"    rgbx.s0 = mad(cR.s1, v, y1); rgbx.s1 = mad(cG.s0, u, y1); rgbx.s1 = mad(cG.s1, v, rgbx.s1); rgbx.s2 = mad(cB.s0, u, y1); rgbx = clamp (rgbx, 0, 32767); pRGB3.s2 += ((uint)rgbx.s0 << 16); pRGB3.s3 = (((uint)rgbx.s2) << 16) + (uint)rgbx.s1;\n";
	return output;
}
std::string ConvertNV12toRGB2(){
	std::string output =
		"		uint8 pRGB0, pRGB1;\n"
		"		float3 yuv; float4 rgbx; rgbx.s3 = 0.0f;//NV12 > half scaled in both directions, UV interleaved\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pUV.s0),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pUV.s0),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s0 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s0 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s0),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s1 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s1 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pUV.s0),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pUV.s0),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s2 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s2 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s0),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s3 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s3 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pUV.s1),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pUV.s1),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s4 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s4 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s1),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s5 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s5 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pUV.s1),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pUV.s1),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s6 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s6 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s1),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s7 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertNV12toRGBX(){
	std::string output =
		"		uint8 pRGB0, pRGB1;\n"
		"		float3 yuv; float4 rgbx; //NV12 > half scaled in both directions, UV interleaved\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pUV.s0),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pUV.s0),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s0 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s0 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s1 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s1 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pUV.s0),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pUV.s0),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s2 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s2 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s3 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s3 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pUV.s1),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pUV.s1),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s4 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s4 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s5 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s5 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pUV.s1),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pUV.s1),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s6 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s6 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s7 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertNV12toRGB4(){
	std::string output =
		"		uint8 pRGB0, pRGB2;\n"
		"		uint4 pRGB1, pRGB3;\n"
		"		float3 yuv; float4 rgbx; rgbx.s3 = 0.0f;//NV12 > half scaled in both directions, UV interleaved\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pUV.s0),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pUV.s0),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s0 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s1 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s0 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s1 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s1 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s2 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s1 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB2.s2 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pUV.s0),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pUV.s0),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s3 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s4 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s3 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s4 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s4 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s5 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s4 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB2.s5 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pUV.s1),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pUV.s1),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s6 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s7 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s6 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s7 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s7 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s0 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s7 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB3.s0 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pUV.s1),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pUV.s1),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s1 = amd_pack15(rgbx.s0,rgbx.s1); pRGB1.s2 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB3.s1 = amd_pack15(rgbx.s0,rgbx.s1); pRGB3.s2 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s2 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s3 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB3.s2 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB3.s3 = amd_pack15(rgbx.s1,rgbx.s2);\n";
	return output;
}
std::string ConvertIYUVtoRGB2(){
	std::string output =
		"		uint8 pRGB0, pRGB1;\n"
		"		float3 yuv; float4 rgbx; rgbx.s3 = 0.0f;//IYUV > All planes seperate\n"
		"		//pixel[0]\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack0(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s0 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s0 = amd_pack(rgbx);\n"
		"		//pixel[1]\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s0),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s1 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s1 = amd_pack(rgbx);\n"
		"		//pixel[2]\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack1(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s2 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s2 = amd_pack(rgbx);\n"
		"		//pixel[3]\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s0),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s3 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s3 = amd_pack(rgbx);\n"
		"		//pixel[4]\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack2(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s4 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s4 = amd_pack(rgbx);\n"
		"		//pixel[5]\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s1),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s5 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s5 = amd_pack(rgbx);\n"
		"		//pixel[6]\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack3(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s6 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s6 = amd_pack(rgbx);\n"
		"		//pixel[7]\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s1),r2f.s0,r2f.s1); //first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s7 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertIYUVtoRGBX(){
	std::string output =
		"		uint8 pRGB0, pRGB1;\n"
		"		float3 yuv; float4 rgbx; //IYUV > All planes seperate\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack0(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s0 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s0 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s1 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s1 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack1(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s2 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s2 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s3 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s3 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack2(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s4 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s4 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s5 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s5 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack3(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s6 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s6 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB0.s7 = amd_pack(rgbx);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0); rgbx.s3 = yuv.s0;\n"
		"		pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertIYUVtoRGB4(){
	std::string output =
		"		uint8 pRGB0, pRGB2;\n"
		"		uint4 pRGB1, pRGB3;\n"
		"		float3 yuv; float4 rgbx; rgbx.s3 = 0.0f;//IYUV > All planes seperate\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack0(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack0(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s0 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s1 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s0 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s1 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s1 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s2 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s1 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB2.s2 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s0),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack1(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack1(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s3 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s4 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s3 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s4 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s0),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s4 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s5 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s0),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s4 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB2.s5 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack0(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack2(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack2(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s6 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s7 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack0(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s6 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s7 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack1(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB0.s7 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s0 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack1(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB2.s7 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB3.s0 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack2(pY0.s1),r2f.s0,r2f.s1); yuv.s1 = mad(amd_unpack3(pU),r2f.s2,r2f.s3); yuv.s2 = mad(amd_unpack3(pV),r2f.s2,r2f.s3);//first row, even pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s1 = amd_pack15(rgbx.s0,rgbx.s1); pRGB1.s2 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack2(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB3.s1 = amd_pack15(rgbx.s0,rgbx.s1); pRGB3.s2 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		yuv.s0 = mad(amd_unpack3(pY0.s1),r2f.s0,r2f.s1);//first row, odd pixel\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB1.s2 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s3 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		yuv.s0 = mad(amd_unpack3(pY1.s1),r2f.s0,r2f.s1);//second row\n"
		"		rgbx.s0 = mad(cR.s1, yuv.s2, yuv.s0); rgbx.s1 = mad(cG.s0, yuv.s1, yuv.s0); rgbx.s1 = mad(cG.s1, yuv.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, yuv.s1, yuv.s0);\n"
		"		pRGB3.s2 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB3.s3 = amd_pack15(rgbx.s1,rgbx.s2);\n";
	return output;
}
std::string ConvertV210toRGB2(){
	std::string output =
		"		uint8 pRGB0;\n"
		"		uint4 pRGB1;\n"
		"		float4 rgbx, uyvy; float3 input; rgbx.s3 = 0.0f; \n"
		"		//Line[0]\n"
		"		input = amd_unpack10(L0.s0); uyvy.s02 = mad(input.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s1 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s1);                                                          uyvy.s3 = mad(input.s0,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s0 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s1 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s1 = mad(input.s2,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s2); uyvy.s2 = mad(input.s0,(float )r2f.s2,(float )r2f.s3); uyvy.s3 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s2 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s3 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s2,(float )r2f.s2,(float )r2f.s3);\n"
		"		input = amd_unpack10(L0.s3); uyvy.s2 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s13 = mad(input.s02,(float2)r2f.s0,(float2)r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s4 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s5 = amd_pack(rgbx);\n"
		"		input = amd_unpack10(L0.s4); uyvy.s02 = mad(input.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s1 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s5);                                                          uyvy.s3 = mad(input.s0,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s6 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s7 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s1 = mad(input.s2,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s6); uyvy.s2 = mad(input.s0,(float )r2f.s2,(float )r2f.s3); uyvy.s3 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s0 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s1 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s2,(float )r2f.s2,(float )r2f.s3);\n"
		"		input = amd_unpack10(L0.s7); uyvy.s2 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s13 = mad(input.s02,(float2)r2f.s0,(float2)r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s2 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s3 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertV210toRGBX(){
	std::string output =
		"		uint8 pRGB0;\n"
		"		uint4 pRGB1;\n"
		"		float4 rgbx, uyvy; float3 input;\n"
		"		//Line[0]\n"
		"		input = amd_unpack10(L0.s0); uyvy.s02 = mad(input.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s1 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s1);                                                          uyvy.s3 = mad(input.s0,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s0 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s1 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s1 = mad(input.s2,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s2); uyvy.s2 = mad(input.s0,(float )r2f.s2,(float )r2f.s3); uyvy.s3 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s2 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s3 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s2,(float )r2f.s2,(float )r2f.s3);\n"
		"		input = amd_unpack10(L0.s3); uyvy.s2 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s13 = mad(input.s02,(float2)r2f.s0,(float2)r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s4 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s5 = amd_pack(rgbx);\n"
		"		input = amd_unpack10(L0.s4); uyvy.s02 = mad(input.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s1 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s5);                                                          uyvy.s3 = mad(input.s0,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s6 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s7 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s1 = mad(input.s2,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s6); uyvy.s2 = mad(input.s0,(float )r2f.s2,(float )r2f.s3); uyvy.s3 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB1.s0 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB1.s1 = amd_pack(rgbx);\n"
		"		                             uyvy.s0 = mad(input.s2,(float )r2f.s2,(float )r2f.s3);\n"
		"		input = amd_unpack10(L0.s7); uyvy.s2 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s13 = mad(input.s02,(float2)r2f.s0,(float2)r2f.s1);"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB1.s2 = amd_pack(rgbx);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB1.s3 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertV210toRGB4(){
	std::string output =
		"		uint8 pRGB0, pRGB1;\n"
		"		uint2 pRGB2;\n"
		"		float4 rgbx, uyvy; float3 input; rgbx.s3 = 0.0f; \n"
		"		//Line[0]\n"
		"		input = amd_unpack10(L0.s0); uyvy.s02 = mad(input.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s1 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s1);                                                          uyvy.s3 = mad(input.s0,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s0 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s1 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s1 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s2 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		                             uyvy.s0 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s1 = mad(input.s2,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s2); uyvy.s2 = mad(input.s0,(float )r2f.s2,(float )r2f.s3); uyvy.s3 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s3 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s4 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s4 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s5 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		                             uyvy.s0 = mad(input.s2,(float )r2f.s2,(float )r2f.s3);\n"
		"		input = amd_unpack10(L0.s3); uyvy.s2 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s13 = mad(input.s02,(float2)r2f.s0,(float2)r2f.s1);"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s6 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s7 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s7 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s0 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		input = amd_unpack10(L0.s4); uyvy.s02 = mad(input.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s1 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s5);                                                          uyvy.s3 = mad(input.s0,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s1 = amd_pack15(rgbx.s0,rgbx.s1); pRGB1.s2 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s2 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s3 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		                             uyvy.s0 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s1 = mad(input.s2,(float )r2f.s0,(float )r2f.s1);\n"
		"		input = amd_unpack10(L0.s6); uyvy.s2 = mad(input.s0,(float )r2f.s2,(float )r2f.s3); uyvy.s3 = mad(input.s1,(float )r2f.s0,(float )r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s4 = amd_pack15(rgbx.s0,rgbx.s1); pRGB1.s5 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s5 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s6 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"		                             uyvy.s0 = mad(input.s2,(float )r2f.s2,(float )r2f.s3);\n"
		"		input = amd_unpack10(L0.s7); uyvy.s2 = mad(input.s1,(float )r2f.s2,(float )r2f.s3); uyvy.s13 = mad(input.s02,(float2)r2f.s0,(float2)r2f.s1);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s7 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s0 = (uint)clamp(rgbx.s2,0.0f,32767.0f);\n"
		"		rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB2.s0 += (((uint)clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB2.s1 = amd_pack15(rgbx.s1,rgbx.s2);\n";
	return output;
}
std::string ConvertV216toRGB2(){
	std::string output =
		"    uint8 pRGB0, pRGB1;\n"
		"    float4 rgbx, uyvy; rgbx.s3 = 0.0f; \n"
		"    uyvy = amd_unpack16(L0.s0, L0.s1); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s1 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L0.s2, L0.s3); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s3 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L0.s4, L0.s5); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s5 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L0.s6, L0.s7); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s7 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s0, L1.s1); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s1 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s2, L1.s3); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s3 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s4, L1.s5); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s5 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s6, L1.s7); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertV216toRGBX(){
	std::string output =
		"    uint8 pRGB0, pRGB1;\n"
		"    float4 rgbx, uyvy; \n"
		"    uyvy = amd_unpack16(L0.s0, L0.s1); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s1 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L0.s2, L0.s3); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s3 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L0.s4, L0.s5); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s5 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L0.s6, L0.s7); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB0.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB0.s7 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s0, L1.s1); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB1.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB1.s1 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s2, L1.s3); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB1.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB1.s3 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s4, L1.s5); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB1.s4 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB1.s5 = amd_pack(rgbx);\n"
		"    uyvy = amd_unpack16(L1.s6, L1.s7); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); rgbx.s3 = uyvy.s1; pRGB1.s6 = amd_pack(rgbx);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); rgbx.s3 = uyvy.s3; pRGB1.s7 = amd_pack(rgbx);\n";
	return output;
}
std::string ConvertV216toRGB4(){
	std::string output =
		"    uint8 pRGB0, pRGB2;\n"
		"    uint4 pRGB1, pRGB3;\n"
		"    float4 rgbx; float4 uyvy; \n"
		"    uyvy = amd_unpack16(L0.s0, L0.s1); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s0 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s1 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s1 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s2 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"    uyvy = amd_unpack16(L0.s2, L0.s3); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s3 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s4 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s4 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB0.s5 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"    uyvy = amd_unpack16(L0.s4, L0.s5); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB0.s6 = amd_pack15(rgbx.s0,rgbx.s1); pRGB0.s7 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB0.s7 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s0 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"    uyvy = amd_unpack16(L0.s6, L0.s7); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB1.s1 = amd_pack15(rgbx.s0,rgbx.s1); pRGB1.s2 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB1.s2 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB1.s3 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"    uyvy = amd_unpack16(L1.s0, L1.s1); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB2.s0 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s1 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB2.s1 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB2.s2 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"    uyvy = amd_unpack16(L1.s2, L1.s3); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB2.s3 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s4 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB2.s4 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB2.s5 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"    uyvy = amd_unpack16(L1.s4, L1.s5); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB2.s6 = amd_pack15(rgbx.s0,rgbx.s1); pRGB2.s7 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB2.s7 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB3.s0 = amd_pack15(rgbx.s1,rgbx.s2);\n"
		"    uyvy = amd_unpack16(L1.s6, L1.s7); \n"
		"    uyvy.s02 = mad(uyvy.s02,(float2)r2f.s2,(float2)r2f.s3); uyvy.s13 = mad(uyvy.s13,(float2)r2f.s0,(float2)r2f.s1);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s1); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s1); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s1); pRGB3.s1 = amd_pack15(rgbx.s0,rgbx.s1); pRGB3.s2 = pRGB3.s2 = (uint) clamp(rgbx.s2,0.0f,32767.0f);\n"
		"    rgbx.s0 = mad(cR.s1, uyvy.s2, uyvy.s3); rgbx.s1 = mad(cG.s0, uyvy.s0, uyvy.s3); rgbx.s1 = mad(cG.s1, uyvy.s2, rgbx.s1); rgbx.s2 = mad(cB.s0, uyvy.s0, uyvy.s3); pRGB3.s2 += (((uint) clamp(rgbx.s0,0.0f,32767.0f))<<16); pRGB3.s3 = amd_pack15(rgbx.s1,rgbx.s2);\n";
	return output;
}
std::string ConvertRGB2toUYVY(){
	std::string output =
		"    uint8 pUYVY;"
		"    float4 f; float3 rgb;\n"
		"    rgb = (float3)(amd_unpack0(L0.s0), amd_unpack1(L0.s0), amd_unpack2(L0.s0));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L0.s0), amd_unpack0(L0.s1), amd_unpack1(L0.s1))) + cY.s3; pUYVY.s0 = amd_pack(f);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = (float3)(amd_unpack2(L0.s1), amd_unpack3(L0.s1), amd_unpack0(L0.s2));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s2), amd_unpack2(L0.s2), amd_unpack3(L0.s2))) + cY.s3; pUYVY.s1 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack0(L0.s3), amd_unpack1(L0.s3), amd_unpack2(L0.s3));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L0.s3), amd_unpack0(L0.s4), amd_unpack1(L0.s4))) + cY.s3; pUYVY.s2 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack2(L0.s4), amd_unpack3(L0.s4), amd_unpack0(L0.s5));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s5), amd_unpack2(L0.s5), amd_unpack3(L0.s5))) + cY.s3; pUYVY.s3 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack0(L1.s0), amd_unpack1(L1.s0), amd_unpack2(L1.s0));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L1.s0), amd_unpack0(L1.s1), amd_unpack1(L1.s1))) + cY.s3; pUYVY.s4 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack2(L1.s1), amd_unpack3(L1.s1), amd_unpack0(L1.s2));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s2), amd_unpack2(L1.s2), amd_unpack3(L1.s2))) + cY.s3; pUYVY.s5 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack0(L1.s3), amd_unpack1(L1.s3), amd_unpack2(L1.s3));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L1.s3), amd_unpack0(L1.s4), amd_unpack1(L1.s4))) + cY.s3; pUYVY.s6 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack2(L1.s4), amd_unpack3(L1.s4), amd_unpack0(L1.s5));\n"
		"    f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s5), amd_unpack2(L1.s5), amd_unpack3(L1.s5))) + cY.s3; pUYVY.s7 = amd_pack(f);\n";
	return output;
}
std::string ConvertRGB2toYUYV(){
	std::string output =
		"    uint8 pUYVY;"
		"    float4 f; float3 rgb;\n"
		"    rgb = (float3)(amd_unpack0(L0.s0), amd_unpack1(L0.s0), amd_unpack2(L0.s0));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack3(L0.s0), amd_unpack0(L0.s1), amd_unpack1(L0.s1))) + cY.s3; pUYVY.s0 = amd_pack(f);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = (float3)(amd_unpack2(L0.s1), amd_unpack3(L0.s1), amd_unpack0(L0.s2));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack1(L0.s2), amd_unpack2(L0.s2), amd_unpack3(L0.s2))) + cY.s3; pUYVY.s1 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack0(L0.s3), amd_unpack1(L0.s3), amd_unpack2(L0.s3));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack3(L0.s3), amd_unpack0(L0.s4), amd_unpack1(L0.s4))) + cY.s3; pUYVY.s2 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack2(L0.s4), amd_unpack3(L0.s4), amd_unpack0(L0.s5));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack1(L0.s5), amd_unpack2(L0.s5), amd_unpack3(L0.s5))) + cY.s3; pUYVY.s3 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack0(L1.s0), amd_unpack1(L1.s0), amd_unpack2(L1.s0));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack3(L1.s0), amd_unpack0(L1.s1), amd_unpack1(L1.s1))) + cY.s3; pUYVY.s4 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack2(L1.s1), amd_unpack3(L1.s1), amd_unpack0(L1.s2));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack1(L1.s2), amd_unpack2(L1.s2), amd_unpack3(L1.s2))) + cY.s3; pUYVY.s5 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack0(L1.s3), amd_unpack1(L1.s3), amd_unpack2(L1.s3));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack3(L1.s3), amd_unpack0(L1.s4), amd_unpack1(L1.s4))) + cY.s3; pUYVY.s6 = amd_pack(f);\n"
		"    rgb = (float3)(amd_unpack2(L1.s4), amd_unpack3(L1.s4), amd_unpack0(L1.s5));\n"
		"    f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack1(L1.s5), amd_unpack2(L1.s5), amd_unpack3(L1.s5))) + cY.s3; pUYVY.s7 = amd_pack(f);\n";
	return output;
}
std::string ConvertRGB2toNV12(){
	std::string output =
		"    uint2 pY0, pY1, pUV;\n"
		"    float4 y0, y1, uv; float3 rgb;\n"
		"    //pixel[0]\n"
		"    rgb = (float3)(amd_unpack0(L0.s0), amd_unpack1(L0.s0), amd_unpack2(L0.s0));\n"
		"    uv.s0 = dot(cU, rgb); y0.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = dot(cV, rgb); \n"
		"    y0.s1 = dot(cY.s012, (float3)(amd_unpack3(L0.s0), amd_unpack0(L0.s1), amd_unpack1(L0.s1))) + cY.s3;\n" // As in MPEG2: U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, but take the mean of two lines: see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = (float3)(amd_unpack0(L1.s0), amd_unpack1(L1.s0), amd_unpack2(L1.s0));\n"
		"    uv.s0 = (dot(cU, rgb)+uv.s0)*0.5 + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = (dot(cV, rgb)+uv.s1)*0.5 + 128.0f;\n"
		"    y1.s1 = dot(cY.s012, (float3)(amd_unpack3(L1.s0), amd_unpack0(L1.s1), amd_unpack1(L1.s1))) + cY.s3;\n"
		"    //pixel[2]\n"
		"    rgb = (float3)(amd_unpack2(L0.s1), amd_unpack3(L0.s1), amd_unpack0(L0.s2));\n"
		"    uv.s2 = dot(cU, rgb); y0.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = dot(cV, rgb);\n"
		"    y0.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s2), amd_unpack2(L0.s2), amd_unpack3(L0.s2))) + cY.s3;\n"
		"    rgb = (float3)(amd_unpack2(L1.s1), amd_unpack3(L1.s1), amd_unpack0(L1.s2));\n"
		"    uv.s2 = (dot(cU, rgb)+uv.s2)*0.5 + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = (dot(cV, rgb)+uv.s3)*0.5 + 128.0f; \n"
		"    y1.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s2), amd_unpack2(L1.s2), amd_unpack3(L1.s2))) + cY.s3;\n"
		"    pY0.s0 = amd_pack(y0);  pY1.s0 = amd_pack(y1); pUV.s0 = amd_pack(uv);\n"
		"    //pixel[4]\n"
		"    rgb = (float3)(amd_unpack0(L0.s3), amd_unpack1(L0.s3), amd_unpack2(L0.s3));\n"
		"    uv.s0 = dot(cU, rgb); y0.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = dot(cV, rgb);\n"
		"    y0.s1 = dot(cY.s012, (float3)(amd_unpack3(L0.s3), amd_unpack0(L0.s4), amd_unpack1(L0.s4))) + cY.s3;\n"
		"    rgb = (float3)(amd_unpack0(L1.s3), amd_unpack1(L1.s3), amd_unpack2(L1.s3));\n"
		"    uv.s0 = (dot(cU, rgb)+uv.s0)*0.5 + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = (dot(cV, rgb)+uv.s1)*0.5 + 128.0f;\n"
		"    y1.s1 = dot(cY.s012, (float3)(amd_unpack3(L1.s3), amd_unpack0(L1.s4), amd_unpack1(L1.s4))) + cY.s3;\n"
		"    //pixel[6]\n"
		"    rgb = (float3)(amd_unpack2(L0.s4), amd_unpack3(L0.s4), amd_unpack0(L0.s5));\n"
		"    uv.s2 = dot(cU, rgb); y0.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = dot(cV, rgb);\n"
		"    y0.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s5), amd_unpack2(L0.s5), amd_unpack3(L0.s5))) + cY.s3;\n"
		"    rgb = (float3)(amd_unpack2(L1.s4), amd_unpack3(L1.s4), amd_unpack0(L1.s5));\n"
		"    uv.s2 = (dot(cU, rgb)+uv.s2)*0.5 + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = (dot(cV, rgb)+uv.s3)*0.5 + 128.0f; \n"
		"    y1.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s5), amd_unpack2(L1.s5), amd_unpack3(L1.s5))) + cY.s3;\n"
		"    pY0.s1 = amd_pack(y0);  pY1.s1 = amd_pack(y1); pUV.s1 = amd_pack(uv);\n";
	return output;
}
std::string ConvertRGB2toIYUV(){
	std::string output =
		"    uint2 pY0, pY1; uint pU, pV;\n"
		"    float4 y0, y1, u, v; float3 rgb;\n"
		"    //pixel[0]\n"
		"    rgb = (float3)(amd_unpack0(L0.s0), amd_unpack1(L0.s0), amd_unpack2(L0.s0));\n"
		"    u.s0 = dot(cU, rgb); y0.s0 = dot(cY.s012, rgb) + cY.s3; v.s0 = dot(cV, rgb); \n"
		"    y0.s1 = dot(cY.s012, (float3)(amd_unpack3(L0.s0), amd_unpack0(L0.s1), amd_unpack1(L0.s1)));\n" // As in MPEG2: U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, but take the mean of two lines: see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = (float3)(amd_unpack0(L1.s0), amd_unpack1(L1.s0), amd_unpack2(L1.s0)) + cY.s3;\n"
		"    u.s0 = (dot(cU, rgb)+u.s0)*0.5 + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; v.s0 = (dot(cV, rgb)+v.s0)*0.5 + 128.0f;\n"
		"    y1.s1 = dot(cY.s012, (float3)(amd_unpack3(L1.s0), amd_unpack0(L1.s1), amd_unpack1(L1.s1))) + cY.s3;\n"
		"    //pixel[2]\n"
		"    rgb = (float3)(amd_unpack2(L0.s1), amd_unpack3(L0.s1), amd_unpack0(L0.s2));\n"
		"    u.s1 = dot(cU, rgb); y0.s2 = dot(cY.s012, rgb) + cY.s3; v.s1 = dot(cV, rgb);\n"
		"    y0.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s2), amd_unpack2(L0.s2), amd_unpack3(L0.s2))) + cY.s3;\n"
		"    rgb = (float3)(amd_unpack2(L1.s1), amd_unpack3(L1.s1), amd_unpack0(L1.s2)) + cY.s3;\n"
		"    u.s1 = (dot(cU, rgb)+u.s1)*0.5 + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; v.s1 = (dot(cV, rgb)+v.s1)*0.5 + 128.0f; \n"
		"    y1.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s2), amd_unpack2(L1.s2), amd_unpack3(L1.s2))) + cY.s3;\n"
		"    pY0.s0 = amd_pack(y0);  pY1.s0 = amd_pack(y1); \n"
		"    //pixel[4]\n"
		"    rgb = (float3)(amd_unpack0(L0.s3), amd_unpack1(L0.s3), amd_unpack2(L0.s3));\n"
		"    u.s2 = dot(cU, rgb); y0.s0 = dot(cY.s012, rgb) + cY.s3; v.s2 = dot(cV, rgb);\n"
		"    y0.s1 = dot(cY.s012, (float3)(amd_unpack3(L0.s3), amd_unpack0(L0.s4), amd_unpack1(L0.s4))) + cY.s3;\n"
		"    rgb = (float3)(amd_unpack0(L1.s3), amd_unpack1(L1.s3), amd_unpack2(L1.s3)) + cY.s3;\n"
		"    u.s2 = (dot(cU, rgb)+u.s2)*0.5 + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; v.s2 = (dot(cV, rgb)+v.s2)*0.5 + 128.0f;\n"
		"    y1.s1 = dot(cY.s012, (float3)(amd_unpack3(L1.s3), amd_unpack0(L1.s4), amd_unpack1(L1.s4))) + cY.s3;\n"
		"    //pixel[6]\n"
		"    rgb = (float3)(amd_unpack2(L0.s4), amd_unpack3(L0.s4), amd_unpack0(L0.s5));\n"
		"    u.s3 = dot(cU, rgb); y0.s2 = dot(cY.s012, rgb) + cY.s3; v.s3 = dot(cV, rgb);\n"
		"    y0.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s5), amd_unpack2(L0.s5), amd_unpack3(L0.s5))) + cY.s3;\n"
		"    rgb = (float3)(amd_unpack2(L1.s4), amd_unpack3(L1.s4), amd_unpack0(L1.s5));\n"
		"    u.s3 = (dot(cU, rgb)+u.s3)*0.5 + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; v.s3 = (dot(cV, rgb)+v.s3)*0.5 + 128.0f; \n"
		"    y1.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s5), amd_unpack2(L1.s5), amd_unpack3(L1.s5))) + cY.s3;\n"
		"    pY0.s1 = amd_pack(y0);  pY1.s1 = amd_pack(y1); pV = amd_pack(v); pU = amd_pack(u);\n";
	return output;
}
std::string ConvertRGB2toV216(){
	std::string output = // RGB2 > V216, for description of V216 see: https://developer.apple.com/library/content/technotes/tn2162/_index.html#//
		"    uint8 pUYVY0, pUYVY1;"
		"    float4 f; float3 rgb;\n"
		"    rgb = (float3)(amd_unpack0(L0.s0), amd_unpack1(L0.s0), amd_unpack2(L0.s0));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L0.s0), amd_unpack0(L0.s1), amd_unpack1(L0.s1))) + cY.s3; pUYVY0.s0 = amd_pack16(f.s0,f.s1); pUYVY0.s1 = amd_pack16(f.s2,f.s3);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = (float3)(amd_unpack2(L0.s1), amd_unpack3(L0.s1), amd_unpack0(L0.s2));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s2), amd_unpack2(L0.s2), amd_unpack3(L0.s2))) + cY.s3; pUYVY0.s2 = amd_pack16(f.s0,f.s1); pUYVY0.s3 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = (float3)(amd_unpack0(L0.s3), amd_unpack1(L0.s3), amd_unpack2(L0.s3));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L0.s3), amd_unpack0(L0.s4), amd_unpack1(L0.s4))) + cY.s3; pUYVY0.s4 = amd_pack16(f.s0,f.s1); pUYVY0.s5 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = (float3)(amd_unpack2(L0.s4), amd_unpack3(L0.s4), amd_unpack0(L0.s5));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L0.s5), amd_unpack2(L0.s5), amd_unpack3(L0.s5))) + cY.s3; pUYVY0.s6 = amd_pack16(f.s0,f.s1); pUYVY0.s7 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = (float3)(amd_unpack0(L1.s0), amd_unpack1(L1.s0), amd_unpack2(L1.s0));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L1.s0), amd_unpack0(L1.s1), amd_unpack1(L1.s1))) + cY.s3; pUYVY1.s0 = amd_pack16(f.s0,f.s1); pUYVY1.s1 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = (float3)(amd_unpack2(L1.s1), amd_unpack3(L1.s1), amd_unpack0(L1.s2));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s2), amd_unpack2(L1.s2), amd_unpack3(L1.s2))) + cY.s3; pUYVY1.s2 = amd_pack16(f.s0,f.s1); pUYVY1.s3 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = (float3)(amd_unpack0(L1.s3), amd_unpack1(L1.s3), amd_unpack2(L1.s3));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack3(L1.s3), amd_unpack0(L1.s4), amd_unpack1(L1.s4))) + cY.s3; pUYVY1.s4 = amd_pack16(f.s0,f.s1); pUYVY1.s5 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = (float3)(amd_unpack2(L1.s4), amd_unpack3(L1.s4), amd_unpack0(L1.s5));\n"
		"    f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, (float3)(amd_unpack1(L1.s5), amd_unpack2(L1.s5), amd_unpack3(L1.s5))) + cY.s3; pUYVY1.s6 = amd_pack16(f.s0,f.s1); pUYVY1.s7 = amd_pack16(f.s2,f.s3);\n";
	return output;
}
std::string ReadAndConvertRGB2toV210(){ // RGB2 > V210, for description of V210 see: https://developer.apple.com/library/content/technotes/tn2162/_index.html#//
	std::string output =
		"		uint8 L0;\n"
		"		uint  L1;\n"
		"		pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx * 36);\n"
		"		L0 = *(__global uint8 *) pRGB_buf;\n"
		"		L1 = *(__global uint  *)&pRGB_buf[32];\n"
		"		uint8 pUYVY0;"
		"		float3 f; float3 rgb;\n"
		"		//Line[0]\n"
		"		rgb = (float3)(amd_unpack0(L0.s0), amd_unpack1(L0.s0), amd_unpack2(L0.s0));\n"
		"		f.s0 = dot(cU, rgb) + 512.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 512.0f;                                                                                                    pUYVY0.s0 = amd_pack10(f.s0,f.s1,f.s2);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"		                                                                                              f.s0 = dot(cY.s012, (float3)(amd_unpack3(L0.s0), amd_unpack0(L0.s1), amd_unpack1(L0.s1))) + cY.s3;                                       ;\n"
		"		rgb = (float3)(amd_unpack2(L0.s1), amd_unpack3(L0.s1), amd_unpack0(L0.s2));                                                                                                                                                             \n"
		"		f.s1 = dot(cU, rgb) + 512.0f; f.s2 = dot(cY.s012, rgb) + cY.s3;                                                                                                                                  pUYVY0.s1 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                                f.s0 = dot(cV, rgb) + 512.0f; f.s1 = dot(cY.s012, (float3)(amd_unpack1(L0.s2), amd_unpack2(L0.s2), amd_unpack3(L0.s2))) + cY.s3;                                       ;\n"
		"		rgb = (float3)(amd_unpack0(L0.s3), amd_unpack1(L0.s3), amd_unpack2(L0.s3));                                                                                                                                                             \n"
		"		f.s2 = dot(cU, rgb) + 512.0f;                                                                                                                                                                    pUYVY0.s2 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                              f.s0 = dot(cY.s012, rgb) + cY.s3; f.s1 = dot(cV, rgb) + 512.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack3(L0.s3), amd_unpack0(L0.s4), amd_unpack1(L0.s4))) + cY.s3; pUYVY0.s3 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		rgb = (float3)(amd_unpack2(L0.s4), amd_unpack3(L0.s4), amd_unpack0(L0.s5));                                                                                                                                                             \n"
		"		f.s0 = dot(cU, rgb) + 512.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 512.0f;                                                                                                    pUYVY0.s4 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                                                              f.s0 = dot(cY.s012, (float3)(amd_unpack1(L0.s5), amd_unpack2(L0.s5), amd_unpack3(L0.s5))) + cY.s3;                                       ;\n"
		"		rgb = (float3)(amd_unpack0(L0.s6), amd_unpack1(L0.s6), amd_unpack2(L0.s6));                                                                                                                                                             \n"
		"		f.s1 = dot(cU, rgb) + 512.0f; f.s2 = dot(cY.s012, rgb) + cY.s3;                                                                                                                                  pUYVY0.s5 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                                f.s0 = dot(cV, rgb) + 512.0f; f.s1 = dot(cY.s012, (float3)(amd_unpack3(L0.s6), amd_unpack0(L0.s7), amd_unpack1(L0.s7))) + cY.s3;                                       ;\n"
		"		rgb = (float3)(amd_unpack2(L0.s7), amd_unpack3(L0.s7), amd_unpack0(L1));                                                                                                                                                                \n"
		"		f.s2 = dot(cU, rgb) + 512.0f;                                                                                                                                                                    pUYVY0.s6 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                              f.s0 = dot(cY.s012, rgb) + cY.s3; f.s1 = dot(cV, rgb) + 512.0f; f.s2 = dot(cY.s012, (float3)(amd_unpack1(L1), amd_unpack2(L1), amd_unpack3(L1)))          + cY.s3; pUYVY0.s7 = amd_pack10(f.s0,f.s1,f.s2);\n";
	return output;
}
std::string ConvertRGB4toUYVY(){
	std::string output =
		"    uint8 pUYVY;"
		"    float4 f; float3 rgb;\n"
		"    rgb = amd_unpackA(L0.s0,L0.s1); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L0.s1,L0.s2)) + cY.s3; pUYVY.s0 = amd_pack(f);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = amd_unpackA(L0.s3,L0.s4); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L0.s4,L0.s5)) + cY.s3; pUYVY.s1 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L0.s6,L0.s7); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L0.s7,L1.s0)) + cY.s3; pUYVY.s2 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L1.s1,L1.s2); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L1.s2,L1.s3)) + cY.s3; pUYVY.s3 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L2.s0,L2.s1); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L2.s1,L2.s2)) + cY.s3; pUYVY.s4 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L2.s3,L2.s4); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L2.s4,L2.s5)) + cY.s3; pUYVY.s5 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L2.s6,L2.s7); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L2.s7,L3.s0)) + cY.s3; pUYVY.s6 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L3.s1,L3.s2); f.s0 = dot(cU, rgb) + 128.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 128.0f; f.s3 = dot(cY.s012, amd_unpackB(L3.s2,L3.s3)) + cY.s3; pUYVY.s7 = amd_pack(f);\n";
	return output;
}
std::string ConvertRGB4toYUYV(){
	std::string output =
		"    uint8 pUYVY;"
		"    float4 f; float3 rgb;\n"
		"    rgb = amd_unpackA(L0.s0,L0.s1); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L0.s1,L0.s2)) + cY.s3; pUYVY.s0 = amd_pack(f);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = amd_unpackA(L0.s3,L0.s4); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L0.s4,L0.s5)) + cY.s3; pUYVY.s1 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L0.s6,L0.s7); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L0.s7,L1.s0)) + cY.s3; pUYVY.s2 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L1.s1,L1.s2); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L1.s2,L1.s3)) + cY.s3; pUYVY.s3 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L2.s0,L2.s1); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L2.s1,L2.s2)) + cY.s3; pUYVY.s4 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L2.s3,L2.s4); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L2.s4,L2.s5)) + cY.s3; pUYVY.s5 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L2.s6,L2.s7); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L2.s7,L3.s0)) + cY.s3; pUYVY.s6 = amd_pack(f);\n"
		"    rgb = amd_unpackA(L3.s1,L3.s2); f.s1 = dot(cU, rgb) + 128.0f; f.s0 = dot(cY.s012, rgb) + cY.s3; f.s3 = dot(cV, rgb) + 128.0f; f.s2 = dot(cY.s012, amd_unpackB(L3.s2,L3.s3)) + cY.s3; pUYVY.s7 = amd_pack(f);\n";
	return output;
}
std::string ConvertRGB4toNV12(){
	std::string output =
		"    uint2 pY0, pY1, pUV;\n"
		"    float4 y0, y1, uv; float3 rgb;\n"
		"    //pixel[0]\n"
		"    rgb = amd_unpackA(L0.s0,L0.s1); uv.s0 = dot(cU, rgb);                  y0.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = dot(cV, rgb);                  y0.s1 = dot(cY.s012, amd_unpackB(L0.s1,L0.s2)) + cY.s3;\n" // As in MPEG2: U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, but take the mean of two lines: see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = amd_unpackA(L2.s0,L2.s1); uv.s0 = (dot(cU, rgb)+uv.s0) + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = (dot(cV, rgb)+uv.s1) + 128.0f; y1.s1 = dot(cY.s012, amd_unpackB(L2.s1,L2.s2)) + cY.s3;\n"
		"    //pixel[2]\n"
		"    rgb = amd_unpackA(L0.s3,L0.s4); uv.s2 = dot(cU, rgb);                  y0.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = dot(cV, rgb);                  y0.s3 = dot(cY.s012, amd_unpackB(L0.s4,L0.s5)) + cY.s3;\n"
		"    rgb = amd_unpackA(L2.s3,L2.s4); uv.s2 = (dot(cU, rgb)+uv.s2) + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = (dot(cV, rgb)+uv.s3) + 128.0f; y1.s3 = dot(cY.s012, amd_unpackB(L2.s4,L2.s5)) + cY.s3;\n"
		"    pY0.s0 = amd_pack(y0); pY1.s0 = amd_pack(y1); pUV.s0 = amd_pack(uv);\n"
		"    //pixel[4]\n"
		"    rgb = amd_unpackA(L0.s6,L0.s7); uv.s0 = dot(cU, rgb);                  y0.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = dot(cV, rgb);                  y0.s1 = dot(cY.s012, amd_unpackB(L0.s7,L1.s0)) + cY.s3;\n"
		"    rgb = amd_unpackA(L2.s6,L2.s7); uv.s0 = (dot(cU, rgb)+uv.s0) + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; uv.s1 = (dot(cV, rgb)+uv.s1) + 128.0f; y1.s1 = dot(cY.s012, amd_unpackB(L2.s7,L3.s0)) + cY.s3;\n"
		"    //pixel[6]\n"
		"    rgb = amd_unpackA(L1.s1,L1.s2); uv.s2 = dot(cU, rgb);                  y0.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = dot(cV, rgb);                  y0.s3 = dot(cY.s012, amd_unpackB(L1.s2,L1.s3)) + cY.s3;\n"
		"    rgb = amd_unpackA(L3.s1,L3.s2); uv.s2 = (dot(cU, rgb)+uv.s2) + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; uv.s3 = (dot(cV, rgb)+uv.s3) + 128.0f; y1.s3 = dot(cY.s012, amd_unpackB(L3.s2,L3.s3)) + cY.s3;\n"
		"    pY0.s1 = amd_pack(y0); pY1.s1 = amd_pack(y1); pUV.s1 = amd_pack(uv);\n";
	return output;
}
std::string ConvertRGB4toIYUV(){
	std::string output =
		"    uint2 pY0, pY1; uint pU, pV;\n"
		"    float4 y0, y1, u, v; float3 rgb;\n"
		"    //pixel[0]\n"
		"    rgb = amd_unpackA(L0.s0,L0.s1); u.s0 = dot(cU, rgb);                 y0.s0 = dot(cY.s012, rgb) + cY.s3; v.s0 = dot(cV, rgb);                 y0.s1 = dot(cY.s012, amd_unpackB(L0.s1,L0.s2)) + cY.s3;\n" // As in MPEG2: U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, but take the mean of two lines: see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = amd_unpackA(L2.s0,L2.s1); u.s0 = (dot(cU, rgb)+u.s0)*0.5 + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; v.s0 = (dot(cV, rgb)+v.s0)*0.5 + 128.0f; y1.s1 = dot(cY.s012, amd_unpackB(L2.s1,L2.s2)) + cY.s3;\n"
		"    //pixel[2]\n"
		"    rgb = amd_unpackA(L0.s3,L0.s4); u.s1 = dot(cU, rgb);                 y0.s2 = dot(cY.s012, rgb) + cY.s3; v.s2 = dot(cV, rgb);                 y0.s3 = dot(cY.s012, amd_unpackB(L0.s4,L0.s5)) + cY.s3;\n"
		"    rgb = amd_unpackA(L2.s3,L2.s4); u.s1 = (dot(cU, rgb)+u.s1)*0.5 + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; v.s1 = (dot(cV, rgb)+v.s1)*0.5 + 128.0f; y1.s3 = dot(cY.s012, amd_unpackB(L2.s4,L2.s5)) + cY.s3;\n"
		"    pY0.s0 = amd_pack(y0); pY1.s0 = amd_pack(y1);\n"
		"    //pixel[4]\n"
		"    rgb = amd_unpackA(L0.s6,L0.s7); u.s2 = dot(cU, rgb);                 y0.s0 = dot(cY.s012, rgb) + cY.s3; v.s2 = dot(cV, rgb);                 y0.s1 = dot(cY.s012, amd_unpackB(L0.s7,L1.s0)) + cY.s3;\n"
		"    rgb = amd_unpackA(L2.s6,L2.s7); u.s2 = (dot(cU, rgb)+u.s2)*0.5 + 128.0f; y1.s0 = dot(cY.s012, rgb) + cY.s3; v.s2 = (dot(cV, rgb)+v.s2)*0.5 + 128.0f; y1.s1 = dot(cY.s012, amd_unpackB(L2.s7,L3.s0)) + cY.s3;\n"
		"    //pixel[6]\n"
		"    rgb = amd_unpackA(L1.s1,L1.s2); u.s3 = dot(cU, rgb);                 y0.s2 = dot(cY.s012, rgb) + cY.s3; v.s3 = dot(cV, rgb);                  y0.s3 = dot(cY.s012, amd_unpackB(L1.s2,L1.s3)) + cY.s3;\n"
		"    rgb = amd_unpackA(L3.s1,L3.s2); u.s3 = (dot(cU, rgb)+u.s3)*0.5 + 128.0f; y1.s2 = dot(cY.s012, rgb) + cY.s3; v.s3 = (dot(cV, rgb)+v.s3)*0.5 + 128.0f;  y1.s3 = dot(cY.s012, amd_unpackB(L3.s2,L3.s3)) + cY.s3;\n"
		"    pY0.s1 = amd_pack(y0); pY1.s1 = amd_pack(y1); pU = amd_pack(u); pV = amd_pack(v);\n";
	return output;
}
std::string ConvertRGB4toV216(){
	std::string output = //RGB4 > V216, for description of V216 see: https://developer.apple.com/library/content/technotes/tn2162/_index.html#//
		"    uint8 pUYVY0, pUYVY1;"
		"    float4 f; float3 rgb;\n"
		"    rgb = amd_unpackA(L0.s0,L0.s1); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L0.s1,L0.s2)) + cY.s3; pUYVY0.s0 = amd_pack16(f.s0,f.s1); pUYVY0.s1 = amd_pack16(f.s2,f.s3);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"    rgb = amd_unpackA(L0.s3,L0.s4); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L0.s4,L0.s5)) + cY.s3; pUYVY0.s2 = amd_pack16(f.s0,f.s1); pUYVY0.s3 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = amd_unpackA(L0.s6,L0.s7); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L0.s7,L1.s0)) + cY.s3; pUYVY0.s4 = amd_pack16(f.s0,f.s1); pUYVY0.s5 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = amd_unpackA(L1.s1,L1.s2); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L1.s2,L1.s3)) + cY.s3; pUYVY0.s6 = amd_pack16(f.s0,f.s1); pUYVY0.s7 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = amd_unpackA(L2.s0,L2.s1); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L2.s1,L2.s2)) + cY.s3; pUYVY1.s0 = amd_pack16(f.s0,f.s1); pUYVY1.s1 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = amd_unpackA(L2.s3,L2.s4); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L2.s4,L2.s5)) + cY.s3; pUYVY1.s2 = amd_pack16(f.s0,f.s1); pUYVY1.s3 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = amd_unpackA(L2.s6,L2.s7); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L2.s7,L3.s0)) + cY.s3; pUYVY1.s4 = amd_pack16(f.s0,f.s1); pUYVY1.s5 = amd_pack16(f.s2,f.s3);\n"
		"    rgb = amd_unpackA(L3.s1,L3.s2); f.s0 = dot(cU, rgb) + 32768.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 32768.0f; f.s3 = dot(cY.s012, amd_unpackB(L3.s2,L3.s3)) + cY.s3; pUYVY1.s6 = amd_pack16(f.s0,f.s1); pUYVY1.s7 = amd_pack16(f.s2,f.s3);\n";
	return output;
}
std::string ReadAndConvertRGB4toV210(){
	std::string output =  //RGB4 > V210, for description of V210 see: https://developer.apple.com/library/content/technotes/tn2162/_index.html#//
		"		uint8 L0, L1;\n"
		"		uint2 L2;\n"
		"		pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx * 72);\n"
		"		L0 = *(__global uint8 *) pRGB_buf;\n"
		"		L1 = *(__global uint8 *)&pRGB_buf[32];\n"
		"		L2 = *(__global uint2 *)&pRGB_buf[64];\n"
		"		uint8 pUYVY0;"
		"		float3 f; float3 rgb;\n"
		"		//Line[0]\n"
		"		rgb = amd_unpackA(L0.s0,L0.s1); f.s0 = dot(cU, rgb) + 512.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 512.0f;                                                        pUYVY0.s0 = amd_pack10(f.s0,f.s1,f.s2);\n" // BT601 U and V are at even pixel location(0,2,..), so ignoring odd pixels for chroma conversion, see https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx as reference
		"		                                                                                                                              f.s0 = dot(cY.s012, amd_unpackB(L0.s1,L0.s2)) + cY.s3;                                       ;\n"
		"		rgb = amd_unpackA(L0.s3,L0.s4); f.s1 = dot(cU, rgb) + 512.0f; f.s2 = dot(cY.s012, rgb) + cY.s3;                                                                                      pUYVY0.s1 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                                                                f.s0 = dot(cV, rgb) + 512.0f; f.s1 = dot(cY.s012, amd_unpackB(L0.s4,L0.s5)) + cY.s3;                                       ;\n"
		"		rgb = amd_unpackA(L0.s6,L0.s7); f.s2 = dot(cU, rgb) + 512.0f;                                                                                                                        pUYVY0.s2 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                              f.s0 = dot(cY.s012, rgb) + cY.s3; f.s1 = dot(cV, rgb) + 512.0f; f.s2 = dot(cY.s012, amd_unpackB(L0.s7,L1.s0)) + cY.s3; pUYVY0.s3 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		rgb = amd_unpackA(L1.s1,L1.s2); f.s0 = dot(cU, rgb) + 512.0f; f.s1 = dot(cY.s012, rgb) + cY.s3; f.s2 = dot(cV, rgb) + 512.0f;                                                        pUYVY0.s4 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                                                                                              f.s0 = dot(cY.s012, amd_unpackB(L1.s2,L1.s3)) + cY.s3;                                       ;\n"
		"		rgb = amd_unpackA(L1.s4,L1.s5); f.s1 = dot(cU, rgb) + 512.0f; f.s2 = dot(cY.s012, rgb) + cY.s3;                                                                                      pUYVY0.s5 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                                                                f.s0 = dot(cV, rgb) + 512.0f; f.s1 = dot(cY.s012, amd_unpackB(L1.s5,L1.s6)) + cY.s3;                                       ;\n"
		"		rgb = amd_unpackA(L1.s7,L2.s0); f.s2 = dot(cU, rgb) + 512.0f;                                                                                                                        pUYVY0.s6 = amd_pack10(f.s0,f.s1,f.s2);\n"
		"		                                                              f.s0 = dot(cY.s012, rgb) + cY.s3; f.s1 = dot(cV, rgb) + 512.0f; f.s2 = dot(cY.s012, amd_unpackB(L2.s0,L2.s1)) + cY.s3; pUYVY0.s7 = amd_pack10(f.s0,f.s1,f.s2);\n";
	return output;
}
// Write output to buffer ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string Write2x8PixelsToRGB2buffer(){
	std::string output =
		"    pRGB0.s0 = ((pRGB0.s0 & 0x00ffffff)      ) + (pRGB0.s1 << 24);\n"
		"    pRGB0.s1 = ((pRGB0.s1 & 0x00ffff00) >>  8) + (pRGB0.s2 << 16);\n"
		"    pRGB0.s2 = ((pRGB0.s2 & 0x00ff0000) >> 16) + (pRGB0.s3 <<  8);\n"
		"    pRGB0.s4 = ((pRGB0.s4 & 0x00ffffff)      ) + (pRGB0.s5 << 24);\n"
		"    pRGB0.s5 = ((pRGB0.s5 & 0x00ffff00) >>  8) + (pRGB0.s6 << 16);\n"
		"    pRGB0.s6 = ((pRGB0.s6 & 0x00ff0000) >> 16) + (pRGB0.s7 <<  8);\n"
		"    pRGB1.s0 = ((pRGB1.s0 & 0x00ffffff)      ) + (pRGB1.s1 << 24);\n"
		"    pRGB1.s1 = ((pRGB1.s1 & 0x00ffff00) >>  8) + (pRGB1.s2 << 16);\n"
		"    pRGB1.s2 = ((pRGB1.s2 & 0x00ff0000) >> 16) + (pRGB1.s3 <<  8);\n"
		"    pRGB1.s4 = ((pRGB1.s4 & 0x00ffffff)      ) + (pRGB1.s5 << 24);\n"
		"    pRGB1.s5 = ((pRGB1.s5 & 0x00ffff00) >>  8) + (pRGB1.s6 << 16);\n"
		"    pRGB1.s6 = ((pRGB1.s6 & 0x00ff0000) >> 16) + (pRGB1.s7 <<  8);\n"
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride * 2) + (gx * 24);\n"
		"    *(__global uint3 *) pRGB_buf = pRGB0.s012;\n"
		"    *(__global uint3 *)&pRGB_buf[12] = pRGB0.s456;\n"
		"    *(__global uint3 *)&pRGB_buf[pRGB_stride] = pRGB1.s012;\n"
		"    *(__global uint3 *)&pRGB_buf[pRGB_stride+12] = pRGB1.s456;\n";
	return output;
}
std::string Write1x4PixelsToRGB2buffer(){
	std::string output =
		"    rgbx.s0 = (amd_unpack0(pRGB0.s0) + amd_unpack0(pRGB0.s1) + amd_unpack0(pRGB1.s0) + amd_unpack0(pRGB1.s1)) * 0.25f;\n"
		"    rgbx.s1 = (amd_unpack1(pRGB0.s0) + amd_unpack1(pRGB0.s1) + amd_unpack1(pRGB1.s0) + amd_unpack1(pRGB1.s1)) * 0.25f;\n"
		"    rgbx.s2 = (amd_unpack2(pRGB0.s0) + amd_unpack2(pRGB0.s1) + amd_unpack2(pRGB1.s0) + amd_unpack2(pRGB1.s1)) * 0.25f;\n"
		"    rgbx.s3 = (amd_unpack0(pRGB0.s2) + amd_unpack0(pRGB0.s3) + amd_unpack0(pRGB1.s2) + amd_unpack0(pRGB1.s3)) * 0.25f;\n"
		"    pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = (amd_unpack1(pRGB0.s2) + amd_unpack1(pRGB0.s3) + amd_unpack1(pRGB1.s2) + amd_unpack1(pRGB1.s3)) * 0.25f;\n"
		"    rgbx.s1 = (amd_unpack2(pRGB0.s2) + amd_unpack2(pRGB0.s3) + amd_unpack2(pRGB1.s2) + amd_unpack2(pRGB1.s3)) * 0.25f;\n"
		"    rgbx.s2 = (amd_unpack0(pRGB0.s4) + amd_unpack0(pRGB0.s5) + amd_unpack0(pRGB1.s4) + amd_unpack0(pRGB1.s5)) * 0.25f;\n"
		"    rgbx.s3 = (amd_unpack1(pRGB0.s4) + amd_unpack1(pRGB0.s5) + amd_unpack1(pRGB1.s4) + amd_unpack1(pRGB1.s5)) * 0.25f;\n"
		"    pRGB0.s1 = amd_pack(rgbx);\n"
		"    rgbx.s0 = (amd_unpack2(pRGB0.s4) + amd_unpack2(pRGB0.s5) + amd_unpack2(pRGB1.s4) + amd_unpack2(pRGB1.s5)) * 0.25f;\n"
		"    rgbx.s1 = (amd_unpack0(pRGB0.s6) + amd_unpack0(pRGB0.s7) + amd_unpack0(pRGB1.s6) + amd_unpack0(pRGB1.s7)) * 0.25f;\n"
		"    rgbx.s2 = (amd_unpack1(pRGB0.s6) + amd_unpack1(pRGB0.s7) + amd_unpack1(pRGB1.s6) + amd_unpack1(pRGB1.s7)) * 0.25f;\n"
		"    rgbx.s3 = (amd_unpack2(pRGB0.s6) + amd_unpack2(pRGB0.s7) + amd_unpack2(pRGB1.s6) + amd_unpack2(pRGB1.s7)) * 0.25f;\n"
		"    pRGB0.s2 = amd_pack(rgbx);\n"
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx * 12);\n"
		"    *(__global uint3 *) pRGB_buf = pRGB0.s012;\n";
	return output;
}
std::string Write2x8PixelsToRGBXbuffer(){
	std::string output =
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride * 2) + (gx << 5);\n"
		"    *(__global uint8 *) pRGB_buf = pRGB0;\n"
		"    *(__global uint8 *)&pRGB_buf[pRGB_stride] = pRGB1;\n";
	return output;
}
std::string Write1x4PixelsToRGBXbuffer(){
	std::string output =
		"    rgbx.s0 = (amd_unpack0(pRGB0.s0) + amd_unpack0(pRGB0.s1) + amd_unpack0(pRGB1.s0) + amd_unpack0(pRGB1.s1)) * 0.25f;\n"
		"    rgbx.s1 = (amd_unpack1(pRGB0.s0) + amd_unpack1(pRGB0.s1) + amd_unpack1(pRGB1.s0) + amd_unpack1(pRGB1.s1)) * 0.25f;\n"
		"    rgbx.s2 = (amd_unpack2(pRGB0.s0) + amd_unpack2(pRGB0.s1) + amd_unpack2(pRGB1.s0) + amd_unpack2(pRGB1.s1)) * 0.25f;\n"
		"    rgbx.s3 = (amd_unpack3(pRGB0.s0) + amd_unpack3(pRGB0.s1) + amd_unpack3(pRGB1.s0) + amd_unpack3(pRGB1.s1)) * 0.25f;\n"
		"    pRGB0.s0 = amd_pack(rgbx);\n"
		"    rgbx.s0 = (amd_unpack0(pRGB0.s2) + amd_unpack0(pRGB0.s3) + amd_unpack0(pRGB1.s2) + amd_unpack0(pRGB1.s3)) * 0.25f;\n"
		"    rgbx.s1 = (amd_unpack1(pRGB0.s2) + amd_unpack1(pRGB0.s3) + amd_unpack1(pRGB1.s2) + amd_unpack1(pRGB1.s3)) * 0.25f;\n"
		"    rgbx.s2 = (amd_unpack2(pRGB0.s2) + amd_unpack2(pRGB0.s3) + amd_unpack2(pRGB1.s2) + amd_unpack2(pRGB1.s3)) * 0.25f;\n"
		"    rgbx.s3 = (amd_unpack3(pRGB0.s2) + amd_unpack3(pRGB0.s3) + amd_unpack3(pRGB1.s2) + amd_unpack3(pRGB1.s3)) * 0.25f;\n"
		"    pRGB0.s1 = amd_pack(rgbx);\n"
		"    rgbx.s0 = (amd_unpack0(pRGB0.s4) + amd_unpack0(pRGB0.s5) + amd_unpack0(pRGB1.s4) + amd_unpack0(pRGB1.s5)) * 0.25f;\n"
		"    rgbx.s1 = (amd_unpack1(pRGB0.s4) + amd_unpack1(pRGB0.s5) + amd_unpack1(pRGB1.s4) + amd_unpack1(pRGB1.s5)) * 0.25f;\n"
		"    rgbx.s2 = (amd_unpack2(pRGB0.s4) + amd_unpack2(pRGB0.s5) + amd_unpack2(pRGB1.s4) + amd_unpack2(pRGB1.s5)) * 0.25f;\n"
		"    rgbx.s3 = (amd_unpack3(pRGB0.s4) + amd_unpack3(pRGB0.s5) + amd_unpack3(pRGB1.s4) + amd_unpack3(pRGB1.s5)) * 0.25f;\n"
		"    pRGB0.s2 = amd_pack(rgbx);\n"
		"    rgbx.s0 = (amd_unpack0(pRGB0.s6) + amd_unpack0(pRGB0.s7) + amd_unpack0(pRGB1.s6) + amd_unpack0(pRGB1.s7)) * 0.25f;\n"
		"    rgbx.s1 = (amd_unpack1(pRGB0.s6) + amd_unpack1(pRGB0.s7) + amd_unpack1(pRGB1.s6) + amd_unpack1(pRGB1.s7)) * 0.25f;\n"
		"    rgbx.s2 = (amd_unpack2(pRGB0.s6) + amd_unpack2(pRGB0.s7) + amd_unpack2(pRGB1.s6) + amd_unpack2(pRGB1.s7)) * 0.25f;\n"
		"    rgbx.s3 = (amd_unpack3(pRGB0.s6) + amd_unpack3(pRGB0.s7) + amd_unpack3(pRGB1.s6) + amd_unpack3(pRGB1.s7)) * 0.25f;\n"
		"    pRGB0.s3 = amd_pack(rgbx);\n"
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx << 4);\n"
		"    *(__global uint4 *) pRGB_buf = pRGB0.s0123;\n";
	return output;
}
std::string Write2x8PixelsToRGB4buffer(){
	std::string output =
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride * 2) + (gx * 48);\n"
		"    *(__global uint8 *) pRGB_buf = pRGB0;\n"
		"    *(__global uint4 *)&pRGB_buf[32] = pRGB1.s0123;\n"
		"    *(__global uint8 *)&pRGB_buf[pRGB_stride] = pRGB2;\n"
		"    *(__global uint4 *)&pRGB_buf[pRGB_stride+32] = pRGB3.s0123;\n";
	return output;
}
std::string Write1x4PixelsToRGB4buffer(){
	std::string output =
		"    rgbx.s0 = ((float)(pRGB0.s0 & 0x0000ffff) + (float)(pRGB0.s1 >> 16) + (float)(pRGB2.s0 & 0x0000ffff) + (float)(pRGB2.s1 >> 16)) * 0.25f;\n"
		"    rgbx.s1 = ((float)(pRGB0.s0 >> 16) + (float)(pRGB0.s2 & 0x0000ffff) + (float)(pRGB2.s0 >> 16) + (float)(pRGB2.s2 & 0x0000ffff)) * 0.25f;\n"
		"    rgbx.s2 = ((float)(pRGB0.s1 & 0x0000ffff) + (float)(pRGB0.s2 >> 16) + (float)(pRGB2.s1 & 0x0000ffff) + (float)(pRGB2.s2 >> 16)) * 0.25f;\n"
		"    rgbx.s3 = ((float)(pRGB0.s3 & 0x0000ffff) + (float)(pRGB0.s4 >> 16) + (float)(pRGB2.s3 & 0x0000ffff) + (float)(pRGB2.s4 >> 16)) * 0.25f;\n"
		"    pRGB0.s0 = ( ( (uint) rgbx.s1)<<16)+(uint)rgbx.s0;\n"
		"    pRGB0.s1 = ( ( (uint) rgbx.s3)<<16)+(uint)rgbx.s2;\n"
		"    rgbx.s0 = ((float)(pRGB0.s3 >> 16) + (float)(pRGB0.s5 & 0x0000ffff) + (float)(pRGB2.s3 >> 16) + (float)(pRGB2.s5 & 0x0000ffff)) * 0.25f;\n"
		"    rgbx.s1 = ((float)(pRGB0.s4 & 0x0000ffff) + (float)(pRGB0.s5 >> 16) + (float)(pRGB2.s4 & 0x0000ffff) + (float)(pRGB2.s5 >> 16)) * 0.25f;\n"
		"    rgbx.s2 = ((float)(pRGB0.s6 & 0x0000ffff) + (float)(pRGB0.s7 >> 16) + (float)(pRGB2.s6 & 0x0000ffff) + (float)(pRGB2.s7 >> 16)) * 0.25f;\n"
		"    rgbx.s3 = ((float)(pRGB0.s6 >> 16) + (float)(pRGB1.s0 & 0x0000ffff) + (float)(pRGB2.s6 >> 16) + (float)(pRGB3.s0 & 0x0000ffff)) * 0.25f;\n"
		"    pRGB0.s2 = ( ( (uint) rgbx.s1)<<16)+(uint)rgbx.s0;\n"
		"    pRGB0.s3 = ( ( (uint) rgbx.s3)<<16)+(uint)rgbx.s2;\n"
		"    rgbx.s0 = ((float)(pRGB0.s7 & 0x0000ffff) + (float)(pRGB1.s0 >> 16) + (float)(pRGB2.s7 & 0x0000ffff) + (float)(pRGB3.s0 >> 16)) * 0.25f;\n"
		"    rgbx.s1 = ((float)(pRGB1.s1 & 0x0000ffff) + (float)(pRGB1.s2 >> 16) + (float)(pRGB3.s1 & 0x0000ffff) + (float)(pRGB3.s2 >> 16)) * 0.25f;\n"
		"    rgbx.s2 = ((float)(pRGB1.s1 >> 16) + (float)(pRGB1.s3 & 0x0000ffff) + (float)(pRGB3.s1 >> 16) + (float)(pRGB3.s3 & 0x0000ffff)) * 0.25f;\n"
		"    rgbx.s3 = ((float)(pRGB1.s2 & 0x0000ffff) + (float)(pRGB1.s3 >> 16) + (float)(pRGB3.s2 & 0x0000ffff) + (float)(pRGB3.s3 >> 16)) * 0.25f;\n"
		"    pRGB0.s4 = ( ( (uint) rgbx.s1)<<16)+(uint)rgbx.s0;\n"
		"    pRGB0.s5 = ( ( (uint) rgbx.s3)<<16)+(uint)rgbx.s2;\n"
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx * 24);\n"
		"    *(__global uint4 *) pRGB_buf = pRGB0.s0123;\n"
		"    *(__global uint2 *) &pRGB_buf[16] = pRGB0.s45;\n";
	return output;
}
std::string Write1x6PixelsToRGB2buffer(){
	std::string output =
		"    pRGB0.s0 = ((pRGB0.s0 & 0x00ffffff)      ) + (pRGB0.s1 << 24);\n"
		"    pRGB0.s1 = ((pRGB0.s1 & 0x00ffff00) >>  8) + (pRGB0.s2 << 16);\n"
		"    pRGB0.s2 = ((pRGB0.s2 & 0x00ff0000) >> 16) + (pRGB0.s3 <<  8);\n"
		"    pRGB0.s3 = ((pRGB0.s4 & 0x00ffffff)      ) + (pRGB0.s5 << 24);\n"
		"    pRGB0.s4 = ((pRGB0.s5 & 0x00ffff00) >>  8) + (pRGB0.s6 << 16);\n"
		"    pRGB0.s5 = ((pRGB0.s6 & 0x00ff0000) >> 16) + (pRGB0.s7 <<  8);\n"
		"    pRGB0.s6 = ((pRGB1.s0 & 0x00ffffff)      ) + (pRGB1.s1 << 24);\n"
		"    pRGB0.s7 = ((pRGB1.s1 & 0x00ffff00) >>  8) + (pRGB1.s2 << 16);\n"
		"    pRGB1.s0 = ((pRGB1.s2 & 0x00ff0000) >> 16) + (pRGB1.s3 <<  8);\n"
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx * 36);\n"
		"    *(__global uint8 *) pRGB_buf = pRGB0;\n"
		"    *(__global uint  *)&pRGB_buf[32] = pRGB1.s0;\n";
	return output;
}
std::string Write1x6PixelsToRGBXbuffer(){
	std::string output =
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx * 48);\n"
		"    *(__global uint8 *) pRGB_buf = pRGB0;\n"
		"    *(__global uint4 *)&pRGB_buf[32] = pRGB1;\n";
	return output;
}
std::string Write1x6PixelsToRGB4buffer(){
	std::string output =
		"    pRGB_buf += pRGB_offset + (gy * pRGB_stride) + (gx * 72);\n"
		"    *(__global uint8 *) pRGB_buf = pRGB0;\n"
		"    *(__global uint8 *)&pRGB_buf[32] = pRGB1;\n"
		"    *(__global uint2 *)&pRGB_buf[64] = pRGB2;\n";
	return output;
}
std::string Write2x8PixelsTo422buffer8bit(){
	std::string output =
		"    p422_buf += p422_offset + (gy * p422_stride * 2) + (gx << 4);\n"
		"    *(__global uint4 *) p422_buf = pUYVY.s0123;\n"
		"    *(__global uint4 *)&p422_buf[p422_stride] = pUYVY.s4567;\n";
	return output;
}
std::string Write1x4PixelsTo422bufferUYVY(){
	std::string output =
		"    f.s0 = (amd_unpack0(pUYVY.s0) + amd_unpack0(pUYVY.s1) + amd_unpack0(pUYVY.s4) + amd_unpack0(pUYVY.s5)) * 0.25f;\n"
		"    f.s1 = (amd_unpack1(pUYVY.s0) + amd_unpack3(pUYVY.s0) + amd_unpack1(pUYVY.s4) + amd_unpack3(pUYVY.s4)) * 0.25f;\n"
		"    f.s2 = (amd_unpack2(pUYVY.s0) + amd_unpack2(pUYVY.s1) + amd_unpack2(pUYVY.s4) + amd_unpack2(pUYVY.s5)) * 0.25f;\n"
		"    f.s3 = (amd_unpack1(pUYVY.s1) + amd_unpack3(pUYVY.s1) + amd_unpack1(pUYVY.s5) + amd_unpack3(pUYVY.s5)) * 0.25f;\n"
		"    pUYVY.s0 = amd_pack(f);\n"
		"    f.s0 = (amd_unpack0(pUYVY.s2) + amd_unpack0(pUYVY.s3) + amd_unpack0(pUYVY.s6) + amd_unpack0(pUYVY.s7)) * 0.25f;\n"
		"    f.s1 = (amd_unpack1(pUYVY.s2) + amd_unpack3(pUYVY.s2) + amd_unpack1(pUYVY.s6) + amd_unpack3(pUYVY.s6)) * 0.25f;\n"
		"    f.s2 = (amd_unpack2(pUYVY.s2) + amd_unpack2(pUYVY.s3) + amd_unpack2(pUYVY.s6) + amd_unpack2(pUYVY.s7)) * 0.25f;\n"
		"    f.s3 = (amd_unpack1(pUYVY.s3) + amd_unpack3(pUYVY.s3) + amd_unpack1(pUYVY.s7) + amd_unpack3(pUYVY.s7)) * 0.25f;\n"
		"    pUYVY.s1 = amd_pack(f);\n"
		"    p422_buf += p422_offset + (gy * p422_stride) + (gx << 3);\n"
		"    *(__global uint2 *) p422_buf = pUYVY.s01;\n";
	return output;
}
std::string Write1x4PixelsTo422bufferYUYV(){
	std::string output =
		"    f.s0 = (amd_unpack0(pUYVY.s0) + amd_unpack2(pUYVY.s0) + amd_unpack0(pUYVY.s4) + amd_unpack3(pUYVY.s4)) * 0.25f;\n"
		"    f.s1 = (amd_unpack1(pUYVY.s0) + amd_unpack1(pUYVY.s1) + amd_unpack1(pUYVY.s4) + amd_unpack1(pUYVY.s5)) * 0.25f;\n"
		"    f.s2 = (amd_unpack0(pUYVY.s1) + amd_unpack2(pUYVY.s1) + amd_unpack0(pUYVY.s5) + amd_unpack2(pUYVY.s5)) * 0.25f;\n"
		"    f.s3 = (amd_unpack3(pUYVY.s1) + amd_unpack3(pUYVY.s1) + amd_unpack3(pUYVY.s4) + amd_unpack3(pUYVY.s5)) * 0.25f;\n"
		"    pUYVY.s0 = amd_pack(f);\n"
		"    f.s0 = (amd_unpack0(pUYVY.s2) + amd_unpack2(pUYVY.s2) + amd_unpack0(pUYVY.s6) + amd_unpack2(pUYVY.s6)) * 0.25f;\n"
		"    f.s1 = (amd_unpack1(pUYVY.s2) + amd_unpack1(pUYVY.s3) + amd_unpack1(pUYVY.s6) + amd_unpack1(pUYVY.s7)) * 0.25f;\n"
		"    f.s2 = (amd_unpack0(pUYVY.s3) + amd_unpack2(pUYVY.s3) + amd_unpack0(pUYVY.s7) + amd_unpack2(pUYVY.s7)) * 0.25f;\n"
		"    f.s3 = (amd_unpack3(pUYVY.s2) + amd_unpack3(pUYVY.s3) + amd_unpack3(pUYVY.s6) + amd_unpack3(pUYVY.s7)) * 0.25f;\n"
		"    pUYVY.s1 = amd_pack(f);\n"
		"    p422_buf += p422_offset + (gy * p422_stride) + (gx << 3);\n"
		"    *(__global uint2 *) p422_buf = pUYVY.s01;\n";
	return output;
}
std::string Write2x8PixelsToYbufferAndUVbuffer(){
	std::string output =
		"    pY_buf += pY_offset + (gy * pY_stride*2) + (gx << 3);\n"
		"    pUV_buf += pUV_offset + (gy * pUV_stride) + (gx << 3);\n"
		"    *(__global uint2 *) pY_buf = pY0;\n"
		"    *(__global uint2 *)&pY_buf[pY_stride] = pY1;\n"
		"    *(__global uint2 *) pUV_buf = pUV;\n";
	return output;
}
std::string Write2x8PixelsToYbufferAndUbufferAndVbuffer(){
	std::string output =
		"    pY_buf += pY_offset + (gy * pY_stride * 2) + (gx << 3);\n"
		"    pU_buf += pU_offset + (gy * pU_stride) + (gx << 2);\n"
		"    pV_buf += pV_offset + (gy * pV_stride) + (gx << 2);\n"
		"    *(__global uint2 *) pY_buf = pY0;\n"
		"    *(__global uint2 *)&pY_buf[pY_stride] = pY1;\n"
		"    *(__global uint *) pU_buf = pU;\n"
		"    *(__global uint *) pV_buf = pV;\n";
	return output;
}
std::string Write1x6PixelsTo422buffer(){
	std::string output =
		"    p422_buf += p422_offset + (gy * p422_stride) + (gx << 5);\n"
		"    *(__global uint8 *) p422_buf = pUYVY0;\n";
	return output;
}
std::string Write2x8PixelsTo422buffer16bit(){
	std::string output =
		"    p422_buf += p422_offset + (gy * p422_stride * 2) + (gx << 5);\n"
		"    *(__global uint8 *) p422_buf = pUYVY0;\n"
		"    *(__global uint8 *)&p422_buf[p422_stride] = pUYVY1;\n";
	return output;
}
std::string Write1x4PixelsTo422buffer16bit(){
	std::string output =
		"    f.s0 = ((float)(pUYVY0.s0 & 0x0000ffff) + (float)(pUYVY0.s2 & 0x0000ffff) + (float)(pUYVY1.s0 & 0x0000ffff) + (float)(pUYVY1.s2 & 0x0000ffff)) * 0.25f;\n"
		"    f.s1 = ((float)(pUYVY0.s0 >> 16) + (float)(pUYVY0.s1 >> 16) + (float)(pUYVY1.s0 >> 16) + (float)(pUYVY1.s1 >> 16)) * 0.25f;\n"
		"    f.s2 = ((float)(pUYVY0.s1 & 0x0000ffff) + (float)(pUYVY0.s3 & 0x0000ffff) + (float)(pUYVY1.s1 & 0x0000ffff) + (float)(pUYVY1.s3 & 0x0000ffff)) * 0.25f;\n"
		"    f.s3 = ((float)(pUYVY0.s2 >> 16) + (float)(pUYVY0.s3 >> 16) + (float)(pUYVY1.s2 >> 16) + (float)(pUYVY1.s3 >> 16)) * 0.25f;\n"
		"    pUYVY0.s0 = amd_pack16(f.s0,f.s1);\n"
		"    pUYVY0.s1 = amd_pack16(f.s2,f.s3);\n"
		"    f.s0 = ((float)(pUYVY0.s4 & 0x0000ffff) + (float)(pUYVY0.s6 & 0x0000ffff) + (float)(pUYVY1.s4 & 0x0000ffff) + (float)(pUYVY1.s6 & 0x0000ffff)) * 0.25f;\n"
		"    f.s1 = ((float)(pUYVY0.s4 >> 16) + (float)(pUYVY0.s5 >> 16) + (float)(pUYVY1.s4 >> 16) + (float)(pUYVY1.s5 >> 16)) * 0.25f;\n"
		"    f.s2 = ((float)(pUYVY0.s5 & 0x0000ffff) + (float)(pUYVY0.s7 & 0x0000ffff) + (float)(pUYVY1.s5 & 0x0000ffff) + (float)(pUYVY1.s7 & 0x0000ffff)) * 0.25f;\n"
		"    f.s3 = ((float)(pUYVY0.s6 >> 16) + (float)(pUYVY0.s7 >> 16) + (float)(pUYVY1.s6 >> 16) + (float)(pUYVY1.s7 >> 16)) * 0.25f;\n"
		"    pUYVY0.s2 = amd_pack16(f.s0,f.s1);\n"
		"    pUYVY0.s3 = amd_pack16(f.s2,f.s3);\n"
		"    p422_buf += p422_offset + (gy * p422_stride) + (gx << 4);\n"
		"    *(__global uint4 *) p422_buf = pUYVY0.s0123;\n";
	return output;
}
//Define help functions for 10 and 16 bit -------------------------------------------------------------------------------------------------------------------------------------------------------
std::string Create_amd_unpack16(){
	std::string output =
		"float4 amd_unpack16(uint src0, uint src1)\n"
		"{\n"
		"  return (float4)((src0 & 0xffff), ((src0 >> 16) & 0xffff), (src1 & 0xffff),((src1 >> 16) & 0xffff));\n"
		"}\n"
		"\n";
	return output;
}
std::string Create_amd_unpackAB(){
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
std::string Create_amd_unpack10(){
	std::string output =
		"float3 amd_unpack10(uint src0)\n"
		"{\n"
		"  return (float3)((float)(src0 & 0x3ff),(float)((src0 >> 10) & 0x3ff), (float)((src0 >> 20) & 0x3ff));\n"
		"}\n";
	return output;
}
std::string Create_amd_pack15(){
	std::string output =
		"uint amd_pack15(float src0, float src1)\n"
		"{\n"
		"  return ( ( ( (uint) clamp(src1,0.0f,32767.0f))<<16) + (uint) clamp(src0,0.0f,32767.0f) );\n"
		"}\n"
		"\n";
	return output;
}
std::string Create_amd_pack16(){
	std::string output =
		"uint amd_pack16(float src0, float src1)\n"
		"{\n"
		"  return ( ( ( (uint) clamp(src1,0.0f,65536.0f))<<16) + (uint) clamp(src0,0.0f,65535.0f) );\n"
		"}\n"
		"\n";
	return output;
}
std::string Create_amd_pack10(){
	std::string output =
		"uint amd_pack10(float src0, float src1, float src2)\n"
		"{\n"
		"  return ( (uint) clamp(src0,0.0f,1023.0f) + (((uint) clamp(src1,0.0f,1023.0f))<<10) + (((uint) clamp(src2,0.0f,1023.0f))<<20));\n"
		"}\n"
		"\n";
	return output;
}
