import os
import caffe_pb2
from nnir import *
import sys
import argparse
import struct
import math

caffe2ir_op_type = {
    'Convolution': 'conv',
    'Deconvolution': 'conv_transpose',
    'BatchNorm' : 'batch_norm',
    'InnerProduct' : 'gemm',
    'ReLU' : 'relu',
    'LRN' : 'lrn',
    'Eltwise' : 'sum',
    'Concat' : 'concat',
    'Softmax' : 'softmax',
    'SoftmaxWithLoss' : 'softmax'
}

def caffe_name_to_ir_name(name):
    return '_'.join(('_'.join(name.split('/')).split('-')))

def caffe_blob_to_ir_tensor(blob_name, blob_data_type, blob_shape):
    tensor = IrTensor()
    tensor.setName(caffe_name_to_ir_name(blob_name))
    tensor.setInfo(blob_data_type, [int(x) for x in blob_shape])
    return tensor

def convert_caffe_bin_to_ir_bin(floatlist):
    buf = struct.pack('%sf' % len(floatlist), *floatlist)
    return buf

def extractBinary(net_parameter, graph):
    initializerList = {}
    layers = net_parameter.layer
    print ("Total number of layers is : " + str(len(layers)))
    if (len(layers) == 0):
        print ("ERROR: unsupported caffemodel,  kindly upgrade your caffemodel with new proto style.")
        sys.exit(1)

    for i in range(len(layers)):
        layer_parameter = layers[i]
        layer_name = caffe_name_to_ir_name(layer_parameter.name)
        print ("Layer name is : "  + layer_name)
        
        ## add weights and biases into the initializer list.
        blob_size = len(layer_parameter.blobs)
        if blob_size > 0:
            weight_blob_proto = layer_parameter.blobs[0]
            weight_len = len(weight_blob_proto.data)
            weight_blob_name = caffe_name_to_ir_name(layer_name + '_w')
            print (weight_blob_name)
            weight_blob_shape = [int(x) for x in weight_blob_proto.shape.dim]
            print (weight_blob_shape)
            blob_data_type = "F064" if (len(weight_blob_proto.double_data) > 0) else "F032"
            print (blob_data_type)
            initializerList[weight_blob_name] = weight_blob_shape
            graph.addVariable(caffe_blob_to_ir_tensor(weight_blob_name, blob_data_type, weight_blob_shape))
            buf = convert_caffe_bin_to_ir_bin(weight_blob_proto.double_data if blob_data_type == "F064" else weight_blob_proto.data)
            graph.addBinary(weight_blob_name, buf)
    
        if blob_size > 1:
            bias_blob_proto = layer_parameter.blobs[1]
            bias_len = len(bias_blob_proto.data)
            bias_blob_name = caffe_name_to_ir_name(layer_name + '_b')
            print (bias_blob_name)
            bias_blob_shape = [int(x) for x in bias_blob_proto.shape.dim]
            print (bias_blob_shape)
            blob_data_type = "F064" if (len(bias_blob_proto.double_data) > 0) else "F032"
            print (blob_data_type)
            initializerList[bias_blob_name] = bias_blob_shape
            graph.addVariable(caffe_blob_to_ir_tensor(bias_blob_name, blob_data_type, bias_blob_shape))
            buf = convert_caffe_bin_to_ir_bin(bias_blob_proto.double_data if blob_data_type == "F064" else bias_blob_proto.data)
            graph.addBinary(bias_blob_name, buf)

    return initializerList

def extractInput(net_parameter, graph, input_dims):
    inputList = {}
    layers = net_parameter.layer
    first_layer_param = layers[0]
    first_layer_param_type = first_layer_param.type
    input_name = ""
    if len(net_parameter.input) != 0:
        input_name = caffe_name_to_ir_name(net_parameter.input[0])
        print ("Entered input")
    elif (first_layer_param_type == "Data" or first_layer_param_type == "Input" or first_layer_param_type == "ImageData"):
        top_list = first_layer_param.top
        print ("top list size : " + str(len(top_list)))
        if (len(top_list) == 0):
            input_name = caffe_name_to_ir_name(first_layer_param.name)
        else:
            input_name = caffe_name_to_ir_name(top_list[0])
        print ("entered type data, input, imagedata")
    else:
        bottom_list = first_layer_param.bottom
        if (len(bottom_list) == 0):
            top_list = first_layer_param.top
            input_name = caffe_name_to_ir_name(top_list[0])
        else:
            input_name = caffe_name_to_ir_name(bottom_list[0])
        print ("entered first layer")
        print ("Lenght of bottom list is : " + str(len(bottom_list)))
    
    print ("Input name : ")
    print (input_name)
    inputList[str(input_name)] = input_dims
    graph.addInput(caffe_blob_to_ir_tensor(input_name, "F032", input_dims))
    return inputList

