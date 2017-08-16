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
#include "exp_comp.h"
#include "exposure_compensation.h"
#include <thread>
#define USE_GAMMA_CORRECTION		1
static const float Gamma = 2.2f;
static int g_Gamma2Linear[256];
static unsigned char g_Linear2Gamma[1024];


/////////////////////////////////////////////////////////////////////////////////////
//! \brief Exposure compensation C reference model. Not used in active stitching lib.
/////////////////////////////////////////////////////////////////////////////////////



inline vx_uint32 count_nz_mean_single(vx_uint32 *p, uint32_t stride, int width, int height, uint32_t *psum)
{
	vx_uint32 cnt = 0, sum = 0;
	for (int i = 0; i < height; i++){
		for (int j = 0; j < width; j++){
			if (p[j] != 0x80000000){
#if USE_LUMA_VALUES_FOR_GAIN
				sum += (p[j] >> 24), cnt++;
#else
				int r = (p[j] & 0xFF);
				int g = (p[j] & 0xFF00) >> 8;
				int b = (p[j] & 0xFF0000) >> 16;
				//sum += (vx_uint32)std::sqrt((r*r) + (g*g) + (b*b));
				sum += (vx_uint32)((r + g + b) / 3);
				cnt++;
#endif
			}
		}
		p += stride;
	}
	*psum += sum;
	return cnt;
}

inline vx_uint32 count_nz_mean_double(vx_uint32 *p, vx_uint32 *q, uint32_t stride, int width, int height, uint32_t *psum, uint32_t *qsum)
{
	vx_uint32 cnt = 0, sum1 = 0, sum2 = 0;
	for (int i = 0; i < height; i++){
		for (int j = 0; j < width; j++){
			if ((p[j] != 0x80000000) && (q[j] != 0x80000000)){
#if USE_LUMA_VALUES_FOR_GAIN
				sum1 += (p[j] >> 24), cnt++;
				sum2 += (q[j] >> 24);
#else
				int r = (p[j] & 0xFF);
				int g = (p[j] & 0xFF00) >> 8;
				int b = (p[j] & 0xFF0000) >> 16;
				sum1 += (vx_uint32)std::sqrt((r*r) + (g*g) + (b*b));
				//	sum1 += (vx_uint32)((r + g + b) / 3);
				r = (q[j] & 0xFF);
				g = (q[j] & 0xFF00) >> 8;
				b = (q[j] & 0xFF0000) >> 16;
				sum2 += (vx_uint32)std::sqrt((r*r) + (g*g) + (b*b));
				//	sum2 += (vx_uint32)((r + g + b) / 3);
				cnt++;
#endif

			}
		}
		p += stride;
		q += stride;
	}
	*psum += sum1;
	*qsum += sum2;
	return cnt;
}

inline vx_uint32 count_nz_mean_single_rgb(vx_uint32 *p, uint32_t stride, int width, int height, uint32_t *psum)
{
	vx_uint32 cnt = 0, sum1[3] = { 0 };
	for (int i = 0; i < height; i++){
		for (int j = 0; j < width; j++){
			if (p[j] != 0x80000000){
				int r = (p[j] & 0xFF);
				int g = (p[j] & 0xFF00) >> 8;
				int b = (p[j] & 0xFF0000) >> 16;
#if USE_GAMMA_CORRECTION
				sum1[0] += g_Gamma2Linear[r];
				sum1[1] += g_Gamma2Linear[g];
				sum1[2] += g_Gamma2Linear[b];
#else
				sum1[0] += r; sum1[1] += g; sum1[2] += b;
#endif
				cnt++;
			}
		}
		p += stride;
	}
	psum[0] += sum1[0];
	psum[1] += sum1[1];
	psum[2] += sum1[2];
	return cnt;
}

inline vx_uint32 count_nz_mean_double_rgb(vx_uint32 *p, vx_uint32 *q, uint32_t stride, int width, int height, uint32_t *psum, uint32_t *qsum)
{
	vx_uint32 cnt = 0, sum1[3] = { 0 }, sum2[3] = { 0 };
	for (int i = 0; i < height; i++){
		for (int j = 0; j < width; j++){
			if ((p[j] != 0x80000000) && (q[j] != 0x80000000)){
				int r = (p[j] & 0xFF);
				int g = (p[j] & 0xFF00) >> 8;
				int b = (p[j] & 0xFF0000) >> 16;
#if USE_GAMMA_CORRECTION
				sum1[0] += g_Gamma2Linear[r];
				sum1[1] += g_Gamma2Linear[g];
				sum1[2] += g_Gamma2Linear[b];
#else
				sum1[0] += r; sum1[1] += g; sum1[2] += b;
#endif
				r = (q[j] & 0xFF);
				g = (q[j] & 0xFF00) >> 8;
				b = (q[j] & 0xFF0000) >> 16;
#if USE_GAMMA_CORRECTION
				sum2[0] += g_Gamma2Linear[r];
				sum2[1] += g_Gamma2Linear[g];
				sum2[2] += g_Gamma2Linear[b];
#else
				sum2[0] += r; sum2[1] += g; sum2[2] += b;
#endif
				cnt++;

			}
		}
		p += stride;
		q += stride;
	}
	psum[0] += sum1[0];
	qsum[0] += sum2[0];
	psum[1] += sum1[1];
	qsum[1] += sum2[1];
	psum[2] += sum1[2];
	qsum[2] += sum2[2];
	return cnt;
}


inline vx_uint32 count_nz_mean_double_32x32(vx_uint32 *p, vx_uint32 *q, uint32_t stride, uint32_t *psum, uint32_t *qsum, uint32_t channel)
{
	vx_uint32 cnt = 0, sum1 = 0, sum2 = 0;
	float fsum1 = 0.0f, fsum2 = 0.0f;
	if (!channel){
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if ((p[j] != 0x80000000) && (q[j] != 0x80000000)){
					sum1 += (p[j] >> 24), cnt++;
					sum2 += (q[j] >> 24);
				}
			}
			p += stride;
			q += stride;
		}
	}
	else if (channel == 1){
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if ((p[j] != 0x80000000) && (q[j] != 0x80000000)){
					sum1 += (p[j] & 0xFF), cnt++;
					sum2 += (q[j] & 0xFF);
				}
			}
			p += stride;
			q += stride;
		}
	}
	else if (channel == 2){
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if ((p[j] != 0x80000000) && (q[j] != 0x80000000)){
					sum1 += (p[j] & 0xFF00) >> 8, cnt++;
					sum2 += (q[j] & 0xFF00) >> 8;
				}
			}
			p += stride;
			q += stride;
		}
	}
	else {
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if ((p[j] != 0x80000000) && (q[j] != 0x80000000)){
					sum1 += (p[j] & 0xFF0000) >> 16, cnt++;
					sum2 += (q[j] & 0xFF0000) >> 16;
				}
			}
			p += stride;
			q += stride;
		}
	}
	*psum += sum1;
	*qsum += sum2;
	return cnt;
}

