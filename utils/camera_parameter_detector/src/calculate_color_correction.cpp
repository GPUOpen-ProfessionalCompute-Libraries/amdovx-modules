#include "calculate_color_correction.h"

#include <iostream>

int calculate_color_correction(params parameter){
	// Read input image
	Mat *image = read_input(parameter);
	if (image == NULL)
		return -1;

	// Get gray image
	Mat image_gray;
	image_gray.create(image->rows, image->cols, CV_8UC1);
	cvtColor(*image, image_gray, COLOR_RGB2GRAY);

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

	// Find rectangles in image_edges
	rectangular_list list;
	list.create_from_image(&image_edges);
	//	list.create_from_image_with_OpenCV(image_edges);

	if (parameter.show_images || parameter.save_images){
		for (int y = 0; y < image_edges.rows; y++)
		{
			for (int x = 0; x < image_edges.cols; x++)
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
	if(!list.find_orientation())
		return -1;

	// Create point lists
	list.create_obj_scene_list((int)50);
	printf("OK: \t Finished creating point lists\n");

	// Get warp parameter and warp
	if (parameter.show_images || parameter.save_images){
		Mat warped_image;
		Mat warped_rotated_image;
		if (list.create_transformation_parameters(*image, &warped_image, &warped_rotated_image) == -1){
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
		if (list.create_transformation_parameters(*image, NULL, &warped_rotated_image) == -1){
			return -1;
		}
		imwrite(parameter.colorchart_image_filename, warped_rotated_image);
	}
	else{
		if (list.create_transformation_parameters(*image, NULL, NULL) == -1){
			return -1;
		}
	}
	printf("OK: \t All warping sucessful\n");

	// Get color values from the input image
	Mat_<Vec3f> degamma_image;
	if (parameter.linear)
		degamma_image = degamma(*image);
	else
		image->copyTo(degamma_image);
	vector<Vec3f> image_colors;
	image_colors = list.generate_color_vector(degamma_image);

	// Generate color correction matrix and calculate it
	color_correction_matrix color_correction;
	color_correction.set_colorspace(parameter.colorspace);
	if (parameter.linear)
		color_correction.calculate(degamma(create_real_colors()), image_colors);
	else
		color_correction.calculate(create_real_colors(), image_colors);

	// Print color correction matrix
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
		if (parameter.show_images){
			show_image("Color corrected image", &gamma_image);
		}
		if (parameter.output_image_filename)
			imwrite(parameter.output_image_filename, gamma_image);
	}
	printf("OK: \t Sucessful\n");
	return 0;
}