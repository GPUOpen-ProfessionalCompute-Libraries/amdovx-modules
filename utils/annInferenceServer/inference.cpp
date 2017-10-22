#include "inference.h"
#include "netutil.h"
#include "common.h"
#include <thread>
#include <chrono>
#include <dlfcn.h>
#include <opencv2/opencv.hpp>
#include <highgui.h>

InferenceEngine::InferenceEngine(int sock_, Arguments * args_, std::string clientName_, InfComCommand * cmd)
    : sock{ sock_ }, args{ args_ }, clientName{ clientName_ },
      GPUs{ cmd->data[1] },
      dimInput{ cmd->data[2], cmd->data[3], cmd->data[4] },
      dimOutput{ cmd->data[5], cmd->data[6], cmd->data[7] },
      moduleHandle{ nullptr }, annCreateGraph{ nullptr },
      device_id{ nullptr }, deviceLockSuccess{ false }
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
    , threadMasterInputQ{ nullptr },
      opencl_context{ nullptr }, opencl_cmdq{ nullptr },
      openvx_context{ nullptr }, openvx_graph{ nullptr }, openvx_input{ nullptr }, openvx_output{ nullptr },
      threadDeviceInputCopy{ nullptr }, threadDeviceProcess{ nullptr }, threadDeviceOutputCopy{ nullptr },
      queueDeviceTagQ{ nullptr }, queueDeviceImageQ{ nullptr },
      queueDeviceInputMemIdle{ nullptr }, queueDeviceInputMemBusy{ nullptr },
      queueDeviceOutputMemIdle{ nullptr }, queueDeviceOutputMemBusy{ nullptr }
#endif
{
    // extract model name, options, and module path
    char modelName_[128] = { 0 }, options_[128] = { 0 };
    sscanf(cmd->message, "%s%s", modelName_, options_);
    modelName = modelName_;
    options = options_;
    // configuration
    batchSize = args->getBatchSize();
    inputSizeInBytes = 4 * dimInput[0] * dimInput[1] * dimInput[2] * batchSize;
    outputSizeInBytes = 4 * dimOutput[0] * dimOutput[1] * dimOutput[2] * batchSize;
    // lock devices
    if(!args->lockGpuDevices(GPUs, device_id))
        deviceLockSuccess = true;
}

InferenceEngine::~InferenceEngine()
{
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
    // wait for all threads to complete and release all resources
    std::tuple<int,char*,int> endOfSequenceInput(-1,nullptr,0);
    inputQ.enqueue(endOfSequenceInput);
    if(threadMasterInputQ && threadMasterInputQ->joinable()) {
        threadMasterInputQ->join();
    }
    std::tuple<char*,int> endOfSequenceImage(nullptr,0);
    int endOfSequenceTag = -1;
    for(int i = 0; i < GPUs; i++) {
        if(queueDeviceTagQ[i]) {
            queueDeviceTagQ[i]->enqueue(endOfSequenceTag);
        }
        if(queueDeviceImageQ[i]) {
            queueDeviceImageQ[i]->enqueue(endOfSequenceImage);
        }
        if(threadDeviceInputCopy[i] && threadDeviceInputCopy[i]->joinable()) {
            threadDeviceInputCopy[i]->join();
        }
        if(threadDeviceProcess[i] && threadDeviceProcess[i]->joinable()) {
            threadDeviceProcess[i]->join();
        }
        if(threadDeviceOutputCopy[i] && threadDeviceOutputCopy[i]->joinable()) {
            threadDeviceOutputCopy[i]->join();
        }
        while(queueDeviceInputMemIdle[i] && queueDeviceInputMemIdle[i]->size() > 0) {
            cl_mem mem;
            queueDeviceInputMemIdle[i]->dequeue(mem);
            clReleaseMemObject(mem);
        }
        while(queueDeviceOutputMemIdle[i] && queueDeviceOutputMemIdle[i]->size() > 0) {
            cl_mem mem;
            queueDeviceOutputMemIdle[i]->dequeue(mem);
            clReleaseMemObject(mem);
        }
        if(queueDeviceTagQ[i]) {
            delete queueDeviceTagQ[i];
        }
        if(queueDeviceImageQ[i]) {
            delete queueDeviceImageQ[i];
        }
        if(queueDeviceInputMemIdle[i]) {
            delete queueDeviceInputMemIdle[i];
        }
        if(queueDeviceInputMemBusy[i]) {
            delete queueDeviceInputMemBusy[i];
        }
        if(queueDeviceOutputMemIdle[i]) {
            delete queueDeviceOutputMemIdle[i];
        }
        if(queueDeviceOutputMemBusy[i]) {
            delete queueDeviceOutputMemBusy[i];
        }
        if(openvx_graph[i]) {
            vxReleaseGraph(&openvx_graph[i]);
        }
        if(openvx_input[i]) {
            vxReleaseTensor(&openvx_input[i]);
        }
        if(openvx_output[i]) {
            vxReleaseTensor(&openvx_output[i]);
        }
        if(openvx_context[i]) {
            vxReleaseContext(&openvx_context[i]);
        }
        if(opencl_cmdq[i]) {
            clReleaseCommandQueue(opencl_cmdq[i]);
        }
        if(opencl_context[i]) {
            clReleaseContext(opencl_context[i]);
        }
    }
#endif

    // release all device resources
    if(deviceLockSuccess) {
        args->releaseGpuDevices(GPUs, device_id);
    }
    if(moduleHandle) {
        dlclose(moduleHandle);
    }
}

