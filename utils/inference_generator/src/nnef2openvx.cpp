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

#include "flat/flat_parser.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <set>
#include <string.h>
#include <stdlib.h>

////
// MAGIC numbers
//
#define VARIABLES_FILE_MAGIC 0xF00DD1E0
#define VARIABLES_DATA_MAGIC 0xF00DD1E1
#define VARIABLES_EOFF_MAGIC 0xF00DD1E2

////
// NNEF to OpenVX Translator
//
class NNEF2OpenVX_Translator : public nnef::Parser::Callback
{
public:
    NNEF2OpenVX_Translator(std::string nnefFolder_, std::string openvxFolder_, int verbose_, bool packVariables_)
      : nnefFolder(nnefFolder_), openvxFolder(openvxFolder_),
        verbose(verbose_), packVariables(packVariables_),
        fpVariables(NULL)
    {
    }

protected:
    ////
    // class variables
    //
    int verbose;
    bool packVariables;
    std::string nnefFolder;
    std::string openvxFolder;
    std::string openvxFilenameC;
    std::string variablesFilename;
    std::ofstream ovxC;
    std::vector<std::string> inputList;
    std::vector<std::string> outputList;
    std::vector<std::string> virtualList;
    std::vector<std::string> variableList;
    std::map<std::string,nnef::Shape> inputShape;
    std::map<std::string,nnef::Shape> outputShape;
    FILE * fpVariables;

protected:
    // utility function
    static void getTensorDims(const nnef::Shape& shape, bool isVirtual, std::vector<size_t>& dims, size_t num_dims = 0) {
        size_t rank = shape.rank();
        if(num_dims == 0) num_dims = (isVirtual ? 4 : rank);
        dims.clear();
        size_t count = 0;
        for(; count < (num_dims - rank); count++) {
            dims.push_back(1);
        }
        for(size_t i = 0; i < rank; i++, count++) {
            dims.push_back(shape[rank-1-i]);
        }
    }

    ////
    // translator callback implementations
    //
    virtual void beginGraph( const nnef::Prototype& proto )
    {
        // show NNEF syntax
        if(verbose & 1) {
            std::cout << "graph " << proto.name() << "( ";
            for ( size_t i = 0; i < proto.paramCount(); ++i ) {
                auto& param = proto.param(i);
                if ( i ) std::cout << ", ";
                std::cout << param.name();
            }
            std::cout << " ) -> ( ";
            for ( size_t i = 0; i < proto.resultCount(); ++i ) {
                auto& result = proto.result(i);
                if ( i ) std::cout << ", ";
                std::cout << result.name();
            }
            std::cout << " )" << std::endl << '{' << std::endl;
        }

        ////
        // get input and output parameter list
        //
        for (size_t i = 0; i < proto.paramCount(); ++i) {
            inputList.push_back(proto.param(i).name());
        }
        for (size_t i = 0; i < proto.resultCount(); ++i) {
            outputList.push_back(proto.result(i).name());
        }

        ////
        // generate OpenVX C code preamble
        //
        openvxFilenameC = openvxFolder + "/annmodule.cpp";
        ovxC.open(openvxFilenameC);
        if(!ovxC) {
            printf("ERROR: unable to create: %s\n", openvxFilenameC.c_str());
            exit(1);
        }
        ovxC << "#include \"annmodule.h\"" << std::endl
             << "#include <VX/vx_khr_nn.h>" << std::endl
             << "#include <vx_amd_nn.h>" << std::endl
             << "#include <vx_ext_amd.h>" << std::endl;
        if(packVariables) {
            ovxC << "#include <stdio.h>" << std::endl;
        }
        ovxC << std::endl
             << "#define ERROR_CHECK_OBJECT(obj) { vx_status status = vxGetStatus((vx_reference)(obj)); if(status != VX_SUCCESS) { vxAddLogEntry((vx_reference)context, status     , \"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\\n\", status, __LINE__); return status; } }" << std::endl
             << "#define ERROR_CHECK_STATUS(call) { vx_status status = (call); if(status != VX_SUCCESS) { vxAddLogEntry((vx_reference)context, status, \"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\\n\", status, __LINE__); return status; } }" << std::endl
             << std::endl;
        if(packVariables) {
            ovxC << "static vx_status initializeTensor(vx_context context, vx_tensor tensor, FILE * fp, const char * binaryFilename)" << std::endl
                 << "{" << std::endl
                 << "    vx_enum data_type = VX_TYPE_FLOAT32;" << std::endl
                 << "    vx_size num_of_dims = 4, dims[4] = { 1, 1, 1, 1 }, stride[4];" << std::endl
                 << "    ERROR_CHECK_STATUS(vxQueryTensor(tensor, VX_TENSOR_DATA_TYPE, &data_type, sizeof(vx_enum)));" << std::endl
                 << "    ERROR_CHECK_STATUS(vxQueryTensor(tensor, VX_TENSOR_NUMBER_OF_DIMS, &num_of_dims, sizeof(vx_size)));" << std::endl
                 << "    ERROR_CHECK_STATUS(vxQueryTensor(tensor, VX_TENSOR_DIMS, &dims, num_of_dims * sizeof(vx_size)));" << std::endl
                 << "    vx_size itemsize = sizeof(float);" << std::endl
                 << "    if(data_type == VX_TYPE_UINT8 || data_type == VX_TYPE_INT8) {" << std::endl
                 << "        itemsize = sizeof(vx_uint8);" << std::endl
                 << "    }" << std::endl
                 << "    else if(data_type == VX_TYPE_UINT16 || data_type == VX_TYPE_INT16 || data_type == VX_TYPE_FLOAT16) {" << std::endl
                 << "        itemsize = sizeof(vx_uint16);" << std::endl
                 << "    }" << std::endl
                 << "    vx_size count = dims[0] * dims[1] * dims[2] * dims[3];" << std::endl
                 << std::endl
                 << "    vx_uint32 h[2] = { 0 };" << std::endl
                 << "    fread(h, 1, sizeof(h), fp);" << std::endl
                 << "    if(h[0] != 0x" << std::hex << VARIABLES_DATA_MAGIC << std::dec << " || (vx_size)h[1] != (count*itemsize)) {" << std::endl
                 << "      vxAddLogEntry((vx_reference)tensor, VX_FAILURE, \"ERROR: invalid data (magic,size)=(0x%x,%d) in %s at byte position %d -- expected size is %ld\\n\", h[0], h[1], binaryFilename, ftell(fp)-sizeof(h), count*itemsize);" << std::endl
                 << "      return VX_FAILURE;" << std::endl
                 << "    }" << std::endl
                 << std::endl
                 << "    vx_map_id map_id;" << std::endl
                 << "    float * ptr;" << std::endl
                 << "    ERROR_CHECK_STATUS(vxMapTensorPatch(tensor, num_of_dims, nullptr, nullptr, &map_id, stride, (void **)&ptr, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST, 0));" << std::endl
                 << "    vx_size n = fread(ptr, itemsize, count, fp);" << std::endl
                 << "    if(n != count) {" << std::endl
                 << "        vxAddLogEntry((vx_reference)tensor, VX_FAILURE, \"ERROR: expected char[%ld], but got char[%ld] in %s\\n\", count*itemsize, n*itemsize, binaryFilename);" << std::endl
                 << "        return VX_FAILURE;" << std::endl
                 << "    }" << std::endl
                 << "    ERROR_CHECK_STATUS(vxUnmapTensorPatch(tensor, map_id));" << std::endl
                 << std::endl
                 << "    return VX_SUCCESS;" << std::endl
                 << "}" << std::endl
                 << std::endl;
        }
        ovxC << "vx_status annCreateGraph(vx_graph graph";
        for(auto& name : inputList) {
            ovxC << ", vx_tensor " << name;
        }
        for(auto& name : outputList) {
            ovxC << ", vx_tensor " << name;
        }
        if(packVariables) {
            ovxC << ", const char * binaryFilename";
        }
        ovxC << ")" << std::endl
             << "{" << std::endl
             << "    vx_context context = vxGetContext((vx_reference)graph);" << std::endl
             << "    ERROR_CHECK_OBJECT(context);" << std::endl
             << "    ERROR_CHECK_STATUS(vxLoadKernels(context, \"vx_nn\"));" << std::endl
             << std::endl;
        if(packVariables) {
            ovxC << "    FILE * fp__variables = fopen(binaryFilename, \"rb\");" << std::endl
                 << "    if(!fp__variables) {" << std::endl
                 << "        vxAddLogEntry((vx_reference)context, VX_FAILURE, \"ERROR: unable to open: %s\\n\", binaryFilename);" << std::endl
                 << "        return VX_FAILURE;" << std::endl
                 << "    }" << std::endl
                 << "    { vx_uint32 magic = 0;" << std::endl
                 << "      fread(&magic, 1, sizeof(magic), fp__variables);" << std::endl
                 << "      if(magic != 0x" << std::hex << VARIABLES_FILE_MAGIC << std::dec << ") {" << std::endl
                 << "        vxAddLogEntry((vx_reference)context, VX_FAILURE, \"ERROR: invalid file magic in %s\\n\", binaryFilename);" << std::endl
                 << "        return VX_FAILURE;" << std::endl
                 << "      }" << std::endl
                 << "    }" << std::endl
                 << std::endl;
        }

        ////
        // initialize tensor binary
        //
        if(packVariables) {
            variablesFilename = openvxFolder + "/weights.bin";
            fpVariables = fopen(variablesFilename.c_str(), "wb");
            if(!fpVariables) {
                printf("ERROR: unable to create: %s\n", variablesFilename.c_str());
                exit(1);
            }
            unsigned int magic = VARIABLES_FILE_MAGIC;
            fwrite(&magic, 1, sizeof(magic), fpVariables);
        }
    }