inline vx_uint32 count_nz_mean_single_32x32(vx_uint32 *p, uint32_t stride, uint32_t *psum, uint32_t channel)
{
	vx_uint32 cnt = 0, sum1 = 0, sum2 = 0;
	float fsum1 = 0;
	if (!channel){
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if (p[j] != 0x80000000){
					sum1 += (p[j] >> 24), cnt++;
				}
			}
			p += stride;
		}
	}
	else if (channel == 1){
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if (p[j] != 0x80000000){
					sum1 += (p[j] & 0xFF), cnt++;
				}
			}
			p += stride;
		}
	}
	else if (channel == 2){
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if (p[j] != 0x80000000){
					sum1 += ((p[j] & 0xFF00) >> 8), cnt++;
				}
			}
			p += stride;
		}
	}
	else if (channel == 2){
		for (int i = 0; i < 32; i++){
			for (int j = 0; j < 32; j++){
				if (p[j] != 0x80000000){
					sum1 += ((p[j] & 0xFF0000) >> 16), cnt++;
				}
			}
			p += stride;
		}
	}
	*psum += sum1;
	return cnt;
}

inline uint8_t saturate_char(int n)
{
	return (n > 255) ? 255 : (n < 0) ? 0 : n;
}

CExpCompensator::CExpCompensator(int rows, int columns)
{
	m_NMat = nullptr;
	m_IMat = nullptr;
	m_AMat = nullptr;
	m_Gains = nullptr;
	m_block_gain_buf = nullptr;
	m_pblockgainInfo = nullptr;
	if (rows && columns){
		m_pIMat = new vx_uint32[rows*columns];
		m_pNMat = new vx_uint32[rows*columns];
	}
}

CExpCompensator::~CExpCompensator()
{
	if (m_pIMat) delete[] m_pIMat;
	if (m_pNMat) delete[] m_pNMat;
}

vx_status CExpCompensator::Initialize(vx_node node, vx_float32 alpha, vx_float32 beta, vx_array valid_roi, vx_image input, vx_image output, vx_array block_gains, vx_int32 channel)
{
	vx_uint32 i, blockgains_bufsize;
	vx_size capacity;
	vx_enum itemtype = VX_TYPE_INVALID;
	ERROR_CHECK_STATUS(vxQueryArray(valid_roi, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
	ERROR_CHECK_STATUS(vxQueryArray(valid_roi, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &itemtype, sizeof(itemtype)));
	if (!capacity) return VX_ERROR_INVALID_PARAMETERS;
	m_numImages = (vx_uint32)capacity;		// assuming the input array of overlaps
	if (input != nullptr){
		ERROR_CHECK_STATUS(vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &m_width, sizeof(m_width)));
		ERROR_CHECK_STATUS(vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &m_height, sizeof(m_height)));
	}
	m_valid_roi = valid_roi;
	m_height /= m_numImages;				//height has to be a multiple of individual eqr height
	m_alpha = alpha, m_beta = beta;
	m_InputImage = input, m_OutputImage = output;
	m_channel = channel;

	// initialize ROI based buffers
	vx_size		stride = 0;
	void		*base_array = 0;
	ERROR_CHECK_STATUS(vxAccessArrayRange(m_valid_roi, 0, capacity, &stride, (void **)&base_array, VX_READ_ONLY));

	// calulate the overlapped ROI matrix [numImages][numImages]	// assumed constant for all the frames. new parameters need new initialization
	vx_rectangle_t *rect_I, *rect_J;
	vx_int32 x1, y1, x2, y2;
	for (i = 0; i < m_numImages; i++){
		memset(&m_pRoi_rect[i][0], (int)-1, sizeof(vx_rectangle_t)*m_numImages);	// initialize to invalid
		rect_I = &vxArrayItem(vx_rectangle_t, base_array, i, stride);
		for (vx_uint32 j = i; j < m_numImages; j++)
		{
			rect_J = &vxArrayItem(vx_rectangle_t, base_array, j, stride);
			x1 = std::max(rect_I->start_x, rect_J->start_x);
			y1 = std::max(rect_I->start_y, rect_J->start_y);
			x2 = std::min(rect_I->end_x, rect_J->end_x);
			y2 = std::min(rect_I->end_y, rect_J->end_y);
			if ((x1 < x2) && (y1 < y2))	// intersect is true
			{
				m_pRoi_rect[i][j].start_x = m_pRoi_rect[j][i].start_x = (vx_uint32)x1;
				m_pRoi_rect[i][j].end_x = m_pRoi_rect[j][i].end_x = (vx_uint32)x2;
				m_pRoi_rect[i][j].start_y = m_pRoi_rect[j][i].start_y = (vx_uint32)y1;
				m_pRoi_rect[i][j].end_y = m_pRoi_rect[j][i].end_y = (vx_uint32)y2;
			}
		}
		memcpy(&mValidRect[i], rect_I, sizeof(vx_rectangle_t));
	}
	ERROR_CHECK_STATUS(vxCommitArrayRange(m_valid_roi, 0, capacity, base_array));

	if (block_gains){
		m_blockgainsStride = (m_width + 31) >> 5;
		blockgains_bufsize = m_blockgainsStride*((m_height + 31) >> 5);
		m_block_gain_buf = new vx_float32[blockgains_bufsize*m_numImages];
		for (i = 0; i < blockgains_bufsize*m_numImages; i++)
			m_block_gain_buf[i] = 1.0f;
		m_pblockgainInfo = new block_gain_info[blockgains_bufsize];
		memset(m_pblockgainInfo, 0, sizeof(block_gain_info)*blockgains_bufsize);
	}

	// allocate N, I A and B arrays
	m_NMat = new vx_uint32*[m_numImages];
	m_IMat = new vx_float32*[m_numImages];
	m_IMatG = new vx_float32*[m_numImages];
	m_IMatB = new vx_float32*[m_numImages];
	m_AMat = new vx_float64*[m_numImages];
	m_Gains = new vx_float32[m_numImages];
	m_GainsG = new vx_float32[m_numImages];
	m_GainsB = new vx_float32[m_numImages];
	for (i = 0; i < m_numImages; i++){
		m_NMat[i] = new vx_uint32[m_numImages];
		m_IMat[i] = new vx_float32[m_numImages];
		m_IMatG[i] = new vx_float32[m_numImages];
		m_IMatB[i] = new vx_float32[m_numImages];
		m_AMat[i] = new vx_float64[m_numImages + 1];	// enough for the augmented matrix [a|b]
		memset(&m_AMat[i][0], 0, (m_numImages + 1)*sizeof(vx_float64));
	}
	memset(&m_Gains[0], 0x00000001, m_numImages*sizeof(vx_float32));
	m_node = node;
	ERROR_CHECK_STATUS(vxReleaseArray((vx_array *)&valid_roi));
	ERROR_CHECK_STATUS(vxReleaseImage((vx_image *)&input));
	m_bUseRGBgains = 0;
	for (i = 0; i < 256; i++) {
		g_Gamma2Linear[i] = (int)(255.0 * powf(i / 255.0f, Gamma));
	}
	for (i = 0; i < 1024; i++) {
		float val = (255.75f*powf(i / 1023.0f, 1 / Gamma));
		if (val>255.0) val = 255.0;
		g_Linear2Gamma[i] = (unsigned char)val;
	}

	return VX_SUCCESS;

}

