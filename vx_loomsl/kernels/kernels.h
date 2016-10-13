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


#ifndef __VX_STITCHING_H__
#define __VX_STITCHING_H__

//////////////////////////////////////////////////////////////////////
// OpenVX standard header files and openVX AMD extensions
#include <VX/vx.h>
#include <vx_ext_amd.h>
#include <VX/vx_compatibility.h>

#if !_WIN32
#include <strings.h>
#define _strnicmp strncasecmp
#define _stricmp  strcasecmp
#endif

//////////////////////////////////////////////////////////////////////
// SHARED_PUBLIC - shared sybols for export
// STITCH_API_ENTRY - export API symbols
#if _WIN32
#define SHARED_PUBLIC extern "C" __declspec(dllexport)
#else
#define SHARED_PUBLIC extern "C" __attribute__ ((visibility ("default")))
#endif
#define LIVE_STITCH_API_ENTRY SHARED_PUBLIC

//////////////////////////////////////////////////////////////////////
// common header files
#include "live_stitch_api.h"
#include <omp.h>
#include <vector>

//////////////////////////////////////////////////////////////////////
//! \brief The AMD extension library for stitching
#define	AMDOVX_LIBRARY_STITCHING          2

//////////////////////////////////////////////////////////////////////
//! \brief The additional image formats supported by stitching module
enum vx_df_image_amd_stitching_e {
	VX_DF_IMAGE_Y210_AMD = VX_DF_IMAGE('Y', '2', '1', '0'),  // AGO image with YUV 4:2:2 10-bit (Y210)
	VX_DF_IMAGE_Y212_AMD = VX_DF_IMAGE('Y', '2', '1', '2'),  // AGO image with YUV 4:2:2 12-bit (Y212)
	VX_DF_IMAGE_Y216_AMD = VX_DF_IMAGE('Y', '2', '1', '6'),  // AGO image with YUV 4:2:2 16-bit (Y216)
	VX_DF_IMAGE_RGB4_AMD = VX_DF_IMAGE('R', 'G', 'B', '4'),  // AGO image with RGB-48 16bit per channel (RGB4)
};

//////////////////////////////////////////////////////////////////////
//! \brief The list of kernels in the stitching library.
enum vx_kernel_stitching_amd_e {
	//! \brief The Lens Distortion Correction Remap kernel. Kernel name is "com.amd.loomsl.lens_distortion_remap".
	AMDOVX_KERNEL_STITCHING_LENS_DISTORTION_REMAP = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x001,

	//! \brief The Initialize Stitch Config kernel. Kernel name is "com.amd.loomsl.initialize_stitch_config".
	AMDOVX_KERNEL_STITCHING_INITIALIZE_STITCH_CONFIG = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x002,

	//! \brief The Simple Stitch Remap kernel. Kernel name is "com.amd.loomsl.initialize_stitch_remap".
	AMDOVX_KERNEL_STITCHING_INITIALIZE_STITCH_REMAP = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x003,

	//! \brief The Color Convert with optional 2x2 scale down function kernel. Kernel name is "com.amd.loomsl.color_convert".
	AMDOVX_KERNEL_STITCHING_COLOR_CONVERT = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x004,

	//! \brief The Warp function kernel. Kernel name is "com.amd.loomsl.warp".
	AMDOVX_KERNEL_STITCHING_WARP = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x005,

	//! \brief The Exposure Compensation kernel. Kernel name is "com.amd.loomsl.exposure_compensation_model".
	AMDOVX_KERNEL_STITCHING_EXPOSURE_COMPENSATION_MODEL = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x006,

	//! \brief The Merge kernel. Kernel name is "com.amd.loomsl.merge".
	AMDOVX_KERNEL_STITCHING_MERGE = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x007,

	//! \brief The Exposure Comp Stage#1 kernel. Kernel name is "com.amd.loomsl.expcomp_compute_gainmatrix".
	AMDOVX_KERNEL_STITCHING_EXPCOMP_COMPUTE_GAINMAT = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x008,

	//! \brief The Exposure Comp Stage#2 kernel. Kernel name is "com.amd.loomsl.expcomp_solvegains".
	AMDOVX_KERNEL_STITCHING_EXPCOMP_SOLVE = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x009,

	//! \brief The Exposure Comp Stage#3 kernel. Kernel name is "com.amd.loomsl.expcomp_applygains".
	AMDOVX_KERNEL_STITCHING_EXPCOMP_APPLYGAINS = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x00A,

	//! \brief The Alpha Blend kernel. Kernel name is "com.amd.loomsl.alpha_blend".
	AMDOVX_KERNEL_STITCHING_ALPHA_BLEND = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x00B,

	//! \brief The Multi-band blend kernel. Kernel name is "com.amd.loomsl.multiband_blend".
	AMDOVX_KERNEL_STITCHING_MULTIBAND_BLEND = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x00C,

	//! \brief The Half scale Gaussian kernel. Kernel name is "com.amd.loomsl.half_scale_gaussian".
	AMDOVX_KERNEL_STITCHING_HALF_SCALE_GAUSSIAN = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x00D,

	//! \brief The Half scale Gaussian kernel. Kernel name is "com.amd.loomsl.upscale_gaussian_subtract".
	AMDOVX_KERNEL_STITCHING_UPSCALE_GAUSSIAN_SUBTRACT = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x00E,

