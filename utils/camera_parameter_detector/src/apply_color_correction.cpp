#include "apply_color_correction.h"

int apply_color_correction(params parameter){
	// Construct cv image
	Mat image;
	int width = parameter.width;
	int height = parameter.height;

	// Read input data
	if (width != 0 && height != 0){
		Mat input_image;
		input_image.create(height, width, CV_8UC3);

		FILE *fp = NULL;
		char *imagedata = NULL;
		int framesize = width*height * 3;
		fp = fopen(parameter.input_image_filename, "rb");
		if (fp == NULL)
		{
			printf("ERROR: \t Input filename (%s) is invalid\n", parameter.input_image_filename);
			return -1;
		}
		imagedata = (char*)malloc(sizeof(char)* framesize);
		fread(imagedata, sizeof(char), framesize, fp);
		memcpy(input_image.data, imagedata, framesize);
		free(imagedata);
		fclose(fp);

		image.create(height, width, CV_8UC3);
		cvtColor(input_image, image, COLOR_RGB2BGR);
	}
	else{
		image = imread(parameter.input_image_filename);
	}

	printf("OK: \t Read input\n");

	//Construct correction matrix
	color_correction_matrix correction(parameter.correction_matrix_OpenCV_filename);

	Mat_<Vec3f> degamma_image;
	if (parameter.linear)
		degamma_image = degamma(image);
	else
		image.copyTo(degamma_image);
	correction.apply(&degamma_image);

	Mat gamma_image;
	if (parameter.linear)
		gamma_image = gamma(degamma_image);
	else
		degamma_image.copyTo(gamma_image);

	imwrite(parameter.output_image_filename, gamma_image);

	return 0;
}