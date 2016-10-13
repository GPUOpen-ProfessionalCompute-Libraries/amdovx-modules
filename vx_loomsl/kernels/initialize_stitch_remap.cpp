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
#include "kernels.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <omp.h>

#define COMPUTE_REMAP_DEBUG 0

//! \brief Function to Compute M.
static void ComputeM(float * M, float th, float fi, float sy)
{
	float sth = sinf(th), cth = cosf(th);
	float sfi = sinf(fi), cfi = cosf(fi);
	float ssy = sinf(sy), csy = cosf(sy);
	M[0] = sth*ssy*sfi + cth*csy; M[1] = ssy*cfi; M[2] = cth*ssy*sfi - sth*csy;
	M[3] = sth*csy*sfi - cth*ssy; M[4] = csy*cfi; M[5] = cth*csy*sfi + sth*ssy;
	M[6] = sth    *cfi;          M[7] = -sfi;     M[8] = cth    *cfi;
}

//! \brief Matix Multiplication Function.
static void MatMul3x3(float * C, const float * A, const float * B)
{
	const float * At = A;
	for (int i = 0; i < 3; i++, At += 3) {
		const float * Bt = B;
		for (int j = 0; j < 3; j++, Bt++, C++) {
			*C = At[0] * Bt[0] + At[1] * Bt[3] + At[2] * Bt[6];
		}
	}
}

//! \brief Matix Multiplication Function.
static void MatMul3x1(float * Y, const float * M, const float * X)
{
	Y[0] = M[0] * X[0] + M[1] * X[1] + M[2] * X[2];
	Y[1] = M[3] * X[0] + M[4] * X[1] + M[5] * X[2];
	Y[2] = M[6] * X[0] + M[7] * X[1] + M[8] * X[2];
}

//! \brief The validator callback.
static vx_status VX_CALLBACK initialize_stitch_remap_validate(vx_node node, const vx_reference parameters[], vx_uint32 num, vx_meta_format metas[])
{
	// check scalar types
	vx_enum types[5] = { VX_TYPE_INVALID, VX_TYPE_INVALID, VX_TYPE_INVALID, VX_TYPE_INVALID };
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[0], VX_SCALAR_ATTRIBUTE_TYPE, &types[0], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[1], VX_SCALAR_ATTRIBUTE_TYPE, &types[1], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[2], VX_SCALAR_ATTRIBUTE_TYPE, &types[2], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[3], VX_SCALAR_ATTRIBUTE_TYPE, &types[3], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[4], VX_SCALAR_ATTRIBUTE_TYPE, &types[4], sizeof(vx_enum)));
	if (types[0] != VX_TYPE_UINT32 || types[1] != VX_TYPE_UINT32 || types[2] != VX_TYPE_UINT32 || types[3] != VX_TYPE_UINT32 || types[4] != VX_TYPE_UINT32) 
	{
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_remap: scalar type not valid\n");
		return status;
	}

	// read scalar values
	vx_uint32 num_cam = 0, num_buff_rows = 0, num_buff_cols = 0, cam_buffer_width = 0, cam_buffer_height = 0, width_eqr = 0, height_eqr = 0;
	vx_uint32 width_scr = 0, height_src = 0;
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[0], &num_buff_rows));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[1], &num_buff_cols));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[2], &cam_buffer_width));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[3], &cam_buffer_height));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[4], &width_eqr));
	if (num_buff_rows < 1 || num_buff_cols < 1 || width_eqr < 1 || cam_buffer_width < 1 || cam_buffer_height < 1) {
		vx_status status = VX_ERROR_INVALID_VALUE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_remap: scalar value not valid\n");
		return status;
	}
	num_cam = (vx_uint32)(num_buff_rows * num_buff_cols);
	height_eqr = width_eqr >> 1;

	// check rig config matrix dimensions
	vx_enum type = VX_TYPE_INVALID;
	vx_size columns = 0, rows = 0;
	ERROR_CHECK_STATUS(vxQueryMatrix((vx_matrix)parameters[5], VX_MATRIX_ATTRIBUTE_TYPE, &type, sizeof(type)));
	ERROR_CHECK_STATUS(vxQueryMatrix((vx_matrix)parameters[5], VX_MATRIX_ATTRIBUTE_COLUMNS, &columns, sizeof(columns)));
	ERROR_CHECK_STATUS(vxQueryMatrix((vx_matrix)parameters[5], VX_MATRIX_ATTRIBUTE_ROWS, &rows, sizeof(rows)));
	if (type != VX_TYPE_FLOAT32 || columns != 4 || rows != 1) {
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_remap: rig params matrix type/dimensions are not valid\n");
		return status;
	}

	// check camera config array dimensions
	type = VX_TYPE_INVALID;
	vx_size size = 0, num_items = 0;
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[6], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[6], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[6], VX_ARRAY_ATTRIBUTE_CAPACITY, &num_items, sizeof(num_items)));
	if ( size != sizeof(camera_params) || (num_items != num_cam)) {
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_remap: camera params array type/dimensions are not valid\n");
		return status;
	}

	// set output remap dimensions same as what was specified
	vx_uint32 src_width = cam_buffer_width, src_height = cam_buffer_height;
	vx_uint32 dst_width = width_eqr, dst_height = height_eqr;
