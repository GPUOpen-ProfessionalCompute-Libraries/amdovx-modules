import os
import caffe_pb2
from nnir import *
import sys
import argparse
import struct
import math

# mapping from caffe layer types to nnir operators.
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

# convert caffename to ir names.
def caffe_name_to_ir_name(name):
    return '_'.join(('_'.join(name.split('/')).split('-')))

# convert caffe blobs to ir tensor.
def caffe_blob_to_ir_tensor(blob_name, blob_data_type, blob_shape):
    tensor = IrTensor()
    tensor.setName(caffe_name_to_ir_name(blob_name))
    tensor.setInfo(blob_data_type, [int(x) for x in blob_shape])
    return tensor

# convert caffe bin formats to ir bin formats.
def convert_caffe_bin_to_ir_bin(floatlist):
    buf = struct.pack('%sf' % len(floatlist), *floatlist)
    return buf

# map caffe attr to ir attr.
def caffe_attr_to_ir_attr(attribute_map):
    attr = IrAttr()
    attr_names = attribute_map.keys()
    for i in range(len(attr_names)):
        attributeInfo = attribute_map[attr_names[i]]
        if type(attributeInfo) is float:
            attr.set(attr_names[i], float(attributeInfo))
        elif type(attributeInfo) is int:
            attr.set(attr_names[i], int(attributeInfo))
        elif type(attributeInfo) == type([]):
            if (type(attributeInfo[0]) is int):
                attr.set(attr_names[i], [int(v) for v in (attributeInfo)])
            elif(type(attributeInfo[0]) is float):
                attr.set(attr_names[i], [float(v) for v in (attributeInfo)])
            else:
                print ("ERROR: unsupported list attribute")
                sys.exit(1)
        else:
            print ("Unsupported type of caffe attribute %s" % attr_names[i])
            sys.exit(1)
    return attr

# map caffe node to ir node.
def caffe_node_to_ir_node(layer_type, layer_info_map):
    node = IrNode()
    input_map = layer_info_map["inputs"]
    output_map = layer_info_map["outputs"]
    weight_map = {}
    if "weights" in layer_info_map:
        weight_map = layer_info_map["weights"]
    bias_map = {}
    if "biases" in layer_info_map:
        bias_map = layer_info_map["biases"]
    attribute_map = layer_info_map["attributes"]

    inputs = []
    for i in range(len(input_map.keys())):
        inputs.append(input_map.keys()[i])
    for i in range(len(weight_map.keys())):
        inputs.append(weight_map.keys()[i])
    for i in range(len(bias_map.keys())):
        inputs.append(bias_map.keys()[i])

    outputs = []
    for i in range(len(output_map.keys())):
        outputs.append(output_map.keys()[i])

    node.set(layer_type, [caffe_name_to_ir_name(name) for name in inputs],\
                         [caffe_name_to_ir_name(name) for name in outputs],\
                         caffe_attr_to_ir_attr(attribute_map))
    return node

# extract binary data from caffe layers if present.
def extractBinary(layer_parameter, graph):
    layer_name = caffe_name_to_ir_name(layer_parameter.name)
    print ("Extracting binaries from : "  + layer_name)
        
    ## dump weights and biases if present.
    blob_size = len(layer_parameter.blobs)
    if blob_size > 0:
        weight_blob_proto = layer_parameter.blobs[0]
        weight_len = len(weight_blob_proto.data)
        weight_blob_name = caffe_name_to_ir_name(layer_name + '_w')
        print (weight_blob_name)
        buf = convert_caffe_bin_to_ir_bin(weight_blob_proto.data)
        graph.addBinary(weight_blob_name, buf)
    
    if blob_size > 1:
        bias_blob_proto = layer_parameter.blobs[1]
        bias_len = len(bias_blob_proto.data)
        bias_blob_name = caffe_name_to_ir_name(layer_name + '_b')
        print (bias_blob_name)
        blob_data_type = "F032"
        buf = convert_caffe_bin_to_ir_bin(bias_blob_proto.data)
        graph.addBinary(bias_blob_name, buf)

