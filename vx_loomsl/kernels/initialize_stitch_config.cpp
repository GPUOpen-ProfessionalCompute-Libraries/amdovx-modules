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

//Developer Debug Variables
#define INITIALIZE_STITCH_DEBUG 0

//! \brief The local data to calculate the overlapping images.
#define MAX_CAM_OVERLAP 6
struct stitch_init_data {
	vx_uint8 Num_camera = 0;
	vx_uint8 Camera_ID[MAX_CAM_OVERLAP];
	float Z_value[MAX_CAM_OVERLAP];
};

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
static vx_status VX_CALLBACK initialize_stitch_config_validate(vx_node node, const vx_reference parameters[], vx_uint32 num, vx_meta_format metas[])
{
	// check input scalar types
	vx_enum types[5] = { VX_TYPE_INVALID, VX_TYPE_INVALID, VX_TYPE_INVALID, VX_TYPE_INVALID };
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[0], VX_SCALAR_ATTRIBUTE_TYPE, &types[0], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[1], VX_SCALAR_ATTRIBUTE_TYPE, &types[1], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[2], VX_SCALAR_ATTRIBUTE_TYPE, &types[2], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[3], VX_SCALAR_ATTRIBUTE_TYPE, &types[3], sizeof(vx_enum)));
	ERROR_CHECK_STATUS(vxQueryScalar((vx_scalar)parameters[4], VX_SCALAR_ATTRIBUTE_TYPE, &types[4], sizeof(vx_enum)));
	if (types[0] != VX_TYPE_UINT32 || types[1] != VX_TYPE_UINT32 || types[2] != VX_TYPE_UINT32 || types[3] != VX_TYPE_UINT32 || types[4] != VX_TYPE_UINT32)
	{
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: scalar type not valid\n");
		return status;
	}
	// read input scalar values
	vx_uint32 num_cam = 0, num_buff_rows = 0, num_buff_cols = 0, cam_buffer_width = 0, cam_buffer_height = 0, width_eqr = 0, height_eqr = 0;
	vx_uint32 width_scr = 0, height_src = 0;
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[0], &num_buff_rows));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[1], &num_buff_cols));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[2], &cam_buffer_width));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[3], &cam_buffer_height));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[4], &width_eqr));
	if (num_buff_rows < 1 || num_buff_cols < 1 || width_eqr < 1 || cam_buffer_width < 1 || cam_buffer_height < 1) {
		vx_status status = VX_ERROR_INVALID_VALUE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: scalar value not valid\n");
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
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: rig params matrix type/dimensions are not valid\n");
		return status;
	}

	// check camera config array dimensions
	type = VX_TYPE_INVALID;
	vx_size size = 0, num_items = 0;
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[6], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[6], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[6], VX_ARRAY_ATTRIBUTE_CAPACITY, &num_items, sizeof(num_items)));
	if (size != sizeof(camera_params) || (num_items != num_cam)) {
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: camera params array type/dimensions are not valid\n");
		return status;
	}

	// check Initialize Stitch Config attributes matrix dimensions
	if (parameters[7]){
		type = VX_TYPE_INVALID;
		columns = 0, rows = 0;
		ERROR_CHECK_STATUS(vxQueryMatrix((vx_matrix)parameters[7], VX_MATRIX_ATTRIBUTE_TYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxQueryMatrix((vx_matrix)parameters[7], VX_MATRIX_ATTRIBUTE_COLUMNS, &columns, sizeof(columns)));
		ERROR_CHECK_STATUS(vxQueryMatrix((vx_matrix)parameters[7], VX_MATRIX_ATTRIBUTE_ROWS, &rows, sizeof(rows)));
		vx_size CTAttr_size = (sizeof(InitializeStitchAttributes) / sizeof(vx_float32));
		if (type != VX_TYPE_FLOAT32 || rows != 1 || columns != CTAttr_size) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: InitializeStitchConfig Attriubutes matrix type/dimensions are not valid\n");
			return status;
		}
	}
	//////////////////////////////////////
	// output: check and set meta
	//////////////////////////////////////
	// valid pixel array
	vx_size capacity = vx_size(width_eqr * height_eqr * num_cam / 8);
	size = 0;
	type = VX_TYPE_INVALID;
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[8], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[8], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[8], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[8], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
	if (size != sizeof(StitchValidPixelEntry)) {
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: valid pixel array type is not valid\n");
		return status;
	}
	// warp remap array
	type = VX_TYPE_INVALID;
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[9], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
	ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[9], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[9], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[9], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
	if (size != sizeof(StitchWarpRemapEntry)) {
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: warp remap array type is not valid\n");
		return status;
	}
	// overlap pixel array
	if (parameters[10]) {
		capacity = vx_size(width_eqr * height_eqr * (num_cam * num_cam / 2) / (128 * 32));
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[10], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[10], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[10], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[10], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (size != sizeof(StitchOverlapPixelEntry)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: overlap pixel array type is not valid\n");
			return status;
		}
	}
	// overlap count matrix
	if (parameters[11]) {
		columns = num_cam, rows = num_cam;
		type = VX_TYPE_INT32;
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[11], VX_MATRIX_ATTRIBUTE_TYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[11], VX_MATRIX_ATTRIBUTE_COLUMNS, &columns, sizeof(columns)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[11], VX_MATRIX_ATTRIBUTE_ROWS, &rows, sizeof(rows)));
	}
	// warp and exposure comp images
	vx_uint32 twidth = width_eqr, theight = height_eqr * num_cam;
	vx_df_image_e tformat = VX_DF_IMAGE_RGBX;
	if (parameters[12]) {
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[12], VX_IMAGE_ATTRIBUTE_WIDTH, &twidth, sizeof(twidth)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[12], VX_IMAGE_ATTRIBUTE_HEIGHT, &theight, sizeof(theight)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[12], VX_IMAGE_ATTRIBUTE_FORMAT, &tformat, sizeof(tformat)));
	}
	if (parameters[13]) {
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[13], VX_IMAGE_ATTRIBUTE_WIDTH, &twidth, sizeof(twidth)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[13], VX_IMAGE_ATTRIBUTE_HEIGHT, &theight, sizeof(theight)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[13], VX_IMAGE_ATTRIBUTE_FORMAT, &tformat, sizeof(tformat)));
	}
	// weight image
	twidth = width_eqr;
	theight = height_eqr * num_cam;
	tformat = VX_DF_IMAGE_U8;
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[14], VX_IMAGE_ATTRIBUTE_WIDTH, &twidth, sizeof(twidth)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[14], VX_IMAGE_ATTRIBUTE_HEIGHT, &theight, sizeof(theight)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[14], VX_IMAGE_ATTRIBUTE_FORMAT, &tformat, sizeof(tformat)));
	// camera ID images
	twidth = width_eqr / 8;
	theight = height_eqr;
	tformat = VX_DF_IMAGE_U8;
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[15], VX_IMAGE_ATTRIBUTE_WIDTH, &twidth, sizeof(twidth)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[15], VX_IMAGE_ATTRIBUTE_HEIGHT, &theight, sizeof(theight)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[15], VX_IMAGE_ATTRIBUTE_FORMAT, &tformat, sizeof(tformat)));
	tformat = VX_DF_IMAGE_U16;
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[16], VX_IMAGE_ATTRIBUTE_WIDTH, &twidth, sizeof(twidth)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[16], VX_IMAGE_ATTRIBUTE_HEIGHT, &theight, sizeof(theight)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[16], VX_IMAGE_ATTRIBUTE_FORMAT, &tformat, sizeof(tformat)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[17], VX_IMAGE_ATTRIBUTE_WIDTH, &twidth, sizeof(twidth)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[17], VX_IMAGE_ATTRIBUTE_HEIGHT, &theight, sizeof(theight)));
	ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[17], VX_IMAGE_ATTRIBUTE_FORMAT, &tformat, sizeof(tformat)));
	// exp comp calc array
	if (parameters[18]) {
		capacity = vx_size(width_eqr * height_eqr * num_cam / (128 * 32));
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[18], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[18], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[18], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[18], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (size != sizeof(StitchExpCompCalcEntry)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: exp comp calc array type is not valid\n");
			return status;
		}
	}
	// mask image
	if (parameters[19]) {
		twidth = width_eqr;
		theight = height_eqr * num_cam;
		tformat = VX_DF_IMAGE_U8;
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[19], VX_IMAGE_ATTRIBUTE_WIDTH, &twidth, sizeof(twidth)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[19], VX_IMAGE_ATTRIBUTE_HEIGHT, &theight, sizeof(theight)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[19], VX_IMAGE_ATTRIBUTE_FORMAT, &tformat, sizeof(tformat)));
	}
	// overlap rectangles array
	if (parameters[20]) {
		capacity = vx_size(num_cam * num_cam);
		type = VX_TYPE_RECTANGLE;
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[20], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[20], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
	}
#if INITIALIZE_STITCH_DEBUG
	if (parameters[21])
	{
		SeamFindSizeInfo size_var_1;	vx_uint32 mode_1 = 2;
		vx_uint32 src_width = (cam_buffer_width / num_buff_cols);
		vx_uint32 src_height = (cam_buffer_height / num_buff_rows);
		ERROR_CHECK_STATUS(seamfind_accurate_utility(mode_1, num_cam, src_width, src_height, width_eqr, (vx_matrix)parameters[5], (vx_array)parameters[6], &size_var_1));
		printf("Valid:%d, weight:%d, Accum:%d, Pref:Info:%d, Path:%d\n", size_var_1.valid_entry, size_var_1.weight_entry, size_var_1.accum_entry, size_var_1.pref_entry, size_var_1.path_entry);
	}
