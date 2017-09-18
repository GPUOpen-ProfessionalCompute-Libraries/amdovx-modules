#include "calculate_color_correction.h"

#include <iostream>

int calculate_color_correction(params parameter){
	int width = parameter.width;
	int height = parameter.height;

	// Construct cv image
	Mat image;

	// Read input data
	if (width != 0 && height != 0){
		Mat input_image;
		input_image.create(height, width, CV_8UC3);


		FILE *fp = NULL;
		char *imagedata = NULL;
		int framesize = width*height * 3;
		fopen_s(&fp, parameter.input_image_filename, "rb");
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
		width = image.cols;
		height = image.rows;
	}
	printf("OK: \t Read input (%s)\n", parameter.input_image_filename);

	// Get gray image
	Mat image_gray;
	image_gray.create(height, width, CV_8UC1);
	cvtColor(image, image_gray, COLOR_RGB2GRAY);



	// Find Canny edges
	Mat image_edges;
	int lowThreshold = 80;
	int highThreshold = 100;
	int kernel_size = 3;
	Canny(image_gray, image_edges, lowThreshold, highThreshold, kernel_size);

	if (parameter.show_images)
		show_image("Corner image", &image_edges);
	if (parameter.save_images)
		imwrite("Corner_image.jpg", image_edges);
	printf("OK: \t Finished Canny edge detection\n");

	//Find rectangles in image_edges
	rectangular_list list;
	list.create_from_image(&image_edges);
	//	list.create_from_image_with_OpenCV(image_edges);

	if (parameter.show_images || parameter.save_images){
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				if (image_edges.at<uchar>(Point(x, y)) != 240 && image_edges.at<uchar>(Point(x, y)) != 0 && image_edges.at<uchar>(Point(x, y)) != 170)
				{
					image_edges.at<uchar>(Point(x, y)) = 50;
				}
			}
		}
		if (parameter.show_images)
			show_image("Rectangulars found", &image_edges);
		if (parameter.save_images)
			imwrite("Rects_found.jpg", image_edges);
		//list.drawSquares(image, "Squares found");
	}

	if (!list.valid)
		return -1;

	// Find rectangles with right vertex relation
	if (parameter.show_images || parameter.save_images){
		list.check_vertex(&image_edges);
		if (parameter.show_images)
			show_image("First sort", &image_edges);
		if (parameter.save_images)
			imwrite("First-sort.jpg", image_edges);
	}
	else{
		list.check_vertex(NULL);
	}

	if (!list.valid)
		return -1;

	// Check near objects
	if (parameter.show_images || parameter.save_images){
		list.check_near_objects(&image_edges);
		if (parameter.show_images)
			show_image("Second sort", &image_edges);
		if (parameter.save_images)
			imwrite("Second-sort.jpg", image_edges);
	}
	else{
		list.check_near_objects(NULL);
	}

	if (!list.valid)
		return -1;

	// Find Orientation, Count rects around
	list.find_orientation();
	if (!list.valid)
		return -1;

	// Create point lists
	list.create_obj_scene_list((int)50);
	printf("OK: \t Finished creating point lists\n");

	// Get warp parameter and warp
	if (parameter.show_images || parameter.save_images){
		Mat warped_image;
		Mat warped_rotated_image;
		if (list.create_transformation_parameters(image, &warped_image, &warped_rotated_image) == -1){
			return -1;
		}
		if (parameter.show_images){
			show_image("Non rotatet and non reflected warped image", &warped_image);
			show_image("Rotated and reflected warped image", &warped_rotated_image);
		}
		if (parameter.save_images)
		{
			imwrite("Warped_non_rotated_non_reflected.jpg", warped_image);
			imwrite("Warped_rotated_reflected.jpg", warped_image);
		}
	}
	else if (parameter.colorchart_image_filename){
		Mat warped_rotated_image;
		if (list.create_transformation_parameters(image, NULL, &warped_rotated_image) == -1){
			return -1;
		}
		imwrite(parameter.colorchart_image_filename, warped_rotated_image);
	}
	else{
		if (list.create_transformation_parameters(image, NULL, NULL) == -1){
			return -1;
		}
	}
	printf("OK: \t All warping sucessful\n");

	// Get color conversion matrix

	int color_correction_mode;
	Mat_<float> colorspace_conversion_matrix;
	switch (parameter.colorspace){
	case 0:
		colorspace_conversion_matrix = RGB_Colorspace();
		color_correction_mode = 0;
		printf("OK: \t Will use RGB\n");
		break;
	case 1:
		colorspace_conversion_matrix = YUV_Colorspace();
		color_correction_mode = 0;
		printf("OK: \t Will use YUV\n");
		break;
	case 2:
		colorspace_conversion_matrix = YUV_Colorspace();
		color_correction_mode = 1;
		printf("OK: \t Will use UV\n");
		break;
	case 3:
		colorspace_conversion_matrix = XYZ_Colorspace();
		color_correction_mode = 0;
		printf("OK: \t Will use XYZ\n");
		break;
	case 4:
		colorspace_conversion_matrix = XYZ_Colorspace();
		color_correction_mode = 2;
		printf("OK: \t Will use XZ\n");
		break;
	}
	cout << colorspace_conversion_matrix << endl;

	// Get color information
	vector<Vec3f> real_colors = create_real_colors();
	if (parameter.linear)
		real_colors = degamma(real_colors);
	colorspace_conversion(colorspace_conversion_matrix, &real_colors);

	Mat_<Vec3f> degamma_image;
	if (parameter.linear)
		degamma_image = degamma(image);
	else
		image.copyTo(degamma_image);
	//image.convertTo(degamma_image,CV_32FC3);
	vector<Vec3f> image_colors;
	image_colors = list.generate_color_vector(degamma_image);
	colorspace_conversion(colorspace_conversion_matrix, &image_colors);

	// Calculate color correction matrix
	color_correction_matrix color_correction(real_colors, image_colors, color_correction_mode);
	printf("Matrix in used colorspace:\n");
	color_correction.print();
	color_correction.change_colorspace(colorspace_conversion_matrix);
	printf("Matrix in RGB colorspace:\n");
	color_correction.print();

	if (parameter.correction_matrix_OpenCV_filename)
		color_correction.save_to_CV_file(parameter.correction_matrix_OpenCV_filename);
	if (parameter.correction_matrix_LOOM_filename)
		color_correction.save_to_LOOM_file(parameter.correction_matrix_LOOM_filename);

	if (parameter.output_image_filename || parameter.show_images){
		// Use color correction matrix
		color_correction.apply(&degamma_image);

		Mat_<Vec3b> gamma_image;
		if (parameter.linear)
			gamma_image = gamma(degamma_image);
		else
			degamma_image.copyTo(gamma_image);
		//degamma_image.convertTo(gamma_image,CV_8UC3);
		if (parameter.show_images){
			show_image("Color corrected image", &gamma_image);
		}

		if (parameter.output_image_filename)
			imwrite(parameter.output_image_filename, gamma_image);
	}
	printf("OK: \t Sucessful\n");




	return 0;
}