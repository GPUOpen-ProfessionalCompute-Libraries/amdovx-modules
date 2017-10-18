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

#define MAX_INPUT_QUEUE_DEPTH       1024  // number of images
#define INFERENCE_SERVICE_IDLE_TIME 1000  // milliseconds

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
    void inputQueueMasterScheduler();

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
    cl_device_id device_id[MAX_NUM_GPU];
    bool deviceLockSuccess;
    // input and output data queues
    MessageQueue<std::tuple<int,char *,int>> inputQ;
    MessageQueue<std::tuple<int,int>> outputQ;
    int numInputReceived;
    int numInputConsumed;
    int numOutputProduced;
    int numOutputConsumed;
    // thread
    std::thread * threadMasterInputQ;
};

#endif