#endif
	//get seamfind variable sizes from the utility functiion
	SeamFindSizeInfo size_var;	vx_uint32 mode = 0;
	// seamfind valid pixel array
	if (parameters[21]) {
		ERROR_CHECK_STATUS(seamfind_utility(mode, width_eqr, num_cam, &size_var));
		capacity = size_var.valid_entry;
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[21], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[21], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[21], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[21], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (size != sizeof(StitchSeamFindValidEntry)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: seamfind valid pixel array type is not valid\n");
			return status;
		}
	}
	// seamfind edge accumulate array.
	if (parameters[22]) {
		capacity = size_var.accum_entry;
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[22], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[22], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[22], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[22], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (size != sizeof(StitchSeamFindAccumEntry)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: seamfind edge accumulate array type is not valid\n");
			return status;
		}
	}
	// seamfind valid weight array
	if (parameters[23]) {
		capacity = size_var.weight_entry;
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[23], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[23], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[23], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[23], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (size != sizeof(StitchSeamFindWeightEntry)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: seamfind valid weight array type is not valid\n");
			return status;
		}
	}
	// seamfind preference array
	if (parameters[24]) {
		capacity = size_var.pref_entry;
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[24], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[24], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[24], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[24], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (size != sizeof(StitchSeamFindPreference)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: seamfind preference array type is not valid\n");
			return status;
		}
	}
	// seamfind Info array
	if (parameters[25]) {
		capacity = size_var.info_entry;
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[25], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[25], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[25], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[25], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		if (size != sizeof(StitchSeamFindInformation)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: seamfind Info array type is not valid\n");
			return status;
		}
	}
	// multiband blend
	if (parameters[26]) {
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[26], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
		type = VX_TYPE_INVALID;
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[26], VX_ARRAY_ATTRIBUTE_ITEMSIZE, &size, sizeof(size)));
		ERROR_CHECK_STATUS(vxQueryArray((vx_array)parameters[26], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		if (size != sizeof(StitchBlendValidEntry)) {
			vx_status status = VX_ERROR_INVALID_TYPE;
			vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: Blend offsets array type is not valid\n");
			return status;
		}
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[26], VX_ARRAY_ATTRIBUTE_ITEMTYPE, &type, sizeof(type)));
		ERROR_CHECK_STATUS(vxSetMetaFormatAttribute(metas[26], VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
	}

	return VX_SUCCESS;
}

//! \brief The kernel execution.
static vx_status VX_CALLBACK initialize_stitch_config_kernel(vx_node node, const vx_reference * parameters, vx_uint32 num)
{
	/***********************************************************************************************************************************
	Get Input Parameters - Variables 0, 1, 2, 3, 4, 5, & 6
	************************************************************************************************************************************/
	// get num cameras and image dimensions
	vx_uint32 num_buff_rows = 0, num_buff_cols = 0, cam_buffer_width = 0, cam_buffer_height = 0, width_eqr = 0, height_eqr = 0;
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[0], &num_buff_rows));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[1], &num_buff_cols));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[2], &cam_buffer_width));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[3], &cam_buffer_height));
	ERROR_CHECK_STATUS(vxReadScalarValue((vx_scalar)parameters[4], &width_eqr));
	height_eqr = width_eqr >> 1;
	int num_cam = num_buff_rows * num_buff_cols;
	int width = 0, height = 0;
	vx_uint32 src_width = 0, src_height = 0;
	vx_uint32 dst_width = 0, dst_height = 0;
	src_width = width = (vx_uint32)(cam_buffer_width / num_buff_cols);
	src_height = height = (vx_uint32)(cam_buffer_height / num_buff_rows);
	dst_width = width_eqr;
	dst_height = height_eqr;
	// get rig and camera parameters
	vx_matrix mat_rig = (vx_matrix)parameters[5];
	vx_array array_cam = (vx_array)parameters[6];
	rig_params rig_par;
	ERROR_CHECK_STATUS(vxReadMatrix(mat_rig, &rig_par));
	camera_params *cam_par = nullptr;
	vx_size stride_cam = sizeof(camera_params), num_items = 0;
	ERROR_CHECK_STATUS(vxQueryArray(array_cam, VX_ARRAY_ATTRIBUTE_NUMITEMS, &num_items, sizeof(num_items)));
	if (num_items != num_cam) {
		vx_status status = VX_ERROR_INVALID_TYPE;
		vxAddLogEntry((vx_reference)node, status, "ERROR: initialize_stitch_config: camera parameter dimensions are not valid\n");
		return status;
	}
	ERROR_CHECK_STATUS(vxAccessArrayRange(array_cam, 0, num_items, &stride_cam, (void **)&cam_par, VX_READ_ONLY));
	// get configuration parameters
	vx_uint32 numCamera = num_cam;
	vx_uint32 widthSrc = width, heightSrcCamera = height;
	vx_uint32 widthDst = width_eqr, heightDstCamera = height_eqr;
	/***********************************************************************************************************************************
	Get InitializeStitchConfig Prefrence - Variables 7 and set default settings for seamfind
	************************************************************************************************************************************/
	vx_int32 SEAM_FIND = 1, SEAM_FREQUENCY = 600, SEAM_STAGGER = 1, HORIZONTAL_SEAM_PRIORITY = 1, VERTICAL_SEAM_PRIORITY = 1, SEAM_QUALITY = 1, SEAM_LOCK = 0, SEAM_FLAG = 0;
	vx_uint32  OVERLAP_RECT = 0, MULTI_BAND = 0, NUM_BANDS = 4;
	#if !ENABLE_HORIZONTAL_SEAM 	
	HORIZONTAL_SEAM_PRIORITY = -1;	
	#endif
	#if !ENABLE_VERTICAL_SEAM 	
	VERTICAL_SEAM_PRIORITY = -1;
	#endif
	//Get Initialize Stitch Config Preference
	vx_matrix InitializeStitchConfigAttr_mat = (vx_matrix)parameters[7];
	if (InitializeStitchConfigAttr_mat)
	{
		InitializeStitchAttributes attr;
		ERROR_CHECK_STATUS(vxReadMatrix(InitializeStitchConfigAttr_mat, &attr));
		SEAM_STAGGER = (vx_uint32)attr.seam_stagger;
		if (attr.overlap_rectangle)			OVERLAP_RECT = (vx_uint32)attr.overlap_rectangle;
		if (!attr.seam_find)				SEAM_FIND = (vx_uint32)attr.seam_find;
		if (attr.seam_vertical_priority)	VERTICAL_SEAM_PRIORITY = (vx_uint32)attr.seam_vertical_priority;
		if (attr.seam_horizontal_priority)	HORIZONTAL_SEAM_PRIORITY = (vx_uint32)attr.seam_horizontal_priority;
		if (attr.seam_frequency)			SEAM_FREQUENCY = (vx_uint32)attr.seam_frequency;
		if (attr.seam_quality)				SEAM_QUALITY = (vx_uint32)attr.seam_quality;
		if (attr.multi_band)				MULTI_BAND = (vx_uint32)attr.multi_band;
		if (attr.num_bands)					NUM_BANDS = (vx_uint32)attr.num_bands;
	}
	//MultiBand Blend Replicate/Reflect
	vx_int32 padding_depth = 0, MODE_REPLICATE = 0, MODE_REFLECT = 0;
	std::vector<stitch_init_data> multiband_count_variable;
	if (MULTI_BAND)	{ padding_depth = (NUM_BANDS <= 4) ? 64 : 128; MODE_REFLECT = 1; multiband_count_variable.resize(widthDst*heightDstCamera); }

	//DEVELOPER DEBUG SUPPORT
#if INITIALIZE_STITCH_DEBUG
	//Developer Control with env variables
	char textBuffer[256];
	vx_uint32 CHANGE_OVERLAP = 0, OVERLAP_RECT_L = 0, OVERLAP_RECT_R = 0, OVERLAP_RECT_T = 0, OVERLAP_RECT_B = 0;

	if (StitchGetEnvironmentVariable("OVERLAP_RECT", textBuffer, sizeof(textBuffer))){ OVERLAP_RECT = (vx_uint32)atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("SEAM_FREQUENCY", textBuffer, sizeof(textBuffer))){ SEAM_FREQUENCY = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("SEAM_STAGGER", textBuffer, sizeof(textBuffer))){ SEAM_STAGGER = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("VERTICAL_SEAM_PRIORITY", textBuffer, sizeof(textBuffer))){ VERTICAL_SEAM_PRIORITY = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("HORIZONTAL_SEAM_PRIORITY", textBuffer, sizeof(textBuffer))){ HORIZONTAL_SEAM_PRIORITY = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("SEAM_QUALITY", textBuffer, sizeof(textBuffer))){ SEAM_QUALITY = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("SEAM_LOCK", textBuffer, sizeof(textBuffer))){ SEAM_LOCK = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("SEAM_FLAG", textBuffer, sizeof(textBuffer))){ SEAM_FLAG = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("CHANGE_OVERLAP", textBuffer, sizeof(textBuffer))){ CHANGE_OVERLAP = atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("OVERLAP_RECT_L", textBuffer, sizeof(textBuffer))){ OVERLAP_RECT_L = (vx_uint32)atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("OVERLAP_RECT_R", textBuffer, sizeof(textBuffer))){ OVERLAP_RECT_R = (vx_uint32)atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("OVERLAP_RECT_T", textBuffer, sizeof(textBuffer))){ OVERLAP_RECT_T = (vx_uint32)atoi(textBuffer); }
	if (StitchGetEnvironmentVariable("OVERLAP_RECT_B", textBuffer, sizeof(textBuffer))){ OVERLAP_RECT_B = (vx_uint32)atoi(textBuffer); }

	//TBD: log print
	printf("**********DEBUG**********\n");
	printf("Debug::NumCamera    -- %d\n", num_cam);
	printf("Debug::Input Camera -- Width:%d	Height:%d \n", widthSrc, heightSrcCamera);
	printf("Debug::Output Eqr   -- Width:%d	Height:%d \n", dst_width, heightDstCamera);
	printf("Debug::Rig Parameters -- Yaw:%.2f Pitch:%.2f Roll:%.2f D:%.2f\n", rig_par.yaw, rig_par.pitch, rig_par.roll, rig_par.d);
	for (int i = 0; i < num_cam; i++)
	{
		printf("Debug::Camera->%d\n", i);
		printf("	Yaw:%f Patch:%f Roll:%f Tx:%f Ty:%f Tz:%f\n", cam_par[i].focal.yaw, cam_par[i].focal.pitch, cam_par[i].focal.roll,
			cam_par[i].focal.tx, cam_par[i].focal.ty, cam_par[i].focal.tz);
		printf("	LensType:%d haw:%.2f hfov:%.2f k1:%f k2:%f k3:%f du0:%.2f dv0:%.2f r_crop:%.2f\n", cam_par[i].lens.lens_type, cam_par[i].lens.haw,
			cam_par[i].lens.hfov, cam_par[i].lens.k1, cam_par[i].lens.k2, cam_par[i].lens.k3, cam_par[i].lens.du0, cam_par[i].lens.dv0, cam_par[i].lens.r_crop);
	}
	printf("**************************\n");
