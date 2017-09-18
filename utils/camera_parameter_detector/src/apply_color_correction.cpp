#include "apply_color_correction.h"

int apply_color_correction(params parameter){
	// Read input image
	Mat *image = read_input(parameter);
	if (image == NULL)
		return -1;

	// Construct correction matrix
	color_correction_matrix correction(parameter.correction_matrix_OpenCV_filename);

	// Apply correction
	Mat_<Vec3f> degamma_image;
	if (parameter.linear)
		degamma_image = degamma(*image);
	else
		image->copyTo(degamma_image);
	correction.apply(&degamma_image);

	Mat gamma_image;
	if (parameter.linear)
		gamma_image = gamma(degamma_image);
	else
		degamma_image.copyTo(gamma_image);

	// Write output
	imwrite(parameter.output_image_filename, gamma_image);
	return 0;
}