	//! \brief The Half scale Gaussian kernel. Kernel name is "com.amd.loomsl.upscale_gaussian_add".
	AMDOVX_KERNEL_STITCHING_UPSCALE_GAUSSIAN_ADD = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x00F,

	//! \brief The Half scale Gaussian kernel. Kernel name is "com.amd.loomsl.laplacian_reconstruct".
	AMDOVX_KERNEL_STITCHING_LAPLACIAN_RECONSTRUCT = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x010,

	//! \brief The Seam Finding kernel. Kernel name is "com.amd.loomsl.seamfind_model".
	AMDOVX_KERNEL_STITCHING_SEAMFIND_MODEL = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x011,

	//! \brief The Seam Finding kernel 0. Kernel name is "com.amd.loomsl.seamfind_scene_detect".
	AMDOVX_KERNEL_STITCHING_SEAMFIND_SCENE_DETECT = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x012,

	//! \brief The Seam Finding kernel 1. Kernel name is "com.amd.loomsl.seamfind_cost_generate".
	AMDOVX_KERNEL_STITCHING_SEAMFIND_COST_GENERATE = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x013,

	//! \brief The Seam Finding kernel 2. Kernel name is "com.amd.loomsl.seamfind_cost_accumulate".
	AMDOVX_KERNEL_STITCHING_SEAMFIND_COST_ACCUMULATE = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x014,

	//! \brief The Seam Finding kernel 3. Kernel name is "com.amd.loomsl.seamfind_path_trace".
	AMDOVX_KERNEL_STITCHING_SEAMFIND_PATH_TRACE = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x015,

	//! \brief The Seam Finding kernel 4. Kernel name is "com.amd.loomsl.seamfind_set_weights".
	AMDOVX_KERNEL_STITCHING_SEAMFIND_SET_WEIGHTS = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x016,

	//! \brief The Seam Finding kernel. Kernel name is "com.amd.stitching.seamfind_analyze".
	AMDOVX_KERNEL_STITCHING_SEAMFIND_ANALYZE = VX_KERNEL_BASE(VX_ID_AMD, AMDOVX_LIBRARY_STITCHING) + 0x017

};

//////////////////////////////////////////////////////////////////////
//! \brief The valid pixel entry for 8 consecutive pixel locations.
typedef struct {
	vx_uint32 camId : 5; // destination buffer/camera ID
	vx_uint32 reserved0 : 2; // reserved (shall be zero)
	vx_uint32 allValid : 1; // all 8 consecutive pixels are valid
	vx_uint32 dstX : 11; // destination pixel x-coordinate/8 (integer)
	vx_uint32 dstY : 13; // destination pixel y-coordinate   (integer)
} StitchValidPixelEntry;

//////////////////////////////////////////////////////////////////////
//! \brief The overlap pixel entry.
typedef struct {
	vx_uint32 camId0 : 5; // destination buffer/camera ID
	vx_uint32 start_x : 14; // destination start pixel x-coordinate
	vx_uint32 start_y : 13; // destination start pixel y-coordinate
	vx_uint32 end_x : 7; // ending pixel x-coordinate within the 128x32 block
	vx_uint32 end_y : 5; // ending pixel y-coordinate within the 128x32 block
	vx_uint32 camId1 : 5; // values [0..30] overlapping cameraId; 31 indicates invalid cameraId
	vx_uint32 camId2 : 5; // values [0..30] overlapping cameraId; 31 indicates invalid cameraId
	vx_uint32 camId3 : 5; // values [0..30] overlapping cameraId; 31 indicates invalid cameraId
	vx_uint32 camId4 : 5; // values [0..30] overlapping cameraId; 31 indicates invalid cameraId
} StitchOverlapPixelEntry;

typedef struct {
	unsigned int camId : 6; // destination buffer/camera ID
	unsigned int dstX : 12; // destination pixel x-coordinate/8 (integer)
	unsigned int dstY : 14; // destination pixel y-coordinate/2 (integer)
	unsigned int start_x : 8; // starting pixel x-coordinate within the 128x32 block
	unsigned int start_y : 8; // starting pixel y-coordinate within the 128x32 block
	unsigned int  end_x : 8;		// ending pixel x-coordinate within the 128x32 block
	unsigned int  end_y : 8;		// ending pixel y-coordinate within the 128x32 block
} StitchExpCompCalcEntry;

typedef struct {
	unsigned int camId : 5; // destination buffer/camera ID
	unsigned int dstX : 14; // destination pixel x-coordinate (integer)
	unsigned int dstY : 13; // destination pixel y-coordinate (integer)
	unsigned int  end_x : 8;		// ending pixel x-coordinate within the 64x16 block
	unsigned int  end_y : 8;		// ending pixel y-coordinate within the 64x16 block
	unsigned int start_x : 8; // starting pixel x-coordinate within the 64x16 block
	unsigned int start_y : 8; // starting pixel y-coordinate within the 64x16 block
} StitchBlendValidEntry;

