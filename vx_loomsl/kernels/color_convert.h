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


#ifndef __COLOR_CONVERT_H__
#define __COLOR_CONVERT_H__

#include "kernels.h"

//////////////////////////////////////////////////////////////////////
//! \brief The kernel registration functions.
vx_status color_convert_general_publish(vx_context context); // for UYVY, YUYV, V210, V216
vx_status color_convert_from_NV12_publish(vx_context context); // for NV12 input
vx_status color_convert_from_IYUV_publish(vx_context context); // for IYUV input
vx_status color_convert_to_NV12_publish(vx_context context); // for NV12 output
vx_status color_convert_to_IYUV_publish(vx_context context); // for IYUV output

//////////////////////////////////////////////////////////////////////
// Define functions to create OpenCL code
// Input Color Conversions  ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string GetColorConversionTableForYUVInput(vx_color_space_e input_color_space);
std::string GetRangeConversionTableFor8bitTo8bit(vx_channel_range_e input_channel_range);
//Input Range Conversion ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string GetRangeConversionTableFor8bitTo15bit(vx_channel_range_e input_channel_range);
std::string GetRangeConversionTableFor10bitTo8bit(vx_channel_range_e input_channel_range);
std::string GetRangeConversionTableFor10bitTo15bit(vx_channel_range_e input_channel_range);
std::string GetRangeConversionTableFor16bitTo8bit(vx_channel_range_e input_channel_range);
std::string GetRangeConversionTableFor16bitTo15bit(vx_channel_range_e input_channel_range);
// Output Color and Range Conversion ------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string GetColorRangeConversionTableFor8bitTo8bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range);
std::string GetColorRangeConversionTableFor8bitTo10bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range);
std::string GetColorRangeConversionTableFor8bitTo16bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range);
std::string GetColorRangeConversionTableFor15bitTo8bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range);
std::string GetColorRangeConversionTableFor15bitTo10bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range);
std::string GetColorRangeConversionTableFor15bitTo16bit(vx_color_space_e output_color_space, vx_channel_range_e output_channel_range);
// Read input from buffer -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string Read2x8PixelsFrom422buffer8bit();
std::string Read2x8PixelsFromYbufferAndUVbuffer();
std::string Read2x8PixelsFromYbufferAndUbufferAndVbuffer();
std::string Read1x6PixelsFrom422buffer();
std::string Read2x8PixelsFrom422buffer16bit();
std::string Read2x8PixelsFromRGBbuffer8bit();
std::string Read2x8PixelsFromRGBbuffer16bit();
// Do Color Conversion --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string ConvertUYVYtoRGB2();
std::string ConvertUYVYtoRGBX();
std::string ConvertUYVYtoRGB4();
std::string ConvertUYVYtoRGB4AndDegamma();
std::string ConvertYUYVtoRGB2();
std::string ConvertYUYVtoRGBX();
std::string ConvertYUYVtoRGB4();
std::string ConvertYUYVtoRGB4AndDegamma();
std::string ConvertNV12toRGB2();
std::string ConvertNV12toRGBX();
std::string ConvertNV12toRGB4();
std::string ConvertNV12toRGB4AndDegamma();
std::string ConvertIYUVtoRGB2();
std::string ConvertIYUVtoRGBX();
std::string ConvertIYUVtoRGB4();
std::string ConvertIYUVtoRGB4AndDegamma();
std::string ConvertV210toRGB2();
std::string ConvertV210toRGBX();
std::string ConvertV210toRGB4();
std::string ConvertV210toRGB4AndDegamma();
std::string ConvertV216toRGB2();
std::string ConvertV216toRGBX();
std::string ConvertV216toRGB4();
std::string ConvertV216toRGB4AndDegamma();
std::string ConvertRGB2toUYVY();
std::string ConvertRGB2toYUYV();
std::string ConvertRGB2toNV12();
std::string ConvertRGB2toIYUV();
std::string ConvertRGB2toV216();
std::string ReadAndConvertRGB2toV210();
std::string ConvertRGB4toUYVY();
std::string ConvertRGB4toYUYV();
std::string ConvertRGB4toNV12();
std::string ConvertRGB4toIYUV();
std::string ConvertRGB4toV216();
std::string ReadAndConvertRGB4toV210();
std::string GammaAndConvertRGB4toUYVY();
std::string GammaAndConvertRGB4toYUYV();
std::string GammaAndConvertRGB4toNV12();
std::string GammaAndConvertRGB4toIYUV();
std::string GammaAndConvertRGB4toV216();
std::string ReadAndGammaAndConvertRGB4toV210();
// Write output to buffer ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
std::string Write2x8PixelsToRGB2buffer();
std::string Write1x4PixelsToRGB2buffer();
std::string Write2x8PixelsToRGBXbuffer();
std::string Write1x4PixelsToRGBXbuffer();
std::string Write2x8PixelsToRGB4buffer();
std::string Write1x4PixelsToRGB4buffer();
std::string Write1x6PixelsToRGB2buffer();
std::string Write1x6PixelsToRGBXbuffer();
std::string Write1x6PixelsToRGB4buffer();
std::string Write2x8PixelsTo422buffer8bit();
std::string Write1x4PixelsTo422bufferUYVY();
std::string Write1x4PixelsTo422bufferYUYV();
std::string Write2x8PixelsToYbufferAndUVbuffer();
std::string Write2x8PixelsToYbufferAndUbufferAndVbuffer();
std::string Write1x6PixelsTo422buffer();
std::string Write2x8PixelsTo422buffer16bit();
std::string Write1x4PixelsTo422buffer16bit();
//Define help functions for 10 and 16 bit -------------------------------------------------------------------------------------------------------------------------------------------------------
std::string Create_amd_unpack16();
std::string Create_amd_unpackAB();
std::string Create_amd_unpack10();
std::string Create_amd_pack15();
std::string Create_amd_pack16();
std::string Create_amd_pack10();
std::string Degamma();
std::string Gamma();
std::string Gamma3();

#endif //__COLOR_CONVERT_H__
