# Radeon Loom Stitching Library Settings(vx_loomsl)
 There are different ways to improve the output or the performance of the stitch process by optimizing or changing certain parameters and settings.
 The settings can be set using [loom_shell](../../utils/loom_shell/README.md) or by using the [API](../README.md).
 In both cases the following function could be used:
 ...
 setGlobalAttribute(offset,value);
 
 Where offset is a number for a certain parameter.
 
 ## General Settings:
 
 * *Profiler:* [0] A profiler, which shows performance numbers, can be turned on; **0: Off**, 1: On



 
 
 ## Warp
 
 ## Exposure Compensation
 * *General Setting:* [1] Option to disable the complete module; **0: Off**, 1: On
 
 ## Seamfind
  * *General Setting:* [2] Option to disable the complete module; **0: Off**, 1: On
  * *Refresh:* [3] Option to disable the seamfind refresh (only calculate seamfind ones); **0: Off**, 1: On
  * *Cost selection:* [4] Option for the cost generation; **0: OpenVX Sobel Mag/Phase**, 1: Optimized
 
 ## Multiband
 
 
