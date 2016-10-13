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


#ifndef __LIVE_STITCH_API_H__
#define __LIVE_STITCH_API_H__

//////////////////////////////////////////////////////////////////////
// standard header files
#include <VX/vx.h>
#include <CL/cl.h>

//////////////////////////////////////////////////////////////////////
//! \brief The attributes
enum {
	LIVE_STITCH_ATTR_PROFILER               =    0,   // profiler attribute: 0:OFF 1:ON
	LIVE_STITCH_ATTR_EXPCOMP                =    1,   // exp-comp attribute: 0:OFF 1:ON
	LIVE_STITCH_ATTR_SEAMFIND               =    2,   // seamfind attribute: 0:OFF 1:ON
	LIVE_STITCH_ATTR_SEAM_REFRESH           =    3,   // seamfind seam refresh attribute: 0:OFF 1:ON
	LIVE_STITCH_ATTR_SEAM_COST_SELECT       =    4,   // seamfind cost generate attribute: 0:OpenVX Sobel Mag/Phase 1:Optimized Sobel Mag/Phase
	LIVE_STITCH_ATTR_MULTIBAND              =    5,   // multiband attribute: 0:OFF 1:ON
	LIVE_STITCH_ATTR_MULTIBAND_NUMBANDS     =    6,   // multiband attribute: numbands 2-6
	LIVE_STITCH_ATTR_STITCH_MODE            =    7,   // stitch mode: 0:normal 1:quick (default: normal)
	LIVE_STITCH_ATTR_INPUT_SCALE_FACTOR     =    8,   // input scale factor: use 0.5 or 1.0 (default 1.0)
	LIVE_STITCH_ATTR_OUTPUT_SCALE_FACTOR    =    9,   // output scale factor: use 0.5 or 1.0 (default 1.0)
	LIVE_STITCH_ATTR_ENABLE_REINITIALIZE    =   10,   // enable lsReinitialize (default disabled)
	LIVE_STITCH_ATTR_CT_OVERLAP_RECT        =   11,   // Initialize Stitch Config attribute: 0 - N Pixels. Reduces the overlap region by n*n pixels.
	LIVE_STITCH_ATTR_CT_SEAM_VERT_PRIORITY  =   12,   // Initialize Stitch Config attribute: -1 to N Flag. -1:Disable 1:highest N:Lowest.
	LIVE_STITCH_ATTR_CT_SEAM_HORT_PRIORITY  =   13,   // Initialize Stitch Config attribute: -1 to N Flag. -1:Disable 1:highest N:Lowest.
	LIVE_STITCH_ATTR_CT_SEAM_FREQUENCY      =   14,   // Initialize Stitch Config attribute: 0 - N Frames. Frequency of seam calculation.
	LIVE_STITCH_ATTR_CT_SEAM_QUALITY        =   15,   // Initialize Stitch Config attribute: 0 - N Flag.   0:Disable Edgeness 1:Enable Edgeness
	LIVE_STITCH_ATTR_CT_SEAM_STAGGER        =   16,   // Initialize Stitch Config attribute: 0 - N Frames. Stagger the seam calculation by N frames
	LIVE_STITCH_ATTR_IO_AUX_DATA_CAPACITY   =   32,   // LoomIO: auxiliary data buffer size in bytes. Default 1024.
	// Dynamic LoomSL attributes
	LIVE_STITCH_ATTR_SEAM_THRESHOLD			=	51,    // seamfind seam refresh Threshold: 0 - 100 percentage change
	// ... reserved for LoomSL internal attributes
	LIVE_STITCH_ATTR_RESERVED_CORE_END      =  127,   // reserved first 128 attributes for LoomSL internal attributes
	LIVE_STITCH_ATTR_RESERVED_EXT_BEGIN     =  128,   // start of reserved attributes for extensions
	// ... reserved for extension attributes
	LIVE_STITCH_ATTR_MAX_COUNT              =  256    // total number of attributes
};


//////////////////////////////////////////////////////////////////////
// Camera Model Parameters

//! \brief The lens type 
typedef enum {
	ptgui_lens_rectilinear  = 0, // PTGui rectilinear lens model
	ptgui_lens_fisheye_ff   = 1, // PTGui full-frame fisheye lens model
	ptgui_lens_fisheye_circ = 2, // PTGui circular fisheye lens model
	adobe_lens_rectilinear  = 3, // adobe rectilinear lens model
	adobe_lens_fisheye_ff   = 4, // adobe full-frame fisheye lens model
} camera_lens_type;

//! \brief The lens distortion parameters 
typedef struct {
	camera_lens_type lens_type;  // lens type
	float            haw;        // horizontal active pixel count (should be < width)
	float            hfov;       // horizontal viewing angle (in degrees)
	float            k1, k2, k3; // lens distortion correction parameters (PTGui: a, b, c)
	float            du0, dv0;   // optical center correction in pixel units
	float            r_crop;     // crop radius in pixels; pixels are invalid if r_crop > 0 && r_crop > r
} camera_lens_params;

