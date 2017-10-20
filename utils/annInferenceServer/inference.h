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
#include <condition_variable>
#include <VX/vx.h>
#include <vx_ext_amd.h>

// inference scheduler modes
#define NO_INFERENCE_SCHEDULER        0
#define SIMPLE_INFERENCE_SCHEDULER    1

// configuration
//   INFERENCE_SCHEDULER_MODE     - pick one of the modes from above
//   INFERENCE_SERVICE_IDLE_TIME  - inference service idle time (milliseconds) if there is no activity
#define INFERENCE_SCHEDULER_MODE       SIMPLE_INFERENCE_SCHEDULER
#define INFERENCE_SERVICE_IDLE_TIME    1

// simple inference configuration
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
#define INFERENCE_PIPE_QUEUE_DEPTH     5  // inference pipe queue depth
#define MAX_INPUT_QUEUE_DEPTH       1024  // number of images
#define USE_VX_TENSOR_WITHOUT_CL       1  // TODO: remove after debug
#endif

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
#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
    // simple scheduler thread workers
    void workMasterInputQ();
    void workDeviceInputCopy(int gpu);
    void workDeviceProcess(int gpu);
    void workDeviceOutputCopy(int gpu);
#endif

private:
    void dumpBuffer(cl_command_queue cmdq, cl_mem mem, std::string fileName);

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
    int batchSize;
    int inputSizeInBytes;
    int outputSizeInBytes;
    bool deviceLockSuccess;
    // scheduler output queue
    //   outputQ: output from the scheduler <tag,label>
    MessageQueue<std::tuple<int,int>>        outputQ;

#if INFERENCE_SCHEDULER_MODE == SIMPLE_INFERENCE_SCHEDULER
    // master input queues
    //   inputQ: input to the scheduler <tag,byteStream,size>
    MessageQueue<std::tuple<int,char *,int>> inputQ;
    // master scheduler thread
    std::thread * threadMasterInputQ;
    // scheduler thread objects
    std::thread * threadDeviceInputCopy[MAX_NUM_GPU];
    std::thread * threadDeviceProcess[MAX_NUM_GPU];
    std::thread * threadDeviceOutputCopy[MAX_NUM_GPU];
    // scheduler device queues
    MessageQueue<int>                    * queueDeviceTagQ[MAX_NUM_GPU];
    MessageQueue<std::tuple<char *,int>> * queueDeviceImageQ[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceInputMemIdle[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceInputMemBusy[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceOutputMemIdle[MAX_NUM_GPU];
    MessageQueue<cl_mem>                 * queueDeviceOutputMemBusy[MAX_NUM_GPU];
    // scheduler resources
    cl_context opencl_context[MAX_NUM_GPU];
    cl_command_queue opencl_cmdq[MAX_NUM_GPU];
    vx_context openvx_context[MAX_NUM_GPU];
    vx_graph openvx_graph[MAX_NUM_GPU];
    vx_tensor openvx_input[MAX_NUM_GPU];
    vx_tensor openvx_output[MAX_NUM_GPU];
#endif
};

#endif
