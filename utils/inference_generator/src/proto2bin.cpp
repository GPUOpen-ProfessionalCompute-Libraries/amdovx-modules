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
#include "../proto/caffe.pb.h"
#include <string.h>

int main(int argc, char* argv[]){

    GOOGLE_PROTOBUF_VERIFY_VERSION;
    
    if(argc < 2){
        printf("Usage Proto2Binary <net.prototxt> \n");
        return -1;
    }

    const char * prototxtFileName = argv[1];

    caffe::NetParameter net_parameter;
    //Read the Prototxt File.
    {
        int fd = open(prototxtFileName,O_RDONLY);
        if(fd < 0) {
            std::cerr << " Unable to open the file : " << prototxtFileName << std::endl;
        }

        google::protobuf::io::FileInputStream fi(fd);
        fi.SetCloseOnDelete(true);
        if(!google::protobuf::TextFormat::Parse(&fi, &net_parameter)){
            std::cerr << " Failed to parse the file : " << prototxtFileName << std::endl;
        }else{
            std::cout << " Read successful " << std::endl;
        }

    }

    //write net parameter to binary data.
    {

        std::fstream output("proto.bin",std::ios::out | std::ios::binary);
        if(!net_parameter.SerializeToOstream(&output)){
            std::cerr << " Failed to write into binary " << std::endl;
        }else{
            std::cout<<"File written into binary successful " << std::endl;
        }

    }

    google::protobuf::ShutdownProtobufLibrary();


    
}
