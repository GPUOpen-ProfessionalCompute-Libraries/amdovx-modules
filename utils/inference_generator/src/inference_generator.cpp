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
    else if(layer.type() == "Pooling") {
        const caffe::PoolingParameter& pooling = layer.pooling_param();
        int pad_h = pooling.has_pad_h() ? pooling.pad_h() : pooling.pad();
        int pad_w = pooling.has_pad_w() ? pooling.pad_w() : pooling.pad();
        int stride_h = pooling.has_stride_h() ? pooling.stride_h() : pooling.stride();
        int stride_w = pooling.has_stride_w() ? pooling.stride_w() : pooling.stride();
        int kernel_h = pooling.has_kernel_h() ? pooling.kernel_h() : pooling.kernel_size();
        int kernel_w = pooling.has_kernel_w() ? pooling.kernel_w() : pooling.kernel_size();
        int pool = pooling.pool();
        params =       std::to_string(kernel_w)
                + " " + std::to_string(kernel_h)
                + " " + std::to_string(stride_w)
                + " " + std::to_string(stride_h)
                + " " + std::to_string(pad_w)
                + " " + std::to_string(pad_h)
                + " " + std::to_string(pool);
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
        // discard if layer is input/data or specific to TEST
        caffe::Phase phase = layer.phase();
        for(int j = 0; j < layer.include_size(); j++) {
            if(layer.include(j).phase())
                phase = layer.include(j).phase();
        }
        if(phase == caffe::TEST) {
            continue;
        }
        else if(layer.type() == "Input" || layer.type() == "Data" || layer.type() == "ImageData") {
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
        if(layer.type()=="Split"){
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
            int kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, pool;
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> pool;
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
    bool isVirtualEnabled)
{
    std::map<std::string,bool> tensorCheck;
    ofsGDF << "import vx_nn" << std::endl;
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
                    if(!isLastLayer){
                        ofsGDF << "read data input.f32" << std::endl;
                    }
                }else {
                    if(isVirtualEnabled) {
                        ofsGDF << "data " << node[i] << " = virtual-tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                        tensorCheck[node[i]] = true;
                    }else{
                        ofsGDF << "data " << node[i] << " = tensor:4,{" << dim[3] << "," << dim[2] << "," << dim[1] << "," << dim[0] << "}," << tensorType << "," << fixedPointPosition << std::endl;
                        tensorCheck[node[i]]= true;
                        if(isFirstLayer) ofsGDF << "read data input.f32" << std::endl;
                    }
                }
            }
        }
        auto&& output = node[3];
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

            ofsGDF << "data " << node[3] << "_params = " << " scalar:VX_TYPE_NN_CONV_PARAMS,{" << pad_w << "," << pad_h << "," << convertPolicy << "," << roundPolicy << ",VX_NN_DS_SIZE_ROUNDING_FLOOR," << dilation_w-1 << "," << dilation_h-1 << "}" << std::endl;
            ofsGDF << "node org.khronos.nn_extension.convolution_layer " << node[4] << " " << node[3] << "_W" << " " << bias << " "
                   << node[3] <<"_params"
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if (type == "Deconvolution"){
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
            }else{
                bias = output + "_B";
                ofsGDF << "data " << bias << " = tensor:1,{" << k << "}," << tensorType << "," << fixedPointPosition << std::endl;
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
            int use_global_stats;
            float eps, beta = 0, gamma = 1;
            std::stringstream ss(params);
            ss >> eps >> use_global_stats;
            ofsGDF << "data " << node[3] <<"_eps =" << " scalar:VX_TYPE_FLOAT32," << eps << std::endl;
            ofsGDF << "data " << node[3] <<"_beta =" << " scalar:VX_TYPE_FLOAT32," << beta << std::endl;
            ofsGDF << "data " << node[3] <<"_gamma ="<< " scalar:VX_TYPE_FLOAT32," << gamma << std::endl;
            ofsGDF << "node com.amd.nn_extension.batch_normalization_layer " << node[4] << " "
                   << node[3] << "_gamma "
                   << node[3] << "_beta "
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
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
            float alpha = 1, beta = 0;
            ofsGDF << "data " << node[3] <<"_alpha = " << " scalar:VX_TYPE_FLOAT32," << alpha << std::endl;
            ofsGDF << "data " << node[3] <<"_beta = " << " scalar:VX_TYPE_FLOAT32," << beta << std::endl;
            ofsGDF << "node org.khronos.nn_extension.scale_layer " << node[4]
                   << node[3] << "_alpha "
                   << node[3] << "_beta"
                   << " " << node[3]
                   << std::endl;
#if ENABLE_DUMP_LAYER_DATA
            ofsGDF << "write "<< node[3] << " out/"<< layer_name << ".f32" << std::endl;
#endif
        }
        else if(type == "Concat") {
            ofsGDF << "node com.amd.nn_extension.concat_layer" ;
            for(int i = 4; i < node.size(); i++) {
                ofsGDF << " " << node[i];
            }
            ofsGDF << " " << node[3] << std::endl;
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
        }
        ofsGDF << std::endl;
    }
}