def extractOutput(net_parameter, graph, inputOutputMap):
    outputList = {}
    layers = net_parameter.layer
    last_layer_param = layers[len(layers) - 1]
    top_list = last_layer_param.top
    if (len(top_list) == 0):
        bottom_list = last_layer_param.bottom
        output_name = caffe_name_to_ir_name(bottom_list[len(bottom_list) - 1])
    else:
        output_name = caffe_name_to_ir_name(str(top_list[0]))
    print ("output name is : " + output_name)
    net_len = len(inputOutputMap)
    out_layer_info = inputOutputMap[net_len - 1]
    output_map = out_layer_info["outputs"]
    output_dims = output_map[output_name]
    graph.addOutput(caffe_blob_to_ir_tensor(output_name, "F032", output_dims))
    return outputList
    

def extractCaffeAttrInfo(layer_param):
    layer_type = layer_param.type
    attribute_map = {}
    if (layer_type == "Convolution" or layer_type == "Deconvolution"):
        conv = layer_param.convolution_param
        pad_h = conv.pad_h if (conv.HasField('pad_h')) else (int(conv.pad[0]) if (len(conv.pad) > 0) else 0)
        pad_w = conv.pad_w if (conv.HasField('pad_w')) else (int(conv.pad[1]) if (len(conv.pad) > 1) else pad_h)
        stride_h = conv.stride_h if (conv.HasField('stride_h')) else (int(conv.stride[0]) if (len(conv.stride) > 0) else 1)
        stride_w = conv.stride_w if (conv.HasField('stride_w')) else (int(conv.stride[1]) if (len(conv.stride) > 1) else stride_h)
        kernel_h = conv.kernel_h if (conv.HasField('kernel_h')) else (int(conv.kernel_size[0]) if (len(conv.kernel_size) > 0) else 0)
        kernel_w = conv.kernel_w if (conv.HasField('kernel_w')) else (int(conv.kernel_size[1]) if (len(conv.kernel_size) > 1) else kernel_h)
        num_out = conv.num_output
        dilation_h = conv.dilation[0] if (len(conv.dilation) > 0) else 1
        dilation_w = conv.dilation[1] if (len(conv.dilation) > 1) else dilation_h
        bias_term = conv.bias_term
        groups = conv.group if (conv.HasField('group')) else 1

        attribute_map["strides"] = [stride_w, stride_h]
        attribute_map["kernel_shape"] = [kernel_w, kernel_h]
        attribute_map["group"] = groups
        attribute_map["pads"] = [pad_w, pad_h, 0, 0]
        attribute_map["dilations"] = [dilation_w, dilation_h]
    
    elif (layer_type == "Pooling"):
        pooling = layer_param.pooling_param
        pad_h = int(pooling.pad_h) if (pooling.HasField('pad_h')) else int(pooling.pad)
        pad_w = int(pooling.pad_w) if (pooling.HasField('pad_w')) else int(pooling.pad)
        stride_h = int(pooling.stride_h) if (pooling.HasField('stride_h')) else int(pooling.stride)
        stride_w = int(pooling.stride_w) if (pooling.HasField('stride_w')) else int(pooling.stride)
        kernel_h = int(pooling.kernel_h) if (pooling.HasField('kernel_h')) else int(pooling.kernel_size)
        kernel_w = int(pooling.kernel_w) if (pooling.HasField('kernel_w')) else int(pooling.kernel_size)
        
        attribute_map["strides"] = [stride_w, stride_h]
        attribute_map["kernel_shape"] = [kernel_w, kernel_h]
        attribute_map["pads"] = [pad_w, pad_h, 0, 0]
    
    elif (layer_type == "LRN"):
        lrn = layer_param.lrn_param
        local_size = lrn.local_size
        alpha = lrn.alpha
        beta = lrn.beta
        k = lrn.k
        
        attribute_map["alpha"] = alpha
        attribute_map["beta"] = beta
        attribute_map["size"] = local_size
        attribute_map["bias"] = k
        
    elif (layer_type == "BatchNorm"):
        attribute_map["epsilon"] = layer_param.batch_norm_param.eps

    return attribute_map

