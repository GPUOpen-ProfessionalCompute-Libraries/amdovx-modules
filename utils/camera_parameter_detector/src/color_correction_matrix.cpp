#include "color_correction_matrix.hpp"

#include <iostream>

color_correction_matrix::color_correction_matrix(vector<Vec3f>real_colors, vector<Vec3f> image_colors, int mode){
	switch (mode){
	case 0:
	{
			  values.create(3, 4, CV_32FC1);
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
			  values.create(3, 4, CV_32FC1);
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
			  values.create(3, 4, CV_32FC1);
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
}

color_correction_matrix::color_correction_matrix(char* filename){
	FileStorage file(filename, FileStorage::READ);

	// Write to file!
	file["Color Correction Matrx"] >> values;
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

int color_correction_matrix::change_colorspace(Mat_<float> conversion_matrix_from_new){
	if (conversion_matrix_from_new.rows != 3 || conversion_matrix_from_new.cols != 3){
		printf("conversion matrix has wrong dimensions");
		return 0;
	}
	Mat_<float> conversion_matrix_to_new;
	invert(conversion_matrix_from_new, conversion_matrix_to_new);

	values.col(3) = conversion_matrix_to_new * values.col(3);
	values.colRange(0, 3) = conversion_matrix_to_new * values.colRange(0, 3) * conversion_matrix_from_new;

	return 0;
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