    virtual void endGraph( const nnef::Prototype& proto )
    {
        // show NNEF syntax
        if(verbose & 1) {
            std::cout << '}' << std::endl;
        }

        ////
        // finalize variables file
        //
        if(packVariables) {
            unsigned int magic = VARIABLES_EOFF_MAGIC;
            fwrite(&magic, 1, sizeof(magic), fpVariables);
            fclose(fpVariables);
            std::cout << "OK: created '" << variablesFilename << "'" << std::endl;
        }

        ////
        // generate clean-up code
        //
        if(packVariables) {
            ovxC << std::endl
                 << "    { vx_uint32 magic = 0;" << std::endl
                 << "      fread(&magic, 1, sizeof(magic), fp__variables);" << std::endl
                 << "      if(magic != 0x" << std::hex << VARIABLES_EOFF_MAGIC << std::dec << ") {" << std::endl
                 << "        vxAddLogEntry((vx_reference)context, VX_FAILURE, \"ERROR: invalid eoff magic in %s\\n\", binaryFilename);" << std::endl
                 << "        return VX_FAILURE;" << std::endl
                 << "      }" << std::endl
                 << "      fclose(fp__variables);" << std::endl
                 << "    }" << std::endl;
        }
        ovxC << std::endl;
        for(auto& name : virtualList) {
            ovxC << "    ERROR_CHECK_STATUS(vxReleaseTensor(&" << name << "));" << std::endl;
        }
        for(auto& name : variableList) {
            ovxC << "    ERROR_CHECK_STATUS(vxReleaseTensor(&" << name << "));" << std::endl;
        }
        ovxC << std::endl;
        ovxC << "    return VX_SUCCESS;" << std::endl;
        ovxC << "}" << std::endl;
        ovxC.close();
        std::cout << "OK: created '" << openvxFilenameC << "'" << std::endl;

        ////
        // generate OpenVX header file
        //
        openvxFilenameC = openvxFolder + "/annmodule.h";
        ovxC.open(openvxFilenameC);
        if(!ovxC) {
            printf("ERROR: unable to create: %s\n", openvxFilenameC.c_str());
            exit(1);
        }
        ovxC << "#ifndef included_file_annmodule_h" << std::endl
             << "#define included_file_annmodule_h" << std::endl
             << std::endl
             << "#include <VX/vx.h>" << std::endl
             << std::endl;
        ovxC << "////" << std::endl
             << "// initialize graph neural network for inference" << std::endl;
        for(auto& name : inputList) {
            if(inputShape.find(name) != inputShape.end()) {
                std::vector<size_t> dims;
                getTensorDims(inputShape[name], true, dims);
                ovxC << "//   " << name << " -- dims[] = {";
                for(size_t i = 0; i < dims.size(); i++) {
                    ovxC << (i == 0 ? " " : ", ") << dims[i];
                }
                ovxC << " } (input)" << std::endl;
            }
        }
        for(auto& name : outputList) {
            if(outputShape.find(name) != outputShape.end()) {
                std::vector<size_t> dims;
                getTensorDims(outputShape[name], true, dims);
                ovxC << "//   " << name << " -- dims[] = {";
                for(size_t i = 0; i < dims.size(); i++) {
                    ovxC << (i == 0 ? " " : ", ") << dims[i];
                }
                ovxC << " } (output)" << std::endl;
            }
        }
        ovxC << "//" << std::endl
             << "vx_status annCreateGraph(vx_graph graph";
        for(auto& name : inputList) {
            ovxC << ", vx_tensor " << name;
        }
        for(auto& name : outputList) {
            ovxC << ", vx_tensor " << name;
        }
        if(packVariables) {
            ovxC << ", const char * binaryFilename";
        }
        ovxC << ");" << std::endl
             << std::endl
             << "#endif" << std::endl;
        ovxC.close();
        std::cout << "OK: created '" << openvxFilenameC << "'" << std::endl;

        ////
        // generate a simple test program
        //
        openvxFilenameC = openvxFolder + "/anntest.cpp";
        ovxC.open(openvxFilenameC);
        if(!ovxC) {
            printf("ERROR: unable to create: %s\n", openvxFilenameC.c_str());
            exit(1);
        }
        ovxC << "#include \"annmodule.h\"" << std::endl
             << "#include <vx_ext_amd.h>" << std::endl
             << "#include <iostream>" << std::endl
             << "#include <stdio.h>" << std::endl
             << "#include <string.h>" << std::endl
             << "#include <string>" << std::endl
             << "#include <inttypes.h>" << std::endl
             << "#include <chrono>" << std::endl
             << "#include <unistd.h>" << std::endl
             << "" << std::endl
             << "#if ENABLE_OPENCV" << std::endl
             << "#include <opencv2/opencv.hpp>" << std::endl
             << "#include <opencv/cv.h>" << std::endl
             << "#include <opencv/highgui.h>" << std::endl
             << "using namespace cv; " << std::endl
             << "#endif" << std::endl
             << "" << std::endl
             << "#define ERROR_CHECK_STATUS(call) { vx_status status = (call); if(status != VX_SUCCESS) { printf(\"ERROR: failed with status = (%d) at \" __FILE__ \"#%d\", status, __LINE__); return -1; } }" << std::endl
             << "" << std::endl
             << "static void VX_CALLBACK log_callback(vx_context context, vx_reference ref, vx_status status, const vx_char string[])" << std::endl
             << "{" << std::endl
             << "    size_t len = strlen(string);" << std::endl
             << "    if (len > 0) {" << std::endl
             << "        printf(\"%s\", string);" << std::endl
             << "        if (string[len - 1] != '\\n')" << std::endl
             << "            printf(\"\\n\");" << std::endl
             << "        fflush(stdout);" << std::endl
             << "    }" << std::endl
             << "}" << std::endl
             << "" << std::endl
             << "inline int64_t clockCounter()" << std::endl
             << "{" << std::endl
             << "    return std::chrono::high_resolution_clock::now().time_since_epoch().count();" << std::endl
             << "}" << std::endl
             << "" << std::endl
             << "inline int64_t clockFrequency()" << std::endl
             << "{" << std::endl
             << "    return std::chrono::high_resolution_clock::period::den / std::chrono::high_resolution_clock::period::num;" << std::endl
             << "}" << std::endl
             << "" << std::endl
             << "static vx_status copyTensor(vx_tensor tensor, std::string fileName, vx_enum usage = VX_WRITE_ONLY)" << std::endl
             << "{" << std::endl
             << "    vx_enum data_type = VX_TYPE_FLOAT32;" << std::endl
             << "    vx_size num_of_dims = 4, dims[4] = { 1, 1, 1, 1 }, stride[4];" << std::endl
             << "    vxQueryTensor(tensor, VX_TENSOR_DATA_TYPE, &data_type, sizeof(data_type));" << std::endl
             << "    vxQueryTensor(tensor, VX_TENSOR_NUMBER_OF_DIMS, &num_of_dims, sizeof(num_of_dims));" << std::endl
             << "    vxQueryTensor(tensor, VX_TENSOR_DIMS, &dims, sizeof(dims[0])*num_of_dims);" << std::endl
             << "    vx_size itemsize = sizeof(float);" << std::endl
             << "    if(data_type == VX_TYPE_UINT8 || data_type == VX_TYPE_INT8) {" << std::endl
             << "        itemsize = sizeof(vx_uint8);" << std::endl
             << "    }" << std::endl
             << "    else if(data_type == VX_TYPE_UINT16 || data_type == VX_TYPE_INT16 || data_type == VX_TYPE_FLOAT16) {" << std::endl
             << "        itemsize = sizeof(vx_uint16);" << std::endl
             << "    }" << std::endl
             << "    vx_size count = dims[0] * dims[1] * dims[2] * dims[3];" << std::endl
             << "    vx_map_id map_id;" << std::endl
             << "    float * ptr;" << std::endl
             << "    vx_status status = vxMapTensorPatch(tensor, num_of_dims, nullptr, nullptr, &map_id, stride, (void **)&ptr, usage, VX_MEMORY_TYPE_HOST, 0);" << std::endl
             << "    if(status) {" << std::endl
             << "        std::cerr << \"ERROR: vxMapTensorPatch() failed for \" << fileName << std::endl;" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    if(usage == VX_WRITE_ONLY) {" << std::endl
             << "#if ENABLE_OPENCV" << std::endl
             << "        if(dims[3] == 1 && dims[2] == 3 && fileName.size() > 4 && (fileName.substr(fileName.size()-4, 4) == \".png\" || fileName.substr(fileName.size()-4, 4) == \".jpg\"))" << std::endl
             << "        {" << std::endl
             << "            Mat img = imread(fileName.c_str(), CV_LOAD_IMAGE_COLOR);" << std::endl
             << "            if(!img.data || img.rows != dims[1] || img.cols != dims[0]) {" << std::endl
             << "                std::cerr << \"ERROR: invalid image or dimensions in \" << fileName << std::endl;" << std::endl
             << "                return -1;" << std::endl
             << "            }" << std::endl
             << "            unsigned char * src = img.data;" << std::endl
             << "            for(vx_size c = 0; c < 3; c++) {" << std::endl
             << "                for(vx_size y = 0; y < dims[1]; y++) {" << std::endl
             << "                    for(vx_size x = 0; x < dims[0]; x++) {" << std::endl
             << "                        ptr[(c*stride[2]+y*stride[1]+x*stride[0])>>2] = src[y*dims[0]*3+x*3+c];" << std::endl
             << "                    }" << std::endl
             << "                }" << std::endl
             << "            }" << std::endl
             << "        }" << std::endl
             << "        else" << std::endl
             << "#endif" << std::endl
             << "        {" << std::endl
             << "            FILE * fp = fopen(fileName.c_str(), \"rb\");" << std::endl
             << "            if(!fp) {" << std::endl
             << "                std::cerr << \"ERROR: unable to open: \" << fileName << std::endl;" << std::endl
             << "                return -1;" << std::endl
             << "            }" << std::endl
             << "            vx_size n = fread(ptr, itemsize, count, fp);" << std::endl
             << "            fclose(fp);" << std::endl
             << "            if(n != count) {" << std::endl
             << "                std::cerr << \"ERROR: expected char[\" << count*itemsize << \"], but got char[\" << n*itemsize << \"] in \" << fileName << std::endl;" << std::endl
             << "                return -1;" << std::endl
             << "            }" << std::endl
             << "        }" << std::endl
             << "    }" << std::endl
             << "    else {" << std::endl
             << "        FILE * fp = fopen(fileName.c_str(), \"wb\");" << std::endl
             << "        if(!fp) {" << std::endl
             << "            std::cerr << \"ERROR: unable to open: \" << fileName << std::endl;" << std::endl
             << "            return -1;" << std::endl
             << "        }" << std::endl
             << "        fwrite(ptr, itemsize, count, fp);" << std::endl
             << "        fclose(fp);" << std::endl
             << "    }" << std::endl
             << "    status = vxUnmapTensorPatch(tensor, map_id);" << std::endl
             << "    if(status) {" << std::endl
             << "        std::cerr << \"ERROR: vxUnmapTensorPatch() failed for \" << fileName << std::endl;" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    return 0;" << std::endl
             << "}" << std::endl
             << "" << std::endl
             << "int main(int argc, const char ** argv)" << std::endl
             << "{" << std::endl
             << "    // check command-line usage" << std::endl
             << "    if(argc < 2) {" << std::endl
             << "        printf(\"Usage: anntest <weights.bin> [<input/output-filename(s)>...]\\n\");" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    const char * binaryFilename = argv[1];" << std::endl
             << "    argc -= 2;" << std::endl
             << "    argv += 2;" << std::endl
             << "" << std::endl
             << "    // create context, input, output, and graph" << std::endl
             << "    vxRegisterLogCallback(NULL, log_callback, vx_false_e);" << std::endl
             << "    vx_context context = vxCreateContext();" << std::endl
             << "    if(vxGetStatus((vx_reference)context)) {" << std::endl
             << "        printf(\"ERROR: vxCreateContext() failed\\n\");" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    vxRegisterLogCallback(context, log_callback, vx_false_e);" << std::endl
             << "" << std::endl
             << "    // create input tensors and initialize" << std::endl
            ;
        for(auto& name : inputList) {
            std::vector<size_t> dims;
            getTensorDims(inputShape[name], true, dims);
            ovxC << "    vx_size " << name << "_dims[" << dims.size() << "] = {";
            for(size_t i = 0; i < dims.size(); i++) {
                ovxC << (i == 0 ? " " : ", ") << dims[i];
            }
            ovxC << " };" << std::endl
                 << "    vx_tensor " << name << " = vxCreateTensor(context, " << dims.size() << ", " << name << "_dims, VX_TYPE_FLOAT32, 0);" << std::endl
                 << "    if(vxGetStatus((vx_reference)" << name << ")) {" << std::endl
                 << "        printf(\"ERROR: vxCreateTensor() failed for " << name << "\\n\");" << std::endl
                 << "        return -1;" << std::endl
                 << "    }" << std::endl
                 << "    if(*argv) {" << std::endl
                 << "        if(strcmp(*argv, \"-\") != 0) {" << std::endl
                 << "            if(copyTensor(" << name << ", *argv, VX_WRITE_ONLY) < 0) {" << std::endl
                 << "                return -1;" << std::endl
                 << "            }" << std::endl
                 << "            printf(\"OK: read tensor '" << name << "' from %s\\n\", *argv);" << std::endl
                 << "        }" << std::endl
                 << "        argv++;" << std::endl
                 << "    }" << std::endl
                ;
        }
        ovxC << "    // create output tensors" << std::endl;
        for(auto& name : outputList) {
            std::vector<size_t> dims;
            getTensorDims(outputShape[name], true, dims);
            ovxC << "    vx_size " << name << "_dims[" << dims.size() << "] = {";
            for(size_t i = 0; i < dims.size(); i++) {
                ovxC << (i == 0 ? " " : ", ") << dims[i];
            }
            ovxC << " };" << std::endl
                 << "    vx_tensor " << name << " = vxCreateTensor(context, " << dims.size() << ", " << name << "_dims, VX_TYPE_FLOAT32, 0);" << std::endl
                 << "    if(vxGetStatus((vx_reference)" << name << ")) {" << std::endl
                 << "        printf(\"ERROR: vxCreateTensor() failed for " << name << "\\n\");" << std::endl
                 << "        return -1;" << std::endl
                 << "    }" << std::endl;
        }
        ovxC << "" << std::endl
             << "    // build graph using annmodule" << std::endl
             << "    vx_status status;" << std::endl
             << "    int64_t freq = clockFrequency(), t0, t1;" << std::endl
             << "    t0 = clockCounter();" << std::endl
             << "    vx_graph graph = vxCreateGraph(context);" << std::endl
             << "    status = vxGetStatus((vx_reference)graph);" << std::endl
             << "    if(status) {" << std::endl
             << "        printf(\"ERROR: vxCreateGraph(...) failed (%d)\\n\", status);" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    status = annCreateGraph(graph, "
            ;
        for(auto& name : inputList) {
            ovxC << name << ", ";
        }
        for(auto& name : outputList) {
            ovxC << name << ", ";
        }
        ovxC << "binaryFilename);" << std::endl
             << "    if(status) {" << std::endl
             << "        printf(\"ERROR: annCreateGraph() failed (%d)\\n\", status);" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    status = vxVerifyGraph(graph);" << std::endl
             << "    if(status) {" << std::endl
             << "        printf(\"ERROR: vxVerifyGraph(...) failed (%d)\\n\", status);" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    t1 = clockCounter();" << std::endl
             << "    printf(\"OK: graph initialization with annCreateGraph() took %.3f msec\\n\", (float)(t1-t0)*1000.0f/(float)freq);" << std::endl
             << "" << std::endl
             << "    t0 = clockCounter();" << std::endl
             << "    status = vxProcessGraph(graph);" << std::endl
             << "    t1 = clockCounter();" << std::endl
             << "    if(status != VX_SUCCESS) {" << std::endl
             << "        printf(\"ERROR: vxProcessGraph() failed (%d)\\n\", status);" << std::endl
             << "        return -1;" << std::endl
             << "    }" << std::endl
             << "    printf(\"OK: vxProcessGraph() took %.3f msec (1st iteration)\\n\", (float)(t1-t0)*1000.0f/(float)freq);" << std::endl
             << "" << std::endl
             << "    // write outputs" << std::endl
            ;
        for(auto& name : outputList) {
            ovxC << "    if(*argv) {" << std::endl
                 << "        if(strcmp(*argv, \"-\") != 0) {" << std::endl
                 << "            if(copyTensor(" << name << ", *argv, VX_READ_ONLY) < 0) {" << std::endl
                 << "                return -1;" << std::endl
                 << "            }" << std::endl
                 << "            printf(\"OK: wrote tensor '" << name << "' into %s\\n\", *argv);" << std::endl
                 << "        }" << std::endl
                 << "        argv++;" << std::endl
                 << "    }" << std::endl
                ;
        }
        ovxC << "" << std::endl
             << "    t0 = clockCounter();" << std::endl
             << "    int N = 100;" << std::endl
             << "    for(int i = 0; i < N; i++) {" << std::endl
             << "        status = vxProcessGraph(graph);" << std::endl
             << "        if(status != VX_SUCCESS)" << std::endl
             << "            break;" << std::endl
             << "    }" << std::endl
             << "    t1 = clockCounter();" << std::endl
             << "    printf(\"OK: vxProcessGraph() took %.3f msec (average over %d iterations)\\n\", (float)(t1-t0)*1000.0f/(float)freq/(float)N, N);" << std::endl
             << "" << std::endl
             << "    // release resources" << std::endl
             << "    ERROR_CHECK_STATUS(vxReleaseGraph(&graph));" << std::endl
            ;
        for(auto& name : inputList) {
            ovxC << "    ERROR_CHECK_STATUS(vxReleaseTensor(&" << name << "));" << std::endl;
        }
        for(auto& name : outputList) {
            ovxC << "    ERROR_CHECK_STATUS(vxReleaseTensor(&" << name << "));" << std::endl;
        }
        ovxC << "    ERROR_CHECK_STATUS(vxReleaseContext(&context));" << std::endl
             << "    printf(\"OK: successful\\n\");" << std::endl
             << "" << std::endl
             << "    return 0;" << std::endl
             << "}" << std::endl
             ;
        ovxC.close();
        std::cout << "OK: created '" << openvxFilenameC << "'" << std::endl;

        ////
        // generate CMakeLists.txt
        //
        openvxFilenameC = openvxFolder + "/CMakeLists.txt";
        ovxC.open(openvxFilenameC);
        if(!ovxC) {
            printf("ERROR: unable to create: %s\n", openvxFilenameC.c_str());
            exit(1);
        }
        ovxC << "cmake_minimum_required (VERSION 2.8)" << std::endl
             << "project (annmodule)" << std::endl
             << "set (CMAKE_CXX_STANDARD 11) " << std::endl
             << "list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)" << std::endl
             << "find_package(OpenCL REQUIRED)" << std::endl
             << "find_package(OpenCV QUIET)" << std::endl
             << "include_directories (${OpenCL_INCLUDE_DIRS} ${OpenCL_INCLUDE_DIRS}/Headers )" << std::endl
             << "include_directories (/opt/rocm/include)" << std::endl
             << "link_directories    (/opt/rocm/lib)" << std::endl
             << "list(APPEND SOURCES annmodule.cpp)" << std::endl
             << "add_library(${PROJECT_NAME} SHARED ${SOURCES})" << std::endl
             << "set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -msse4.2 -std=c++11\")" << std::endl
             << "target_link_libraries(${PROJECT_NAME} openvx vx_nn pthread)" << std::endl
             << "add_executable(anntest anntest.cpp)" << std::endl
             << "if (OpenCV_FOUND)" << std::endl
             << "  target_compile_definitions(anntest PUBLIC ENABLE_OPENCV=1)" << std::endl
             << "  include_directories(${OpenCV_INCLUDE_DIRS})" << std::endl
             << "  target_link_libraries(anntest ${OpenCV_LIBRARIES})" << std::endl
             << "else(OpenCV_FOUND)" << std::endl
             << "  target_compile_definitions(anntest PUBLIC ENABLE_OPENCV=0)" << std::endl
             << "endif(OpenCV_FOUND)" << std::endl
             << "target_link_libraries(anntest openvx vx_nn pthread ${PROJECT_NAME})" << std::endl
            ;
        ovxC.close();
        std::cout << "OK: created '" << openvxFilenameC << "'" << std::endl;
    }

    virtual void operation(const nnef::Prototype& proto,
                           const nnef::Dictionary<nnef::Value>& args,
                           const nnef::Dictionary<nnef::Shape>& shapes )
    {
        // show NNEF syntax
        if(verbose & 1) {
            std::cout << '\t';
            for ( size_t i = 0; i < proto.resultCount(); ++i ) {
                auto& result = proto.result(i);
                if ( i ) std::cout << ", ";
                std::cout << args[result.name()];
            }
            std::cout << " = " << proto.name() << "(";
            for ( size_t i = 0; i < proto.paramCount(); ++i ) {
                auto& param = proto.param(i);
                if ( i ) std::cout << ", ";
                if ( !param.type()->isTensor() )
                    std::cout << param.name() << " = ";
                std::cout << args[param.name()];
            }
            std::cout << ")" << std::endl;
        }

        ////
        // utility functions
        //
        auto getTensorOrScalar = [] (const nnef::Value& v) -> std::string {
            std::string value = "0";
            if(v) {
                if(v.kind() == nnef::Value::Tensor) {
                    value = v.tensor().id;
                }
                else if(v.kind() == nnef::Value::Scalar) {
                    value = std::to_string(v.scalar());
                }
            }
            return value;
        };
        auto getExtentArray = [] (const nnef::Value& v) -> std::vector<size_t> {
            std::vector<size_t> value;
            if(v && v.kind() == nnef::Value::Array) {
                auto&& a = v.array();
                for(auto& i : a) {
                    value.push_back(i.integer());
                }
            }
            return value;
        };
        auto getPaddingInfo = [] (const nnef::Value& v, size_t pad[4]) {
            std::vector<size_t> value;
            if(v && v.kind() == nnef::Value::Array) {
                auto&& a = v.array();
                if(a.size() == 2) {
                    pad[0] = a[0][0].integer();
                    pad[1] = a[0][1].integer();
                    pad[2] = a[1][0].integer();
                    pad[3] = a[1][1].integer();
                }
            }
        };
        auto codeGenTensorCreate = [](std::ofstream& ovxC, const std::string& name, const nnef::Shape& shape, bool isVirtual, size_t num_dims = 0) {
            std::vector<size_t> dims;
            getTensorDims(shape, isVirtual, dims, num_dims);
            ovxC << "    vx_size " << name << "_dims[" << dims.size() << "] = {";
            for(size_t i = 0; i < dims.size(); i++) {
                ovxC << (i == 0 ? " " : ", ") << dims[i];
            }
            ovxC << " };" << std::endl;
            ovxC << "    vx_tensor " << name << " = "
                 << (isVirtual ? "vxCreateVirtualTensor(graph, " : "vxCreateTensor(context, ")
                 << dims.size() << ", " << name << "_dims, VX_TYPE_FLOAT32, 0);" << std::endl;
            ovxC << "    ERROR_CHECK_OBJECT(" << name << ");" << std::endl;
        };
        auto copyTensorFile = [](const std::string& nnefFolder, const std::string& label, const nnef::Shape& shape, FILE * fpVariables) {
            std::string fileName = nnefFolder + "/" + label + ".dat";
            FILE * fp = fopen(fileName.c_str(), "rb");
            if(!fp) {
                printf("ERROR: unable to open: %s\n", fileName.c_str());
                exit(1);
            }
            enum TensorDataType : unsigned char {
                TensorDataType_Float,
                TensorDataType_Quantized,
                TensorDataType_Signed,
                TensorDataType_Unsigned
            };
            struct TensorFileHeader {
                unsigned char  magic[2];
                unsigned char  major;
                unsigned char  minor;
                unsigned int   offset;
                unsigned int   rank;
                unsigned int   dim[8];
                unsigned char  data_type;
                unsigned char  bit_width;
                unsigned short quant_alg_len;
                char           quant_alg[1024];
            } h = { 0 };
            unsigned int offset = 0;
            offset += fread(&h.magic, 1, sizeof(h.magic), fp);
            offset += fread(&h.major, 1, sizeof(h.major), fp);
            offset += fread(&h.minor, 1, sizeof(h.minor), fp);
            offset += fread(&h.offset, 1, sizeof(h.offset), fp);
            offset += fread(&h.rank, 1, sizeof(h.rank), fp);
            if(h.rank > 0) {
                offset += fread(h.dim, 1, h.rank * sizeof(h.dim[0]), fp);
            }
            offset += fread(&h.data_type, 1, sizeof(h.data_type), fp);
            offset += fread(&h.bit_width, 1, sizeof(h.bit_width), fp);
            offset += fread(&h.quant_alg_len, 1, sizeof(h.quant_alg_len), fp);
            if(h.quant_alg_len > 0) {
                offset += fread(h.quant_alg, 1, h.quant_alg_len, fp);
            }
            if(h.magic[0] != 0x4e || h.magic[1] != 0xef || h.major != 1 || h.minor != 0
                                  || h.bit_width == 0 || h.rank > 8 || h.quant_alg_len >= 1024
                                  || (12 + h.rank * 4 + 4 + h.quant_alg_len) != offset || h.offset < offset)
            {
                printf("ERROR: invalid or unsupported tensor file: %s\n", fileName.c_str());
                printf(" [ 0x%02x, 0x%02x, %d, %d, %d, %d, {", h.magic[0], h.magic[1], h.major, h.minor, h.offset, h.rank);
                for(unsigned int i = 0; i < h.rank; i++) printf(" %d", h.dim[i]);
                printf(" }, %d, %d, %d, '%s' ] offset = %d\n", h.data_type, h.bit_width, h.quant_alg_len, h.quant_alg, offset);
                exit(1);
            }
            if(h.offset > offset) {
                fseek(fp, h.offset, SEEK_SET);
            }
            unsigned int size = h.bit_width;
            for(unsigned int i = 0; i < h.rank; i++) {
                size *= h.dim[i];
                if(h.dim[i] != shape[i]) {
                    printf("ERROR: dimension[%d] mismatch: %d in %s (must be %d)\n", i, h.dim[i], fileName.c_str(), shape[i]);
                    exit(1);
                }
            }
            size = (size + 7) >> 3;
            unsigned int magic = VARIABLES_DATA_MAGIC;
            fwrite(&magic, 1, sizeof(magic), fpVariables);
            fwrite(&size, 1, sizeof(size), fpVariables);
            if(h.data_type == TensorDataType_Float && h.bit_width == 32) {
                for(offset = 0; offset < size; ) {
                    char buf[8192];
                    unsigned int N = std::min(size-offset, (unsigned int)sizeof(buf));
                    unsigned int n = fread(buf, 1, N, fp);
                    if(n != N) {
                        printf("ERROR: unable to read %d bytes of data from %s\n", size, fileName.c_str());
                        exit(1);
                    }
                    fwrite(buf, 1, n, fpVariables);
                    offset += n;
                }
            }
            else if(h.data_type == TensorDataType_Float && h.bit_width == 16) {
                for(offset = 0; offset < size; ) {
                    char buf[2048];
                    unsigned int N = std::min(size-offset, (unsigned int)sizeof(buf));
                    unsigned int n = fread(buf, 1, N, fp);
                    if(n != N) {
                        printf("ERROR: unable to read %d bytes of data from %s\n", size, fileName.c_str());
                        exit(1);
                    }
                    unsigned int k = n * 8 / h.bit_width;
                    unsigned int fbuf[1024];
                    for(unsigned int i = 0; i < k; i++) {
                        unsigned short fp16 = ((unsigned short *)buf)[i];
                        unsigned int fp32  = ((fp16 & 0x7fff) << 13) + ((127 - 15) << 23);
                        unsigned int exp = fp32 & (0x7c00 << 13);
                        if (exp == (0x7c00 << 13)) {
                            fp32 += (128 - 16) << 23;
                        }
                        else if (exp == 0) {
                            unsigned int magic = (113 << 23);
                            fp32 += (1 << 23);
                            *(float *)&fp32 -= *(float *)&magic;
                        }
                        fp32 |= ((fp16 & 0x8000) << 16);
                        fbuf[i] = fp32;
                    }
                    fwrite(fbuf, sizeof(unsigned int), k, fpVariables);
                    offset += n;
                }
            }
            else if(h.data_type == TensorDataType_Quantized && h.bit_width > 0) {
                float fmin = 0, fmax = 1;
                char s[1024] = { 0 };
                for(unsigned int i = 0, j = 0; h.quant_alg[i]; i++) {
                    if(h.quant_alg[i] != ' ' && h.quant_alg[i] != '\t')
                        s[j++] = h.quant_alg[i];
                }
                if(strstr(s, "linear_quantize(") && strstr(s, "min=") && strstr(s, "max=")) {
                    const char * p;
                    if((p = strstr(s, "min=")) != nullptr) {
                        fmin = (float)atof(p+4);
                    }
                    if((p = strstr(s, "max=")) != nullptr) {
                        fmax = (float)atof(p+4);
                    }
                }
                else {
                    printf("ERROR: unsupported quantization algorithm '%s' in %s\n", h.quant_alg, fileName.c_str());
                    exit(1);
                }
                for(offset = 0; offset < size; ) {
                    char buf[4096];
                    unsigned int maxN = (1024 * h.bit_width) / 8;
                    unsigned int N = std::min(size-offset, maxN);
                    unsigned int n = fread(buf, 1, N, fp);
                    if(n != N) {
                        printf("ERROR: unable to read %d bytes of data from %s\n", size, fileName.c_str());
                        exit(1);
                    }
                    offset += n;
                    float f[1024];
                    unsigned int k = maxN * 8 / h.bit_width;
                    for(unsigned int i = 0; i < k; i++) {
                        unsigned int v = 0;
                        for(unsigned int j = 0; j < h.bit_width; j++) {
                            unsigned int l = i * h.bit_width + j;
                            v |= ((buf[l >> 3] >> (l & 7)) << j);
                        }
                        f[i] = fmin + (float)v * (fmax - fmin) / (float) ((1 << h.bit_width) - 1);
                    }
                    fwrite(f, sizeof(float), k, fpVariables);
                }
            }
            else {
                printf("ERROR: import of Tensor DataType=%d BitWidth=%d is not yet supported\n", h.data_type, h.bit_width);
                exit(1);
            }
            fclose(fp);
        };

        ////
        // process operations
        //
        std::string opname = proto.name();
        if(opname == "external") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << std::endl;
            }
            inputShape[output] = shape;
        }
        else if(opname == "variable") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& label = args["label"].string();
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " label=" << label << std::endl;
            }
            codeGenTensorCreate(ovxC, output, shape, false);
            if(packVariables) {
                copyTensorFile(nnefFolder, label, shape, fpVariables);
                ovxC << "    ERROR_CHECK_STATUS(initializeTensor(context, " << output << ", fp__variables, binaryFilename));" << std::endl;
            }
            variableList.push_back(output);
        }
        else if(opname == "conv") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["input"].tensor().id;
            const std::string& filter = args["filter"].tensor().id;
            const std::string& bias = getTensorOrScalar(args["bias"]);
            const std::string& border = args["border"].string();
            const auto& padding = args["padding"];
            const auto& stride = args["stride"];
            const auto& dilation = args["dilation"];
            const auto& groups = args["groups"] ? args["groups"].integer() : 1;
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input << " " << filter << " " << bias
                          << " border=" << border << " " << padding << " " << stride << " " << dilation << " " << groups << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            if(shape[2] == 1 && shape[3] == 1) {
                ovxC << "    { vx_node node = vxFullyConnectedLayer(graph, " << input << ", " << filter << ", "
                     << ((bias[0] == '0') ? "NULL" : bias) << ", VX_CONVERT_POLICY_SATURATE, VX_ROUND_POLICY_TO_NEAREST_EVEN, " << output << ");" << std::endl;
                ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
                ovxC << "    }" << std::endl;
            }
            else {
                std::vector<size_t>&& vDilation = getExtentArray(dilation);
                size_t pad[4] = { 0, 0, 0, 0 };
                getPaddingInfo(padding, pad);
                ovxC << "    { vx_nn_convolution_params_t conv_params = { 0 };" << std::endl;
                ovxC << "      conv_params.padding_x = " << pad[1] << ";" << std::endl;
                ovxC << "      conv_params.padding_y = " << pad[0] << ";" << std::endl;
                ovxC << "      conv_params.dilation_x = " << (vDilation.size() > 1 ? vDilation[1] - 1 : 0) << ";" << std::endl;
                ovxC << "      conv_params.dilation_y = " << (vDilation.size() > 0 ? vDilation[0] - 1 : 0) << ";" << std::endl;
                ovxC << "      conv_params.overflow_policy = " << "VX_CONVERT_POLICY_SATURATE" << ";" << std::endl;
                ovxC << "      conv_params.rounding_policy = " << "VX_ROUND_POLICY_TO_NEAREST_EVEN" << ";" << std::endl;
                ovxC << "      conv_params.down_scale_size_rounding = " << "VX_NN_DS_SIZE_ROUNDING_FLOOR" << ";" << std::endl;
                ovxC << "      vx_node node = vxConvolutionLayer(graph, " << input << ", " << filter << ", "
                     << ((bias[0] == '0') ? "NULL" : bias) << ", &conv_params, sizeof(conv_params), " << output << ");" << std::endl;
                ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
                ovxC << "    }" << std::endl;
            }
        }
        else if(opname == "relu") {
            const std::string& output = args["y"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["x"].tensor().id;
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            ovxC << "    { vx_node node = vxActivationLayer(graph, " << input << ", VX_NN_ACTIVATION_RELU, 0.0f, 0.0f, " << output << ");" << std::endl;
            ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
            ovxC << "    }" << std::endl;
        }
        else if(opname == "max_pool") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["input"].tensor().id;
            const auto& size = args["size"];
            const std::string& border = args["border"].string();
            const auto& padding = args["padding"];
            const auto& stride = args["stride"];
            const auto& dilation = args["dilation"];
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input
                          << " size=" << size << " border=" << border << " " << padding << " " << stride << " " << dilation << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            std::vector<size_t>&& vSize = getExtentArray(size);
            size_t pad[4] = { 0, 0, 0, 0 };
            getPaddingInfo(padding, pad);
            ovxC << "    { vx_node node = vxPoolingLayer(graph, " << input << ", VX_NN_POOLING_MAX, "
                 << size[3] << ", " << size[2] << ", " << pad[1] << ", " << pad[0] << ", "
                 << "VX_ROUND_POLICY_TO_NEAREST_EVEN, " << output << ");" << std::endl;
            ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
            ovxC << "    }" << std::endl;
        }
        else if(opname == "avg_pool") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["input"].tensor().id;
            const auto& size = args["size"];
            const std::string& border = args["border"].string();
            const auto& padding = args["padding"];
            const auto& stride = args["stride"];
            const auto& dilation = args["dilation"];
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input
                          << " size=" << size << " border=" << border << " " << padding << " " << stride << " " << dilation << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            std::vector<size_t>&& vSize = getExtentArray(size);
            size_t pad[4] = { 0, 0, 0, 0 };
            getPaddingInfo(padding, pad);
            ovxC << "    { vx_node node = vxPoolingLayer(graph, " << input << ", VX_NN_POOLING_AVG, "
                 << size[3] << ", " << size[2] << ", " << pad[1] << ", " << pad[0] << ", "
                 << "VX_ROUND_POLICY_TO_NEAREST_EVEN, " << output << ");" << std::endl;
            ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
            ovxC << "    }" << std::endl;
        }
        else if(opname == "concat") {
            const std::string& output = args["value"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            std::vector<std::string> itemList;
            const auto& inputpar = args["values"];
            for(size_t i = 0; i < inputpar.size(); i++) {
                std::string name = inputpar[i].tensor().id;
                itemList.push_back(name);
            }
            const int axis = args["axis"].integer();
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " [";
                for(auto& v : itemList) std::cout << " " << v;
                std::cout << " ] axis=" << axis << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            ovxC << "    { vx_node node = vxConcatLayer(graph, " << output;
            for(auto& v : itemList) {
                ovxC << ", " << v;
            }
            for(size_t i = itemList.size(); i < 8; i++) {
                ovxC << ", NULL";
            }
            ovxC << ");" << std::endl;
            ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
            ovxC << "    }" << std::endl;
        }
        else if(opname == "batch_normalization") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["input"].tensor().id;
            const std::string& mean = args["mean"].tensor().id;
            const std::string& variance = args["variance"].tensor().id;
            std::string scale = getTensorOrScalar(args["scale"]);
            std::string offset = getTensorOrScalar(args["offset"]);
            const float epsilon = args["epsilon"].scalar();
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input
                          << " " << mean << " " << variance << " " << offset << " " << scale << " " << epsilon << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            ovxC << "    { vx_node node = vxBatchNormalizationLayer(graph, " << input << ", " << mean << ", " << variance
                 << ", " << (scale[0] == '1' ? "NULL" : scale) << ", " << (offset[0] == '0' ? "NULL" : offset)
                 << ", " << epsilon << ", " << output << ");" << std::endl;
            ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
            ovxC << "    }" << std::endl;
        }
        else if(opname == "add") {
            const std::string& output = args["z"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input1 = args["x"].tensor().id;
            const std::string& input2 = args["y"].tensor().id;
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input1 << " " << input2 << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            ovxC << "    { vx_node node = vxTensorAddNode(graph, " << input1 << ", " << input2 << ", VX_CONVERT_POLICY_SATURATE, " << output << ");" << std::endl;
            ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
            ovxC << "    }" << std::endl;
        }
        else if(opname == "softmax") {
            const std::string& output = args["y"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["x"].tensor().id;
            std::vector<size_t>&& axes = getExtentArray(args["axes"]);
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input << " " << args["axes"] << std::endl;
            }
            if(axes.size() != 1 || axes[0] != 1) {
                std::cout << "ERROR: " << opname << " with " << args["axes"] << " is *** not yet supported ***" << std::endl;
                exit(1);
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            ovxC << "    { vx_node node = vxSoftmaxLayer(graph, " << input << ", " << output << ");" << std::endl;
            ovxC << "      ERROR_CHECK_STATUS(vxReleaseNode(&node));" << std::endl;
            ovxC << "    }" << std::endl;
        }
        else if(opname == "sum_reduce") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["input"].tensor().id;
            const auto& axes = args["axes"];
            const bool normalize = args["normalize"].logical();
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input << " " << axes << " " << normalize << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            std::cout << opname << " *** not yet supported ***" << std::endl;
            exit(1);
        }
        else if(opname == "mean_reduce") {
            const std::string& output = args["output"].tensor().id;
            const nnef::Shape& shape = shapes[output];
            const std::string& input = args["input"].tensor().id;
            const auto& axes = args["axes"];
            if(verbose & 2) {
                std::cout << opname << " " << output << " " << shape << " " << input << " " << axes << std::endl;
            }
            if(std::find(outputList.begin(), outputList.end(), output) == outputList.end()) {
                codeGenTensorCreate(ovxC, output, shape, true);
                virtualList.push_back(output);
            }
            else {
                outputShape[output] = shape;
            }
            std::cout << opname << " *** not yet supported ***" << std::endl;
            exit(1);
        }
        else {
            std::cout << opname << " *** not yet supported ***" << std::endl;
            exit(1);
        }
    }
    
    virtual bool isAtomic( const nnef::Prototype& proto, const nnef::Dictionary<nnef::Value>& args )
    {
        static std::set<std::string> atomics =
        {
            "sqr", "sqrt", "min", "max",
            "softmax", "relu", "tanh", "sigmoid",
            "batch_normalization", "max_pool", "avg_pool",
            "quantize_linear", "quantize_logarithmic"
        };
        return atomics.find(proto.name()) != atomics.end();
    }
};

