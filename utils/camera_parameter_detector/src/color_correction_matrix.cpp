#include "color_correction_matrix.hpp"

//#include <iostream>

color_correction_matrix::color_correction_matrix(){
	values.create(3, 4, CV_32FC1);
}
color_correction_matrix::color_correction_matrix(char* filename){
	FileStorage file(filename, FileStorage::READ);

	// Write to file!
	file["Color Correction Matrx"] >> values;
}

int color_correction_matrix::set_colorspace(int value){
	switch (value){
	case 0:
		colorspace_conversion_matrix = RGB_Colorspace();
		calculation_mode = 0;
		printf("OK: \t Will use RGB\n");
		break;
	case 1:
		colorspace_conversion_matrix = YUV_Colorspace();
		calculation_mode = 0;
		printf("OK: \t Will use YUV\n");
		break;
	case 2:
		colorspace_conversion_matrix = YUV_Colorspace();
		calculation_mode = 1;
		printf("OK: \t Will use UV\n");
		break;
	case 3:
		colorspace_conversion_matrix = XYZ_Colorspace();
		calculation_mode = 0;
		printf("OK: \t Will use XYZ\n");
		break;
	case 4:
		colorspace_conversion_matrix = XYZ_Colorspace();
		calculation_mode = 2;
		printf("OK: \t Will use XZ\n");
		break;
	}
	return 0;
}
int color_correction_matrix::calculate(vector<Vec3f>real_colors, vector<Vec3f> image_colors){
	colorspace_conversion(&real_colors);
	colorspace_conversion(&image_colors);
	switch (calculation_mode){
	case 0:
	{
			  Mat real_colors_mat;
			  real_colors_mat = construct3xk_from_vectorlist(real_colors);
			  //	cout << real_colors_mat << endl;
			  Mat image_colors_mat;
			  image_colors_mat = construct4xk_from_vectorlist(image_colors);
			  //	cout << image_colors_mat << endl;

			  Mat image_colors_mat_transposed;
			  transpose(image_colors_mat, image_colors_mat_transposed);
			  //	cout << image_colors_mat_transposed << endl;

			  Mat image_colors_mat_multransposed;
			  mulTransposed(image_colors_mat, image_colors_mat_multransposed, false);

			  Mat image_colors_mat_multransposed_inverted;
			  invert(image_colors_mat_multransposed, image_colors_mat_multransposed_inverted);
			  //	cout << image_colors_mat_multransposed_inverted << endl;

			  values = real_colors_mat * image_colors_mat_transposed * image_colors_mat_multransposed_inverted;
			  //	cout << values << endl;
	}
		break;
	case 1:
	{
			  values.setTo(0);
			  Mat real_colors_mat;
			  real_colors_mat = construct2xk_from_vectorlist(real_colors,1,2);
			  //	cout << real_colors_mat << endl;
			  Mat image_colors_mat;
			  image_colors_mat = construct3xk_from_vectorlist(image_colors,1,2);
			  //	cout << image_colors_mat << endl;

			  Mat image_colors_mat_transposed;
			  transpose(image_colors_mat, image_colors_mat_transposed);
			  //	cout << image_colors_mat_transposed << endl;

			  Mat image_colors_mat_multransposed;
			  mulTransposed(image_colors_mat, image_colors_mat_multransposed, false);

			  Mat image_colors_mat_multransposed_inverted;
			  invert(image_colors_mat_multransposed, image_colors_mat_multransposed_inverted);
			  //	cout << image_colors_mat_multransposed_inverted << endl;

			  //Mat_<float> result_mat(2,3, CV_32FC1);
			  values(cv::Rect_<int>(1, 1, 3, 2)) = real_colors_mat * image_colors_mat_transposed * image_colors_mat_multransposed_inverted;
			  //result_mat.copyTo(values(cv::Rect_<int>(1, 1, 2, 3)));
			  values.at<float>(0, 0) = 1;
			  //	cout << values << endl;
	}
		break;
	case 2:
	{
			  values.setTo(0);
			  Mat real_colors_mat;
			  real_colors_mat = construct2xk_from_vectorlist(real_colors, 0, 2);
			  //	cout << real_colors_mat << endl;
			  Mat image_colors_mat;
			  image_colors_mat = construct3xk_from_vectorlist(image_colors, 0, 2);
			  //	cout << image_colors_mat << endl;

			  Mat image_colors_mat_transposed;
			  transpose(image_colors_mat, image_colors_mat_transposed);
			  //	cout << image_colors_mat_transposed << endl;

			  Mat image_colors_mat_multransposed;
			  mulTransposed(image_colors_mat, image_colors_mat_multransposed, false);

			  Mat image_colors_mat_multransposed_inverted;
			  invert(image_colors_mat_multransposed, image_colors_mat_multransposed_inverted);
			  //	cout << image_colors_mat_multransposed_inverted << endl;

			  Mat_<float> result_mat(2,3, CV_32FC1);
			  result_mat = real_colors_mat * image_colors_mat_transposed * image_colors_mat_multransposed_inverted;
			  values.at<float>(0, 0) = result_mat.at<float>(0, 0);
			  values.at<float>(0, 2) = result_mat.at<float>(0, 1);
			  values.at<float>(0, 3) = result_mat.at<float>(0, 2);
			  values.at<float>(2, 0) = result_mat.at<float>(1, 0);
			  values.at<float>(2, 2) = result_mat.at<float>(1, 1);
			  values.at<float>(2, 3) = result_mat.at<float>(1, 2);
		
			  values.at<float>(1, 1) = 1;
			  //	cout << values << endl;
	}
		break;
	}
	change_colorspace();
	return 0;
}

