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
#include <vx_ext_amd.h>
#include <sstream>
#include <stdarg.h>

// Version
#define LS_VERSION             "0.9"

//////////////////////////////////////////////////////////////////////
//! \brief The magic number for validation
#define LIVE_STITCH_MAGIC      0x600df00d

//////////////////////////////////////////////////////////////////////
//! \brief The camera configuration.
typedef struct {
	unsigned int    num_cam;       // number of cameras
	unsigned int    width, height; // individual camera width and height
	camera_params * cam_par;       // camera parameters
} camera_config;

//////////////////////////////////////////////////////////////////////
//! \brief The stitching modes
enum {
	stitching_mode_normal          = 0, // normal mode
	stitching_mode_quick_and_dirty = 1, // quick and dirty mode
};

//////////////////////////////////////////////////////////////////////
//! \brief The LoomIO module info
#define LOOMIO_MAX_LENGTH_MODULE_NAME        64
#define LOOMIO_MAX_LENGTH_KERNEL_NAME        VX_MAX_KERNEL_NAME
#define LOOMIO_MAX_LENGTH_KERNEL_ARGUMENTS   1024
#define LOOMIO_MIN_AUX_DATA_CAPACITY          256
#define LOOMIO_DEFAULT_AUX_DATA_CAPACITY     1024
#define LOOMIO_MAX_AUX_DATA_CAPACITY         8192
struct ls_loomio_info {
	char module[LOOMIO_MAX_LENGTH_MODULE_NAME];
	char kernelName[LOOMIO_MAX_LENGTH_KERNEL_NAME];
	char kernelArguments[LOOMIO_MAX_LENGTH_KERNEL_ARGUMENTS];
};

//////////////////////////////////////////////////////////////////////
//! \brief The stitch handle
struct ls_context_t {
	// magic word and status
	int  magic;                                 // should be LIVE_STITCH_MAGIC
	bool initialized;                           // true if initialized
	bool scheduled;                             // true if scheduled
	bool reinitialize_required;                 // true if reinitialize required
	bool rig_params_updated;                    // true if rig parameters updated
	bool camera_params_updated;                 // true if camera parameters updated
	bool overlay_params_updated;                // true if overlay parameters updated
	// configuration parameters
	vx_int32    stitching_mode;                 // stitching mode
	vx_uint32   num_cameras;                    // number of cameras
	vx_uint32   num_camera_rows;				// camera buffer number of rows
	vx_uint32   num_camera_columns;				// camera buffer number of cols
	vx_df_image camera_buffer_format;           // camera buffer format (VX_DF_IMAGE_UYVY/YUYV/RGB)
	vx_uint32   camera_buffer_width;            // camera buffer width
	vx_uint32   camera_buffer_height;           // camera buffer height
	camera_params * camera_par;                 // individual camera parameters
	vx_float32  camera_rgb_scale_factor;        // camera image scale factor (valid values: 1.0 and 0.5) 
	vx_uint32   camera_rgb_buffer_width;        // camera buffer width after color conversion
	vx_uint32   camera_rgb_buffer_height;       // camera buffer height after color conversion
	vx_uint32   num_overlays;                   // number of overlays
	vx_uint32   num_overlay_rows;           // overlay buffer width
	vx_uint32   num_overlay_columns;          // overlay buffer height
	vx_uint32   overlay_buffer_width;           // overlay buffer width
	vx_uint32   overlay_buffer_height;          // overlay buffer height
	camera_params * overlay_par;                // individual overlay parameters
	rig_params  rig_par;                        // rig parameters
	vx_uint32   output_buffer_width;            // output equirectangular image width
	vx_uint32   output_buffer_height;           // output equirectangular image height
	vx_df_image output_buffer_format;           // output image format (VX_DF_IMAGE_UYVY/YUYV/RGB)
	vx_float32  output_rgb_scale_factor;        // output image downscale factor (valid values: 1.0 and 0.5)
	vx_uint32   output_rgb_buffer_width;        // camera buffer width after color conversion
	vx_uint32   output_rgb_buffer_height;       // camera buffer height after color conversion
	cl_context  opencl_context;                 // OpenCL context for DGMA interop
	vx_uint32   camera_buffer_stride_in_bytes;  // stride of each row in input opencl buffer
	vx_uint32   overlay_buffer_stride_in_bytes; // stride of each row in overlay opencl buffer (optional)
	vx_uint32   output_buffer_stride_in_bytes;  // stride of each row in output opencl buffer
	// global options
	vx_uint32  EXPO_COMP, SEAM_FIND;			// exposure comp/ seam find flags from environment variable
	vx_uint32  SEAM_COST_SELECT;				// seam find cost generation flag from environment variable
	vx_uint32  SEAM_REFRESH;					// seamfind seam refresh flag from environment variable
	vx_uint32  MULTIBAND_BLEND;                 // multiband blend flag from environment variable
	// global OpenVX objects
	vx_context context;                         // OpenVX context
	vx_graph graphStitch, graphInitializeStitch;   // separate graphs for frame-level stitching and Initialize Stitch Config
	vx_graph graphOverlay;                      // graph for overlay computation
	// configuration OpenVX objects
	vx_matrix rig_par_mat, cam_par_mat;         // rig and camera parameters
	vx_array cam_par_array;						// camera parameters
	vx_array ovr_par_array;                       // overlay parameters
	// data objects
	vx_remap overlay_remap;                     // remap table for overlay
	vx_remap initialize_stitch_remap;
	vx_image Img_input, Img_output, Img_overlay;
	vx_image Img_input_rgb, Img_output_rgb, Img_overlay_rgb, Img_overlay_rgba;
	vx_node InputColorConvertNode, SimpleStitchRemapNode, OutputColorConvertNode;
	//Stitch Mode 2
	vx_array ValidPixelEntry, WarpRemapEntry, OverlapPixelEntry, valid_array, gain_array;
	vx_matrix InitializeStitchConfig_matrix, overlap_matrix, A_matrix;
	vx_image RGBY1, RGBY2, weight_image, cam_id_image, group1_image, group2_image;
	vx_node InitializeStitchConfigNode, WarpNode, ExpcompComputeGainNode, ExpcompSolveGainNode, ExpcompApplyGainNode, MergeNode;
	vx_float32 alpha, beta;                     // needed for expcomp
	vx_int32 * A_matrix_initial_value;          // needed for expcomp
	//Stitch SEAMFIND DATA OBJECTS
	vx_array overlap_rect_array, seamfind_valid_array, seamfind_weight_array, seamfind_accum_array, seamfind_pref_array, seamfind_info_array, seamfind_path_array, seamfind_scene_array;
	vx_image mask_image, u8_image, sobelx_image, sobely_image, s16_image, sobel_magnitude_image, sobel_phase_image, new_weight_image;
	vx_node SobelNode, MagnitudeNode, PhaseNode, ConvertDepthNode, SeamfindStep1Node, SeamfindStep2Node, SeamfindStep3Node, SeamfindStep4Node, SeamfindStep5Node;
	vx_scalar input_shift, current_frame, scene_threshold, flag;
	vx_int32  current_frame_value;
	vx_uint32 scene_threshold_value, SEAM_FIND_TARGET;
	//Stitch Multiband DATA objects
	vx_int32 num_bands;
	vx_array band_weights_array, blend_offsets;
	vx_image blend_mask_image;
	StitchMultibandData *pStitchMultiband;
	// LoomIO support
	vx_uint32 loomioAuxDataLength;
	vx_scalar cameraMediaConfig, overlayMediaConfig, outputMediaConfig, viewingMediaConfig;
	vx_array loomioCameraAuxData, loomioOverlayAuxData, loomioOutputAuxData, loomioViewingAuxData;
	vx_node nodeLoomIoCamera, nodeLoomIoOverlay, nodeLoomIoOutput, nodeLoomIoViewing;
	ls_loomio_info loomio_camera, loomio_output, loomio_overlay, loomio_viewing;
	FILE * loomioAuxDumpFile;
	// attributes
	vx_float32 live_stitch_attr[LIVE_STITCH_ATTR_MAX_COUNT];
};

//////////////////////////////////////////////////////////////////////
//! \brief The global attributes with default values
static bool g_live_stitch_attr_initialized = false;
static vx_float32 g_live_stitch_attr[LIVE_STITCH_ATTR_MAX_COUNT] = { 0 };
static stitch_log_callback_f g_live_stitch_log_message_callback = nullptr;

//////////////////////////////////////////////////////////////////////
//! \brief The macro for object creation error checking and reporting.
#define ERROR_CHECK_OBJECT_(call) { vx_reference obj = (vx_reference)(call); vx_status status = vxGetStatus(obj); if(status != VX_SUCCESS) { ls_printf("ERROR: OpenVX object creation failed at " __FILE__ "#%d\n", __LINE__); return status; } }
//! \brief The macro for status error checking and reporting.
#define ERROR_CHECK_STATUS_(call) {vx_status status = (call); if(status != VX_SUCCESS) { ls_printf("ERROR: OpenVX call failed with status = (%d) at " __FILE__ "#%d\n", status, __LINE__); return status; }  }
//! \brief The macro for type error checking and reporting.
#define ERROR_CHECK_TYPE_(call) { vx_enum type_ = (call); if(type_ == VX_TYPE_INVALID) { ls_printf("ERROR: OpenVX call failed with type = VX_TYPE_INVALID at " __FILE__ "#%d\n", __LINE__); return VX_ERROR_NOT_SUFFICIENT; }  }
//! \brief The macro for object creation error checking and reporting.
#define ERROR_CHECK_ALLOC_(call) { void * obj = (call); if(!obj) { ls_printf("ERROR: memory allocation failed at " __FILE__ "#%d\n", __LINE__); return VX_ERROR_NOT_ALLOCATED; } }
//! \brief The log callback.
void ls_printf(char * format, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer)-1, format, args);
	if (g_live_stitch_log_message_callback) {
		g_live_stitch_log_message_callback(buffer);
	}
	else {
		printf("%s", buffer);
		fflush(stdout);
	}
	va_end(args);
}
//! \brief The log callback.
static void VX_CALLBACK log_callback(vx_context context, vx_reference ref, vx_status status, const vx_char string[])
{
	if (g_live_stitch_log_message_callback) {
		g_live_stitch_log_message_callback(string);
		if (!(string[0] && string[strlen(string) - 1] == '\n')) g_live_stitch_log_message_callback("\n");
	}
	else {
		printf("LOG:[status=%d] %s", status, string);
		if (!(string[0] && string[strlen(string) - 1] == '\n')) printf("\n");
		fflush(stdout);
	}
}
//! \brief Function to set default values to global attributes
static void ResetLiveStitchGlobalAttributes()
{
	// set default settings only once
	if (!g_live_stitch_attr_initialized) {
		g_live_stitch_attr_initialized = true;
		memset(g_live_stitch_attr, 0, sizeof(g_live_stitch_attr));       // initialize all settings to zero
		g_live_stitch_attr[LIVE_STITCH_ATTR_EXPCOMP] = 1;                // EXPO COMP TURNED ON  - Default for mode 2
		g_live_stitch_attr[LIVE_STITCH_ATTR_SEAMFIND] = 1;               // SEAMFIND TURNED ON - Default for mode 2
		g_live_stitch_attr[LIVE_STITCH_ATTR_SEAM_REFRESH] = 1;           // SEAMFIND SEAM SCENE CHANGE TURNED ON - Default for mode 2
		g_live_stitch_attr[LIVE_STITCH_ATTR_SEAM_COST_SELECT] = 1;       // SEAMFIND Cost Generation set to - Default: 1:Optimized Sobel Mag/Phase
		g_live_stitch_attr[LIVE_STITCH_ATTR_CT_SEAM_STAGGER] = 1;        // SEAMFIND Seam Stagger set to - Default: 1
		g_live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND] = 1;              // Enable Blend
		g_live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND_NUMBANDS] = 4;     // Use 4-band Blend
		g_live_stitch_attr[LIVE_STITCH_ATTR_STITCH_MODE] = (float)stitching_mode_normal; // Normal mode
		g_live_stitch_attr[LIVE_STITCH_ATTR_INPUT_SCALE_FACTOR] = 1.0f;                  // no input scaling
		g_live_stitch_attr[LIVE_STITCH_ATTR_OUTPUT_SCALE_FACTOR] = 1.0f;                 // no output scaling
		g_live_stitch_attr[LIVE_STITCH_ATTR_ENABLE_REINITIALIZE] = 0.0f;                 // lsReinitialize disabled
		// LoomIO specific attributes
		g_live_stitch_attr[LIVE_STITCH_ATTR_IO_AUX_DATA_CAPACITY] = (float)LOOMIO_DEFAULT_AUX_DATA_CAPACITY;
	}
}
static std::vector<std::string> split(std::string str, char delimiter) {
	std::stringstream ss(str);
	std::string tok;
	std::vector<std::string> internal;
	while (std::getline(ss, tok, delimiter)) {
		internal.push_back(tok);
	}
	return internal;
}
static vx_status IsValidContext(ls_context stitch)
{
	if (!stitch || stitch->magic != LIVE_STITCH_MAGIC) return VX_ERROR_INVALID_REFERENCE;
	return VX_SUCCESS;
}
static vx_status IsValidContextAndInitialized(ls_context stitch)
{
	if (!stitch || stitch->magic != LIVE_STITCH_MAGIC) return VX_ERROR_INVALID_REFERENCE;
	if (!stitch->initialized) return VX_ERROR_NOT_ALLOCATED;
	return VX_SUCCESS;
}
static vx_status IsValidContextAndNotInitialized(ls_context stitch)
{
	if (!stitch || stitch->magic != LIVE_STITCH_MAGIC) return VX_ERROR_INVALID_REFERENCE;
	if (stitch->initialized) return VX_ERROR_NOT_SUPPORTED;
	return VX_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////
// Stitch API implementation

//! \brief Get version.
LIVE_STITCH_API_ENTRY const char * VX_API_CALL lsGetVersion()
{
	return LS_VERSION;
}

//! \brief Set callback for log messages.
LIVE_STITCH_API_ENTRY void VX_API_CALL lsGlobalSetLogCallback(stitch_log_callback_f callback)
{
	g_live_stitch_log_message_callback = callback;
}

//! \brief Set global attributes. Note that current global attributes will become default attributes for when a stitch context is created.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGlobalSetAttributes(vx_uint32 attr_offset, vx_uint32 attr_count, const vx_float32 * attr_ptr)
{
	// make sure global attributes are reset to default
	if (!g_live_stitch_attr_initialized) ResetLiveStitchGlobalAttributes();

	// bounding check
	if ((attr_offset + attr_count) > LIVE_STITCH_ATTR_MAX_COUNT)
		return VX_ERROR_INVALID_DIMENSION;

	// set global live_stitch_attr[]
	memcpy(&g_live_stitch_attr[attr_offset], attr_ptr, attr_count * sizeof(vx_float32));
	return VX_SUCCESS;
}

//! \brief Get global attributes. Note that current global attributes will become default attributes for when a stitch context is created.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGlobalGetAttributes(vx_uint32 attr_offset, vx_uint32 attr_count, vx_float32 * attr_ptr)
{
	// make sure global attributes are reset to default
	if (!g_live_stitch_attr_initialized) ResetLiveStitchGlobalAttributes();

	// bounding check
	if ((attr_offset + attr_count) > LIVE_STITCH_ATTR_MAX_COUNT)
		return VX_ERROR_INVALID_DIMENSION;

	// get global live_stitch_attr[]
	memcpy(attr_ptr, &g_live_stitch_attr[attr_offset], attr_count * sizeof(vx_float32));
	return VX_SUCCESS;
}

//! \brief Create stitch context.
LIVE_STITCH_API_ENTRY ls_context VX_API_CALL lsCreateContext()
{
	// make sure global attributes are reset to default
	if (!g_live_stitch_attr_initialized) ResetLiveStitchGlobalAttributes();

	/////////////////////////////////////////////////////////
	// create stitch handle and initialize live_stitch_attr
	ls_context stitch = new ls_context_t;
	if (stitch) {
		memset(stitch, 0, sizeof(ls_context_t));
		memcpy(stitch->live_stitch_attr, g_live_stitch_attr, sizeof(stitch->live_stitch_attr));
		stitch->magic = LIVE_STITCH_MAGIC;
	}
	return stitch;
}

//! \brief Set context specific attributes.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetAttributes(ls_context stitch, vx_uint32 attr_offset, vx_uint32 attr_count, const vx_float32 * attr_ptr)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));

	// bounding check
	if ((attr_offset + attr_count) > LIVE_STITCH_ATTR_MAX_COUNT)
		return VX_ERROR_INVALID_DIMENSION;

	for (vx_uint32 attr = attr_offset; attr < (attr_offset + attr_count); attr++) {
		if (attr == LIVE_STITCH_ATTR_SEAM_THRESHOLD) {
			// update scalar of seafind k0 kernel
			stitch->scene_threshold_value = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_SEAM_THRESHOLD];
			if (stitch->scene_threshold) {
				vx_status status = vxWriteScalarValue(stitch->scene_threshold, &stitch->scene_threshold_value);
				if (status != VX_SUCCESS)
					return status;
			}
		}
		else {
			// not all attributes are supported
			return VX_ERROR_NOT_SUPPORTED;
		}
	}

	// update live_stitch_attr to reflect recent changes
	memcpy(&stitch->live_stitch_attr[attr_offset], attr_ptr, attr_count * sizeof(vx_float32));
	return VX_SUCCESS;
}