vx_status CExpCompensator::DeInitialize()
{
	// free all allocated buffers
	for (int i = 0; i < (int)m_numImages; i++)
	{
		if (m_NMat[i]) delete[] m_NMat[i];
		if (m_IMat[i]) delete[] m_IMat[i];
		if (m_AMat[i]) delete[] m_AMat[i];
		if (m_IMatG[i]) delete[] m_IMatG[i];
		if (m_IMatB[i]) delete[] m_IMatB[i];
	}
	if (m_block_gain_buf) delete[] m_block_gain_buf;
	if (m_pblockgainInfo) delete[] m_pblockgainInfo;
	delete[] m_NMat;
	delete[] m_IMat;
	delete[] m_AMat;
	delete[] m_IMatG;
	delete[] m_IMatB;
	delete[] m_Gains;
	delete[] m_GainsG;
	delete[] m_GainsB;
	return VX_SUCCESS;
}

vx_status CExpCompensator::Process()
{
	if (m_channel >> 8)
		return CompensateGainsRGB((m_channel & 0xFF));
	else
		return CompensateGains();
}

vx_status CExpCompensator::ProcessBlockGains(vx_array ArrBlkGains)
{
	CompensateBlockGains();
	vx_uint32 blockgains_bufsize = m_blockgainsStride*((m_height + 31) >> 5);
	ERROR_CHECK_STATUS(vxTruncateArray(ArrBlkGains, 0));
	ERROR_CHECK_STATUS(vxAddArrayItems(ArrBlkGains, blockgains_bufsize*m_numImages, m_block_gain_buf, sizeof(vx_float32)));
	return VX_SUCCESS;
}


// CPU based implementation for calculating image based gains
vx_status CExpCompensator::CompensateGains()
{
	vx_status status;
	int i;

	// Access full images for reading
	vx_imagepatch_addressing_t addr = { 0 };
	vx_rectangle_t rect;
	rect.start_x = 0;
	rect.start_y = 0;
	rect.end_x = m_width;
	rect.end_y = m_height*m_numImages;
	vx_uint8 * base_ptr = nullptr;
	ERROR_CHECK_STATUS(vxAccessImagePatch(m_InputImage, &rect, 0, &addr, (void **)&base_ptr, VX_READ_ONLY));
	// assume stride_x and stride_y are constant
	m_stride = addr.stride_y;
	m_stride_x = addr.stride_x;

	vx_uint32 nz, ISum, JSum;
	for (i = 0; i < (int)m_numImages; i++){
		m_IMat[i][i] = 0.0f; m_NMat[i][i] = 0;
		for (int j = i; j < (int)m_numImages; j++){
			ISum = 0, JSum = 0, nz = 0;
			if (m_pRoi_rect[i][j].start_x != -1)	{ // if intersect
				// find the i,j intersect rect
				if (i == j){
					vx_uint32 *pI = (vx_uint32 *)(base_ptr + (m_height*i + m_pRoi_rect[i][j].start_y)*m_stride + (m_pRoi_rect[i][j].start_x*m_stride_x));
					nz += count_nz_mean_single(pI, (m_stride >> 2), (m_pRoi_rect[i][j].end_x - m_pRoi_rect[i][j].start_x), (m_pRoi_rect[i][j].end_y - m_pRoi_rect[i][j].start_y), &ISum);
				}
				else
				{
					vx_uint32 *pI = (vx_uint32 *)(base_ptr + (m_height*i + m_pRoi_rect[i][j].start_y)*m_stride + (m_pRoi_rect[i][j].start_x*m_stride_x));
					vx_uint32 *pJ = (vx_uint32 *)(base_ptr + (m_height*j + m_pRoi_rect[i][j].start_y)*m_stride + (m_pRoi_rect[i][j].start_x*m_stride_x));
					nz += count_nz_mean_double(pI, pJ, (m_stride >> 2), (m_pRoi_rect[i][j].end_x - m_pRoi_rect[i][j].start_x), (m_pRoi_rect[i][j].end_y - m_pRoi_rect[i][j].start_y), &ISum, &JSum);
				}
			}
			nz = std::max(nz, (vx_uint32)1);
			if (i == j){
				m_IMat[i][j] = (float)ISum / nz;
				m_NMat[i][j] = nz;
			}
			else
			{
				m_IMat[i][j] = (float)ISum / nz;
				m_IMat[j][i] = (float)JSum / nz;
				m_NMat[i][j] = m_NMat[j][i] = nz;
			}
		}
	}

	// generate augmented matrix[A/b] for solving gains
	for (int i = 0; i < (int)m_numImages; i++){
		for (int j = 0; j < (int)m_numImages; ++j) {
			m_AMat[i][m_numImages] += m_beta * m_NMat[i][j];		// b matrix
			m_AMat[i][i] += m_beta * m_NMat[i][j];
			if (j == i)			continue;
			m_AMat[i][i] += 2 * m_alpha * m_IMat[i][j] * m_IMat[i][j] * m_NMat[i][j];
			m_AMat[i][j] -= 2 * m_alpha * m_IMat[i][j] * m_IMat[j][i] * m_NMat[i][j];
		}
	}

	//solve the linear equation A*gains_ = B
	solve_gauss(m_AMat, m_Gains, m_numImages);
	// Apply gains to all images
	status = ApplyGains(base_ptr);
	// commit image patch
	if ((status = vxCommitImagePatch(m_InputImage, &rect, 0, &addr, (void *)base_ptr) != VX_SUCCESS)) {
		vxAddLogEntry((vx_reference)m_node, VX_FAILURE, "ERROR Decoder Node: vxCommitImagePatch(READ) failed, status = %d\n", status);
		return VX_FAILURE;
	}
	return VX_SUCCESS;
}

