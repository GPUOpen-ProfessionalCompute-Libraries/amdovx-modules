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
//  - provide a convenient way to manipulate certain features and parameters of the stitch
//  - all attributes assume default values when not set explicitly before creating the context
//  - the default values of these attributes will be good enough for most applications
//  - only dynamic LoomSL attributes can be modified using lsSetAttributes API 
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
	LIVE_STITCH_ATTR_REDUCE_OVERLAP_REGION  =   11,   // Reduces the overlap region by n*n pixels (default: 0)
	LIVE_STITCH_ATTR_SEAM_VERT_PRIORITY     =   12,   // Vertical seam priority: -1 to N Flag. -1:Disable 1:highest N:Lowest. (default 0)
	LIVE_STITCH_ATTR_SEAM_HORT_PRIORITY     =   13,   // Horizontal seam priority: -1 to N Flag. -1:Disable 1:highest N:Lowest. (default 0)
	LIVE_STITCH_ATTR_SEAM_FREQUENCY         =   14,   // Seam frequecy: 0 - N Frames. Frequency of seam calculation.
	LIVE_STITCH_ATTR_SEAM_QUALITY           =   15,   // Seam quality, quality: 0 - N Flag.   0:Disable Edgeness 1:Enable Edgeness (default 0)
	LIVE_STITCH_ATTR_SEAM_STAGGER           =   16,   // Seam stagger: 0 - N Frames. Stagger the seam calculation by N frames
	LIVE_STITCH_ATTR_SEAM_LOCK              =   17,   // Seam lock (default: 0)
	LIVE_STITCH_ATTR_SEAM_FLAGS             =   18,   // Seam flags (default: 0)
	LIVE_STITCH_ATTR_MULTIBAND_PAD_PIXELS   =   19,   // multiband attribute: padding pixel count (default: 0)
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
	adobe_lens_fisheye      = 4, // adobe fisheye lens model
} camera_lens_type;