//! \brief Get context specific attributes.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetAttributes(ls_context stitch, vx_uint32 attr_offset, vx_uint32 attr_count, vx_float32 * attr_ptr)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));

	// bounding check
	if ((attr_offset + attr_count) > LIVE_STITCH_ATTR_MAX_COUNT)
		return VX_ERROR_INVALID_DIMENSION;

	// get live_stitch_attr
	memcpy(attr_ptr, &stitch->live_stitch_attr[attr_offset], attr_count * sizeof(vx_float32));
	return VX_SUCCESS;
}

//! \brief Set stitch configuration.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOpenVXContext(ls_context stitch, vx_context  openvx_context)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	if (stitch->context) {
		ls_printf("ERROR: lsSetOpenVXContext: OpenVX context already exists\n");
		return VX_ERROR_NOT_SUPPORTED;
	}
	stitch->context = openvx_context;
	if (stitch->opencl_context) {
		ERROR_CHECK_STATUS_(vxSetContextAttribute(stitch->context, VX_CONTEXT_ATTRIBUTE_AMD_OPENCL_CONTEXT, &stitch->opencl_context, sizeof(cl_context)));
	}
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOpenCLContext(ls_context stitch, cl_context  opencl_context)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	stitch->opencl_context = opencl_context;
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetRigParams(ls_context stitch, const rig_params * par)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (stitch->initialized && stitch->live_stitch_attr[LIVE_STITCH_ATTR_ENABLE_REINITIALIZE] == 0.0f) {
		ls_printf("ERROR: lsSetRigParams: lsReinitialize has been disabled\n");
		return VX_ERROR_NOT_SUPPORTED;
	}
	memcpy(&stitch->rig_par, par, sizeof(stitch->rig_par));
	// check and mark whether reinitialize is required
	if (stitch->initialized) {
		stitch->reinitialize_required = true;
		stitch->rig_params_updated = true;
	}
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraConfig(ls_context stitch, vx_uint32 num_camera_rows, vx_uint32 num_camera_columns, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	if (buffer_format != VX_DF_IMAGE_UYVY && buffer_format != VX_DF_IMAGE_YUYV && buffer_format != VX_DF_IMAGE_RGB) {
		ls_printf("ERROR: lsSetCameraConfig: only UYVY/YUYV/RGB buffer formats are allowed\n");
		return VX_ERROR_INVALID_FORMAT;
	}
	// check and set camera scale factor from attributes
	stitch->camera_rgb_scale_factor = 1.0f;
	if (stitch->live_stitch_attr[LIVE_STITCH_ATTR_INPUT_SCALE_FACTOR] > 0.0f) {
		stitch->camera_rgb_scale_factor = stitch->live_stitch_attr[LIVE_STITCH_ATTR_INPUT_SCALE_FACTOR];
	}
	if (stitch->camera_rgb_scale_factor != 0.5f && stitch->camera_rgb_scale_factor != 1) {
		ls_printf("ERROR: Input Scale Factor of 0.5 or 1 only supported in this Release\n");
		return VX_ERROR_INVALID_PARAMETERS;
	}
	if (stitch->camera_buffer_format == VX_DF_IMAGE_RGB && stitch->camera_rgb_scale_factor != 1) {
		ls_printf("ERROR: Input Scale Factor of 1 only supported with RGB input format in this Release\n");
		return VX_ERROR_INVALID_PARAMETERS;
	}
	// check num rows and columns
	if (num_camera_rows < 1 || num_camera_columns < 1 ||
		(buffer_width % num_camera_columns) != 0 ||
		(buffer_height % num_camera_rows) != 0)
	{
		ls_printf("ERROR: lsSetCameraConfig: dimensions are is not multiple of camera rows & columns\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	// check that camera dimensions are multiples of 16x2
	if (((buffer_width / num_camera_columns) % 16) != 0 || ((buffer_height / num_camera_rows) % 2) != 0) {
		ls_printf("ERROR: lsSetCameraConfig: camera dimensions are required to be multiple of 16x2\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	// set configuration parameters
	stitch->num_cameras = num_camera_rows * num_camera_columns;
	stitch->num_camera_rows = num_camera_rows;
	stitch->num_camera_columns = num_camera_columns;
	stitch->camera_buffer_format = buffer_format;
	stitch->camera_buffer_width = buffer_width;
	stitch->camera_buffer_height = buffer_height;
	ERROR_CHECK_ALLOC_(stitch->camera_par = new camera_params[stitch->num_cameras]());
	stitch->camera_rgb_buffer_width = (vx_uint32)(stitch->camera_rgb_scale_factor * stitch->camera_buffer_width);
	stitch->camera_rgb_buffer_height = (vx_uint32)(stitch->camera_rgb_scale_factor * stitch->camera_buffer_height);
	// set default orientations
	for (vx_uint32 i = 0; i < stitch->num_cameras; i++) {
		stitch->camera_par[i].focal.yaw = -180.0f + 360.0f * (float)i / (float)stitch->num_cameras;
	}
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputConfig(ls_context stitch, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	if (buffer_format != VX_DF_IMAGE_UYVY && buffer_format != VX_DF_IMAGE_YUYV && buffer_format != VX_DF_IMAGE_RGB) {
		ls_printf("ERROR: lsSetOutputConfig: only UYVY/YUYV/RGB buffer formats are allowed\n");
		return VX_ERROR_INVALID_FORMAT;
	}
	if (buffer_width != (buffer_height * 2)) {
		ls_printf("ERROR: lsSetOutputConfig: buffer_width should be 2 times buffer_height\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	// get output scale factor and check it's validity
	stitch->output_rgb_scale_factor = 1.0f;
	if (stitch->live_stitch_attr[LIVE_STITCH_ATTR_OUTPUT_SCALE_FACTOR] > 0.0f) {
		stitch->output_rgb_scale_factor = stitch->live_stitch_attr[LIVE_STITCH_ATTR_OUTPUT_SCALE_FACTOR];
	}
	if (stitch->output_rgb_scale_factor != 0.5f && stitch->output_rgb_scale_factor != 1) {
		ls_printf("ERROR: Output Scale Factor of 0.5 or 1 only supported in this Release\n");
		return VX_ERROR_INVALID_PARAMETERS;
	}
	if (buffer_format == VX_DF_IMAGE_RGB && stitch->output_rgb_scale_factor != 1) {
		ls_printf("ERROR: Output Scale Factor of 1 only supported with RGB output format in this Release\n");
		return VX_ERROR_INVALID_PARAMETERS;
	}
	// check that dimensions are multiples of 16x2
	if ((buffer_width % 16) != 0 || (buffer_height % 2) != 0) {
		ls_printf("ERROR: lsSetOutputConfig: output dimensions are required to be multiple of 16x2\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	// set configuration parameters
	stitch->output_buffer_format = buffer_format;
	stitch->output_buffer_width = buffer_width;
	stitch->output_buffer_height = buffer_height;
	stitch->output_rgb_buffer_width = (vx_uint32)(stitch->output_buffer_width / stitch->output_rgb_scale_factor);
	stitch->output_rgb_buffer_height = (vx_uint32)(stitch->output_buffer_height / stitch->output_rgb_scale_factor);

	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayConfig(ls_context stitch, vx_uint32 num_overlay_rows, vx_uint32 num_overlay_columns, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	if (buffer_format != VX_DF_IMAGE_RGBX) {
		ls_printf("ERROR: lsSetOverlayConfig: only RGBX buffer formats are allowed\n");
		return VX_ERROR_INVALID_FORMAT;
	}
	// check num rows and columns
	if (num_overlay_rows < 1 || num_overlay_columns < 1 ||
		(buffer_width % num_overlay_columns) != 0 ||
		(buffer_height % num_overlay_rows) != 0)
	{
		ls_printf("ERROR: lsSetOverlayConfig: dimensions are is not multiple of overlay rows and columns\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	// check that overlay dimensions are multiples of 16x2
	if (((buffer_width / num_overlay_columns) % 16) != 0 || ((buffer_height / num_overlay_rows) % 2) != 0) {
		ls_printf("ERROR: lsSetOverlayConfig: overlay dimensions are required to be multiple of 16x2\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	// set configuration parameters
	stitch->num_overlays = num_overlay_rows * num_overlay_columns;
	stitch->num_overlay_rows = num_overlay_rows;
	stitch->num_overlay_columns = num_overlay_columns;
	stitch->overlay_buffer_width = buffer_width;
	stitch->overlay_buffer_height = buffer_height;
	ERROR_CHECK_ALLOC_(stitch->overlay_par = new camera_params[stitch->num_overlays]());
	// set default orientations
	stitch->overlay_par[0].focal.pitch = -90.0f;
	for (vx_uint32 i = 1; i < stitch->num_overlays; i++) {
		// set default overlay locations
		stitch->overlay_par[i].focal.pitch = -90.0f + 180.0f * (float)i / (float)(stitch->num_overlays-1);
	}
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraParams(ls_context stitch, vx_uint32 cam_index, const camera_params * par)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (cam_index >= stitch->num_cameras) {
		ls_printf("ERROR: lsSetCameraParams: invalid camera index (%d)\n", cam_index);
		return VX_ERROR_INVALID_VALUE;
	}
	if (stitch->initialized && stitch->live_stitch_attr[LIVE_STITCH_ATTR_ENABLE_REINITIALIZE] == 0.0f) {
		ls_printf("ERROR: lsSetCameraParams: lsReinitialize has been disabled\n");
		return VX_ERROR_NOT_SUPPORTED;
	}
	memcpy(&stitch->camera_par[cam_index], par, sizeof(camera_params));
	// check and mark whether reinitialize is required
	if (stitch->initialized) {
		stitch->reinitialize_required = true;
		stitch->camera_params_updated = true;
	}
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayParams(ls_context stitch, vx_uint32 overlay_index, const camera_params * par)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (overlay_index >= stitch->num_overlays) {
		ls_printf("ERROR: lsSetOverlayParams: invalid overlay index (%d)\n", overlay_index);
		return VX_ERROR_INVALID_VALUE;
	}
	if (stitch->initialized && stitch->live_stitch_attr[LIVE_STITCH_ATTR_ENABLE_REINITIALIZE] == 0.0f) {
		ls_printf("ERROR: lsSetOverlayParams: lsReinitialize has been disabled\n");
		return VX_ERROR_NOT_SUPPORTED;
	}
	memcpy(&stitch->overlay_par[overlay_index], par, sizeof(camera_params));
	// check and mark whether reinitialize is required
	if (stitch->initialized) {
		stitch->reinitialize_required = true;
		stitch->overlay_params_updated = true;
	}
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraBufferStride(ls_context stitch, vx_uint32 camera_buffer_stride_in_bytes)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	if ((camera_buffer_stride_in_bytes % 16) != 0) {
		ls_printf("ERROR: lsSetCameraBufferStride: stride has to be a multiple of 16\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	stitch->camera_buffer_stride_in_bytes = camera_buffer_stride_in_bytes;
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputBufferStride(ls_context stitch, vx_uint32 output_buffer_stride_in_bytes)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	if ((output_buffer_stride_in_bytes % 16) != 0) {
		ls_printf("ERROR: lsSetOutputBufferStride: stride has to be a multiple of 16\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	stitch->output_buffer_stride_in_bytes = output_buffer_stride_in_bytes;
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayBufferStride(ls_context stitch, vx_uint32 overlay_buffer_stride_in_bytes)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	if ((overlay_buffer_stride_in_bytes % 16) != 0) {
		ls_printf("ERROR: lsSetOverlayBufferStride: stride has to be a multiple of 16\n");
		return VX_ERROR_INVALID_DIMENSION;
	}
	stitch->overlay_buffer_stride_in_bytes = overlay_buffer_stride_in_bytes;
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraModule(ls_context stitch, const char * module, const char * kernelName, const char * kernelArguments)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	strncpy(stitch->loomio_camera.module, module, LOOMIO_MAX_LENGTH_MODULE_NAME-1);
	strncpy(stitch->loomio_camera.kernelName, kernelName, LOOMIO_MAX_LENGTH_KERNEL_NAME-1);
	strncpy(stitch->loomio_camera.kernelArguments, kernelArguments, LOOMIO_MAX_LENGTH_KERNEL_ARGUMENTS-1);
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputModule(ls_context stitch, const char * module, const char * kernelName, const char * kernelArguments)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	strncpy(stitch->loomio_output.module, module, LOOMIO_MAX_LENGTH_MODULE_NAME-1);
	strncpy(stitch->loomio_output.kernelName, kernelName, LOOMIO_MAX_LENGTH_KERNEL_NAME-1);
	strncpy(stitch->loomio_output.kernelArguments, kernelArguments, LOOMIO_MAX_LENGTH_KERNEL_ARGUMENTS-1);
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayModule(ls_context stitch, const char * module, const char * kernelName, const char * kernelArguments)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	strncpy(stitch->loomio_overlay.module, module, LOOMIO_MAX_LENGTH_MODULE_NAME-1);
	strncpy(stitch->loomio_overlay.kernelName, kernelName, LOOMIO_MAX_LENGTH_KERNEL_NAME-1);
	strncpy(stitch->loomio_overlay.kernelArguments, kernelArguments, LOOMIO_MAX_LENGTH_KERNEL_ARGUMENTS-1);
	return VX_SUCCESS;
}

LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetViewingModule(ls_context stitch, const char * module, const char * kernelName, const char * kernelArguments)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));
	strncpy(stitch->loomio_viewing.module, module, LOOMIO_MAX_LENGTH_MODULE_NAME - 1);
	strncpy(stitch->loomio_viewing.kernelName, kernelName, LOOMIO_MAX_LENGTH_KERNEL_NAME - 1);
	strncpy(stitch->loomio_viewing.kernelArguments, kernelArguments, LOOMIO_MAX_LENGTH_KERNEL_ARGUMENTS - 1);
	return VX_SUCCESS;
}

//! \brief initialize the stitch context.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsInitialize(ls_context stitch)
{
	ERROR_CHECK_STATUS_(IsValidContextAndNotInitialized(stitch));

	/////////////////////////////////////////////////////////
	// pick default stitch mode and aux data length
	stitch->stitching_mode = stitching_mode_normal;
	if (stitch->live_stitch_attr[LIVE_STITCH_ATTR_STITCH_MODE] == (float)stitching_mode_quick_and_dirty)
		stitch->stitching_mode = stitching_mode_quick_and_dirty;
	stitch->loomioAuxDataLength = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_IO_AUX_DATA_CAPACITY];
	if (stitch->loomioAuxDataLength < (float)LOOMIO_MIN_AUX_DATA_CAPACITY ||
		stitch->loomioAuxDataLength > (float)LOOMIO_MAX_AUX_DATA_CAPACITY ||
		(stitch->loomioAuxDataLength & 15) != 0)
	{
		ls_printf("ERROR: lsInitialize: invalid LIVE_STITCH_ATTR_IO_AUX_DATA_CAPACITY attribute value: %d\n", stitch->loomioAuxDataLength);
		return VX_ERROR_INVALID_PARAMETERS;
	}

	/////////////////////////////////////////////////////////
	// create and initialize OpenVX context and graphs
	if (!stitch->context) {
		stitch->context = vxCreateContext();
		if (stitch->opencl_context) {
			ERROR_CHECK_STATUS_(vxSetContextAttribute(stitch->context, VX_CONTEXT_ATTRIBUTE_AMD_OPENCL_CONTEXT, &stitch->opencl_context, sizeof(cl_context)));
		}
	}
	ERROR_CHECK_OBJECT_(stitch->context);
	vxRegisterLogCallback(stitch->context, log_callback, vx_false_e);
	ERROR_CHECK_STATUS_(vxPublishKernels(stitch->context));
	ERROR_CHECK_OBJECT_(stitch->graphStitch = vxCreateGraph(stitch->context));
	ERROR_CHECK_OBJECT_(stitch->graphInitializeStitch = vxCreateGraph(stitch->context));
	if (stitch->live_stitch_attr[LIVE_STITCH_ATTR_PROFILER] == 2.0f) {
		ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->graphStitch, VX_DIRECTIVE_AMD_ENABLE_PROFILE_CAPTURE));
	}

	// create and initialize rig and camera param objects
	ERROR_CHECK_OBJECT_(stitch->rig_par_mat = vxCreateMatrix(stitch->context, VX_TYPE_FLOAT32, sizeof(rig_params) / sizeof(vx_float32), 1));
	ERROR_CHECK_OBJECT_(stitch->cam_par_mat = vxCreateMatrix(stitch->context, VX_TYPE_FLOAT32, sizeof(camera_params) / sizeof(vx_float32), stitch->num_cameras));
	ERROR_CHECK_STATUS_(vxCopyMatrix(stitch->rig_par_mat, &stitch->rig_par, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST));
	ERROR_CHECK_STATUS_(vxCopyMatrix(stitch->cam_par_mat, stitch->camera_par, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST));

	vx_enum CameraParamsType;
	ERROR_CHECK_TYPE_(CameraParamsType = vxRegisterUserStruct(stitch->context, sizeof(camera_params)));
	ERROR_CHECK_OBJECT_(stitch->cam_par_array = vxCreateArray(stitch->context, CameraParamsType, stitch->num_cameras));
	ERROR_CHECK_STATUS_(vxTruncateArray(stitch->cam_par_array, 0));
	ERROR_CHECK_STATUS_(vxAddArrayItems(stitch->cam_par_array, stitch->num_cameras, stitch->camera_par, sizeof(camera_params)));

	// creating OpenVX image objects for input & output OpenCL buffers
	if (strlen(stitch->loomio_camera.kernelName) > 0) {
		// load OpenVX module (if specified)
		if (strlen(stitch->loomio_camera.module) > 0) {
			vx_status status = vxLoadKernels(stitch->context, stitch->loomio_camera.module);
			if (status != VX_SUCCESS) {
				ls_printf("ERROR: lsInitialize: vxLoadKernels(%s) failed (%d)\n", stitch->loomio_camera.module, status);
				return VX_ERROR_INVALID_PARAMETERS;
			}
		}
		// instantiate specified node into the graph
		ERROR_CHECK_OBJECT_(stitch->cameraMediaConfig = vxCreateScalar(stitch->context, VX_TYPE_STRING_AMD, stitch->loomio_camera.kernelArguments));
		ERROR_CHECK_OBJECT_(stitch->Img_input = vxCreateVirtualImage(stitch->graphStitch, stitch->camera_buffer_width, stitch->camera_buffer_height, stitch->camera_buffer_format));
		ERROR_CHECK_OBJECT_(stitch->loomioCameraAuxData = vxCreateArray(stitch->context, VX_TYPE_UINT8, stitch->loomioAuxDataLength));
		vx_reference params[] = {
			(vx_reference)stitch->cameraMediaConfig,
			(vx_reference)stitch->Img_input,
			(vx_reference)stitch->loomioCameraAuxData,
		};
		ERROR_CHECK_OBJECT_(stitch->nodeLoomIoCamera = stitchCreateNode(stitch->graphStitch, stitch->loomio_camera.kernelName, params, dimof(params)));
	}
	else {
		// need image created from OpenCL handle
		vx_imagepatch_addressing_t addr_in = { 0 };
		void *ptr[1] = { nullptr };
		addr_in.dim_x = stitch->camera_buffer_width;
		addr_in.dim_y = (stitch->camera_buffer_height);
		addr_in.stride_x = (stitch->camera_buffer_format == VX_DF_IMAGE_RGB) ? 3 : 2;
		addr_in.stride_y = stitch->camera_buffer_stride_in_bytes;
		if(addr_in.stride_y == 0) addr_in.stride_y = addr_in.stride_x * addr_in.dim_x;
		ERROR_CHECK_OBJECT_(stitch->Img_input = vxCreateImageFromHandle(stitch->context, stitch->camera_buffer_format, &addr_in, ptr, VX_MEMORY_TYPE_OPENCL));
	}
	if (strlen(stitch->loomio_output.kernelName) > 0) {
		// load OpenVX module (if specified)
		if (strlen(stitch->loomio_output.module) > 0) {
			vx_status status = vxLoadKernels(stitch->context, stitch->loomio_output.module);
			if (status != VX_SUCCESS) {
				ls_printf("ERROR: lsInitialize: vxLoadKernels(%s) failed (%d)\n", stitch->loomio_output.module, status);
				return VX_ERROR_INVALID_PARAMETERS;
			}
		}
		// instantiate specified node into the graph
		ERROR_CHECK_OBJECT_(stitch->outputMediaConfig = vxCreateScalar(stitch->context, VX_TYPE_STRING_AMD, stitch->loomio_output.kernelArguments));
		stitch->Img_output = vxCreateVirtualImage(stitch->graphStitch,
			(vx_uint32)(stitch->output_rgb_scale_factor * stitch->output_buffer_width), 
			(vx_uint32)(stitch->output_rgb_scale_factor * stitch->output_buffer_height),
			stitch->output_buffer_format);
		ERROR_CHECK_OBJECT_(stitch->Img_output);
		vx_uint32 zero = 0;
		ERROR_CHECK_OBJECT_(stitch->loomioOutputAuxData = vxCreateArray(stitch->context, VX_TYPE_UINT8, stitch->loomioAuxDataLength));
		vx_reference params[] = {
			(vx_reference)stitch->outputMediaConfig,
			(vx_reference)stitch->Img_output,
			(vx_reference)stitch->loomioCameraAuxData,
			(vx_reference)stitch->loomioOutputAuxData,
		};
		ERROR_CHECK_OBJECT_(stitch->nodeLoomIoOutput = stitchCreateNode(stitch->graphStitch, stitch->loomio_output.kernelName, params, dimof(params)));
	}
	else {
		// need image created from OpenCL handle
		vx_imagepatch_addressing_t addr_out = { 0 };
		void *ptr[1] = { nullptr };
		addr_out.dim_x = (vx_uint32)(stitch->output_rgb_scale_factor * stitch->output_buffer_width);
		addr_out.dim_y = (vx_uint32)(stitch->output_rgb_scale_factor * stitch->output_buffer_height);
		addr_out.stride_x = (stitch->output_buffer_format == VX_DF_IMAGE_RGB) ? 3 : 2;
		addr_out.stride_y = stitch->output_buffer_stride_in_bytes;
		if (addr_out.stride_y == 0) addr_out.stride_y = addr_out.stride_x * addr_out.dim_x;
		ERROR_CHECK_OBJECT_(stitch->Img_output = vxCreateImageFromHandle(stitch->context, stitch->output_buffer_format, &addr_out, ptr, VX_MEMORY_TYPE_OPENCL));
	}
	// create temporary images when extra color conversion is needed
	if (stitch->camera_buffer_format != VX_DF_IMAGE_RGB) {
		ERROR_CHECK_OBJECT_(stitch->Img_input_rgb = vxCreateVirtualImage(stitch->graphStitch, stitch->camera_rgb_buffer_width, stitch->camera_rgb_buffer_height, VX_DF_IMAGE_RGB));
	}
	if (stitch->output_buffer_format != VX_DF_IMAGE_RGB) {
		vx_uint32 output_img_width = (vx_uint32)(stitch->output_rgb_scale_factor * stitch->output_buffer_width);
		vx_uint32 output_img_height = (vx_uint32)(stitch->output_rgb_scale_factor * stitch->output_buffer_height);
		ERROR_CHECK_OBJECT_(stitch->Img_output_rgb = vxCreateVirtualImage(stitch->graphStitch, output_img_width, output_img_height, VX_DF_IMAGE_RGB));
	}

	if (stitch->num_overlays > 0) {
		// create and initialize rig and camera param objects
		vx_enum OverlayParamsType;
		ERROR_CHECK_TYPE_(OverlayParamsType = vxRegisterUserStruct(stitch->context, sizeof(camera_params)));
		ERROR_CHECK_OBJECT_(stitch->ovr_par_array = vxCreateArray(stitch->context, OverlayParamsType, stitch->num_overlays));
		ERROR_CHECK_STATUS_(vxTruncateArray(stitch->ovr_par_array, 0));
		ERROR_CHECK_STATUS_(vxAddArrayItems(stitch->ovr_par_array, stitch->num_overlays, stitch->overlay_par, sizeof(camera_params)));
		// create overlay image
		if (strlen(stitch->loomio_overlay.kernelName) > 0) {
			// load OpenVX module (if specified)
			if (strlen(stitch->loomio_overlay.module) > 0) {
				vx_status status = vxLoadKernels(stitch->context, stitch->loomio_overlay.module);
				if (status != VX_SUCCESS) {
					ls_printf("ERROR: lsInitialize: vxLoadKernels(%s) failed (%d)\n", stitch->loomio_overlay.module, status);
					return VX_ERROR_INVALID_PARAMETERS;
				}
			}
			// instantiate specified node into the graph
			ERROR_CHECK_OBJECT_(stitch->overlayMediaConfig = vxCreateScalar(stitch->context, VX_TYPE_STRING_AMD, stitch->loomio_overlay.kernelArguments));
			ERROR_CHECK_OBJECT_(stitch->Img_overlay = vxCreateVirtualImage(stitch->graphStitch, stitch->overlay_buffer_width, stitch->overlay_buffer_height, VX_DF_IMAGE_RGBX));
			ERROR_CHECK_OBJECT_(stitch->loomioOverlayAuxData = vxCreateArray(stitch->context, VX_TYPE_UINT8, stitch->loomioAuxDataLength));
			vx_reference params[] = {
				(vx_reference)stitch->overlayMediaConfig,
				(vx_reference)stitch->Img_overlay,
				(vx_reference)stitch->loomioOverlayAuxData,
			};
			ERROR_CHECK_OBJECT_(stitch->nodeLoomIoOverlay = stitchCreateNode(stitch->graphStitch, stitch->loomio_overlay.kernelName, params, dimof(params)));
		}
		else {
			// need image created from OpenCL handle
			vx_imagepatch_addressing_t addr_overlay = { 0 };
			void *ptr_overlay[1] = { nullptr };
			addr_overlay.dim_x = stitch->overlay_buffer_width;
			addr_overlay.dim_y = (stitch->overlay_buffer_height);
			addr_overlay.stride_x = 4;
			addr_overlay.stride_y = stitch->overlay_buffer_stride_in_bytes;
			if (addr_overlay.stride_y == 0) addr_overlay.stride_y = addr_overlay.stride_x * addr_overlay.dim_x;
			ERROR_CHECK_OBJECT_(stitch->Img_overlay = vxCreateImageFromHandle(stitch->context, VX_DF_IMAGE_RGBX, &addr_overlay, ptr_overlay, VX_MEMORY_TYPE_OPENCL));
		}
		// create remap table object and image for overlay warp
		ERROR_CHECK_OBJECT_(stitch->overlay_remap = vxCreateRemap(stitch->context, stitch->overlay_buffer_width, stitch->overlay_buffer_height, stitch->output_rgb_buffer_width, stitch->output_rgb_buffer_height));
		ERROR_CHECK_OBJECT_(stitch->Img_overlay_rgb = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, stitch->output_rgb_buffer_height, VX_DF_IMAGE_RGB));
		ERROR_CHECK_OBJECT_(stitch->Img_overlay_rgba = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, stitch->output_rgb_buffer_height, VX_DF_IMAGE_RGBX));
		// build and verify graphOverlay using stitchInitializeStitchRemapNode
		ERROR_CHECK_OBJECT_(stitch->graphOverlay = vxCreateGraph(stitch->context));
		vx_node node;
		ERROR_CHECK_OBJECT_(node = stitchInitializeStitchRemapNode(stitch->graphOverlay, stitch->num_overlay_rows, stitch->num_overlay_columns, stitch->overlay_buffer_width, stitch->overlay_buffer_height, stitch->output_rgb_buffer_width, stitch->rig_par_mat, stitch->ovr_par_array, stitch->overlay_remap));
		ERROR_CHECK_STATUS_(vxReleaseNode(&node));
		ERROR_CHECK_STATUS_(vxVerifyGraph(stitch->graphOverlay));
		// execute graphOverlay to initialize remap table
		ERROR_CHECK_STATUS_(vxProcessGraph(stitch->graphOverlay));
	}

	////////////////////////////////////////////////////////////////////////
	// build the input and output processing parts of stitch graph
	vx_image rgb_input = stitch->Img_input;
	if (stitch->camera_buffer_format != VX_DF_IMAGE_RGB) {
		// needs input color conversion
		stitch->InputColorConvertNode = stitchColorConvertNode(stitch->graphStitch, rgb_input, stitch->Img_input_rgb);
		ERROR_CHECK_OBJECT_(stitch->InputColorConvertNode);
		rgb_input = stitch->Img_input_rgb;
	}
	vx_image rgb_output = stitch->Img_output;
	if (stitch->output_buffer_format != VX_DF_IMAGE_RGB) {
		// needs output color conversion
		stitch->OutputColorConvertNode = stitchColorConvertNode(stitch->graphStitch, stitch->Img_output_rgb, rgb_output);
		ERROR_CHECK_OBJECT_(stitch->OutputColorConvertNode);
		rgb_output = stitch->Img_output_rgb;
	}
	if (stitch->Img_overlay) {
		// need add overlay
		vx_node node = vxRemapNode(stitch->graphStitch, stitch->Img_overlay, stitch->overlay_remap, VX_INTERPOLATION_TYPE_BILINEAR, stitch->Img_overlay_rgba);
		ERROR_CHECK_OBJECT_(node);
		ERROR_CHECK_STATUS_(vxReleaseNode(&node));
		node = stitchAlphaBlendNode(stitch->graphStitch, stitch->Img_overlay_rgb, stitch->Img_overlay_rgba, rgb_output);
		ERROR_CHECK_OBJECT_(node);
		ERROR_CHECK_STATUS_(vxReleaseNode(&node));
		rgb_output = stitch->Img_overlay_rgb;
	}
	if (strlen(stitch->loomio_viewing.kernelName) > 0) {
		// load OpenVX module (if specified)
		if (strlen(stitch->loomio_viewing.module) > 0) {
			vx_status status = vxLoadKernels(stitch->context, stitch->loomio_viewing.module);
			if (status != VX_SUCCESS) {
				ls_printf("ERROR: lsInitialize: vxLoadKernels(%s) failed (%d)\n", stitch->loomio_viewing.module, status);
				return VX_ERROR_INVALID_PARAMETERS;
			}
		}
		// instantiate specified node into the graph
		vx_uint32 zero = 0;
		ERROR_CHECK_OBJECT_(stitch->viewingMediaConfig = vxCreateScalar(stitch->context, VX_TYPE_STRING_AMD, stitch->loomio_viewing.kernelArguments));
		ERROR_CHECK_OBJECT_(stitch->loomioViewingAuxData = vxCreateArray(stitch->context, VX_TYPE_UINT8, stitch->loomioAuxDataLength));
		vx_reference params[] = {
			(vx_reference)stitch->viewingMediaConfig,
			(vx_reference)rgb_output,
			(vx_reference)stitch->loomioCameraAuxData,
			(vx_reference)stitch->loomioViewingAuxData,
		};
		ERROR_CHECK_OBJECT_(stitch->nodeLoomIoViewing = stitchCreateNode(stitch->graphStitch, stitch->loomio_viewing.kernelName, params, dimof(params)));
	}

	/***********************************************************************************************************************************
	Quick Stitch mode -> Simple stitch
	************************************************************************************************************************************/
	if (stitch->stitching_mode == stitching_mode_quick_and_dirty)
	{
		////////////////////////////////////////////////////////////////////////
		// create and process graphInitializeStitch using stitchInitializeStitchRemapNode
		////////////////////////////////////////////////////////////////////////
		// create remap table object
		ERROR_CHECK_OBJECT_(stitch->initialize_stitch_remap = vxCreateRemap(stitch->context, stitch->camera_rgb_buffer_width, stitch->camera_rgb_buffer_height, stitch->output_rgb_buffer_width, stitch->output_rgb_buffer_height));
		// build and verify graphInitializeStitch
		vx_node node;
		ERROR_CHECK_OBJECT_(node = stitchInitializeStitchRemapNode(stitch->graphInitializeStitch, stitch->num_camera_rows, stitch->num_camera_columns, stitch->camera_rgb_buffer_width, stitch->camera_rgb_buffer_height, stitch->output_rgb_buffer_width, stitch->rig_par_mat, stitch->cam_par_array, stitch->initialize_stitch_remap));
		ERROR_CHECK_STATUS_(vxReleaseNode(&node));
		ERROR_CHECK_STATUS_(vxVerifyGraph(stitch->graphInitializeStitch));
		// execute graphInitializeStitch to initialize remap table
		ERROR_CHECK_STATUS_(vxProcessGraph(stitch->graphInitializeStitch));

		////////////////////////////////////////////////////////////////////////
		// create and verify graphStitch using simple remap kernel
		////////////////////////////////////////////////////////////////////////
		ERROR_CHECK_OBJECT_(stitch->SimpleStitchRemapNode = vxRemapNode(stitch->graphStitch, rgb_input, stitch->initialize_stitch_remap, VX_INTERPOLATION_TYPE_BILINEAR, rgb_output));
		ERROR_CHECK_STATUS_(vxVerifyGraph(stitch->graphStitch));
	}
	/***********************************************************************************************************************************
	Normal Stitch mode -> Color Convert, Warp, Expo Comp & Merge
	************************************************************************************************************************************/
	else if (stitch->stitching_mode == stitching_mode_normal)
	{
		////////////////////////////////////////////////////////////////////////
		// get configuration from environment variables
		////////////////////////////////////////////////////////////////////////
		char textBuffer[256];
		stitch->EXPO_COMP = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_EXPCOMP];
		stitch->SEAM_FIND = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_SEAMFIND];
		stitch->SEAM_REFRESH = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_SEAM_REFRESH];
		stitch->SEAM_COST_SELECT = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_SEAM_COST_SELECT];
		stitch->MULTIBAND_BLEND = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND];
		stitch->num_bands = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND_NUMBANDS];
		if (stitch->num_bands < 2) {
			// general protection
			stitch->live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND] = 0.0f;
			stitch->live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND_NUMBANDS] = 0.0f;
			stitch->MULTIBAND_BLEND = 0;
			stitch->num_bands = 0;
		}

		// general protection: If numcam is less than 2, turn off Expo Comp, SeamFind & MultiBand Blend
		if (stitch->num_cameras <= 1){ stitch->EXPO_COMP = 0; stitch->SEAM_FIND = 0; stitch->MULTIBAND_BLEND = 0; };

		//Setting Initialize Stitch Config preference from global attributes
		InitializeStitchAttributes attr;
		attr.overlap_rectangle = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_CT_OVERLAP_RECT];
		attr.seam_find = (vx_float32)stitch->SEAM_FIND;
		attr.seam_vertical_priority = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_CT_SEAM_VERT_PRIORITY];
		attr.seam_horizontal_priority = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_CT_SEAM_HORT_PRIORITY];
		attr.seam_frequency = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_CT_SEAM_FREQUENCY];
		attr.seam_quality = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_CT_SEAM_QUALITY];
		attr.seam_stagger = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_CT_SEAM_STAGGER];
		attr.multi_band = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND];
		attr.num_bands = (vx_float32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_MULTIBAND_NUMBANDS];
		int CTAttr_size = (sizeof(InitializeStitchAttributes) / sizeof(vx_float32));
		ERROR_CHECK_OBJECT_(stitch->InitializeStitchConfig_matrix = vxCreateMatrix(stitch->context, VX_TYPE_FLOAT32, CTAttr_size, 1));
		ERROR_CHECK_STATUS_(vxWriteMatrix(stitch->InitializeStitchConfig_matrix, &attr));

		////////////////////////////////////////////////////////////////////////
		// create and process graphInitializeStitch using stitchInitializeStitchConfigNode
		////////////////////////////////////////////////////////////////////////
		// create data objects needed by warp kernel
		vx_uint32 width1 = (stitch->output_rgb_buffer_width + 127) >> 7;    // /128
		vx_uint32 height1 = (stitch->output_rgb_buffer_height + 31) >> 5;   // /32
		vx_enum StitchValidPixelEntryType, StitchWarpRemapEntryType;
		ERROR_CHECK_TYPE_(StitchValidPixelEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchValidPixelEntry)));
		ERROR_CHECK_TYPE_(StitchWarpRemapEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchWarpRemapEntry)));
		ERROR_CHECK_OBJECT_(stitch->ValidPixelEntry = vxCreateArray(stitch->context, StitchValidPixelEntryType, ((stitch->output_rgb_buffer_width * stitch->output_rgb_buffer_height * stitch->num_cameras) / 8)));
		ERROR_CHECK_OBJECT_(stitch->WarpRemapEntry = vxCreateArray(stitch->context, StitchWarpRemapEntryType, ((stitch->output_rgb_buffer_width * stitch->output_rgb_buffer_height * stitch->num_cameras) / 8)));
		ERROR_CHECK_OBJECT_(stitch->RGBY1 = vxCreateImage(stitch->context, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_RGBX));
		// create data objects needed by exposure comp kernel
		if (stitch->EXPO_COMP) {
			vx_enum StitchOverlapPixelEntryType, StitchExpCompCalcEntryType;
			ERROR_CHECK_TYPE_(StitchOverlapPixelEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchOverlapPixelEntry)));
			ERROR_CHECK_TYPE_(StitchExpCompCalcEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchExpCompCalcEntry)));
			ERROR_CHECK_OBJECT_(stitch->OverlapPixelEntry = vxCreateArray(stitch->context, StitchOverlapPixelEntryType, (width1 * height1 * (stitch->num_cameras * stitch->num_cameras / 2))));
			ERROR_CHECK_OBJECT_(stitch->overlap_matrix = vxCreateMatrix(stitch->context, VX_TYPE_INT32, stitch->num_cameras, stitch->num_cameras));
			ERROR_CHECK_OBJECT_(stitch->RGBY2 = vxCreateImage(stitch->context, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_RGBX));
			ERROR_CHECK_OBJECT_(stitch->valid_array = vxCreateArray(stitch->context, StitchExpCompCalcEntryType, (width1 * height1 * stitch->num_cameras)));
		}
		// create data objects needed by merge kernel
		ERROR_CHECK_OBJECT_(stitch->weight_image = vxCreateImage(stitch->context, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
		ERROR_CHECK_OBJECT_(stitch->cam_id_image = vxCreateImage(stitch->context, (stitch->output_rgb_buffer_width / 8), stitch->output_rgb_buffer_height, VX_DF_IMAGE_U8));
		ERROR_CHECK_OBJECT_(stitch->group1_image = vxCreateImage(stitch->context, (stitch->output_rgb_buffer_width / 8), stitch->output_rgb_buffer_height, VX_DF_IMAGE_U16));
		ERROR_CHECK_OBJECT_(stitch->group2_image = vxCreateImage(stitch->context, (stitch->output_rgb_buffer_width / 8), stitch->output_rgb_buffer_height, VX_DF_IMAGE_U16));
		// create data objects needed by seamfind kernel
		if (stitch->SEAM_FIND) {
			//SeamFind Images
			ERROR_CHECK_OBJECT_(stitch->mask_image = vxCreateImage(stitch->context, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
			ERROR_CHECK_OBJECT_(stitch->overlap_rect_array = vxCreateArray(stitch->context, VX_TYPE_RECTANGLE, (stitch->num_cameras * stitch->num_cameras)));
			ERROR_CHECK_OBJECT_(stitch->u8_image = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
			//SeamFind Array Types
			vx_enum StitchSeamFindValidEntryType, StitchSeamFindWeightEntryType;
			vx_enum StitchSeamFindAccumEntryType, StitchSeamFindPreferenceType;
			vx_enum StitchSeamFindInformationType, StitchSeamFindPathEntryType;
			ERROR_CHECK_TYPE_(StitchSeamFindValidEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchSeamFindValidEntry)));
			ERROR_CHECK_TYPE_(StitchSeamFindWeightEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchSeamFindWeightEntry)));
			ERROR_CHECK_TYPE_(StitchSeamFindAccumEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchSeamFindAccumEntry)));
			ERROR_CHECK_TYPE_(StitchSeamFindPreferenceType = vxRegisterUserStruct(stitch->context, sizeof(StitchSeamFindPreference)));
			ERROR_CHECK_TYPE_(StitchSeamFindInformationType = vxRegisterUserStruct(stitch->context, sizeof(StitchSeamFindInformation)));
			ERROR_CHECK_TYPE_(StitchSeamFindPathEntryType = vxRegisterUserStruct(stitch->context, sizeof(StitchSeamFindPathEntry)));

			//get seamfind variable sizes from the utility functiion
			SeamFindSizeInfo size_var;	vx_uint32 mode = 0;
			ERROR_CHECK_STATUS_(seamfind_utility(mode, stitch->output_rgb_buffer_width, stitch->num_cameras, &size_var));

			//SeamFind Arrays
			ERROR_CHECK_OBJECT_(stitch->seamfind_valid_array = vxCreateArray(stitch->context, StitchSeamFindValidEntryType, size_var.valid_entry));
			ERROR_CHECK_OBJECT_(stitch->seamfind_weight_array = vxCreateArray(stitch->context, StitchSeamFindWeightEntryType, size_var.weight_entry));
			ERROR_CHECK_OBJECT_(stitch->seamfind_accum_array = vxCreateArray(stitch->context, StitchSeamFindAccumEntryType, size_var.accum_entry));
			ERROR_CHECK_OBJECT_(stitch->seamfind_pref_array = vxCreateArray(stitch->context, StitchSeamFindPreferenceType, size_var.pref_entry));
			ERROR_CHECK_OBJECT_(stitch->seamfind_info_array = vxCreateArray(stitch->context, StitchSeamFindInformationType, size_var.info_entry));
			ERROR_CHECK_OBJECT_(stitch->seamfind_path_array = vxCreateArray(stitch->context, StitchSeamFindPathEntryType, size_var.path_entry));
		}
		else if (stitch->MULTIBAND_BLEND) {
			ERROR_CHECK_OBJECT_(stitch->mask_image = vxCreateImage(stitch->context, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
		}

		vx_enum StitchBlendValidType;
		ERROR_CHECK_TYPE_(StitchBlendValidType = vxRegisterUserStruct(stitch->context, sizeof(StitchBlendValidEntry)));
		if (stitch->MULTIBAND_BLEND && stitch->num_bands > 0) {
			ERROR_CHECK_ALLOC_(stitch->pStitchMultiband = new StitchMultibandData[stitch->num_bands]);
			memset(stitch->pStitchMultiband, 0, sizeof(StitchMultibandData)*stitch->num_bands);
			vx_uint32 * array_offset = new vx_uint32[stitch->num_bands];
			ERROR_CHECK_ALLOC_(array_offset);
			vx_uint32 totalCount = Compute_StitchBlendArraySize(stitch->output_rgb_buffer_width, stitch->output_rgb_buffer_height, stitch->num_cameras, stitch->num_bands, array_offset);
			for (int level = 0; level < stitch->num_bands; level++) {
				stitch->pStitchMultiband[level].valid_array_offset = array_offset[level];
			}
			delete[] array_offset;
			ERROR_CHECK_OBJECT_(stitch->blend_offsets = vxCreateArray(stitch->context, StitchBlendValidType, totalCount));
		}

		// build graph and process the graph to initialize the data objects
		vx_node node = stitchInitializeStitchConfigNode(stitch->graphInitializeStitch, 
			stitch->num_camera_rows, stitch->num_camera_columns, stitch->camera_rgb_buffer_width, stitch->camera_rgb_buffer_height, stitch->output_rgb_buffer_width, 
			stitch->rig_par_mat, stitch->cam_par_array, stitch->InitializeStitchConfig_matrix,
			stitch->ValidPixelEntry, stitch->WarpRemapEntry, stitch->OverlapPixelEntry,
			stitch->overlap_matrix, stitch->RGBY1, stitch->RGBY2, 
			stitch->weight_image, stitch->cam_id_image, stitch->group1_image, stitch->group2_image, stitch->valid_array,
			stitch->mask_image, stitch->overlap_rect_array, 
			stitch->seamfind_valid_array, stitch->seamfind_accum_array, stitch->seamfind_weight_array, stitch->seamfind_pref_array, stitch->seamfind_info_array,
			stitch->blend_offsets);
		ERROR_CHECK_OBJECT_(node);
		ERROR_CHECK_STATUS_(vxReleaseNode(&node));
		ERROR_CHECK_STATUS_(vxVerifyGraph(stitch->graphInitializeStitch));
		ERROR_CHECK_STATUS_(vxProcessGraph(stitch->graphInitializeStitch));
		
		////////////////////////////////////////////////////////////////////////
		// create and verify graphStitch using low-level kernels
		////////////////////////////////////////////////////////////////////////
		// warping
		if (!stitch->SEAM_FIND) {
			ERROR_CHECK_OBJECT_(stitch->WarpNode = stitchWarpNode(stitch->graphStitch, 1, stitch->num_cameras, stitch->ValidPixelEntry, stitch->WarpRemapEntry, rgb_input, stitch->RGBY1, stitch->num_camera_columns));
		}
		else {
			ERROR_CHECK_OBJECT_(stitch->WarpNode = stitchWarpU8Node(stitch->graphStitch, 1, stitch->num_cameras, stitch->ValidPixelEntry, stitch->WarpRemapEntry, rgb_input, stitch->RGBY1, stitch->u8_image, stitch->num_camera_columns));
		}

		// exposure comp
		vx_image merge_input = stitch->RGBY1;
		if (stitch->EXPO_COMP) {
			// data objects specific to exposure comp
			ERROR_CHECK_OBJECT_(stitch->A_matrix = vxCreateMatrix(stitch->context, VX_TYPE_INT32, stitch->num_cameras, stitch->num_cameras));
			ERROR_CHECK_ALLOC_(stitch->A_matrix_initial_value = new vx_int32[stitch->num_cameras * stitch->num_cameras]);
			memset(stitch->A_matrix_initial_value, 0, stitch->num_cameras * stitch->num_cameras * sizeof(vx_int32));
			ERROR_CHECK_STATUS_(vxWriteMatrix(stitch->A_matrix, stitch->A_matrix_initial_value));
			stitch->alpha = 0.01f;
			stitch->beta = 100.0f;
			ERROR_CHECK_OBJECT_(stitch->gain_array = vxCreateArray(stitch->context, VX_TYPE_FLOAT32, stitch->num_cameras));
			// graph
			if (stitch->MULTIBAND_BLEND) {
				ERROR_CHECK_OBJECT_(stitch->ExpcompComputeGainNode = stitchExposureCompCalcErrorFnNode(stitch->graphStitch, stitch->num_cameras, stitch->RGBY1, stitch->OverlapPixelEntry, stitch->mask_image, stitch->A_matrix));
			}
			else {
				ERROR_CHECK_OBJECT_(stitch->ExpcompComputeGainNode = stitchExposureCompCalcErrorFnNode(stitch->graphStitch, stitch->num_cameras, stitch->RGBY1, stitch->OverlapPixelEntry, NULL, stitch->A_matrix));
			}
			ERROR_CHECK_OBJECT_(stitch->ExpcompSolveGainNode = stitchExposureCompSolveForGainNode(stitch->graphStitch, stitch->alpha, stitch->beta, stitch->A_matrix, stitch->overlap_matrix, stitch->gain_array));
			ERROR_CHECK_OBJECT_(stitch->ExpcompApplyGainNode = stitchExposureCompApplyGainNode(stitch->graphStitch, stitch->RGBY1, stitch->gain_array, stitch->valid_array, stitch->RGBY2));
			// update merge input
			merge_input = stitch->RGBY2;
		}
		if (stitch->SEAM_FIND) {
			//SeamFind Images
			if (!stitch->SEAM_COST_SELECT){
				ERROR_CHECK_OBJECT_(stitch->sobelx_image = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_S16));
				ERROR_CHECK_OBJECT_(stitch->sobely_image = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_S16));
				ERROR_CHECK_OBJECT_(stitch->s16_image = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_S16));
			}
			ERROR_CHECK_OBJECT_(stitch->sobel_magnitude_image = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
			ERROR_CHECK_OBJECT_(stitch->sobel_phase_image = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
			ERROR_CHECK_OBJECT_(stitch->new_weight_image = vxCreateImage(stitch->context, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
			//Smart Cut Weight Image
			void *weight_image_ptr = nullptr; vx_rectangle_t weight_rect; vx_imagepatch_addressing_t weight_addr;
			weight_rect.start_x = weight_rect.start_y = 0; weight_rect.end_x = stitch->output_rgb_buffer_width; weight_rect.end_y = (stitch->output_rgb_buffer_height * stitch->num_cameras);
			//SeamFind Weight image
			void *new_weight_image_ptr = nullptr; vx_rectangle_t output_weight_rect; vx_imagepatch_addressing_t output_weight_addr;
			output_weight_rect.start_x = output_weight_rect.start_y = 0; output_weight_rect.end_x = stitch->output_rgb_buffer_width; output_weight_rect.end_y = (stitch->output_rgb_buffer_height * stitch->num_cameras);
			ERROR_CHECK_STATUS_(vxAccessImagePatch(stitch->weight_image, &weight_rect, 0, &weight_addr, &weight_image_ptr, VX_READ_ONLY));
			ERROR_CHECK_STATUS_(vxAccessImagePatch(stitch->new_weight_image, &output_weight_rect, 0, &output_weight_addr, &new_weight_image_ptr, VX_READ_AND_WRITE));
			vx_uint8 *weight_ptr = (vx_uint8*)weight_image_ptr;
			vx_uint8 *output_weight_ptr = (vx_uint8*)new_weight_image_ptr;
			//Copy Smart Cut weight into SeamFind weight image
			void *ptr1 = nullptr;	void *ptr2 = nullptr;
			size_t len = output_weight_addr.stride_x * (output_weight_addr.dim_x * output_weight_addr.scale_x) / VX_SCALE_UNITY;
			for (vx_uint32 y = 0; y < (stitch->output_rgb_buffer_height * stitch->num_cameras); y += output_weight_addr.step_y)
			{
				ptr1 = vxFormatImagePatchAddress2d(weight_image_ptr, 0, y - weight_rect.start_y, &weight_addr);
				ptr2 = vxFormatImagePatchAddress2d(new_weight_image_ptr, 0, y - output_weight_rect.start_y, &output_weight_addr);
				memcpy(ptr2, ptr1, len);
			}
			ERROR_CHECK_STATUS_(vxCommitImagePatch(stitch->weight_image, &weight_rect, 0, &weight_addr, weight_image_ptr));
			ERROR_CHECK_STATUS_(vxCommitImagePatch(stitch->new_weight_image, &output_weight_rect, 0, &output_weight_addr, new_weight_image_ptr));

			//SeamFind Scalars
			vx_int32 input_shift_value = 0;
			stitch->current_frame_value = 0;
			ERROR_CHECK_OBJECT_(stitch->input_shift = vxCreateScalar(stitch->context, VX_TYPE_INT32, &input_shift_value));
			ERROR_CHECK_OBJECT_(stitch->current_frame = vxCreateScalar(stitch->context, VX_TYPE_UINT32, &stitch->current_frame_value));
			//SeamFind Seam Refresh
			if (stitch->SEAM_REFRESH)
			{
				//Seam Refresh Scalars
				stitch->scene_threshold_value = (vx_uint32)stitch->live_stitch_attr[LIVE_STITCH_ATTR_SEAM_THRESHOLD];
				ERROR_CHECK_OBJECT_(stitch->scene_threshold = vxCreateScalar(stitch->context, VX_TYPE_UINT32, &stitch->scene_threshold_value));
				//Seam Refresh Array
				vx_enum StitchSeamSceneType;
				ERROR_CHECK_TYPE_(StitchSeamSceneType = vxRegisterUserStruct(stitch->context, sizeof(StitchSeamFindSceneEntry)));
				ERROR_CHECK_OBJECT_(stitch->seamfind_scene_array = vxCreateArray(stitch->context, StitchSeamSceneType, ((stitch->num_cameras * stitch->num_cameras) / 2)));
				std::vector<StitchSeamFindSceneEntry> seam_scene;
				seam_scene.resize((stitch->num_cameras * stitch->num_cameras) / 2);
				memset(&seam_scene[0], 0, ((stitch->num_cameras * stitch->num_cameras) / 2) * (sizeof(StitchSeamFindSceneEntry) / sizeof(vx_uint8)));
				StitchSeamFindSceneEntry *ARRAY_SeamFind_ptr = &seam_scene[0];
				ERROR_CHECK_STATUS_(vxTruncateArray(stitch->seamfind_scene_array, 0));
				ERROR_CHECK_STATUS_(vxAddArrayItems(stitch->seamfind_scene_array, ((stitch->num_cameras * stitch->num_cameras) / 2), ARRAY_SeamFind_ptr, sizeof(StitchSeamFindSceneEntry)));
				//SeamFind Step 1: Seam Refresh 
				stitch->SeamfindStep1Node = stitchSeamFindSceneDetectNode(stitch->graphStitch, stitch->current_frame, stitch->scene_threshold,
					stitch->u8_image, stitch->seamfind_info_array, stitch->seamfind_pref_array, stitch->seamfind_scene_array);
				ERROR_CHECK_OBJECT_(stitch->SeamfindStep1Node);
			}
			//SeamFind Step 2 - Cost Generation: 0:OpenVX Sobel 1:Optimized Sobel
			if (!stitch->SEAM_COST_SELECT){
				ERROR_CHECK_OBJECT_(stitch->SobelNode = vxSobel3x3Node(stitch->graphStitch, stitch->u8_image, stitch->sobelx_image, stitch->sobely_image));
				ERROR_CHECK_OBJECT_(stitch->MagnitudeNode = vxMagnitudeNode(stitch->graphStitch, stitch->sobelx_image, stitch->sobely_image, stitch->s16_image));
				ERROR_CHECK_OBJECT_(stitch->PhaseNode = vxPhaseNode(stitch->graphStitch, stitch->sobelx_image, stitch->sobely_image, stitch->sobel_phase_image));
				ERROR_CHECK_OBJECT_(stitch->ConvertDepthNode = vxConvertDepthNode(stitch->graphStitch, stitch->s16_image, stitch->sobel_magnitude_image, VX_CONVERT_POLICY_SATURATE, stitch->input_shift));
			}
			else{
				vx_uint32 exe_flag = 1;
				ERROR_CHECK_OBJECT_(stitch->flag = vxCreateScalar(stitch->context, VX_TYPE_UINT32, &exe_flag));
				ERROR_CHECK_OBJECT_(stitch->SeamfindStep2Node = stitchSeamFindCostGenerateNode(stitch->graphStitch, stitch->flag, stitch->u8_image, stitch->sobel_magnitude_image, stitch->sobel_phase_image));
			}
			//SeamFind Step 3 - Cost Accumulate
			stitch->SeamfindStep3Node = stitchSeamFindCostAccumulateNode(stitch->graphStitch, stitch->current_frame, stitch->output_rgb_buffer_width, stitch->output_rgb_buffer_height,
				stitch->sobel_magnitude_image, stitch->sobel_phase_image, stitch->mask_image, stitch->seamfind_valid_array, stitch->seamfind_pref_array,
				stitch->seamfind_info_array, stitch->seamfind_accum_array);
			ERROR_CHECK_OBJECT_(stitch->SeamfindStep3Node);
			//SeamFind Step 4 - Path Trace
			stitch->SeamfindStep4Node = stitchSeamFindPathTraceNode(stitch->graphStitch, stitch->current_frame, stitch->weight_image, stitch->seamfind_info_array, 
				stitch->seamfind_accum_array, stitch->seamfind_pref_array, stitch->seamfind_path_array);
			ERROR_CHECK_OBJECT_(stitch->SeamfindStep4Node);
			//SeamFind Step 5 - Set Weights
			stitch->SeamfindStep5Node = stitchSeamFindSetWeightsNode(stitch->graphStitch, stitch->current_frame, stitch->num_cameras, stitch->output_rgb_buffer_width, stitch->output_rgb_buffer_height,
				stitch->seamfind_weight_array, stitch->seamfind_path_array, stitch->seamfind_pref_array, stitch->new_weight_image);
			ERROR_CHECK_OBJECT_(stitch->SeamfindStep5Node);
		}
		// create data objects and nodes for multiband blending
		if (stitch->MULTIBAND_BLEND){
			stitch->pStitchMultiband[0].WeightPyrImgGaussian = stitch->SEAM_FIND ? stitch->new_weight_image : stitch->weight_image;	// for level0: weight image is mask image after seem find
			stitch->pStitchMultiband[0].DstPyrImgGaussian = stitch->EXPO_COMP ? stitch->RGBY2 : stitch->RGBY1;			// for level0: dst image is image after exposure_comp
			ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[0].DstPyrImgLaplacian = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_RGB4_AMD));
			ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[0].DstPyrImgLaplacianRec = vxCreateVirtualImage(stitch->graphStitch, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_RGBX));
			//rgb_output;
			// create gaussian weight pyramid images and Laplacian pyramids.
			for (int i = 1; i < stitch->num_bands; i++){
				vx_uint32 width_l = (stitch->output_rgb_buffer_width >> i);
				vx_uint32 height_l = (stitch->output_rgb_buffer_height >> i)*stitch->num_cameras;
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].WeightPyrImgGaussian = vxCreateVirtualImage(stitch->graphStitch, width_l, height_l, VX_DF_IMAGE_U8));
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].DstPyrImgGaussian = vxCreateVirtualImage(stitch->graphStitch, width_l, height_l, VX_DF_IMAGE_RGBX));
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].DstPyrImgLaplacian = vxCreateVirtualImage(stitch->graphStitch, width_l, height_l, VX_DF_IMAGE_RGB4_AMD));
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].DstPyrImgLaplacianRec = vxCreateVirtualImage(stitch->graphStitch, width_l, height_l, VX_DF_IMAGE_RGB4_AMD));
				stitch->pStitchMultiband[i].WeightHSGNode = stitchMultiBandHalfScaleGaussianNode(stitch->graphStitch, stitch->num_cameras, stitch->pStitchMultiband[i].valid_array_offset,
					stitch->blend_offsets, stitch->pStitchMultiband[i - 1].WeightPyrImgGaussian, stitch->pStitchMultiband[i].WeightPyrImgGaussian);
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].WeightHSGNode);
				stitch->pStitchMultiband[i].SourceHSGNode = stitchMultiBandHalfScaleGaussianNode(stitch->graphStitch, stitch->num_cameras, stitch->pStitchMultiband[i].valid_array_offset,
					stitch->blend_offsets, stitch->pStitchMultiband[i - 1].DstPyrImgGaussian, stitch->pStitchMultiband[i].DstPyrImgGaussian);
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].SourceHSGNode);
				stitch->pStitchMultiband[i - 1].UpscaleSubtractNode = stitchMultiBandUpscaleGaussianSubtractNode(stitch->graphStitch, stitch->num_cameras, stitch->pStitchMultiband[i - 1].valid_array_offset,
					stitch->pStitchMultiband[i - 1].DstPyrImgGaussian, stitch->pStitchMultiband[i].DstPyrImgGaussian, stitch->blend_offsets, stitch->pStitchMultiband[i-1].WeightPyrImgGaussian, stitch->pStitchMultiband[i - 1].DstPyrImgLaplacian);
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i - 1].UpscaleSubtractNode);
			}
			// reconstruct Laplacian after blending with corresponding weights: for band = num_bands-1, laplacian and gaussian is the same
			int i = stitch->num_bands - 1;
			stitch->pStitchMultiband[i].BlendNode = stitchMultiBandMergeNode(stitch->graphStitch, stitch->num_cameras, stitch->pStitchMultiband[i].valid_array_offset,
				stitch->pStitchMultiband[i].DstPyrImgGaussian, stitch->pStitchMultiband[i].WeightPyrImgGaussian, stitch->blend_offsets, stitch->pStitchMultiband[i].DstPyrImgLaplacianRec);
			ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].BlendNode);
			--i;
			for (; i > 0; --i){
				stitch->pStitchMultiband[i].UpscaleAddNode = stitchMultiBandUpscaleGaussianAddNode(stitch->graphStitch, stitch->num_cameras, stitch->pStitchMultiband[i].valid_array_offset,
					stitch->pStitchMultiband[i].DstPyrImgLaplacian, stitch->pStitchMultiband[i + 1].DstPyrImgLaplacianRec, stitch->blend_offsets, stitch->pStitchMultiband[i].DstPyrImgLaplacianRec);
				ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[i].UpscaleAddNode);
			}
			// for the lowest level
			stitch->pStitchMultiband[0].UpscaleAddNode = stitchMultiBandLaplacianReconstructNode(stitch->graphStitch, stitch->num_cameras, stitch->pStitchMultiband[0].valid_array_offset,
				stitch->pStitchMultiband[0].DstPyrImgLaplacian, stitch->pStitchMultiband[1].DstPyrImgLaplacianRec, stitch->blend_offsets, stitch->pStitchMultiband[0].DstPyrImgLaplacianRec);
			ERROR_CHECK_OBJECT_(stitch->pStitchMultiband[0].UpscaleAddNode);
			// update merge input
			merge_input = stitch->pStitchMultiband[0].DstPyrImgLaplacianRec;
		}
		// merge data objects and node
		ERROR_CHECK_OBJECT_(stitch->band_weights_array = vxCreateArray(stitch->context, VX_TYPE_FLOAT32, 1));
		vx_float32 *band_weights_ptr = nullptr;
		vx_float32 bandweight = 1.0f;
		band_weights_ptr = &bandweight;
		ERROR_CHECK_STATUS_(vxTruncateArray(stitch->band_weights_array, 0));
		ERROR_CHECK_STATUS_(vxAddArrayItems(stitch->band_weights_array, 1, band_weights_ptr, sizeof(vx_float32)));
		if (stitch->MULTIBAND_BLEND)
		{		
			ERROR_CHECK_OBJECT_(stitch->blend_mask_image = vxCreateImage(stitch->context, stitch->output_rgb_buffer_width, (stitch->output_rgb_buffer_height * stitch->num_cameras), VX_DF_IMAGE_U8));
			void *blend_mask_image_ptr = nullptr; vx_rectangle_t blend_mask_rect; vx_imagepatch_addressing_t blend_mask_addr;
			blend_mask_rect.start_x = blend_mask_rect.start_y = 0; blend_mask_rect.end_x = stitch->output_rgb_buffer_width; blend_mask_rect.end_y = (stitch->output_rgb_buffer_height * stitch->num_cameras);
			ERROR_CHECK_STATUS_(vxAccessImagePatch(stitch->blend_mask_image, &blend_mask_rect, 0, &blend_mask_addr, &blend_mask_image_ptr, VX_READ_AND_WRITE));
			memset(blend_mask_image_ptr, -1, (stitch->output_rgb_buffer_width * stitch->output_rgb_buffer_height * stitch->num_cameras));
			ERROR_CHECK_STATUS_(vxCommitImagePatch(stitch->blend_mask_image, &blend_mask_rect, 0, &blend_mask_addr, blend_mask_image_ptr));			
			ERROR_CHECK_OBJECT_(stitch->MergeNode = stitchMergeNode(stitch->graphStitch, 1, stitch->band_weights_array, stitch->cam_id_image, stitch->group1_image, stitch->group2_image, merge_input, stitch->blend_mask_image, rgb_output));
		}
		else
		{
			if (!stitch->SEAM_FIND) {
				ERROR_CHECK_OBJECT_(stitch->MergeNode = stitchMergeNode(stitch->graphStitch, 1, stitch->band_weights_array, stitch->cam_id_image, stitch->group1_image, stitch->group2_image, merge_input, stitch->weight_image, rgb_output));
			}
			else {
				ERROR_CHECK_OBJECT_(stitch->MergeNode = stitchMergeNode(stitch->graphStitch, 1, stitch->band_weights_array, stitch->cam_id_image, stitch->group1_image, stitch->group2_image, merge_input, stitch->new_weight_image, rgb_output));
			}
		}
		ERROR_CHECK_OBJECT_(stitch->MergeNode);
		// verify the graph
		ERROR_CHECK_STATUS_(vxVerifyGraph(stitch->graphStitch));
		
		stitch->SEAM_FIND_TARGET = 0;
		if (StitchGetEnvironmentVariable("SEAM_FIND_TARGET", textBuffer, sizeof(textBuffer))) { stitch->SEAM_FIND_TARGET = atoi(textBuffer); }
		// copy RGBY1 data from CPU to GPU because graphStitch expects the data initialized on GPU
		ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->RGBY1, VX_DIRECTIVE_AMD_COPY_TO_OPENCL));
		if (stitch->SEAM_FIND)
		{
			ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->new_weight_image, VX_DIRECTIVE_AMD_COPY_TO_OPENCL));
			ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->seamfind_accum_array, VX_DIRECTIVE_AMD_COPY_TO_OPENCL));	
			if (stitch->SEAM_REFRESH)
			{
				ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->seamfind_pref_array, VX_DIRECTIVE_AMD_COPY_TO_OPENCL));
				if (!stitch->SEAM_FIND_TARGET)
				ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->seamfind_scene_array, VX_DIRECTIVE_AMD_COPY_TO_OPENCL));
			}
		}	
	}
	/***********************************************************************************************************************************
	Other Modes
	************************************************************************************************************************************/
	else {
		vxAddLogEntry((vx_reference)stitch->context, VX_ERROR_NO_RESOURCES, "Other Stitching Modes are under development\nMode-1 = Quick Stitch Mode Available\nMode-2 = Normal Stitch Mode Available\n");
		return VX_ERROR_NO_RESOURCES;
	}

	// mark that initialization is successful
	stitch->initialized = true;

	// debug: dump auxiliary data
	if (stitch->loomioCameraAuxData || stitch->loomioOverlayAuxData || stitch->loomioOutputAuxData || stitch->loomioViewingAuxData) {
		char fileName[1024] = { 0 };
		if (StitchGetEnvironmentVariable("LOOMIO_AUX_DUMP", fileName, sizeof(fileName))) {
			stitch->loomioAuxDumpFile = fopen(fileName, "wb");
			if (!stitch->loomioAuxDumpFile) { printf("ERROR: unable to create: %s\n", fileName); return VX_FAILURE; }
			printf("OK: dumping auxiliary data into %s\n", fileName);
		}
	}

	return VX_SUCCESS;
}