vx_status CExpCompensator::CompensateGainsRGB(vx_int32 ref_img)
{
	vx_status status;
	int i;

	// Access full images for reading
	vx_imagepatch_addressing_t addr = { 0 };
	vx_rectangle_t rect;
	rect.start_x = 0;
	rect.start_y = 0;
	rect.end_x = m_width;
	rect.end_y = m_height*m_numImages;
	vx_uint8 * base_ptr = nullptr;
	ERROR_CHECK_STATUS(vxAccessImagePatch(m_InputImage, &rect, 0, &addr, (void **)&base_ptr, VX_READ_ONLY));
	// assume stride_x and stride_y are constant
	m_stride = addr.stride_y;
	m_stride_x = addr.stride_x;
	m_bUseRGBgains = 1;

	vx_uint32 nz, ISum[3], JSum[3];
	for (i = 0; i < (int)m_numImages; i++){
		m_IMat[i][i] = 0.0f; m_NMat[i][i] = 0;
		m_IMatG[i][i] = 0.0f; m_IMatB[i][i] = 0.0f;
		for (int j = i; j < (int)m_numImages; j++){
			ISum[0] = ISum[1] = ISum[2] = 0; 
			JSum[0] = JSum[1] = JSum[2] = 0; nz = 0;
			if (m_pRoi_rect[i][j].start_x != -1)	{ // if intersect
				// find the i,j intersect rect
				if (i == j){
					vx_uint32 *pI = (vx_uint32 *)(base_ptr + (m_height*i + m_pRoi_rect[i][j].start_y)*m_stride + (m_pRoi_rect[i][j].start_x*m_stride_x));
					nz += count_nz_mean_single_rgb(pI, (m_stride >> 2), (m_pRoi_rect[i][j].end_x - m_pRoi_rect[i][j].start_x), (m_pRoi_rect[i][j].end_y - m_pRoi_rect[i][j].start_y), ISum);
				}
				else
				{
					vx_uint32 *pI = (vx_uint32 *)(base_ptr + (m_height*i + m_pRoi_rect[i][j].start_y)*m_stride + (m_pRoi_rect[i][j].start_x*m_stride_x));
					vx_uint32 *pJ = (vx_uint32 *)(base_ptr + (m_height*j + m_pRoi_rect[i][j].start_y)*m_stride + (m_pRoi_rect[i][j].start_x*m_stride_x));
					nz += count_nz_mean_double_rgb(pI, pJ, (m_stride >> 2), (m_pRoi_rect[i][j].end_x - m_pRoi_rect[i][j].start_x), (m_pRoi_rect[i][j].end_y - m_pRoi_rect[i][j].start_y), ISum, JSum);
				}
			}
			nz = std::max(nz, (vx_uint32)1);
			if (i == j){
				m_IMat[i][j] = (float)ISum[0] / nz;
				m_IMatG[i][j] = (float)ISum[1] / nz;
				m_IMatB[i][j] = (float)ISum[2] / nz;
				m_NMat[i][j] = nz;
			}
			else
			{
				m_IMat[i][j] = (float)ISum[0] / nz;
				m_IMat[j][i] = (float)JSum[0] / nz;
				m_IMatG[i][j] = (float)ISum[1] / nz;
				m_IMatG[j][i] = (float)JSum[1] / nz;
				m_IMatB[i][j] = (float)ISum[2] / nz;
				m_IMatB[j][i] = (float)JSum[2] / nz;
				m_NMat[i][j] = m_NMat[j][i] = nz;
				//printf("ImageNum: %d CR[%d]: %f CG:%f CB: %f\n", i, j, (float)JSum[0] / ISum[0], (float)JSum[1] / ISum[1], (float)JSum[2] / ISum[2]);
			}
		}
	}
#if 0
	// todo:: calc standard deviation for average 
	float mean_r = 0, mean_g=0, mean_b=0;
	int num = 0;
	for (int i = 0; i < (int)m_numImages; i++){
		if (m_NMat[i][i]) {
			mean_r += m_IMat[i][i];
			mean_g += m_IMatG[i][i];
			mean_b += m_IMatB[i][i];
			num++;
		}
	}
	if (num) {
		mean_r /= num; mean_g /= num; mean_b /= num;
	}
	float sd_r = 0, sd_g = 0, sd_b = 0;
	for (int i = 0; i < (int)m_numImages; i++){
		if (m_NMat[i][i]) {
			float diff = (m_IMat[i][i] - mean_r);
			sd_r += diff*diff;
			diff = (m_IMatG[i][i] - mean_g);
			sd_g += diff*diff;
			diff = (m_IMatB[i][i] - mean_b);
			sd_b += diff*diff;
		}
	}
	if (num) {
		sd_r /= num; sd_g /= num; sd_b /= num;
	}
#endif
	// generate augmented matrix[A/b] for solving gains
	for (int i = 0; i < (int)m_numImages; i++){
		for (int j = 0; j < (int)m_numImages; ++j) {
			m_AMat[i][m_numImages] += m_beta * m_NMat[i][j];		// b matrix
			m_AMat[i][i] += m_beta * m_NMat[i][j];
			if (j == i)			continue;
			m_AMat[i][i] += 2 * m_alpha * m_IMat[i][j] * m_IMat[i][j] * m_NMat[i][j];
			m_AMat[i][j] -= 2 * m_alpha * m_IMat[i][j] * m_IMat[j][i] * m_NMat[i][j];
		}
	}
	//solve the linear equation A*gains_ = B
	solve_gauss(m_AMat, m_Gains, m_numImages);

	// generate augmented matrix[A/b] for solving gains for G channel
	for (i = 0; i < (int)m_numImages; i++){
		memset(&m_AMat[i][0], 0, (m_numImages + 1)*sizeof(vx_float64));
	}

	for (int i = 0; i < (int)m_numImages; i++){
		for (int j = 0; j < (int)m_numImages; ++j) {
			m_AMat[i][m_numImages] += m_beta * m_NMat[i][j];		// b matrix
			m_AMat[i][i] += m_beta * m_NMat[i][j];
			if (j == i)			continue;
			m_AMat[i][i] += 2 * m_alpha * m_IMatG[i][j] * m_IMatG[i][j] * m_NMat[i][j];
			m_AMat[i][j] -= 2 * m_alpha * m_IMatG[i][j] * m_IMatG[j][i] * m_NMat[i][j];
		}
	}
	//solve the linear equation A*gains_ = B
	solve_gauss(m_AMat, m_GainsG, m_numImages);

	for (i = 0; i < (int)m_numImages; i++){
		memset(&m_AMat[i][0], 0, (m_numImages + 1)*sizeof(vx_float64));
	}
	// generate augmented matrix[A/b] for solving gains for B channel
	for (int i = 0; i < (int)m_numImages; i++){
		for (int j = 0; j < (int)m_numImages; ++j) {
			m_AMat[i][m_numImages] += m_beta * m_NMat[i][j];		// b matrix
			m_AMat[i][i] += m_beta * m_NMat[i][j];
			if (j == i)			continue;
			m_AMat[i][i] += 2 * m_alpha * m_IMatB[i][j] * m_IMatB[i][j] * m_NMat[i][j];
			m_AMat[i][j] -= 2 * m_alpha * m_IMatB[i][j] * m_IMatB[j][i] * m_NMat[i][j];
		}
	}

	//solve the linear equation A*gains_ = B
	solve_gauss(m_AMat, m_GainsB, m_numImages);
	// Apply gains to all images
	status = ApplyGains(base_ptr);
	// commit image patch
	if ((status = vxCommitImagePatch(m_InputImage, &rect, 0, &addr, (void *)base_ptr) != VX_SUCCESS)) {
		vxAddLogEntry((vx_reference)m_node, VX_FAILURE, "ERROR Decoder Node: vxCommitImagePatch(READ) failed, status = %d\n", status);
		return VX_FAILURE;
	}
	return VX_SUCCESS;
}


