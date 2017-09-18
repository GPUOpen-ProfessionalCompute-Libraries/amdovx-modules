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

Vec3f colorspace_conversion(Mat_<float> conversion_matrix, Vec3f input){
	if (conversion_matrix.rows != 3 || conversion_matrix.cols != 3){
		printf("conversion matrix has wrong dimensions");
		return 0;
	}
	Vec3f output;
	output[0] = conversion_matrix.at<float>(0, 0) * input[0] + conversion_matrix.at<float>(0, 1) * input[1] + conversion_matrix.at<float>(0, 2) * input[2];
	output[1] = conversion_matrix.at<float>(1, 0) * input[0] + conversion_matrix.at<float>(1, 1) * input[1] + conversion_matrix.at<float>(1, 2) * input[2];
	output[2] = conversion_matrix.at<float>(2, 0) * input[0] + conversion_matrix.at<float>(2, 1) * input[1] + conversion_matrix.at<float>(2, 2) * input[2];
	return output;
}

int colorspace_conversion(Mat_<float> conversion_matrix, vector<Vec3f> *input){
	for (int i = 0; i < input->size(); i++){
		input->at(i) = (colorspace_conversion(conversion_matrix, input->at(i)));
	}
	return 1;
}

Mat_<float> YUV_Colorspace(){
	Mat_<float> output(3,3,CV_32FC1);
	output.at<float>(0, 0) = 0.299f;    output.at<float>(0, 1) = 0.587f;	output.at<float>(0, 2) = 0.114f;
	output.at<float>(1, 0) = -0.14713f;	output.at<float>(1, 1) = -0.28886f;	output.at<float>(1, 2) = 0.436f;
	output.at<float>(2, 0) = 0.615f;	output.at<float>(2, 1) = -0.51499f;	output.at<float>(2, 2) = -0.10001f;
	return output;
}

Mat_<float> RGB_Colorspace(){
	Mat_<float> output(3, 3, CV_32FC1);
	output.at<float>(0, 0) = 1; output.at<float>(0, 1) = 0;	output.at<float>(0, 2) = 0;
	output.at<float>(1, 0) = 0;	output.at<float>(1, 1) = 1;	output.at<float>(1, 2) = 0;
	output.at<float>(2, 0) = 0;	output.at<float>(2, 1) = 0;	output.at<float>(2, 2) = 1;
	return output;
}

Mat_<float> XYZ_Colorspace(){
	Mat_<float> output(3, 3, CV_32FC1);
	output.at<float>(0, 0) = 0.4124f; output.at<float>(0, 1) = 0.3576f; output.at<float>(0, 2) = 0.1805f;
	output.at<float>(1, 0) = 0.2126f; output.at<float>(1, 1) = 0.7152f; output.at<float>(1, 2) = 0.0722f;
	output.at<float>(2, 0) = 0.0193f; output.at<float>(2, 1) = 0.1192f; output.at<float>(2, 2) = 0.9505f;
	return output;
}