# extracting input from caffe network and converting into ir input.
def extractInput(net_parameter, graph, input_dims):
    inputList = {}
    layers = net_parameter.layer
    first_layer_param = layers[0]
    first_layer_param_type = first_layer_param.type
    input_name = ""
    if len(net_parameter.input) != 0:
        input_name = caffe_name_to_ir_name(net_parameter.input[0])
    elif (first_layer_param_type == "Data" or first_layer_param_type == "Input" or first_layer_param_type == "ImageData"):
        top_list = first_layer_param.top
        if (len(top_list) == 0):
            input_name = caffe_name_to_ir_name(first_layer_param.name)
        else:
            input_name = caffe_name_to_ir_name(top_list[0])
    else:
        bottom_list = first_layer_param.bottom
        if (len(bottom_list) == 0):
            top_list = first_layer_param.top
            input_name = caffe_name_to_ir_name(top_list[0])
        else:
            input_name = caffe_name_to_ir_name(bottom_list[0])
    
    inputList[str(input_name)] = input_dims
    graph.addInput(caffe_blob_to_ir_tensor(input_name, "F032", input_dims))
    return inputList

# extraction of output from caffe network to ir output.
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
    
# extract layer attribute information from caffe layers.
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
        attribute_map["pads"] = [pad_w, pad_h, pad_w, pad_h]
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
        attribute_map["pads"] = [pad_w, pad_h, pad_w, pad_h]
        attribute_map["dilations"] = [0,0]
    
    elif (layer_type == "LRN"):
        lrn = layer_param.lrn_param
        local_size = int(lrn.local_size)
        alpha = float(lrn.alpha)
        beta = float(lrn.beta)
        k = float(lrn.k)
        
        attribute_map["alpha"] = alpha
        attribute_map["beta"] = beta
        attribute_map["size"] = local_size
        attribute_map["bias"] = k
        
    elif (layer_type == "BatchNorm"):
        attribute_map["epsilon"] = float(layer_param.batch_norm_param.eps)
    
    elif (layer_type == "InnerProduct"):
        attribute_map["broadcast"] = 1
        attribute_map["transB"] = 1

    return attribute_map