int InferenceEngine::run()
{
    //////
    /// make device lock is successful
    ///
    if(!deviceLockSuccess) {
        return error_close(sock, "could not lock %d GPUs devices for inference request from %s", GPUs, clientName.c_str());
    }

    //////
    /// check for model validity
    ///
    bool found = false;
    for(size_t i = 0; i < args->getNumConfigureddModels(); i++) {
        std::tuple<std::string,int,int,int,int,int,int,std::string> info = args->getConfiguredModelInfo(i);
        if(std::get<0>(info) == modelName &&
           std::get<1>(info) == dimInput[0] &&
           std::get<2>(info) == dimInput[1] &&
           std::get<3>(info) == dimInput[2] &&
           std::get<4>(info) == dimOutput[0] &&
           std::get<5>(info) == dimOutput[1] &&
           std::get<6>(info) == dimOutput[2])
        {
            modelPath = args->getConfigurationDir() + "/" + std::get<7>(info);
            found = true;
            break;
        }
    }
    if(!found) {
        for(size_t i = 0; i < args->getNumUploadedModels(); i++) {
            std::tuple<std::string,int,int,int,int,int,int> info = args->getUploadedModelInfo(i);
            if(std::get<0>(info) == modelName &&
               std::get<1>(info) == dimInput[0] &&
               std::get<2>(info) == dimInput[1] &&
               std::get<3>(info) == dimInput[2] &&
               std::get<4>(info) == dimOutput[0] &&
               std::get<5>(info) == dimOutput[1] &&
               std::get<6>(info) == dimOutput[2])
            {
                modelPath = args->getConfigurationDir() + "/" + modelName;
                found = true;
                break;
            }
        }
    }
    if(found) {
        modulePath = modelPath + "/build/" + MODULE_LIBNAME;
        moduleHandle = dlopen(modulePath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if(!moduleHandle) {
            found = false;
            error("could not locate module %s for %s", modulePath.c_str(), clientName.c_str());
        }
        else if(!(annCreateGraph = (type_annCreateGraph *) dlsym(moduleHandle, "annCreateGraph"))) {
            found = false;
            error("could not find function annCreateGraph() in module %s for %s", modulePath.c_str(), clientName.c_str());
        }
    }
    else {
        error("unable to find requested model:%s input:%dx%dx%d output:%dx%dx%d from %s", modelName.c_str(),
              dimInput[2], dimInput[1], dimInput[0], dimOutput[2], dimOutput[1], dimOutput[0], clientName.c_str());
    }
    if(!found) {
        // send and wait for INFCOM_CMD_DONE message
        InfComCommand reply = {
            INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
        };
        ERRCHK(sendCommand(sock, reply, clientName));
        ERRCHK(recvCommand(sock, reply, clientName, INFCOM_CMD_DONE));
        close(sock);
        return -1;
    }
    info("found requested model:%s input:%dx%dx%d output:%dx%dx%d from %s", modelName.c_str(),
          dimInput[2], dimInput[1], dimInput[0], dimOutput[2], dimOutput[1], dimOutput[0], clientName.c_str());

    // send and wait for INFCOM_CMD_INFERENCE_INITIALIZATION message
    InfComCommand updateCmd = {
        INFCOM_MAGIC, INFCOM_CMD_INFERENCE_INITIALIZATION, { 0 }, "started initialization"
    };
    ERRCHK(sendCommand(sock, updateCmd, clientName));
    ERRCHK(recvCommand(sock, updateCmd, clientName, INFCOM_CMD_INFERENCE_INITIALIZATION));
    info(updateCmd.message);

#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
    info("InferenceEngine: using SIMPLE_INFERENCE_SCHEDULER");
    //////
    /// allocate OpenVX and OpenCL resources
    ///
    for(int gpu = 0; gpu < GPUs; gpu++) {
        //////
        // create OpenCL context
        cl_context_properties ctxprop[] = {
            CL_CONTEXT_PLATFORM, (cl_context_properties)args->getPlatformId(),
            0, 0
        };
        cl_int err;
        opencl_context[gpu] = clCreateContext(ctxprop, 1, &device_id[gpu], NULL, NULL, &err);
        if(err)
            fatal("InferenceEngine: clCreateContext(#%d) failed (%d)", gpu, err);
#if defined(CL_VERSION_2_0)
        cl_queue_properties properties[] = { CL_QUEUE_PROPERTIES, 0, 0, 0 };
        opencl_cmdq[gpu] = clCreateCommandQueueWithProperties(opencl_context[gpu], device_id[gpu], properties, &err);
#else
        opencl_cmdq[gpu] = clCreateCommandQueue(opencl_context[gpu], device_id[gpu], 0, &err);
#endif
        if(err) {
            fatal("InferenceEngine: clCreateCommandQueue(device_id[%d]) failed (%d)", gpu, err);
        }

        //////
        // create OpenVX context
        vx_status status;
        openvx_context[gpu] = vxCreateContext();
        if((status = vxGetStatus((vx_reference)openvx_context[gpu])) != VX_SUCCESS)
            fatal("InferenceEngine: vxCreateContext(#%d) failed (%d)", gpu, status);
        if((status = vxSetContextAttribute(openvx_context[gpu], VX_CONTEXT_ATTRIBUTE_AMD_OPENCL_CONTEXT,
                                          &opencl_context[gpu], sizeof(cl_context))) != VX_SUCCESS)
            fatal("InferenceEngine: vxSetContextAttribute(#%d,VX_CONTEXT_ATTRIBUTE_AMD_OPENCL_CONTEXT) failed (%d)", gpu, status);
        vx_size idim[4] = { (vx_size)dimInput[0], (vx_size)dimInput[1], (vx_size)dimInput[2], (vx_size)batchSize };
        vx_size odim[4] = { (vx_size)dimOutput[0], (vx_size)dimOutput[1], (vx_size)dimOutput[2], (vx_size)batchSize };
#if USE_VX_TENSOR_WITHOUT_CL
        openvx_input[gpu] = vxCreateTensor(openvx_context[gpu], 4, idim, VX_TYPE_FLOAT32, 0);
        openvx_output[gpu] = vxCreateTensor(openvx_context[gpu], 4, odim, VX_TYPE_FLOAT32, 0);
#else
        vx_size istride[4] = { 4, (vx_size)4 * dimInput[0], (vx_size)4 * dimInput[0] * dimInput[1], (vx_size)4 * dimInput[0] * dimInput[1] * dimInput[2] };
        vx_size ostride[4] = { 4, (vx_size)4 * dimOutput[0], (vx_size)4 * dimOutput[0] * dimOutput[1], (vx_size)4 * dimOutput[0] * dimOutput[1] * dimOutput[2] };
        openvx_input[gpu] = vxCreateTensorFromHandle(openvx_context[gpu], 4, idim, VX_TYPE_FLOAT32, 0, istride, nullptr, VX_MEMORY_TYPE_OPENCL);
        openvx_output[gpu] = vxCreateTensorFromHandle(openvx_context[gpu], 4, odim, VX_TYPE_FLOAT32, 0, ostride, nullptr, VX_MEMORY_TYPE_OPENCL);
#endif
        if((status = vxGetStatus((vx_reference)openvx_input[gpu])) != VX_SUCCESS)
            fatal("InferenceEngine: vxCreateTensorFromHandle(input#%d) failed (%d)", gpu, status);
        if((status = vxGetStatus((vx_reference)openvx_output[gpu])) != VX_SUCCESS)
            fatal("InferenceEngine: vxCreateTensorFromHandle(output#%d) failed (%d)", gpu, status);
        //////
        // load the model
        openvx_graph[gpu] = annCreateGraph(openvx_context[gpu], openvx_input[gpu], openvx_output[gpu], modelPath.c_str());
        if((status = vxGetStatus((vx_reference)openvx_graph[gpu])) != VX_SUCCESS)
            fatal("InferenceEngine: annCreateGraph(#%d) failed (%d)", gpu, status);

        // send and wait for INFCOM_CMD_INFERENCE_INITIALIZATION message
        updateCmd.data[0] = 80 * (gpu + 1) / GPUs;
        sprintf(updateCmd.message, "completed OpenVX graph for GPU#%d", gpu);
        ERRCHK(sendCommand(sock, updateCmd, clientName));
        ERRCHK(recvCommand(sock, updateCmd, clientName, INFCOM_CMD_INFERENCE_INITIALIZATION));
        info(updateCmd.message);
    }

    //////
    /// start scheduler queues and allocate OpenCL buffers for input and output
    ///
    for(int gpu = 0; gpu < GPUs; gpu++) {
        // create device queues
        queueDeviceTagQ[gpu] = new MessageQueue<int>();
        queueDeviceImageQ[gpu] = new MessageQueue<std::tuple<char*,int>>();
        queueDeviceInputMemIdle[gpu] = new MessageQueue<cl_mem>();
        queueDeviceInputMemBusy[gpu] = new MessageQueue<cl_mem>();
        queueDeviceOutputMemIdle[gpu] = new MessageQueue<cl_mem>();
        queueDeviceOutputMemBusy[gpu] = new MessageQueue<cl_mem>();
        // create OpenCL buffers for input/output and add them to queueDeviceInputMemIdle/queueDeviceOutputMemIdle
        for(int i = 0; i < INFERENCE_PIPE_QUEUE_DEPTH; i++) {
            cl_mem mem;
            cl_int err;
            mem = clCreateBuffer(opencl_context[gpu], CL_MEM_READ_WRITE, inputSizeInBytes, NULL, &err);
            if(err) {
                fatal("workDeviceInputCopy: clCreateBuffer(#%d,%d) failed (%d)", gpu, inputSizeInBytes, err);
            }
            queueDeviceInputMemIdle[gpu]->enqueue(mem);
            mem = clCreateBuffer(opencl_context[gpu], CL_MEM_READ_WRITE, outputSizeInBytes, NULL, &err);
            if(err) {
                fatal("workDeviceInputCopy: clCreateBuffer(#%d,%d) failed (%d)", gpu, outputSizeInBytes, err);
            }
            queueDeviceOutputMemIdle[gpu]->enqueue(mem);
        }
    }
#else
    info("InferenceEngine: using NO_INFERENCE_SCHEDULER");
#endif

    //////
    /// start scheduler threads
    ///
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
    threadMasterInputQ = new std::thread(&InferenceEngine::workMasterInputQ, this);
    for(int gpu = 0; gpu < GPUs; gpu++) {
        threadDeviceInputCopy[gpu] = new std::thread(&InferenceEngine::workDeviceInputCopy, this, gpu);
        threadDeviceProcess[gpu] = new std::thread(&InferenceEngine::workDeviceProcess, this, gpu);
        threadDeviceOutputCopy[gpu] = new std::thread(&InferenceEngine::workDeviceOutputCopy, this, gpu);
    }
#endif

    // send and wait for INFCOM_CMD_INFERENCE_INITIALIZATION message
    updateCmd.data[0] = 100;
    sprintf(updateCmd.message, "inference engine is ready");
    ERRCHK(sendCommand(sock, updateCmd, clientName));
    ERRCHK(recvCommand(sock, updateCmd, clientName, INFCOM_CMD_INFERENCE_INITIALIZATION));
    info(updateCmd.message);

    ////////
    /// \brief keep running the inference in loop
    ///
    bool endOfImageRequested = false;
    for(bool endOfSequence = false; !endOfSequence; ) {
        bool didSomething = false;

        // send all the available results to the client
        int resultCountAvailable = outputQ.size();
        if(resultCountAvailable > 0) {
            didSomething = true;
            while(resultCountAvailable > 0) {
                int resultCount = std::min(resultCountAvailable, INFCOM_MAX_IMAGES_PER_PACKET);
                InfComCommand cmd = {
                    INFCOM_MAGIC, INFCOM_CMD_INFERENCE_RESULT, { resultCount, 0 }, { 0 }
                };
                for(int i = 0; i < resultCount; i++) {
                    std::tuple<int,int> result;
                    outputQ.dequeue(result);
                    int tag = std::get<0>(result);
                    int label = std::get<1>(result);
                    if(tag < 0) {
                        endOfSequence = true;
                        resultCount = i;
                        break;
                    }
                    cmd.data[2 + i * 2 + 0] = tag; // tag
                    cmd.data[2 + i * 2 + 1] = label; // label
                }
                if(resultCount > 0) {
                    cmd.data[0] = resultCount;
                    ERRCHK(sendCommand(sock, cmd, clientName));
                    resultCountAvailable -= resultCount;
                    ERRCHK(recvCommand(sock, cmd, clientName, INFCOM_CMD_INFERENCE_RESULT));
                }
                if(endOfSequence) {
                    break;
                }
            }
        }

        // if not endOfImageRequested, request client to send images
        if(!endOfImageRequested) {
            // get number of empty slots in the input queue
            int imageCountRequested = 0;
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
            imageCountRequested = MAX_INPUT_QUEUE_DEPTH - inputQ.size();
#else
            imageCountRequested = 1;
#endif
            if(imageCountRequested > 0) {
                didSomething = true;
                // send request for upto INFCOM_MAX_IMAGES_PER_PACKET images
                imageCountRequested = std::min(imageCountRequested, INFCOM_MAX_IMAGES_PER_PACKET);
                InfComCommand cmd = {
                    INFCOM_MAGIC, INFCOM_CMD_SEND_IMAGES, { imageCountRequested }, { 0 }
                };
                ERRCHK(sendCommand(sock, cmd, clientName));
                ERRCHK(recvCommand(sock, cmd, clientName, INFCOM_CMD_SEND_IMAGES));

                // check of endOfImageRequested and receive images one at a time
                int imageCountReceived = cmd.data[0];
                if(imageCountReceived < 0) {
                    // submit the endOfSequence indicator to scheduler
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
                    inputQ.enqueue(std::tuple<int,char*,int>(-1,nullptr,0));
#else
                    endOfSequence = true;
#endif
                    endOfImageRequested = true;
                }
                int i = 0;
                for(; i < imageCountReceived; i++) {
                    // get header with tag and size info
                    int header[2] = { 0, 0 };
                    ERRCHK(recvPacket(sock, &header, sizeof(header), clientName));
                    int tag = header[0];
                    int size = header[1];
                    // do sanity check with unreasonable parameters
                    if(tag < 0 || size <= 0 || size > 50000000) {
                        return error_close(sock, "invalid (tag:%d,size:%d) from %s", tag, size, clientName.c_str());
                    }
                    // allocate and receive the image and EOF market
                    char * byteStream = new char [size];
                    ERRCHK(recvBuffer(sock, byteStream, size, clientName));
                    int eofMarker = 0;
                    ERRCHK(recvPacket(sock, &eofMarker, sizeof(eofMarker), clientName));
                    if(eofMarker != INFCOM_EOF_MARKER) {
                        return error_close(sock, "eofMarker 0x%08x (incorrect)", eofMarker);
                    }
                    // submit the input (tag,byteStream,size) to scheduler
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
                    inputQ.enqueue(std::tuple<int,char*,int>(tag,byteStream,size));
#else
                    // dummy processing that takes 4ms/images
                    int label = tag % 10;
                    delete[] byteStream;
                    std::this_thread::sleep_for(std::chrono::milliseconds(4));
                    outputQ.enqueue(std::tuple<int,int>(tag,label));
#endif
                }
            }
        }

        // if nothing done, wait for sometime
        if(!didSomething && INFERENCE_SERVICE_IDLE_TIME > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(INFERENCE_SERVICE_IDLE_TIME));
        }
    }
    info("runInference: terminated for %s", clientName.c_str());

    // send and wait for INFCOM_CMD_DONE message
    InfComCommand reply = {
        INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
    };
    ERRCHK(sendCommand(sock, reply, clientName));
    ERRCHK(recvCommand(sock, reply, clientName, INFCOM_CMD_DONE));

    return 0;
}

