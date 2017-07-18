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

void removeUnknownTypes(std::string& str, const std::string& from, const std::string& to){
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

int main(int argc, char* argv[]){
    
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    caffe::NetParameter net_parameter;
    
    if(argc < 2) {
       printf("Usage bin2proto <net.caffemodel> \n");
       return -1;
    }
    const char * fileName = argv[1];
   
 {
    //Read the caffemodel file.
    std::fstream input(fileName, std::ios::in| std::ios::binary);
    if(!net_parameter.ParseFromIstream(&input)){
	std::cerr << " Caffemodel read failed " << std::endl;
    }
    else{
       std::cout << " Caffemodel read succeeded " << std::endl;
    }
 }

 {
    std::fstream fs;
    fs.open("net.prototxt", std::ios::out);
    std::string out;
    if(google::protobuf::TextFormat::PrintToString(net_parameter,&out)){
        std::cout << " File Write Successfull" << std::endl;
        removeUnknownTypes(out,"95: 0","");
        fs << out;
    }
    else{
      std::cerr << " File write failed " << std::endl;
    } 
 }
   

}