# calculate dimensions of the output of each layer.
def calculateTensorDims(layer_param, input_map, attribute_map):
    dimList = {}
    output_dims = [0, 0, 0, 0]
    inputs = input_map.keys()
    if(layer_param.type == "Convolution"):
        strides = attribute_map["strides"]
        pads = attribute_map["pads"]
        dilations = attribute_map["dilations"]
        kernel_shape = attribute_map["kernel_shape"]
        n,c,h,w = input_map[inputs[0]]

        #output_dims[3] = (pads[0] + int(w) + pads[2] - ((kernel_shape[0] - 1) * dilations[0] + 1)) // strides[0] + 1
        #output_dims[2] = (pads[1] + int(h) + pads[3] - ((kernel_shape[1] - 1) * dilations[1] + 1)) // strides[1] + 1
        
        output_dims[3] = ((int(w) + 2 * pads[0] - kernel_shape[0] - (kernel_shape[0] - 1) * (dilations[0] - 1))/ strides[0]) + 1
        output_dims[2] = ((int(h) + 2 * pads[1] - kernel_shape[1] - (kernel_shape[1] - 1) * (dilations[1] - 1))/ strides[1]) + 1
        output_dims[1] = layer_param.convolution_param.num_output
        output_dims[0] = n

        weight_dims = [output_dims[1], c, kernel_shape[1], kernel_shape[0]]
        dimList["weights"] = weight_dims
        if (layer_param.convolution_param.bias_term):
            bias_dims = [weight_dims[0]]
            dimList["bias"] = bias_dims

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

        weight_dims = [output_dims[1], c, kernel_shape[1] , kernel_shape[0]]
        dimList["weights"] = weight_dims
        if (layer_param.convolution_param.bias_term):
            bias_dims = [weight_dims[0]]
            dimList["bias"] = bias_dims

    elif (layer_param.type == "Pooling"):
        strides = attribute_map["strides"]
        pads = attribute_map["pads"]
        kernel_shape = attribute_map["kernel_shape"]
        n,c,h,w = input_map[str(inputs[0])]
        dilations = attribute_map["dilations"]        

        if (layer_param.pooling_param.global_pooling):
            kernel_shape[1] = h
            kernel_shape[0] = w
            pads[0] = 0
            pads[1] = 0
            strides[0] = 1
            strides[1] = 1
        #dilations = [0,0]
        #output_dims[3] = (pads[0] + int(w) + pads[2] - ((kernel_shape[0] - 1) * dilations[0] + 1)) // strides[0] + 1
        #output_dims[2] = (pads[1] + int(h) + pads[3] - ((kernel_shape[1] - 1) * dilations[1] + 1)) // strides[1] + 1
        
        output_dims[3] = int(math.ceil(float(w + 2 * pads[0] + strides[0] - kernel_shape[0])/strides[0]))
        output_dims[2] = int(math.ceil(float(h + 2 * pads[1] + strides[1] - kernel_shape[1])/strides[1]))
        if (pads[1] > 0):
            if (output_dims[2] - 1) * stride[1] >= (h + pads[1]):
                output_dims[2] = output_dims[2] - 1
        if (pads[0] > 0):
            if (output_dims[3] - 1) * strides[0] >= (w + pads[0]):
                output_dims[3] = output_dims[3] - 1
        
        output_dims[1] = c
        output_dims[0] = n
    
    elif (layer_param.type == "InnerProduct"):
        n,c,h,w = input_map[str(inputs[0])]
        output_dims[3] = 1
        output_dims[2] = 1
        output_dims[1] = layer_param.inner_product_param.num_output
        output_dims[0] = n 
        
        weight_dims = [output_dims[1], c, h, w]
        dimList["weights"] = weight_dims
        if (layer_param.inner_product_param.bias_term):
            dimList["bias"] = [weight_dims[0]]
 
    elif (layer_param.type == "Concat"):
        inputs = input_map.keys()
        for i in range(len(inputs)):
            n,c,h,w = input_map[inputs[i]]
            output_dims[1] += c
    else:
        output_dims[0],output_dims[1],output_dims[2],output_dims[3] = input_map[str(inputs[0])]

    dimList["output"] = output_dims
    
    return dimList