//! \brief The camera alignment.
//  Uses left-handed Cartesian coordinate system with camera pointing in +ve z-direction.
typedef struct {
    float yaw, pitch, roll; // orientation in degrees (y-axis, x-axis, z-axis)
    float tx, ty, tz;       // focal point from rig center in depth pixel units
} camera_align;

//! \brief The camera parameters.
typedef struct {
    camera_align       focal;  // camera focal point and direction parameters
    camera_lens_params lens;   // lens parameters
} camera_params;

//! \brief The rig parameters.
typedef struct {
    float yaw, pitch, roll; // orientation in degrees (y-axis, x-axis, z-axis)
    float d;                // focus sphere radius in depth pixel units (default: 0.0 for infinity)
} rig_params;

//////////////////////////////////////////////////////////////////////
//! \brief The log callback function
typedef void(*stitch_log_callback_f)(const char * message);

//////////////////////////////////////////////////////////////////////
//! \brief The stitch context
typedef struct ls_context_t * ls_context;

//! \brief The stitch API linkage
#ifndef LIVE_STITCH_API_ENTRY
#define LIVE_STITCH_API_ENTRY extern "C"
#endif

//! \brief Get version.
LIVE_STITCH_API_ENTRY const char * VX_API_CALL lsGetVersion();

//! \brief Set callback for log messages.
LIVE_STITCH_API_ENTRY void VX_API_CALL lsGlobalSetLogCallback(stitch_log_callback_f callback);

//! \brief Set global attributes. Note that current global attributes will become default attributes for when a stitch context is created.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGlobalSetAttributes(vx_uint32 attr_offset, vx_uint32 attr_count, const vx_float32 * attr_ptr);

//! \brief Get global attributes. Note that current global attributes will become default attributes for when a stitch context is created.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGlobalGetAttributes(vx_uint32 attr_offset, vx_uint32 attr_count, vx_float32 * attr_ptr);

//! \brief Create stitch context.
LIVE_STITCH_API_ENTRY ls_context VX_API_CALL lsCreateContext();

//! \brief Release stitch context. The ls_context will be reset to NULL.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsReleaseContext(ls_context * pStitch);

//! \brief Set stitch configuration.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOpenVXContext(ls_context stitch, vx_context  openvx_context);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOpenCLContext(ls_context stitch, cl_context  opencl_context);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetRigParams(ls_context stitch, const rig_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraConfig(ls_context stitch, vx_uint32 num_camera_rows, vx_uint32 num_camera_columns, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputConfig(ls_context stitch, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayConfig(ls_context stitch, vx_uint32 num_overlay_rows, vx_uint32 num_overlay_columns, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraParams(ls_context stitch, vx_uint32 cam_index, const camera_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayParams(ls_context stitch, vx_uint32 overlay_index, const camera_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraBufferStride(ls_context stitch, vx_uint32 camera_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputBufferStride(ls_context stitch, vx_uint32 output_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayBufferStride(ls_context stitch, vx_uint32 overlay_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetViewingModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);

//! \brief initialize/reinitialize the stitch context.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsInitialize(ls_context stitch);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsReinitialize(ls_context stitch);

//! \brief Set OpenCL buffers
//     input_buffer   - input opencl buffer with images from all cameras
//     overlay_buffer - overlay opencl buffer with all images
//     output_buffer  - output opencl buffer for output equirectangular image
//   Use of nullptr will return the control of previously set buffer
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraBuffer(ls_context stitch, cl_mem * input_buffer);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputBuffer(ls_context stitch, cl_mem * output_buffer);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayBuffer(ls_context stitch, cl_mem * overlay_buffer);

//! \brief Schedule a frame
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsScheduleFrame(ls_context stitch);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsWaitForCompletion(ls_context stitch);

//! \brief access to context specific attributes.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetAttributes(ls_context stitch, vx_uint32 attr_offset, vx_uint32 attr_count, const vx_float32 * attr_ptr);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetAttributes(ls_context stitch, vx_uint32 attr_offset, vx_uint32 attr_count, vx_float32 * attr_ptr);

//! \brief query functions.
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOpenVXContext(ls_context stitch, vx_context  * openvx_context);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOpenCLContext(ls_context stitch, cl_context  * opencl_context);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetRigParams(ls_context stitch, rig_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraConfig(ls_context stitch, vx_uint32 * num_camera_rows, vx_uint32 * num_camera_columns, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputConfig(ls_context stitch, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayConfig(ls_context stitch, vx_uint32 * num_overlay_rows, vx_uint32 * num_overlay_columns, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraParams(ls_context stitch, vx_uint32 cam_index, camera_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayParams(ls_context stitch, vx_uint32 overlay_index, camera_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraBufferStride(ls_context stitch, vx_uint32 * camera_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputBufferStride(ls_context stitch, vx_uint32 * output_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayBufferStride(ls_context stitch, vx_uint32 * overlay_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetViewingModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsExportConfiguration(ls_context stitch, const char * exportType, char * text, size_t size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsImportConfiguration(ls_context stitch, const char * importType, const char * text);

#endif //__LIVE_STITCH_API_H__