//////////////////////////////////////////////////////////////////////
//! \brief The warp pixel remap entry for 8 consecutive pixel locations.
//  Entry is invalid if srcX and srcY has all bits set to 1s.
//  For srcX and srcY coordinates, below fixed-point representation is used:
//     Q13.3
typedef struct {
	vx_uint16 srcX0; // source pixel (x,y) for (dstX*8+0,dstY) in Q13.3 format
	vx_uint16 srcY0;
	vx_uint16 srcX1; // source pixel (x,y) for (dstX*8+1,dstY) in Q13.3 format
	vx_uint16 srcY1;
	vx_uint16 srcX2; // source pixel (x,y) for (dstX*8+2,dstY) in Q13.3 format
	vx_uint16 srcY2;
	vx_uint16 srcX3; // source pixel (x,y) for (dstX*8+3,dstY) in Q13.3 format
	vx_uint16 srcY3;
	vx_uint16 srcX4; // source pixel (x,y) for (dstX*8+4,dstY) in Q13.3 format
	vx_uint16 srcY4;
	vx_uint16 srcX5; // source pixel (x,y) for (dstX*8+5,dstY) in Q13.3 format
	vx_uint16 srcY5;
	vx_uint16 srcX6; // source pixel (x,y) for (dstX*8+6,dstY) in Q13.3 format
	vx_uint16 srcY6;
	vx_uint16 srcX7; // source pixel (x,y) for (dstX*8+7,dstY) in Q13.3 format
	vx_uint16 srcY7;
} StitchWarpRemapEntry;

//////////////////////////////////////////////////////////////////////
//! \brief The merge cameraId packing within U016 pixel entry.
typedef struct {
	vx_uint16 camId0 : 5; // values 0..30 are valid; 31 indicates invalid cameraId
	vx_uint16 camId1 : 5; // values 0..30 are valid; 31 indicates invalid cameraId
	vx_uint16 camId2 : 5; // values 0..30 are valid; 31 indicates invalid cameraId
	vx_uint16 reserved0 : 1; // reserved (shall be zero)
} StitchMergeCamIdEntry;


/*********************************************************************
SeamFind Data Structures
**********************************************************************/
#define ENABLE_VERTICAL_SEAM 1
#define ENABLE_HORIZONTAL_SEAM 1

#define VERTICAL_SEAM 0
#define HORIZONTAL_SEAM 1
#define DIAGONAL_SEAM 2

//! \brief TheInitialize Stitch Config User Perference attributes.
typedef struct {
	vx_float32 overlap_rectangle;
	vx_float32 seam_find;
	vx_float32 seam_vertical_priority;
	vx_float32 seam_horizontal_priority;
	vx_float32 seam_frequency;
	vx_float32 seam_quality;
	vx_float32 seam_stagger;
	vx_float32 multi_band;
	vx_float32 num_bands;
} InitializeStitchAttributes;

//! \brief The Seam Size Information struct.
typedef struct {
	vx_uint32 valid_entry;
	vx_uint32 weight_entry;
	vx_uint32 accum_entry;
	vx_uint32 pref_entry;
	vx_uint32 info_entry;
	vx_uint32 path_entry;
} SeamFindSizeInfo;

//! \brief The Seam Information struct.
typedef struct {
	vx_int16 cam_id_1;		// Overlap CAM ID - 1
	vx_int16 cam_id_2;		// Overlap CAM ID - 2
	vx_int16 start_x;		// Overlap Rectangle start x
	vx_int16 end_x;			// Overlap Rectangle end x
	vx_int16 start_y;		// Overlap Rectangle start y
	vx_int16 end_y;			// Overlap Rectangle end y
	vx_int32 offset;		// Offset location in accumulate buffer
} StitchSeamFindInformation;

//! \brief The Seam Find Preference attributes.
typedef struct {
	vx_int16 type;			// Overlap type: 0 - Vertical Overlap, 1 - Hortzontal Overlap,  2 - Diagonal Overlap
	vx_int16 seam_type_num;	// Overlap type ID - vertical/horizontal overlap ID
	vx_int16 start_frame;	// Start frame to calculate the seam
	vx_int16 frequency;		// Frequency to calculate the seam		
	vx_int16 quality;		// Quality of the calculated the seam		
	vx_int16 priority;		// Priority to calculate the seam	
	vx_int16 seam_lock;		// Lock the seam after scene change is detected for n frames
	vx_int16 scene_flag;	// Scene change detection flag
} StitchSeamFindPreference;

//! \brief The valid pixel entry for Seam Find.
typedef struct {
	vx_int16 dstX;		 // destination pixel x-coordinate (integer)
	vx_int16 dstY;		 // destination pixel y-coordinate  (integer)
	vx_int16 height;	 // y - Height
	vx_int16 width;		 // x - Width
	vx_int16 OverLapX;   // Absolute Overlap destination pixel x-coordinate (integer)
	vx_int16 OverLapY;   // Absolute Overlap destination pixel y-coordinate (integer)
	vx_int16 CAMERA_ID_1;// Overlap Camera i
	vx_int16 ID;		 // Overlap Number	
} StitchSeamFindValidEntry;

//! \brief The valid pixel entry for Weight Manipulation Seam Find.
typedef struct {
	vx_int16 x;				//pixel x-coordinate (integer)
	vx_int16 y;				//pixel y-coordinate (integer)
	vx_int16 cam_id_1;		// Overlap Camera i
	vx_int16 cam_id_2;		// Overlap Camera j
	vx_int16 overlap_id;	// Overlap Number	
	vx_int16 overlap_type;	// Overlap Type: 0: Vert Seam 1: Hort Seam 2: Diag Seam
} StitchSeamFindWeightEntry;

