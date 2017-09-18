#include "utils.h"

Vec3f convert2float(Vec3b input){
	Vec3f output;
	output[0] = (float)input[0];
	output[1] = (float)input[1];
	output[2] = (float)input[2];
	return output;
}

Mat degamma(Mat image){
	int width = image.cols;
	int height = image.rows;
	Mat degamma_image;
	degamma_image.create(height, width, CV_32FC3);
	for (int i = 0; i < width; i++){
		for (int j = 0; j < height; j++){
			Vec3b intensity_in = image.at<Vec3b>(j, i);
			degamma_image.at<Vec3f>(j, i) = degamma(intensity_in);
		}
	}
	return degamma_image;
}

vector<Vec3f> degamma(vector<Vec3f> input){
	vector<Vec3f> output;
	for (int i = 0; i < input.size(); i++){
		Vec3b intensity_in = input.at(i);
		output.push_back(degamma(intensity_in));
	}
	return output;
}

Vec3f degamma(Vec3b intensity_in){
	Vec3f intensity_out;
	for (int n = 0; n < 3; n++){
		if (intensity_in[n]>(0.04045 * 255))
		{
			intensity_out[n] = (float)pow((((float)intensity_in[n] / 255 + 0.055) / (1 + 0.055)), (float)2.4) * 255;
		}
		else{
			intensity_out[n] = (float)intensity_in[n] / (float)12.92;
		}
	}
	return intensity_out;
}

Mat gamma(Mat image){
	int width = image.cols;
	int height = image.rows;
	Mat gamma_image;
	gamma_image.create(height, width, CV_8UC3);
	for (int i = 0; i < width; i++){
		for (int j = 0; j < height; j++){
			Vec3f intensity_in = image.at<Vec3f>(j, i);
			gamma_image.at<Vec3b>(j, i) = gamma(intensity_in);
		}
	}
	return gamma_image;
}

Vec3b gamma(Vec3f intensity_in){
	Vec3f intensity_out;
	for (int n = 0; n < 3; n++){
		if (intensity_in[n]>(0.0031308 * 255))
		{
			intensity_out[n] = ((float)pow(((float)intensity_in[n] / 255), (float)1 / (float)2.4)* (float)(1 + 0.055) - (float)0.055) * 255;
		}
		else{
			intensity_out[n] = (float)intensity_in[n] * (float)12.92;
		}
	}
	return intensity_out;
}

int show_image(char* name, Mat *image){
	namedWindow(name, WINDOW_AUTOSIZE);		// Create a window for display.
	imshow(name, *image);					// Show our image inside it.
	waitKey(0);
	return 0;
}

vector<Vec3f> create_real_colors(){
	vector<Vec3f> real_colors;
	Vec3b real_color;
	real_color.val[2] = 115; real_color.val[1] = 82;  real_color.val[0] = 68;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 194; real_color.val[1] = 150; real_color.val[0] = 130; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 98;  real_color.val[1] = 122; real_color.val[0] = 157; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 87;  real_color.val[1] = 108; real_color.val[0] = 67;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 133; real_color.val[1] = 128; real_color.val[0] = 177; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 103; real_color.val[1] = 189; real_color.val[0] = 170; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 214; real_color.val[1] = 126; real_color.val[0] = 44;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 80;  real_color.val[1] = 91;  real_color.val[0] = 166; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 193; real_color.val[1] = 90;  real_color.val[0] = 99;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 94;  real_color.val[1] = 60;  real_color.val[0] = 108; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 157; real_color.val[1] = 188; real_color.val[0] = 64;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 224; real_color.val[1] = 163; real_color.val[0] = 46;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 56;  real_color.val[1] = 61;  real_color.val[0] = 150; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 70;  real_color.val[1] = 148; real_color.val[0] = 73;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 175; real_color.val[1] = 54;  real_color.val[0] = 60;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 231; real_color.val[1] = 199; real_color.val[0] = 31;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 187; real_color.val[1] = 86;  real_color.val[0] = 149; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 8;   real_color.val[1] = 133; real_color.val[0] = 161; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 243; real_color.val[1] = 243; real_color.val[0] = 242; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 200; real_color.val[1] = 200; real_color.val[0] = 200; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 160; real_color.val[1] = 160; real_color.val[0] = 160; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 122; real_color.val[1] = 122; real_color.val[0] = 121; real_colors.push_back(degamma(real_color));
	real_color.val[2] = 85;  real_color.val[1] = 85;  real_color.val[0] = 85;  real_colors.push_back(degamma(real_color));
	real_color.val[2] = 52;  real_color.val[1] = 52;  real_color.val[0] = 52;  real_colors.push_back(degamma(real_color));
	return real_colors;
}

Mat *read_input(params parameter){
	// Create cv image
	Mat image;

	// Read input data
	if (parameter.input_image_type_OpenCV){
		image = imread(parameter.input_image_filename);
		parameter.width = image.cols;
		parameter.height = image.rows;
	}
	else{ // Input format raw:
		Mat input_image;
		input_image.create(parameter.height, parameter.width, CV_8UC3);

		FILE *fp = NULL;
		char *imagedata = NULL;
		int framesize = parameter.width*parameter.height * 3;
		fopen_s(&fp, parameter.input_image_filename, "rb");
		if (fp == NULL)
		{
			printf("ERROR: \t Input filename (%s) is invalid\n", parameter.input_image_filename);
			return NULL;
		}
		imagedata = (char*)malloc(sizeof(char)* framesize);
		fread(imagedata, sizeof(char), framesize, fp);
		memcpy(input_image.data, imagedata, framesize);
		free(imagedata);
		fclose(fp);

		image.create(parameter.height, parameter.width, CV_8UC3);
		cvtColor(input_image, image, COLOR_RGB2BGR);
	}
	printf("OK: \t Read input (%s)\n", parameter.input_image_filename);
	return &image;
}
