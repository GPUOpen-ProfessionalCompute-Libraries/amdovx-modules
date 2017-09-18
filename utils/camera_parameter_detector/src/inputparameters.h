#include <iostream>
#include <string>

using namespace std;

#ifndef PARAMS
#define PARAMS
class params{
public:
	// Values:
	params();
	char* input_image_filename;
	int height;
	int width;
	char* output_image_filename;
	char* colorchart_image_filename;
	char* correction_matrix_OpenCV_filename;
	char* correction_matrix_LOOM_filename;
	bool save_images;
	bool show_images;
	int colorspace; // 0: RGB, 1: YUV, 2: UV, 3: XYZ
	bool compute; // true: find colorchecker, calculate matrix; false: only apply matrix
	bool linear;

	// Functions:
	int read(int argc, char ** argv);
	int check();
};
#endif

int printhelp();