// CPU based implementation for calculating block based gains
vx_status CExpCompensator::CompensateBlockGains()
{
	vx_status status;
	int i;
	vx_uint32 num_blocks_w, num_blocks_h;

	// Access full images for reading
	vx_imagepatch_addressing_t addr = { 0 };
	vx_rectangle_t rect;
	rect.start_x = 0;
	rect.start_y = 0;
	rect.end_x = m_width;
	rect.end_y = m_height*m_numImages;
	vx_uint8 * base_ptr = nullptr;
	ERROR_CHECK_STATUS(vxAccessImagePatch(m_InputImage, &rect, 0, &addr, (void **)&base_ptr, VX_READ_ONLY));
	// assume stride_x and stride_y are constant
	m_stride = addr.stride_y;
	m_stride_x = addr.stride_x;
	num_blocks_h = (m_height + 31) >> 5;
	num_blocks_w = (m_width + 31) >> 5;
    // go through all 32x32 blocks in dst
	vx_uint32 blk_id = 0;
	for (vx_uint32 by = 0, bs_y = 0; by < num_blocks_h; by++, bs_y += 32){
		for (vx_uint32 bx = 0, bs_x = 0; bx < num_blocks_w; bx++, bs_x += 32, blk_id++){
            // for each block,  check if it is in any overlap region
			block_gain_info *BgInfo = &m_pblockgainInfo[blk_id];
			BgInfo->b_dstX = bx, BgInfo->b_dstY = by;
			vx_uint32 nz, ISum, JSum;
			for (i = 0; i < (int)m_numImages; i++){
				for (int j = i; j < (int)m_numImages; j++){
					if ((m_pRoi_rect[i][j].start_x != -1) && (by >= (m_pRoi_rect[i][j].start_y >> 5)) && (by < ((m_pRoi_rect[i][j].end_y+31) >> 5)) 
						&& (bx >= (m_pRoi_rect[i][j].start_x >> 5)) && (bx < ((m_pRoi_rect[i][j].end_x+31) >> 5))){
						ISum = 0, JSum = 0, nz = 0;
						// find the i,j intersect rect
						if (i == j){
							vx_uint32 *pI = (vx_uint32 *)(base_ptr + (m_height*i + bs_y)*m_stride + (bs_x *m_stride_x));
							nz += count_nz_mean_single_32x32(pI, (m_stride >> 2), &ISum, m_channel);
						}
						else
						{
							vx_uint32 *pI = (vx_uint32 *)(base_ptr + (m_height*i + bs_y)*m_stride + (bs_x*m_stride_x));
							vx_uint32 *pJ = (vx_uint32 *)(base_ptr + (m_height*j + bs_y)*m_stride + (bs_x*m_stride_x));
							nz += count_nz_mean_double_32x32(pI, pJ, (m_stride >> 2), &ISum, &JSum, m_channel);
						}
						nz = std::max(nz, (vx_uint32)1);
						BgInfo->Count[i][j] = nz;
						BgInfo->Sum[i][j] = ISum / nz;
						if (i != j) {
							BgInfo->Count[j][i] = nz;  BgInfo->Sum[j][i] = JSum / nz;
						}
					}
				}
			}
		}
        // for testing 
	}
	block_gain_info * pBg = &m_pblockgainInfo[0];
	int block_gain_buf_size = m_blockgainsStride* num_blocks_h;
	// compute gain for each 32x32 block for all images and store in m_block_gain_buf
	for (int blk = 0; pBg && blk < (int)(num_blocks_h*num_blocks_w); pBg++, blk++)
	{
		// generate augmented matrix[A/b] for solving gains
		vx_uint32 N;
		for (i = 0; i < (int)m_numImages; i++){
			memset(&m_AMat[i][0], 0, (m_numImages + 1)*sizeof(vx_float64));		//initialize
			for (int j = 0; j < (int)m_numImages; ++j) {
				N = pBg->Count[i][j];
				m_AMat[i][m_numImages] += m_beta * N;		// b matrix
				m_AMat[i][i] += m_beta * N;
				if (j == i)			continue;
				m_AMat[i][i] += 2 * m_alpha * pBg->Sum[i][j] * pBg->Sum[i][j] * N;
				m_AMat[i][j] -= 2 * m_alpha * pBg->Sum[i][j] * pBg->Sum[j][i] * N;
			}
		}
		solve_gauss(m_AMat, m_Gains, m_numImages);
		for (i = 0; i < (int)m_numImages; i++){
			vx_float32 *pblk = m_block_gain_buf + i*block_gain_buf_size;
			*(pblk + pBg->b_dstY*m_blockgainsStride + pBg->b_dstX) = m_Gains[i];
		}
	}
    // filter gains with (0.25, 0.5, 0.25) for valid regions only.
	vx_size		stride = 0;
	void		*base_array = 0;
	vx_size capacity;
	ERROR_CHECK_STATUS(vxQueryArray(m_valid_roi, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity)));
	ERROR_CHECK_STATUS(vxAccessArrayRange(m_valid_roi, 0, capacity, &stride, (void **)&base_array, VX_READ_ONLY));
    // allocate temporary buffer for storing the filtered block coefficients.
	vx_float32 *tmp = new vx_float32[m_blockgainsStride*num_blocks_h];
	// calulate the overlapped ROI matrix [numImages][numImages]	// assumed constant for all the frames. new parameters need new initialization
	for (i = 0; i < (int)m_numImages; i++){
		vx_rectangle_t *rect_I = &vxArrayItem(vx_rectangle_t, base_array, i, stride);
		int start_y = (rect_I->start_y >> 5), end_y = (rect_I->end_y + 31) >> 5;
		int start_x = (rect_I->start_x >> 5), end_x = (rect_I->end_x + 31) >> 5;
		vx_float32 *src = m_block_gain_buf + i*block_gain_buf_size + start_y*m_blockgainsStride;
		vx_float32* dst = tmp + start_y*m_blockgainsStride;
		for (int y = start_y; y < end_y; y++)
		{
			const vx_float32* srow0 = y > 0 ? src - m_blockgainsStride : src;
			const vx_float32* srow1 = src;
			const vx_float32* srow2 = (y <(end_y-1)) ? src + m_blockgainsStride: src;
			for (int x = start_x+1; x <= end_x - 1; x++)
			{
				dst[x] = srow0[x] * 0.125f + srow1[x - 1] * 0.125f + srow1[x] * 0.5f + srow1[x + 1] * 0.125f + srow2[x] * 0.125f;
			}
			src += m_blockgainsStride;
			dst += m_blockgainsStride;
		}
        // copy tmp to src
		src = m_block_gain_buf + i*block_gain_buf_size + start_y*m_blockgainsStride;
		dst = tmp + start_y*m_blockgainsStride;
		for (int y = start_y; y < end_y; y++)
		{
			memcpy((src + start_x + 1), (dst + start_x + 1), (end_x - start_x - 2)*sizeof(vx_float32));
			src += m_blockgainsStride;
			dst += m_blockgainsStride;
		}
	}
	ERROR_CHECK_STATUS(vxCommitArrayRange(m_valid_roi, 0, capacity, base_array));
	ERROR_CHECK_STATUS(vxReleaseArray(&m_valid_roi));