def calculateTensorDims(layer_param, input_map, attribute_map):
    output_dims = [0, 0, 0, 0]
    inputs = input_map.keys()
    if(layer_param.type == "Convolution"):
        strides = attribute_map["strides"]
        pads = attribute_map["pads"]
        dilations = attribute_map["dilations"]
        kernel_shape = attribute_map["kernel_shape"]
        n,c,h,w = input_map[inputs[0]]
        
        output_dims[3] = ((int(w) + 2 * pads[0] - kernel_shape[0] - (kernel_shape[0] - 1) * (dilations[0] - 1))/ strides[0]) + 1
        output_dims[2] = ((int(h) + 2 * pads[1] - kernel_shape[1] - (kernel_shape[1] - 1) * (dilations[1] - 1))/ strides[1]) + 1
        output_dims[1] = layer_param.convolution_param.num_output
        output_dims[0] = n

    elif (layer_param.type == "Deconvolution"):
        strides = attribute_map["strides"]
        pads = attribute_map["pads"]
        dilations = attribute_map["dilations"]
        kernel_shape = attribute_map["kernel_shape"]
        n,c,h,w = input_map[str(inputs[0])]

        output_dims[3] = strides[0] * (w - 1) + dilations[0] * (kernel_shape[0] - 1) + 1 - (2 * pads[0])
        output_dims[2] = strides[1] * (h - 1) + dilations[1] * (kernel_shape[1] - 1) + 1 - (2 * pads[1])
        output_dims[1] = layer_param.convolution_param.num_output
        output_dims[0] = n

    elif (layer_param.type == "Pooling"):
        strides = attribute_map["strides"]
        pads = attribute_map["pads"]
        kernel_shape = attribute_map["kernel_shape"]
        n,c,h,w = input_map[str(inputs[0])]        

        if (layer_param.pooling_param.global_pooling):
            kernel_shape[1] = h
            kernel_shape[0] = w
            pads[0] = 0
            pads[1] = 0
            strides[0] = 1
            strides[1] = 1
        
        output_dims[3] = int(math.ceil(float(w + 2 * pads[0] + strides[0] - kernel_shape[0])/strides[0]))
        output_dims[2] = int(math.ceil(float(h + 2 * pads[1] + strides[1] - kernel_shape[1])/strides[1]))
        output_dims[1] = c
        output_dims[0] = n
    
    elif (layer_param.type == "InnerProduct"):
        n,c,h,w = input_map[str(inputs[0])]
        output_dims[3] = 1
        output_dims[2] = 1
        output_dims[1] = layer_param.inner_product_param.num_output
        output_dims[0] = n    
    else:
        output_dims[0],output_dims[1],output_dims[2],output_dims[3] = input_map[str(inputs[0])]

    return output_dims
        
        
    