int main(int argc, const char * argv[])
{
    ////
    // get command-line parameters
    //
    int verbose = 0;
    bool packVariables = true;
    if(argc > 2 && !strcmp(argv[1], "-v")) {
        verbose = atoi(argv[2]);
        argc -= 2;
        argv += 2;
    }
    if(argc > 1 && !strcmp(argv[1], "--no-variables")) {
        packVariables = false;
        argc -= 1;
        argv += 1;
    }
    if(argc < 3) {
        printf("Usage: nnef2openvx [-v <verbose>] [--no-variables] <nnefContainerFolder> <openvxOutputFolder>\n");
        return -1;
    }
    std::string nnefContainedFolder = argv[1];
    std::string openvxOutputFolder = argv[2];
    std::string nnefFilename = nnefContainedFolder + "/graph.nnef";

    ////
    // parse NNEF structure and translate to OpenVX code
    //
    std::ifstream ifs(nnefFilename.c_str());
    if(!ifs) {
        printf("ERROR: unable to open: %s\n", nnefFilename.c_str());
        return -1;
    }
    printf("OK: parsing %s ...\n", nnefFilename.c_str());
    std::unique_ptr<nnef::Parser> parser((nnef::Parser*)new nnef::FlatParser());
    try {
        NNEF2OpenVX_Translator callback(nnefContainedFolder, openvxOutputFolder, verbose, packVariables);
        parser->parse(ifs, callback);
    }
    catch(nnef::Error e) {
        printf("Parse error: [%u:%u] %s\n", e.position().line, e.position().column, e.what());
        auto origin = e.position().origin;
        while(origin) {
            printf("... evaluated from [%u:%u]\n", origin->line, origin->column);
            origin = origin->origin;
        }
    }
    ifs.close();

    return 0;
}
