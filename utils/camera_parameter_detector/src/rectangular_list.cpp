#include "rectangular_list.hpp"

int rectangular_list::create_from_image(Mat *image){ // Using selfwritten detection function
	for (int y = 0; y < image->rows; y++)
	{
		for (int x = 0; x < image->cols; x++)
		{
			if (image->at<uchar>(Point(x, y)) > 250)
			{
				rectangular temporal(image, x, y);
				if (temporal.valid)
				{
					temporal.SetCorner(image, 240);
					rect_list.push_back(temporal);
				}
			}
		}
	}
	return check_resulting_list("Finished looking for rects");
}

int rectangular_list::create_from_image_with_OpenCV(Mat image){ // Testcode only > not in used due to bad results
	square_list.clear();
	vector<vector<Point> > contours;
	findContours(image, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);

	vector<Point> approx;

	// test each contour
	for (size_t i = 0; i < contours.size(); i++)
	{
		// approximate contour with accuracy proportional
		// to the contour perimeter
		approxPolyDP(Mat(contours[i]), approx, arcLength(Mat(contours[i]), true)*0.02, true);

		// square contours should have 4 vertices after approximation
		// relatively large area (to filter out noisy contours)
		// and be convex.
		// Note: absolute value of an area is used because
		// area may be positive or negative - in accordance with the
		// contour orientation
		if (approx.size() == 4 &&
			fabs(contourArea(Mat(approx))) > 25 &&
			isContourConvex(Mat(approx)))
		{
			double maxCosine = 0;

			for (int j = 2; j < 5; j++)
			{
				// find the maximum cosine of the angle between joint edges
				double cosine = fabs(angle(approx[j % 4], approx[j - 2], approx[j - 1]));
				maxCosine = MAX(maxCosine, cosine);
			}

			// if cosines of all angles are small
			// (all angles are ~90 degree) then write quandrange
			// vertices to resultant sequence
			if (maxCosine < 0.3)
				square_list.push_back(approx);
		}
	}


/*	Mat pyr, timg, gray0(image.size(), CV_8U), gray;

	// down-scale and upscale the image to filter out the noise
/*	pyrDown(image, pyr, Size(image.cols / 2, image.rows / 2));
	pyrUp(pyr, timg, image.size());*/
/*	timg = image;
	
	vector<vector<Point> > contours;

	//int thresh = 100, N = 11;
	int thresh = 100, N = 11;

	// find square_list in every color plane of the image
	for (int c = 0; c < 3; c++)
	{
		int ch[] = { c, 0 };
		mixChannels(&timg, 1, &gray0, 1, ch, 1);

		// try several threshold levels
		for (int l = 0; l < N; l++)
		{
			// hack: use Canny instead of zero threshold level.
			// Canny helps to catch square_list with gradient shading
			if (l == 0)
			{
				// apply Canny. Take the upper threshold from slider
				// and set the lower to 0 (which forces edges merging)
				Canny(gray0, gray, 80, thresh, 3);
				// dilate canny output to remove potential
				// holes between edge segments
				
				//dilate(gray, gray, Mat(), Point(-1, -1));
			}
			else
			{
				// apply threshold if l!=0:
				//     tgray(x,y) = gray(x,y) < (l+1)*255/N ? 255 : 0
				gray = gray0 >= (l + 1) * 255 / N;
			}

			// find contours and store them all as a list
			findContours(gray, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);

			vector<Point> approx;

			// test each contour
			for (size_t i = 0; i < contours.size(); i++)
			{
				// approximate contour with accuracy proportional
				// to the contour perimeter
				approxPolyDP(Mat(contours[i]), approx, arcLength(Mat(contours[i]), true)*0.02, true);

				// square contours should have 4 vertices after approximation
				// relatively large area (to filter out noisy contours)
				// and be convex.
				// Note: absolute value of an area is used because
				// area may be positive or negative - in accordance with the
				// contour orientation
				if (approx.size() == 4 &&
					fabs(contourArea(Mat(approx))) > 25 &&
					isContourConvex(Mat(approx)))
				{
					double maxCosine = 0;

					for (int j = 2; j < 5; j++)
					{
						// find the maximum cosine of the angle between joint edges
						double cosine = fabs(angle(approx[j % 4], approx[j - 2], approx[j - 1]));
						maxCosine = MAX(maxCosine, cosine);
					}

					// if cosines of all angles are small
					// (all angles are ~90 degree) then write quandrange
					// vertices to resultant sequence
					if (maxCosine < 0.3)
						square_list.push_back(approx);
				}
			}
		}
	}*/


	for (int i = 0; i < square_list.size(); i++){
		rectangular rect(square_list.at(i));
		rect_list.push_back(rect);
	}

	return check_resulting_list("Finished looking for rects");
}