int color_correction_matrix::apply(Mat_<Vec3f> *image){
	for (int i = 0; i < image->cols; i++){
		for (int j = 0; j < image->rows; j++){
			Vec3f intensity_in = image->at<Vec3f>(j, i);
			Vec3f intensity_out;
			for (int w = 0; w < 3; w++){
				intensity_out[w] = values.at<float>(w, 0)*intensity_in[0] + values.at<float>(w, 1)*intensity_in[1] + values.at<float>(w, 2)*intensity_in[2] + values.at<float>(w, 3)*255;
			}
			image->at<Vec3f>(j, i) = intensity_out;
		}
	}
	return 0;
}

int color_correction_matrix::print(){
	for (int i = 0; i < 3; i++){
		for (int j = 0; j < 4; j++){
			printf("%f\t",values.at<float>(i, j));
		}
		printf("\n");
	}
	return 0;
}

int color_correction_matrix::save_to_CV_file(char* filename){
	FileStorage file(filename, FileStorage::WRITE);
	
	// Write to file!
	file << "Color Correction Matrx" << values;
	return 0;
}

int color_correction_matrix::save_to_LOOM_file(char* filename){
	FILE *filepointer;
	fopen_s(&filepointer, filename, "w");
	fprintf(filepointer, "%f %f %f %f ", values.at<float>(2, 2), values.at<float>(2, 1), values.at<float>(2, 0), values.at<float>(2, 3));
	fprintf(filepointer, "%f %f %f %f ", values.at<float>(1, 2), values.at<float>(1, 1), values.at<float>(1, 0), values.at<float>(1, 3));
	fprintf(filepointer, "%f %f %f %f ", values.at<float>(0, 2), values.at<float>(0, 1), values.at<float>(0, 0), values.at<float>(0, 3));

	fclose(filepointer);
	filepointer = NULL;
	return 0;
}

int color_correction_matrix::change_colorspace(){
	if (colorspace_conversion_matrix.rows != 3 || colorspace_conversion_matrix.cols != 3){
		printf("conversion matrix has wrong dimensions");
		return 0;
	}
	Mat_<float> conversion_matrix_to_new;
	invert(colorspace_conversion_matrix, conversion_matrix_to_new);

	values.col(3) = conversion_matrix_to_new * values.col(3);
	values.colRange(0, 3) = conversion_matrix_to_new * values.colRange(0, 3) * colorspace_conversion_matrix;

	return 0;
}

Vec3f color_correction_matrix::colorspace_conversion(Vec3f input){
	if (colorspace_conversion_matrix.rows != 3 || colorspace_conversion_matrix.cols != 3){
		printf("conversion matrix has wrong dimensions");
		return 0;
	}
	Vec3f output;
	output[0] = colorspace_conversion_matrix.at<float>(0, 0) * input[0] + colorspace_conversion_matrix.at<float>(0, 1) * input[1] + colorspace_conversion_matrix.at<float>(0, 2) * input[2];
	output[1] = colorspace_conversion_matrix.at<float>(1, 0) * input[0] + colorspace_conversion_matrix.at<float>(1, 1) * input[1] + colorspace_conversion_matrix.at<float>(1, 2) * input[2];
	output[2] = colorspace_conversion_matrix.at<float>(2, 0) * input[0] + colorspace_conversion_matrix.at<float>(2, 1) * input[1] + colorspace_conversion_matrix.at<float>(2, 2) * input[2];
	return output;
}

int color_correction_matrix::colorspace_conversion(vector<Vec3f> *input){
	for (size_t i = 0; i < input->size(); i++){
		input->at(i) = (colorspace_conversion(input->at(i)));
	}
	return 1;
}

Mat construct4xk_from_vectorlist(vector<Vec3f> input){
	Mat output;
	output.create(4, (int)input.size(), CV_32FC1);
	for (int i = 0; i < input.size(); i++){
		output.at<float>(0, i) = input.at(i)[0];
		output.at<float>(1, i) = input.at(i)[1];
		output.at<float>(2, i) = input.at(i)[2];
		output.at<float>(3, i) = 255;
	}
	return output;
}

Mat construct3xk_from_vectorlist(vector<Vec3f> input){
	Mat output;
	output.create(3, (int)input.size(), CV_32FC1);
	for (int i = 0; i < input.size(); i++){
		output.at<float>(0, i) = input.at(i)[0];
		output.at<float>(1, i) = input.at(i)[1];
		output.at<float>(2, i) = input.at(i)[2];
	}
	return output;
}

Mat construct3xk_from_vectorlist(vector<Vec3f> input, int col1, int col2){
	Mat output;
	output.create(3, (int)input.size(), CV_32FC1);
	for (int i = 0; i < input.size(); i++){
		output.at<float>(0, i) = input.at(i)[col1];
		output.at<float>(1, i) = input.at(i)[col2];
		output.at<float>(2, i) = 255;
	}
	return output;
}

Mat construct2xk_from_vectorlist(vector<Vec3f> input, int col1, int col2){
	Mat output;
	output.create(2, (int)input.size(), CV_32FC1);
	for (int i = 0; i < input.size(); i++){
		output.at<float>(0, i) = input.at(i)[col1];
		output.at<float>(1, i) = input.at(i)[col2];
	}
	return output;
}

Mat_<float> YUV_Colorspace(){
	Mat_<float> output(3, 3, CV_32FC1);
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