def extractCaffeNodeInfo(net_parameter, graph, inputsInfo, initializerInfo):
    inputOutputMap = {}
    dropoutLayerMap = {}
    splitLayerMap = {}
    layers = net_parameter.layer
    count = 0
    for i in range(len(layers)):
        layer_param = layers[i]
        layer_name = str(layer_param.name)
        layer_type = str(layer_param.type)
        inputs = layer_param.bottom
        outputs = layer_param.top

        if (layer_type == "Data" or layer_type == "ImageData" or layer_type == "Input"):
            continue
        
        if (layer_type == "Dropout"):
            for k in range(len(inputs)):
                dropoutLayerMap[str(outputs[k])] = str(inputs[k])
            continue

        if (layer_type == "Split"):
            for k in range(len(outputs)):
                splitLayerMap[str(outputs[k])] = str(inputs[k])
            continue
            
        layer_info_map = {}
        input_map = {}
        output_map = {}
        layer_info_map["layer_name"] = layer_name
        if layer_type in caffe2ir_op_type:
            layer_info_map["layer_type"] = caffe2ir_op_type[layer_type]
        elif layer_type == "Pooling":
            pool_type = layer_param.pooling_param.pool
            layer_info_map["layer_type"] = "max_pool" if (pool_type == caffe_pb2.PoolingParameter.MAX) else "avg_pool"
        else:
            print ("ERROR: caffe operation %s is not supported yet." % (layer_type))
            sys.exit(1)

        #extract attributes.
        attribute_map = extractCaffeAttrInfo(layer_param)
        layer_info_map["attributes"] = attribute_map

        #extract input and output information.
        #input information.
        if (count == 0):
            for k in range(len(inputs)):
                if str(inputs[k]) in inputsInfo:
                    input_map[str(inputs[k])] = inputsInfo[str(inputs[k])] 
        else:
            for k in range(len(inputs)):
                previous_layer_info = inputOutputMap[count - 1]
                prevOutMap = previous_layer_info["outputs"]
                if str(inputs[k]) in prevOutMap:
                    input_map[str(inputs[k])] = prevOutMap[str(inputs[k])]
                elif str(inputs[k]) in dropoutLayerMap:
                    input_map[dropoutLayerMap[str(inputs[k])]] =  prevOutMap[dropoutLayerMap[str(inputs[k])]]
                elif str(inputs[k]) in splitLayerMap:
                    input_map[splitLayerMap[str(inputs[k])]] = prevOutMap[splitLayerMap[str(inputs[k])]]
                else:
                    if (((layer_type == "Softmax") or (layer_type == "SoftmaxWithLoss")) and k != 0):
                        break
                    print ("ERROR: unknown dimensions for %s " % (inputs[k]))
                    sys.exit(1)

        # calculate output dimensions.
        outputDims = calculateTensorDims(layer_param, input_map, attribute_map)
        output_map[str(outputs[0])] = outputDims

        ## add inputs and outputs to the layer info.
        layer_info_map["inputs"] = input_map
        layer_info_map["outputs"] = output_map
        
        #add weights and biases info if present into the layer info.
        weights = layer_name + '_w'
        biases = layer_name +  '_b'
        weights_map = {}
        bias_map = {}
        if (weights in initializerInfo):
            weights_map[weights] = initializerInfo[weights]
            layer_info_map["weights"] = weights_map
        if (biases in initializerInfo):
            bias_map[biases] = initializerInfo[biases]
            layer_info_map["biases"] = bias_map

        print (layer_info_map)
        inputOutputMap[count] = layer_info_map
        count += 1
        
    return inputOutputMap
 
def caffe_graph_to_ir_graph(net_parameter, input_dims):
    graph = IrGraph()
    initializerMap = extractBinary(net_parameter, graph)
    inputMap = extractInput(net_parameter, graph, input_dims)
    inputOutputMap = extractCaffeNodeInfo(net_parameter, graph, inputMap, initializerMap) 
    outputList = extractOutput(net_parameter, graph, inputOutputMap )
    return graph

def caffe2ir(net_parameter, input_dims, outputFolder):
    graph = caffe_graph_to_ir_graph(net_parameter, input_dims)
    graph.toFile(outputFolder)
    print ("graph successfully formed.")

def main():
    if len(sys.argv) < 4:
        print ("Usage : python caffe2nnir.py <caffeModel> <nnirOutputFolder> --input-dims [n,c,h,w]")
        sys.exit(1)
    caffeFileName = sys.argv[1]
    outputFolder = sys.argv[2]
    input_dims = sys.argv[4].replace('[','').replace(']','').split(',')
    print ("loading caffemodel from %s ..." % (caffeFileName))
    net_parameter = caffe_pb2.NetParameter()
    if not os.path.isfile(caffeFileName):
        print ("ERROR: unable to open : " + caffeFileName)
        sys.exit(1)

    print ('parsing the string from caffe model.')
    print (caffeFileName)
    net_parameter.ParseFromString(open(caffeFileName, 'rb').read())
    print ("caffemodel read successful")
    print ("converting to AMD NNIR model in %s ... " % (outputFolder))
    print ("input parameters obtained are : " + str(input_dims[0]) + " " + str(input_dims[1]) + " " + str(input_dims[2]) + " " + str(input_dims[3])) 

    caffe2ir(net_parameter, input_dims, outputFolder)

if __name__ == '__main__':
    main()