//! \brief initialize the stitch context.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsReinitialize(ls_context stitch)
{
	ERROR_CHECK_STATUS_(IsValidContextAndInitialized(stitch));
	if (stitch->scheduled) {
		ls_printf("ERROR: lsReinitialize: can't reinitialize when already scheduled\n");
		return VX_ERROR_GRAPH_SCHEDULED;
	}
	if (stitch->live_stitch_attr[LIVE_STITCH_ATTR_ENABLE_REINITIALIZE] == 0.0f) {
		ls_printf("ERROR: lsReinitialize has been disabled\n");
		return VX_ERROR_NOT_SUPPORTED;
	}

	if (stitch->rig_params_updated || stitch->camera_params_updated) {
		// execute graphInitializeStitch to re-initialize tables
		ERROR_CHECK_STATUS_(vxProcessGraph(stitch->graphInitializeStitch));
		if (stitch->RGBY1) {
			// copy RGBY1 data from CPU to GPU because graphStitch expects the data initialized on GPU
			ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->RGBY1, VX_DIRECTIVE_AMD_COPY_TO_OPENCL));
		}
	}
	if (stitch->overlay_params_updated) {
		// execute graphOverlay to re-initialize tables
		ERROR_CHECK_STATUS_(vxProcessGraph(stitch->graphOverlay));
	}

	// clear flags
	stitch->reinitialize_required = false;
	stitch->rig_params_updated = false;
	stitch->camera_params_updated = false;
	stitch->overlay_params_updated = false;

	return VX_SUCCESS;
}

