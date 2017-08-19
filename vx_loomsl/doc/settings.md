# Radeon Loom Stitching Library Settings(vx_loomsl)
 There are different ways to improve the output or the performance of the stitch process by optimizing or changing certain parameters and settings.
 The settings can be set using [loom_shell](../../utils/loom_shell/README.md) or by using the [API](../README.md).
 In both cases the following function could be used:
 
``` 
setGlobalAttribute(offset,value);
```
 
 Where offset is a number for a certain parameter.
 
 ## General Settings:
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 |LIVE_STITCH_ATTR_PROFILER | 0      | A profiler, which shows performance numbers | **0: Off**, 1: On |
 |LIVE_STITCH_ATTR_STITCH_MODE | 7 | Stitch mode | **0: Normal Stitch**, 1: Quick Stitch |
 |LIVE_STITCH_ATTR_ENABLE_REINITIALIZE | 10 | Reinitialization | **0: Disable**, 1: Enable |
 |LIVE_STITCH_ATTR_FAST_INIT | 56 | Quick reinitialization on GPU | 0: Off, **1: On** |
 |LIVE_STITCH_ATTR_SAVE_AND_LOAD_INIT | 57 | Save initialized stitch tables for quick run&load | **0: Off**, 1: On |
 
 ## High precision 
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 |LIVE_STITCH_ATTR_PRECISION | 58 | Enables a 16bit flow from input to output color convert | **0: Auto-Detect**, 1: 8-bit, 2: 16-bit |
 |LIVE_STITCH_ATTR_WARP_INTERPOLATION | 59 | Warp interpolation mode | **0: bilinear**, 1: bicubic |
 |LIVE_STITCH_ATTR_LINEAR_COLORSPACE | 60 | Use a linear colorspace in the whole flow | **0: Nonlinear**, 1: Linear |
 
 ## Temporal Filter - Noise Filter
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 |LIVE_STITCH_ATTR_NOISE_FILTER | 55 | Option to enable/disable the module | **0: Off**, 1: On |
 |LIVE_STITCH_ATTR_NOISE_FILTER_LAMBDA | 65 | Dynamic Lambda to control denoising | **1**; Range: 0 - 1 | 
 
 ## Exposure Compensation
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 | LIVE_STITCH_ATTR_EXPCOMP | 1 | Option to enable/disable the module | 0: Off, **1: On** |
 | LIVE_STITCH_ATTR_EXPCOMP_GAIN_IMG_W | 30 | Gain image width | **1** |
 | LIVE_STITCH_ATTR_EXPCOMP_GAIN_IMG_H | 31 | Gain image height | **1** |
 | LIVE_STITCH_ATTR_EXPCOMP_GAIN_IMG_C | 36 | Gain image number channels | **1** |
 | LIVE_STITCH_ATTR_EXPCOMP_ALPHA_VALUE | 37 | Variance of gain | **0.01** |
 | LIVE_STITCH_ATTR_EXPCOMP_BETA_VALUE | 38 | Variance of mean intensity | **100** |
  
 ## Seamfind
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 |LIVE_STITCH_ATTR_SEAMFIND | 2 | Option to enable/disable the module | 0: Off, **1: On** |
 |LIVE_STITCH_ATTR_SEAM_REFRESH | 3 | Option to disable the seamfind refresh (only calculate seamfind ones) | 0: Off, **1: On** |
 |LIVE_STITCH_ATTR_SEAM_COST_SELECT | 4 | Option for the cost generator | 0: OpenVX Sobel Mag/Phase, **1: Optimized** |
 |LIVE_STITCH_ATTR_REDUCE_OVERLAP_REGION | 11 | Reduces the overlap region by n * n pixels | **0** |
 |LIVE_STITCH_ATTR_SEAM_VERT_PRIORITY | 12 | Vertical seam priority: -1 to N Flag. -1:Disable 1:highest N:Lowest | **1** |
 |LIVE_STITCH_ATTR_SEAM_HORT_PRIORITY | 13 | Horizontal seam priority: -1 to N Flag. -1:Disable 1:highest N:Lowest | **-1** |
 |LIVE_STITCH_ATTR_SEAM_FREQUENCY  | 14 | Seam frequecy: 0 - N Frames. Frequency of seam calculation. | **6000** |
 |LIVE_STITCH_ATTR_SEAM_QUALITY | 15 | Seam quality, quality: 0 - N Flag.   0:Disable Edgeness 1:Enable Edgeness | **1** |
 |LIVE_STITCH_ATTR_SEAM_STAGGER | 16 | Seam stagger: 0 - N Frames. Stagger the seam calculation by N frames | **1** |
 |LIVE_STITCH_ATTR_SEAM_LOCK | 17 | Seam lock | **0** |
 |LIVE_STITCH_ATTR_SEAM_FLAGS | 18 | Seam flags | **0** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_ENABLE | 20 | Find special case for circular fisheye on equator | 0: disabled, **1: enabled** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_HFOV_MIN | 21 | Min HFOV in degrees | **120** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_PITCH_TOL | 22 | Pitch tolerance in degrees | **5** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_YAW_TOL | 23 | Yaw tolerance in degrees | **5** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_OVERLAP_HR | 24 | Max horizental overlap ratio | **0.15** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_OVERLAP_VD | 25 | Max vertical overlap in degrees |  **20** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_TOPBOT_TOL | 26 | Top and bottom camera pitch tolerance | **5** |
 |LIVE_STITCH_ATTR_SEAM_COEQUSH_TOPBOT_VGD | 27 | Top and bottom camera vertical overlap clamp in degrees for equaor cameras | **0** |
 |LIVE_STITCH_ATTR_SEAM_THRESHOLD  | 64 | Seamfind refresh Threshold: 0 - 100 percentage change | **25** | 

 
 ## Multiband
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 |LIVE_STITCH_ATTR_MULTIBAND | 5 | Option to enable/disable the module | 0: Off, **1: On** |
 |LIVE_STITCH_ATTR_MULTIBAND_NUMBANDS | 6 | Number of bands | **4**; Range 2-6 |
 |LIVE_STITCH_ATTR_MULTIBAND_PAD_PIXELS | 29 | Padding pixel count | **64** |
 
 ## Chrominance Key
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 |LIVE_STITCH_ATTR_CHROMA_KEY	 | 50 | Option to enable/disable the module | **0: Off**, 1: On |
 |LIVE_STITCH_ATTR_CHROMA_KEY_VALUE | 51 | Key value | **8454016** (Green 0x80FF80), Range: 0 - N |
 |LIVE_STITCH_ATTR_CHROMA_KEY_TOL | 52 | Tolerance | **25**, Range: 0 - N |
 |LIVE_STITCH_ATTR_CHROMA_KEY_EED | 53 | Erode and dilate mask | **0: Off**, 1: On |
 
 ## 4k encoding
 | Name  | Offset | Description                                 | Value           |
 |-------|------------------|---------------------------------------------|-------------------|
 |LIVE_STITCH_ATTR_OUTPUT_TILE_NUM_X | 40 | Number of horizontal tiles in the output | **1** |
 |LIVE_STITCH_ATTR_OUTPUT_TILE_NUM_Y | 41 | Number of vertical tiles in the output | **1** |
 |LIVE_STITCH_ATTR_OUTPUT_src_tile_overlap | 42 | Overlap pixel count | **0** |
 |LIVE_STITCH_ATTR_OUTPUT_TILE_BUFFER_VALUE | 43 | Tiled buffer default value | **0** |
 |LIVE_STITCH_ATTR_OUTPUT_ENCODER_WIDTH | 44 | Encoder buffer width | **3840** |
 |LIVE_STITCH_ATTR_OUTPUT_ENCODER_HEIGHT | 45 | Encoder buffer height | **2160** |
 |LIVE_STITCH_ATTR_OUTPUT_ENCODER_STRIDE_Y | 46 | Encoder buffer stride_y | **2160** |
 
 ## LoomIO
| Name  | Offset | Description                                 | Value           |
|-------|------------------|---------------------------------------------|-------------------|
|LIVE_STITCH_ATTR_IO_OUTPUT_AUX_SELECTION | 32 | Auxiliary data buffer selection | **0: default**, 1: camera, 2: overlay |
|LIVE_STITCH_ATTR_IO_CAMERA_AUX_DATA_SIZE | 33 | Camera auxiliary data buffer size in byte | **1024** |
|LIVE_STITCH_ATTR_IO_OVERLAY_AUX_DATA_SIZE | 34 | Overlay auxiliary data buffer size in byte | **1024** |
|LIVE_STITCH_ATTR_IO_OUTPUT_AUX_DATA_SIZE | 35 | Display auxiliary data buffer size in byte | **1024** |

 
