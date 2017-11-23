# annInferenceServer

## DESCRIPTION
Inference Server

## Command-line Usage
    % annInferenceServer [-p port] [-b default-batch-size] [-gpu <comma-separated-list-of-GPUs>] [-q <max-pending-batches>] [-w <server-work-folder>]

## Features

 - Works with annInferenceApp
 - TCP/IP client-server network communication protocol and message formats
 - Multi-GPU high-throughput live streaming batch scheduler
 - OpenVX NN graph compiler from pre-trained caffe models

## Dependencies
ROCm, OpenCL, MIOpenGEMM, and MIOpen installed in `/opt/rocm`

## How to build and install?
    % cd ~
    % git clone --recursive -b develop https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-modules
    % mkdir amdovx-modules/build
    % cd amdovx-modules/build
    % cmake -DCMAKE_BUILD_TYPE=Release ..
    % make
    % sudo chmod -R 777 /opt/rocm
    % make install

## How to run the server?
    % export PATH=$PATH:/opt/rocm/bin
    % export LD_LIBRARY_PATH=/opt/rocm/lib
    % annInferenceServer

## How to build and run the app?

 - Open the Qt project `amdovx-modules/utils/annInferenceApp/annInferenceApp.pro` and build
 - Launch the application
 - Select the server hostname and click the `Connect` button
 - You should see successful connection, if server is running
 - Browse and pick the .prototxt of a pre-trained Caffe Model
 - Browse and pick the .caffemodel of a pre-trained Caffe Model
 - Specify the input tensor height and width: look for `CxHxW(inp)`
 - Click the `Upload` button to upload the model and compile
 - Browse and pick the synset text file for labels
 - Browse and pick the folder with input images for inference testing
 - Image List and Image Count are optional
 - Click the `Run` button
