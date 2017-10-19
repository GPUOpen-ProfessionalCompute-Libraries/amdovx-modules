#ifndef INFERENCE_H
#define INFERENCE_H

#include "arguments.h"
#include "infcom.h"
#include <string>
#include <tuple>
#include <queue>
#include <vector>
#include <mutex>
#include <thread>
#include <VX/vx.h>
#include <vx_ext_amd.h>

#define INFERENCE_PIPE_QUEUE_DEPTH     5  // inference pipe queue depth
#define MAX_INPUT_QUEUE_DEPTH       1024  // number of images
#define INFERENCE_SERVICE_IDLE_TIME 1000  // milliseconds

extern "C" {
    typedef VX_API_ENTRY vx_graph VX_API_CALL type_annCreateGraph(
            vx_context context,
            vx_tensor input,
            vx_tensor output,
            const char * options
        );
};

template<typename T>
class MessageQueue {
public:
    MessageQueue() : enqueueCount{ 0 }, dequeueCount{ 0 } {
    }
    size_t size() {
        return enqueueCount - dequeueCount;
    }
    void enqueue(T const& value) {
        mutex.lock();
        queue.push(value);
        enqueueCount++;
        mutex.unlock();
        signal.notify_one();
    }
    void dequeue(T& value) {
        std::unique_lock<std::mutex> lock(mutex);
        while(queue.empty()) {
            signal.wait(lock);
        }
        value = queue.front();
        queue.pop();
        dequeueCount++;
    }

private:
    int enqueueCount;
    int dequeueCount;
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable signal;
};

class InferenceEngine {
public:
    InferenceEngine(int sock, Arguments * args, std::string clientName, InfComCommand * cmd);
    ~InferenceEngine();
    int run();

protected:
    void workMasterInputQ();
    void workDeviceInputCopy(int gpu);
    void workDeviceProcess(int gpu);
    void workDeviceOutputCopy(int gpu);

 private:
    // configuration
    int sock;
    Arguments * args;
    std::string modelName;
    std::string options;
    int GPUs;
    int dimInput[3];
    int dimOutput[3];
    std::string clientName;
    std::string modelPath;
    std::string modulePath;
    void * moduleHandle;
    type_annCreateGraph * annCreateGraph;
    cl_device_id device_id[MAX_NUM_GPU];
    cl_context opencl_context[MAX_NUM_GPU];
    vx_context openvx_context[MAX_NUM_GPU];
    vx_graph openvx_graph[MAX_NUM_GPU];
    vx_tensor openvx_input[MAX_NUM_GPU];
    vx_tensor openvx_output[MAX_NUM_GPU];
    int batchSize;
    int inputSizeInBytes;
    int outputSizeInBytes;
    bool deviceLockSuccess;
    // threads
    std::thread * threadMasterInputQ;
    std::thread * threadDeviceInputCopy[MAX_NUM_GPU];
    std::thread * threadDeviceProcess[MAX_NUM_GPU];
    std::thread * threadDeviceOutputCopy[MAX_NUM_GPU];
    // master input/output queues
    MessageQueue<std::tuple<int,char *,int>> inputQ;
    MessageQueue<std::tuple<int,int>>        outputQ;
    // device queues
    MessageQueue<int>                    * queueDeviceTagQ[MAX_NUM_GPU];
    MessageQueue<std::tuple<char *,int>> * queueDeviceImageQ[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceInputMemIdle[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceInputMemBusy[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceOutputMemIdle[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceOutputMemBusy[MAX_NUM_GPU];
};

#endif
