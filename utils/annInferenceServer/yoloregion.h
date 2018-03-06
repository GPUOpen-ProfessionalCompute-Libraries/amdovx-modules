#ifndef YOLOREGION_H
#define YOLOREGION_H

#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

struct box
{
    float x, y, w, h;
};

struct rect
{
    float left, top, right, bottom;
};

struct YoloDetectedObject
{
    int left, top, right, bottom;
    float   confidence;
    int     objType;
    std::string objClassName;
};


class CYoloRegion
{
public:
    CYoloRegion();
    ~CYoloRegion();

    void Initialize(int c, int h, int w, int size);
    int GetObjectDetections(float* in_data, std::vector<std::pair<float, float>>& biases, int c, int h, int w,
                               int classes, int imgw, int imgh,
                               float thresh, float nms_thresh,
                               int blockwd,
                               std::vector<YoloDetectedObject> &objects);
private:
    int Nb;              // number of bounding boxes
    bool initialized;
    unsigned int outputSize;
    int totalObjectsPerClass;
    float *output;
    std::vector<box> boxes;

    // private member functions
    void Reshape(float *input, float *output, int n, int size);
    float Sigmoid(float x);
    void SoftmaxRegion(float *input, int classes, float *output);
    int argmax(float *a, int n);
    float box_iou(box a, box b);              // intersection over union

};

#endif // YOLOREGION_H

