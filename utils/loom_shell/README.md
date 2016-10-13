# Radeon LoomShell

## DESCRIPTION
LoomShell is an interpreter that enables stitching 360 degree videos using a script. It provides direct access to Live Stitch API by encapsulating the calls to enable rapid prototyping.

### Command-line Usage
    % loom_shell.exe [script.lss]

### Available Commands
    basic commands
        help
        verbose ON
        verbose OFF
        run script.lss
        exit
        quit
    context
        lsCreateContext() => ls[#]
        lsReleaseContext(ls[#])
    rig and image configuration
        lsSetOutputConfig(ls[#],format,width,height)
        lsSetCameraConfig(ls[#],num_rows,num_cols,format,width,height)
        lsSetCameraParams(ls[#],index,{{yaw,pitch,roll,tx,ty,tz},{lens,haw,hfov,k1,k2,k3,du0,dv0,r_crop}})
        lsSetOverlayConfig(ls[#],num_rows,num_cols,format,width,height)
        lsSetOverlayParams(ls[#],index,{{yaw,pitch,roll,tx,ty,tz},{lens,haw,hfov,k1,k2,k3,du0,dv0,r_crop}})
        lsSetRigParams(ls[#],{yaw,pitch,roll,d})
        lsGetOutputConfig(ls[#])
        lsGetCameraConfig(ls[#])
        lsGetCameraParams(ls[#],index)
        lsGetOverlayConfig(ls[#])
        lsGetOverlayParams(ls[#],index)
        lsGetRigParams(ls[#])
    import/export configuration
        lsExportConfiguration(ls[#],"<exportType>","<fileName>")
        lsImportConfiguration(ls[#],"<importType>","<fileName>")
    LoomIO configuration
        lsSetCameraModule(ls[#],"module","kernelName","kernelArguments")
        lsSetOutputModule(ls[#],"module","kernelName","kernelArguments")
        lsSetOverlayModule(ls[#],"module","kernelName","kernelArguments")
        lsSetViewingModule(ls[#],"module","kernelName","kernelArguments")
        lsGetCameraModule(ls[#])
        lsGetOutputModule(ls[#])
        lsGetOverlayModule(ls[#])
        lsGetViewingModule(ls[#])
    initialize and schedule
        lsInitialize(ls[#])
        lsScheduleFrame(ls[#])
        lsWaitForCompletion(ls[#])
        process ls[#] <num-frames>|live
        process-all <num-frames>|live
    image I/O configuration (not supported with LoomIO)
        lsSetCameraBufferStride(ls[#],stride_in_bytes)
        lsSetOutputBufferStride(ls[#],stride_in_bytes)
        lsSetOverlayBufferStride(ls[#],stride_in_bytes)
        lsSetCameraBuffer(ls[#],buf[#]|NULL)
        lsSetOutputBuffer(ls[#],buf[#]|NULL)
        lsSetOverlayBuffer(ls[#],buf[#]|NULL)
        lsGetCameraBufferStride(ls[#])
        lsGetOutputBufferStride(ls[#])
        lsGetOverlayBufferStride(ls[#])
    OpenCL buffers
        clCreateBuffer(cl[#],<size-in-bytes>) => buf[#]
        clReleaseMemObject(buf[#])
        load-buf buf[#] "fileName.bin"
        save-buf buf[#] "fileName.bin"
        load-bmp buf[#] "fileName.bmp" width height stride_in_bytes
        save-bmp buf[#] "fileName.bmp" width height stride_in_bytes
        load-bmps buf[#] "<fileNameFormat>.bmp" width height num_rows num_columns stride_in_bytes
        save-bmps buf[#] "<fileNameFormat>.bmp" width height num_rows num_columns stride_in_bytes
    OpenCL context (advanced)
        lsGetOpenCLContext(ls[#],cl[#])
        clCreateContext(<platform#>|"<platform-name>",<device#>|"<device-name>") => cl[#]
        clReleaseContext(cl[#])
        lsSetOpenCLContext(ls[#],cl[#])
    OpenVX context (advanced)
        lsGetOpenVXContext(ls[#],vx[#])
        vxCreateContext() => vx[#]
        vxReleaseContext(vx[#])
        lsSetOpenVXContext(ls[#],vx[#])
    attributes (advanced)
        lsGlobalSetAttributes(offset,count,"attr.txt")
        lsGlobalGetAttributes(offset,count,"attr.txt")
        lsGlobalSetAttributes(offset,count,{value(s)})
        lsGlobalGetAttributes(offset,count)
        lsSetAttributes(ls[#],offset,count,"attr.txt")
        lsGetAttributes(ls[#],offset,count,"attr.txt")
        lsSetAttributes(ls[#],offset,count,{value(s)})
        lsGetAttributes(ls[#],offset,count)