#if 0
	// dump block gain buffer to file
	FILE *fp = fopen("c:\\test\\imvt\\bg.bin", "wb");
	if (fp){
		fwrite(m_block_gain_buf, sizeof(float), block_gain_buf_size*m_numImages, fp);
	}
	fclose(fp);
	// Apply gains to all images
	status = ApplyBlockGains(base_ptr);
#endif
	// commit image patch
	if ((status = vxCommitImagePatch(m_InputImage, &rect, 0, &addr, (void *)base_ptr) != VX_SUCCESS)) {
		vxAddLogEntry((vx_reference)m_node, VX_FAILURE, "ERROR Decoder Node: vxCommitImagePatch(READ) failed, status = %d\n", status);
		return VX_FAILURE;
	}
	return VX_SUCCESS;
}

vx_status CExpCompensator::SolveForGains(vx_float32 alpha, vx_float32 beta, vx_uint32 *pIMat, vx_uint32 *pNMat, vx_uint32 num_images, vx_array Gains_arr, vx_uint32 rows, vx_uint32 cols)
{
	int i, N = cols*cols;
	m_numImages = num_images;
	int bRGBGain = (rows >= 3 * cols) ? 1 : 0;
	float *gains = new float[num_images];

	// normalize intensity 
	vx_uint32 *pGMat = nullptr; 
	vx_uint32 *pBMat = nullptr;
	if (bRGBGain){
		int offs = num_images*cols;
		pGMat = pIMat + offs;
		pBMat = pIMat + 2*offs;
	}
	for (i = 0; i < N; i++){
		if (pNMat[i]){
			pIMat[i] =(vx_uint32) (pIMat[i]*16.0/ pNMat[i]);			// I values are scaled
			if (bRGBGain){
				pGMat[i] = (vx_uint32)(pGMat[i] * 16.0 / pNMat[i]);			// I values are scaled
				pBMat[i] = (vx_uint32)(pBMat[i] * 16.0 / pNMat[i]);			// I values are scaled
			}
		}
	}
	// generate augmented matrix[A/b] for solving gains
	m_AMat = new vx_float64*[m_numImages];
	for (i = 0; i < (int)m_numImages; i++){
		m_AMat[i] = new vx_float64[m_numImages + 1];	// enough for the augmented matrix [a|b]
		memset(&m_AMat[i][0], 0, (m_numImages + 1)*sizeof(vx_float64));
	}
	for (i = 0; i < (int)num_images; i++){
		vx_uint32 *pI = pIMat + i*cols;
		vx_uint32 *pN = pNMat + i*cols;
		for (int j = 0; j < (int)num_images; ++j) {
			vx_uint32 N = pN[j] ? pN[j] : 1;
			m_AMat[i][m_numImages] += beta * N;		// b matrix
			m_AMat[i][i] += beta * N;
			if (j == i)			continue;
			m_AMat[i][i] += 2 * alpha * pI[j] * pI[j] * N;
			m_AMat[i][j] -= 2 * alpha * pI[j] * pIMat[j*num_images + i] * N;
		}
	}
	solve_gauss(m_AMat, gains, m_numImages);
	if (bRGBGain){
		float *gain_g = new float[num_images];
		float *gain_b = new float[num_images];
		for (i = 0; i < (int)m_numImages; i++){
			memset(&m_AMat[i][0], 0, (m_numImages + 1)*sizeof(vx_float64));
		}
		for (i = 0; i < (int)m_numImages; i++){
			vx_uint32 *pI = pGMat + i*cols;
			vx_uint32 *pN = pNMat + i*cols;
			for (int j = 0; j < (int)num_images; ++j) {
				vx_uint32 N = pN[j] ? pN[j] : 1;
				m_AMat[i][m_numImages] += beta * N;		// b matrix
				m_AMat[i][i] += beta * N;
				if (j == i)			continue;
				m_AMat[i][i] += 2 * alpha * pI[j] * pI[j] * N;
				m_AMat[i][j] -= 2 * alpha * pI[j] * pGMat[j*num_images + i] * N;
			}
		}
		//solve the linear equation A*gains_ = B
		solve_gauss(m_AMat, gain_g, m_numImages);

		for (i = 0; i < (int)m_numImages; i++){
			memset(&m_AMat[i][0], 0, (m_numImages + 1)*sizeof(vx_float64));
		}
		// generate augmented matrix[A/b] for solving gains for B channel
		for (i = 0; i < (int)m_numImages; i++){
			vx_uint32 *pI = pBMat + i*cols;
			vx_uint32 *pN = pNMat + i*cols;
			for (int j = 0; j < (int)num_images; ++j) {
				vx_uint32 N = pN[j] ? pN[j] : 1;
				m_AMat[i][m_numImages] += beta * N;		// b matrix
				m_AMat[i][i] += beta * N;
				if (j == i)			continue;
				m_AMat[i][i] += 2 * alpha * pI[j] * pI[j] * N;
				m_AMat[i][j] -= 2 * alpha * pI[j] * pBMat[j*num_images + i] * N;
			}
		}
		//solve the linear equation A*gains_ = B
		solve_gauss(m_AMat, gain_b, m_numImages);
		float *pRGB_gains = new float[m_numImages * 3];
		for (i = 0; i < (int)m_numImages; i++){
			// gamma correction for the gains
			pRGB_gains[i * 3]     = powf(gains[i], 0.454546f);
			pRGB_gains[i * 3 + 1] = powf(gain_g[i], 0.454546f);
			pRGB_gains[i * 3 + 2] = powf(gain_b[i], 0.454546f);
		}
		ERROR_CHECK_STATUS(vxTruncateArray(Gains_arr, 0));
		ERROR_CHECK_STATUS(vxAddArrayItems(Gains_arr, m_numImages*3, pRGB_gains, sizeof(float)));
		delete[] pRGB_gains;
		delete[] gain_g;
		delete[] gain_b;
	}
	else
	{
		ERROR_CHECK_STATUS(vxTruncateArray(Gains_arr, 0));
		ERROR_CHECK_STATUS(vxAddArrayItems(Gains_arr, m_numImages, gains, sizeof(float)));
	}
	delete[] gains;
	for (i = 0; i < (int)num_images; i++){
		delete[] m_AMat[i];
	}
	delete[] m_AMat;
	return VX_SUCCESS;
}

