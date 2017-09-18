#include "inputparameters.h"

params::params(){
	input_image_filename = nullptr;
	input_image_type_OpenCV = true;
	height = 0; width = 0;
	output_image_filename = nullptr;
	colorchart_image_filename = nullptr;
	correction_matrix_OpenCV_filename = nullptr;
	correction_matrix_LOOM_filename = nullptr;
	save_images = false;
	show_images = false;
	colorspace = 0; // 0: RGB, 1: YUV, 2: UV, 3: XYZ
	linear = true;
	compute = true; // true: find colorchecker, calculate matrix; false: only apply matrix
}

int params::read(int argc, char ** argv){
	if (argc < 2) { // if to less input arguments, print helpstring
		printhelp();
		return -1;
	}
	for (int i = 0; i < argc; ++i) { // loop through arguments
		if (std::string(argv[i]) == "--input") {
			if (i + 1 < argc) { 
				input_image_filename = argv[++i]; 
			}
			else { 
				std::cerr << "--input option requires one argument." << std::endl;
				return -1;
			}
		}
		if (std::string(argv[i]) == "--help") {
			printhelp();
		}
		if (std::string(argv[i]) == "--output") {
			if (i + 1 < argc) { 
				output_image_filename = argv[++i];
			}
			else {
				std::cerr << "--output option requires one argument." << std::endl;
				return -1;
			}
		}		
		if (std::string(argv[i]) == "--colorchartImg") {
			if (i + 1 < argc) { 
				colorchart_image_filename = argv[++i];
			}
			else { 
				std::cerr << "--colorchartImg option requires one argument." << std::endl;
				return -1;
			}
		}	
		if (std::string(argv[i]) == "--correctionMatrixOpenCV") {
			if (i + 1 < argc) { 
				correction_matrix_OpenCV_filename = argv[++i]; 
			}
			else { 
				std::cerr << "--correctionMatrixOpenCV option requires one argument." << std::endl;
				return -1;
			}
		}
		if (std::string(argv[i]) == "--correctionMatrixLoom") {
			if (i + 1 < argc) {
				correction_matrix_LOOM_filename = argv[++i];
			}
			else {
				std::cerr << "--correctionMatrixLoom option requires one argument." << std::endl;
				return -1;
			}
		}	
		if (std::string(argv[i]) == "--width") {
			if (i + 1 < argc) {
				width = atoi(argv[++i]);
			}
			else {
				std::cerr << "--width option requires one argument." << std::endl;
				return -1;
			}
		}
		if (std::string(argv[i]) == "--height") {
			if (i + 1 < argc) {
				height = atoi(argv[++i]);
			}
			else { 
				std::cerr << "--height option requires one argument." << std::endl;
				return -1;
			}
		}		
		if (std::string(argv[i]) == "--apply") {
			compute = false;
		}		
		if (std::string(argv[i]) == "--saveImg") {
			save_images = true;
		}		
		if (std::string(argv[i]) == "--showImg") {
			show_images = true;
		}		
		if (std::string(argv[i]) == "--gamma_corrected") {
			linear = false;
		}
		if (std::string(argv[i]) == "--colorspace") {
			if (i + 1 < argc) {
				i++;
				if (string(argv[++i]) == "RGB")
					colorspace = 0;
				if (string(argv[++i]) == "YUV")
					colorspace = 1;
				if (string(argv[++i]) == "UV")
					colorspace = 2;
				if (string(argv[++i]) == "XYZ")
					colorspace = 3;
				if (string(argv[++i]) == "XZ")
					colorspace = 4;
			}
			else {
				std::cerr << "--colorspace space option requires one argument." << std::endl;
				return -1;
			}
		}
	}

	return 0;
}

int params::check(){
	if (input_image_filename == nullptr){
		std::cerr << "An input file is required\n" << std::endl;
		printhelp();
		return -1;
	}
	if (!compute && !correction_matrix_OpenCV_filename){
		std::cerr << "For applying a color correction matrix, a color correction matrix in a for OpenCV readable format is necessary!\n" << std::endl;
		return -1;
	}
	if (!compute && !output_image_filename){
		std::cerr << "For applying a color correction matrix, a output filename needs to be specified!\n" << std::endl;
		return -1;
	}
	if (string(input_image_filename).substr(string(input_image_filename).length() - 3) == "raw")
	{
		input_image_type_OpenCV = false;
		if (width == 0 || height == 0){
			std::cerr << "For an input of type raw, a width and height are necessary!\n" << std::endl;
			return -1;
		}
	}

	return 0;
}

int printhelp(){
	printf("Necessary arguments:\n");
	printf("--input \t\t\tfilename\t an input image as jpg, raw or other formats accepted by OpenCV, for raw please specify width and height\n");
	printf("\nOptional arguments:\n"
		"--help print this helptext"
		"\nFilenames:\n"
		"--output \t\t\tfilename \t define an output image, which will be color corrected\n"
		"--colorchartImg \t\tfilename \t defines a filename to save the image of the cutted and rotated colorchart\n"
		"--correctionMatrixOpenCV \tfilename \t saves the color correction matrix as readable type for OpenCV\n"
		"--correctionMatrixLoom \t\tfilename \t expands the file by another color correction matrix as readable type for Loom\n"
		"--width \t\t\tvalue \t\t specify the width for raw\n"
		"--height \t\t\tvalue \t\t specify the height for raw\n"
		"\nMore settings:\n"
		"--apply \t\t\t\t\t do not look for a colorchart, expects a correctionMatrixOpenCV\n"
		"--saveImg \t\t\t\t\t saves intermediate images in same folder\n"
		"--showImg \t\t\t\t\t shows intermediate images\n"
		"--gamma_corrected \t\t\t\t\t\t uses the gamma corrected value instead of converting them into linear colorspace"
		"--colorspace \t\t\tspace \t\t define colorspace to calculate correction matrix in, possible options: RGB, YUV, UV, XYZ\n");
	return 0;
}