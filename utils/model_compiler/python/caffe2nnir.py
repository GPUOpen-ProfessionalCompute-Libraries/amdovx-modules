import os
import caffe_pb2
from nnir import *
import sys
import argparse
import struct

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

def extractOutput(net_parameter, graph):
    outputList = {}
    layers = net_parameter.layer
    last_layer_param = layers[len(layers) - 1]
    top_list = last_layer_param.top
    if (len(top_list) == 0):
        bottom_list = last_layer_param.bottom
        output_name = caffe_name_to_ir_name(bottom_list[len(bottom_list) - 1])
    else:
        output_name = caffe_name_to_ir_name(top_list[0])
    
    print ("output name is : " + output_name)
    

def extractCaffeAttrInfo(layer_param):
    layer_type = layer_param.type
    attribute_map = {}
    if (layer_type == "Convolution" or layer_type == "Deconvolution"):
        conv = layer_param.convolution_param
        pad_h = conv.pad_h if (conv.pad_h is not "") else (conv.pad[0] if (len(conv.pad) > 0) else 0)
        pad_w = conv.pad_w if (conv.pad_w is not None) else (conv.pad[1] if (len(conv.pad) > 1) else pad_h)
        stride_h = conv.stride_h if (conv.stride_h is not None) else (conv.stride[0] if (len(conv.stride) > 0) else 1)
        stride_w = conv.stride_w if (conv.stride_w is not None) else (conv.stride[1] if (len(conv.stride) > 1) else stride_w)
        kernel_h = conv.kernel_h if (conv.kernel_h is not None) else (conv.kernel_size[0] if (len(conv.kernel_size) > 0) else 0)
        kernel_w = conv.kernel_w if (conv.kernel_w is not None) else (conv.kernel_size[1] if (len(conv.kernel_size) > 1) else kernel_h)
        num_out = conv.num_output
        dilation_h = conv.dilation[0] if (len(conv.dilation) > 0) else 1
        dilation_w = conv.dilation[1] if (len(conv.dilation) > 1) else dilation_h
        bias_term = conv.bias_term
        groups = conv.group if (conv.group is not None) else 1

        attribute_map["strides"] = [stride_w, stride_h]
        attribute_map["kernel_shape"] = [kernel_w, kernel_h]
        attribute_map["group"] = groups
        attribute_map["pads"] = [pad_w, pad_h, 0, 0]
        attribute_map["dilations"] = [dilation_w, dilation_h]
    
    elif (layer_type == "Pooling"):
        pooling = layer_param.pooling_param
        pad_h = pooling.pad_h if (pooling.pad_h is not None) else pooling.pad
        pad_w = pooling.pad_w if (pooling.pad_w is not None) else pooling.pad
        stride_h = pooling.stride_h if (pooling.stride_h is not None) else pooling.stride
        stride_w = pooling.stride_w if (pooling.stride_w is not None) else pooling.stride
        kernel_h = pooling.kernel_h if (pooling.kernel_h is not None) else pooling.kernel_size
        kernel_w = pooling.kernel_w if (pooling.kernel_w is not None) else pooling.kernel_size
        
        attribute_map["strides"] = [stride_w, stride_h]
        attribute_map["kernel_shape"] = [kernel_w, kernel_h]
        attribute_map["pads"] = [pad_w, pad_h, 0, 0]
    
    elif (layer_type == "LRN"):
        lrn = layer.lrn_param
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
    
def extractCaffeNodeInfo(net_parameter, graph, inputList, initializerList):
    inputOutputMap = {}
    layers = net_parameter.layer
    for i in range(len(layers)):
        layer_param = layers[i]
        layer_name = layer_param.name
        layer_type = layer_param.type
        inputs = layer_param.bottom
        outputs = layer_param.top

        if (layer_type == "Data" or layer_type == "ImageData" or layer_type == "Input"):
            continue

        layer_info_map = {}
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
        layer_info_map["attributes"] = extractCaffeAttrInfo(layer_param)
        
        print (layer_info_map) 
 
def caffe_graph_to_ir_graph(net_parameter, input_dims):
    graph = IrGraph()
    initializerMap = extractBinary(net_parameter, graph)
    inputMap = extractInput(net_parameter, graph, input_dims)
    inputOutputMap = extractCaffeNodeInfo(net_parameter, graph, inputMap, initializerMap) 
    #outputList = extractOutput(net_parameter, graph)
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