//! \brief Release stitch context. The ls_context will be reset to NULL.
SHARED_PUBLIC vx_status VX_API_CALL lsReleaseContext(ls_context * pStitch)
{
	if (!pStitch) {
		return VX_ERROR_INVALID_REFERENCE;
	}
	else {
		ls_context stitch = *pStitch;
		ERROR_CHECK_STATUS_(IsValidContext(stitch));
		// graph profile dump if requested
		if (stitch->live_stitch_attr[LIVE_STITCH_ATTR_PROFILER]) {
			const char * name[3] = { "graphInitializeStitch", "graphOverlay", "graphStitch" };
			vx_graph graph[3] = { stitch->graphInitializeStitch, stitch->graphOverlay, stitch->graphStitch };
			for (int i = 0; i < 3; i++) {
				if (graph[i]) {
					ls_printf("> graph profile: %s\n", name[i]); char fileName[] = "stdout";
					ERROR_CHECK_STATUS_(vxQueryGraph(graph[i], VX_GRAPH_ATTRIBUTE_AMD_PERFORMANCE_INTERNAL_PROFILE, fileName, 0));
				}
			}
		}
		// configuration
		if (stitch->camera_par) delete[] stitch->camera_par;
		if (stitch->overlay_par) delete[] stitch->overlay_par;
		//Matrix
		if (stitch->rig_par_mat) ERROR_CHECK_STATUS_(vxReleaseMatrix(&stitch->rig_par_mat));
		if (stitch->cam_par_mat) ERROR_CHECK_STATUS_(vxReleaseMatrix(&stitch->cam_par_mat));
		//Array
		if (stitch->cam_par_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->cam_par_array));
		if (stitch->ovr_par_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->ovr_par_array));
		//Stitch Mode 1 Release
		//Image
		if (stitch->Img_input) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->Img_input));
		if (stitch->Img_output) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->Img_output));
		if (stitch->Img_input_rgb) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->Img_input_rgb));
		if (stitch->Img_output_rgb) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->Img_output_rgb));
		if (stitch->Img_overlay) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->Img_overlay));
		if (stitch->Img_overlay_rgb) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->Img_overlay_rgb));
		if (stitch->Img_overlay_rgba) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->Img_overlay_rgba));
		//Remap
		if (stitch->overlay_remap) ERROR_CHECK_STATUS_(vxReleaseRemap(&stitch->overlay_remap));
		if (stitch->initialize_stitch_remap) ERROR_CHECK_STATUS_(vxReleaseRemap(&stitch->initialize_stitch_remap));
		//Node
		if (stitch->InputColorConvertNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->InputColorConvertNode));
		if (stitch->SimpleStitchRemapNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->SimpleStitchRemapNode));
		if (stitch->OutputColorConvertNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->OutputColorConvertNode));

		//Stitch Mode 2 Release
		//Image
		if (stitch->RGBY1) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->RGBY1));
		if (stitch->RGBY2) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->RGBY2));
		if (stitch->weight_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->weight_image));
		if (stitch->cam_id_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->cam_id_image));
		if (stitch->group1_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->group1_image));
		if (stitch->group2_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->group2_image));
		//Matrix
		if (stitch->overlap_matrix) ERROR_CHECK_STATUS_(vxReleaseMatrix(&stitch->overlap_matrix));
		if (stitch->A_matrix) ERROR_CHECK_STATUS_(vxReleaseMatrix(&stitch->A_matrix));
		if (stitch->A_matrix_initial_value) delete[] stitch->A_matrix_initial_value;
		//Array
		if (stitch->ValidPixelEntry) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->ValidPixelEntry));
		if (stitch->WarpRemapEntry) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->WarpRemapEntry));
		if (stitch->OverlapPixelEntry) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->OverlapPixelEntry));
		if (stitch->valid_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->valid_array));
		if (stitch->gain_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->gain_array));
		//Node
		if (stitch->WarpNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->WarpNode));
		if (stitch->ExpcompComputeGainNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->ExpcompComputeGainNode));
		if (stitch->ExpcompSolveGainNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->ExpcompSolveGainNode));
		if (stitch->ExpcompApplyGainNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->ExpcompApplyGainNode));
		if (stitch->MergeNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->MergeNode));

		//Stitch SeamFind
		//Image
		if (stitch->mask_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->mask_image));
		if (stitch->u8_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->u8_image));
		if (stitch->s16_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->s16_image));
		if (stitch->sobelx_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->sobelx_image));
		if (stitch->sobely_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->sobely_image));
		if (stitch->sobel_magnitude_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->sobel_magnitude_image));
		if (stitch->sobel_phase_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->sobel_phase_image));
		if (stitch->new_weight_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->new_weight_image));
		//Array
		if (stitch->overlap_rect_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->overlap_rect_array));
		if (stitch->seamfind_valid_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->seamfind_valid_array));
		if (stitch->seamfind_weight_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->seamfind_weight_array));
		if (stitch->seamfind_accum_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->seamfind_accum_array));
		if (stitch->seamfind_pref_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->seamfind_pref_array));	
		if (stitch->seamfind_path_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->seamfind_path_array));
		if (stitch->seamfind_scene_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->seamfind_scene_array));
		//Node
		if (stitch->SobelNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->SobelNode));
		if (stitch->MagnitudeNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->MagnitudeNode));
		if (stitch->PhaseNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->PhaseNode));
		if (stitch->ConvertDepthNode) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->ConvertDepthNode));
		if (stitch->SeamfindStep1Node) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->SeamfindStep1Node));
		if (stitch->SeamfindStep2Node) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->SeamfindStep2Node));
		if (stitch->SeamfindStep3Node) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->SeamfindStep3Node));
		if (stitch->SeamfindStep4Node) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->SeamfindStep4Node));
		if (stitch->SeamfindStep5Node) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->SeamfindStep5Node));
		//Scalar
		if (stitch->input_shift) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->input_shift));
		if (stitch->current_frame) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->current_frame));
		if (stitch->scene_threshold) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->scene_threshold));
		if (stitch->flag) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->flag));

		//Stitch MultiBand
		//Image
		if (stitch->blend_mask_image) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->blend_mask_image));
		//Array
		if (stitch->band_weights_array) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->band_weights_array));
		if (stitch->blend_offsets) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->blend_offsets));
		//Node
		if (stitch->MULTIBAND_BLEND && stitch->pStitchMultiband){
			for (int i = 0; i < stitch->num_bands; i++){
				if (stitch->pStitchMultiband[i].BlendNode)ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->pStitchMultiband[i].BlendNode));
				if (stitch->pStitchMultiband[i].SourceHSGNode)ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->pStitchMultiband[i].SourceHSGNode));
				if (stitch->pStitchMultiband[i].WeightHSGNode)ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->pStitchMultiband[i].WeightHSGNode));
				if (stitch->pStitchMultiband[i].UpscaleSubtractNode)ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->pStitchMultiband[i].UpscaleSubtractNode));
				if (stitch->pStitchMultiband[i].UpscaleAddNode)ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->pStitchMultiband[i].UpscaleAddNode));
				if (stitch->pStitchMultiband[i].LaplacianReconNode)ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->pStitchMultiband[i].LaplacianReconNode));
				if (stitch->pStitchMultiband[i].DstPyrImgGaussian) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->pStitchMultiband[i].DstPyrImgGaussian));
				if (stitch->pStitchMultiband[i].WeightPyrImgGaussian) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->pStitchMultiband[i].WeightPyrImgGaussian));
				if (stitch->pStitchMultiband[i].DstPyrImgLaplacian) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->pStitchMultiband[i].DstPyrImgLaplacian));
				if (stitch->pStitchMultiband[i].DstPyrImgLaplacianRec) ERROR_CHECK_STATUS_(vxReleaseImage(&stitch->pStitchMultiband[i].DstPyrImgLaplacianRec));
			}
			delete stitch->pStitchMultiband;
		}
		// LoomIO
		if (stitch->cameraMediaConfig) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->cameraMediaConfig));
		if (stitch->overlayMediaConfig) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->overlayMediaConfig));
		if (stitch->outputMediaConfig) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->outputMediaConfig));
		if (stitch->viewingMediaConfig) ERROR_CHECK_STATUS_(vxReleaseScalar(&stitch->viewingMediaConfig));
		if (stitch->loomioCameraAuxData) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->loomioCameraAuxData));
		if (stitch->loomioOverlayAuxData) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->loomioOverlayAuxData));
		if (stitch->loomioOutputAuxData) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->loomioOutputAuxData));
		if (stitch->loomioViewingAuxData) ERROR_CHECK_STATUS_(vxReleaseArray(&stitch->loomioViewingAuxData));
		if (stitch->nodeLoomIoCamera) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->nodeLoomIoCamera));
		if (stitch->nodeLoomIoOverlay) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->nodeLoomIoOverlay));
		if (stitch->nodeLoomIoOutput) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->nodeLoomIoOutput));
		if (stitch->nodeLoomIoViewing) ERROR_CHECK_STATUS_(vxReleaseNode(&stitch->nodeLoomIoViewing));

		//Graph & Context
		if (stitch->graphStitch) ERROR_CHECK_STATUS_(vxReleaseGraph(&stitch->graphStitch));
		if (stitch->graphInitializeStitch) ERROR_CHECK_STATUS_(vxReleaseGraph(&stitch->graphInitializeStitch));
		if (stitch->graphOverlay) ERROR_CHECK_STATUS_(vxReleaseGraph(&stitch->graphOverlay));
		if (stitch->context) ERROR_CHECK_STATUS_(vxReleaseContext(&stitch->context));

		// TBD need complete cleanup to be reviewed

		// debug aux dumps
		if (stitch->loomioAuxDumpFile) {
			fclose(stitch->loomioAuxDumpFile);
		}

		// clear the magic and destroy
		stitch->magic = ~LIVE_STITCH_MAGIC;
		delete stitch;
		*pStitch = nullptr;
	}
	return VX_SUCCESS;
}