#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
void InferenceEngine::workMasterInputQ()
{
    args->lock();
    info("workMasterInputQ: started for %s", clientName.c_str());
    args->unlock();

    int batchSize = args->getBatchSize();
    int totalInputCount = 0;
    int inputCountInBatch = 0, gpu = 0;
    for(;;) {
        // get next item from the input queue
        std::tuple<int,char*,int> input;
        inputQ.dequeue(input);
        int tag = std::get<0>(input);
        char * byteStream = std::get<1>(input);
        int size = std::get<2>(input);

        // check for end of input
        if(tag < 0 || byteStream == nullptr || size == 0)
            break;
        totalInputCount++;

        // add the image to selected deviceQ
        std::tuple<char*,int> image(byteStream,size);
        queueDeviceTagQ[gpu]->enqueue(tag);
        queueDeviceImageQ[gpu]->enqueue(image);

        // at the end of Batch pick another device
        inputCountInBatch++;
        if(inputCountInBatch == batchSize) {
            inputCountInBatch = 0;
            gpu = (gpu + 1) % GPUs;
            for(int i = 0; i < GPUs; i++) {
                if(i != gpu && queueDeviceTagQ[i]->size() < queueDeviceTagQ[gpu]->size()) {
                    gpu = i;
                }
            }
        }
    }

    // send endOfSequence indicator to all scheduler threads
    for(int i = 0; i < GPUs; i++) {
        int endOfSequenceTag = -1;
        std::tuple<char*,int> endOfSequenceImage(nullptr,0);
        queueDeviceTagQ[gpu]->enqueue(endOfSequenceTag);
        queueDeviceImageQ[gpu]->enqueue(endOfSequenceImage);
    }

    args->lock();
    info("workMasterInputQ: terminated for %s [scheduled %d images]", clientName.c_str(), totalInputCount);
    args->unlock();
}