# extract caffe node information into ir nodes.            
def extractCaffeNodeInfo(net_parameter, graph, inputsInfo):
    inputOutputMap = {}
    dropoutLayerMap = {}
    splitLayerMap = {}
    outputNameAliasMap = {}
    layers = net_parameter.layer
    count = 0

    if (len(layers) == 0):
        print ("ERROR: unsupported caffemodel, kindly upgrade your caffemodel.")
        sys.exit(1)

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
                print (k)
                print (inputs[k])
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
                    if str(inputs[k]) in outputNameAliasMap:
                        input_map[outputNameAliasMap[dropoutLayerMap[str(inputs[k])]]] = prevOutMap[outputNameAliasMap[dropoutLayerMap[str(inputs[k])]]]
                    else:
                        input_map[dropoutLayerMap[str(inputs[k])]] =  prevOutMap[dropoutLayerMap[str(inputs[k])]]
                elif str(inputs[k]) in splitLayerMap:
                    if str(inputs[k]) in outputNameAliasMap:
                        input_map[outputNameAliasMap[splitLayerMap[str(inputs[k])]]] = prevOutMap[outputNameAliasMap[splitLayerMap[str(inputs[k])]]]
                    else:
                        input_map[splitLayerMap[str(inputs[k])]] = prevOutMap[splitLayerMap[str(inputs[k])]]
                else:
                    if (((layer_type == "Softmax") or (layer_type == "SoftmaxWithLoss")) and k != 0):
                        break
                    elif str(inputs[k]) in outputNameAliasMap:
                        input_map[outputNameAliasMap[inputs[k]]] = prevOutMap[outputNameAliasMap[str(inputs[k])]]
                    else:
                        print ("ERROR: unknown dimensions for %s " % (inputs[k]))
                        sys.exit(1)

        # calculate output dimensions.
        dimList = calculateTensorDims(layer_param, input_map, attribute_map)
        if str(layer_name) != str(outputs[0]):
            outputNameAliasMap[str(outputs[0])] = str(layer_name)
        output_map[caffe_name_to_ir_name(layer_name)] = dimList["output"]

        ## add inputs and outputs to the layer info.
        layer_info_map["inputs"] = input_map
        layer_info_map["outputs"] = output_map
        
        #add weights and biases info if present into the layer info.
        extractBinary(layer_param, graph)
        weights = layer_name + '_w'
        biases = layer_name +  '_b'
        weights_map = {}
        bias_map = {}
        if "weights" in dimList:
            weights = layer_name + '_w'
            weight_dims = dimList["weights"]
            weights_map[weights] = weight_dims
            graph.addVariable(caffe_blob_to_ir_tensor(weights, "F032", weight_dims))
            layer_info_map["weights"] = weights_map
        if "bias" in dimList:
            biases = layer_name + "_b"
            bias_dims = dimList["bias"]
            bias_map[biases] = bias_dims
            graph.addVariable(caffe_blob_to_ir_tensor(biases, "F032", bias_dims))
            layer_info_map["biases"] = bias_map
    
        print (layer_info_map)
        inputOutputMap[count] = layer_info_map
        count += 1

        node = caffe_node_to_ir_node(layer_info_map["layer_type"], layer_info_map)
        graph.addNode(node)

    graph.updateLocals()
    return inputOutputMap

# convert caffe graph to ir graph. 
def caffe_graph_to_ir_graph(net_parameter, input_dims):
    graph = IrGraph()
    inputMap = extractInput(net_parameter, graph, input_dims)
    inputOutputMap = extractCaffeNodeInfo(net_parameter, graph, inputMap) 
    outputList = extractOutput(net_parameter, graph, inputOutputMap )
    return graph

# convert caffe representation to ir representation.
def caffe2ir(net_parameter, input_dims, outputFolder):
    graph = caffe_graph_to_ir_graph(net_parameter, input_dims)
    graph.toFile(outputFolder)
    print ("OK: graph successfully formed.")

def main():
    if len(sys.argv) < 4:
        print ("Usage : python caffe2nnir.py <caffeModel> <nnirOutputFolder> --input-dims [n,c,h,w]")
        sys.exit(1)
    caffeFileName = sys.argv[1]
    outputFolder = sys.argv[2]
    input_dims = sys.argv[4].replace('[','').replace(']','').split(',')
    print ("OK: loading caffemodel from %s ..." % (caffeFileName))
    net_parameter = caffe_pb2.NetParameter()
    if not os.path.isfile(caffeFileName):
        print ("ERROR: unable to open : " + caffeFileName)
        sys.exit(1)

    print ("parsing the caffemodel from : ")
    print (caffeFileName)
    net_parameter.ParseFromString(open(caffeFileName, 'rb').read())
    print ("OK: caffemodel read successful")
    print ("converting to AMD NNIR format in %s ... " % (outputFolder))
    print ("input parameters obtained are : " + str(input_dims[0]) + " " + str(input_dims[1]) + " " + str(input_dims[2]) + " " + str(input_dims[3])) 

    caffe2ir(net_parameter, input_dims, outputFolder)

if __name__ == '__main__':
    main()