#endif
	/***********************************************************************************************************************************
	Initialize Stitch Config Outputs
	************************************************************************************************************************************/
	//Size for Variables 8 & 9
	vx_size warp_size_arr = vx_size(ceil(((widthDst * heightDstCamera) * numCamera) / 8));;

	//Variables 8 & 9 mapped to a local vector datatype
	vx_array arr_stitchValidPixelEntry = (vx_array)parameters[8];
	std::vector<StitchValidPixelEntry> RemapDestination;
	RemapDestination.resize(warp_size_arr);

	vx_array arr_stitchWarpRemapEntry = (vx_array)parameters[9];
	std::vector<StitchWarpRemapEntry> RemapEntry;
	RemapEntry.resize(warp_size_arr);

	//Internal Data Variables - Overlap Calculate Data & Valid Region Rectangles
	std::vector<stitch_init_data> stitch_component;
	stitch_component.resize(widthDst*heightDstCamera);
	std::vector<vx_rectangle_t> Rectangle;
	Rectangle.resize(numCamera);

	//Calculate Simple MASK image - Variable 19 - Used in Seam Find - Optional Output
	vx_image mask_image = (vx_image)parameters[19];
	void *mask_image_ptr = NULL; vx_rectangle_t mask_rect; vx_imagepatch_addressing_t mask_addr;
	if (mask_image != NULL)
	{
		vx_int32 width = 0, height = 0;	vx_uint32 plane = 0;
		ERROR_CHECK_STATUS(vxQueryImage(mask_image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width)));
		ERROR_CHECK_STATUS(vxQueryImage(mask_image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height)));
		mask_rect.start_x = mask_rect.start_y = 0; mask_rect.end_x = width; mask_rect.end_y = height;
		ERROR_CHECK_STATUS(vxAccessImagePatch(mask_image, &mask_rect, plane, &mask_addr, &mask_image_ptr, VX_READ_AND_WRITE));
		memset(mask_image_ptr, 0, (width*height));
	}
	vx_uint8 *MASK_ptr = (vx_uint8*)mask_image_ptr;

	//Calculate ROI FOR EACH OVERLAP - Used in Seam Find - Optional Output
	std::vector<vx_rectangle_t> VX_Overlap_ROI;
	vx_uint32 max_roi = numCamera*numCamera;
	VX_Overlap_ROI.resize(max_roi);
	for (vx_uint32 i = 0; i < max_roi; i++)	{
		VX_Overlap_ROI[i].start_x = widthDst; VX_Overlap_ROI[i].end_x = 0;
		VX_Overlap_ROI[i].start_y = heightDstCamera; VX_Overlap_ROI[i].end_y = 0;
	}
	/***********************************************************************************************************************************
	Generate Warp tables for each Camera using warp and lens parameters
	************************************************************************************************************************************/
	// pre-compute M & T of each camera
	float * Mcam = new float[num_cam * 9];
	float * Tcam = new float[num_cam * 3];
	float * fcam = new float[num_cam * 2];
	float Mr[9];
	float deg2rad = (float)M_PI / 180.0f;
	ComputeM(Mr, rig_par.yaw * deg2rad, rig_par.pitch * deg2rad, rig_par.roll * deg2rad);

	for (int cam = 0; cam < num_cam; cam++)
	{
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
			if (cam_par[cam].lens.lens_type == 2 && cam_par[cam].lens.r_crop <= 0.0f)
				vxAddLogEntry((vx_reference)array_cam, VX_ERROR_INVALID_TYPE, "WARNING: initialize_stitch_config: Camera:%d -- Circular Fisheye Lens without r_crop=%0.2f is not fully supported.\n", cam, cam_par[cam].lens.r_crop);
		}
		else if (cam_par[cam].lens.lens_type == 3) { // adobe rectilinear
			fcam[cam * 2 + 0] = 1.0f / tanf(0.5f * cam_par[cam].lens.hfov * deg2rad);
			fcam[cam * 2 + 1] = 0.5f * cam_par[cam].lens.haw;
		}
		else if (cam_par[cam].lens.lens_type == 4) { // adobe fisheye
			fcam[cam * 2 + 0] = 1.0f / (0.5f * cam_par[cam].lens.hfov * deg2rad);
			fcam[cam * 2 + 1] = 0.5f * cam_par[cam].lens.haw;
		}
		else{ // unsupported lens
			vxAddLogEntry((vx_reference)array_cam, VX_ERROR_INVALID_TYPE, "ERROR: initialize_stitch_config: lens_type = %d not supported [cam#%d]\n", cam_par[cam].lens.lens_type, cam);
			return VX_ERROR_INVALID_TYPE;
		}
	}
	// compute warp pixel map
	float pi_by_h = (float)M_PI / (float)height_eqr;
	float width2 = (float)width * 0.5f, height2 = (float)height * 0.5f;
	const float * T = Tcam, *M = Mcam, *f = fcam;
	int  array_var_counter = 0; // optimized warp variable

	for (int cam = 0; cam < num_cam; cam++, T += 3, M += 9, f += 2)
	{
		vx_uint32 yw_offset = cam * heightDstCamera;
		int min_x = widthDst, max_x = 0, min_y = heightDstCamera, max_y = 0;
		int bit_counter = 0, empty_set = 0;
		int src_width_start = ((cam % num_buff_cols) * src_width);
		int src_width_end = (((cam % num_buff_cols) + 1) * src_width) - 1;
		int src_camera_center = (cam_buffer_width / (2 * num_buff_cols));

		for (int y = 0; y < (int)height_eqr; y++)
		{
			float pe = (float)y * pi_by_h - (float)M_PI_2;
			float sin_pe = sinf(pe);
			float cos_pe = cosf(pe);
			for (int x = 0; x < (int)width_eqr; x++)
			{
				bit_counter++;
				float xd, yd;

				float te = (float)x * pi_by_h - (float)M_PI;
				float sin_te = sinf(te);
				float cos_te = cosf(te);
				float X[3] = { sin_te*cos_pe, sin_pe, cos_te*cos_pe };

				const camera_params * par = &cam_par[cam];
				float Xt[3] = { X[0] - T[0], X[1] - T[1], X[2] - T[2] };
				float nfactor = sqrtf(Xt[0] * Xt[0] + Xt[1] * Xt[1] + Xt[2] * Xt[2]);
				Xt[0] /= nfactor;
				Xt[1] /= nfactor;
				Xt[2] /= nfactor;
				float Y[3];
				MatMul3x1(Y, M, Xt);

				if (Y[2] > 0.0f)
				{
					float ph = atan2f(Y[1], Y[0]);
					float th = asinf(sqrtf(Y[0] * Y[0] + Y[1] * Y[1]));
					float rd = 0.0f;
					if (par->lens.lens_type == 0)
					{ // ptgui rectilinear
						float a = par->lens.k1;
						float b = par->lens.k2;
						float c = par->lens.k3;
						float d = 1.0f - a - b - c;
						float r = tanf(th) * f[0];
						rd = r * (d + r * (c + r * (b + r * a)));
					}
					else if (par->lens.lens_type == 1 || par->lens.lens_type == 2)
					{ // ptgui fisheye circ
						float a = par->lens.k1;
						float b = par->lens.k2;
						float c = par->lens.k3;
						float d = 1.0f - a - b - c;
						float r = th * f[0];
						rd = r * (d + r * (c + r * (b + r * a)));
					}
					else if (par->lens.lens_type == 3)
					{ // adobe rectilinear
						float r = tanf(th) * f[0], r2 = r * r;
						rd = r * (1 + r2 * (par->lens.k1 + r2 * (par->lens.k2 + r2 * par->lens.k3)));
					}
					else if (par->lens.lens_type == 4)
					{ // adobe fisheye
						float r = th * f[0], r2 = r * r;
						rd = r * (1 + r2 * (par->lens.k1 + r2 * par->lens.k2));
					}
					xd = f[1] * rd * cosf(ph);
					yd = f[1] * rd * sinf(ph);
					float rr = sqrtf(xd*xd + yd*yd);

					float du0 = par->lens.du0;
					if (num_buff_cols > 1){ du0 += (src_camera_center + src_width_start); }
					else{ du0 += width2; }

					xd = du0 + xd;
					yd = height2 + par->lens.dv0 + yd;

					if (xd >= src_width_start && xd < src_width_end && yd >= 0 && yd < height - 1 && (par->lens.r_crop <= 0.0f || rr <= par->lens.r_crop))
					{
						if (x < min_x)	min_x = x;	if (x > max_x)	max_x = x;
						if (y < min_y)	min_y = y;	if (y > max_y)	max_y = y;

						int ID = (y * widthDst) + x;
						if (stitch_component[ID].Num_camera >= 5){
							vxAddLogEntry((vx_reference)mask_image, VX_ERROR_INVALID_VALUE, "ERROR: initialize_stitch_config: check camera parameters, camera overlaps greater than 5 not supported in this release\n");
							return VX_ERROR_INVALID_VALUE;
						}

						stitch_component[ID].Camera_ID[stitch_component[ID].Num_camera] = cam;
						stitch_component[ID].Z_value[stitch_component[ID].Num_camera] = Y[2];
						stitch_component[ID].Num_camera++;
						// if Relection/Replicate in Multiband Mode
						if (MULTI_BAND){
							multiband_count_variable[ID].Camera_ID[multiband_count_variable[ID].Num_camera] = cam;
							multiband_count_variable[ID].Z_value[multiband_count_variable[ID].Num_camera] = Y[2];
							multiband_count_variable[ID].Num_camera++;
						}
						// set mask image
						if (mask_image != NULL){
							int MASK_ID = ((y + yw_offset)*widthDst) + x;
							MASK_ptr[MASK_ID] = 255;
						}
					}

					else if (MODE_REFLECT && ((xd >= src_width_start - padding_depth) && (xd < src_width_end + padding_depth) && (yd >= -padding_depth && yd < height + padding_depth)) && (par->lens.r_crop <= 0.0f || rr <= par->lens.r_crop))
					{
						if (xd < src_width_start) xd = abs((src_width_start - xd) + src_width_start);
						else  if (ceil(xd) >= src_width_end) xd = src_width_end - (xd - src_width_end);

						if (yd < 0) yd = abs(yd);
						else if (ceil(yd) >= height) yd = (height - 1) - (yd - (height - 1));

						int ID = (y * widthDst) + x;
						multiband_count_variable[ID].Camera_ID[multiband_count_variable[ID].Num_camera] = cam;
						multiband_count_variable[ID].Z_value[multiband_count_variable[ID].Num_camera] = Y[2];
						multiband_count_variable[ID].Num_camera++;
					}
					else if (MODE_REPLICATE && (xd >= src_width_start - padding_depth && xd < src_width_end + padding_depth) && (yd >= -padding_depth && yd < height + padding_depth) && (par->lens.r_crop <= 0.0f || rr <= par->lens.r_crop))
					{
						if (xd < src_width_start) xd = (float)src_width_start;
						else  if (ceil(xd) >= src_width_end) xd = (float)src_width_end;

						if (yd < 0) yd = 0;
						else if (ceil(yd) >= height) yd = (float)(height - 1);

						int ID = (y * widthDst) + x;
						multiband_count_variable[ID].Camera_ID[multiband_count_variable[ID].Num_camera] = cam;
						multiband_count_variable[ID].Z_value[multiband_count_variable[ID].Num_camera] = Y[2];
						multiband_count_variable[ID].Num_camera++;
					}
					else
					{
						xd = -1; yd = -1;
					}
				}
				else { xd = -1; yd = -1; }
				/***********************************************************************************************************************************
				Enter the xd & yd values calulated into warp data structure in Q13.3 format
				************************************************************************************************************************************/
				vx_uint16 bit_var_x = (vx_uint16)xd;
				vx_uint16 bit_var_y = (vx_uint16)yd;

				if (xd >= 0 && xd < (cam_buffer_width + padding_depth) && yd >= 0 && yd < height + padding_depth)
				{
					//Q13.3 format
					bit_var_x = (bit_var_x << 3);
					bit_var_y = (bit_var_y << 3);
					//X fractional Value
					float rem_x = xd - int(xd);
					if (rem_x >= 0.125 && rem_x < 0.25) bit_var_x = bit_var_x | 0x0001;
					if (rem_x >= 0.25 && rem_x < 0.375) bit_var_x = bit_var_x | 0x0002;
					if (rem_x >= 0.375 && rem_x < 0.5)  bit_var_x = bit_var_x | 0x0003;
					if (rem_x >= 0.5 && rem_x < 0.625)  bit_var_x = bit_var_x | 0x0004;
					if (rem_x >= 0.625 && rem_x < 0.750)bit_var_x = bit_var_x | 0x0005;
					if (rem_x >= 0.750 && rem_x < 0.875)bit_var_x = bit_var_x | 0x0006;
					if (rem_x >= 0.875 && rem_x < 1.0)  bit_var_x = bit_var_x | 0x0007;
					//y fractional Value
					float rem_y = yd - int(yd);
					if (rem_y >= 0.125 && rem_y < 0.25) bit_var_y = bit_var_y | 0x0001;
					if (rem_y >= 0.25 && rem_y < 0.375) bit_var_y = bit_var_y | 0x0002;
					if (rem_y >= 0.375 && rem_y < 0.5)  bit_var_y = bit_var_y | 0x0003;
					if (rem_y >= 0.5 && rem_y < 0.625)  bit_var_y = bit_var_y | 0x0004;
					if (rem_y >= 0.625 && rem_y < 0.750)bit_var_y = bit_var_y | 0x0005;
					if (rem_y >= 0.750 && rem_y < 0.875)bit_var_y = bit_var_y | 0x0006;
					if (rem_y >= 0.875 && rem_y < 1.0)  bit_var_y = bit_var_y | 0x0007;
					//Check if there is a Valid Pixel in the set
					empty_set++;
				}

				if (bit_counter == 1)
				{
					RemapDestination[array_var_counter].camId = cam;
					RemapDestination[array_var_counter].reserved0 = 0;
					RemapDestination[array_var_counter].dstX = x >> 3;
					RemapDestination[array_var_counter].dstY = y;
					RemapDestination[array_var_counter].allValid = 1;
					RemapEntry[array_var_counter].srcX0 = bit_var_x;
					RemapEntry[array_var_counter].srcY0 = bit_var_y;
				}
				else if (bit_counter == 2)
				{
					RemapEntry[array_var_counter].srcX1 = bit_var_x;
					RemapEntry[array_var_counter].srcY1 = bit_var_y;
				}
				else if (bit_counter == 3)
				{
					RemapEntry[array_var_counter].srcX2 = bit_var_x;
					RemapEntry[array_var_counter].srcY2 = bit_var_y;
				}
				else if (bit_counter == 4)
				{
					RemapEntry[array_var_counter].srcX3 = bit_var_x;
					RemapEntry[array_var_counter].srcY3 = bit_var_y;
				}
				else if (bit_counter == 5)
				{
					RemapEntry[array_var_counter].srcX4 = bit_var_x;
					RemapEntry[array_var_counter].srcY4 = bit_var_y;
				}
				else if (bit_counter == 6)
				{
					RemapEntry[array_var_counter].srcX5 = bit_var_x;
					RemapEntry[array_var_counter].srcY5 = bit_var_y;
				}
				else if (bit_counter == 7)
				{
					RemapEntry[array_var_counter].srcX6 = bit_var_x;
					RemapEntry[array_var_counter].srcY6 = bit_var_y;
				}
				else if (bit_counter == 8)
				{
					RemapEntry[array_var_counter].srcX7 = bit_var_x;
					RemapEntry[array_var_counter].srcY7 = bit_var_y;

					bit_counter = 0;
					if (empty_set != 0)	array_var_counter++;
					empty_set = 0;
				}
			}
		}
		if (bit_counter != 0) array_var_counter++;

		//Setting Warp image ROI in the output Image
		Rectangle[cam].start_x = min_x;
		Rectangle[cam].start_y = min_y;
		Rectangle[cam].end_x = max_x;
		Rectangle[cam].end_y = max_y;
	}

	delete[] Mcam;
	delete[] Tcam;
	delete[] fcam;
	/***********************************************************************************************************************************
	Warp Kernel Variables - Variables 8 & 9
	************************************************************************************************************************************/
	//Remap Warp Entry Table Size Check 
	if (array_var_counter >= warp_size_arr)
	{
		vxAddLogEntry((vx_reference)arr_stitchWarpRemapEntry, VX_ERROR_INVALID_DIMENSION, "ERROR: initialize_stitch_config: Warp Entry has more Entries than Expected. Invalid Array Sizes for Parameters 8 & 9\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	//Remap Warp Entry Table 
	StitchWarpRemapEntry *Remap_ptr = &RemapEntry[0];
	ERROR_CHECK_STATUS(vxTruncateArray(arr_stitchWarpRemapEntry, 0));
	ERROR_CHECK_STATUS(vxAddArrayItems(arr_stitchWarpRemapEntry, array_var_counter, Remap_ptr, sizeof(StitchWarpRemapEntry)));
	//Remap Valid Pixel Table 
	StitchValidPixelEntry *Remap_valid_ptr = &RemapDestination[0];
	ERROR_CHECK_STATUS(vxTruncateArray(arr_stitchValidPixelEntry, 0));
	ERROR_CHECK_STATUS(vxAddArrayItems(arr_stitchValidPixelEntry, array_var_counter, Remap_valid_ptr, sizeof(StitchValidPixelEntry)));
	/***********************************************************************************************************************************
	Exposure Comp Variables - Variables 18 & 26
	************************************************************************************************************************************/
	vx_rectangle_t *Rectangle_ptr = &Rectangle[0];
	//Valid Pixel Array 
	vx_array arr_StitchExpCompCalcEntry = (vx_array)parameters[18];
	if (arr_StitchExpCompCalcEntry != NULL)
		ERROR_CHECK_STATUS(Compute_StitchExpCompCalcValidEntry(Rectangle_ptr, arr_StitchExpCompCalcEntry, numCamera, heightDstCamera));
	//Blend Offsets Array 
	vx_array arr_StitchBlendOffsets = (vx_array)parameters[26];
	if (arr_StitchBlendOffsets != NULL)
	{
		if (MULTI_BAND != 0)
			ERROR_CHECK_STATUS(Compute_StitchMultiBandCalcValidEntry(Rectangle_ptr, arr_StitchBlendOffsets, numCamera, NUM_BANDS, widthDst, heightDstCamera));
	}
	/***********************************************************************************************************************************
	Initial weight image & overlap matrix - Variables 11 & 14
	************************************************************************************************************************************/\
	vx_matrix pixel_count_mat = (vx_matrix)parameters[11];
	vx_size mat_size = numCamera * numCamera;
	int *pixel_matrix = new int[mat_size];
	memset(&pixel_matrix[0], 0, (mat_size * sizeof(int)));
	//Calculate Simple Cut image
	vx_image weight_image = (vx_image)parameters[14];
	void *weight_img_ptr = NULL; vx_rectangle_t rect; vx_imagepatch_addressing_t addr;
	if (weight_image != NULL)
	{
		vx_int32 width = 0, height = 0;	vx_uint32 plane = 0;
		ERROR_CHECK_STATUS(vxQueryImage(weight_image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width)));
		ERROR_CHECK_STATUS(vxQueryImage(weight_image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height)));
		rect.start_x = rect.start_y = 0; rect.end_x = width; rect.end_y = height;
		ERROR_CHECK_STATUS(vxAccessImagePatch(weight_image, &rect, plane, &addr, &weight_img_ptr, VX_READ_AND_WRITE));
		memset(weight_img_ptr, 0, (width*height));
	}
	vx_uint8 *weight_ptr = (vx_uint8*)weight_img_ptr;
	int weight_image_max_size = (int)(widthDst*heightDstCamera*numCamera);

	for (vx_uint32 ye = 0; ye < heightDstCamera; ye++)
	for (vx_uint32 xe = 0; xe < widthDst; xe++)
	{
		int ID = (ye*widthDst) + xe;
		vx_uint8 cam_id = -1;

		if (stitch_component[ID].Num_camera == 1)
		{
			cam_id = (vx_uint8)stitch_component[ID].Camera_ID[0];
			if (weight_image != NULL)
			{
				int ye_offset = cam_id * heightDstCamera;
				int ID_weight = ((ye + ye_offset)*widthDst) + xe;
				if (ID_weight >= weight_image_max_size){
					vxAddLogEntry((vx_reference)weight_image, VX_ERROR_INVALID_VALUE, "ERROR: initialize_stitch_config: check camera parameters, out of bound memory access\n");
					return VX_ERROR_INVALID_VALUE;
				}
				weight_ptr[ID_weight] = 255;
			}
		}
		else if (stitch_component[ID].Num_camera > 1)
		{
			float Z = -1;
			for (int i = 0; i < stitch_component[ID].Num_camera; i++)
			{
				for (int j = i + 1; j < stitch_component[ID].Num_camera; j++)
				{
					if (!MULTI_BAND)
					{
						// Calculate Overlap Matrix Variables 
						int index1 = (int)((stitch_component[ID].Camera_ID[i] * numCamera) + stitch_component[ID].Camera_ID[j]);
						int index2 = (int)((stitch_component[ID].Camera_ID[j] * numCamera) + stitch_component[ID].Camera_ID[i]);
						pixel_matrix[index1]++;
						pixel_matrix[index2]++;
					}
					else if (MULTI_BAND)
					{
						// Calculate Overlap Matrix Variables with reflection/replicate
						int index1 = (int)((multiband_count_variable[ID].Camera_ID[i] * numCamera) + multiband_count_variable[ID].Camera_ID[j]);
						int index2 = (int)((multiband_count_variable[ID].Camera_ID[j] * numCamera) + multiband_count_variable[ID].Camera_ID[i]);
						pixel_matrix[index1]++;
						pixel_matrix[index2]++;
					}
					// Calculate ROI ARRAY Variables 
					int cam1 = stitch_component[ID].Camera_ID[i];
					int cam2 = stitch_component[ID].Camera_ID[j];
					if ((cam1 >= (int)(numCamera)) || (cam2 >= (int)(numCamera))){
						vxAddLogEntry((vx_reference)weight_image, VX_ERROR_INVALID_VALUE, "ERROR: initialize_stitch_config: check camera parameters, out of bound memory access\n");
						return VX_ERROR_INVALID_VALUE;
					}

					int ROI_ID1 = (cam1 * numCamera) + cam2;
					int ROI_ID2 = (cam2 * numCamera) + cam1;
					if (xe < VX_Overlap_ROI[ROI_ID1].start_x){ VX_Overlap_ROI[ROI_ID1].start_x = xe; VX_Overlap_ROI[ROI_ID2].start_x = xe; }
					if (xe > VX_Overlap_ROI[ROI_ID1].end_x){ VX_Overlap_ROI[ROI_ID1].end_x = xe; VX_Overlap_ROI[ROI_ID2].end_x = xe; }
					if (ye < VX_Overlap_ROI[ROI_ID1].start_y){ VX_Overlap_ROI[ROI_ID1].start_y = ye; VX_Overlap_ROI[ROI_ID2].start_y = ye; }
					if (ye > VX_Overlap_ROI[ROI_ID1].end_y){ VX_Overlap_ROI[ROI_ID1].end_y = ye; VX_Overlap_ROI[ROI_ID2].end_y = ye; }
				}
				//Get the Camera ID for the nearest PIXEL 
				if (Z < stitch_component[ID].Z_value[i])
				{
					Z = stitch_component[ID].Z_value[i];
					cam_id = stitch_component[ID].Camera_ID[i];
				}
			}
			// set weight image
			if (weight_image != NULL)
			{
				int ye_offset = cam_id * heightDstCamera;
				int ID_weight = ((ye + ye_offset) * widthDst) + xe;
				if (ID_weight >= weight_image_max_size){
					vxAddLogEntry((vx_reference)weight_image, VX_ERROR_INVALID_VALUE, "ERROR: initialize_stitch_config: check camera parameters, out of bound memory access\n");
					return VX_ERROR_INVALID_VALUE;
				}
				weight_ptr[ID_weight] = 255;
			}
		}
	}

	/***********************************************************************************************************************************
	SeamFind Entry used to calculate - Variable 21, 22, 23, 24 & 25 -- Vertical Seam and Horizontal Seam
	************************************************************************************************************************************/
	if (SEAM_FIND)
	{
		SeamFindSizeInfo size_var;	vx_uint32 mode = 0;
		ERROR_CHECK_STATUS(seamfind_utility(mode, width_eqr, num_cam, &size_var));

		vx_array Array_SeamFindValidEntry = (vx_array)parameters[21];
		vx_size seamfind_valid_entry_count = 0;

		vx_array Array_SeamFindWeightEntry = (vx_array)parameters[23];
		std::vector<StitchSeamFindWeightEntry> Valid_Weight_Entry;
		Valid_Weight_Entry.resize(size_var.weight_entry);
		vx_size  seamfind_weight_entry_count = 0;

		vx_array Array_SeamFindPrefEntry = (vx_array)parameters[24];
		std::vector<StitchSeamFindPreference> Preference_Entry;
		Preference_Entry.resize(size_var.pref_entry);

		vx_array Array_SeamFindInfoEntry = (vx_array)parameters[25];
		std::vector<StitchSeamFindInformation> Information_Entry;
		Information_Entry.resize(size_var.info_entry);

		//seamfind data struct allocation
		if (Array_SeamFindValidEntry != NULL)
		{
			// Reduce/Resize Overlap Rectangles and count the number of overlaps
			vx_int16 num_vertical_overlap = 0, num_horizontal_overlap = 0;
			for (vx_uint32 i = 0; i < numCamera; i++)
			for (vx_uint32 j = i + 1; j < numCamera; j++)
			{
				vx_uint32 ID = (i * numCamera) + j;
				if (pixel_matrix[ID] != 0)
				{
					VX_Overlap_ROI[ID].start_y = VX_Overlap_ROI[ID].start_y + OVERLAP_RECT;
					VX_Overlap_ROI[ID].end_y = VX_Overlap_ROI[ID].end_y - OVERLAP_RECT;
					VX_Overlap_ROI[ID].start_x = VX_Overlap_ROI[ID].start_x + OVERLAP_RECT;
					VX_Overlap_ROI[ID].end_x = VX_Overlap_ROI[ID].end_x - OVERLAP_RECT;
#if INITIALIZE_STITCH_DEBUG
					if (CHANGE_OVERLAP)
					{
						VX_Overlap_ROI[ID].start_y = VX_Overlap_ROI[ID].start_y + OVERLAP_RECT_T;
						VX_Overlap_ROI[ID].end_y = VX_Overlap_ROI[ID].end_y - OVERLAP_RECT_B;
						VX_Overlap_ROI[ID].start_x = VX_Overlap_ROI[ID].start_x + OVERLAP_RECT_L;
						VX_Overlap_ROI[ID].end_x = VX_Overlap_ROI[ID].end_x - OVERLAP_RECT_R;
					}
#endif

					vx_int16 y_dir = VX_Overlap_ROI[ID].end_y - VX_Overlap_ROI[ID].start_y;
					vx_int16 x_dir = VX_Overlap_ROI[ID].end_x - VX_Overlap_ROI[ID].start_x;
					#if ENABLE_VERTICAL_SEAM
					if (y_dir >= x_dir){ num_vertical_overlap++; }
					else if (x_dir > y_dir && (cam_par[i].lens.lens_type == 2 && cam_par[j].lens.lens_type == 2) && (cam_par[i].lens.r_crop > 0 && cam_par[j].lens.r_crop > 0))
					{
						int flag_1 = -1, flag_2 = -1, start_x = -1, end_x = -1;
						vx_uint32 offset_1 = i * heightDstCamera;
						vx_uint32 offset_2 = j * heightDstCamera;
						int mid_y = VX_Overlap_ROI[ID].start_y + (y_dir / 2);
						for (vx_uint32 xe = VX_Overlap_ROI[ID].start_x; xe <= VX_Overlap_ROI[ID].end_x; xe++)
						{
							vx_uint32 pixel_id_1 = ((mid_y + offset_1) * widthDst) + xe;
							vx_uint32 pixel_id_2 = ((mid_y + offset_2) * widthDst) + xe;
							if (MASK_ptr[pixel_id_1] && MASK_ptr[pixel_id_2])
							{
								if (start_x == -1)start_x = xe;
								end_x = xe;
								if (weight_ptr[pixel_id_1] == 255)flag_1 = 1;
								if (weight_ptr[pixel_id_2] == 255)flag_2 = 1;
							}
							else if ((start_x != -1 && end_x != -1) && (flag_1 != -1 && flag_2 != -1))
							{
								VX_Overlap_ROI[ID].start_x = start_x;
								VX_Overlap_ROI[ID].end_x = end_x;
								break;
							}
							else{ flag_1 = -1; flag_2 = -1; start_x = -1; end_x = -1; }
						}
						if ((start_x != -1 && end_x != -1) && (flag_1 != -1 && flag_2 != -1))
						{
							VX_Overlap_ROI[ID].start_x = start_x;
							VX_Overlap_ROI[ID].end_x = end_x;
						}
					}
					#endif
					#if ENABLE_HORIZONTAL_SEAM
					if (x_dir > y_dir){ num_horizontal_overlap++; }
					#endif
				}
			}

			std::vector<StitchSeamFindValidEntry> Valid_Entry;
			Valid_Entry.resize(size_var.valid_entry);
			vx_int16 overlap_number = 0, horizontal_overlap = 0, vertical_overlap = 0;
			vx_uint32 accumulation_entry_offset = 0;

			for (vx_uint32 i = 0; i < numCamera; i++)
			for (vx_uint32 j = i + 1; j < numCamera; j++)
			{
				vx_uint32 ID = (i * numCamera) + j;
				if (pixel_matrix[ID] != 0)
				{
					vx_int16 y_dir = VX_Overlap_ROI[ID].end_y - VX_Overlap_ROI[ID].start_y;
					vx_int16 x_dir = VX_Overlap_ROI[ID].end_x - VX_Overlap_ROI[ID].start_x;
					vx_uint32 offset_1 = i * heightDstCamera;
					vx_uint32 offset_2 = j * heightDstCamera;

					if (y_dir >= x_dir)// vertical seam
					{
						vx_int16 SEAM_TYPE = VERTICAL_SEAM;
						#if ENABLE_VERTICAL_SEAM
						vx_uint32 ye = VX_Overlap_ROI[ID].start_y;
						vx_uint32 overlap_ye = VX_Overlap_ROI[ID].start_y + offset_2;

						for (vx_uint32 xe = VX_Overlap_ROI[ID].start_x; xe <= VX_Overlap_ROI[ID].end_x; xe++)
						{
							Valid_Entry[seamfind_valid_entry_count].dstX = (vx_int16)xe;
							Valid_Entry[seamfind_valid_entry_count].dstY = (vx_int16)ye;
							Valid_Entry[seamfind_valid_entry_count].height = (vx_int16)y_dir;
							Valid_Entry[seamfind_valid_entry_count].width = (vx_int16)x_dir;
							Valid_Entry[seamfind_valid_entry_count].OverLapX = (vx_int16)xe;
							Valid_Entry[seamfind_valid_entry_count].OverLapY = (vx_int16)overlap_ye;
							Valid_Entry[seamfind_valid_entry_count].CAMERA_ID_1 = (vx_int16)i;
							Valid_Entry[seamfind_valid_entry_count].ID = (vx_int16)overlap_number;

							seamfind_valid_entry_count++;
							//Entry Table Size Check 
							if (seamfind_valid_entry_count >= size_var.valid_entry){
								vxAddLogEntry((vx_reference)Array_SeamFindValidEntry, VX_ERROR_INVALID_DIMENSION, "ERROR: initialize_stitch_config: SeamFindValidEntry has more Entries than Expected. Invalid Array Size\n");
								return VX_ERROR_INVALID_DIMENSION;
							}
						}

						if (Array_SeamFindWeightEntry != NULL)
						{
							//SeamFind Set Weight Work Items
							for (vx_uint32 ye = VX_Overlap_ROI[ID].start_y; ye <= VX_Overlap_ROI[ID].end_y; ye++)
							for (vx_uint32 xe = VX_Overlap_ROI[ID].start_x; xe <= VX_Overlap_ROI[ID].end_x; xe++)
							{
								vx_uint32 pixel_id_1 = ((ye + offset_1) * widthDst) + xe;
								vx_uint32 pixel_id_2 = ((ye + offset_2) * widthDst) + xe;
								if (MASK_ptr[pixel_id_1] && MASK_ptr[pixel_id_2])
								{
									Valid_Weight_Entry[seamfind_weight_entry_count].x = xe;
									Valid_Weight_Entry[seamfind_weight_entry_count].y = ye;
									Valid_Weight_Entry[seamfind_weight_entry_count].cam_id_1 = i;
									Valid_Weight_Entry[seamfind_weight_entry_count].cam_id_2 = j;
									Valid_Weight_Entry[seamfind_weight_entry_count].overlap_id = overlap_number;
									Valid_Weight_Entry[seamfind_weight_entry_count].overlap_type = VERTICAL_SEAM;

									seamfind_weight_entry_count++;
									//Entry Table Size Check 
									if (seamfind_weight_entry_count >= size_var.weight_entry){
										vxAddLogEntry((vx_reference)Array_SeamFindWeightEntry, VX_ERROR_INVALID_DIMENSION, "ERROR: initialize_stitch_config: SeamFindWeightEntry has more Entries than Expected. Invalid Array Size\n");
										return VX_ERROR_INVALID_DIMENSION;
									}
								}
							}
						}
						#endif
						//Enter Seam Pref Array entry
						Preference_Entry[overlap_number].type = SEAM_TYPE;
						Preference_Entry[overlap_number].seam_type_num = vertical_overlap;
						Preference_Entry[overlap_number].start_frame = (SEAM_STAGGER * (num_horizontal_overlap + vertical_overlap));
						Preference_Entry[overlap_number].frequency = SEAM_FREQUENCY;
						Preference_Entry[overlap_number].quality = SEAM_QUALITY;
						Preference_Entry[overlap_number].priority = VERTICAL_SEAM_PRIORITY;
						Preference_Entry[overlap_number].seam_lock = SEAM_LOCK;
						Preference_Entry[overlap_number].scene_flag = SEAM_FLAG;

						//Enter Seam Info Array entry
						Information_Entry[overlap_number].cam_id_1 = i;
						Information_Entry[overlap_number].cam_id_2 = j;
						Information_Entry[overlap_number].start_x = VX_Overlap_ROI[ID].start_x;
						Information_Entry[overlap_number].end_x = VX_Overlap_ROI[ID].end_x;
						Information_Entry[overlap_number].start_y = VX_Overlap_ROI[ID].start_y;
						Information_Entry[overlap_number].end_y = VX_Overlap_ROI[ID].end_y;
						#if ENABLE_VERTICAL_SEAM
						Information_Entry[overlap_number].offset = accumulation_entry_offset;
						accumulation_entry_offset += ((x_dir + 1) * (y_dir + 1));
						#else
						Information_Entry[overlap_number].offset = 0;
						#endif
						vertical_overlap++;
					}
					else if (x_dir > y_dir && !(cam_par[i].lens.lens_type == 2 && cam_par[j].lens.lens_type == 2)) // horizontal seams for non-circular fisheye lens
					{
						vx_int16 SEAM_TYPE = HORIZONTAL_SEAM;
						#if ENABLE_HORIZONTAL_SEAM
						vx_uint32 xe = VX_Overlap_ROI[ID].start_x;
						for (vx_uint32 ye = VX_Overlap_ROI[ID].start_y; ye <= VX_Overlap_ROI[ID].end_y; ye++)
						{
							Valid_Entry[seamfind_valid_entry_count].dstX = (vx_int16)xe;
							Valid_Entry[seamfind_valid_entry_count].dstY = (vx_int16)ye;
							Valid_Entry[seamfind_valid_entry_count].height = (vx_int16)y_dir;
							Valid_Entry[seamfind_valid_entry_count].width = (vx_int16)x_dir;
							Valid_Entry[seamfind_valid_entry_count].OverLapX = (vx_int16)xe;
							Valid_Entry[seamfind_valid_entry_count].OverLapY = (vx_int16)(ye + offset_2);
							Valid_Entry[seamfind_valid_entry_count].CAMERA_ID_1 = (vx_int16)i;
							Valid_Entry[seamfind_valid_entry_count].ID = (vx_int16)overlap_number;

							seamfind_valid_entry_count++;
							//Entry Table Size Check 
							if (seamfind_valid_entry_count >= size_var.valid_entry){
								vxAddLogEntry((vx_reference)Array_SeamFindValidEntry, VX_ERROR_INVALID_DIMENSION, "ERROR: initialize_stitch_config: SeamFindValidEntry has more Entries than Expected. Invalid Array Size\n");
								return VX_ERROR_INVALID_DIMENSION;
							}
						}

						if (Array_SeamFindWeightEntry != NULL)
						{
							//SeamFind Set Weight Work Items
							for (vx_uint32 ye = VX_Overlap_ROI[ID].start_y; ye <= VX_Overlap_ROI[ID].end_y; ye++)
							for (vx_uint32 xe = VX_Overlap_ROI[ID].start_x; xe <= VX_Overlap_ROI[ID].end_x; xe++)
							{
								vx_uint32 pixel_id_1 = ((ye + offset_1) * widthDst) + xe;
								vx_uint32 pixel_id_2 = ((ye + offset_2) * widthDst) + xe;
								if (MASK_ptr[pixel_id_1] && MASK_ptr[pixel_id_2])
								{
									Valid_Weight_Entry[seamfind_weight_entry_count].x = xe;
									Valid_Weight_Entry[seamfind_weight_entry_count].y = ye;
									Valid_Weight_Entry[seamfind_weight_entry_count].cam_id_1 = i;
									Valid_Weight_Entry[seamfind_weight_entry_count].cam_id_2 = j;
									Valid_Weight_Entry[seamfind_weight_entry_count].overlap_id = overlap_number;
									Valid_Weight_Entry[seamfind_weight_entry_count].overlap_type = HORIZONTAL_SEAM;

									seamfind_weight_entry_count++;
									//Entry Table Size Check 
									if (seamfind_weight_entry_count >= size_var.weight_entry){
										vxAddLogEntry((vx_reference)Array_SeamFindWeightEntry, VX_ERROR_INVALID_DIMENSION, "ERROR: initialize_stitch_config: SeamFindWeightEntry has more Entries than Expected. Invalid Array Size\n");
										return VX_ERROR_INVALID_DIMENSION;
									}
								}
							}
						}
						#endif
						//Enter Seam Pref Array entry
						Preference_Entry[overlap_number].type = SEAM_TYPE;
						Preference_Entry[overlap_number].seam_type_num = horizontal_overlap;
						Preference_Entry[overlap_number].start_frame = (SEAM_STAGGER * horizontal_overlap);
						Preference_Entry[overlap_number].frequency = SEAM_FREQUENCY;
						Preference_Entry[overlap_number].quality = SEAM_QUALITY;
						Preference_Entry[overlap_number].priority = HORIZONTAL_SEAM_PRIORITY;
						Preference_Entry[overlap_number].seam_lock = SEAM_LOCK;
						Preference_Entry[overlap_number].scene_flag = SEAM_FLAG;

						//Enter Seam Info Array entry
						Information_Entry[overlap_number].cam_id_1 = i;
						Information_Entry[overlap_number].cam_id_2 = j;
						Information_Entry[overlap_number].start_x = VX_Overlap_ROI[ID].start_x;
						Information_Entry[overlap_number].end_x = VX_Overlap_ROI[ID].end_x;
						Information_Entry[overlap_number].start_y = VX_Overlap_ROI[ID].start_y;
						Information_Entry[overlap_number].end_y = VX_Overlap_ROI[ID].end_y;
						#if ENABLE_HORIZONTAL_SEAM
						Information_Entry[overlap_number].offset = accumulation_entry_offset;
						accumulation_entry_offset += ((x_dir + 1) * (y_dir + 1));
						#else
						Information_Entry[overlap_number].offset = 0;
						#endif
						horizontal_overlap++;
					}
					//Number of Overlaps
					overlap_number++;
					//Entry Table Size Check 
					if (overlap_number >= (int)size_var.info_entry || overlap_number >= (int)size_var.pref_entry){
						vxAddLogEntry((vx_reference)Array_SeamFindInfoEntry, VX_ERROR_INVALID_DIMENSION, "ERROR: initialize_stitch_config: SeamFindInfo/PrefEntry has more Entries than Expected. Invalid Array Size\n");
						return VX_ERROR_INVALID_DIMENSION;
					}
				}
			}

			//SeamFind Entry used to calculate variable 21
			if (Array_SeamFindValidEntry != NULL)
			{
				StitchSeamFindValidEntry *SeamFindValid_ptr = &Valid_Entry[0];
				ERROR_CHECK_STATUS(vxTruncateArray(Array_SeamFindValidEntry, 0));
				ERROR_CHECK_STATUS(vxAddArrayItems(Array_SeamFindValidEntry, seamfind_valid_entry_count, SeamFindValid_ptr, sizeof(StitchSeamFindValidEntry)));
			}
			//SeamFind Accum Entry set to -1 variable 22
			vx_array Array_SeamFindAccumEntry = (vx_array)parameters[22];
			if (Array_SeamFindAccumEntry != NULL)
			{
				//Entry Table Size Check 
				if (accumulation_entry_offset >= size_var.accum_entry){
					vxAddLogEntry((vx_reference)Array_SeamFindAccumEntry, VX_ERROR_INVALID_DIMENSION, "ERROR: initialize_stitch_config: SeamFindAccumEntry has more Entries than Expected. Invalid Array Size for parameter 22\n");
					return VX_ERROR_INVALID_DIMENSION;
				}
				std::vector<StitchSeamFindAccumEntry> Accum_Entry;
				Accum_Entry.resize(accumulation_entry_offset);
				int accum_data_size = (sizeof(StitchSeamFindAccumEntry) / sizeof(vx_int8));
				memset(&Accum_Entry[0], -1, (accumulation_entry_offset * accum_data_size));

				StitchSeamFindAccumEntry *SeamFindAccum_ptr = &Accum_Entry[0];
				ERROR_CHECK_STATUS(vxTruncateArray(Array_SeamFindAccumEntry, 0));
				if (accumulation_entry_offset)
					ERROR_CHECK_STATUS(vxAddArrayItems(Array_SeamFindAccumEntry, (vx_size)accumulation_entry_offset, SeamFindAccum_ptr, sizeof(StitchSeamFindAccumEntry)));
			}
			//SeamFind Weight Entry used to calculate variable 23
			if (Array_SeamFindWeightEntry != NULL)
			{
				StitchSeamFindWeightEntry *SeamFindWeight_ptr = &Valid_Weight_Entry[0];
				ERROR_CHECK_STATUS(vxTruncateArray(Array_SeamFindWeightEntry, 0));
				ERROR_CHECK_STATUS(vxAddArrayItems(Array_SeamFindWeightEntry, seamfind_weight_entry_count, SeamFindWeight_ptr, sizeof(StitchSeamFindWeightEntry)));
			}
			//SeamFind Pref used to calculate variable 24
			if (Array_SeamFindPrefEntry != NULL)
			{
				StitchSeamFindPreference *SeamFindPref_ptr = &Preference_Entry[0];
				ERROR_CHECK_STATUS(vxTruncateArray(Array_SeamFindPrefEntry, 0));
				ERROR_CHECK_STATUS(vxAddArrayItems(Array_SeamFindPrefEntry, overlap_number, SeamFindPref_ptr, sizeof(StitchSeamFindPreference)));
			}
			//SeamFind Pref used to calculate variable 25
			if (Array_SeamFindInfoEntry != NULL)
			{
				StitchSeamFindInformation *SeamFindInfo_ptr = &Information_Entry[0];
				ERROR_CHECK_STATUS(vxTruncateArray(Array_SeamFindInfoEntry, 0));
				ERROR_CHECK_STATUS(vxAddArrayItems(Array_SeamFindInfoEntry, overlap_number, SeamFindInfo_ptr, sizeof(StitchSeamFindInformation)));
			}
		}
	}
	/***********************************************************************************************************************************
	ROI ARRAY Variables - Variables  10 & 20
	************************************************************************************************************************************/
	vx_rectangle_t *Overlap_Rectangle_ptr = &VX_Overlap_ROI[0];

	//Rectangle Array used to calculate variable 10
	vx_array arr_stitchOverlapPixelEntry = (vx_array)parameters[10];
	if (arr_stitchOverlapPixelEntry != NULL)
	{
		ERROR_CHECK_STATUS(Compute_StitchExpCompCalcEntry(Overlap_Rectangle_ptr, arr_stitchOverlapPixelEntry, numCamera));
	}

	//Overlap Rectangle Array -- Variable 20
	vx_array arr_Overlap_ROI = (vx_array)parameters[20];
	if (arr_Overlap_ROI != NULL)
	{
		ERROR_CHECK_STATUS(vxTruncateArray(arr_Overlap_ROI, 0));
		ERROR_CHECK_STATUS(vxAddArrayItems(arr_Overlap_ROI, max_roi, Overlap_Rectangle_ptr, sizeof(vx_rectangle_t)));
	}
	/***********************************************************************************************************************************
	Weight Image, Mask Image and Overlap Pixel Count - commit access
	************************************************************************************************************************************/

	if (weight_image != NULL) ERROR_CHECK_STATUS(vxCommitImagePatch(weight_image, &rect, 0, &addr, weight_img_ptr));
	if (pixel_count_mat != NULL) ERROR_CHECK_STATUS(vxWriteMatrix(pixel_count_mat, pixel_matrix));
	if (mask_image != NULL)	ERROR_CHECK_STATUS(vxCommitImagePatch(mask_image, &mask_rect, 0, &mask_addr, mask_image_ptr));

	/***********************************************************************************************************************************
	Camera ID, group1 & group2 image outputs - Variables 15,16 & 17
	************************************************************************************************************************************/
	//Calculate Camera ID selection Image
	vx_image cam_id_image = (vx_image)parameters[15];
	void *cam_id_img_ptr = NULL; vx_rectangle_t cam_id_rect; vx_imagepatch_addressing_t cam_id_addr;

	//Calculate group1 ID selection Image
	vx_image group1_image = (vx_image)parameters[16];
	void *group1_ptr = NULL; vx_rectangle_t group1_rect; vx_imagepatch_addressing_t group1_addr;

	//Calculate group2 ID selection Image
	vx_image group2_image = (vx_image)parameters[17];
	void *group2_ptr = NULL; vx_rectangle_t group2_rect; vx_imagepatch_addressing_t group2_addr;

	if (cam_id_image != NULL && group1_image != NULL && group2_image != NULL)
	{
		vx_int32 width = 0, height = 0;	vx_uint32 plane = 0;
		ERROR_CHECK_STATUS(vxQueryImage(cam_id_image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width)));
		ERROR_CHECK_STATUS(vxQueryImage(cam_id_image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height)));

		cam_id_rect.start_x = cam_id_rect.start_y = 0; cam_id_rect.end_x = width; cam_id_rect.end_y = height;
		group1_rect.start_x = group1_rect.start_y = 0; group1_rect.end_x = width; group1_rect.end_y = height;
		group2_rect.start_x = group2_rect.start_y = 0; group2_rect.end_x = width; group2_rect.end_y = height;

		ERROR_CHECK_STATUS(vxAccessImagePatch(cam_id_image, &cam_id_rect, plane, &cam_id_addr, &cam_id_img_ptr, VX_READ_AND_WRITE));
		ERROR_CHECK_STATUS(vxAccessImagePatch(group1_image, &group1_rect, plane, &group1_addr, &group1_ptr, VX_READ_AND_WRITE));
		ERROR_CHECK_STATUS(vxAccessImagePatch(group2_image, &group2_rect, plane, &group2_addr, &group2_ptr, VX_READ_AND_WRITE));

		vx_uint8 *ptr0 = (vx_uint8*)cam_id_img_ptr;
		vx_uint16 *ptr1 = (vx_uint16*)group1_ptr;
		vx_uint16 *ptr2 = (vx_uint16*)group2_ptr;

		int camera_used[31], place_counter = 0;
		vx_uint32 X = 0, Y = 0;
		for (int c = 0; c < 31; c++) camera_used[c] = 0;

		for (vx_uint32 ye = 0; ye < heightDstCamera; ye++)
		for (vx_uint32 xe = 0; xe < widthDst; xe++)
		{
			place_counter++;
			int ID = (ye*widthDst) + xe;
			vx_uint8 cam_id = -1;

			if (place_counter == 1){ X = xe >> 3; Y = ye; }

			//If Multiband turned on use extended region
			if (!MULTI_BAND)
			{
				if (stitch_component[ID].Num_camera > 0)
				for (int i = 0; i < stitch_component[ID].Num_camera; i++)
				{
					cam_id = (vx_uint8)stitch_component[ID].Camera_ID[i];
					camera_used[cam_id] = 1;
				}
			}
			else if (MULTI_BAND)
			{
				if (multiband_count_variable[ID].Num_camera > 0)
				for (int i = 0; i < multiband_count_variable[ID].Num_camera; i++)
				{
					cam_id = (vx_uint8)multiband_count_variable[ID].Camera_ID[i];
					camera_used[cam_id] = 1;
				}
			}
			//Add the pixel information for every eight pixel
			if (place_counter == 8)
			{
				vx_uint8 CAM_COUNT = 0, single_camera = 31, pixel = 31;
				vx_uint16 group1 = -1, group2 = -1;

				for (int c = 0; c < 31; c++){ CAM_COUNT = CAM_COUNT + camera_used[c]; }

				if (CAM_COUNT == 1)
				for (int c = 0; c < 31; c++)
				if (camera_used[c] == 1)
					single_camera = c;
				//Enter the Camera ID for the single image
				if (CAM_COUNT == 1)	pixel = single_camera;
				else if (CAM_COUNT == 2)	pixel = 128;
				else if (CAM_COUNT == 3)	pixel = 129;
				else if (CAM_COUNT == 4)	pixel = 130;
				else if (CAM_COUNT == 5)	pixel = 131;
				else if (CAM_COUNT == 6)	pixel = 132;

				int ID_Image = (Y * width) + X;
				ptr0[ID_Image] = pixel;
				//Enter Group 1 and Group 2 image information when there are more than 1 overlap
				vx_uint16 camera_ids[MAX_CAM_OVERLAP], count = 0;
				for (int i = 0; i < MAX_CAM_OVERLAP; i++) camera_ids[i] = 31;

				if (CAM_COUNT > 1)
				{
					for (int c = 0; c < 31; c++)
					if (camera_used[c] == 1)
					{
						camera_ids[count] = c;
						count++;
					}
					group1 = (camera_ids[0] & 0x1F); group1 = group1 | ((camera_ids[1] & 0x1F) << 5); group1 = group1 | ((camera_ids[2] & 0x1F) << 10);
					group2 = (camera_ids[3] & 0x1F); group2 = group2 | ((camera_ids[4] & 0x1F) << 5); group2 = group2 | ((camera_ids[5] & 0x1F) << 10);
				}
				ptr1[ID_Image] = group1;
				ptr2[ID_Image] = group2;
				place_counter = 0;
				X = 0; Y = 0;
				memset(&camera_used[0], 0, (31 * sizeof(int)));
			}
		}
		ERROR_CHECK_STATUS(vxCommitImagePatch(cam_id_image, &cam_id_rect, 0, &cam_id_addr, cam_id_img_ptr));
		ERROR_CHECK_STATUS(vxCommitImagePatch(group1_image, &group1_rect, 0, &group1_addr, group1_ptr));
		ERROR_CHECK_STATUS(vxCommitImagePatch(group2_image, &group2_rect, 0, &group2_addr, group2_ptr));
	}
	/***********************************************************************************************************************************
	Initialize the RGBY images to (0,0,0,128) - Variables 12 & 13
	************************************************************************************************************************************/
	//RGBA1 intitialization Image
	vx_image RGBA1_image = (vx_image)parameters[12];
	void *RGBA1_ptr = NULL;	vx_rectangle_t RGBA1_rect;	vx_imagepatch_addressing_t RGBA1_addr;
	//RGBA2 intitialization Image
	vx_image RGBA2_image = (vx_image)parameters[13];

	if (RGBA1_image != NULL)
	{
		vx_int32 width = 0, height = 0;
		vx_uint32 plane = 0;
		ERROR_CHECK_STATUS(vxQueryImage(RGBA1_image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width)));
		ERROR_CHECK_STATUS(vxQueryImage(RGBA1_image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height)));

		RGBA1_rect.start_x = RGBA1_rect.start_y = 0; RGBA1_rect.end_x = width; RGBA1_rect.end_y = height;
		ERROR_CHECK_STATUS(vxAccessImagePatch(RGBA1_image, &RGBA1_rect, plane, &RGBA1_addr, &RGBA1_ptr, VX_READ_AND_WRITE));
		vx_uint32 *ptr1 = (vx_uint32 *)RGBA1_ptr;
		for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			int ID = (y*width) + x;
			ptr1[ID] = 0x80000000;
		}
		ERROR_CHECK_STATUS(vxCommitImagePatch(RGBA1_image, &RGBA1_rect, 0, &RGBA1_addr, RGBA1_ptr));
	}

	ERROR_CHECK_STATUS(vxCommitArrayRange(array_cam, 0, num_cam, cam_par));
	//Release Memory
	stitch_component.clear(); multiband_count_variable.clear();
	RemapDestination.clear(); RemapEntry.clear();
	VX_Overlap_ROI.clear();
	delete[] pixel_matrix;

	return VX_SUCCESS;
}