//! \brief The Output Accum entry for Seam Find.
typedef struct{
	vx_int16 parent_x;		//pixel x-coordinate of parent (integer)
	vx_int16 parent_y;		//pixel y-coordinate of parent (integer)
	vx_int32 value;			//value accumulated
	vx_int32 propagate;		//propogate paths from start to finish
}StitchSeamFindAccumEntry;

//! \brief The path entry for Seam Find.
typedef struct {
	vx_int16 min_pixel;			//pixel x/y - coordinate (integer)
	vx_int16 weight_value_i;    //mask Value             (integer)
} StitchSeamFindPathEntry;

//! \brief The Scene Change Segments for Seam Find.
#define MAX_SEGMENTS 24
#define MAX_SEAM_BYTES 8
typedef struct{
	vx_uint8 segment[MAX_SEGMENTS][MAX_SEAM_BYTES];
}StitchSeamFindSceneEntry;

//! \brief The Output Accum entry for Seam Find.
typedef struct{
	vx_int16 parent_x;
	vx_int16 parent_y;
	vx_int32 value;
}StitchSeamFindAccum;

/*********************************************************************
Multiband blend Data Structures
**********************************************************************/
typedef struct{
	vx_image WeightPyrImgGaussian;
	vx_image DstPyrImgGaussian;
	vx_image DstPyrImgLaplacian;
	vx_image DstPyrImgLaplacianRec;
	vx_node WeightHSGNode;
	vx_node SourceHSGNode;
	vx_node UpscaleSubtractNode;
	vx_node BlendNode;
	vx_node UpscaleAddNode;
	vx_node LaplacianReconNode;
	vx_uint32 valid_array_offset;		// in number of elements
}StitchMultibandData;

//////////////////////////////////////////////////////////////////////
//! \brief The gray scale compute method modes
enum {
	STITCH_GRAY_SCALE_COMPUTE_METHOD_AVG = 0, // Use: Y = (R + G + B) / 3
	STITCH_GRAY_SCALE_COMPUTE_METHOD_DIST = 1, // Use: Y = sqrt(R*R + G*G + B*B)
};

//////////////////////////////////////////////////////////////////////
// Useful macros to detect number of parameters
#define NUM_LENS_PARAM_RECTILINEAR				(sizeof(lens_param_rectilinear) / sizeof(float))
#define NUM_LENS_PARAM_FISHEYE					(sizeof(lens_param_fisheye) / sizeof(float))
#define NUM_LENS_PARAM_BARREL					(sizeof(lens_param_barrel) / sizeof(float))
#define NUM_LENS_PARAM_CIRCULAR_FISHEYE			(sizeof(lens_param_circular_fisheye) / sizeof(float))
#define NUM_WARP_PARAM							(sizeof(camera_param_alignment) / sizeof(float))
#define NUM_RIG_PARAM							(sizeof(rig_param_orientation) / sizeof(float))

//////////////////////////////////////////////////////////////////////
//! \brief The macro for error checking from OpenVX status.
#define ERROR_CHECK_STATUS(call) { vx_status status = (call); if(status != VX_SUCCESS){ printf("ERROR: failed with status = (%d) at " __FILE__ "#%d\n", status, __LINE__); return status; }}
//! \brief The macro for error checking from OpenVX object.
#define ERROR_CHECK_OBJECT(obj)  { vx_status status = vxGetStatus((vx_reference)(obj)); if(status != VX_SUCCESS){ vxAddLogEntry((vx_reference)(obj), status, "ERROR: failed with status = (%d) at " __FILE__ "#%d\n", status, __LINE__); return status; }}
//! \brief The macro for getting the dimensions.
#define dimof(x)                    (sizeof(x)/sizeof(x[0]))

//////////////////////////////////////////////////////////////////////
//! \brief The user kernel registration functions.
vx_status color_convert_publish(vx_context context);
vx_status lens_distortion_remap_publish(vx_context context);
vx_status initialize_stitch_config_publish(vx_context context);
vx_status initialize_stitch_remap_publish(vx_context context);
vx_status warp_publish(vx_context context);
vx_status exposure_compensation_publish(vx_context context);
vx_status merge_publish(vx_context context);
vx_status exposure_comp_calcErrorFn_publish(vx_context context);
vx_status exposure_comp_solvegains_publish(vx_context context);
vx_status exposure_comp_applygains_publish(vx_context context);
vx_status alpha_blend_publish(vx_context context);
vx_status blend_2bandfilter_publish(vx_context context);
vx_status multiband_blend_publish(vx_context context);
vx_status half_scale_gaussian_publish(vx_context context);
vx_status upscale_gaussian_subtract_publish(vx_context context);
vx_status upscale_gaussian_add_publish(vx_context context);
vx_status laplacian_reconstruct_publish(vx_context context);

//SeamFind Kernels
vx_status seamfind_model_publish(vx_context context);
vx_status seamfind_scene_detect_publish(vx_context context);
vx_status seamfind_cost_generate_publish(vx_context context);
vx_status seamfind_cost_accumulate_publish(vx_context context);
vx_status seamfind_path_trace_publish(vx_context context);
vx_status seamfind_set_weights_publish(vx_context context);
vx_status seamfind_analyze_publish(vx_context context);