//! \brief The lens distortion parameters 
typedef struct {
	float            hfov;        // horizontal viewing angle (in degrees)
	float            haw;         // horizontal active pixel count (should be < width)
	float            r_crop;      // crop radius in pixels; pixels are invalid if r_crop > 0 && r_crop > r
	float            du0, dv0;    // optical center correction in pixel units
	camera_lens_type lens_type;   // lens type
	float            k1, k2, k3;  // lens distortion correction parameters (PTGui: a, b, c)
	float            reserved[7]; // reserved (shall be zero)
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

//! \brief Query the version of live stitch API implementation.
//  - return version string
LIVE_STITCH_API_ENTRY const char * VX_API_CALL lsGetVersion();

//! \brief Set callback for log messages.
//  - by default, log messages from library will be printed to stdout
//  - the log messages can be redirected to application using a callback
LIVE_STITCH_API_ENTRY void VX_API_CALL lsGlobalSetLogCallback(stitch_log_callback_f callback);

//! \brief Set global attributes. 
//  - global attributes will decide the features and parameters at the time of stitch context creation
//  - Note that updating global attributes will not change the features and/or parameters of stitch contexts that have already been created
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGlobalSetAttributes(vx_uint32 attr_offset, vx_uint32 attr_count, const vx_float32 * attr_ptr);

//! \brief Query current values of global attributes.
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGlobalGetAttributes(vx_uint32 attr_offset, vx_uint32 attr_count, vx_float32 * attr_ptr);

//! \brief Create stitch context.
//  - create a stitch context using the feature dictated by global attributes prior to this call
//  - returns NULL on error (see log messages for further details)
LIVE_STITCH_API_ENTRY ls_context VX_API_CALL lsCreateContext();

//! \brief Release stitch context. The ls_context will be reset to NULL.
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsReleaseContext(ls_context * pStitch);

//! \brief Set the stitched output image buffer format and dimensions
//  - supported formats: VX_DF_IMAGE_RGB, VX_DF_IMAGE_UYVY, VX_DF_IMAGE_YUYV
//  - buffer_width must be multiple of 16
//  - buffer_height must be multiple of 2
//  - Note that output scale factor attribute is not supported when output format is VX_DF_IMAGE_RGB
//  - Note that this function must be called before lsInitialize call
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputConfig(ls_context stitch, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height);

//! \brief Set the stitched input camera image buffer format and dimensions
//  - num_camera_rows: number of image tile rows inside the buffer (veritical direction)
//  - num_camera_columns: number of image tile columns inside the buffer (horizontal direction)
//  - supported formats: VX_DF_IMAGE_RGB, VX_DF_IMAGE_UYVY, VX_DF_IMAGE_YUYV
//  - buffer_width must be multiple of 16 and less than 8K
//  - buffer_height must be multiple of 2 and less than 8K
//  - dimensions of all image tiles inside the buffer must be same
//  - Note that input scale factor attribute is not supported when input format is VX_DF_IMAGE_RGB
//  - Note that this function must be called before lsInitialize call
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraConfig(ls_context stitch, vx_uint32 num_camera_rows, vx_uint32 num_camera_columns, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height);

//! \brief Set the stitched input overlay image buffer format and dimensions
//  - num_overlay_rows: number of image tile rows inside the buffer (veritical direction)
//  - num_overlay_columns: number of image tile columns inside the buffer (horizontal direction)
//  - supported formats: VX_DF_IMAGE_RGBX (with alpha channel)
//  - buffer_width must be multiple of 16 and less than 8K
//  - buffer_height must be multiple of 2 and less than 8K
//  - dimensions of all image tiles inside the buffer must be same
//  - Note that this function must be called before lsInitialize call to activate overlay feature
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayConfig(ls_context stitch, vx_uint32 num_overlay_rows, vx_uint32 num_overlay_columns, vx_df_image buffer_format, vx_uint32 buffer_width, vx_uint32 buffer_height);

//! \brief Set the orientation and lens parameters of individual camera
//  - cam_index: index of camera in the rig: must be [0 .. (num_camera_rows*num_camera_columns-1)]
//  - par: camera orientation and lens parameters
//  - Note that camera parameters must be set using this call or lsImportConfiguration before lsInitialize call
//  - If reinitialize is enabled, this function can be called after lsInitialize.
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraParams(ls_context stitch, vx_uint32 cam_index, const camera_params * par);

//! \brief Set the orientation and lens parameters of individual overlay
//  - overlay_index: index of overlay: must be [0 .. (num_overlay_rows*num_overlay_columns-1)]
//  - par: overlay orientation and lens parameters (use rectilinear lens model for planar videos)
//  - Note that this function must be called before lsInitialize call if overlays are used
//  - If reinitialize is enabled, this function can be called after lsInitialize.
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayParams(ls_context stitch, vx_uint32 overlay_index, const camera_params * par);

//! \brief Set the rig orientation that correspond to center of the output Equirectangular image.
//  - par: rig orientation
//  - Note that this function must be called before lsInitialize if application wants to set rig orientation
//  - If reinitialize is enabled, this function can be called after lsInitialize.
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetRigParams(ls_context stitch, const rig_params * par);

//! \brief Set LoomIO plug-in module for input, overlay, and outputs.
//  - LoomIO plug-in is an OpenVX kernel with kernels arguments compatible with LoomIO specification
//       lsSetCameraModule: LoomIO plug-in for camera capture
//       lsSetOverlayModule: LoomIO plug-in for dynamic overlay generation
//       lsSetOutputModule: LoomIO plug-in for stitched image output
//       lsSetViewingModule: LoomIO plug-in for local viewing of stitched RGB output
//  - kernelName: name of the OpenVX kernel that implements the LoomIO plug-in
//  - kernelArguments: LoomIO plug-in specific arguments (refer to LoomIO plug-in documentation for details)
//  - openvx_module: name of OpenVX module that implements that OpenVX kernel
//  - Note that these function must be called before lsInitialize call to activate LoomIO plug-ins
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetViewingModule(ls_context stitch, const char * openvx_module, const char * kernelName, const char * kernelArguments);

//! \brief Set the stride of OpenCL buffers.
//  - Note that these function must be called before lsInitialize call if LoomIO plug-ins are not used and stride requires extra padding
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraBufferStride(ls_context stitch, vx_uint32 camera_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputBufferStride(ls_context stitch, vx_uint32 output_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayBufferStride(ls_context stitch, vx_uint32 overlay_buffer_stride_in_bytes);

//! \brief Set application specific OpenCL context for use by LoomSL library.
//  - Note that these function must be called before lsInitialize call if application requires use of a its own OpenCL context
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOpenCLContext(ls_context stitch, cl_context opencl_context);

//! \brief Set application specific OpenVX context for use by LoomSL library.
//  - Note that these function must be called before lsInitialize call if application requires use of a its own OpenVX context
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOpenVXContext(ls_context stitch, vx_context openvx_context);

//! \brief initialize the stitch context.
//  - shall be called after all the configuration parameters are set and before scheduling a frame for stitching
//  - can be called only once after creating the context
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsInitialize(ls_context stitch);

//! \brief reinitialize the stitch context.
//  - need to reinitialize if any camera/overlay/rig parameters are updated
//  - can be called only after lsInitialize call
//  - this feature is disabled by default (use global attribute to enable)
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsReinitialize(ls_context stitch);

//! \brief Set OpenCL buffers
//     input_buffer   - input opencl buffer with images from all cameras
//     overlay_buffer - overlay opencl buffer with all images
//     output_buffer  - output opencl buffer for output equirectangular image
//   Use of nullptr will return the control of previously set buffer
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetCameraBuffer(ls_context stitch, cl_mem * input_buffer);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOutputBuffer(ls_context stitch, cl_mem * output_buffer);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetOverlayBuffer(ls_context stitch, cl_mem * overlay_buffer);

//! \brief Schedule a frame
//  - only one frame can be scheduled at a time
//  - every lsScheduleFrame call should be followed by lsWaitForCompletion call
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsScheduleFrame(ls_context stitch);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsWaitForCompletion(ls_context stitch);

//! \brief access to context specific attributes.
//  - only dynamic LoomSL attributes can be modified using lsSetAttributes API
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsSetAttributes(ls_context stitch, vx_uint32 attr_offset, vx_uint32 attr_count, const vx_float32 * attr_ptr);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetAttributes(ls_context stitch, vx_uint32 attr_offset, vx_uint32 attr_count, vx_float32 * attr_ptr);

//! \brief Query configuration.
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputConfig(ls_context stitch, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraConfig(ls_context stitch, vx_uint32 * num_camera_rows, vx_uint32 * num_camera_columns, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayConfig(ls_context stitch, vx_uint32 * num_overlay_rows, vx_uint32 * num_overlay_columns, vx_df_image * buffer_format, vx_uint32 * buffer_width, vx_uint32 * buffer_height);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraParams(ls_context stitch, vx_uint32 cam_index, camera_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayParams(ls_context stitch, vx_uint32 overlay_index, camera_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetRigParams(ls_context stitch, rig_params * par);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetViewingModule(ls_context stitch, char * openvx_module, size_t openvx_module_size, char * kernelName, size_t kernelName_size, char * kernelArguments, size_t kernelArguments_size);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetCameraBufferStride(ls_context stitch, vx_uint32 * camera_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOutputBufferStride(ls_context stitch, vx_uint32 * output_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOverlayBufferStride(ls_context stitch, vx_uint32 * overlay_buffer_stride_in_bytes);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOpenCLContext(ls_context stitch, cl_context  * opencl_context);
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsGetOpenVXContext(ls_context stitch, vx_context  * openvx_context);

//! \brief import configuration.
//  - importType: "pts" - PtGui project (.pts text file)
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsImportConfiguration(ls_context stitch, const char * importType, const char * fileName);

//! \brief export configuration.
//  - exportType: "pts" - PtGui project (.pts text file)
//  - exportType: "loom_shell" - loom_shell script (.lss text file)
//  - return VX_SUCCESS or error code (see log messages for further details)
LIVE_STITCH_API_ENTRY vx_status VX_API_CALL lsExportConfiguration(ls_context stitch, const char * exportType, const char * fileName);

#endif //__LIVE_STITCH_API_H__