int rectangular_list::drawSquares(Mat& image, char *wndname)
{
	if (square_list.size() != rect_list.size()){ // If rects are not in square list, add them
		for (int i = 0; i < rect_list.size(); i++)
		{
			square_list.push_back(rect_list.at(i).output_pointlist());
		}
	}
	for (size_t i = 0; i < square_list.size(); i++)
	{
		const Point* p = &square_list[i][0];
		int n = (int)square_list[i].size();
		polylines(image, &p, &n, 1, true, Scalar(0, 255, 0), 3, LINE_AA);
	}

	// Show image
	imshow(wndname, image); 
	waitKey(0);

	return 0;
}

int rectangular_list::check_vertex(Mat *image){
	int i = 0;
	while (i<rect_list.size()){
		if (rect_list.at(i).RelationLeftRightVertex() > 0.25 || rect_list.at(i).RelationTopBottomVertex() > 0.25 || rect_list.at(i).RelationSquare() > 0.25 || rect_list.at(i).size < 25)
		{
			if (image != NULL)
				rect_list.at(i).SetCorner(image, 70);
			rect_list.erase(rect_list.begin() + i);
		}
		else{
			if (image != NULL)
				rect_list.at(i).SetCorner(image, 255);
			i++;
		}
	}
	return check_resulting_list("Reduced vertexes");
}

int rectangular_list::check_near_objects(Mat *image){
	calc_maxNearObjects();
	int i = 0;
	while (i<rect_list.size()){
		if (rect_list.at(i).numberOfNearObjects > maxNearObjects*0.8)
		{
			if (image != NULL)
				rect_list.at(i).SetCorner(image, 255);
			i++;
		}
		else
		{
			if (image != NULL)
				rect_list.at(i).SetCorner(image, 100);
			rect_list.erase(rect_list.begin() + i);
		}
	}
	return check_resulting_list("Reduced for to less surrounding squares");
}

int rectangular_list::find_orientation(){
	for (int i = 0; i < rect_list.size(); i++)
	{
		orientation += rect_list.at(i).countRectsAround(rect_list);
		mean_size += rect_list.at(i).size;
		mean_vertex += rect_list.at(i).meanVertex;
	}
	mean_size = mean_size / rect_list.size();
	mean_vertex = mean_vertex / rect_list.size();

	if (orientation == 0){
		valid = false;
		printf("ERROR: \t Did not find right orientation, perhaps not enough or too many rects? - abort\n");
		return -1;
	}
	printf("OK: \t Found orientation\n");		
	return 0;
}

int rectangular_list::create_obj_scene_list(int size_of_rect_in){
	if (size_of_rect_in == 0){
		size_of_rect = (int)this->mean_vertex;
	}
	else{
		size_of_rect = size_of_rect_in;
	}
	size_between_rect = int((float)size_of_rect*0.125);

	for (int i = 0; i < (int)rect_list.size(); i++)
	{
		// Add Points
		this->rect_list.at(i).CreatePointlists(&obj_list, &scene_list, size_of_rect, size_between_rect, orientation);
	}
	return 0;
}

int rectangular_list::create_transformation_parameters(Mat input_image, Mat *output_image, Mat *output_image_rotated){	
	transformation_parameters = estimateRigidTransform(scene_list, obj_list, true);

	if (transformation_parameters.empty()){
		printf("ERROR: \t Affine transform failed - Abort\n");
		valid = false;
		return -1;
	}

	Mat warped_image;
	warpAffine(input_image, warped_image, transformation_parameters, Size(6 * size_of_rect + 5 * size_between_rect, 4 * size_of_rect + 3 * size_between_rect));
	if (output_image != NULL)
		warped_image.copyTo(*output_image);

	// Get intensity information
	Vec3b intensity = warped_image.at<Vec3b>(size_of_rect / 2, 5 * size_of_rect + 5 * size_between_rect + size_of_rect / 2);
	uchar redRightTopCorner = intensity.val[2];
	intensity = warped_image.at<Vec3b>(3 * size_of_rect + 3 * size_between_rect + size_of_rect / 2, size_of_rect / 2);
	uchar redLeftBottomCorner = intensity.val[2];

	if (redRightTopCorner > redLeftBottomCorner) // need to rotate
	{
		transformation_parameters.at<float>(0, 0) = -1 * transformation_parameters.at<float>(0, 0);
		transformation_parameters.at<float>(0, 1) = -1 * transformation_parameters.at<float>(0, 1);
		transformation_parameters.at<float>(1, 0) = -1 * transformation_parameters.at<float>(1, 0);
		transformation_parameters.at<float>(1, 1) = -1 * transformation_parameters.at<float>(1, 1);
		transformation_parameters.at<float>(0, 2) = -1 * transformation_parameters.at<float>(0, 2) + 6 * size_of_rect + 5 * size_between_rect;
		transformation_parameters.at<float>(1, 2) = -1 * transformation_parameters.at<float>(1, 2) + 4 * size_of_rect + 3 * size_between_rect;
		printf("OK: \t rotated\n");
	}
	
	if (output_image_rotated != NULL){
		warpAffine(input_image, *output_image_rotated, transformation_parameters, Size(6 * size_of_rect + 5 * size_between_rect, 4 * size_of_rect + 3 * size_between_rect));
	}

	return 0;
}