//! \brief The kernel publisher.
vx_status initialize_stitch_config_publish(vx_context context)
{
	// add kernel to the context with callbacks
	vx_kernel kernel = vxAddUserKernel(context, "com.amd.loomsl.initialize_stitch_config", AMDOVX_KERNEL_STITCHING_INITIALIZE_STITCH_CONFIG, initialize_stitch_config_kernel, 27, initialize_stitch_config_validate, nullptr, nullptr);
	ERROR_CHECK_OBJECT(kernel);

	// set kernel parameters
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 0, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 1, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 2, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 3, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 4, VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 5, VX_INPUT, VX_TYPE_MATRIX, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 6, VX_INPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 7, VX_INPUT, VX_TYPE_MATRIX, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 8, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 9, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 10, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 11, VX_OUTPUT, VX_TYPE_MATRIX, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 12, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 13, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 14, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 15, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 16, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 17, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 18, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 19, VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 20, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 21, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 22, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 23, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 24, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 25, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));
	ERROR_CHECK_STATUS(vxAddParameterToKernel(kernel, 26, VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL));

	// finalize and release kernel object
	ERROR_CHECK_STATUS(vxFinalizeKernel(kernel));
	ERROR_CHECK_STATUS(vxReleaseKernel(&kernel));

	return VX_SUCCESS;
}