// solving linear equation of Augmented matrix[A|b] using gaussian elemination method
void CExpCompensator::solve_gauss(vx_float64 **A, vx_float32 *g, int num)
{
	int n = num;

	for (int i = 0; i < n; i++) {
		// Search for maximum in this column
		double maxEl = fabs(A[i][i]);
		int maxRow = i;
		for (int k = i + 1; k<n; k++) {
			if (fabs(A[k][i]) > maxEl) {
				maxEl = fabs(A[k][i]);
				maxRow = k;
			}
		}
		// Swap maximum row with current row (column by column)
		for (int k = i; k < n + 1; k++) {
			double tmp = A[maxRow][k];
			A[maxRow][k] = A[i][k];
			A[i][k] = tmp;
		}
		// Make all rows below this one 0 in current column: scale
		for (int k = i + 1; k < n; k++) {
			double c = -A[k][i] / A[i][i];
			for (int j = i; j < n + 1; j++) {
				if (i == j) {
					A[k][j] = 0;
				}
				else {
					A[k][j] += c * A[i][j];
				}
			}
		}
	}

	// Solve equation Ax=b for an upper triangular matrix A
	for (int i = n - 1; i >= 0; i--) {
		double gain = (A[i][n] / A[i][i]);
		for (int k = i - 1; k >= 0; k--) {
			A[k][n] -= A[k][i] * gain;
		}
		g[i] = (vx_float32)gain;
	}
	return;
}

vx_status CExpCompensator::ApplyGains(void *in_base_addr)
{
	uint32_t num_threads = m_numImages - 1;
	std::thread *thread_apply_gains = new std::thread[num_threads];
	for (int i = 0; i < (int)m_numImages - 1; i++)
	{
		thread_apply_gains[i] = std::thread(&CExpCompensator::applygains_thread_func, this, i, (char *)in_base_addr);
	}
	applygains_thread_func(num_threads, (char *)in_base_addr);
	//Join the threads with the main thread
	for (int i = 0; i < (int)m_numImages - 1; i++) {
		thread_apply_gains[i].join();
	}
	return VX_SUCCESS;
}