//////////////////////////////////////////////////////////////////////
//! \brief The utility function to get reference of a node parameter at specified index
vx_reference avxGetNodeParamRef(vx_node node, vx_uint32 index);

//! \brief The module entry point for publishing kernel.
SHARED_PUBLIC vx_status VX_API_CALL vxPublishKernels(vx_context context);

//////////////////////////////////////////////////////////////////////

/*! \brief [Graph] Creates a Initialize Stitch Config node.
* \param [in] graph The reference to the graph.
* TBD
* \see <tt>AMDOVX_KERNEL_STITCHING_INITIALIZE_STITCH_CONFIG</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchInitializeStitchConfigNode(vx_graph graph, 
	vx_uint32 num_buff_rows, vx_uint32 num_buff_cols, vx_uint32 cam_buffer_width, vx_uint32 cam_buffer_height, vx_uint32 dst_width, 
	vx_matrix rig_param, vx_array camera_param, vx_matrix InitializeStitchConfigAttr,
	vx_array valid_pixels, vx_array warp_remap, vx_array overlap_pixel, vx_matrix overlap_count,
	vx_image warp_image, vx_image exp_comp_image, vx_image weight_image,
	vx_image camera_id_image, vx_image group1_image, vx_image group2_image,
	vx_array exp_comp_calc, vx_image mask_image, vx_array overlap_rect,
	vx_array seamfind_valid, vx_array seamfind_accum, vx_array seamfind_weight, vx_array seamfind_pref, vx_array seamfind_info,
	vx_array twoband_blend);

/*! \brief [Graph] Creates a Color Convert node.
* \param [in] graph The reference to the graph.
* \param [in] input The input image.
* \param [out] output The output image.
* \see <tt>AMDOVX_KERNEL_STITCHING_COLOR_CONVERT</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchColorConvertNode(vx_graph graph, vx_image input, vx_image output);

/*! \brief [Graph] Creates a Lens Distortion node.
* \param [in] graph The reference to the graph.
* \param [in] input The input lens param matrix.
* \param [out] output The output remap table.
* \see <tt>AMDOVX_KERNEL_STITCHING_LENS_DISTORTION_REMAP</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchLensDistortionNode(vx_graph graph, vx_matrix lens_param, vx_matrix warp_param, vx_matrix rig_parm, vx_remap table, vx_array warp_ROI);

/*! \brief [Graph] Creates a Warp node.
* \param [in] graph The reference to the graph.
* \param [in] input The input computation method type.
* \param [in] input The input scalar number of cameras.
* \param [in] input The input array of StitchValidPixel.
* \param [in] input The input array of StitchWarpRemap
* \param [in] input The input image.
* \param [out] output The output image.
* \param [in] num_camera_columns The number of camera columns (optional)
* \see <tt>AMDOVX_KERNEL_STITCHING_WARP</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchWarpNode(vx_graph graph, vx_enum method, vx_uint32 num_cam,
	vx_array ValidPixelEntry, vx_array WarpRemapEntry, vx_image input, vx_image output, vx_uint32 num_camera_columns);

/*! \brief [Graph] Creates a Stitch Merge node.
* \param [in] graph The reference to the graph.
* \param [in] input The Scalar to indicate number of bands.
* \param [in] input The Array of Float32 Band Weights.
* \param [in] input The camera id image.
* \param [in] input The group1 id image.
* \param [in] input The group2 id image.
* \param [in] input The input image.
* \param [in] input The weight image.
* \param [out] output The output image.
* \see <tt>AMDOVX_KERNEL_STITCHING_MERGE</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchMergeNode(vx_graph graph, vx_uint8 numBands,
	vx_array bandWeights, vx_image camera_id_image, vx_image group1_image, vx_image group2_image, vx_image input, vx_image weight_image, vx_image output);

/*! \brief [Graph] Creates a AlphaBlend node.
* \param [in] graph The reference to the graph.
* \param [in] input_rgb Input RGB image.
* \param [in] input_rgba Input RGBX image with alpha channel.
* \param [out] output_rgb Output RGB image.
* \see <tt>AMDOVX_KERNEL_STITCHING_ALPHA_BLEND</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchAlphaBlendNode(vx_graph graph, vx_image input_rgb, vx_image input_rgba, vx_image output_rgb);

/*! \brief [Graph] Creates a Initialize Stitch Remap.
* \param [in] graph The reference to the graph.
* TBD
* \see <tt>AMDOVX_KERNEL_STITCHING_INITIALIZE_STITCH_REMAP</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchInitializeStitchRemapNode(vx_graph graph, vx_uint32 num_buff_rows,
	vx_uint32 num_buff_cols, vx_uint32 cam_buffer_width, vx_uint32 cam_buffer_height, vx_uint32 dst_width, vx_matrix rig_param, vx_array camera_param, vx_remap table);

/*! \brief [Graph] Creates a ExposureCompCalcErrorFn node.
* \param [in] graph      The reference to the graph.
* \param [in] numCameras Scalar (uint32: number of cameras)
* \param [in] input      Input image
* \param [in] exp_data   Input Array of expdata.
* \param [in] mask       Mask image.
* \param [out] out_intensity     Output matrix for sum of overlapping pixels.
* \see <tt>AMDOVX_KERNEL_STITCHING_EXPOSURE_COMP_CALC_ERROR_FUNC</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchExposureCompCalcErrorFnNode(vx_graph graph, vx_uint32 numCameras,
	vx_image input, vx_array exp_data, vx_image mask, vx_matrix out_intensity);


/*! \brief [Graph] Creates a ExposureCompSolveForGain node.
* \param [in] graph         The reference to the graph.
* \param [in] alpha         Input scalar (float32 alpha value: typically square of standard deviation of normalized gain error)
* \param [in] beta          Input scalar (float32 beta value: typically square of standard deviation of normalized intensity error)
* \param [in] in_intensity  Input matrix for sum of overlapping pixels.
* \param [in] in_count      Input matrix for count of overlapping pixels.
* \param [out] out_gains    Output array for gains.
* \see <tt>AMDOVX_KERNEL_STITCHING_EXPOSURE_COMP_SOLVE_FOR_GAIN</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchExposureCompSolveForGainNode(vx_graph graph, vx_float32 alpha,
	vx_float32 beta, vx_matrix in_intensity, vx_matrix in_count, vx_array out_gains);

/*! \brief [Graph] Creates a ExposureCompApplyGain node.
* \param [in] graph      The reference to the graph.
* \param [in] input      Input image
* \param [in] in_gains   Array of valid regions of rectangles
* \param [in] in_offsets Array of StitchExpCompCalcEntry
* \param [out] output    Exposure adjusted image.
* \see <tt>AMDOVX_KERNEL_STITCHING_EXPOSURE_COMP_SOLVE_FOR_GAIN</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchExposureCompApplyGainNode(vx_graph graph, vx_image input,
	vx_array in_gains, vx_array in_offsets, vx_image output);

/*! \brief [Graph] Creates a stitchBlendMultiBandMerge node.
* \param [in] graph         The reference to the graph.
* \param [in] num_cameras   Input Scalar (uint32: number of cameras)
* \param [in] blend_array_offs Scalar (uint32: start_offset to valid_arr in #of elements)
* \param [in] input         Src image (RGBA)
* \param [in] weight_img    Weight image(source U8 image)
* \param [in] valid_arr     Offsets/valid rect array (offsets will be useful for GPU kernel)
* \param [out] output       Reconstructed from multibands.
* \see <tt>AMDOVX_KERNEL_STITCHING_MULTIBAND_BLEND</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchMultiBandMergeNode(vx_graph graph, vx_uint32 num_cameras, vx_uint32 blend_array_offs,
	vx_image input, vx_image weight_img, vx_array valid_arr, vx_image output);

/*! \brief [Graph] Creates a stitchMultiBandHalfScaleGaussian node.
* \param [in] graph         The reference to the graph.
* \param [in] num_cameras   Scalar (uint32: number of cameras)
* \param [in] blend_array_offs  Scalar (uint32: start_offset to valid_arr in #of elements)
* \param [in] valid_arr     Offsets/valid rect array (offsets will be useful for GPU kernel)
* \param [in] input         Src image (RGBA)
* \param [out] output   Output image
* \see <tt>AMDOVX_KERNEL_STITCHING_HALF_SCALE_GAUSSIAN</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchMultiBandHalfScaleGaussianNode(vx_graph graph, vx_uint32 num_cameras, vx_uint32 blend_array_offs,
	vx_array valid_arr, vx_image input, vx_image output);

/*! \brief [Graph] Creates a stitchMultiBandUpscaleGaussianSubtract node.
* \param [in] graph The reference to the graph.
* \param [in] num_cameras Scalar (uint32: number of cameras)
* \param [in] blend_array_offs Scalar (uint32: start_offset to valid_arr in #of elements)
* \param [in] input1 Src_image1
* \param [in] input2 Src_image2
* \param [in] valid_arr Offsets/valid rect array (offsets will be useful for GPU kernel)
* \param [in/optional] Weight_img input weight image for blending
* \param [out] output Output image.
* \see <tt>AMDOVX_KERNEL_STITCHING_HALF_SCALE_GAUSSIAN</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchMultiBandUpscaleGaussianSubtractNode(vx_graph graph, vx_uint32 num_cameras, vx_uint32 blend_array_offs,
	vx_image input1, vx_image input2, vx_array valid_arr, vx_image weight_img, vx_image output);

/*! \brief [Graph] Creates a stitchMultiBandUpscaleGaussianAdd node.
* \param [in] graph The reference to the graph.
* \param [in] num_cameras scalar (uint32: number of cameras)
* \param [in] blend_array_offs scalar (uint32: start_offset to valid_arr in #of elements)
* \param [in] input1 src image1
* \param [in] input2 src image2
* \param [in] valid_arr Offsets/valid rect array (offsets will be useful for GPU kernel)
* \param [out] output (img)
* \see <tt>AMDOVX_KERNEL_STITCHING_HALF_SCALE_GAUSSIAN</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchMultiBandUpscaleGaussianAddNode(vx_graph graph, vx_uint32 num_cameras, vx_uint32 blend_array_offs,
	vx_image input1, vx_image input2, vx_array valid_arr, vx_image output);

/*! \brief [Graph] Creates a stitchMultiBandLaplacianReconstruct node.
* \param [in] graph The reference to the graph.
* \param [in] num_cameras The number of cameras
* \param [in] blend_array_offs The start_offsets to valid_arr in #of elements
* \param [in] input1 The src image1
* \param [in] input2 The src image2
* \param [in] valid_arr The offsets/valid rect array (offsets will be useful for GPU kernel)
* \param [out] output image.
* \see <tt>AMDOVX_KERNEL_STITCHING_LAPLACIAN_RECONSTRUCT</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchMultiBandLaplacianReconstructNode(vx_graph graph, vx_uint32 num_cameras, vx_uint32 blend_array_offs,
	vx_image input1, vx_image input2, vx_array valid_arr, vx_image output);

/***********************************************************************************************************************************
Seam Find
Talk to Radha : TBD
************************************************************************************************************************************/
/*! \brief [Graph] Creates a Warp U8 node.
* \param [in] graph The reference to the graph.
* \param [in] method The input computation method type.
* \param [in] num_cam The input scalar number of cameras.
* \param [in] ValidPixelEntry The input array of StitchValidPixel.
* \param [in] WarpRemapEntry The input array of StitchWarpRemap
* \param [in] input The input image.
* \param [out] output The output image.
* \param [out] output_u8 The U8 output image.
* \param [out] num_camera_columns The num of camera columns.
* \see <tt>AMDOVX_KERNEL_STITCHING_WARP</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchWarpU8Node(vx_graph graph, vx_enum method, vx_uint32 num_cam,
	vx_array ValidPixelEntry, vx_array WarpRemapEntry, vx_image input, vx_image output, vx_image output_u8, vx_uint32 num_camera_columns);


/*! \brief [Graph] Creates a SeamFind Accumulate node K0 - GPU/CPU.
* \param [in] graph The reference to the graph.
* \param [in] current_frame The input scalar current frame.
* \param [in] scene_threshold The input scalar threshold.
* \param [in] input_image The input U8 Image.
* \param [in] seam_info The input array of seam info.
* \param [out] seam_pref The array of seam preference.
* \param [out] output The array of seam scene change.
* \see <tt>AMDOVX_KERNEL_STITCHING_SEAMFIND_K0</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchSeamFindSceneDetectNode(vx_graph graph, vx_scalar current_frame, vx_scalar scene_threshold,
	vx_image input_image, vx_array seam_info, vx_array seam_pref, vx_array seam_scene_change);

/*! \brief [Graph] Creates a SeamFind Cost Generate node - K1 - GPU.
* \param [in] graph The reference to the graph.
* \param [in] executeFlag The input scalar to bypass the execution of kernel.
* \param [in] input_weight_image The input U8 weight image from Warp.
* \param [out] magnitude_image The output magnitude image.
* \param [out] phase_image The output phase image.
* \see <tt>AMDOVX_KERNEL_STITCHING_SEAMFIND_COST_GENERATE</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchSeamFindCostGenerateNode(vx_graph graph, vx_scalar executeFlag,
	vx_image input_weight_image, vx_image magnitude_image, vx_image phase_image);

/*! \brief [Graph] Creates a SeamFind Accumulate node K2 - GPU.
* \param [in] graph The reference to the graph.
* \param [in] current_frame The Current Frame.
* \param [in] output_width  The output image width.
* \param [in] output_height The output image height.
* \param [in] magnitude_img The input Magnitude Image from K1
* \param [in] phase_img     The input Phase Image from K1
* \param [in] mask_img      The input Mask Image fromInitialize Stitch Config.
* \param [in] valid_seam    The input array of valid_seam pixels
* \param [in] pref_seam     The input array of seam preference
* \param [out] output       The output seam_accum array.
* \see <tt>AMDOVX_KERNEL_STITCHING_SEAMFIND_K2</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchSeamFindK2Node(vx_graph graph, vx_scalar current_frame,
	vx_uint32 output_width, vx_uint32 output_height, vx_image magnitude_img, vx_image phase_img,
	vx_image mask_img, vx_array valid_seam, vx_array pref_seam, vx_array accum_seam);

/*! \brief [Graph] Creates a SeamFind Cost Accumulate node - K2 - GPU.
* \param [in] graph The reference to the graph.
* \param [in] current_frame The Current Frame.
* \param [in] output_width  The output image width.
* \param [in] output_height The output image height.
* \param [in] magnitude_img The input Magnitude Image from K1.
* \param [in] phase_img     The input Phase Image from K1.
* \param [in] mask_img      The input Mask Image fromInitialize Stitch Config.
* \param [in] valid_seam    The input array of valid_seam pixels.
* \param [in] pref_seam     The input array of seam preference.
* \param [in] info_seam     The input seam info array.
* \param [out] output       The output seam_accum array.
* \see <tt>AMDOVX_KERNEL_STITCHING_SEAMFIND_K2</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchSeamFindCostAccumulateNode(vx_graph graph, vx_scalar current_frame,
	vx_uint32 output_width, vx_uint32 output_height, vx_image magnitude_img, vx_image phase_img,
	vx_image mask_img, vx_array valid_seam, vx_array pref_seam, vx_array info_seam, vx_array accum_seam);

/*! \brief [Graph] Creates a SeamFind Accumulate node K3_A - GPU/CPU.
* \param [in] graph The reference to the graph.
* \param [in] current_frame The Current Frame.
* \param [in] weight_image  The input Weight Image
* \param [in] seam_info     The input seam info array.
* \param [in] seam_accum    The input seam_accum array.
* \param [in] seam_pref     The input array of seam preference
* \param [out] output       The Path Array.
* \see <tt>AMDOVX_KERNEL_STITCHING_SEAMFIND_K3_A</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchSeamFindPathTraceNode(vx_graph graph, vx_scalar current_frame, vx_image weight_image, vx_array seam_info,
	vx_array seam_accum, vx_array seam_pref, vx_array paths);

/*! \brief [Graph] Creates a SeamFind Accumulate node K3_B - GPU.
* \param [in] graph         The reference to the graph.
* \param [in] current_frame The input Current Frame.
* \param [in] NumCam        The input scalar number of cameras.
* \param [in] output_width  The input output width.
* \param [in] output_height The input output height.
* \param [in] seam_weight   The input array of seam weights.
* \param [in] seam_path     The input array of seam path .
* \param [in] seam_pref     The input array of seam preference.
* \param [out] output       The weight image.
* \see <tt>AMDOVX_KERNEL_STITCHING_SEAMFIND_K3_B</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchSeamFindSetWeightsNode(vx_graph graph, vx_scalar current_frame, vx_uint32 NumCam,
	vx_uint32 output_width, vx_uint32 output_height, vx_array seam_weight, vx_array seam_path,
	vx_array seam_pref, vx_image weight_image);

/*! \brief [Graph] Creates a SeamFind CPU Node.
* \param [in] graph         The reference to the graph.
* \param [in] numCam        The input scalar number of cameras.
* \param [in] overlap_roi   The input array of overlap_roi
* \param [in] overlap_matrix The input Overlap Matrix.
* \param [in] cost_img      The input Cost Image.
* \param [in] mask_img      The input Mask Image fromInitialize Stitch Config.
* \param [in] weight_image  The input Weight Image
* \param [out] output       The output New Weight Image.
* \see <tt>AMDOVX_KERNEL_STITCHING_SEAMFIND_MODEL</tt>
* \return <tt>\ref vx_node</tt>.
* \retval vx_node A node reference. Any possible errors preventing a successful creation should be checked using <tt>\ref vxGetStatus</tt>
*/
VX_API_ENTRY vx_node VX_API_CALL stitchSeamFindModelNode(vx_graph graph, vx_uint32 numCam,
	vx_array overlap_roi, vx_matrix overlap_matrix, vx_image cost_img,
	vx_image mask_img, vx_image weight_image, vx_image new_weight_image);