void InferenceEngine::workDeviceInputCopy(int gpu)
{
    args->lock();
    info("workDeviceInputCopy: GPU#%d started for %s", gpu, clientName.c_str());
    args->unlock();

    // create OpenCL command-queue
    cl_command_queue cmdq;
    cl_int err;
#if defined(CL_VERSION_2_0)
    cl_queue_properties properties[] = { CL_QUEUE_PROPERTIES, 0, 0 };
    cmdq = clCreateCommandQueueWithProperties(opencl_context[gpu], device_id[gpu], properties, &err);
#else
    cmdq = clCreateCommandQueue(opencl_context[gpu], device_id[gpu], 0, &err);
#endif
    if(err) {
        fatal("workDeviceInputCopy: clCreateCommandQueue(device_id[%d]) failed (%d)", gpu, err);
    }

    int totalBatchCounter = 0, totalImageCounter = 0;
    for(bool endOfSequenceReached = false; !endOfSequenceReached; ) {
        // get an empty OpenCL buffer and lock the buffer for writing
        cl_mem mem = nullptr;
        queueDeviceInputMemIdle[gpu]->dequeue(mem);
        if(mem == nullptr) {
            fatal("workDeviceInputCopy: unexpected nullptr in queueDeviceInputMemIdle[%d]", gpu);
        }
        cl_int err;
        float * mapped_ptr = (float *)clEnqueueMapBuffer(cmdq, mem, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, inputSizeInBytes, 0, NULL, NULL, &err);
        if(err) {
            fatal("workDeviceInputCopy: clEnqueueMapBuffer(#%d) failed (%d)", gpu, err);
        }

        // get next batch of inputs and convert them into tensor and release input byteStream
        // TODO: replace with an efficient implementation
        int inputCount = 0;
        for(; inputCount < batchSize; inputCount++) {
            // get next item from the input queue and check for end of input
            std::tuple<char*,int> image;
            queueDeviceImageQ[gpu]->dequeue(image);
            char * byteStream = std::get<0>(image);
            int size = std::get<1>(image);
            if(byteStream == nullptr || size == 0) {
                endOfSequenceReached = true;
                break;
            }

            // decode, scale, and format convert into the OpenCL buffer
            float * buf = mapped_ptr + dimInput[0] * dimInput[1] * dimInput[2] * inputCount;
            cv::Mat matOrig = cv::imdecode(cv::Mat(1, size, CV_8UC1, byteStream), CV_LOAD_IMAGE_UNCHANGED);
            cv::Mat matScaled;
            cv::resize(matOrig, matScaled, cv::Size(dimInput[0], dimInput[1]));
            for(int c = 0; c < 3; c++) {
                unsigned char * img = matScaled.data + 2 - c;
                int n = dimInput[0] * dimInput[1];
                for(int i = 0; i < n; i++, img += 3) {
                    buf[i] = *img;
                }
            }

            // release byteStream
            delete[] byteStream;
        }

        // unlock the OpenCL buffer to perform the writing
        err = clEnqueueUnmapMemObject(cmdq, mem, mapped_ptr, 0, NULL, NULL);
        if(err) {
            fatal("workDeviceInputCopy: clEnqueueMapBuffer(#%d) failed (%d)", gpu, err);
        }
        err = clFinish(cmdq);
        if(err) {
            fatal("workDeviceInputCopy: clFinish(#%d) failed (%d)", gpu, err);
        }

        if(inputCount > 0) {
            // add the input for processing
            queueDeviceInputMemBusy[gpu]->enqueue(mem);
            // update counters
            totalBatchCounter++;
            totalImageCounter += inputCount;
        }
        else {
            // add the input back to idle queue
            queueDeviceInputMemIdle[gpu]->enqueue(mem);
        }

    }

    // release OpenCL command queue
    clReleaseCommandQueue(cmdq);

    // add the endOfSequenceMarker to next stage
    cl_mem endOfSequenceMarker = nullptr;
    queueDeviceInputMemBusy[gpu]->enqueue(endOfSequenceMarker);

    args->lock();
    info("workDeviceInputCopy: GPU#%d terminated for %s [processed %d batches, %d images]", gpu, clientName.c_str(), totalBatchCounter, totalImageCounter);
    args->unlock();
}