vector<Vec3f> rectangular_list::generate_color_vector(Mat_<float> image){
	vector <Vec3f> output;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 6; j++)
		{
			// Calculate center and calculate color from there
			Point center;
			center.x = (j*size_of_rect) + max(0, (j - 1)) * size_between_rect + size_of_rect / 2;
			center.y = (i*size_of_rect) + max(0, (i - 1)) * size_between_rect + size_of_rect / 2;
			Vec3f color = calculate_color(center, transformation_parameters, image, (int)mean_size);
			output.push_back(color);
		}
	}
	return output;
}

int rectangular_list::size(){
	return (int)rect_list.size();
}


int rectangular_list::calc_maxNearObjects(){
	for (int i = 0; i < rect_list.size(); i++){
		rect_list.at(i).CalcNumberOfNearObjects(rect_list);
		if (rect_list.at(i).numberOfNearObjects > maxNearObjects){
			maxNearObjects = rect_list.at(i).numberOfNearObjects;
		}
	}
	return 0;
}

int rectangular_list::check_resulting_list(char* text){
	if (rect_list.size() < 4){
		valid = false;
		printf("Error: \t %s - Not enough rects here (%d)!\n", text, rect_list.size());
		return -1;
	}
	printf("OK: \t %s: %d found\n", text, rect_list.size());
	return 0;
}

Vec3f rectangular_list::calculate_color(Point center, Mat Transformation, Mat image, int predicted_size){
	// get real center
	Point warped_center;
	float divisor = Transformation.at<float>(0, 0) * Transformation.at<float>(1, 1) - Transformation.at<float>(0, 1) * Transformation.at<float>(1, 0);
	warped_center.x = (int)round((Transformation.at<float>(1, 1)*(center.x - Transformation.at<float>(0, 2)) - Transformation.at<float>(0, 1)*(center.y - Transformation.at<float>(1, 2))) / divisor);
	warped_center.y = (int)round((-1 * Transformation.at<float>(1, 0)*(center.x - Transformation.at<float>(0, 2)) + Transformation.at<float>(0, 0)*(center.y - Transformation.at<float>(1, 2))) / divisor);

	// get color information from center
	Vec3f intensity_float = image.at<Vec3f>(Point(warped_center.x, warped_center.y));

	// add color information to mean & variance
	Vec3f mean = intensity_float;
	Vec3f difference;
	Vec3f variance = 0;

	// set variables for loop
	int counter = 1;
	Point relevant = warped_center;
	Point add;
	add.x = 0;
	add.y = 0;
	int maxX = 0;
	int maxY = 0;
	int direction = 4;

	bool small_variance = true;
	while (small_variance)
	{
		// Increase counter
		counter++;

		// define new direction
		if (add.x == maxX && add.y == maxY)
		{
			if (maxX == maxY)
			{
				maxX++;
				direction = 4;
			}
			else if (maxX == maxY + 1)
			{
				maxY++;
				direction = 1;
			}
		}
		else if (add.x == maxX && add.y == -maxY)
			direction = 2;
		else if (add.x == -maxX && add.y == -maxY)
			direction = 3;
		else if (add.x == -maxX && add.y == maxY)
			direction = 4;

		// calculate next pixel 
		switch (direction)
		{
		case(1) :
			relevant.y--;
			add.y--;
			break;
		case(2) :
			relevant.x--;
			add.x--;
			break;
		case(3) :
			relevant.y++;
			add.y++;
			break;
		case(4) :
			relevant.x++;
			add.x++;
			break;
		}

		// get intensity information
		relevant.x = min(max(0, relevant.x), image.cols - 1);
		relevant.y = min(max(0, relevant.y), image.rows - 1);
		intensity_float = image.at<Vec3f>(Point(relevant.x, relevant.y));

		// check intensity and add it to mean & variance
		difference = (intensity_float - mean).mul((intensity_float - mean));
		float confidence = 3;
		if ((difference[0] > confidence * variance[0] && difference[1] > confidence * variance[1] && difference[2] > confidence * variance[2]) && counter > (predicted_size / 10))
		{
			small_variance = false;
		}
		else{
			mean = ((float)counter - 1) / (float)counter * mean + 1 / (float)counter * intensity_float;
			variance = ((float)counter - 1) / (float)counter * variance + 1 / (float)counter * (intensity_float - mean).mul((intensity_float - mean));
		}

		// Check if maximal size is reached
		if (counter == predicted_size)
		{
			small_variance = false;
		}
	}
	return mean;
}

static double angle(Point pt1, Point pt2, Point pt0)
{
	double dx1 = pt1.x - pt0.x;
	double dy1 = pt1.y - pt0.y;
	double dx2 = pt2.x - pt0.x;
	double dy2 = pt2.y - pt0.y;
	return (dx1*dx2 + dy1*dy2) / sqrt((dx1*dx1 + dy1*dy1)*(dx2*dx2 + dy2*dy2) + 1e-10);
}