#if 1 // remap set meta not implemented in openvx.
	vx_uint32 widthSrc = 0, heightSrc = 0, widthDst = 0, heightDst = 0;
	ERROR_CHECK_STATUS(vxQueryRemap((vx_remap)parameters[7], VX_REMAP_ATTRIBUTE_SOURCE_WIDTH, &widthSrc, sizeof(widthSrc)));
	ERROR_CHECK_STATUS(vxQueryRemap((vx_remap)parameters[7], VX_REMAP_ATTRIBUTE_SOURCE_HEIGHT, &heightSrc, sizeof(heightSrc)));
	ERROR_CHECK_STATUS(vxQueryRemap((vx_remap)parameters[7], VX_REMAP_ATTRIBUTE_DESTINATION_WIDTH, &widthDst, sizeof(widthDst)));
	ERROR_CHECK_STATUS(vxQueryRemap((vx_remap)parameters[7], VX_REMAP_ATTRIBUTE_DESTINATION_HEIGHT, &heightDst, sizeof(heightDst)));
	if(widthSrc != src_width || heightSrc != src_height || widthDst != dst_width || heightDst != dst_height) {
		vx_status status = VX_ERROR_INVALID_DIMENSION;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_remap: remap dimensions are not valid\n");
		return status;
	}
#else
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[6], VX_REMAP_ATTRIBUTE_SOURCE_WIDTH, &src_width, sizeof(src_width)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[6], VX_REMAP_ATTRIBUTE_SOURCE_HEIGHT, &src_height, sizeof(src_height)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[6], VX_REMAP_ATTRIBUTE_DESTINATION_WIDTH, &dst_width, sizeof(dst_width)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[6], VX_REMAP_ATTRIBUTE_DESTINATION_HEIGHT, &dst_height, sizeof(dst_height)));
#endif
	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK initialize_stitch_remap_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	///////////////////////////////////////////////////////
	// get num cameras and image dimensions
	vx_uint32 num_cam = 0, num_buff_rows = 0, num_buff_cols = 0, cam_buffer_width = 0, cam_buffer_height = 0, width_eqr = 0, height_eqr = 0;
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[0], &num_buff_rows));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[1], &num_buff_cols));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[2], &cam_buffer_width));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[3], &cam_buffer_height));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[4], &width_eqr));
	height_eqr = width_eqr >> 1;
	num_cam = (vx_uint32)(num_buff_rows * num_buff_cols);

	vx_uint32 src_width = 0, src_height = 0;
	vx_uint32 dst_width = 0, dst_height = 0;
	src_width = (vx_uint32)(cam_buffer_width / num_buff_cols);
	src_height = (vx_uint32)(cam_buffer_height / num_buff_rows);
	dst_width = width_eqr;
	dst_height = height_eqr;

	// get rig & cameras parameters
	vx_matrix mat_rig = (vx_matrix)parameters[5];
	vx_array array_cam = (vx_array)parameters[6];
	vx_remap remap = (vx_remap)parameters[7];

	int width = src_width;
	int height = src_height;

	// get rig and camera parameters
	rig_params rig_par;
	ERROR_CHECK_STATUS(vxReadMatrix(mat_rig, &rig_par));
	camera_params *cam_par = nullptr;
	vx_size stride = sizeof(camera_params), num_items = 0;
	ERROR_CHECK_STATUS(vxQueryArray(array_cam, VX_ARRAY_ATTRIBUTE_NUMITEMS, &num_items, sizeof(num_items)));
	if (num_items != num_cam) {
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_remap: camera parameter dimensions are not valid\n");
		return status;
	}
	ERROR_CHECK_STATUS(vxAccessArrayRange(array_cam, 0, num_items, &stride, (void **)&cam_par, VX_READ_ONLY));

