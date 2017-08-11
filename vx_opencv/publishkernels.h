/* 
Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
 
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


#ifndef _PUBLISH_KERNELS_H_
#define _PUBLISH_KERNELS_H_

#if _WIN32 
#define SHARED_PUBLIC __declspec(dllexport)
#else
#define SHARED_PUBLIC __attribute__ ((visibility ("default")))
#endif

#include <VX/vx.h>
#include "vx_opencv.h"
#include"OpenCV_Tunnel.h"

#ifndef USE_OPENCV_CONTRIB
#define USE_OPENCV_CONTRIB 0
#endif

extern "C" SHARED_PUBLIC vx_status VX_API_CALL vxPublishKernels(vx_context context);
vx_status ADD_KERENEL(std::function<vx_status(vx_context)>);
vx_status get_kernels_to_publish();

vx_status CV_absdiff_Register(vx_context);
vx_status CV_adaptiveThreshold_Register(vx_context);
vx_status CV_add_Register(vx_context);
vx_status CV_AddWeighted_Register(vx_context);
vx_status CV_bilateralFilter_Register(vx_context);
vx_status CV_bitwise_and_Register(vx_context);
vx_status CV_bitwise_not_Register(vx_context);
vx_status CV_bitwise_or_Register(vx_context);
vx_status CV_bitwise_xor_Register(vx_context);
vx_status CV_blur_Register(vx_context);
vx_status CV_Boxfilter_Register(vx_context);
vx_status CV_brisk_compute_Register(vx_context);
vx_status CV_brisk_detect_Register(vx_context);
vx_status CV_buildOpticalFlowPyramid_Register(vx_context);
vx_status CV_buildPyramid_Register(vx_context);
vx_status CV_Canny_Register(vx_context);
vx_status CV_compare_Register(vx_context);
vx_status CV_convertScaleAbs_Register(vx_context);
vx_status CV_cornerHarris_Register(vx_context);
vx_status CV_cornerMinEigenVal_Register(vx_context);
vx_status CV_countNonZero_Register(vx_context);
vx_status CV_cvtColor_Register(vx_context);
vx_status CV_dilate_Register(vx_context);
vx_status CV_distanceTransform_Register(vx_context);
vx_status CV_divide_Register(vx_context);
vx_status CV_erode_Register(vx_context);
vx_status CV_FAST_detector_Register(vx_context);
vx_status CV_fastNlMeansDenoising_Register(vx_context);
vx_status CV_fastNlMeansDenoisingColored_Register(vx_context);
vx_status CV_filter2D_Register(vx_context);
vx_status CV_flip_Register(vx_context);
vx_status CV_Gaussianblur_Register(vx_context);
vx_status CV_good_features_to_track_Register(vx_context);
vx_status CV_integral_Register(vx_context);
vx_status CV_Laplacian_Register(vx_context);
vx_status CV_MedianBlur_Register(vx_context);
vx_status CV_morphologyEx_Register(vx_context);
vx_status CV_MSER_detect_Register(vx_context);
vx_status CV_multiply_Register(vx_context);
vx_status CV_norm_Register(vx_context);
vx_status CV_ORB_compute_Register(vx_context);
vx_status CV_ORB_detect_Register(vx_context);
vx_status CV_pyrdown_Register(vx_context);
vx_status CV_pyrup_Register(vx_context);
vx_status CV_resize_Register(vx_context);
vx_status CV_Scharr_Register(vx_context);
vx_status CV_sepFilter2D_Register(vx_context);
vx_status CV_simple_blob_detect_Register(vx_context);
vx_status CV_simple_blob_detect_initialize_Register(vx_context);
vx_status CV_Sobel_Register(vx_context);
vx_status CV_subtract_Register(vx_context);
vx_status CV_threshold_Register(vx_context);
vx_status CV_transpose_Register(vx_context);
vx_status CV_warpAffine_Register(vx_context);
vx_status CV_warpPerspective_Register(vx_context);

#if USE_OPENCV_CONTRIB
vx_status CV_SIFT_compute_Register(vx_context);
vx_status CV_SIFT_detect_Register(vx_context);
vx_status CV_star_detect_Register(vx_context);
vx_status CV_SURF_compute_Register(vx_context);
vx_status CV_SURF_detect_Register(vx_context);
#endif



//Filters
#define VX_KERNEL_EXT_CV_Medianblur_NAME				"org.opencv.medianblur"
#define VX_KERNEL_EXT_CV_Boxfilter_NAME					"org.opencv.boxfilter"
#define VX_KERNEL_EXT_CV_Gaussianblur_NAME				"org.opencv.gaussianblur"
#define VX_KERNEL_EXT_CV_Blur_NAME						"org.opencv.blur"
#define VX_KERNEL_EXT_CV_BilateralFilter_NAME			"org.opencv.bilateralfilter"
#define VX_KERNEL_EXT_CV_FILTER2D_NAME					"org.opencv.filter2d"
#define VX_KERNEL_EXT_CV_SEPFILTER2D_NAME				"org.opencv.sepfilter2d"

//Corner Detectors
#define VX_KERNEL_EXT_CV_FAST_Detector_NAME				"org.opencv.fast"
#define VX_KERNEL_EXT_CV_GoodFeature_Detector_NAME      "org.opencv.good_features_to_track"

//Non-Free Freature Detectors
#define VX_KERNEL_EXT_CV_SIFT_DETECT_NAME				"org.opencv.sift_detect"
#define VX_KERNEL_EXT_CV_SURF_DETECT_NAME				"org.opencv.surf_detect"
#define VX_KERNEL_EXT_CV_SIFT_Compute_NAME				"org.opencv.sift_compute"
#define VX_KERNEL_EXT_CV_SURF_Compute_NAME				"org.opencv.surf_compute"
#define VX_KERNEL_EXT_CV_STAR_FEATURE_Detector_NAME     "org.opencv.star_detect"

//Feature Detectors
#define VX_KERNEL_EXT_CV_BRISK_Detector_NAME			"org.opencv.brisk_detect"
#define VX_KERNEL_EXT_CV_MSER_Detector_NAME				"org.opencv.mser_detect"
#define VX_KERNEL_EXT_CV_ORB_Detector_NAME				"org.opencv.orb_detect"
#define VX_KERNEL_EXT_CV_SIMPLE_BLOB_Detector_NAME		"org.opencv.simple_blob_detect"
#define VX_KERNEL_EXT_CV_SIMPLE_BLOB_Detector_Init_NAME	"org.opencv.simple_blob_detect_initialize"

//Feature Descriptors or Compute Nodes
#define VX_KERNEL_EXT_CV_BRISK_Compute_NAME				"org.opencv.brisk_compute"
#define VX_KERNEL_EXT_CV_ORB_Compute_NAME				"org.opencv.ORB_compute"

//Other OpenCV Tunneled Nodes
#define VX_KERNEL_EXT_CV_SOBEL_NAME						"org.opencv.sobel"
#define VX_KERNEL_EXT_CV_SCHARR_NAME					"org.opencv.scharr"
#define VX_KERNEL_EXT_CV_CANNY_NAME						"org.opencv.canny"
#define VX_KERNEL_EXT_CV_LAPLACIAN_NAME					"org.opencv.laplacian"
#define VX_KERNEL_EXT_CV_MORPHOLOGYEX_NAME				"org.opencv.morphologyex"
#define VX_KERNEL_EXT_CV_MULTIPLY_NAME					"org.opencv.multiply"
#define VX_KERNEL_EXT_CV_DIVIDE_NAME					"org.opencv.divide"
#define VX_KERNEL_EXT_CV_ADD_NAME						"org.opencv.add"
#define VX_KERNEL_EXT_CV_SUBTRACT_NAME					"org.opencv.subtract"
#define VX_KERNEL_EXT_CV_BITWISE_AND_NAME				"org.opencv.bitwise_and"
#define VX_KERNEL_EXT_CV_BITWISE_NOT_NAME				"org.opencv.bitwise_not"
#define VX_KERNEL_EXT_CV_BITWISE_OR_NAME				"org.opencv.bitwise_or"
#define VX_KERNEL_EXT_CV_BITWISE_XOR_NAME				"org.opencv.bitwise_xor"
#define VX_KERNEL_EXT_CV_CONVERT_SCALE_ABS_NAME				"org.opencv.convertscaleabs"
#define VX_KERNEL_EXT_CV_ADD_WEIGHTED_NAME					"org.opencv.addweighted"
#define VX_KERNEL_EXT_CV_FLIP_NAME							"org.opencv.flip"
#define VX_KERNEL_EXT_CV_TRANSPOSE_NAME						"org.opencv.transpose"
#define VX_KERNEL_EXT_CV_ABSDIFF_NAME						"org.opencv.absdiff"
#define VX_KERNEL_EXT_CV_COMPARE_NAME						"org.opencv.compare"
#define VX_KERNEL_EXT_CV_RESIZE_NAME						"org.opencv.resize"
#define VX_KERNEL_EXT_CV_ADAPTIVE_THRESHOLD_NAME			"org.opencv.adaptivethreshold"
#define VX_KERNEL_EXT_CV_DISTANCE_TRANSFORM_NAME			"org.opencv.distancetransform"
#define VX_KERNEL_EXT_CV_CVTCOLOR_NAME						"org.opencv.cvtcolor"
#define VX_KERNEL_EXT_CV_THRESHOLD_NAME						"org.opencv.threshold"
#define VX_KERNEL_EXT_CV_fastNlMeansDenoising_NAME			"org.opencv.fastnlmeansdenoising"
#define VX_KERNEL_EXT_CV_fastNlMeansDenoisingColored_NAME	"org.opencv.fastnlmeansdenoisingcolored"
#define VX_KERNEL_EXT_CV_BUILD_OPTICAL_FLOW_PYRAMID_NAME	"org.opencv.buildopticalflowpyramid"
#define VX_KERNEL_EXT_CV_BUILDPYRAMID_NAME					"org.opencv.buildpyramid"
#define VX_KERNEL_EXT_CV_pyrUp_NAME							"org.opencv.pyrup"
#define VX_KERNEL_EXT_CV_pyrDown_NAME						"org.opencv.pyrdown"
#define VX_KERNEL_EXT_CV_DILATE_NAME						"org.opencv.dilate"
#define VX_KERNEL_EXT_CV_ERODE_NAME							"org.opencv.erode"
#define VX_KERNEL_EXT_CV_WARPAFFINE_NAME					"org.opencv.warpaffine"
#define VX_KERNEL_EXT_CV_WARPPERSPECTIVE_NAME				"org.opencv.warpperspective"
#define VX_KERNEL_EXT_CV_CORNERHARRIS_NAME					"org.opencv.cornerharris"
#define VX_KERNEL_EXT_CV_cornerMinEigenVal_NAME				"org.opencv.cornermineigenval"
#define VX_KERNEL_EXT_CV_integral_NAME						"org.opencv.integral"
#define VX_KERNEL_EXT_CV_countNonZero_NAME					"org.opencv.countnonzero"
#define VX_KERNEL_EXT_CV_norm_NAME							"org.opencv.norm"



#endif //_AMDVX_EXT__PUBLISH_KERNELS_H_