/////////////////////////////////////////////////////////////////////////////////////
/*! \brief The utility function to initialize StitchOverlapPixelEntry array for exp_comp kernel implementation
* \param[in] input Array of valid regions of rectangles
* \param[out] output Array of StitchOverlapPixelEntry .
*/
extern vx_status Compute_StitchExpCompCalcEntry(vx_rectangle_t *pValid_roi, vx_array ExpCompOut, int numCameras);
extern vx_status Compute_StitchExpCompCalcValidEntry(vx_rectangle_t *pValid_roi, vx_array pExpCompOut, int numCameras, int Dst_height);
extern vx_status Compute_StitchBlendCalcValidEntry(vx_rectangle_t *pValid_roi, vx_array blendOffs, int numCameras);
bool StitchGetEnvironmentVariable(const char * name, char * value, size_t valueSize);
vx_status simple_blend(vx_uint32 BLEND_MODE, vx_uint32 BLEND_HORIZONTAL, vx_uint32 BLEND_WIDTH, vx_image weight_image, vx_uint32 heightDstCamera, vx_uint32 numCamera);
vx_status Seamfind_CopyWeights(vx_image weight_image, vx_image new_weight_image, vx_rectangle_t *Overlap_ROI, vx_int32 *Overlap_matrix, vx_uint32 width, vx_uint32 height, vx_uint32 NumCam);
vx_status Seamfind_seamrange(vx_uint32 *seam_adjust, vx_uint32 x_dir);
vx_uint32 Compute_StitchBlendArraySize(int width, int height, int num_camera, int num_bands, vx_uint32 offset[]);
vx_status Compute_StitchMultiBandCalcValidEntry(vx_rectangle_t *pOverlap_roi, vx_array blendOffs, int numCameras, int numBands, int width, int height);
vx_node stitchCreateNode(vx_graph graph, vx_enum kernelEnum, vx_reference params[], vx_uint32 num);
vx_node stitchCreateNode(vx_graph graph, const char * kernelName, vx_reference params[], vx_uint32 num);
vx_status seamfind_utility(vx_uint32 mode, vx_uint32 eqr_width, vx_uint32 num_cam, SeamFindSizeInfo *entry_var);
vx_status seamfind_accurate_utility(vx_uint32 mode, vx_uint32 num_cam, vx_uint32 ip_width, vx_uint32 ip_height, vx_uint32 eqr_width, vx_matrix mat_rig_params, vx_array array_cam_params, SeamFindSizeInfo *entry_var);

#endif //__VX_STITCHING_H__