#if COMPUTE_REMAP_DEBUG
	printf("**********COMPUTE REMAP DEBUG**********\n");
	printf("Debug::NumCamera -- %d\n", num_cam);
	printf("Debug::Input Buffer -- Rows:%d Cols:%d \n", num_buff_rows, num_buff_cols);
	printf("Debug::Input Buffer -- Width:%d Height:%d \n", cam_buffer_width, cam_buffer_height);
	printf("Debug::Camera -- Width:%d Height:%d \n", src_width, src_height);
	printf("Debug::Output Eqr -- Width:%d Height:%d \n", dst_width, dst_height);
	printf("Debug::Rig Parameters -- Yaw:%.2f Pitch:%.2f Roll:%.2f D:%.2f\n", rig_par.yaw, rig_par.pitch, rig_par.roll, rig_par.d);
	printf("Debug::Camera Parameters -- NumItems:%d\n", num_items);
	for (vx_uint32 i = 0; i < num_cam; i++)
	{
		printf("Debug::Camera->%d\n",i);
		printf("	Yaw:%f Patch:%f Roll:%f Tx:%f Ty:%f Tz:%f\n", cam_par[i].focal.yaw, cam_par[i].focal.pitch, cam_par[i].focal.roll, 
			cam_par[i].focal.tx, cam_par[i].focal.ty, cam_par[i].focal.tz);
		printf("	LensType:%d haw:%.2f hfov:%.2f k1:%f k2:%f k3:%f du0:%.2f dv0:%.2f r_crop:%.2f\n", cam_par[i].lens.lens_type, cam_par[i].lens.haw,
			cam_par[i].lens.hfov, cam_par[i].lens.k1, cam_par[i].lens.k2, cam_par[i].lens.k3, cam_par[i].lens.du0, cam_par[i].lens.dv0, cam_par[i].lens.r_crop);
	}
	printf("**************************************\n");