void InferenceEngine::workDeviceProcess(int gpu)
{
    args->lock();
    info("workDeviceProcess: GPU#%d started for %s", gpu, clientName.c_str());
    args->unlock();

    int processCounter = 0;
    for(;;) {
        // get a busy OpenCL buffer for input and check for end of sequence marker
        cl_mem input = nullptr;
        queueDeviceInputMemBusy[gpu]->dequeue(input);
        if(!input) {
            break;
        }

        // get an empty OpenCL buffer for output and a busy OpenCL buffer for input
        cl_mem output = nullptr;
        queueDeviceOutputMemIdle[gpu]->dequeue(output);
        if(input == nullptr) {
            fatal("workDeviceProcess: unexpected nullptr in queueDeviceOutputMemIdle[%d]", gpu);
        }

        // process the graph
        vx_status status;
#if USE_VX_TENSOR_WITHOUT_CL
        int err;
        vx_map_id map_id;
        vx_size stride[4];
        void * ptr = nullptr;
        status = vxMapTensorPatch(openvx_input[gpu], 4, NULL, NULL, &map_id, stride, &ptr, VX_WRITE_ONLY, VX_MEMORY_TYPE_OPENCL, 0);
        err = clEnqueueReadBuffer(opencl_cmdq[gpu], input, 0, 0, stride[3] * batchSize, ptr, 0, NULL, NULL);
        if(err) fatal("clEnqueueReadBuffer(#%d,%d) failed (%d)", gpu, stride[3] * batchSize, err);
        err = clFinish(opencl_cmdq[gpu]);
        if(err) fatal("clFinish(#%d,%d) for read failed (%d)", gpu, err);
        if(!status) vxUnmapTensorPatch(openvx_input[gpu], map_id);
#else
        status = vxSwapTensorHandle(openvx_input[gpu], input, nullptr);
        if(status != VX_SUCCESS) {
            fatal("workDeviceProcess: vxSwapTensorHandle(input#%d) failed(%d)", gpu, status);
        }
        status = vxSwapTensorHandle(openvx_output[gpu], output, nullptr);
        if(status != VX_SUCCESS) {
            fatal("workDeviceProcess: vxSwapTensorHandle(output#%d) failed(%d)", gpu, status);
        }
#endif
#if DEBUG_DUMP // TODO remove
        {{{{static bool onlyonce = true; if(onlyonce) {
            onlyonce = false;
            dumpBuffer(opencl_cmdq[gpu], input, "input1.f32");
            dumpBuffer(opencl_cmdq[gpu], output, "output1.f32");
        }}}}}
#endif
        status = vxProcessGraph(openvx_graph[gpu]);
        if(status != VX_SUCCESS) {
            fatal("workDeviceProcess: vxProcessGraph(#%d) failed(%d)", gpu, status);
        }
#if USE_VX_TENSOR_WITHOUT_CL
        ptr = nullptr;
        status = vxMapTensorPatch(openvx_output[gpu], 4, NULL, NULL, &map_id, stride, &ptr, VX_READ_ONLY, VX_MEMORY_TYPE_OPENCL, 0);
        err = clEnqueueWriteBuffer(opencl_cmdq[gpu], output, 0, 0, stride[3] * batchSize, ptr, 0, NULL, NULL);
        if(err) fatal("clEnqueueWriteBuffer(#%d,%d) failed (%d)", gpu, stride[3] * batchSize, err);
        err = clFinish(opencl_cmdq[gpu]);
        if(err) fatal("clFinish(#%d,%d) for write failed (%d)", gpu, err);
        if(!status) vxUnmapTensorPatch(openvx_output[gpu], map_id);
#endif
#if DEBUG_DUMP // TODO remove
        {{{{static bool onlyonce = true; if(onlyonce) {
            onlyonce = false;
            dumpBuffer(opencl_cmdq[gpu], input, "input2.f32");
            dumpBuffer(opencl_cmdq[gpu], output, "output2.f32");
        }}}}}
#endif

        // add the input for idle queue and output to busy queue
        queueDeviceInputMemIdle[gpu]->enqueue(input);
        queueDeviceOutputMemBusy[gpu]->enqueue(output);
        processCounter++;
    }

    // add the endOfSequenceMarker to next stage
    cl_mem endOfSequenceMarker = nullptr;
    queueDeviceOutputMemBusy[gpu]->enqueue(endOfSequenceMarker);

    args->lock();
    info("workDeviceProcess: GPU#%d terminated for %s [processed %d batches]", gpu, clientName.c_str(), processCounter);
    args->unlock();
}

