#include "inference.h"
#include "netutil.h"
#include "common.h"
#include <thread>
#include <chrono>

InferenceEngine::InferenceEngine(int sock_, Arguments * args_, std::string clientName_, InfComCommand * cmd)
    : sock{ sock_ }, args{ args_ }, clientName{ clientName_ },
      GPUs{ cmd->data[1] },
      dimInput{ cmd->data[2], cmd->data[3], cmd->data[4] },
      dimOutput{ cmd->data[5], cmd->data[6], cmd->data[7] },
      device_id{ nullptr }, deviceLockSuccess{ false }
{
    // extract model name and options
    char modelName_[128] = { 0 }, options_[128] = { 0 };
    sscanf(cmd->message, "%s%s", modelName_, options_);
    modelName = modelName_;
    options = options_;
    // other initializations
    numInputReceived = 0;
    numInputConsumed = 0;
    numOutputProduced = 0;
    numOutputConsumed = 0;
    // lock devices
    if(!args->lockGpuDevices(GPUs, device_id))
        deviceLockSuccess = true;
    // threads
    threadMasterInputQ = nullptr;
}

InferenceEngine::~InferenceEngine()
{
    // wait for all threads to complete
    if(threadMasterInputQ && threadMasterInputQ->joinable()) {
        threadMasterInputQ->join();
    }

    // release all device resources
    if(deviceLockSuccess) {
        args->releaseGpuDevices(GPUs, device_id);
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
        std::tuple<std::string,int,int,int,int,int,int> info = args->getConfiguredModelInfo(i);
        if(std::get<0>(info) == modelName &&
           std::get<1>(info) == dimInput[0] &&
           std::get<2>(info) == dimInput[1] &&
           std::get<3>(info) == dimInput[2] &&
           std::get<4>(info) == dimOutput[0] &&
           std::get<5>(info) == dimOutput[1] &&
           std::get<6>(info) == dimOutput[2])
        {
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
                found = true;
                break;
            }
        }
    }
    if(found) {
        std::string modulePath = args->getConfigurationDir() + "/" + modelName + "/" + MODULE_LIBNAME;
        struct stat sbuf = { 0 };
        if(stat(modulePath.c_str(), &sbuf) != 0) {
            found = false;
            error("could not locate module %s for %s", modulePath.c_str(), clientName.c_str());
        }
    }
    else {
        error("unable to find requested model:%s input:%dx%dx%d output:%dx%dx%d from %s", modelName.c_str(),
              dimInput[2], dimInput[1], dimInput[0], dimOutput[2], dimOutput[1], dimOutput[0], clientName.c_str());
    }
#if 0 // TODO: need to check before enable
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
#endif

    //////
    /// allocate OpenVX and OpenCL resources
    ///
    // TODO

    //////
    /// load the model
    ///
    // TODO

    //////
    /// start scheduler threads
    ///
    threadMasterInputQ = new std::thread(&InferenceEngine::inputQueueMasterScheduler, this);
    for(int gpu = 0; gpu < GPUs; gpu++) {
        // TODO: start device thread
    }

    ////////
    /// \brief keep running the inference in loop
    ///
    bool endOfImageRequested = false;
    for(;;) {
        bool didSomething = false;

        // send all the available results to the client
        int resultCountAvailable = outputQ.size();
        if(resultCountAvailable == 0) {
            // check for end of processing loop
            if(endOfImageRequested && numInputReceived == numOutputConsumed)
                break;
        }
        else {
            didSomething = true;
            while(resultCountAvailable > 0) {
                int resultCount = std::min(resultCountAvailable, INFCOM_MAX_IMAGES_PER_PACKET);
                InfComCommand cmd = {
                    INFCOM_MAGIC, INFCOM_CMD_INFERENCE_RESULT, { resultCount, 0 }, { 0 }
                };
                for(int i = 0; i < resultCount; i++) {
                    std::tuple<int,int> result;
                    outputQ.dequeue(result);
                    cmd.data[2 + i * 2 + 0] = std::get<0>(result); // tag
                    cmd.data[2 + i * 2 + 1] = std::get<1>(result); // label
                    numOutputConsumed++;
                }
                ERRCHK(sendCommand(sock, cmd, clientName));
                resultCountAvailable -= resultCount;
            }
        }

        // if not endOfImageRequested, request client to send images
        if(!endOfImageRequested) {
            // get number of empty slots in the input queue
            int imageCountRequested = MAX_INPUT_QUEUE_DEPTH - inputQ.size();
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
                    inputQ.enqueue(std::tuple<int,char*,int>(-1,nullptr,0));
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
                    // add (tag,byteStream,size) to input queue
                    numInputReceived++;
                    inputQ.enqueue(std::tuple<int,char*,int>(tag,byteStream,size));
                }
            }
        }

        // if nothing done, wait for sometime
        if(!didSomething && INFERENCE_SERVICE_IDLE_TIME > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(INFERENCE_SERVICE_IDLE_TIME));
        }
    }
    info("runInference: terminated for %s [received %d] [sent %d]", clientName.c_str(), numInputReceived, numOutputConsumed);

    // send and wait for INFCOM_CMD_DONE message
    InfComCommand reply = {
        INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
    };
    ERRCHK(sendCommand(sock, reply, clientName));
    ERRCHK(recvCommand(sock, reply, clientName, INFCOM_CMD_DONE));

    return 0;
}

void InferenceEngine::inputQueueMasterScheduler()
{
    info("inputQueueMasterScheduler: started for %s", clientName.c_str());
    for(;;) {
        // get next item from the input queue and check for endOfImageRequested
        std::tuple<int,char*,int> input;
        inputQ.dequeue(input);
        int tag = std::get<0>(input);
        char * byteStream = std::get<1>(input);
        int size = std::get<2>(input);
        if(tag < 0 || byteStream == nullptr || size == 0)
            break;
        numInputConsumed++;

        // pick a device queue and add to it for further processing
        // TODO
        delete[] byteStream;
        outputQ.enqueue(std::tuple<int,int>(tag,5)); // TODO: this is just a test
        numOutputProduced++;
    }
    info("inputQueueMasterScheduler: terminated for %s [consumed %d] [produced %d]", clientName.c_str(), numInputConsumed, numOutputProduced);
}