vx_status CExpCompensator::applygains_thread_func(vx_int32 img_num, char *in_base_addr)
{
	vx_status status;
	// access image patch output for writing
	vx_imagepatch_addressing_t addr = { 0 };
	vx_rectangle_t rect;
	rect.start_x = 0;
	rect.start_y = m_height*img_num;
	rect.end_x = m_width;
	rect.end_y = rect.start_y + m_height;
	vx_uint8 * base_ptr = nullptr;
	ERROR_CHECK_STATUS(vxAccessImagePatch(m_OutputImage, &rect, 0, &addr, (void **)&base_ptr, VX_WRITE_ONLY));

	vx_int32 width = mValidRect[img_num].end_x - mValidRect[img_num].start_x;
	vx_int32 height = mValidRect[img_num].end_y - mValidRect[img_num].start_y;
	vx_uint32 *pRGB = (vx_uint32 *)(in_base_addr + (img_num*m_height + mValidRect[img_num].start_y)*m_stride + (mValidRect[img_num].start_x*m_stride_x));
	vx_uint32 *pDst = (vx_uint32 *)(base_ptr + mValidRect[img_num].start_y*addr.stride_y + mValidRect[img_num].start_x*addr.stride_x);
	float g_y = m_Gains[img_num];
	float g_r, g_g, g_b;
	//	g_y = (float)pow(g_y, 1.2);
	if (m_bUseRGBgains){
		g_r = g_y;
		g_g = m_GainsG[img_num];
		g_b = m_GainsB[img_num];
#if USE_GAMMA_CORRECTION
		g_r = powf(g_r, 0.454546f);
		g_g = powf(g_g, 0.454546f);
		g_b = powf(g_b, 0.454546f);
#endif
	}
	else{
		g_r = g_g = g_b = g_y;	// todo: check if we need to apply gain factor for RGB
	}
	// todo:: if we do the following code in CPU, need to optimize using SSE
	for (int i = 0; i < height; i++){
		for (int j = 0; j < width; j++){
			// apply gain only to valid pixels : todo: multiply with approapriate gain factors for R, G and B
			if (pRGB[j] != 0x80000000){
				uint8_t *p = (uint8_t *)&pRGB[j];
				uint8_t *d = (uint8_t *)&pDst[j];
				d[0] = saturate_char((int)(p[0] * g_r));
				d[1] = saturate_char((int)(p[1] * g_g));
				d[2] = saturate_char((int)(p[2] * g_b));
				d[3] = saturate_char((int)(p[3] * g_y));
			}
			else
				pDst[j] = pRGB[j];
		}
		pRGB += (m_stride >> 2);
		pDst += (addr.stride_y >> 2);
	}
	// commit image patch
	if ((status = vxCommitImagePatch(m_OutputImage, &rect, 0, &addr, (void *)base_ptr) != VX_SUCCESS)) {
		vxAddLogEntry((vx_reference)m_node, VX_FAILURE, "ERROR Decoder Node: vxCommitImagePatch(WRITE) failed, status = %d\n", status);
		return VX_FAILURE;
	}
	return status;
}

vx_status CExpCompensator::ApplyBlockGains(void *in_base_addr)
{
	uint32_t num_threads = m_numImages - 1;
	applygains_thread_func(0, (char *)in_base_addr);
	applygains_thread_func(1, (char *)in_base_addr);
	applygains_thread_func(2, (char *)in_base_addr);
	applygains_thread_func(3, (char *)in_base_addr);
	return VX_SUCCESS;
}

vx_status CExpCompensator::applyblockgains_thread_func(vx_int32 img_num, char *in_base_addr)
{
	vx_status status;
	// access image patch output for writing
	float *gain_buf = m_block_gain_buf + (img_num*m_blockgainsStride*((m_height + 31) >> 5));
	vx_imagepatch_addressing_t addr = { 0 };
	vx_rectangle_t rect;
	rect.start_x = 0;
	rect.start_y = m_height*img_num;
	rect.end_x = m_width;
	rect.end_y = rect.start_y + m_height;
	vx_uint8 * base_ptr = nullptr;
	ERROR_CHECK_STATUS(vxAccessImagePatch(m_OutputImage, &rect, 0, &addr, (void **)&base_ptr, VX_WRITE_ONLY));

	vx_int32 width = mValidRect[img_num].end_x - mValidRect[img_num].start_x;
	vx_int32 height = mValidRect[img_num].end_y - mValidRect[img_num].start_y;
	vx_uint32 *pRGB = (vx_uint32 *)(in_base_addr + (img_num*m_height + mValidRect[img_num].start_y)*m_stride + (mValidRect[img_num].start_x*m_stride_x));
	vx_uint32 *pDst = (vx_uint32 *)(base_ptr + mValidRect[img_num].start_y*addr.stride_y + mValidRect[img_num].start_x*addr.stride_x);
	int b_startx = mValidRect[img_num].start_x >> 5;
	int b_endx = std::min((m_width>>5),((mValidRect[img_num].end_x + 31) >> 5));
	int b_starty = mValidRect[img_num].start_y >> 5;
	int b_endy = std::min((m_height >> 5), ((mValidRect[img_num].end_y + 31) >> 5));

	// todo:: if we do the following code in CPU, need to optimize using SSE
	for (int i = b_starty; i < b_endy; i++){
		for (int j = b_startx; j < b_endx; j++){
			int pos = i * 32 * (m_stride >> 2) + j * 32;
			int dpos = i * 32 * ((addr.stride_y >> 2) >> 2) + j * 32;
			int end_y = (((i + 1) << 5) >= (int)m_height) ? m_height - (i * 32) : 32;
			int end_x = (((j + 1) << 5) >= (int)m_width) ? m_width - (i * 32) : 32;
			float g_y = *(gain_buf + b_starty*m_blockgainsStride + b_startx);
			vx_uint32 *pSrc = pRGB + pos;
			vx_uint32 *pRGBDst = pDst + dpos;
			for (int y = 0; y < end_y; y++) {
				for (int x = 0; x < end_x; x++){
					// apply gain only to valid pixels : todo: multiply with approapriate gain factors for R, G and B
					if (pSrc[x] != 0x80000000){
						uint8_t *p = (uint8_t *)&pRGB[x];
						uint8_t *d = (uint8_t *)&pRGBDst[x];
						d[0] = saturate_char((int)(p[0] * g_y));
						d[1] = saturate_char((int)(p[1] * g_y));
						d[2] = saturate_char((int)(p[2] * g_y));
						d[3] = saturate_char((int)(p[3] * g_y));
					}
					else
						pRGBDst[dpos + x] = pSrc[pos + x];
				}
				pSrc += (m_stride >> 2);
				pRGBDst += (addr.stride_y >> 2);
			}
		}
	}
	// commit image patch
	if ((status = vxCommitImagePatch(m_OutputImage, &rect, 0, &addr, (void *)base_ptr) != VX_SUCCESS)) {
		vxAddLogEntry((vx_reference)m_node, VX_FAILURE, "ERROR Decoder Node: vxCommitImagePatch(WRITE) failed, status = %d\n", status);
		return VX_FAILURE;
	}
	return status;
}