void InferenceEngine::workDeviceOutputCopy(int gpu)
{
    args->lock();
    info("workDeviceOutputCopy: GPU#%d started for %s", gpu, clientName.c_str());
    args->unlock();

    // create OpenCL command-queue
    cl_command_queue cmdq;
    cl_int err;
#if defined(CL_VERSION_2_0)
    cl_queue_properties properties[] = { CL_QUEUE_PROPERTIES, 0, 0 };
    cmdq = clCreateCommandQueueWithProperties(opencl_context[gpu], device_id[gpu], properties, &err);
#else
    cmdq = clCreateCommandQueue(opencl_context[gpu], device_id[gpu], 0, &err);
#endif
    if(err) {
        fatal("workDeviceOutputCopy: clCreateCommandQueue(device_id[%d]) failed (%d)", gpu, err);
    }

    int totalBatchCounter = 0, totalImageCounter = 0;
    for(bool endOfSequenceReached = false; !endOfSequenceReached; ) {
        // get an output OpenCL buffer and lock the buffer for reading
        cl_mem mem = nullptr;
        queueDeviceOutputMemBusy[gpu]->dequeue(mem);
        if(mem == nullptr) {
            break;
        }
        cl_int err;
        float * mapped_ptr = (float *)clEnqueueMapBuffer(cmdq, mem, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, outputSizeInBytes, 0, NULL, NULL, &err);
        if(err) {
            fatal("workDeviceOutputCopy: clEnqueueMapBuffer(#%d) failed (%d)", gpu, err);
        }

        // get next batch of inputs
        int outputCount = 0;
        for(; outputCount < batchSize; outputCount++) {
            // get next item from the tag queue and check for end of input
            int tag;
            queueDeviceTagQ[gpu]->dequeue(tag);
            if(tag < 0) {
                endOfSequenceReached = true;
                break;
            }

            // decode, scale, and format convert into the OpenCL buffer
            float * buf = mapped_ptr + dimOutput[0] * dimOutput[1] * dimOutput[2] * outputCount;
            int label = 0;
            float max_prob = buf[0];
            for(int c = 1; c < dimOutput[2]; c++) {
                float prob = buf[c];
                if(prob > max_prob) {
                    label = c;
                    max_prob = prob;
                }
            }
            outputQ.enqueue(std::tuple<int,int>(tag,label));
        }

        // unlock the OpenCL buffer to perform the writing
        err = clEnqueueUnmapMemObject(cmdq, mem, mapped_ptr, 0, NULL, NULL);
        if(err) {
            fatal("workDeviceOutputCopy: clEnqueueMapBuffer(#%d) failed (%d)", gpu, err);
        }
        err = clFinish(cmdq);
        if(err) {
            fatal("workDeviceOutputCopy: clFinish(#%d) failed (%d)", gpu, err);
        }

        // add the output back to idle queue
        queueDeviceOutputMemIdle[gpu]->enqueue(mem);

        // update counter
        if(outputCount > 0) {
            totalBatchCounter++;
            totalImageCounter += outputCount;
        }
    }

    // release OpenCL command queue
    clReleaseCommandQueue(cmdq);

    // send end of sequence marker to next stage
    outputQ.enqueue(std::tuple<int,int>(-1,-1));

    args->lock();
    info("workDeviceOutputCopy: GPU#%d terminated for %s [processed %d batches, %d images]", gpu, clientName.c_str(), totalBatchCounter, totalImageCounter);
    args->unlock();
}
#endif

void InferenceEngine::dumpBuffer(cl_command_queue cmdq, cl_mem mem, std::string fileName)
{
    cl_int err;
    size_t size = 0;
    err = clGetMemObjectInfo(mem, CL_MEM_SIZE, sizeof(size), &size, NULL);
    if(err) return;
    unsigned char * ptr = (unsigned char *)clEnqueueMapBuffer(cmdq, mem, CL_TRUE, CL_MAP_READ, 0, size, 0, NULL, NULL, &err);
    if(err) return;
    err = clFinish(cmdq);
    if(err) return;
    FILE * fp = fopen(fileName.c_str(), "wb");
    if(err) return;
    fwrite(ptr, 1, size, fp);
    fclose(fp);
    err = clEnqueueUnmapMemObject(cmdq, mem, ptr, 0, NULL, NULL);
    if(err) return;
    err = clFinish(cmdq);
    if(err) return;
    printf("OK: dumped %ld bytes into %s\n", size, fileName.c_str());
}
