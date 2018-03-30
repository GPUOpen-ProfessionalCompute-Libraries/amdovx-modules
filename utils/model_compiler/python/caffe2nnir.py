import os
import caffe_pb2
from nnir import *
import sys
import argparse
import struct

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
    initializerList = []
    layers = net_parameter.layer
    print ("Total number of layers is : " + str(len(layers)))
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
            initializerList.append(weight_blob_name)
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
            initializerList.append(bias_blob_name)
            graph.addVariable(caffe_blob_to_ir_tensor(bias_blob_name, blob_data_type, bias_blob_shape))
            buf = convert_caffe_bin_to_ir_bin(bias_blob_proto.double_data if blob_data_type == "F064" else bias_blob_proto.data)
            graph.addBinary(bias_blob_name, buf)

    return initializerList
 
def caffe_graph_to_ir_graph(net_parameter, input_dims):
    graph = IrGraph()
    initializerList = extractBinary(net_parameter, graph)
    
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
    f = open(caffeFileName, 'rb')
    net_parameter.ParseFromString(f.read())
    print ("caffemodel read successful")
    print ("converting to AMD NNIR model in %s ... " % (outputFolder))
    print ("input parameters obtained are : " + str(input_dims[0]) + " " + str(input_dims[1]) + " " + str(input_dims[2]) + " " + str(input_dims[3])) 

    caffe2ir(net_parameter, input_dims, outputFolder)

if __name__ == '__main__':
    main()
