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


#ifndef __LENS_DISTORTION_REMAP_H__
#define __LENS_DISTORTION_REMAP_H__

#include "kernels.h"

//////////////////////////////////////////////////////////////////////
//! \brief floating-point 2d coordinate.
typedef struct {
	float x, y;
} StitchCoord2dFloat;

//////////////////////////////////////////////////////////////////////
// calculate lens distorion and warp maps from rig and camera configuration
vx_status CalculateLensDistortionAndWarpMaps(
	vx_uint32 numCamera,                     // [in] number of cameras
	vx_uint32 camWidth, vx_uint32 camHeight, // [in] individual camera dimensions
	vx_uint32 eqrWidth, vx_uint32 eqrHeight, // [in] output equirectangular dimensions
	const rig_params * rigParam,             // [in] rig configuration
	const camera_params * camParam,          // [in] individual camera configuration: size: [numCamera]
	vx_uint32 * validPixelCamMap,            // [out] valid pixel camera index map: size: [eqrWidth * eqrHeight] (optional)
	vx_uint32 paddingPixelCount,             // [in] padding pixels around valid region
	vx_uint32 * paddedPixelCamMap,           // [out] padded pixel camera index map: size: [eqrWidth * eqrHeight] (optional)
	StitchCoord2dFloat * camSrcMap,          // [out] camera coordinate mapping: size: [numCamera * eqrWidth * eqrHeight] (optional)
	vx_float32 * internalBufferForCamIndex,  // [tmp] buffer for internal use: size: [eqrWidth * eqrHeight] (optional)
	vx_uint8 * defaultCamIndex               // [out] default camera index (255 refers to no camera): size: [eqrWidth * eqrHeight] (optional)
	);

//////////////////////////////////////////////////////////////////////
// calculate overlap regions and returns number of overlaps
vx_uint32 CalculateValidOverlapRegions(
	vx_uint32 numCamera,                 // [in] number of cameras
	vx_uint32 eqrWidth,                  // [in] output equirectangular image width
	vx_uint32 eqrHeight,                 // [in] output equirectangular image height
	const vx_uint32 * validPixelCamMap,  // [in] valid pixel camera index map: size: [eqrWidth * eqrHeight]
	vx_rectangle_t ** overlapValid,      // [out] overlap regions: overlapValid[cam_i][cam_j] for overlap of cam_i and cam_j, cam_j <= cam_i
	vx_uint32 * validCamOverlapInfo,     // [out] camera overlap info - use "validCamOverlapInfo[cam_i] & (1 << cam_j)": size: [32]
	const vx_uint32 * paddedPixelCamMap, // [in] padded pixel camera index map: size: [eqrWidth * eqrHeight](optional)
	vx_rectangle_t ** overlapPadded,     // [out] overlap regions: overlapPadded[cam_i][cam_j] for overlap of cam_i and cam_j, cam_j <= cam_i(optional)
	vx_uint32 * paddedCamOverlapInfo     // [out] camera overlap info - use "paddedCamOverlapInfo[cam_i] & (1 << cam_j)": size: [32](optional)
	);

//////////////////////////////////////////////////////////////////////
// Generate valid mask image
vx_status GenerateValidMaskImage(
	vx_uint32 numCamera,                  // [in] number of cameras
	vx_uint32 eqrWidth,                   // [in] output equirectangular image width
	vx_uint32 eqrHeight,                  // [in] output equirectangular image height
	const vx_uint32 * validPixelCamMap,   // [in] valid pixel camera index map: size: [eqrWidth * eqrHeight]
	vx_uint32  maskStride,                // [in] stride (in bytes) of mask image
	vx_uint8 * maskBuf                    // [out] valid mask image buffer: size: [eqrWidth * eqrHeight * numCamera]
	);

#endif //__LENS_DISTORTION_REMAP_H__
