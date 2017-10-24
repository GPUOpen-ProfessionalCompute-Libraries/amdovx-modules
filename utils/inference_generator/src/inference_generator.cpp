/*
Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <iostream>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <fstream>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "caffe.pb.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

#define error(...) printf("ERROR: " __VA_ARGS__), exit(1)
#define info(...)  printf("OK: " __VA_ARGS__)

//Dump Layer Data : disabled unless enabled explicitly by setting ENABLE_DUMP_LAYER_DATA = 1
#ifndef ENABLE_DUMP_LAYER_DATA
#define ENABLE_DUMP_LAYER_DATA 0
#endif

#ifndef ENABLE_DIRECTIVE
#define ENABLE_DIRECTIVE 0
#endif

void getLayerParams(
    const caffe::LayerParameter& layer,
    std::string& params)
{
    if(layer.type() == "Convolution") {
        const caffe::ConvolutionParameter& conv = layer.convolution_param();
        int pad_h = conv.has_pad_h() ? conv.pad_h() : (conv.pad_size() > 0 ? conv.pad(0) : 0);
        int pad_w = conv.has_pad_w() ? conv.pad_w() : (conv.pad_size() > 1 ? conv.pad(1) : pad_h);
        int stride_h = conv.has_stride_h() ? conv.stride_h() : (conv.stride_size() > 0 ? conv.stride(0) : 1);
        int stride_w = conv.has_stride_w() ? conv.stride_w() : (conv.stride_size() > 1 ? conv.stride(1) : stride_h);
        int kernel_h = conv.has_kernel_h() ? conv.kernel_h() : (conv.kernel_size_size() > 0 ? conv.kernel_size(0) : 0);
        int kernel_w = conv.has_kernel_w() ? conv.kernel_w() : (conv.kernel_size_size() > 1 ? conv.kernel_size(1) : kernel_h);
        int k = conv.num_output();
        int dilation_h = conv.dilation_size() > 0 ? conv.dilation(0) : 1;
        int dilation_w = conv.dilation_size() > 1 ? conv.dilation(1) : dilation_h;
        int bias_term = conv.bias_term();
        int group = conv.has_group() ? conv.group() : 0;
        params =       std::to_string(k)
                + " " + std::to_string(kernel_w)
                + " " + std::to_string(kernel_h)
                + " " + std::to_string(stride_w)
                + " " + std::to_string(stride_h)
                + " " + std::to_string(pad_w)
                + " " + std::to_string(pad_h)
                + " " + std::to_string(dilation_w)
                + " " + std::to_string(dilation_h)
                + " " + std::to_string(bias_term)
                + " " + std::to_string(group);
    }
    else if(layer.type() == "Pooling") {
        const caffe::PoolingParameter& pooling = layer.pooling_param();
        int pad_h = pooling.has_pad_h() ? pooling.pad_h() : pooling.pad();
        int pad_w = pooling.has_pad_w() ? pooling.pad_w() : pooling.pad();
        int stride_h = pooling.has_stride_h() ? pooling.stride_h() : pooling.stride();
        int stride_w = pooling.has_stride_w() ? pooling.stride_w() : pooling.stride();
        int kernel_h = pooling.has_kernel_h() ? pooling.kernel_h() : pooling.kernel_size();
        int kernel_w = pooling.has_kernel_w() ? pooling.kernel_w() : pooling.kernel_size();
        int pool = pooling.pool();
        int global_pooling = pooling.global_pooling() == true ? 1 : 0;
        params =       std::to_string(kernel_w)
                + " " + std::to_string(kernel_h)
                + " " + std::to_string(stride_w)
                + " " + std::to_string(stride_h)
                + " " + std::to_string(pad_w)
                + " " + std::to_string(pad_h)
                + " " + std::to_string(pool)
                + " " + std::to_string(global_pooling);
    }
    else if(layer.type() == "InnerProduct") {
        const caffe::InnerProductParameter& innerprod = layer.inner_product_param();
        int k = innerprod.num_output();
        int bias_term = innerprod.bias_term();
        params = std::to_string(k) + " " + std::to_string(bias_term);
    }
    else if(layer.type() == "LRN") {
        const caffe::LRNParameter& lrn = layer.lrn_param();
        const caffe::LRNParameter::NormRegion& norm_region = lrn.norm_region();
        params =       std::to_string(lrn.local_size())
                + " " + std::to_string(lrn.alpha())
                + " " + std::to_string(lrn.beta())
                + " " + std::to_string(norm_region)
                + " " + std::to_string(lrn.k());
    }
    else if(layer.type() == "BatchNorm") {
        const caffe::BatchNormParameter& norm = layer.batch_norm_param();
        int use_global_stats = norm.use_global_stats();
        float eps = norm.eps();
        params =       std::to_string(eps)
                + " " + std::to_string(use_global_stats);
    }
    else if(layer.type() == "Scale") {
        const caffe::ScaleParameter& scale = layer.scale_param();
        params = std::to_string(scale.bias_term());
    }
    else if(layer.type() == "Dropout") {
        const caffe::DropoutParameter& dropout = layer.dropout_param();
        params = std::to_string(dropout.dropout_ratio());
    }
    else if(layer.type() == "Eltwise") {
        const caffe::EltwiseParameter& eltwise = layer.eltwise_param();
        params = std::to_string(eltwise.operation());
    }
    else if(layer.type() == "Deconvolution") {
        const caffe::ConvolutionParameter& conv = layer.convolution_param();
        int pad_h = conv.has_pad_h() ? conv.pad_h() : (conv.pad_size() > 0 ? conv.pad(0) : 0);
        int pad_w = conv.has_pad_w() ? conv.pad_w() : (conv.pad_size() > 1 ? conv.pad(1) : pad_h);
        int stride_h = conv.has_stride_h() ? conv.stride_h() : (conv.stride_size() > 0 ? conv.stride(0) : 1);
        int stride_w = conv.has_stride_w() ? conv.stride_w() : (conv.stride_size() > 1 ? conv.stride(1) : stride_h);
        int kernel_h = conv.has_kernel_h() ? conv.kernel_h() : (conv.kernel_size_size() > 0 ? conv.kernel_size(0) : 0);
        int kernel_w = conv.has_kernel_w() ? conv.kernel_w() : (conv.kernel_size_size() > 1 ? conv.kernel_size(1) : kernel_h);
        int k = conv.num_output();
        int dilation_h = conv.dilation_size() > 0 ? conv.dilation(0) : 1;
        int dilation_w = conv.dilation_size() > 1 ? conv.dilation(1) : dilation_h;
        int bias_term = conv.bias_term();
        params =       std::to_string(k)
                + " " + std::to_string(kernel_w)
                + " " + std::to_string(kernel_h)
                + " " + std::to_string(stride_w)
                + " " + std::to_string(stride_h)
                + " " + std::to_string(pad_w)
                + " " + std::to_string(pad_h)
                + " " + std::to_string(dilation_w)
                + " " + std::to_string(dilation_h)
                + " " + std::to_string(bias_term);
    }
}

int loadCaffeProtoTxt(
    const char * prototxtFileName,
    std::vector<std::vector<std::string>>& net,
    int inputDim[4])
{
    // verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    //google::protobuf::Message * msg = new google::protobuf::Message();
    caffe::NetParameter * msg = new caffe::NetParameter();

    // open prototxt and parse
    int fd = open(prototxtFileName, O_RDONLY);
    if(fd < 0)
        error("unable to open: %s\n", prototxtFileName);
    google::protobuf::io::FileInputStream fi(fd);
    fi.SetCloseOnDelete(true);
    if (!google::protobuf::TextFormat::Parse(&fi, msg))
        error("failed to parse file: %s\n", prototxtFileName);
    info("loadCaffeProtoTxt: loading %s from %s\n", msg->has_name() ? msg->name().c_str() : "(none)", prototxtFileName);

    if(!(msg->layer_size() > 0)) {
        std::cerr << "ERROR: [Unsupported caffe prototxt] please upgrade this prototxt, currently uses deprecated V1LayerParameters." << std::endl;
        return -1;
    }

    // initialize outputNameMap and input dimensions if available
    std::map<std::string,std::string> outputNameMap;
    if(msg->input_size() > 0) {
        outputNameMap[msg->input(0)] = msg->input(0);
    }

    if(msg->input_dim_size() == 4  && ((inputDim[0]==0) || (inputDim[1]==0) || (inputDim[2]==0) || (inputDim[3]==0)) ) {
        inputDim[0] = msg->input_dim(0);
        inputDim[1] = msg->input_dim(1);
        inputDim[2] = msg->input_dim(2);
        inputDim[3] = msg->input_dim(3);
    }

    // process network layer by layer
    for(int i = 0; i < msg->layer_size(); i++) {
        // get current layer
        const caffe::LayerParameter layer = msg->layer(i);

        if(layer.type() == "Input" || layer.type() == "Data" || layer.type() == "ImageData") {
            outputNameMap[layer.top(0)] = layer.top(0);

            if(layer.type() == "Input"  && ((inputDim[0]==0) || (inputDim[1]==0) || (inputDim[2]==0) || (inputDim[3]==0))) {
                inputDim[0] = layer.input_param().shape(0).dim(0);
                inputDim[1] = layer.input_param().shape(0).dim(1);
                inputDim[2] = layer.input_param().shape(0).dim(2);
                inputDim[3] = layer.input_param().shape(0).dim(3);
            }
            continue;
        }

        //Split type.
        if(layer.type()=="Split") {
            for(int j=0; j< layer.top_size() ; j++ )
            {
                // get layer information and add to net
                std::vector<std::string> node;
                node.push_back(layer.type());
                node.push_back("");
                node.push_back(layer.top(j));
                node.push_back(layer.top(j));
                for(int z = 0; z < layer.bottom_size();z++) {
                    if(outputNameMap.find(layer.bottom(z)) == outputNameMap.end()) {
                        outputNameMap[layer.bottom(z)] = layer.bottom(z);
                    }
                    node.push_back(outputNameMap[layer.bottom(z)]);

                }
                net.push_back(node);

                // update output name with layer name
                outputNameMap[layer.top(j)] = layer.top(j);
            }
            continue;
        }

        // get layer information and add to net
        std::vector<std::string> node;
        std::string params;
        getLayerParams(layer, params);
        node.push_back(layer.type());
        node.push_back(params);
        node.push_back(layer.top(0));
        node.push_back(layer.name());
        for(int j = 0; j < layer.bottom_size()  ; j++) {
            if(outputNameMap.find(layer.bottom(j)) == outputNameMap.end()) {
                outputNameMap[layer.bottom(j)] = layer.bottom(j);
            }
            node.push_back(outputNameMap[layer.bottom(j)]);
        }
        net.push_back(node);

        // update output name with layer name
        outputNameMap[layer.top(0)] = layer.name();
    }

    return 0;
}

int calculateTensorDim(
    std::vector<std::vector<std::string>>& net,
    int inputDim[4],
    std::map<std::string,std::vector<int>>& tensorMap)
{
    tensorMap[net[0][4]] = std::vector<int>{inputDim[0], inputDim[1], inputDim[2], inputDim[3]};

    for(auto& node : net) {
        auto&& type = node[0];
        auto&& params = node[1];
        auto&& output = node[3];
        auto&& input = node[4];
        auto&& it = tensorMap.find(input);
        if(it == tensorMap.end()) {
            error("calculateTensorDim: no dims found for %s\n", input.c_str());
        }

        auto&& idim = it->second;
        int n = idim[0], c = idim[1], H = idim[2], W = idim[3];
        int k = c, h = H, w = W;

        if (n < 1 || c < 1 || H < 1 || W < 1)
            error("calculateTensorDim: got invalid dim %dx%dx%dx%d for %s\n", n, c, H, W, input.c_str());

        if(type == "Convolution") {
            std::stringstream ss(params);
            int kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term;
            w = ((W + 2 * pad_w - kernel_w - (kernel_w - 1) * (dilation_w - 1)) / stride_w) + 1;
            h = ((H + 2 * pad_h - kernel_h - (kernel_h - 1) * (dilation_h - 1)) / stride_h) + 1;
            tensorMap[output + "_W"] = std::vector<int>{k, c, kernel_h, kernel_w};
            if(bias_term) {
                tensorMap[output + "_B"] = std::vector<int>{k};
            }
        }
        else if(type == "Deconvolution") {
            std::stringstream ss(params);
            int kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term;
            w = stride_w * (W - 1) + dilation_w * (kernel_w - 1) + 1 - ( 2* pad_w );
            h = stride_h * (H - 1) + dilation_h * (kernel_h - 1) + 1 - ( 2* pad_h );
            tensorMap[output + "_W"] = std::vector<int>{k, c, kernel_h, kernel_w};
            if(bias_term) {
                tensorMap[output + "_B"] = std::vector<int>{k};
            }
        }
        else if(type == "Pooling") {
            std::stringstream ss(params);
            int kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, pool, global_pooling;
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> pool >> global_pooling;
            if(global_pooling) {
                // Compute kernel_w and kernel_h and write back the params for the GDF and C-code gen
                kernel_h = H;
                kernel_w = W;
                pad_h = pad_w = 0;
                stride_h = stride_w = 1;
                params =        std::to_string(kernel_w)
                        + " " + std::to_string(kernel_h)
                        + " " + std::to_string(stride_w)
                        + " " + std::to_string(stride_h)
                        + " " + std::to_string(pad_w)
                        + " " + std::to_string(pad_h)
                        + " " + std::to_string(pool)
                        + " " + std::to_string(global_pooling);
            }
            w = static_cast<int>(ceil( static_cast<float> (W + 2 * pad_w + stride_w - kernel_w)/ stride_w));
            h = static_cast<int>(ceil( static_cast<float> (H + 2 * pad_h + stride_h - kernel_h)/ stride_h));
            if(pad_h > 0) if((h-1)*stride_h >= (H+pad_h)) h=h-1;
            if(pad_w > 0) if((w-1)*stride_w >= (W+pad_w)) w=w-1;
        }
        else if(type == "InnerProduct") {
            std::stringstream ss(params);
            ss >> k;
            w = 1;
            h = 1;
            tensorMap[output + "_W"] = std::vector<int>{k, c, H, W};
        }
        else if(type == "Concat") {
            for(int i = 5; i < node.size(); i++) {
                auto&& dim = tensorMap[node[i]];
                k += dim[1];
                if(dim[0] != n || dim[2] != H || dim[3] != W)
                    error("calculateTensorDim: Concat: got invalid dim %dx%dx%dx%d for %s (should be %dx*x%dx%d)\n", dim[0], dim[1], dim[2], dim[3], node[i].c_str(), n, H, W);
            }
        }
        else if(type == "SoftmaxWithLoss") {
            output = node[5];
        }
        else if (type == "BatchNorm") {
            std::stringstream ss(params);
            int use_global_stats;
            float eps;
            ss >> eps >> use_global_stats;
            tensorMap[output + "_W"] = std::vector<int>{k};
            tensorMap[output + "_B"] = std::vector<int>{k};
        }
        else if(type == "Scale") {
            std::stringstream ss(params);
            int bias_term;
            ss >> bias_term;
            tensorMap[output + "_W"] = std::vector<int>{k};
            if(bias_term) {
                tensorMap[output + "_B"] = std::vector<int>{k};
            }
        }

        tensorMap[output] = std::vector<int>{n, k, h, w};
        if(n < 1 || k < 1 || h < 1 || w < 1)
            error("calculateTensorDim: got invalid dim %dx%dx%dx%d for %s\n", n, k, h, w, output.c_str());
    }
    return 0;
}

void formatFileName(std::string& str, const std::string& from, const std::string& to)
{
    //Written to avoid conflicts with file creation with filenames that contain "/"
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

void writeGDF(
    std::ostream& ofsGDF,
    std::vector<std::vector<std::string>>& net,
    std::map<std::string,std::vector<int>>& tensorMap,
    std::string tensorType,
    int fixedPointPosition,
    std::string convertPolicy,
    std::string roundPolicy,
    bool isVirtualEnabled,
    std::string outputFolder,
    bool bFuseScaleLayer)
{
    std::map<std::string,bool> tensorCheck;
    ofsGDF << "import vx_nn" << std::endl;
    bool bfuse_scale_layer = bFuseScaleLayer;

    for(auto& node : net) {
        // create input/output tensor objects
        bool isFirstLayer = (&node == &net.front());
        bool isLastLayer = (&node == &net.back());
        for(size_t i = 4; i < node.size(); i++) {
            if(node[i] != "" && tensorCheck.find(node[i]) == tensorCheck.end()) {
                auto&& dim = tensorMap[node[i]];
                if((isVirtualEnabled && isFirstLayer) || (isVirtualEnabled && isLastLayer)) {
                    ofsGDF << "data " << node[i] << " = tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                    tensorCheck[node[i]] = true;
                    if(!isLastLayer) {
                        ofsGDF << "read data input.f32" << std::endl;
                    }
                }
                else {
                    if(isVirtualEnabled) {
                        ofsGDF << "data " << node[i] << " = virtual-tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                        tensorCheck[node[i]] = true;
                    }
                    else {
                        ofsGDF << "data " << node[i] << " = tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                        tensorCheck[node[i]]= true;
                        if(isFirstLayer) ofsGDF << "read data input.f32" << std::endl;
                    }
                }
            }
        }
        auto&& output = node[3];
        if (node[0] == "BatchNorm" && !isLastLayer && bfuse_scale_layer) {
            auto& next_node = *std::next(&node);
            if (next_node[0] == "Scale") {
                auto&& next_output = next_node[3];
                auto&& odim = tensorMap[next_output];
                tensorCheck[output] = true; // make sure next node doesn't create input tensor
                if(!tensorCheck[next_output]) {
                    if(!isVirtualEnabled) {
                        ofsGDF << "data " << next_output << " = tensor:4,{" << odim[3] << "," << odim[2] << "," << odim[1] << "," << odim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                    }
                    else {
                        if(!isLastLayer) {
                            ofsGDF << "data " << next_output << " = virtual-tensor:4,{" << odim[3] << "," << odim[2] << "," << odim[1] << "," << odim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                        }
                        else {
                            ofsGDF << "data " << next_output << " = tensor:4,{" << odim[3] << "," << odim[2] << "," << odim[1] << "," << odim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                        }
                    }
#if ENABLE_DIRECTIVE
                    ofsGDF << "directive " << next_output << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
                }
                tensorCheck[next_output] = true;
                bfuse_scale_layer = true;
            }
        }

        if (node[0] == "Scale" && !isFirstLayer && bfuse_scale_layer) {
            auto& prev_node = *std::prev(&node);
            if (prev_node[0]=="BatchNorm")
            continue;
        }

        auto&& odim = tensorMap[output];
        if(!tensorCheck[output]) {
            if(!isVirtualEnabled) {
                ofsGDF << "data " << output << " = tensor:4,{" << odim[3] << "," << odim[2] << "," << odim[1] << "," << odim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
            } else {
                if(!isLastLayer) {
                    ofsGDF << "data " << output << " = virtual-tensor:4,{" << odim[3] << "," << odim[2] << "," << odim[1] << "," << odim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                }
                else {
                    ofsGDF << "data " << output << " = tensor:4,{" << odim[3] << "," << odim[2] << "," << odim[1] << "," << odim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                }
            }
#if ENABLE_DIRECTIVE
            ofsGDF << "directive " << output << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
        }
        tensorCheck[output] = true;

        // create node object
        auto&& type = node[0];
        auto&& params = node[1];
        std::string layer_name = node[3];
        formatFileName(layer_name,"/","_");
        if(type == "Convolution") {
            std::stringstream ss(params);
            int k, kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term, group;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term >> group;
            std::string weights = output + "_W";
            auto&& dim = tensorMap[weights];
            ofsGDF << "data " << weights << " = tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
            ofsGDF << "init " << weights << " ";
            if(group > 1) ofsGDF << "@repeat~" << group << "~";
            ofsGDF << "weights/" << layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
            ofsGDF << "directive " << weights << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
            tensorCheck[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = output + "_B";
                ofsGDF << "data " << bias << " = tensor:1,{" << k << "}," << tensorType << "," << fixedPointPosition << std::endl;
                ofsGDF << "init " << bias << " ";
                if(group > 1) ofsGDF << "@repeat~" << group << "~";
                ofsGDF << "bias/"<< layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
                ofsGDF << "directive " << bias << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
                tensorCheck[bias] = true;
            }

            ofsGDF << "data " << node[3] << "_params = " << " scalar:VX_TYPE_NN_CONV_PARAMS,{" << pad_w << "," << pad_h << "," << convertPolicy << "," << roundPolicy << ",VX_NN_DS_SIZE_ROUNDING_FLOOR," << dilation_w-1 << "," << dilation_h-1 << "}" << std::endl;
            ofsGDF << "node org.khronos.nn_extension.convolution_layer " << node[4] << " " << node[3] << "_W" << " " << bias << " "
                   << node[3] <<"_params"
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if (type == "Deconvolution") {
            std::stringstream ss(params);
            int k, kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term;
            std::string weights = output + "_W";
            auto&& dim = tensorMap[weights];
            ofsGDF << "data " << weights << " = tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
            ofsGDF << "init " << weights << " weights/" << layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
            ofsGDF << "directive " << weights << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
            tensorCheck[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = output + "_B";
                ofsGDF << "data " << bias << " = tensor:1,{" << k << "}," << tensorType << "," << fixedPointPosition << std::endl;
                ofsGDF << "init " << bias << " bias/"<< layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
                ofsGDF << "directive " << bias << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
                tensorCheck[bias] = true;
            }

            ofsGDF << "data " << node[3] << "_params = " << " scalar:VX_TYPE_NN_DECONV_PARAMS,{" << pad_w << "," << pad_h << "," << convertPolicy << "," << roundPolicy << "," << dilation_w-1 << "," << dilation_h-1 << "}" << std::endl;
            ofsGDF << "node org.khronos.nn_extension.deconvolution_layer " << node[4] << " " << node[3] << "_W" << " " << bias << " "
                   << node[3] <<"_params"
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "Pooling") {
            std::stringstream ss(params);
            int kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, pool;
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> pool;
            if((pool != 0 && pool != 1)) error("writeGDF: pooling_layer supports only MAX and AVG\n");
            ofsGDF << "data " << node[3] <<"_type = " <<  " scalar:VX_TYPE_ENUM," << (pool == 0 ? "VX_NN_POOLING_MAX" : "VX_NN_POOLING_AVG")<< std::endl;
            ofsGDF << "data " << node[3] <<"_kernel_w = " << "scalar:VX_TYPE_SIZE," << kernel_w << std::endl;
            ofsGDF << "data " << node[3] <<"_kernel_h = " << "scalar:VX_TYPE_SIZE," << kernel_h << std::endl;
            ofsGDF << "data " << node[3] <<"_pad_w = " << "scalar:VX_TYPE_SIZE," << pad_w << std::endl;
            ofsGDF << "data " << node[3] <<"_pad_h = " << "scalar:VX_TYPE_SIZE," << pad_h << std::endl;
            ofsGDF << "data " << node[3] <<"_roundPolicy = " << " scalar:VX_TYPE_ENUM," << roundPolicy << std::endl;
            ofsGDF << "node org.khronos.nn_extension.pooling_layer " << node[4] << " "
                   << node[3] << "_type" << " "
                   << node[3] << "_kernel_w "
                   << node[3] << "_kernel_h "
                   << node[3] << "_pad_w "
                   << node[3] << "_pad_h "
                   << node[3] << "_roundPolicy"
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "InnerProduct") {
            std::stringstream ss(params);
            int k, bias_term;
            ss >> k >> bias_term;
            std::string weights = output + "_W";
            auto&& dim = tensorMap[weights];
            ofsGDF << "data " << weights << " = tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
            ofsGDF << "init " << weights << " weights/"<< layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
            ofsGDF << "directive " << weights << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
            tensorCheck[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = output + "_B";
                ofsGDF << "data " << bias << " = tensor:1,{" << k << "}," << tensorType << "," << fixedPointPosition << std::endl;
                ofsGDF << "init " << bias << " bias/"<< layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
                ofsGDF << "directive " << bias << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
                tensorCheck[bias] = true;
            }
            ofsGDF << "data " << node[3] <<"_convertPolicy = " << " scalar:VX_TYPE_ENUM," << convertPolicy << std::endl;
            ofsGDF << "data " << node[3] <<"_roundPolicy =" << " scalar:VX_TYPE_ENUM,VX_" << roundPolicy << std::endl;
            ofsGDF << "node org.khronos.nn_extension.fully_connected_layer " << node[4] << " " << node[3] << "_W" << " " << bias << " "
                   << node[3] << "_convertPolicy "
                   << node[3] << "_roundPolicy"
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "ReLU") {
            ofsGDF << "data " << node[3] << "_mode = " << " scalar:VX_TYPE_ENUM,VX_NN_ACTIVATION_RELU" << std::endl;
            ofsGDF << "data " << node[3] << "_param_a =" << " scalar:VX_TYPE_FLOAT32,0" << std::endl;
            ofsGDF << "data " << node[3] << "_param_b =" << " scalar:VX_TYPE_FLOAT32,0" << std::endl;
            ofsGDF << "node org.khronos.nn_extension.activation_layer " << node[4] << " "
                   << node[3] << "_mode "
                   << node[3] << "_param_a "
                   << node[3] << "_param_b"
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "LRN") {
            int normalization_size;
            float alpha, beta, k;
            std::string norm_region;
            std::stringstream ss(params);
            ss >> normalization_size >> alpha >> beta >> norm_region >> k;
            std::string lrnType;
            if(norm_region == "1") lrnType = "VX_NN_NORMALIZATION_SAME_MAP";
            else lrnType = "VX_NN_NORMALIZATION_ACROSS_MAPS";
            ofsGDF << "data " << node[3] << "_mode = " << " scalar:VX_TYPE_ENUM," << lrnType << std::endl;
            ofsGDF << "data " << node[3] << "_size = " << " scalar:VX_TYPE_SIZE," << normalization_size << std::endl;
            ofsGDF << "data " << node[3] << "_alpha =" << " scalar:VX_TYPE_FLOAT32," << alpha << std::endl;
            ofsGDF << "data " << node[3] << "_beta ="  << " scalar:VX_TYPE_FLOAT32," << beta << std::endl;
            ofsGDF << "data " << node[3] << "_bias ="  << " scalar:VX_TYPE_FLOAT32," << k << std::endl;
            ofsGDF << "node org.khronos.nn_extension.normalization_layer " << node[4] << " "
                   << node[3] << "_mode "
                   << node[3] << "_size "
                   << node[3] << "_alpha "
                   << node[3] << "_beta "
                   << node[3] << " "
                   << node[3] << "_bias"
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "BatchNorm") {
            int use_global_stats, bias_term;
            float eps;
            std::stringstream ss(params);
            ss >> eps >> use_global_stats;
            std::string weights = output + "_W";
            auto&& dim = tensorMap[weights];
            ofsGDF << "data " << weights << " = tensor:1,{" << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
            ofsGDF << "init " << weights << " weights/" << layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
            ofsGDF << "directive " << weights << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
            tensorCheck[weights] = true;
            std::string bias = output + "_B";
            dim = tensorMap[bias];
            ofsGDF << "data " << bias << " = tensor:1,{" << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
            ofsGDF << "init " << bias << " bias/" << layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
            ofsGDF << "directive " << bias << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
            tensorCheck[bias] = true;
            bias = "NULL";
            if (bfuse_scale_layer) {
                // check next node. If scale extract weight and bias paramters for scale layer.
                auto& next_node = *std::next(&node);
                auto&& next_output = next_node[3];
                auto&& nn_params = next_node[1];
                std::string nn_layer_name = next_node[3];
                formatFileName(nn_layer_name,"/","_");
                weights = next_output + "_W";
                std::stringstream ss(nn_params);
                ss >> bias_term;
                dim = tensorMap[weights];
                ofsGDF << "data " << weights << " = tensor:1,{" << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                ofsGDF << "init " << weights << " weights/" << nn_layer_name << ".f32" << std::endl;
                tensorCheck[weights] = true;
                if(bias_term) {
                    bias = next_output + "_B";
                    ofsGDF << "data " << bias << " = tensor:1,{" << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                    ofsGDF << "init " << bias << " bias/"<< nn_layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
                    ofsGDF << "directive " << bias << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
                    tensorCheck[bias] = true;
                }
                ofsGDF << "data " << node[3] << "_eps ="  << " scalar:VX_TYPE_FLOAT32," << eps << std::endl;
                ofsGDF << "node com.amd.nn_extension.batch_normalization_layer " << node[4] << " " << node[3] << "_W "
                       << node[3] << "_B "
                       << weights << " "
                       << bias << " "
                       << node[3] << "_eps "
                       << next_node[3]
                       << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< next_node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
            }
            else {
                weights = output +"_W1";
                ofsGDF << "data " << weights << " = tensor:1,{" << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                // put default scale and bias term
                std::vector<float> scale_arr(dim[0]);
                std::fill(scale_arr.begin(), scale_arr.end(), 1.0);
                std::string fileName_weights = outputFolder + "/scale_init.f32";
                FILE *fp = fopen(fileName_weights.c_str(), "wb");
                if (fp) {
                    fwrite(scale_arr.data(), sizeof(float), dim[0], fp);
                    fclose(fp);
                }
                ofsGDF << "init " << weights << " scale_init.f32" << std::endl;
                ofsGDF << "data " << node[3] << "_eps ="  << " scalar:VX_TYPE_FLOAT32," << eps << std::endl;
                ofsGDF << "node com.amd.nn_extension.batch_normalization_layer " << node[4] << " " << node[3] << "_W "
                       << node[3] << "_B "
                       << weights << " "
                       << bias << " "
                       << node[3] << "_eps "
                       << output
                       << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< output << " out/"<< layer_name << ".f32" << std::endl;
#endif
            }
        }
        else if(type == "Eltwise") {
            int op;
            std::stringstream ss(params);
            ss >> op;
            auto&& dim = tensorMap[node[3]];
            for(int i = 4; i < node.size(); i++) {
                auto&& idim = tensorMap[node[i]];
                if(dim[0] != idim[0] || dim[1] != idim[1] || dim[2] != idim[2] || dim[3] != idim[3])
                    error("writeGDF: Eltwise op=%d requires same dimension inputs: %s[%dx%dx%dx%d] != %s[%dx%dx%dx%d]\n", op, node[i].c_str(), idim[0], idim[1], idim[2], idim[3], node[i-1].c_str(), dim[0], dim[1], dim[2], dim[3]);
                dim = idim;
            }
            std::string tmp = node[4];
            for(int i = 5; i < node.size(); i++) {
                std::string out = node[3];
                if(i < node.size()-1) {
                    out += "tmp_" + std::to_string(i-4);
                    ofsGDF << "data " << out << " = tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                    tensorCheck[out] = true;
                }
                if(op == 1) {
                    ofsGDF << "data " << node[3] <<"_convertPolicy =" << " scalar:VX_TYPE_ENUM," << convertPolicy << std::endl;
                    ofsGDF << "node org.khronos.openvx.tensor_add " << tmp << " " << node[i] << " "
                           << node[3] << "_convertPolicy"
                           << " " << out
                           << std::endl;
                    tmp = out;
#if ENABLE_DUMP_LAYER_DATA
                    ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
                }
                else error("writeGDF: Eltwise op=%d not supported\n", op);
            }
        }
        else if(type == "Scale") {
            int bias_term;
            auto&& type = node[0];
            auto&& params = node[1];
            std::string layer_name = node[3];
            formatFileName(layer_name,"/","_");
            std::string weights = output + "_W";
            std::stringstream ss(params); ss >> bias_term;
            auto&& dim = tensorMap[weights];
            ofsGDF << "data " << weights << " = tensor:1,{" << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
            ofsGDF << "init " << weights << " weights/" << layer_name << ".f32" << std::endl;
            tensorCheck[weights] = true;
#if ENABLE_DIRECTIVE
            ofsGDF << "directive " << weights << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
            std::string bias = "NULL";
            if(bias_term) {
                bias = output + "_B ";
                ofsGDF << "data " << bias << " = tensor:1,{" << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                ofsGDF << "init " << bias << " bias/"<< layer_name << ".f32" << std::endl;
#if ENABLE_DIRECTIVE
                ofsGDF << "directive " << bias << " VX_DIRECTIVE_AMD_COPY_TO_OPENCL" << std::endl;
#endif
                tensorCheck[bias] = true;
            }

            ofsGDF << "node com.amd.nn_extension.scale_layer " << node[4] << " "
                   << node[3] << "_W "
                   << node[3] << "_B "
                   << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "Concat") {
            ofsGDF << "node com.amd.nn_extension.concat_layer";
            ofsGDF << " " << node[3];
            for(int i = 4; i < node.size(); i++) {
                ofsGDF << " " << node[i];
            }
            ofsGDF << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "Dropout") {
            //during inference dropout layer copies its input to output.
            ofsGDF << "node org.khronos.openvx.copy " << node[4] << " " << node[3] << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "Softmax") {
            ofsGDF << "node org.khronos.nn_extension.softmax_layer " << node[4]
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "Split") {
            ofsGDF << "node org.khronos.openvx.copy " << node[4] << " " << node[3] << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "SoftmaxWithLoss") {
            ofsGDF << "node org.khronos.nn_extension.softmax_layer " << node[4]
                   << " " << node[5]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else {
            ofsGDF << "# "
                   << std::left << std::setw(16) << node[0]
                   << std::left << std::setw(24) << node[1]
                   << std::left << std::setw(32) << node[3]
                      ;
            for(size_t i = 4; i < node.size(); i++)
                ofsGDF << std::left << std::setw(32) << node[i];
            ofsGDF << std::endl;
        }
        if(isLastLayer) {
            ofsGDF << "write " << node[3] << " output.f32" << std::endl;
            auto&& odim = tensorMap[node[3]];
            printf("#OUTPUT-TENSOR: %s %d %d %d %d\n", node[3].c_str(), odim[0], odim[1], odim[2], odim[3]);
        }
        ofsGDF << std::endl;
    }
}

void dumpLayerData(const caffe::LayerParameter& layer_parameter, std::string outputFolder)
{
    std:: string layer_name;
    if(layer_parameter.has_name()) {
        layer_name = layer_parameter.name();
        formatFileName(layer_name,"/","_");
    }

    std::string fileName_weights = outputFolder + "/weights/" + layer_name + ".f32";
    std::string fileName_bias = outputFolder + "/bias/" + layer_name + ".f32";
    FILE * fs_weights;
    FILE * fs_bias;
    fs_weights = fopen(fileName_weights.c_str(), "wb");
    fs_bias    = fopen(fileName_bias.c_str(),"wb");
    if(!fs_weights || !fs_bias) {
        printf("ERROR: unable to create dump files: make sure weights and bias folders are writable.\n");
        exit(1);
    }
    int blob_size = layer_parameter.blobs_size();
    if(blob_size > 0) {
        //Extracting the weights.
        const caffe::BlobProto& weights_blob = layer_parameter.blobs(0);
        int weightsize = weights_blob.data_size();

        for(int i=0;i<weightsize;i++) {
            float weight = weights_blob.data(i);
            fwrite(&weight,sizeof(float),1,fs_weights);
        }
        //Extraction of bias if exists.
        if(blob_size >= 2) {
            //Extraction of Bias.
            const caffe::BlobProto bias_blob = layer_parameter.blobs(1);
            int biassize = bias_blob.data_size();

            for(int i=0; i < biassize; i++) {
                float bias = bias_blob.data(i);
                fwrite(&bias,sizeof(float),1,fs_bias);
            }
        }
    }

    fclose(fs_weights);
    fclose(fs_bias);
}

void writeVXCode(
    std::ostream& ofsCodeC,
    std::vector<std::vector<std::string>>& net,
    std::map<std::string,std::vector<int>>& tensorMap,
    std::string tensorType,
    int fixedPosition,
    std::string convertPolicy,
    std::string roundPolicy,
    bool isVirtualEnabled,
    bool bFuseScaleLayer,
    std::string codeType)
{
    auto&& inputTensorName = net[0][4];
    auto&& outputTensorName = net[net.size()-1][3];

    bool bfuse_scale_layer = bFuseScaleLayer;
    std::map<std::string,bool> declare_tensor_check;
    for(auto& node : net) {
        //declare input tensors.
        bool isFirstLayer = (&node == &net.front());
        bool isLastLayer = (&node == &net.back());

        std::string layerName = node[3];
        formatFileName(layerName, "/", "_");
        std::string inputName = node[4];
        formatFileName(inputName, "/", "_");
        if(codeType == "initialize") {
            ofsCodeC << "    // " << layerName <<" Layer" << std::endl;
        }
        for(size_t i=4; i < node.size(); i++) {
            if(node[i] != "" && declare_tensor_check.find(node[i]) == declare_tensor_check.end()) {
                auto&& dim = tensorMap[node[i]];
                if(codeType == "initialize") {
                    if(node[i] != inputTensorName && node[i] != outputTensorName) {
                        ofsCodeC << "    vx_size " << node[i] << "_dims[4] = { " << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << " };" << std::endl;
                        ofsCodeC << "    vx_tensor " << node[i] << ";" << std::endl;
                        ofsCodeC << "    " << node[i] << " = vxCreateTensor(context, 4, " << node[i] + "_dims,"<< tensorType <<", " << fixedPosition << ");" << std::endl;
                        ofsCodeC << "    " << "ERROR_CHECK_OBJECT("  << node[i] << ");" << std::endl;
                    }
                }
                else if(codeType == "release") {
                    if(node[i] != inputTensorName && node[i] != outputTensorName) {
                        ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << node[i] << "));" << std::endl;
                    }
                }
                declare_tensor_check[node[i]]= true;
            }
        }

        if (node[0] == "BatchNorm" && !isLastLayer && bfuse_scale_layer) {
            auto&& output = node[3];
            auto& next_node = *std::next(&node);
            if (next_node[0] == "Scale") {
                auto&& next_output = next_node[3];
                std::string nextOutput = next_node[3];
                formatFileName(nextOutput,"/","_");
                auto&& odim = tensorMap[next_output];
                if(!declare_tensor_check[next_output]) {
                    if(codeType == "initialize") {
                        ofsCodeC << "    vx_size " << nextOutput << "_dims[4] = { " << odim[3] << ", " << odim[2] << ", " << odim[1] << ", " << odim[0] << " };" << std::endl;
                        ofsCodeC << "    vx_tensor " << nextOutput << ";" << std::endl;
                        ofsCodeC << "    " << nextOutput << " = vxCreateVirtualTensor(graph,4, " << nextOutput + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                        ofsCodeC << "    " << "ERROR_CHECK_OBJECT("  << nextOutput << ");" << std::endl;
                    }
                    else if(codeType == "release") {
                        ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << nextOutput << "));" << std::endl;
                    }
                    declare_tensor_check[output] = true;
                }
                declare_tensor_check[next_output] = true;
                bfuse_scale_layer = true;
            }
        }
        if (node[0] == "Scale" && !isFirstLayer && bfuse_scale_layer) {
            auto& prev_node = *std::prev(&node);
            if (prev_node[0]=="BatchNorm"){
                if(codeType == "initialize")  {
                    ofsCodeC << "    // [NOTE -- Scale Layer Fused With Batch Norm Layer]" << std::endl<< std::endl;
                }
                continue;
            }
        }

        // declare output tensor.
        auto&& output = node[3];
        auto&& odim = tensorMap[output];
        if(!declare_tensor_check[output]) {
            if(codeType == "initialize") {
                ofsCodeC << "    vx_size " << layerName << "_dims[4] = { " << odim[3] << ", " << odim[2] << ", " << odim[1] << ", " << odim[0] << " };" << std::endl;
                if(layerName != outputTensorName) {
                    ofsCodeC << "    vx_tensor " << layerName << ";" << std::endl;
                    ofsCodeC << "    " << layerName << " = vxCreateVirtualTensor(graph,4, " << layerName + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT("  << layerName << ");" << std::endl;
                }
            }
            else if(codeType == "release") {
                if(layerName != outputTensorName) {
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << layerName << "));" << std::endl;
                }
            }
            declare_tensor_check[output] = true;
        }

        auto&& type = node[0];
        auto&& params = node[1];
        if(type == "Convolution") {
            std::stringstream ss(params);
            int k, kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term;
            std::string weights = layerName + "_W";
            std::string dim_weights = output + "_W";
            auto&& dim = tensorMap[dim_weights];
            if(codeType == "initialize") {
                ofsCodeC << "    vx_size " << weights << "_dims[4] = { " << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << " };" << std::endl;
                ofsCodeC << "    vx_tensor " << weights << ";" << std::endl;
                ofsCodeC << "    " << weights << " = vxCreateTensor(context,4, " << weights + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << weights << ", dataFolder + \"/weights/" + layerName + ".f32\"));" << std::endl;
            }
            else if(codeType == "release") {
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << "));" << std::endl;
            }
            declare_tensor_check[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = layerName + "_B";
                if(codeType == "initialize") {
                    ofsCodeC << "    vx_size " << bias << "_dims[1] = { " << k << " };" << std::endl;
                    ofsCodeC << "    vx_tensor " << bias << ";" << std::endl;
                    ofsCodeC << "    " << bias << " = vxCreateTensor(context,1, " << bias + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << bias << ", dataFolder + \"/bias/" + layerName + ".f32\"));" << std::endl;
                }
                else if(codeType == "release") {
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << "));" << std::endl;
                }
                declare_tensor_check[bias] = true;
            }
            if(codeType == "initialize") {
                ofsCodeC << "    vx_nn_convolution_params_t " << layerName << "_params;" << std::endl;
                ofsCodeC << "    " << layerName + "_params.padding_x = " << pad_w << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.padding_y = " << pad_h << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.overflow_policy = " << convertPolicy << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.rounding_policy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.down_scale_size_rounding = " << "VX_NN_DS_SIZE_ROUNDING_FLOOR ;" << std::endl;
                ofsCodeC << "    " << layerName + "_params.dilation_x = " << dilation_w - 1 << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.dilation_y = " << dilation_h - 1 << ";" << std::endl;
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxConvolutionLayer(graph, " << inputName << ", " << weights << ", " << bias << ", &" << layerName + "_params, " << "sizeof(" << layerName + "_params ), " << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "Deconvolution") {
            std::stringstream ss(params);
            int k, kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term;
            std::string weights = layerName + "_W";
            std::string dim_weights = output + "_W";
            auto&& dim = tensorMap[dim_weights];
            if(codeType == "initialize") {
                ofsCodeC << "    vx_size " << weights << "_dims[4] = { " << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << " };" << std::endl;
                ofsCodeC << "    vx_tensor " << weights << ";" << std::endl;
                ofsCodeC << "    " << weights + "= vxCreateTensor(context,4, " << weights + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << weights << ", dataFolder + \"/weights/" + layerName + ".f32\"));" << std::endl;
            }
            else if(codeType == "release") {
                ofsCodeC << "    " << "vxReleaseTensor(&" << weights << " );" << std::endl;
            }
            declare_tensor_check[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = layerName + "_B";
                if(codeType == "initialize") {
                    ofsCodeC << "    vx_size " << bias << "_dims[1] = { " << k << " };" << std::endl;
                    ofsCodeC << "    vx_tensor " << bias << ";" << std::endl;
                    ofsCodeC << "    " << bias + " = vxCreateTensor(context,1, " << bias + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << bias << ", dataFolder + \"/bias/" + layerName + ".f32\"));" << std::endl;
                }
                else if(codeType == "release") {
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << "));" << std::endl;
                }
                declare_tensor_check[bias] = true;
            }
            if(codeType == "initialize") {
                ofsCodeC << "    vx_nn_deconvolution_params_t " << layerName << "_params;" << std::endl;
                ofsCodeC << "    " << layerName + "_params.padding_x = " << pad_w << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.padding_y = " << pad_h << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.overflow_policy = " << convertPolicy << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.rounding_policy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.a_x = " << dilation_w - 1 << ";" << std::endl;
                ofsCodeC << "    " << layerName + "_params.a_y = " << dilation_h - 1 << ";" << std::endl;
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << " vxDeconvolutionLayer(graph, " << inputName << ", " << weights << ", " << bias << ", &" << layerName + "_params, " << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "Pooling") {
            std::stringstream ss(params);
            int kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, pool;
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> pool;
            if((pool != 0 && pool != 1)) error("writeGDF: pooling_layer supports only MAX and AVG\n");
            if(codeType == "initialize") {
                ofsCodeC << "    vx_enum " << layerName << "_type = " << (pool == 0 ? "VX_NN_POOLING_MAX" : "VX_NN_POOLING_AVG") << ";" << std::endl;
                ofsCodeC << "    vx_size " << layerName << "_kernel_w = " << kernel_w << ";" << std::endl;
                ofsCodeC << "    vx_size " << layerName << "_kernel_h = " << kernel_h << ";" << std::endl;
                ofsCodeC << "    vx_size " << layerName << "_pad_w = " << pad_w << ";" << std::endl;
                ofsCodeC << "    vx_size " << layerName << "_pad_h = " << pad_h << ";" << std::endl;
                ofsCodeC << "    vx_enum " << layerName << "_roundPolicy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxPoolingLayer(graph, " << inputName << ", " << layerName + "_type" << ", " << layerName + "_kernel_w, " << layerName + "_kernel_h, "
                                   << layerName + "_pad_w, " << layerName + "_pad_h, " << layerName + "_roundPolicy, " << layerName << " );" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "InnerProduct") {
            std::stringstream ss(params);
            int k,bias_term;
            ss >> k >> bias_term;
            std::string weights = layerName + "_W";
            std::string dim_weights = output + "_W";
            auto&& dim = tensorMap[dim_weights];
            if(codeType == "initialize") {
                ofsCodeC << "    vx_size " << weights << "_dims[4] = { " << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << " };" << std::endl;
                ofsCodeC << "    vx_tensor " << weights << ";" << std::endl;
                ofsCodeC << "    " << weights << "= vxCreateTensor(context,4," << weights + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << weights << ", dataFolder + \"/weights/" + layerName + ".f32\"));" << std::endl;
            }
            else if(codeType == "release") {
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << "));" << std::endl;
            }
            declare_tensor_check[weights]= true;
            std::string bias= "NULL";
            if(bias_term) {
                bias = layerName + "_B";
                if(codeType == "initialize") {
                    ofsCodeC << "    vx_size " << bias << "_dims[1] = { " << k << " };" << std::endl;
                    ofsCodeC << "    vx_tensor " << bias << ";" << std::endl;
                    ofsCodeC << "    " << bias << "= vxCreateTensor(context,1," << bias + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << bias << ", dataFolder + \"/bias/" + layerName + ".f32\"));" << std::endl;
                }
                else if(codeType == "release") {
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << "));" << std::endl;
                }
                declare_tensor_check[bias]= true;
            }
            if(codeType == "initialize") {
                ofsCodeC << "    vx_enum " << layerName << "_convertPolicy = " << convertPolicy << ";" << std::endl;
                ofsCodeC << "    vx_enum " << layerName << "_roundPolicy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxFullyConnectedLayer( graph, " << inputName << ", " << weights << ", " << bias << ", " << layerName + "_convertPolicy, " << layerName + "_roundPolicy, " << layerName + ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "ReLU") {
            if(codeType == "initialize") {
                ofsCodeC << "    vx_enum " << layerName << "_mode = " << "VX_NN_ACTIVATION_RELU ; " << std::endl;
                ofsCodeC << "    vx_float32 " << layerName << "_param_a = 0;" << std::endl;
                ofsCodeC << "    vx_float32 " << layerName << "_param_b = 0;" << std::endl;
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxActivationLayer(graph, " << inputName << ", " << layerName + "_mode, " << layerName + "_param_a, " << layerName + "_param_b, " << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "LRN") {
            int normalization_size; float alpha,beta,k;
            std::string norm_region;
            std::stringstream ss(params);
            ss >> normalization_size >> alpha >> beta >> norm_region >> k;
            std::string lrnType;
            lrnType =  (norm_region == "1") ? "VX_NN_NORMALIZATION_SAME_MAP" : "VX_NN_NORMALIZATION_ACROSS_MAPS";
            if(codeType == "initialize") {
                ofsCodeC << "    vx_enum " << layerName << "_mode = " << lrnType << ";" << std::endl;
                ofsCodeC << "    vx_size " << layerName << "_size = "  << normalization_size << ";" << std::endl;
                ofsCodeC << "    vx_float32 " << layerName << "_alpha = " << alpha << ";" << std::endl;
                ofsCodeC << "    vx_float32 " << layerName << "_beta = " << beta << ";" << std::endl;
                ofsCodeC << "    vx_float32 " << layerName << "_bias = " << k << ";" << std::endl;
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxNormalizationLayer( graph, " << inputName << ", " << layerName + "_mode, " << layerName + "_size, " << layerName + "_alpha, " << layerName + "_beta, "
                         << layerName << ", " << layerName + "_bias );" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "BatchNorm") {
            int use_global_stats;
            std::stringstream ss(params);
            float eps;
            ss >> eps >> use_global_stats;
            std::string weights = layerName + "_W";
            std::string dim_weights = output + "_W";
            auto&& dim = tensorMap[dim_weights];
            if(codeType == "initialize") {
                ofsCodeC << "    vx_size " << weights << "_dims[1] = { " << dim[0] << " };" << std::endl;
                ofsCodeC << "    vx_float32 " << layerName << "_eps = " << eps << ";" << std::endl;
                ofsCodeC << "    vx_tensor " << weights << ";" << std::endl;
                ofsCodeC << "    " << weights << " = vxCreateTensor(context,1, " << weights + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << weights << ", dataFolder + \"/weights/" + layerName + ".f32\"));" << std::endl;
            }
            else if(codeType == "release") {
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << "));" << std::endl;
            }
            declare_tensor_check[weights] = true;
            std::string bias = layerName + "_B";
            std::string dim_bias = output + "_B";
            dim = tensorMap[dim_bias];
            if(codeType == "initialize") {
                ofsCodeC << "    vx_size " << bias << "_dims[1] = { " << dim[0] << " };" << std::endl;
                ofsCodeC << "    vx_tensor " << bias << ";" << std::endl;
                ofsCodeC << "    " << bias << " = vxCreateTensor(context,1, " << bias + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << bias << ", dataFolder + \"/bias/" + layerName + ".f32\"));" << std::endl;
            }
            else if(codeType == "release") {
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << "));" << std::endl;
            }
            declare_tensor_check[bias] = true;

            // put default scale and bias term
            std::vector<float> scale_arr(dim[0]);
            std::fill(scale_arr.begin(), scale_arr.end(), 1.0);
            std::string fileName_weights = "scale_init.f32";
            FILE *fp = fopen(fileName_weights.c_str(), "wb");
            if (fp) {
                fwrite(scale_arr.data(), sizeof(float), dim[0], fp);
                fclose(fp);
            }
            bias = "NULL";

            if (bfuse_scale_layer) {
                // check next node. If scale extract weight and bias paramters for scale layer.
                int bias_term;
                auto& next_node = *std::next(&node);
                auto&& next_output = next_node[3];
                auto&& nn_params = next_node[1];
                std::string nn_layer_name = next_node[3];
                formatFileName(nn_layer_name,"/","_");
                weights = nn_layer_name + "_W";
                std::string dim_weights = next_output + "_W";
                dim = tensorMap[dim_weights];
                if(codeType == "initialize") {
                    ofsCodeC << "    vx_size " << weights << "_dims[1] = { " << dim[0] << " };" << std::endl;
                    ofsCodeC << "    vx_tensor " << weights << ";" << std::endl;
                    ofsCodeC << "    " << weights << " = vxCreateTensor(context,1, " << weights + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << weights << ", dataFolder + \"/weights/" + nn_layer_name + ".f32\"));" << std::endl;
                }
                else if(codeType == "release") {
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << "));" << std::endl;
                }
                declare_tensor_check[weights] = true;

                std::stringstream ss(nn_params);
                ss >> bias_term;
                if(bias_term) {
                    bias = nn_layer_name + "_B";
                    std::string dim_bias = next_output + "_B";
                    dim = tensorMap[dim_bias];
                    if(codeType == "initialize") {
                        ofsCodeC << "    vx_size " << bias << "_dims[1] = { " << dim[0] << " };" << std::endl;
                        ofsCodeC << "    vx_tensor " << bias << ";" << std::endl;
                        ofsCodeC << "    " << bias << " = vxCreateTensor(context,1, " << bias + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                        ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                        ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << bias << ", dataFolder + \"/bias/" + nn_layer_name + ".f32\"));" << std::endl;
                    }
                    else if(codeType == "release") {
                        ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << "));" << std::endl;
                    }
                    declare_tensor_check[bias] = true;
                }
                if(codeType == "initialize") {
                    ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                    ofsCodeC << "    " << layerName + "_node = " << "vxBatchNormalizationLayer(graph, "
                             << inputName +", "
                             << layerName + "_W, "
                             << layerName + "_B, "
                             << weights+", "
                             << bias+", "
                             << layerName + "_eps, "
                             << nn_layer_name << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
                }
                else if(codeType == "release") {
                }
            }
            else{
                weights = layerName +"_W1";
                if(codeType == "initialize") {
                    ofsCodeC << "    vx_size " << weights << "_dims[1] = { " << dim[0] << " };" << std::endl;
                    ofsCodeC << "    vx_tensor " << weights << ";" << std::endl;
                    ofsCodeC << "    " << weights << " = vxCreateTensor(context,1, " << weights + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                    ofsCodeC << "    " << "FILE * " << weights << "_file = fopen(\"scale_init.f32\", " << "\"rb\"" << ");" << std::endl;
                    ofsCodeC << "    " << "if(!" << weights << "_file) { std::cerr << \"ERROR: unable to open the file \" << fileName << std::endl; return nullptr; }" << std::endl;
                    ofsCodeC << "    " << "vx_size  " << weights << "_size =  " << dim[0] << ";" << std::endl;
                    ofsCodeC << "    " << "float * " << weights << "_buf = new float[" << weights + "_size];" << std::endl;
                    ofsCodeC << "    " << "size_t " << weights + "_b_res_size;" << std::endl;
                    ofsCodeC << "    " << weights + "_b_res_size = " <<"fread(" << weights + "_buf,sizeof(float)," << weights + "_size," << weights + "_file);" << std::endl;
                    ofsCodeC << "    " << "if(" + weights + "_b_res_size != " << weights + "_size ) { fclose(" << weights << "_file); return nullptr; " <<  "}" << std::endl;
                    ofsCodeC << "    " << "vx_size " << weights  + "_m_size = 4;" << std::endl;
                    ofsCodeC << "    " << "vx_size " << weights + "_m_stride[1];" << std::endl;
                    ofsCodeC << "    " << "for ( vx_uint32 i=0; i < 1; i++) { " << weights + "_m_stride[i] = " << weights + "_m_size; "
                             << weights + "_m_size *= " << weights + "_dims[i]; } " << std::endl;
                    ofsCodeC << "    " << "vxCopyTensorPatch( " << weights << ", 1, nullptr, nullptr," << weights + "_m_stride, " << weights +"_buf, "
                             << "VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST); " << std::endl;
                    ofsCodeC << "    " << "fclose(" << weights << "_file);" << std::endl;
                    ofsCodeC << "    " << "delete []" << weights + "_buf;" << std::endl;
                }
                else if(codeType == "release") {
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << "));" << std::endl;
                }
                declare_tensor_check[weights] = true;

                if(codeType == "initialize") {
                    ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                    ofsCodeC << "    " << layerName + "_node = " << "vxBatchNormalizationLayer(graph, "
                             << inputName +", "
                             << layerName + "_W, "
                             << layerName + "_B, "
                             << weights+", "
                             << bias+", "
                             << layerName + "_eps, "
                             << layerName << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
                }
                else if(codeType == "release") {
                }
            }
        }
        else if(type == "Eltwise") {
            int op;
            std::stringstream ss(params);
            ss >> op;
            auto&& dim = tensorMap[output];
            for(int i=4; i < node.size(); i++) {
                auto&& idim= tensorMap[node[i]];
                if(dim[0]!= idim[0] || dim[1] != idim[1] || dim[2] != idim[2] || dim[3] != idim[3])
                    error("generateCode : Eltwise op=%d requires same dimension inputs : %s[%dx%dx%dx%d] != %s[%dx%dx%dx%d]\n", op, node[i].c_str(),idim[0], idim[1], idim[2], idim[3], node[i-1].c_str(), dim[0],dim[1],dim[2],dim[3]);
                dim = idim;
            }
            std::string tmp = inputName;
            for(int i=5; i < node.size() ; i++) {
                std::string out = layerName;
                if(i < node.size() - 1) {
                    out += "tmp_"+ std::to_string(i-4);
                    if(codeType == "initialize") {
                        ofsCodeC << "    vx_size " << out << "_dim[4] = { " << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << " };" << std::endl;
                        ofsCodeC << "    vx_tensor " << out << "; " << std::endl;
                        ofsCodeC << "    " << out << "= vxCreateTensor(context,4, " << out + "_dim, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    }
                    declare_tensor_check[out]= true;
                }
                if(op == 1) {
                    if(codeType == "initialize") {
                        ofsCodeC << "    vx_enum " << layerName << "_convertPolicy = " << convertPolicy << ";" << std::endl;
                        ofsCodeC << "    vx_node    " << layerName <<"_node;" << std::endl;
                        ofsCodeC << "    " << layerName + "_node = " << "vxTensorAddNode(graph, " << tmp << ", " << node[i] << ", " << layerName + "_convertPolicy, " << out << ");" << std::endl;
                        ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                        ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
                    }
                    tmp = out;
                }
                else error("generateCode : Eltwise op=%d not supported\n", op);
            }
        }
        else if(type == "Scale") {
            int bias_term;
            std::stringstream ss(params); ss >> bias_term;

            std::string weights = layerName + "_W";
            std::string dim_weights = output + "_W";
            auto&& dim = tensorMap[dim_weights];
            if(codeType == "initialize") {
                ofsCodeC << "    vx_size " << weights << "_dims[1] = { " << dim[0] << " };" << std::endl;
                ofsCodeC << "    vx_tensor " << weights << ";" << std::endl;
                ofsCodeC << "    " << weights << " = vxCreateTensor(context,1, " << weights + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << weights << ", dataFolder + \"/weights/" + layerName + ".f32\"));" << std::endl;
            }
            else if(codeType == "release") {
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << "));" << std::endl;
            }
            declare_tensor_check[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = layerName + "_B";
                std::string dim_bias = output + "_B";
                dim = tensorMap[dim_bias];
                if(codeType == "initialize") {
                    ofsCodeC << "    vx_size " << bias << "_dims[1] = { " << dim[0] << " };" << std::endl;
                    ofsCodeC << "    vx_tensor " << bias << ";" << std::endl;
                    ofsCodeC << "    " << bias << " = vxCreateTensor(context,1, " << bias + "_dims, " << tensorType << ", " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(copyTensor(" << bias << ", dataFolder + \"/bias/" + layerName + ".f32\"));" << std::endl;
                }
                else if(codeType == "release") {
                    ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << "));" << std::endl;
                }
                declare_tensor_check[bias] = true;
            }
            if(codeType == "initialize") {
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxScaleLayer(graph, "
                                   << inputName +", "
                                   << layerName + "_W, "
                                   << bias + ", "
                                   << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
            else if(codeType == "release") {
            }
        }
        else if(type == "Concat") {
            if(codeType == "initialize") {
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " <<  layerName + "_node = " << "vxConcatLayer(graph, ";
                for(int i = 4; i < node.size(); i++) {
                    std::string layerInputs = node[i];
                    formatFileName(layerInputs, "/", "_");
                    ofsCodeC << layerInputs << ", ";
                }
                ofsCodeC << layerName << " );" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "Dropout") {
            //during inference dropout layer propogates input to output .
            if(codeType ==  "initialize") {
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxCopyNode( graph, (vx_reference)" << inputName << ", (vx_reference)" << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "Softmax") {
            if(codeType == "initialize") {
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxSoftmaxLayer(graph, " << inputName << ", " << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "Split") {
            if(codeType == "initialize") {
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxCopyNode( graph, (vx_reference)"<< inputName << ", (vx_reference)" << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        else if(type == "SoftmaxWithLoss") {
            if(codeType == "initialize") {
                ofsCodeC << "    vx_node " << layerName << "_node;" << std::endl;
                ofsCodeC << "    " << layerName + "_node = " << "vxSoftmaxLayer(graph, " << inputName << ", " << layerName << ");" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_OBJECT(" + layerName + "_node);" << std::endl;
                ofsCodeC << "    " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << layerName + "_node));" << std::endl;
            }
        }
        if(codeType== "initialize")
            ofsCodeC << std::endl;
    }
}

void generateCopyTensorCode(std::ostream& ofsCodeC)
{
    ofsCodeC << "static vx_status copyTensor(vx_tensor tensor, std::string fileName, vx_enum usage = VX_WRITE_ONLY)" << std::endl
             << "{" << std::endl
             << "    vx_size num_of_dims = 4, dims[4] = { 1, 1, 1, 1 }, stride[4];" << std::endl
             << "    vxQueryTensor(tensor, VX_TENSOR_NUMBER_OF_DIMS, &num_of_dims, sizeof(num_of_dims));" << std::endl
             << "    vxQueryTensor(tensor, VX_TENSOR_DIMS, &dims, sizeof(dims[0])*num_of_dims);" << std::endl
             << "    vx_size count = dims[0] * dims[1] * dims[2] * dims[3];" << std::endl
             << "    vx_map_id map_id;" << std::endl
             << "    float * ptr;" << std::endl
             << "    vx_status status = vxMapTensorPatch(tensor, num_of_dims, nullptr, nullptr, &map_id, stride, (void **)&ptr, usage, VX_MEMORY_TYPE_HOST, 0);" << std::endl
             << "    if(status) {" << std::endl
             << "        std::cerr << \"ERROR: vxMapTensorPatch() failed for \" << fileName << std::endl;" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    FILE * fp = fopen(fileName.c_str(), usage == VX_WRITE_ONLY ? \"rb\" : \"wb\");" << std::endl
             << "    if(!fp) {" << std::endl
             << "        std::cerr << \"ERROR: unable to open: \" << fileName << std::endl;" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    if(usage == VX_WRITE_ONLY) {" << std::endl
             << "        vx_size n = fread(ptr, sizeof(float), count, fp);" << std::endl
             << "        if(n != count) {" << std::endl
             << "            std::cerr << \"ERROR: expected float[\" << count << \"], but got float[\" << n << \"] in \" << fileName << std::endl;" << std::endl
             << "            return -1;" << std::endl
             << "        }" << std::endl
             << "    }" << std::endl
             << "    else {" << std::endl
             << "        fwrite(ptr, sizeof(float), count, fp);" << std::endl
             << "    }" << std::endl
             << "    fclose(fp);" << std::endl
             << "    status = vxUnmapTensorPatch(tensor, map_id);" << std::endl
             << "    if(status) {" << std::endl
             << "        std::cerr << \"ERROR: vxUnmapTensorPatch() failed for \" << fileName << std::endl;" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    return 0;" << std::endl
             << "}" << std::endl << std::endl;
}

void generateCode(
    std::ostream& ofsCodeH,
    std::ostream& ofsCodeC,
    std::ofstream& ofsCodeM,
    std::ofstream& ofsCodeA,
    std::ofstream& ofsCodeD,
    std::vector<std::vector<std::string>>& net,
    std::map<std::string,std::vector<int>>& tensorMap,
    std::string tensorType,
    int fixedPointPosition,
    std::string convertPolicy,
    std::string roundPolicy,
    bool isVirtualEnabled,
    std::string outputFolder,
    bool bFuseScaleLayer)
{
    ////
    // generate .h file
    //
    ofsCodeH << "#ifndef annmodule_h" <<  std::endl
             << "#define annmodule_h" <<  std::endl
             <<                         std::endl
             << "#include <VX/vx.h>" << std::endl
             <<                         std::endl
             << "extern \"C\" {"     << std::endl
             << "    VX_API_ENTRY void     VX_API_CALL annGetTensorDimensions(vx_size dimInput[4], vx_size dimOutput[4]);" << std::endl
             << "    VX_API_ENTRY vx_graph VX_API_CALL annCreateGraph(vx_context context, vx_tensor input, vx_tensor output, const char * options);" << std::endl
             << "};"                 << std::endl
             <<                         std::endl
             << "#endif" <<  std::endl;

    ////
    // generate .cpp file
    //
    ofsCodeC << "#include \"annmodule.h\"" << std::endl << std::endl;
    ofsCodeC << "#include <vx_ext_amd.h>" << std::endl;
    ofsCodeC << "#include <VX/vx_khr_nn.h>" << std::endl;
    ofsCodeC << "#include <vx_amd_nn.h>" << std::endl<< std::endl;
    ofsCodeC << "#include <iostream>" << std::endl;
    ofsCodeC << "#include <stdio.h>" << std::endl;
    ofsCodeC << "#include <stdlib.h>" << std::endl << std::endl;

    ofsCodeC << "#define ERROR_CHECK_STATUS(call) { vx_status status = (call); if(status != VX_SUCCESS) { vxAddLogEntry(NULL, status, \"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\", status, __LINE__); return nullptr; } }" << std::endl;
    ofsCodeC << "#define ERROR_CHECK_OBJECT(obj) { vx_status status = vxGetStatus((vx_reference)(obj)); if(status != VX_SUCCESS) { vxAddLogEntry((vx_reference)(obj), status, \"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\", status, __LINE__); return nullptr; } }" << std::endl << std::endl;

    generateCopyTensorCode(ofsCodeC);

    auto&& input = net[0][4];
    auto&& output = net[net.size()-1][3];
    auto&& idim = tensorMap[input];
    auto&& odim = tensorMap[output];
    ofsCodeC << "VX_API_ENTRY void VX_API_CALL annGetTensorDimensions(vx_size dimInput[4], vx_size dimOutput[4])" << std::endl
             << "{" << std::endl
             << "    dimInput[0] = " << idim[3] << ";" << std::endl
             << "    dimInput[1] = " << idim[2] << ";" << std::endl
             << "    dimInput[2] = " << idim[1] << ";" << std::endl
             << "    dimInput[3] = " << idim[0] << ";" << std::endl
             << "    dimOutput[0] = " << odim[3] << ";" << std::endl
             << "    dimOutput[1] = " << odim[2] << ";" << std::endl
             << "    dimOutput[2] = " << odim[1] << ";" << std::endl
             << "    dimOutput[3] = " << odim[0] << ";" << std::endl
             << "}" << std::endl << std::endl;

    ofsCodeC << "VX_API_ENTRY vx_graph VX_API_CALL annCreateGraph(vx_context context, vx_tensor " << input << ", vx_tensor " << output << ", const char * dataFolder_)" << std::endl;
    ofsCodeC << "{" << std::endl;
    ofsCodeC << "    // load neural network extension kernels" << std::endl;
    ofsCodeC << "    ERROR_CHECK_STATUS(vxLoadKernels(context,\"vx_nn\"));" << std::endl;
    ofsCodeC << std::endl;
    ofsCodeC << "    // create graph" << std::endl;
    ofsCodeC << "    vx_graph graph = vxCreateGraph(context); " << std::endl;
    ofsCodeC << "    ERROR_CHECK_OBJECT(graph);" << std::endl;
    ofsCodeC << std::endl;
    ofsCodeC << "    // get dataFolder option" << std::endl;
    ofsCodeC << "    std::string dataFolder = dataFolder_ ? dataFolder_ : \".\", fileName;" << std::endl;
    ofsCodeC << std::endl;
    ofsCodeC << "    ////" << std::endl;
    ofsCodeC << "    // initialize the graph" << std::endl;
    writeVXCode(ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, bFuseScaleLayer, "initialize");
    ofsCodeC << "    ////" << std::endl;
    ofsCodeC << "    // release intermediate objects" << std::endl;
    writeVXCode(ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, bFuseScaleLayer, "release");
    ofsCodeC << std::endl;
    ofsCodeC << "    ////" << std::endl;
    ofsCodeC << "    // verify the built graph" << std::endl;
    ofsCodeC << "    ERROR_CHECK_STATUS(vxVerifyGraph(graph));" << std::endl;
    ofsCodeC << std::endl;
    ofsCodeC << "    return graph;" << std::endl;
    ofsCodeC << "}" << std::endl;

    /////
    // generate CMakeLists.txt
    //
    ofsCodeM << "cmake_minimum_required (VERSION 2.8)" << std::endl;
    ofsCodeM << "project (annmodule)" << std::endl;
    ofsCodeM << "set (CMAKE_CXX_STANDARD 11)" << std::endl;
    ofsCodeM << "list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)" << std::endl;
    ofsCodeM << "find_package(OpenCL     REQUIRED)" << std::endl;
    ofsCodeM << "include_directories (${OpenCL_INCLUDE_DIRS} ${OpenCL_INCLUDE_DIRS}/Headers )" << std::endl;
    ofsCodeM << "include_directories (/opt/rocm/include)" << std::endl;
    ofsCodeM << "link_directories    (/opt/rocm/lib)" << std::endl;
    ofsCodeM << "list(APPEND SOURCES annmodule.cpp)" << std::endl;
    ofsCodeM << "add_library(${PROJECT_NAME} SHARED ${SOURCES})" << std::endl;
    ofsCodeM << "set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -msse4.2 -std=c++11\")" << std::endl;
    ofsCodeM << "target_link_libraries(${PROJECT_NAME} openvx vx_nn pthread)" << std::endl;
    ofsCodeM << "add_executable(annunit annunit.cpp)" << std::endl;
    ofsCodeM << "target_link_libraries(annunit openvx vx_nn pthread ${PROJECT_NAME})" << std::endl;

    /////
    // generate simple application
    //
    ofsCodeA << "#include \"annmodule.h\"" << std::endl ;
    ofsCodeA << "#include <vx_ext_amd.h>" << std::endl;
    ofsCodeA << "#include <iostream>" << std::endl;
    ofsCodeA << "#include <stdio.h>" << std::endl;
    ofsCodeA << "#include <string>" << std::endl;
    ofsCodeA << std::endl;

    generateCopyTensorCode(ofsCodeA);

    ofsCodeA << "int main(int argc , char ** argv)" << std::endl;
    ofsCodeA << "{" << std::endl;
    ofsCodeA << "    // get module configuration" << std::endl;
    ofsCodeA << "    vx_size dimInput[4] = { 0 }, dimOutput[4] = { 0 };" << std::endl;
    ofsCodeA << "    annGetTensorDimensions(dimInput, dimOutput);" << std::endl;
    ofsCodeA << "    printf(\"OK: annGetTensorDimensions() => [input %ldx%ldx%ldx%ld] [output %ldx%ldx%ldx%ld]\\n\", dimInput[0], dimInput[1], dimInput[2], dimInput[3], dimOutput[0], dimOutput[1], dimOutput[2], dimOutput[3]);" << std::endl;
    ofsCodeA << std::endl;
    ofsCodeA << "    // create context, input, output, and graph" << std::endl;
    ofsCodeA << "    vx_context context = vxCreateContext();" << std::endl;
    ofsCodeA << "    if(vxGetStatus((vx_reference)context)) {" << std::endl;
    ofsCodeA << "        printf(\"ERROR: vxCreateContext() failed\\n\");" << std::endl;
    ofsCodeA << "        return -1;" << std::endl;
    ofsCodeA << "    }" << std::endl;
    ofsCodeA << "    vx_tensor input = vxCreateTensor(context, 4, dimInput, VX_TYPE_FLOAT32, 0);" << std::endl;
    ofsCodeA << "    if(vxGetStatus((vx_reference)input)) {" << std::endl;
    ofsCodeA << "        printf(\"ERROR: vxCreateTensor(input,4,{%ld,%ld,%ld,%ld}) failed\\n\", dimInput[0], dimInput[1], dimInput[2], dimInput[3]);" << std::endl;
    ofsCodeA << "        return -1;" << std::endl;
    ofsCodeA << "    }" << std::endl;
    ofsCodeA << "    vx_tensor output = vxCreateTensor(context, 4, dimOutput, VX_TYPE_FLOAT32, 0);" << std::endl;
    ofsCodeA << "    if(vxGetStatus((vx_reference)output)) {" << std::endl;
    ofsCodeA << "        printf(\"ERROR: vxCreateTensor(output,4,{%ld,%ld,%ld,%ld}) failed\\n\", dimOutput[0], dimOutput[1], dimOutput[2], dimOutput[3]);" << std::endl;
    ofsCodeA << "        return -1;" << std::endl;
    ofsCodeA << "    }" << std::endl;
    ofsCodeA << std::endl;
    ofsCodeA << "    // build graph from the module" << std::endl;
    ofsCodeA << "    vx_graph graph = annCreateGraph(context, input, output, argc > 1 ? argv[1] : nullptr);" << std::endl;
    ofsCodeA << "    if(vxGetStatus((vx_reference)graph)) {" << std::endl;
    ofsCodeA << "        printf(\"ERROR: annCreateGraph(...,%s) failed\\n\", argv[1]);" << std::endl;
    ofsCodeA << "        return -1;" << std::endl;
    ofsCodeA << "    }" << std::endl;
    ofsCodeA << std::endl;
    ofsCodeA << "    if(argc > 2 && copyTensor(input, argv[2], VX_WRITE_ONLY) < 0) {" << std::endl;
    ofsCodeA << "        return -1;" << std::endl;
    ofsCodeA << "    }" << std::endl;
    ofsCodeA << std::endl;
    ofsCodeA << "    vx_status status = vxProcessGraph(graph);" << std::endl;
    ofsCodeA << "    if(status != VX_SUCCESS) {" << std::endl;
    ofsCodeA << "        printf(\"ERROR: vxProcessGraph() failed (%d)\\n\", status);" << std::endl;
    ofsCodeA << "        return -1;" << std::endl;
    ofsCodeA << "    }" << std::endl;
    ofsCodeA << std::endl;
    ofsCodeA << "    if(argc > 3 && copyTensor(output, argv[3], VX_READ_ONLY) < 0) {" << std::endl;
    ofsCodeA << "        return -1;" << std::endl;
    ofsCodeA << "    }" << std::endl;
    ofsCodeA << std::endl;
    ofsCodeA << "    // release resources" << std::endl;
    ofsCodeA << "    vxReleaseGraph(&graph);" << std::endl;
    ofsCodeA << "    vxReleaseTensor(&input);" << std::endl;
    ofsCodeA << "    vxReleaseTensor(&output);" << std::endl;
    ofsCodeA << "    vxReleaseContext(&context);" << std::endl;
    ofsCodeA << std::endl;
    ofsCodeA << "    return 0;"<< std::endl;
    ofsCodeA << "}" << std::endl;
   
    ofsCodeD << "find_path(OPENCL_INCLUDE_DIRS"  << std::endl;
    ofsCodeD << "NAMES OpenCL/cl.h CL/cl.h" << std::endl;
    ofsCodeD << "HINTS" << std::endl;
    ofsCodeD << "${OPENCL_ROOT}/include" << std::endl;
    ofsCodeD << "$ENV{AMDAPPSDKROOT}/include" << std::endl;
    ofsCodeD << "PATHS" << std::endl;
    ofsCodeD << "/usr/include" << std::endl;
    ofsCodeD << "/usr/local/include" << std::endl;
    ofsCodeD << "/opt/rocm/opencl/include" << std::endl;
    ofsCodeD << "DOC \"OpenCL header file path\"" << std::endl;
    ofsCodeD << ")" << std::endl;
    ofsCodeD << "mark_as_advanced( OPENCL_INCLUDE_DIRS )" << std::endl << std::endl;
    ofsCodeD << "if(\"${CMAKE_SIZEOF_VOID_P}\" EQUAL \"8\")" << std::endl;
    ofsCodeD << "   find_library( OPENCL_LIBRARIES" << std::endl;
    ofsCodeD << "       NAMES OpenCL" << std::endl;
    ofsCodeD << "       HINTS" << std::endl;
    ofsCodeD << "       ${OPENCL_ROOT}/lib" << std::endl;
    ofsCodeD << "       $ENV{AMDAPPSDKROOT}/lib" << std::endl;
    ofsCodeD << "       DOC \"OpenCL dynamic library path\"" << std::endl;
    ofsCodeD << "       PATH_SUFFIXES x86_64 x64 x86_64/sdk" << std::endl;
    ofsCodeD << "       PATHS" << std::endl;
    ofsCodeD << "       /usr/lib" << std::endl;
    ofsCodeD << "       /opt/rocm/opencl/lib" << std::endl;
    ofsCodeD << "       )" << std::endl;
    ofsCodeD << "else( )" << std::endl;
    ofsCodeD << "   find_library( OPENCL_LIBRARIES" << std::endl;
    ofsCodeD << "       NAMES OpenCL" << std::endl;
    ofsCodeD << "       HINTS" << std::endl;
    ofsCodeD << "       ${OPENCL_ROOT}/lib" << std::endl;
    ofsCodeD << "       $ENV{AMDAPPSDKROOT}/lib" << std::endl;
    ofsCodeD << "       DOC \"OpenCL dynamic library path\"" << std::endl;
    ofsCodeD << "       PATH_SUFFIXES x86 Win32" << std::endl;
    ofsCodeD << "       PATHS" << std::endl;
    ofsCodeD << "       /usr/lib" << std::endl;
    ofsCodeD << "       )" << std::endl;
    ofsCodeD << "endif( )" << std::endl;
    ofsCodeD << "mark_as_advanced( OPENCL_LIBRARIES )" << std::endl << std::endl;
    ofsCodeD << "include( FindPackageHandleStandardArgs )" << std::endl;
    ofsCodeD << "find_package_handle_standard_args( OPENCL DEFAULT_MSG OPENCL_LIBRARIES OPENCL_INCLUDE_DIRS )" << std::endl;
    ofsCodeD << "set(OpenCL_FOUND ${OPENCL_FOUND} CACHE INTERNAL \"\")" << std::endl;
    ofsCodeD << "set(OpenCL_LIBRARIES ${OPENCL_LIBRARIES} CACHE INTERNAL \"\")" << std::endl;
    ofsCodeD << "set(OpenCL_INCLUDE_DIRS ${OPENCL_INCLUDE_DIRS} CACHE INTERNAL \"\")" << std::endl;
    ofsCodeD << "if( NOT OPENCL_FOUND )" << std::endl;
    ofsCodeD << "   message( STATUS \"FindOpenCL looked for libraries named: OpenCL\" )" << std::endl;
    ofsCodeD << "endif()" << std::endl;
}

void parseCaffeModel(const caffe::NetParameter& net_parameter, std::vector<std::vector<std::string>>& net, int inputDim[4], std::string outputFolder, int flags)
{
    if(net_parameter.has_name())
        std::cout<<"Fetching the weights for : " << net_parameter.name()<< std::endl;

    std::map<std::string,std::string> outputNameMap, splitNameMap;
    if(net_parameter.input_size() > 0) {
        outputNameMap[net_parameter.input(0)] = net_parameter.input(0);
    }

    if(net_parameter.input_dim_size()==4 && ((inputDim[0]==0) || (inputDim[1]==0) || (inputDim[2]==0) || (inputDim[3]==0)))
    {
        inputDim[0] = net_parameter.input_dim(0);
        inputDim[1] = net_parameter.input_dim(1);
        inputDim[2] = net_parameter.input_dim(2);
        inputDim[3] = net_parameter.input_dim(3);
    }

    //extract layer information.
    for(int i=0; i < net_parameter.layer_size() ;i++)
    {
        const caffe::LayerParameter& layer_parameter = net_parameter.layer(i);

        if(layer_parameter.top_size() == 0)
            continue;

        //Check layer name.
        if(layer_parameter.type() == "Input" || layer_parameter.type() == "Data" || layer_parameter.type() == "ImageData" ) {
            outputNameMap[layer_parameter.top(0)]= layer_parameter.top(0);
            if(layer_parameter.type() == "Input"  && ((inputDim[0]==0) || (inputDim[1]==0) || (inputDim[2]==0) || (inputDim[3]==0))) {
                inputDim[0] = layer_parameter.input_param().shape(0).dim(0);
                inputDim[1] = layer_parameter.input_param().shape(0).dim(1);
                inputDim[2] = layer_parameter.input_param().shape(0).dim(2);
                inputDim[3] = layer_parameter.input_param().shape(0).dim(3);
            }
            continue;
        }

        //dump layer data.
        dumpLayerData(layer_parameter, outputFolder);

        // enable Split optimization using a bit in flags (i.e., remove Split by using variable renaming instead of a copy)
        bool isSplitEnabled = (flags & 1);
        if(!isSplitEnabled) {
            if(layer_parameter.type()=="Split") {
                for(int j=0; j< layer_parameter.top_size() ; j++ ) {
                    // get layer information and add to net
                    std::vector<std::string> node;
                    node.push_back(layer_parameter.type());
                    node.push_back("");
                    node.push_back(layer_parameter.top(j));
                    node.push_back(layer_parameter.top(j));
                    for(int z = 0; z < layer_parameter.bottom_size();z++) {
                        if(outputNameMap.find(layer_parameter.bottom(z)) == outputNameMap.end()) {
                            outputNameMap[layer_parameter.bottom(z)] = layer_parameter.bottom(z);
                        }
                        node.push_back(outputNameMap[layer_parameter.bottom(z)]);
                    }
                    net.push_back(node);
                    // update output name with layer name
                    outputNameMap[layer_parameter.top(j)] = layer_parameter.top(j);
                }
                continue;
            }
        }
        else
        {
            //Split type.
            if(layer_parameter.type()=="Split") {
                splitNameMap[layer_parameter.name()]= layer_parameter.bottom(0);
                for(int j=0; j< layer_parameter.top_size() ; j++ ) {
                    splitNameMap[layer_parameter.top(j)] = layer_parameter.bottom(0);
                }
                continue;
            }
        }

        // get layer information and add to net
        std::vector<std::string> node;
        std::string params;
        getLayerParams(layer_parameter, params);
        node.push_back(layer_parameter.type());
        node.push_back(params);
        node.push_back(layer_parameter.top(0));
        node.push_back(layer_parameter.name());
        for(int j = 0; j < layer_parameter.bottom_size()  ; j++) {
            if(isSplitEnabled && (strstr(layer_parameter.bottom(j).c_str(),"split"))) {
                outputNameMap[layer_parameter.bottom(j)]= splitNameMap[layer_parameter.bottom(j)];
            }
            if(outputNameMap.find(layer_parameter.bottom(j)) == outputNameMap.end()) {
                outputNameMap[layer_parameter.bottom(j)] = layer_parameter.bottom(j);
            }
            node.push_back(outputNameMap[layer_parameter.bottom(j)]);
        }
        net.push_back(node);
        // update output name with layer name
        outputNameMap[layer_parameter.top(0)] = layer_parameter.name();
    }
}

int loadCaffeModelFile(
    const char* fileName,
    std::vector<std::vector<std::string>>& net,
    int inputDim[4],
    std::string outputFolder,
    int flags)
{
    //verify the version of protobuf library.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    //read the caffemodel.
    caffe::NetParameter net_parameter;
    std:: cout<<"Reading the binary file from : "<< fileName<< std::endl;
    std::fstream input(fileName, std::ios::in| std::ios::binary);
    bool isSuccess = net_parameter.ParseFromIstream(&input);
    if(isSuccess) {
        std::cout << "CaffeModel Read Successful" << std::endl;
        int layer_param_size = net_parameter.layer_size();
        if(layer_param_size > 0) {
            parseCaffeModel(net_parameter, net, inputDim, outputFolder, flags);
        }
        else {
            std::cerr << "ERROR: [Unsupported caffemodel] please upgrade this caffemodel, currently uses deprecated V1LayerParameters." << std::endl;
            return -1;
        }
    }
    else {
        std::cerr << "CaffeModel Read Failed" << std::endl;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    const char * usage =
            "Usage:\n"
            "  % inference_generator [options] <net.prototxt|net.caffemodel> [n c H W [type fixed-point-position [convert-policy round-policy]]]\n"
            "    options:\n"
            "      --[no-]virtual-buffers    - do/don't use virtual buffers (default: ON)\n"
            "      --[no-]generate-gdf       - do/don't generate RunVX GDF with weight/bias initialization (default: ON)\n"
            "      --[no-]generate-vx-code   - do/don't generate OpenVX C Code with weight/bias initialization (default: ON)\n"
            "      --output-dir <folder>     - specify output folder for weights/biases, GDF, and OpenVX C Code (default: current)\n"
            "      --flags <int>             - specify custom flags (default: 0)\n"
            ;

    // get options
    bool isVirtualEnabled = true;
    bool generateGDF = true;
    bool generateVXC = true;
    bool bFuseScaleWithBatchNorm = true;
    std::string outputFolder = ".";
    int flags = 0;
    for(; argc > 1 && argv[1][0] == '-'; argc--, argv++) {
        if(!strcmp(argv[1], "--virtual-buffers")) {
            isVirtualEnabled = true;
        }
        else if(!strcmp(argv[1], "--no-virtual-buffers")) {
            isVirtualEnabled = false;
        }
        else if(!strcmp(argv[1], "--generate-gdf")) {
            generateGDF = true;
        }
        else if(!strcmp(argv[1], "--no-generate-gdf")) {
            generateGDF = false;
        }
        else if(!strcmp(argv[1], "--generate-vx-code")) {
            generateVXC = true;
        }
        else if(!strcmp(argv[1], "--no-generate-vx-code")) {
            generateVXC = false;
        }
        else if(!strcmp(argv[1], "--output-dir") && argc > 2) {
            outputFolder = argv[2];
            argc--;
            argv++;
            mkdir(outputFolder.c_str(), 0777);
        }
        else if(!strcmp(argv[1], "--flags") && argc > 2) {
            flags = atoi(argv[2]);
            argc--;
            argv++;
        }
        else {
            printf("ERROR: invalid option: %s\n", argv[1]);
            return -1;
        }
    }

    // check for command-line arguments
    if(argc < 2) {
        printf("%s", usage);
        return -1;
    }

    // get command-line arguments
    int inputDim[4] = { 0, 0, 0, 0 }, fixedPointPosition = 0;
    const char * tensorType = "VX_TYPE_FLOAT32";
    const char * convertPolicy = "VX_CONVERT_POLICY_SATURATE";
    const char * roundPolicy = "VX_ROUND_POLICY_TO_NEAREST_EVEN";
    const char *  fileName = argv[1];
    if(argc > 2) inputDim[0] = atoi(argv[2]);
    if(argc > 3) inputDim[1] = atoi(argv[3]);
    if(argc > 4) inputDim[2] = atoi(argv[4]);
    if(argc > 5) inputDim[3] = atoi(argv[5]);
    if(argc > 6) tensorType = argv[6];
    if(argc > 7) fixedPointPosition = atoi(argv[7]);
    if(argc > 8) convertPolicy = argv[8];
    if(argc > 9) roundPolicy = argv[9];
    std::vector<std::vector<std::string>> net;

    flags &= 3;     // we are only interersted in LSBs 0 & 1
    bFuseScaleWithBatchNorm = !((flags & 2) >> 1);

    // load caffe model (or just .prototxt)
    if(strstr(fileName,".caffemodel")) {
        // make sure that weights and bias folder are created
        std::string dir = outputFolder + "/weights";
        mkdir(dir.c_str(), 0777);
        dir = outputFolder + "/bias";
        mkdir(dir.c_str(), 0777);
        // load caffe model
        if(loadCaffeModelFile(fileName, net, inputDim, outputFolder, flags) < 0) {
            return -1;
        }
    }
    else if(strstr(fileName,".prototxt")) {
        if(loadCaffeProtoTxt(fileName, net, inputDim) < 0) {
            return -1;
        }
    }
    else {
        printf("%s", usage);
        return -1;
    }

    // generate tensorMap for given input dimensions
    std::map<std::string,std::vector<int>> tensorMap;
    if(calculateTensorDim(net, inputDim, tensorMap) < 0) {
        return -1;
    }

    if(generateGDF) {
        std::ofstream ofsGDF(outputFolder + "/net.gdf", std::ios::binary);
        writeGDF(ofsGDF, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, outputFolder, bFuseScaleWithBatchNorm);
    }

    if(generateVXC) {
        std::ofstream ofsCodeH(outputFolder + "/annmodule.h", std::ios::binary);
        std::ofstream ofsCodeC(outputFolder + "/annmodule.cpp", std::ios::binary);
        std::ofstream ofsCodeM(outputFolder + "/CMakeLists.txt", std::ios::binary);
        std::ofstream ofsCodeA(outputFolder + "/annunit.cpp", std::ios::binary);
        std::string dir = outputFolder + "/cmake";
        mkdir(dir.c_str(), 0777);
        std::ofstream ofsCodeD(dir + "/FindOpenCL.cmake", std::ios::binary);
        generateCode(ofsCodeH, ofsCodeC, ofsCodeM, ofsCodeA, ofsCodeD, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, outputFolder, bFuseScaleWithBatchNorm);
    }

    return 0;
}