//! \brief Set OpenCL buffers
//     input_buffer   - input opencl buffer with images from all cameras
//     overlay_buffer - overlay opencl buffer with all images
//     output_buffer  - output opencl buffer for output equirectangular image
//   Use of nullptr will return the control of previously set buffer
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraBuffer(ls_context stitch, cl_mem * input_buffer)
{
	ERROR_CHECK_STATUS_(IsValidContextAndInitialized(stitch));
	// check to make sure that LoomIO for camera is not active
	if (stitch->nodeLoomIoCamera) return VX_ERROR_NOT_ALLOCATED;

	// switch the user specified OpenCL buffer into image
	void * ptr_in[] = { input_buffer ? input_buffer[0] : nullptr };
	ERROR_CHECK_STATUS_(vxSwapImageHandle(stitch->Img_input, ptr_in, nullptr, 1));

	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputBuffer(ls_context stitch, cl_mem * output_buffer)
{
	ERROR_CHECK_STATUS_(IsValidContextAndInitialized(stitch));
	// check to make sure that LoomIO for output is not active
	if (stitch->nodeLoomIoOutput) return VX_ERROR_NOT_ALLOCATED;

	// switch the user specified OpenCL buffer into image
	void * ptr_out[] = { output_buffer ? output_buffer [0] : nullptr };
	ERROR_CHECK_STATUS_(vxSwapImageHandle(stitch->Img_output, ptr_out, nullptr, 1));

	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayBuffer(ls_context stitch, cl_mem * overlay_buffer)
{
	ERROR_CHECK_STATUS_(IsValidContextAndInitialized(stitch));
	// check to make sure that LoomIO for overlay is not active
	if (stitch->nodeLoomIoOverlay) return VX_ERROR_NOT_ALLOCATED;

	// switch the user specified OpenCL buffer into image
	void * ptr_overlay[] = { overlay_buffer ? overlay_buffer[0] : nullptr };
	ERROR_CHECK_STATUS_(vxSwapImageHandle(stitch->Img_overlay, ptr_overlay, nullptr, 1));

	return VX_SUCCESS;
}

//! \brief Schedule next frame
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsScheduleFrame(ls_context stitch)
{
	ERROR_CHECK_STATUS_(IsValidContextAndInitialized(stitch));
	if (stitch->scheduled) {
		ls_printf("ERROR: lsScheduleFrame: already scheduled\n");
		return VX_ERROR_GRAPH_SCHEDULED;
	}
	if (stitch->reinitialize_required) {
		ls_printf("ERROR: lsScheduleFrame: reinitialize required\n");
		return VX_FAILURE;
	}

	// seamfind needs frame counter values to be incremented
	if (stitch->SEAM_FIND) {
		ERROR_CHECK_STATUS_(vxWriteScalarValue(stitch->current_frame, &stitch->current_frame_value));
		stitch->current_frame_value++;
	}

	// exposure comp expects A_matrix to be initialized to ZERO on GPU
	if (stitch->EXPO_COMP && stitch->A_matrix) {
		ERROR_CHECK_STATUS_(vxWriteMatrix(stitch->A_matrix, stitch->A_matrix_initial_value));
		ERROR_CHECK_STATUS_(vxDirective((vx_reference)stitch->A_matrix, VX_DIRECTIVE_AMD_COPY_TO_OPENCL));
	}

	// start the graph schedule
	ERROR_CHECK_STATUS_(vxScheduleGraph(stitch->graphStitch));
	stitch->scheduled = true;

	return VX_SUCCESS;
}

//! \brief Schedule next frame
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsWaitForCompletion(ls_context stitch)
{
	ERROR_CHECK_STATUS_(IsValidContextAndInitialized(stitch));
	if (!stitch->scheduled) {
		ls_printf("ERROR: lsWaitForCompletion: not scheduled\n");
		return VX_ERROR_GRAPH_SCHEDULED;
	}

	// wait for graph completion
	ERROR_CHECK_STATUS_(vxWaitGraph(stitch->graphStitch));
	stitch->scheduled = false;

	// debug: dump auxiliary data
	if (stitch->loomioAuxDumpFile) {
		vx_array auxList[] = { stitch->loomioCameraAuxData, stitch->loomioOverlayAuxData, stitch->loomioOutputAuxData, stitch->loomioViewingAuxData };
		for (size_t i = 0; i < sizeof(auxList) / sizeof(auxList[0]); i++) {
			if (auxList[i]) {
				vx_size numItems = 0;
				ERROR_CHECK_STATUS_(vxQueryArray(auxList[i], VX_ARRAY_NUMITEMS, &numItems, sizeof(numItems)));
				if (numItems > 0) {
					vx_map_id map_id = 0;
					vx_size stride = 0;
					char * ptr = nullptr;
					ERROR_CHECK_STATUS_(vxMapArrayRange(auxList[i], 0, numItems, &map_id, &stride, (void **)&ptr, VX_READ_ONLY, VX_MEMORY_TYPE_HOST, VX_NOGAP_X));
					fwrite(ptr, 1, numItems * stride, stitch->loomioAuxDumpFile);
					fflush(stitch->loomioAuxDumpFile);
					ERROR_CHECK_STATUS_(vxUnmapArrayRange(auxList[i], map_id));
				}
			}
		}
	}

	return VX_SUCCESS;
}

//! \brief query functions.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOpenVXContext(ls_context stitch, vx_context  * openvx_context)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (!stitch->context) {
		stitch->context = vxCreateContext();
		ERROR_CHECK_OBJECT_(stitch->context);
		if (stitch->opencl_context) {
			ERROR_CHECK_STATUS_(vxSetContextAttribute(stitch->context, VX_CONTEXT_ATTRIBUTE_AMD_OPENCL_CONTEXT, &stitch->opencl_context, sizeof(cl_context)));
		}
	}
	*openvx_context = stitch->context;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOpenCLContext(ls_context stitch, cl_context  * opencl_context)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (!stitch->opencl_context) {
		vx_context openvx_context = nullptr;
		vx_status status = lsGetOpenVXContext(stitch, &openvx_context);
		if (status)
			return status;
		status = vxQueryContext(openvx_context, VX_CONTEXT_ATTRIBUTE_AMD_OPENCL_CONTEXT, &stitch->opencl_context, sizeof(cl_context));
		if (status)
			return status;
	}
	*opencl_context = stitch->opencl_context;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetRigParams(ls_context stitch, rig_params * par)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	memcpy(par, &stitch->rig_par, sizeof(rig_params));
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraConfig(ls_context stitch, vx_uint32 * num_camera_rows, vx_uint32 * num_camera_columns, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	*num_camera_rows = stitch->num_camera_rows;
	*num_camera_columns = stitch->num_camera_columns;
	*buffer_format = stitch->camera_buffer_format;
	*buffer_width = stitch->camera_buffer_width;
	*buffer_height = stitch->camera_buffer_height;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputConfig(ls_context stitch, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	*buffer_format = stitch->output_buffer_format;
	*buffer_width = stitch->output_buffer_width;
	*buffer_height = stitch->output_buffer_height;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayConfig(ls_context stitch, vx_uint32 * num_overlay_rows, vx_uint32 * num_overlay_columns, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	*num_overlay_rows = stitch->num_overlay_rows;
	*num_overlay_columns = stitch->num_overlay_columns;
	*buffer_format = VX_DF_IMAGE_RGBX;
	*buffer_width = stitch->overlay_buffer_width;
	*buffer_height = stitch->overlay_buffer_height;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraParams(ls_context stitch, vx_uint32 cam_index, camera_params * par)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (cam_index >= stitch->num_cameras) {
		ls_printf("ERROR: lsGetCameraParams: invalid camera index (%d)\n", cam_index);
		return VX_ERROR_INVALID_VALUE;
	}
	memcpy(par, &stitch->camera_par[cam_index], sizeof(camera_params));
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayParams(ls_context stitch, vx_uint32 overlay_index, camera_params * par)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (overlay_index >= stitch->num_overlays) {
		ls_printf("ERROR: lsGetOverlayParams: invalid camera index (%d)\n", overlay_index);
		return VX_ERROR_INVALID_VALUE;
	}
	memcpy(par, &stitch->overlay_par[overlay_index], sizeof(camera_params));
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraBufferStride(ls_context stitch, vx_uint32 * camera_buffer_stride_in_bytes)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	*camera_buffer_stride_in_bytes = stitch->camera_buffer_stride_in_bytes;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputBufferStride(ls_context stitch, vx_uint32 * output_buffer_stride_in_bytes)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	*output_buffer_stride_in_bytes = stitch->output_buffer_stride_in_bytes;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayBufferStride(ls_context stitch, vx_uint32 * overlay_buffer_stride_in_bytes)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	*overlay_buffer_stride_in_bytes = stitch->overlay_buffer_stride_in_bytes;
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	strncpy(openvx_module, stitch->loomio_camera.module, openvx_module_size);
	strncpy(kernelName, stitch->loomio_camera.kernelName, kernelName_size);
	strncpy(kernelArguments, stitch->loomio_camera.kernelArguments, kernelArguments_size);
	openvx_module[openvx_module_size-1] = '\0';
	kernelName[kernelName_size - 1] = '\0';
	kernelArguments[kernelArguments_size - 1] = '\0';
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	strncpy(openvx_module, stitch->loomio_output.module, openvx_module_size);
	strncpy(kernelName, stitch->loomio_output.kernelName, kernelName_size);
	strncpy(kernelArguments, stitch->loomio_output.kernelArguments, kernelArguments_size);
	openvx_module[openvx_module_size - 1] = '\0';
	kernelName[kernelName_size - 1] = '\0';
	kernelArguments[kernelArguments_size - 1] = '\0';
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	strncpy(openvx_module, stitch->loomio_overlay.module, openvx_module_size);
	strncpy(kernelName, stitch->loomio_overlay.kernelName, kernelName_size);
	strncpy(kernelArguments, stitch->loomio_overlay.kernelArguments, kernelArguments_size);
	openvx_module[openvx_module_size - 1] = '\0';
	kernelName[kernelName_size - 1] = '\0';
	kernelArguments[kernelArguments_size - 1] = '\0';
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetViewingModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	strncpy(openvx_module, stitch->loomio_viewing.module, openvx_module_size);
	strncpy(kernelName, stitch->loomio_viewing.kernelName, kernelName_size);
	strncpy(kernelArguments, stitch->loomio_viewing.kernelArguments, kernelArguments_size);
	openvx_module[openvx_module_size - 1] = '\0';
	kernelName[kernelName_size - 1] = '\0';
	kernelArguments[kernelArguments_size - 1] = '\0';
	return VX_SUCCESS;
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsExportConfiguration(ls_context stitch, const char * exportType, char * buf, size_t size)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
#define APPEND_TO_BUF(call) { call; if (pos < size) { strncpy(&buf[pos], txt, size - pos); pos += strlen(&buf[pos]); } }
	buf[0] = buf[--size] = '\0';
	char txt[1024]; size_t pos = 0;
	if (!_stricmp(exportType, "loom_shell")) {
		buf[--size] = '\0';
		char txt[1024];
		size_t pos = 0;
		APPEND_TO_BUF(sprintf(txt, "lsSetRigParams(ls[0],{%.3f,%.3f,%.3f,%.3f})\n", stitch->rig_par.yaw, stitch->rig_par.pitch, stitch->rig_par.roll, stitch->rig_par.d));
		APPEND_TO_BUF(sprintf(txt, "lsSetOutputConfig(ls[0],%4.4s,%d,%d)\n", &stitch->output_buffer_format, stitch->output_buffer_width, stitch->output_buffer_height));
		APPEND_TO_BUF(sprintf(txt, "lsSetCameraConfig(ls[0],%d,%d,%4.4s,%d,%d)\n", stitch->num_camera_rows, stitch->num_camera_columns, &stitch->camera_buffer_format, stitch->camera_buffer_width, stitch->camera_buffer_height));
		for (vx_uint32 i = 0; i < stitch->num_cameras; i++) {
			camera_params camera_par = stitch->camera_par[i];
			APPEND_TO_BUF(sprintf(txt, "lsSetCameraParams(ls[0],%2d,{{%8.3f,%8.3f,%8.3f,%.0f,%.0f,%.0f},{%d,%.0f,%.3f,%f,%f,%f,%.3f,%.3f,%.1f}})\n", i,
				camera_par.focal.yaw, camera_par.focal.pitch, camera_par.focal.roll,
				camera_par.focal.tx, camera_par.focal.ty, camera_par.focal.tz,
				camera_par.lens.lens_type, camera_par.lens.haw, camera_par.lens.hfov,
				camera_par.lens.k1, camera_par.lens.k2, camera_par.lens.k3,
				camera_par.lens.du0, camera_par.lens.dv0, camera_par.lens.r_crop));
		}
		if (stitch->num_overlays > 0) {
			APPEND_TO_BUF(sprintf(txt, "lsSetOverlayConfig(%d,%d,RGBA,%d,%d)\n", stitch->num_overlay_rows, stitch->num_overlay_columns, stitch->overlay_buffer_width, stitch->overlay_buffer_height));
			for (vx_uint32 i = 0; i < stitch->num_overlays; i++) {
				camera_params camera_par = stitch->overlay_par[i];
				APPEND_TO_BUF(sprintf(txt, "lsSetOverlayParams(ls[0],%2d,{{%8.3f,%8.3f,%8.3f,%.0f,%.0f,%.0f},{%d,%.0f,%.3f,%f,%f,%f,%.3f,%.3f,%.1f}})\n", i,
					camera_par.focal.yaw, camera_par.focal.pitch, camera_par.focal.roll,
					camera_par.focal.tx, camera_par.focal.ty, camera_par.focal.tz,
					camera_par.lens.lens_type, camera_par.lens.haw, camera_par.lens.hfov,
					camera_par.lens.k1, camera_par.lens.k2, camera_par.lens.k3,
					camera_par.lens.du0, camera_par.lens.dv0, camera_par.lens.r_crop));
			}
		}
		if (stitch->output_buffer_stride_in_bytes) {
			APPEND_TO_BUF(sprintf(txt, "lsSetOutputBufferStride(ls[0],%d)\n", stitch->output_buffer_stride_in_bytes));
		}
		if (stitch->camera_buffer_stride_in_bytes) {
			APPEND_TO_BUF(sprintf(txt, "lsSetCameraBufferStride(ls[0],%d)\n", stitch->camera_buffer_stride_in_bytes));
		}
		if (stitch->overlay_buffer_stride_in_bytes) {
			APPEND_TO_BUF(sprintf(txt, "lsSetOverlayBufferStride(ls[0],%d)\n", stitch->overlay_buffer_stride_in_bytes));
		}
		if (stitch->loomio_viewing.kernelName[0]) {
			APPEND_TO_BUF(sprintf(txt, "lsSetViewingModule(ls[0],\"%s\",\"%s\",\"%s\")\n", stitch->loomio_viewing.module, stitch->loomio_viewing.kernelName, stitch->loomio_viewing.kernelArguments));
		}
		if (stitch->loomio_output.kernelName[0]) {
			APPEND_TO_BUF(sprintf(txt, "lsSetOutputModule(ls[0],\"%s\",\"%s\",\"%s\")\n", stitch->loomio_output.module, stitch->loomio_output.kernelName, stitch->loomio_output.kernelArguments));
		}
		if (stitch->loomio_camera.kernelName[0]) {
			APPEND_TO_BUF(sprintf(txt, "lsSetCameraModule(ls[0],\"%s\",\"%s\",\"%s\")\n", stitch->loomio_camera.module, stitch->loomio_camera.kernelName, stitch->loomio_camera.kernelArguments));
		}
		if (stitch->loomio_overlay.kernelName[0]) {
			APPEND_TO_BUF(sprintf(txt, "lsSetOverlayModule(ls[0],\"%s\",\"%s\",\"%s\")\n", stitch->loomio_overlay.module, stitch->loomio_overlay.kernelName, stitch->loomio_overlay.kernelArguments));
		}
		if (pos == size) {
			ls_printf("ERROR: lsExportConfiguration: %s: buffer size too small: %d\n", exportType, (vx_uint32)(size+1));
			return VX_ERROR_NOT_SUFFICIENT;
		}
	}
	else if (!_stricmp(exportType, "gdf:rig")) {
		APPEND_TO_BUF(sprintf(txt, "%.3f %.3f %.3f %.3f\n", stitch->rig_par.yaw, stitch->rig_par.pitch, stitch->rig_par.roll, stitch->rig_par.d));
		if (strlen(txt) < size) {
			strcpy(buf, txt);
		}
		else {
			ls_printf("ERROR: lsExportConfiguration: %s: buffer size too small: %d\n", exportType, (vx_uint32)(size + 1));
			return VX_ERROR_NOT_SUFFICIENT;
		}
	}
	else if (!_stricmp(exportType, "gdf:camera")) {
		// write output as hex values
		for (vx_uint32 i = 0; i < stitch->num_cameras; i++) {
			unsigned char * par = (unsigned char *) &stitch->camera_par[i];
			for (size_t j = 0; j < sizeof(camera_params); j++) {
				APPEND_TO_BUF(sprintf(txt, " %02X", par[j]));
			}
			APPEND_TO_BUF(sprintf(txt, "\n"));
		}
		if (pos == size) {
			ls_printf("ERROR: lsExportConfiguration: %s: buffer size too small: %d\n", exportType, (vx_uint32)(size + 1));
			return VX_ERROR_NOT_SUFFICIENT;
		}
	}
	else if (!_stricmp(exportType, "gdf:overlay")) {
		// write output as hex values
		for (vx_uint32 i = 0; i < stitch->num_overlays; i++) {
			unsigned char * par = (unsigned char *)&stitch->overlay_par[i];
			for (size_t j = 0; j < sizeof(camera_params); j++) {
				APPEND_TO_BUF(sprintf(txt, " %02X", par[j]));
			}
			APPEND_TO_BUF(sprintf(txt, "\n"));
		}
		if (pos == size) {
			ls_printf("ERROR: lsExportConfiguration: %s: buffer size too small: %d\n", exportType, (vx_uint32)(size + 1));
			return VX_ERROR_NOT_SUFFICIENT;
		}
	}
	else if (!_strnicmp(exportType, "pts", 3)) {
		const char * camFileFormat = exportType[3] == ':' ? &exportType[4] : "CAM%02d.jpg";
		bool gotCamFileList = strstr(camFileFormat, ",") ? true : false;
		if (stitch->num_cameras < 1) {
			ls_printf("ERROR: lsExportConfiguration: %s: no cameras detected in current configuration\n", exportType);
			return VX_ERROR_NOT_SUFFICIENT;
		}
		if (stitch->rig_par.d != 0.0f) {
			ls_printf("ERROR: lsExportConfiguration: %s: non-zero d field in rig_param is not supported\n", exportType);
			return VX_ERROR_NOT_SUPPORTED;
		}
		APPEND_TO_BUF(sprintf(txt, "# ptGui project file\n\n"));
		APPEND_TO_BUF(sprintf(txt, "p w%d h%d f2 v360 u0 n\"JPEG g0 q95\"\n", stitch->output_buffer_width, stitch->output_buffer_height));
		APPEND_TO_BUF(sprintf(txt, "m g0 i0\n\n"));
		APPEND_TO_BUF(sprintf(txt, "# input images:\n"));
		for (vx_uint32 i = 0; i < stitch->num_cameras; i++) {
			int lens_type = -1;
			if (stitch->camera_par[i].lens.lens_type == ptgui_lens_rectilinear)
				lens_type = 0;
			else if (stitch->camera_par[i].lens.lens_type == ptgui_lens_fisheye_ff)
				lens_type = 3;
			else if (stitch->camera_par[i].lens.lens_type == ptgui_lens_fisheye_circ)
				lens_type = 2;
			else {
				ls_printf("ERROR: lsExportConfiguration: %s: lens_type of camera#%d not supported\n", exportType, i);
				return VX_ERROR_NOT_SUPPORTED;
			}
			if (stitch->camera_par[i].focal.tx != 0.0f || stitch->camera_par[i].focal.ty != 0.0f || stitch->camera_par[i].focal.tz != 0.0f) {
				ls_printf("ERROR: lsExportConfiguration: %s: non-zero tx/ty/tz fields in camera#%d is not supported\n", exportType, i);
				return VX_ERROR_NOT_SUPPORTED;
			}
			// get camFileName from camFileFormat
			char camFileName[1024];
			if (gotCamFileList) {
				strncpy(camFileName, camFileFormat, sizeof(camFileName));
				char * p = strstr(camFileName, ","); if (p) *p = '\0';
				const char * s = camFileFormat;
				while (*s && *s != ',')
					s++;
				if (*s == ',')
					camFileFormat = s + 1;
			}
			else {
				sprintf(camFileName, camFileFormat, i);
			}
			// add camera entry
			vx_uint32 width = stitch->camera_buffer_width / stitch->num_camera_columns;
			vx_uint32 height = stitch->camera_buffer_height / stitch->num_camera_rows;
			APPEND_TO_BUF(sprintf(txt, "#-imgfile %d %d \"%s\"\n", width, height, camFileName));
			vx_float32 d = stitch->camera_par[i].lens.du0;
			vx_float32 e = stitch->camera_par[i].lens.dv0;
			if (stitch->camera_par[i].lens.lens_type == ptgui_lens_fisheye_circ && stitch->camera_par[i].lens.haw != width) {
				vx_uint32 left = 0, right = width - 1, top = 0, bottom = height - 1;
				int cx = (width / 2) + (int)d, cy = (height / 2) + (int)e, haw = (int)stitch->camera_par[i].lens.haw;
				left = cx - (haw / 2); right = left + haw - 1;
				top = cy - (haw / 2); bottom = top + haw - 1;
				d = d - (int)d;
				e = e - (int)e;
				APPEND_TO_BUF(sprintf(txt, "o f%d y%f r%f p%f v%f a%f b%f c%f d%f e%f g0 t0 C%d,%d,%d,%d\n",
					lens_type,
					stitch->camera_par[i].focal.yaw - stitch->rig_par.yaw,
					stitch->camera_par[i].focal.roll - stitch->rig_par.roll,
					stitch->camera_par[i].focal.pitch - stitch->rig_par.pitch,
					stitch->camera_par[i].lens.hfov,
					stitch->camera_par[i].lens.k1, stitch->camera_par[i].lens.k2, stitch->camera_par[i].lens.k3,
					d, e, left, right, top, bottom));
			}
			else {
				APPEND_TO_BUF(sprintf(txt, "o f%d y%f r%f p%f v%f a%f b%f c%f d%f e%f g0 t0\n",
					lens_type,
					stitch->camera_par[i].focal.yaw - stitch->rig_par.yaw,
					stitch->camera_par[i].focal.roll - stitch->rig_par.roll,
					stitch->camera_par[i].focal.pitch - stitch->rig_par.pitch,
					stitch->camera_par[i].lens.hfov * width / stitch->camera_par[i].lens.haw,
					stitch->camera_par[i].lens.k1, stitch->camera_par[i].lens.k2, stitch->camera_par[i].lens.k3,
					d, e));
			}
		}
	}
	else {
		ls_printf("ERROR: lsExportConfiguration: unsupported exportType: %s\n", exportType);
		return VX_ERROR_NOT_SUPPORTED;
	}
	return VX_SUCCESS;
#undef  APPEND_TO_BUF
}
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsImportConfiguration(ls_context stitch, const char * importType, const char * text)
{
	ERROR_CHECK_STATUS_(IsValidContext(stitch));
	if (!_stricmp(importType, "pts")) {
		if (stitch->num_cameras < 1) {
			ls_printf("ERROR: lsImportConfiguration: %s: needs more than one camera in the configuration\n", importType);
			return VX_ERROR_NOT_SUFFICIENT;
		}
		vx_uint32 camIndex = 0;
		camera_lens_type lens_type = ptgui_lens_fisheye_ff;
		vx_float32 a = 0, b = 0, c = 0, d = 0, e = 0, yaw = 0, pitch = 0, roll = 0, hfov = 0;
		vx_uint32 width = stitch->camera_buffer_width / stitch->num_camera_columns;
		vx_uint32 height = stitch->camera_buffer_height / stitch->num_camera_rows;
		bool isDummy = false;
		while (*text) {
			if (!strncmp(text, "#-dummyimage", 12)) {
				isDummy = true;
			}
			else if (*text == 'o') {
				vx_uint32 left = 0, top = 0, right = width-1, bottom = height-1;
				if (camIndex >= stitch->num_cameras) {
					ls_printf("ERROR: lsImportConfiguration: %s: PTS has more cameras than current configuration\n", importType);
					return VX_ERROR_NOT_SUPPORTED;
				}
				while (*text && *text != '\n') {
					// skip till whitespace
					while (*text && *text != '\n' && *text != ' ' && *text != '\t') text++;
					while (*text && *text != '\n' && (*text == ' ' || *text == '\t')) text++;
					// process fields
					if (*text == 'f') {
						if (text[1] == '0') lens_type = ptgui_lens_rectilinear;
						else if (text[1] == '2') lens_type = ptgui_lens_fisheye_circ;
						else if (text[1] == '3') lens_type = ptgui_lens_fisheye_ff;
						else {
							ls_printf("ERROR: lsImportConfiguration: %s: lens_type f%c not supported\n", importType, text[1]);
							return VX_ERROR_NOT_SUPPORTED;
						}
					}
					else if (*text == 'y') yaw = (float)atof(&text[1]);
					else if (*text == 'p') pitch = (float)atof(&text[1]);
					else if (*text == 'r') roll = (float)atof(&text[1]);
					else if (*text == 'v' && text[1] != '=') hfov = (float)atof(&text[1]);
					else if (*text == 'a' && text[1] != '=') a = (float)atof(&text[1]);
					else if (*text == 'b' && text[1] != '=') b = (float)atof(&text[1]);
					else if (*text == 'c' && text[1] != '=') c = (float)atof(&text[1]);
					else if (*text == 'd' && text[1] != '=') d = (float)atof(&text[1]);
					else if (*text == 'e' && text[1] != '=') e = (float)atof(&text[1]);
					else if (*text == 'C') sscanf(&text[1], "%d,%d,%d,%d", &left, &right, &top, &bottom);
				}
				if (!isDummy) {
					camera_params * par = &stitch->camera_par[camIndex++];
					par->focal.yaw = yaw;
					par->focal.pitch = pitch;
					par->focal.roll = roll;
					par->focal.tx = 0.0f;
					par->focal.ty = 0.0f;
					par->focal.tz = 0.0f;
					par->lens.lens_type = lens_type;
					par->lens.hfov = hfov;
					par->lens.k1 = a;
					par->lens.k2 = b;
					par->lens.k3 = c;
					par->lens.du0 = d;
					par->lens.dv0 = e;
					par->lens.haw = (float)width;
					par->lens.r_crop = 0.0f;
					if (lens_type == ptgui_lens_fisheye_circ) {
						par->lens.haw = (float)(right - left);
						par->lens.r_crop = par->lens.haw * 0.5f;
					}
				}
				isDummy = false;
			}
			// skip till end-of-line
			while (*text && *text != '\n')
				text++;
			if (*text == '\n')
				text++;
		}
		if (camIndex != stitch->num_cameras) {
			ls_printf("ERROR: lsImportConfiguration: %s: could not import for all %d cameras (found %d)\n", importType, stitch->num_cameras, camIndex);
			return VX_ERROR_NOT_SUFFICIENT;
		}
	}
	else {
		ls_printf("ERROR: lsImportConfiguration: unsupported importType: %s\n", importType);
		return VX_ERROR_NOT_SUPPORTED;
	}
	return VX_SUCCESS;
}