#endif

	// pre-compute M & T of each camera
	float * Mcam = new float[num_cam * 9];
	float * Tcam = new float[num_cam * 3];
	float * fcam = new float[num_cam * 2];
	float Mr[9];
	float deg2rad = (float)M_PI / 180.0f;
	ComputeM(Mr, rig_par.yaw * deg2rad, rig_par.pitch * deg2rad, rig_par.roll * deg2rad);
	for (vx_uint32 cam = 0; cam < num_cam; cam++) {
		float Mc[9];
		ComputeM(Mc, cam_par[cam].focal.yaw * deg2rad, cam_par[cam].focal.pitch * deg2rad, cam_par[cam].focal.roll * deg2rad);
		MatMul3x3(&Mcam[cam * 9], Mc, Mr);
		if (rig_par.d > 0.0f) {
			Tcam[cam * 3 + 0] = cam_par[cam].focal.tx / rig_par.d;
			Tcam[cam * 3 + 1] = cam_par[cam].focal.ty / rig_par.d;
			Tcam[cam * 3 + 2] = cam_par[cam].focal.tz / rig_par.d;
		}
		else {
			Tcam[cam * 3 + 0] = Tcam[cam * 3 + 1] = Tcam[cam * 3 + 2] = 0.0f;
		}

		if (cam_par[cam].lens.lens_type == 0) { // ptgui rectilinear
			fcam[cam * 2 + 0] = 1.0f / tanf(0.5f * cam_par[cam].lens.hfov * deg2rad);
			fcam[cam * 2 + 1] = 0.5f * cam_par[cam].lens.haw;
		}
		else if (cam_par[cam].lens.lens_type == 1 || cam_par[cam].lens.lens_type == 2) { // ptgui fisheye
			fcam[cam * 2 + 0] = 1.0f / (0.5f * cam_par[cam].lens.hfov * deg2rad);
			fcam[cam * 2 + 1] = 0.5f * cam_par[cam].lens.haw;
		}
		else if (cam_par[cam].lens.lens_type == 3) { // adobe rectilinear
			fcam[cam * 2 + 0] = 1.0f / tanf(0.5f * cam_par[cam].lens.hfov * deg2rad);
			fcam[cam * 2 + 1] = 0.5f * cam_par[cam].lens.haw;
		}
		else if (cam_par[cam].lens.lens_type == 4) { // adobe fisheye
			fcam[cam * 2 + 0] = 1.0f / (0.5f * cam_par[cam].lens.hfov * deg2rad);
			fcam[cam * 2 + 1] = 0.5f * cam_par[cam].lens.haw;
		}
		else{ // Unsupported Lens
			vxAddLogEntry((vx_reference)array_cam, VX_ERROR_INVALID_TYPE, "ERROR: initialize_stitch_remap: lens_type = %d not supported [cam#%d]\n", cam_par[cam].lens.lens_type, cam);
			return VX_ERROR_INVALID_TYPE;
		}
	}

	// compute warp pixel map
	float pi_by_h = (float)M_PI / (float)dst_height;
	float width2 = (float)width * 0.5f, height2 = (float)height * 0.5f;
	for (int y = 0; y < (int)dst_height; y++) {
		float pe = (float)y * pi_by_h - (float)M_PI_2;
		float sin_pe = sinf(pe);
		float cos_pe = cosf(pe);
		for (int x = 0; x < (int)dst_width; x++) {
			float te = (float)x * pi_by_h - (float)M_PI;
			float sin_te = sinf(te);
			float cos_te = cosf(te);
			float X[3] = { sin_te*cos_pe, sin_pe, cos_te*cos_pe };
			const float * T = Tcam, *M = Mcam, *f = fcam;
			float best_xd = -1, best_yd = -1, best_rd = 1e20f;
			int best_cam = -1;
			for (vx_uint32 cam = 0; cam < num_cam; cam++, T += 3, M += 9, f += 2) {
				const camera_params * par = &cam_par[cam];
				float Xt[3] = { X[0] - T[0], X[1] - T[1], X[2] - T[2] };
				float nfactor = sqrtf(Xt[0] * Xt[0] + Xt[1] * Xt[1] + Xt[2] * Xt[2]);
				Xt[0] /= nfactor;
				Xt[1] /= nfactor;
				Xt[2] /= nfactor;
				float Y[3];
				MatMul3x1(Y, M, Xt);
				if (Y[2] > 0.0f) {
					float ph = atan2f(Y[1], Y[0]);
					float th = asinf(sqrtf(Y[0] * Y[0] + Y[1] * Y[1]));
					float rd = 0.0f;
					if (par->lens.lens_type == 0) { // ptgui rectilinear
						float a = par->lens.k1;
						float b = par->lens.k2;
						float c = par->lens.k3;
						float d = 1.0f - a - b - c;
						float r = tanf(th) * f[0];
						rd = r * (d + r * (c + r * (b + r * a)));
					}
					else if (par->lens.lens_type == 1 || par->lens.lens_type == 2) { // ptgui fisheye circ
						float a = par->lens.k1;
						float b = par->lens.k2;
						float c = par->lens.k3;
						float d = 1.0f - a - b - c;
						float r = th * f[0];
						rd = r * (d + r * (c + r * (b + r * a)));
					}
					else if (par->lens.lens_type == 3) { // adobe rectilinear
						float r = tanf(th) * f[0], r2 = r * r;
						rd = r * (1 + r2 * (par->lens.k1 + r2 * (par->lens.k2 + r2 * par->lens.k3)));
					}
					else if (par->lens.lens_type == 4) { // adobe fisheye
						float r = th * f[0], r2 = r * r;
						rd = r * (1 + r2 * (par->lens.k1 + r2 * par->lens.k2));
					}

					float xd = f[1] * rd * cosf(ph);
					float yd = f[1] * rd * sinf(ph);
					float rr = sqrtf(xd*xd + yd*yd);
								
					float du0 = par->lens.du0;
					if (num_buff_cols > 1){	du0 += ((cam_buffer_width /(2 * num_buff_cols)) + ((cam % num_buff_cols) * src_width));}
					else{ du0 += width2;}
					
					xd = du0 + xd;
					yd = height2 + par->lens.dv0 + yd;
					if (xd >= 0 && xd < (cam_buffer_width - 1) && yd >= 0 && yd < height - 1 && (par->lens.r_crop <= 0.0f || rr <= par->lens.r_crop)) {
						if (best_cam < 0 || th < best_rd) {
							best_rd = th;
							best_xd = xd;
							best_yd = yd;
							best_cam = cam;
						}
					}
				}
			}
			if (best_cam >= 0) {
				int source_offset = (int)(best_cam / num_buff_cols) * height;
				ERROR_CHECK_STATUS(vxSetRemapPoint(remap, x, y, best_xd, best_yd + source_offset));
			}
			else {
				ERROR_CHECK_STATUS(vxSetRemapPoint(remap, x, y, -1.0f, -1.0f));
			}
		}
	}

	ERROR_CHECK_STATUS(vxCommitArrayRange(array_cam, 0, num_cam, cam_par));
	delete[] Mcam;
	delete[] Tcam;
	delete[] fcam;

	return VX_SUCCESS;
}

//! \brief The kernel publisher.
vx_status initialize_stitch_remap_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddUserKernel(context, "com.amd.loomsl.initialize_stitch_remap", AMDOVX_KERNEL_STITCHING_INITIALIZE_STITCH_REMAP, initialize_stitch_remap_kernel, 8, initialize_stitch_remap_validate, nullptr, nullptr);
	ERROR_CHECK_OBJECT(kernel);

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED)); // num_buff_rows
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED)); // num_buff_cols
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED)); // cam_buffer_width
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 3, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED)); // cam_buffer_height
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 4, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED)); // output_width (equirectangular)
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 5, VX_INPUT, VX_TYPE_MATRIX, VX_PARAMETER_STATE_REQUIRED)); // rig_params
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 6, VX_INPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED));  // camera_params[]
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 7, VX_OUTPUT, VX_TYPE_REMAP, VX_PARAMETER_STATE_REQUIRED)); // remap table

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}