void dumpLayerData(const caffe::LayerParameter& layer_parameter, std::string outputFolder)
{
    std:: string layer_name;
    if(layer_parameter.has_name()){
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

        for(int i=0;i<weightsize;i++){
            float weight = weights_blob.data(i);
            fwrite(&weight,sizeof(float),1,fs_weights);
        }
        //Extraction of bias if exists.
        if(blob_size >= 2){
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
    std::ostream& ofsCodeH,
    std::ostream& ofsCodeC,
    std::vector<std::vector<std::string>>& net,
    std::map<std::string,std::vector<int>>& tensorMap,
    std::string tensorType,
    int fixedPosition,
    std::string convertPolicy,
    std::string roundPolicy,
    bool isVirtualEnabled,
    std::string codeType)
{
    if(codeType == "declaration") {
        ofsCodeH << "	vx_context context; " << std::endl;
        ofsCodeH << "	vx_graph graph; " << std::endl;
    }
    else if(codeType == "initialize") {
        ofsCodeC << "	context = vxCreateContext(); " << std::endl;
        ofsCodeC << "   ERROR_CHECK_OBJECT(context);" << std::endl;
        ofsCodeC << "	graph = vxCreateGraph(context); " << std::endl;
        ofsCodeC << "   ERROR_CHECK_OBJECT(graph);" << std::endl;
    }
    else if(codeType == "release_graph") {
        ofsCodeC << "   ERROR_CHECK_STATUS(vxReleaseGraph(&graph));" << std::endl;
    }
    else if(codeType == "release_context") {
        ofsCodeC << "   ERROR_CHECK_STATUS(vxReleaseContext(&context));" << std::endl;
    }
    std::map<std::string,bool> declare_tensor_check;
    for(auto& node : net) {
        //declare input tensors.
        bool isFirstLayer = (&node == &net.front());
        bool isLastLayer = (&node == &net.back());
        for(size_t i=4; i < node.size(); i++) {
            if(node[i] != "" && declare_tensor_check.find(node[i]) == declare_tensor_check.end()) {
                auto&& dim = tensorMap[node[i]];
                if(codeType == "declaration") {
                    ofsCodeH << "	vx_size " << node[i] << "_dims[4];" << std::endl;
                    ofsCodeH << "	vx_tensor " << node[i] << " ;" << std::endl;
                }
                else if(codeType == "constructor") {
                    ofsCodeC << "	" << node[i] + "_dims  {" << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << "}," << std::endl;
                }
                else if(codeType == "initialize") {
                    ofsCodeC << "	" << node[i] << " = vxCreateTensor(context, 4, " << node[i] + "_dims, VX_TYPE_FLOAT32," << fixedPosition << ");" << std::endl;
                    ofsCodeC << "   " << "ERROR_CHECK_OBJECT("  << node[i] << ");" << std::endl;
                }
                else if(codeType == "run") {
                    if(isFirstLayer) {
                        ofsCodeC << "   " << "vx_size " << node[i] << "_m_size = 4;" << std::endl;
                        ofsCodeC << "   " << "vx_size " << node[i] << "_m_stride[4];" << std::endl;
                        ofsCodeC << "   " << "for (vx_uint32 i=0; i < 4 ; i++ ) { " << node[i] << "_m_stride[i] = " << node[i] << "_m_size;" << node[i] +"_m_size *= " << node[i] + "_dims[i]; }" << std::endl;
                        ofsCodeC << "   " << "vxCopyTensorPatch (" << node[i] << ", 4, nullptr, nullptr, " << node[i] << "_m_stride, inputTensor, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST );" << std::endl;
                        ofsCodeC << "   " << std::endl;
                        ofsCodeC << "   " << "vxProcessGraph(graph);" << std::endl;
                        ofsCodeC << "   " << std::endl;
                    }

                }
                else if(codeType == "release_tensors") {
                    ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << node[i] << " ));" << std::endl;
                }
                declare_tensor_check[node[i]]= true;
            }
        }
        //declare output tensor.
        auto&& output = node[3];
        auto&& odim = tensorMap[output];
        if(!declare_tensor_check[output]) {
            if(codeType == "declaration") {
                ofsCodeH << "	vx_size " << output << "_dims[4];" << std::endl;
                ofsCodeH << "	vx_tensor " << output << " ;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << output + "_dims  {" << odim[3] << ", " << odim[2] << ", " << odim[1] << ", " << odim[0] << "}," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output << " = vxCreateTensor(context,4, " << output + "_dims, VX_TYPE_FLOAT32," << fixedPosition << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT("  << output << ");" << std::endl;
            }
            else if(codeType == "release_tensors") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << output << " ));" << std::endl;
            }
            declare_tensor_check[output] = true;
        }

        auto&& type = node[0];
        auto&& params = node[1];
        std::string layer_name = node[3];
        formatFileName(layer_name,"/","_");
        if(type == "Convolution") {
            std::stringstream ss(params);
            int k, kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term;
            std::string weights = output + "_W";
            auto&& dim = tensorMap[weights];
            if(codeType == "declaration") {
                ofsCodeH << "	vx_size " << weights << "_dims[4];" << std::endl;
                ofsCodeH << "	vx_tensor " << weights << " ;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << weights + "_dims {" << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << "}," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << weights << " = vxCreateTensor(context,4, " << weights + "_dims, VX_TYPE_FLOAT32," << fixedPosition << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "   " << "fileName = str + " << "\"/weights/" + layer_name + ".f32\";" << std::endl;
                ofsCodeC << "	" << "FILE * " << weights << "_file = fopen(fileName.c_str(), " << "\"rb\"" << ");" << std::endl;
                ofsCodeC << "	" << "if(!" << weights << "_file) { std::cerr << \"ERROR: unable to open the file \" << fileName << std::endl; }" << std::endl;
                ofsCodeC << "	" << "vx_size  " << weights << "_size =  " << 4 * dim[3] * dim[2] * dim[1] * dim[0] << ";" << std::endl;
                ofsCodeC << "	" << "float * " << weights << "_buf = new float[" << weights + "_size];" << std::endl;
                ofsCodeC << "	" << "size_t " << weights + "_res_size;" << std::endl;
                ofsCodeC << "	" << weights + "_res_size = " << "fread(" << weights + "_buf,1," << weights + "_size," << weights + "_file);" << std::endl;
                ofsCodeC << "	" << "if(" + weights + "_res_size != " << weights + "_size ) { std::cerr << \"ERROR: read error in : \" << fileName << std::endl; }"<< std::endl;
                ofsCodeC << "	" << "vx_size " << weights  + "_m_size = 4;" << std::endl;
                ofsCodeC << "	" << "vx_size " << weights + "_m_stride[4];" << std::endl;
                ofsCodeC << "	" << "for ( vx_uint32 i=0; i < 4; i++) { " << weights+"_m_stride[i] = " << weights + "_m_size; "
                         << weights + "_m_size *= " << weights+ "_dims[i]; } " << std::endl;
                ofsCodeC << "	" << "vxCopyTensorPatch( " << weights << ", 4, nullptr, nullptr," << weights+"_m_stride, " << weights +"_buf, "
                         << "VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST); " << std::endl;
                ofsCodeC << "	" << "fclose(" << weights + "_file);" << std::endl;
                ofsCodeC << "	" << "delete " << weights + "_buf;" << std::endl;
            }
            else if(codeType == "release_tensors") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << " ));" << std::endl;
            }
            declare_tensor_check[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = output + "_B";
                if(codeType == "declaration") {
                    ofsCodeH << "	vx_size " << bias << "_dims[1];" << std::endl;
                    ofsCodeH << "	vx_tensor " << bias << ";" << std::endl;
                }
                else if(codeType == "constructor") {
                    ofsCodeC << "	" << bias + "_dims  { " << k << " }, " << std::endl;
                }
                else if(codeType == "initialize") {
                    ofsCodeC << "	" << bias << " = vxCreateTensor(context,1, " << bias + "_dims, VX_TYPE_FLOAT32," << fixedPosition << ");" << std::endl;
                    ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                    ofsCodeC << "   " << "fileName = str + " << "\"/bias/" + layer_name + ".f32\";" << std::endl;
                    ofsCodeC << "	" << "FILE * " << bias << "_file = fopen(fileName.c_str(), " << "\"rb\"" << ");" << std::endl;                    ofsCodeC << "	" << "vx_size  " << bias << "_size =  " << 4 * k << ";" << std::endl;
                    ofsCodeC << "	" << "float * " << bias << "_buf = new float[" << bias + "_size];" << std::endl;
                    ofsCodeC << "	" << "fread(" << bias + "_buf,1," << bias + "_size," << bias + "_file);" << std::endl;
                    ofsCodeC << "	" << "vx_size " << bias  + "_m_size = 4;" << std::endl;
                    ofsCodeC << "	" << "vx_size " << bias + "_m_stride[1];" << std::endl;
                    ofsCodeC << "	" << "for ( vx_uint32 i=0; i < 1; i++) { " << bias + "_m_stride[i] = " << bias + "_m_size; "
                             << bias + "_m_size *= " << bias + "_dims[i]; } " << std::endl;
                    ofsCodeC << "	" << "vxCopyTensorPatch( " << bias << ", 1, nullptr, nullptr," << bias + "_m_stride, " << bias +"_buf, "
                             << "VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST); " << std::endl;
                    ofsCodeC << "	" << "fclose(" << bias + "_file);" << std::endl;
                    ofsCodeC << "	" << "delete " << bias + "_buf;" << std::endl;
                }
                else if(codeType == "release_tensors") {
                    ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << " ));" << std::endl;
                }
                declare_tensor_check[bias] = true;
            }
            if(codeType == "declaration") {
                ofsCodeH << "	vx_nn_convolution_params_t " << output << "_params;" << std::endl;
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_params.padding_x = " << pad_w << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.padding_y = " << pad_h << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.overflow_policy = " << convertPolicy << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.rounding_policy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.down_scale_size_rounding = " << "VX_NN_DS_SIZE_ROUNDING_FLOOR ;" << std::endl;
                ofsCodeC << "	" << output + "_params.dilation_x = " << dilation_w - 1 << " ;" << std::endl;
                ofsCodeC << "	" << output + "_params.dilation_y = " << dilation_h - 1 << " ;" << std::endl;
                ofsCodeC << "	" << output + "_node = " << "vxConvolutionLayer(graph, " << node[4] << ", " << weights << ", " << bias << ", &" << output + "_params, " << "sizeof(" << output + "_params ), " << output << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;

            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "Deconvolution") {
            std::stringstream ss(params);
            int k, kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h, bias_term;
            ss >> k >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h >> bias_term;
            std::string weights = output + "_W";
            auto&& dim = tensorMap[weights];
            if(codeType == "declaration") {
                ofsCodeH << "	vx_size " << weights << "_dims[4];" << std::endl;
                ofsCodeH << "	vx_tensor " << weights << " ;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << weights + "_dims{ " << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << "}," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << weights + "= vxCreateTensor(context,4, " << weights + "_dims, VX_TYPE_FLOAT32," << fixedPosition << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "   " << "fileName = str + " << "\"/weights/" + layer_name + ".f32\";" << std::endl;
                ofsCodeC << "	" << "FILE * " << weights << "_file = fopen(fileName.c_str(), " << "\"rb\"" << ");" << std::endl;
                ofsCodeC << "	" << "if(!" << weights << "_file) { std::cerr << \"ERROR: unable to open the file \" << fileName << std::endl; }" << std::endl;
                ofsCodeC << "	" << "vx_size  " << weights << "_size =  " << 4 * dim[3] * dim[2] * dim[1] * dim[0] << ";" << std::endl;
                ofsCodeC << "	" << "float * " << weights << "_buf = new float[" << weights + "_size];" << std::endl;
                ofsCodeC << "	" << "size_t " << weights + "_res_size;" << std::endl;
                ofsCodeC << "	" << weights + "_res_size = " << "fread(" << weights + "_buf,1," << weights + "_size," << weights + "_file);" << std::endl;
                ofsCodeC << "	" << "if(" + weights + "_res_size != " << weights + "_size ) { std::cerr << \"ERROR: read error in : \" << fileName << std::endl; }"<< std::endl;
                ofsCodeC << "	" << "vx_size " << weights  + "_m_size = 4;" << std::endl;
                ofsCodeC << "	" << "vx_size " << weights + "_m_stride[4];" << std::endl;
                ofsCodeC << "	" << "for ( vx_uint32 i=0; i < 4; i++) { " << weights+"_m_stride[i] = " << weights + "_m_size; "
                         << weights + "_m_size *= " << weights+ "_dims[i]; } " << std::endl;
                ofsCodeC << "	" << "vxCopyTensorPatch( " << weights << ", 4, nullptr, nullptr," << weights+"_m_stride, " << weights +"_buf, "
                         << "VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST); " << std::endl;
                ofsCodeC << "	" << "fclose(" << weights + "_file);" << std::endl;
                ofsCodeC << "	" << "delete " << weights + "_buf;" << std::endl;
            }
            else if(codeType == "release_tensors") {
                ofsCodeC << "   " << "vxReleaseTensor(&" << weights << " );" << std::endl;
            }
            declare_tensor_check[weights] = true;
            std::string bias = "NULL";
            if(bias_term) {
                bias = output + "_B";
                if(codeType == "declaration") {
                    ofsCodeH << "	vx_size " << bias << "_dims[1];" << std::endl;
                    ofsCodeH << "	vx_tensor " << bias << ";" << std::endl;
                }
                else if(codeType == "constructor") {
                    ofsCodeC << "	" << bias + "_dims{" << k << "}," << std::endl;
                }
                else if(codeType == "initialize") {
                    ofsCodeC << "	" << bias + " = vxCreateTensor(context,1, " << bias + "_dims, VX_TYPE_FLOAT32, " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                    ofsCodeC << "   " << "fileName = str + " << "\"/bias/" + layer_name + ".f32\";" << std::endl;
                    ofsCodeC << "	" << "FILE * " << bias << "_file = fopen(fileName.c_str(), " << "\"rb\"" << ");" << std::endl;
                    ofsCodeC << "	" << "if(!" << bias << "_file) { std::cerr << \"ERROR: unable to open the file \" << fileName << std::endl; }" << std::endl;
                    ofsCodeC << "	" << "vx_size  " << bias << "_size =  " << 4 * k << ";" << std::endl;
                    ofsCodeC << "	" << "float * " << bias << "_buf = new float[" << bias + "_size];" << std::endl;
                    ofsCodeC << "	" << "fread(" << bias + "_buf,1," << bias + "_size," << bias + "_file);" << std::endl;
                    ofsCodeC << "	" << "vx_size " << bias  + "_m_size = 4;" << std::endl;
                    ofsCodeC << "	" << "vx_size " << bias + "_m_stride[1];" << std::endl;
                    ofsCodeC << "	" << "for ( vx_uint32 i=0; i < 1; i++) { " << bias + "_m_stride[i] = " << bias + "_m_size; "
                             << bias + "_m_size *= " << bias + "_dims[i]; } " << std::endl;
                    ofsCodeC << "	" << "vxCopyTensorPatch( " << bias << ", 1, nullptr, nullptr," << bias + "_m_stride, " << bias +"_buf, "
                             << "VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST); " << std::endl;
                    ofsCodeC << "	" << "fclose(" << bias + "_file);" << std::endl;
                    ofsCodeC << "	" << "delete " << bias + "_buf;" << std::endl;
                }
                else if(codeType == "release_tensors") {
                    ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << " ));" << std::endl;
                }
                declare_tensor_check[bias] = true;
            }
            if(codeType == "declaration") {
                ofsCodeH << "	vx_nn_deconvolution_params_t " << output << "_params;" << std::endl;
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_params.padding_x = " << pad_w << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.padding_y = " << pad_h << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.overflow_policy = " << convertPolicy << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.rounding_policy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.a_x = " << dilation_w - 1 << ";" << std::endl;
                ofsCodeC << "	" << output + "_params.a_y = " << dilation_h - 1 << ";" << std::endl;
                ofsCodeC << "	" << output + "_node = " << " vxDeconvolutionLayer(graph, " << node[4] << ", " << weights << ", " << bias << ", &" << output + "_params, " << output << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "Pooling") {
            std::stringstream ss(params);
            int kernel_w, kernel_h, stride_w, stride_h, pad_w, pad_h, pool;
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> pool;
            if((pool != 0 && pool != 1)) error("writeGDF: pooling_layer supports only MAX and AVG\n");
            if(codeType == "declaration" ) {
                ofsCodeH << "	vx_enum " << output << "_type;" << std::endl;
                ofsCodeH << "	vx_size " << output << "_kernel_w;" << std::endl;
                ofsCodeH << "	vx_size " << output << "_kernel_h;" << std::endl;
                ofsCodeH << "	vx_size " << output << "_pad_w;" << std::endl;
                ofsCodeH << "	vx_size " << output << "_pad_h;" << std::endl;
                ofsCodeH << "	vx_enum " << output << "_roundPolicy;" << std::endl;
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << output + "_kernel_w(" << kernel_w << ")," << std::endl;
                ofsCodeC << "	" << output + "_kernel_h(" << kernel_h << ")," << std::endl;
                ofsCodeC << "	" << output + "_pad_w(" << pad_w << ")," << std::endl;
                ofsCodeC << "	" << output + "_pad_h(" << pad_h << ")," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_type = " << (pool == 0 ? "VX_NN_POOLING_MAX" : "VX_NN_POOLING_AVG") << " ;" << std::endl;
                ofsCodeC << "	" << output + "_roundPolicy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "	" << output + "_node = " << "vxPoolingLayer(graph, " << node[4] << ", " << output + "_type" << ", " << output + "_kernel_w, " << output + "_kernel_h, "
                         << output + "_pad_w, " << output + "_pad_h, " << output + "_roundPolicy, " << output << " );" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "InnerProduct") {
            std::stringstream ss(params);
            int k,bias_term;
            ss >> k >> bias_term;
            std::string weights = output + "_W";
            auto&& dim = tensorMap[weights];
            if(codeType == "declaration") {
                ofsCodeH << "	vx_size " << weights << "_dims[4];" << std::endl;
                ofsCodeH << "	vx_tensor " << weights << ";" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << weights + "_dims{" << dim[3] << ", " << dim[2] << "," << dim[1] << "," << dim[0] << "}," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << weights << "= vxCreateTensor(context,4," << weights + "_dims, VX_TYPE_FLOAT32, " << fixedPosition << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" << weights << "); " << std::endl;
                ofsCodeC << "   " << "fileName = str + " << "\"/weights/" + layer_name + ".f32\";" << std::endl;
                ofsCodeC << "	" << "FILE * " << weights << "_file = fopen(fileName.c_str(), " << "\"rb\"" << ");" << std::endl;
                ofsCodeC << "	" << "if(!" << weights << "_file) { std::cerr << \"ERROR: unable to open the file \" << fileName << std::endl; }" << std::endl;
                ofsCodeC << "	" << "vx_size  " << weights << "_size =  " << 4 * dim[3] * dim[2] * dim[1] * dim[0] << ";" << std::endl;
                ofsCodeC << "	" << "float * " << weights << "_buf = new float[" << weights + "_size];" << std::endl;
                ofsCodeC << "	" << "size_t " << weights + "_res_size;" << std::endl;
                ofsCodeC << "	" << weights + "_res_size = " << "fread(" << weights + "_buf,1," << weights + "_size," << weights + "_file);" << std::endl;
                ofsCodeC << "	" << "if(" + weights + "_res_size != " << weights + "_size ) { std::cerr << \"ERROR: read error in : \" << fileName << std::endl; }"<< std::endl;
                ofsCodeC << "	" << "vx_size " << weights  + "_m_size = 4;" << std::endl;
                ofsCodeC << "	" << "vx_size " << weights + "_m_stride[4];" << std::endl;
                ofsCodeC << "	" << "for ( vx_uint32 i=0; i < 4; i++) { " << weights+"_m_stride[i] = " << weights + "_m_size; "
                         << weights + "_m_size *= " << weights+ "_dims[i]; } " << std::endl;
                ofsCodeC << "	" << "vxCopyTensorPatch( " << weights << ", 4, nullptr, nullptr," << weights+"_m_stride, " << weights +"_buf, "
                         << "VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST); " << std::endl;
                ofsCodeC << "	" << "fclose(" << weights + "_file);" << std::endl;
                ofsCodeC << "	" << "delete " << weights + "_buf;" << std::endl;
            }
            else if(codeType == "release_tensors") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << weights << " ));" << std::endl;
            }
            declare_tensor_check[weights]= true;
            std::string bias= "NULL";
            if(bias_term) {
                bias = output + "_B" ;
                if(codeType == "declaration") {
                    ofsCodeH << "	vx_size " << bias << "_dims[1];" << std::endl;
                    ofsCodeH << "	vx_tensor " << bias << ";" << std::endl;
                }
                else if(codeType == "constructor") {
                    ofsCodeC << "	" << bias + "_dims{" << k << "}," << std::endl;
                }
                else if(codeType == "initialize") {
                    ofsCodeC << "	" << bias << "= vxCreateTensor(context,1," << bias + "_dims, VX_TYPE_FLOAT32, " << fixedPosition << ");" << std::endl;
                    ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" << bias << "); " << std::endl;
                    ofsCodeC << "   " << "fileName = str + " << "\"/bias/" + layer_name + ".f32\";" << std::endl;
                    ofsCodeC << "	" << "FILE * " << bias << "_file = fopen(fileName.c_str(), " << "\"rb\"" << ");" << std::endl;
                    ofsCodeC << "	" << "if(!" << bias << "_file) { std::cerr << \"ERROR: unable to open the file \" << fileName << std::endl; }" << std::endl;
                    ofsCodeC << "	" << "vx_size  " << bias << "_size =  " << 4 * k << ";" << std::endl;
                    ofsCodeC << "	" << "float * " << bias << "_buf = new float[" << bias + "_size];" << std::endl;
                    ofsCodeC << "	" << "fread(" << bias + "_buf,1," << bias + "_size," << bias + "_file);" << std::endl;
                    ofsCodeC << "	" << "vx_size " << bias  + "_m_size = 4;" << std::endl;
                    ofsCodeC << "	" << "vx_size " << bias + "_m_stride[1];" << std::endl;
                    ofsCodeC << "	" << "for ( vx_uint32 i=0; i < 1; i++) { " << bias + "_m_stride[i] = " << bias + "_m_size; "
                             << bias + "_m_size *= " << bias + "_dims[i]; } " << std::endl;
                    ofsCodeC << "	" << "vxCopyTensorPatch( " << bias << ", 1, nullptr, nullptr," << bias + "_m_stride, " << bias +"_buf, "
                             << "VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST); " << std::endl;
                    ofsCodeC << "	" << "fclose(" << bias + "_file);" << std::endl;
                    ofsCodeC << "	" << "delete " << bias + "_buf;" << std::endl;
                }
                else if(codeType == "release_tensors") {
                    ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseTensor(&" << bias << " ));" << std::endl;
                }
                declare_tensor_check[bias]= true;
            }
            if(codeType == "declaration") {
                ofsCodeH << "	vx_enum " << output << "_convertPolicy;" << std::endl;
                ofsCodeH << "	vx_enum " << output << "_roundPolicy;" << std::endl;
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_convertPolicy = " << convertPolicy << ";" << std::endl;
                ofsCodeC << "	" << output + "_roundPolicy = " << roundPolicy << ";" << std::endl;
                ofsCodeC << "	" << output + "_node = " << "vxFullyConnectedLayer( graph, " << node[4] << ", " << weights << ", " << bias << ", " << output + "_convertPolicy, " << output + "_roundPolicy, " << output + ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "ReLU") {
            if(codeType == "declaration") {
                ofsCodeH << "	vx_enum " << output << "_mode;" << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_param_a;" << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_param_b;" << std::endl;
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << output + "_param_a(" << 0 << ")," << std::endl;
                ofsCodeC << "	" << output + "_param_b(" << 0 << ")," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_mode = " << "VX_NN_ACTIVATION_RELU ; " << std::endl;
                ofsCodeC << "	" << output + "_node = " << "vxActivationLayer(graph, " << node[4] << ", " << output + "_mode, " << output + "_param_a, " << output + "_param_b, " << output << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "LRN") {
            int normalization_size; float alpha,beta,k;
            std::string norm_region;
            std::stringstream ss(params);
            ss >> normalization_size >> alpha >> beta >> norm_region >> k;
            std::string lrnType;
            lrnType =  (norm_region == "1") ? "VX_NORMALIZATION_SAME_MAP" : "VX_NORMALIZATION_ACROSS_MAPS";
            if(codeType == "declaration") {
                ofsCodeH << "	vx_enum " << output << "_mode;" << std::endl;
                ofsCodeH << "	vx_size " << output << "_size;" << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_alpha;" << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_beta;" << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_bias;" << std::endl;
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << output + "_size("  << normalization_size << ")," << std::endl;
                ofsCodeC << " 	" << output + "_alpha(" << alpha << ")," << std::endl;
                ofsCodeC << "	" << output + "_beta(" << beta << ")," << std::endl;
                ofsCodeC << "	" << output + "_bias(" << k << ")," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_mode = " << lrnType << ";" << std::endl;
                ofsCodeC << "	" << output + "_node = " << "vxNormalizationLayer( graph, " << node[4] << ", " << output + "_mode, " << output + "_size, " << output + "_alpha, " << output + "_beta, "
                         << output << ", " << output + "_bias );" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "BatchNorm") {
            int use_global_stats;
            float eps,beta=0, gamma= 1;
            std::stringstream ss(params);
            ss >> eps >> use_global_stats;
            if(codeType == "declaration") {
                ofsCodeH << "	vx_float32 " << output << "_eps; " << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_beta; " << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_gamma;" << std::endl;
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << output + "_eps(" << eps << ")," << std::endl;
                ofsCodeC << "	" << output + "_beta(" << beta << ")," << std::endl;
                ofsCodeC << "	" << output + "_gamma(" << gamma << ")," << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_node = " << "vxBatchNormalizationLayer(graph, " << node[4] << output + "_gamma, " << output + "_beta, " << output << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
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
            std::string tmp = node[4];
            for(int i=5; i < node.size() ; i++) {
                std::string out = node[3];
                if(i < node.size()- 1) {
                    out += "tmp_"+ std::to_string(i-4);
                    if(codeType == "declaration") {
                        ofsCodeH << "	vx_size " << out << "_dim[4];" << std::endl;
                        ofsCodeH << "	vx_tensor " << out << "; " << std::endl;
                    }
                    else if(codeType == "constructor") {
                        ofsCodeC << "	" << out + "_dim {" << dim[3] << ", " << dim[2] << ", " << dim[1] << ", " << dim[0] << " }," << std::endl;
                    }
                    else if(codeType == "initialize") {
                        ofsCodeC << "	" << out << "= vxCreateTensor(context,4, " << out + "_dim, VX_TYPE_FLOAT32, " << fixedPosition << ");" << std::endl;
                    }
                    declare_tensor_check[out]= true;
                }
                if(op == 1) {
                    if(codeType == "declaration") {
                        ofsCodeH << "	vx_enum " << node[3] << "_convertPolicy;" << std::endl;
                        ofsCodeH << "	vx_node	" << node[3] <<"_node;" << std::endl;
                    }
                    else if(codeType == "initialize") {
                        ofsCodeC << "	" << node[3] + "_convertPolicy = " << convertPolicy << ";" << std::endl;
                        ofsCodeC << "	" << node[3] + "_node = " << "vxTensorAddNode(graph, " << tmp << ", " << node[i] << ", " << node[3] + "_convertPolicy, " << out << ");" << std::endl;
                        ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
                    }
                    else if(codeType == "release_nodes") {
                        ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << node[3] + "_node ));" << std::endl;
                    }
                    tmp = out;
                }
                else error("generateCode : Eltwise op=%d not supported\n", op);
            }
        }
        else if(type == "Scale") {
            float alpha =1,beta=0;
            if(codeType == "declaration") {
                ofsCodeH << "	vx_float32 " << output << "_alpha;" << std::endl;
                ofsCodeH << "	vx_float32 " << output << "_beta;" << std::endl;
                ofsCodeH << "	vx_node	" << output << "_node;" << std::endl;
            }
            else if(codeType == "constructor") {
                ofsCodeC << "	" << output + "_alpha(" << alpha << ")," << std::endl;
                ofsCodeC << "	" << output + "_beta(" << beta << ")," << std::endl;
            }
        }
        else if(type == "Concat") {
            if(codeType == "declaration") {
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" <<  output + "_node = " << "vxConcatLayer(graph, " ;
                for(int i=4;i < node.size(); i++) {
                    ofsCodeC << node[i] << ", " ;
                }
                ofsCodeC << node[3] << " );" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "Dropout") {
            //during inference dropout layer propogates input to output .
            if(codeType == "declaration") {
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType ==  "initialize") {
                ofsCodeC << "	" << output + "_node = " << "vxCopyLayer( graph, " << node[4] << ", " << node[3] << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }        }
        else if(type == "Softmax") {
            if(codeType == "declaration") {
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_node = " << "vxSoftmaxLayer(graph, " << node[4] << ", " << node[3] << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "Split") {
            if(codeType == "declaration") {
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_node = " << "vxCopyLayer(graph, " << node[4] << ", " << node[3] << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        else if(type == "SoftmaxWithLoss") {
            if(codeType == "declaration") {
                ofsCodeH << "	vx_node " << output << "_node;" << std::endl;
            }
            else if(codeType == "initialize") {
                ofsCodeC << "	" << output + "_node = " << "vxSoftmaxLayer(graph, " << node[4] << ", " << node[3] << ");" << std::endl;
                ofsCodeC << "   " << "ERROR_CHECK_OBJECT(" + output + "_node);" << std::endl;
            }
            else if(codeType == "release_nodes") {
                ofsCodeC << "   " << "ERROR_CHECK_STATUS(vxReleaseNode(&" << output + "_node ));" << std::endl;
            }
        }
        ofsCodeH << std::endl;
        if(isLastLayer && codeType == "initialize")
        {
            ofsCodeC << "   " << "vxVerifyGraph(graph);" << std::endl;
        }
        if(isLastLayer && codeType == "constructor")
        {
            ofsCodeC << "   " << "context {NULL}," << std::endl;
            ofsCodeC << "   " << "graph {NULL}" << std::endl;
        }
        if(isLastLayer && codeType == "run") {
            ofsCodeC << "   " << "vx_size " << output << "_m_size = 4;" << std::endl;
            ofsCodeC << "   " << "vx_size " << output << "_m_stride[4];" << std::endl;
            ofsCodeC << "   " << "for (vx_uint32 i=0; i < 4 ; i++ ) { " << output << "_m_stride[i] = " << output << "_m_size;" << output +"_m_size *= " << output + "_dims[i]; }" << std::endl;
            ofsCodeC << "   " << "outputSizeInBytes = " << output  + "_m_size;" << std::endl;
            ofsCodeC << "   " << "vxCopyTensorPatch (" << output << ", 4, nullptr, nullptr, " << output << "_m_stride, " << "outputTensor, VX_READ_ONLY, VX_MEMORY_TYPE_HOST );" << std::endl;
        }
        if(codeType== "initialize") ofsCodeC << std::endl;
    }
}

void generateCode(
    std::ostream& ofsCodeH,
    std::ostream& ofsCodeC,
    std::ofstream& ofsCodeM,
    std::vector<std::vector<std::string>>& net,
    std::map<std::string,std::vector<int>>& tensorMap,
    std::string tensorType,
    int fixedPointPosition,
    std::string convertPolicy,
    std::string roundPolicy,
    bool isVirtualEnabled)
{
    // TODO: this code needs to be verified

    ofsCodeH << "#ifndef __net_h__" << std::endl;
    ofsCodeH << "#define __net_h__" << std::endl << std::endl;
    ofsCodeH << "#include <VX/vx.h>" << std::endl;
    ofsCodeH << "#include <vx_ext_amd.h>" << std::endl;
    ofsCodeH << "#include <VX/vx_khr_nn.h>" << std::endl << std::endl;
    ofsCodeH << "#include <iostream>" << std::endl;
    ofsCodeH << "#include <stdio.h>" << std::endl;
    ofsCodeH << "#include <stdlib.h>" << std::endl << std::endl;

  /*  ofsCodeH << "#define ERROR_CHECK_STATUS(call) { \n vx_status status = (call); \n "
             << "if(status != VX_SUCCESS ) \n { \n "
             << "   vxAddLogEntry((vx_reference)(obj), status,\"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\", status, __LINE__); \n  } " << std::endl
             << "   return status;\n }"
             << std::endl << std::endl; */

    ofsCodeH << "#define ERROR_CHECK_STATUS(call) { vx_status status = (call); if(status != VX_SUCCESS){ vxAddLogEntry(NULL, status, \"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\", status, __LINE__); return status; }}" << std::endl;
/*
    ofsCodeH << "#define ERROR_CHECK_OBJECT(obj) { \n vx_status status = vxGetStatus((vx_reference) (obj)); \n "
             << "if(status != VX_SUCCESS ) \n { \n"
             <<  "  vxAddLogEntry((vx_reference)(obj), status,\"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\", status, __LINE__); \n  }  " << std::endl
             << "   return status;\n }"
             << std::endl << std::endl;
*/

    ofsCodeH << "#define ERROR_CHECK_OBJECT(obj) { vx_status status = vxGetStatus((vx_reference)(obj)); if(status != VX_SUCCESS){ vxAddLogEntry((vx_reference)(obj), status, \"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\", status, __LINE__); return status; }}"
             << std::endl;
    ofsCodeH
            << "class NetVX {" << std::endl
            << "public:" << std::endl
            << "    NetVX();" << std::endl
            << "    int Initialize(const char * dataFolder);" << std::endl
            << "    int Shutdown();" << std::endl
            << "    int Run(float * inputTensor, size_t inputSizeInBytes, float * outputTensor, size_t outputSizeInBytes);" << std::endl
            << "    ~NetVX();"
            << std::endl << std::endl
            << "protected:" << std::endl
               ;
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "declaration");
    ofsCodeH << "};" << std::endl << std::endl << "#endif" << std::endl;

    ofsCodeC << "#include \"net.h\"" << std::endl << std::endl;
    ofsCodeC << "NetVX::NetVX() :" << std::endl;
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "constructor");
    ofsCodeC << "{" << std::endl;
    ofsCodeC << std::endl;
    ofsCodeC << "}" << std::endl << std::endl;

    ofsCodeC << "NetVX::~NetVX() {}" << std::endl << std::endl;

    ofsCodeC << "int NetVX::Initialize(const char * dataFolder)" << std::endl;
    ofsCodeC << "{" << std::endl;
    ofsCodeC << "   std::string str = dataFolder, fileName;" << std::endl;
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "initialize");
    ofsCodeC << "	return 0;" << std::endl;
    ofsCodeC << "}" << std::endl << std::endl;

    ofsCodeC << "int NetVX::Shutdown()" << std::endl;
    ofsCodeC << "{" << std::endl;
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "release_nodes");
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "release_graph");
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "release_tensors");
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "release_context");
    ofsCodeC << "	return 0;" << std::endl;
    ofsCodeC << "}" << std::endl << std::endl;

    ofsCodeC << "int NetVX::Run(float * inputTensor, size_t inputSizeInBytes, float * outputTensor, size_t outputSizeInBytes)" << std::endl;
    ofsCodeC << "{" << std::endl;
    writeVXCode(ofsCodeH,ofsCodeC, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled, "run");
    ofsCodeC << "	return 0;" << std::endl;
    ofsCodeC << "}" << std::endl << std::endl;

    ofsCodeM << "#include \"net.h\"" << std::endl ;
    ofsCodeM << "#include <iostream> " << std::endl;
    ofsCodeM << "#include <stdio.h>" << std::endl;
    ofsCodeM << "int main(int argc , char * argv[])" << std::endl;
    ofsCodeM << "{" << std::endl;
    ofsCodeM << "   " << "NetVX net;" << std::endl;
    ofsCodeM << "   " << "FILE * fInput;" << std::endl;
    ofsCodeM << "   " << "fInput = fopen(\"input.f32\", \"rb\" );" << std::endl;
    ofsCodeM << "   " << "if(!fInput) { std::cerr << \" Unable to open the file input.f32 \" << std::endl; return -1; } " << std::endl;
    ofsCodeM << "   " << "fseek(fInput, 0, SEEK_END);" << std::endl;
    ofsCodeM << "   " << "size_t input_bytes;" << std::endl;
    ofsCodeM << "   " << "input_bytes = ftell(fInput);" << std::endl;
    ofsCodeM << "   " << "rewind(fInput);" << std::endl;
    ofsCodeM << "   " << "float * inputTensor = new float[input_bytes];" << std::endl;
    ofsCodeM << "   " << "size_t result = fread(inputTensor, 1, input_bytes,fInput );" << std::endl;
    ofsCodeM << "   " << "if(result != input_bytes) { std::cerr << \" Reading error \" << std::endl; return -1; } " << std::endl;
    ofsCodeM << "   " << "fclose(fInput);" << std::endl;
    ofsCodeM << "   " << "float * outputTensor;" << std::endl;
    ofsCodeM << "   " << "size_t outputSizeInBytes;" << std::endl << std::endl;
    ofsCodeM << "   " << "net.Initialize(\".\");" << std::endl;
    ofsCodeM << "   " << "net.Run(inputTensor, input_bytes, outputTensor, outputSizeInBytes);" << std::endl << std::endl;
    ofsCodeM << "   " << "FILE * fOut;" << std::endl;
    ofsCodeM << "   " << "fOut = fopen(\"output.f32\", \"wb\");" << std::endl;
    ofsCodeM << "   " << "if(!fOut) { std::cerr << \"ERROR: unable to open output.f32\" << std::endl; }" << std::endl;
    ofsCodeM << "   " << "fwrite(outputTensor, sizeof(float), outputSizeInBytes, fOut );" << std::endl;
    ofsCodeM << "   " << "fclose(fOut);" << std::endl << std::endl;
    ofsCodeM << "   " << "//Release nodes,graph,tensors,context" << std::endl;
    ofsCodeM << "   " << "net.Shutdown(); " << std::endl;
    ofsCodeM << "   " << "delete[] inputTensor;" << std::endl;
    ofsCodeM << "   " << "delete[] outputTensor;" << std::endl;
    ofsCodeM << "   " << "return 1;" << std::endl << std::endl;
    ofsCodeM << "}" << std::endl;

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

        caffe::Phase phase = layer_parameter.phase();
        for(int j=0;j<layer_parameter.include_size();j++){
            if(layer_parameter.include(j).phase()){
                phase = layer_parameter.include(j).phase();
            }
        }

        if(phase== caffe::TEST){
            continue;
        }

        //Check layer name.
        else if(layer_parameter.type() == "Input" || layer_parameter.type() == "Data" || layer_parameter.type() == "ImageData" ) {
            outputNameMap[layer_parameter.top(0)]= layer_parameter.top(0);
            if(layer_parameter.type() == "Input"  && ((inputDim[0]==0) || (inputDim[1]==0) || (inputDim[2]==0) || (inputDim[3]==0))){
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
        bool isSplitEnabled = (flags == 1) ? true : false;
        if(!isSplitEnabled) {
            if(layer_parameter.type()=="Split"){
                for(int j=0; j< layer_parameter.top_size() ; j++ ){
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
            if(layer_parameter.type()=="Split"){
                splitNameMap[layer_parameter.name()]= layer_parameter.bottom(0);
                for(int j=0; j< layer_parameter.top_size() ; j++ ){
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
            if(isSplitEnabled && (strstr(layer_parameter.bottom(j).c_str(),"split"))){
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
            "      --[no-]generate-vx-code   - do/don't generate OpenVX C Code with weight/bias initialization (default: OFF)\n"
            "      --output-dir <folder>     - specify output folder for weights/biases, GDF, and OpenVX C Code (default: current)\n"
            "      --flags <int>             - specify custom flags (default: 0)\n"
            ;

    // get options
    bool isVirtualEnabled = true;
    bool generateGDF = true;
    bool generateVXC = false;
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

    if(flags != 1 && flags != 0 ) {
        printf("ERROR: flags can only take 0 or 1\n");
        return -1;
    }

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
    else{
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
        writeGDF(ofsGDF, net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled);
    }

    if(generateVXC) {
        std::ofstream ofsCodeH(outputFolder + "/net.h", std::ios::binary);
        std::ofstream ofsCodeC(outputFolder + "/net.cpp", std::ios::binary);
        std::ofstream ofsCodeM(outputFolder + "/main.cpp", std::ios::binary);
        generateCode(ofsCodeH, ofsCodeC, ofsCodeM,  net, tensorMap, tensorType, fixedPointPosition, convertPolicy, roundPolicy, isVirtualEnabled);
    }

    return 0;
}
