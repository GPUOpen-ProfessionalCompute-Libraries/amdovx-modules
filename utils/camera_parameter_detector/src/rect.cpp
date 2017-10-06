#include "rect.hpp"

// Public ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rectangular::rectangular(Mat *image, int x, int y){
	// Set parameters for search
	int max_skip_pixel = 2;
	int width = image->cols;
	int height = image->rows;

	// Init values for search
	Point lastPoint;
	lastPoint.x = x;
	lastPoint.y = y;

	this->leftTopCorner = lastPoint;
	this->cornerPoints.push_back(lastPoint);

	Point startPoint = lastPoint;
	this->valid = true;

	int skipped_pixels = 0;
	int search_status = 10;

	// Start looping
	while (search_status != DIRECTIONS_SEARCH_DONE){
		while (search_status == DIRECTIONS_SEARCH_RIGHT || search_status == DIRECTIONS_SKIPPING_RIGHT || search_status == DIRECTIONS_SEARCH_RIGHT_2){ // Search Right
			if (search_status == DIRECTIONS_SEARCH_RIGHT_2 && (!(lastPoint.x < (startPoint.x - 1) || lastPoint.x >(startPoint.x + 1) || lastPoint.y < (startPoint.y - 1) || lastPoint.y >(startPoint.y + 1))))
			{ // Check if end is reached
				this->valid = true;
				search_status = DIRECTIONS_SEARCH_DONE;
				break;
			}
			if (lastPoint.x == width - 1 || (lastPoint.y < height - 3 && image->at<uchar>(Point(lastPoint.x, lastPoint.y + 1)) > 250 && image->at<uchar>(Point(lastPoint.x, lastPoint.y + 2)) > 250 && lastPoint.x > (this->leftTopCorner.x + 5)))
			{ // Check if turn is necessary (edge reached, or turn probable
				if (search_status == DIRECTIONS_SEARCH_RIGHT_2){
					this->valid = false;
					search_status = DIRECTIONS_SEARCH_DONE;
					break;
				}
				else{
					search_status = DIRECTIONS_CHECK_RIGHT_DOWN;
					break;
				}
			}
			else if (image->at<uchar>(Point(lastPoint.x + 1, lastPoint.y)) > 250)
			{ // Check for next point same height
				lastPoint.x++;
				skipped_pixels = 0;
				search_status = (search_status == DIRECTIONS_SKIPPING_RIGHT) ? DIRECTIONS_SEARCH_RIGHT : search_status;
			}
			else if (image->at<uchar>(Point(lastPoint.x + 1, max(0, lastPoint.y - 1))) > 250)
			{ // Check for next point smaller height
				lastPoint.x++;
				lastPoint.y = max(0, lastPoint.y - 1);
				skipped_pixels = 0;
				search_status = (search_status == DIRECTIONS_SKIPPING_RIGHT) ? DIRECTIONS_SEARCH_RIGHT : search_status;
			}
			else if (image->at<uchar>(Point(lastPoint.x + 1, min(height - 1, lastPoint.y + 1))) > 250)
			{ // Check for next point bigger height
				lastPoint.x++;
				lastPoint.y = min(height - 1, lastPoint.y + 1);
				skipped_pixels = 0;
				search_status = (search_status == DIRECTIONS_SKIPPING_RIGHT) ? DIRECTIONS_SEARCH_RIGHT : search_status;
			}
			// If no next point found
			else if (search_status == DIRECTIONS_SEARCH_RIGHT || search_status == DIRECTIONS_SKIPPING_RIGHT){
				search_status = DIRECTIONS_CHECK_RIGHT_DOWN;
				break;
			}
			else if (search_status == DIRECTIONS_SEARCH_RIGHT_2){
				search_status = DIRECTIONS_CHECK_RIGHT_2;
				break;
			}
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == DIRECTIONS_CHECK_RIGHT_DOWN)
		{
			if (this->leftTopCorner.x + 3 > lastPoint.x)
			{ // If to close to last turn point > not valid
				search_status = DIRECTIONS_SEARCH_DONE;
				this->valid = false;
			}
			else{
				this->rightTopCorner = lastPoint;
				search_status = DIRECTIONS_SEARCH_DOWN;
			}
		}

		if (search_status == DIRECTIONS_CHECK_RIGHT_2)
		{
			if (this->leftTopCorner.x == lastPoint.x)
			{
				if (lastPoint.y == 0 || skipped_pixels >= max_skip_pixel){
					search_status = DIRECTIONS_SEARCH_DONE;
					this->valid = false;
				}
				else{ // Try to skipp pixels when going up
					lastPoint.y--;
					skipped_pixels = 1;
					search_status = DIRECTIONS_SKIPPING_UP;
				}
			}
			else{
				search_status = DIRECTIONS_SEARCH_DONE;
				this->valid = false;
			}
		}

		while (search_status == DIRECTIONS_SEARCH_DOWN || search_status == DIRECTIONS_SKIPPING_DOWN){ // Search Down
			if (lastPoint.y == height - 1 || (lastPoint.x > 2 && image->at<uchar>(Point(lastPoint.x - 1, lastPoint.y)) > 250 && image->at<uchar>(Point(lastPoint.x - 2, lastPoint.y)) > 250 && lastPoint.y > (this->rightTopCorner.y + 5)))
			{ // Check if turn is necessary either due to edge or probable turn
				search_status = DIRECTIONS_CHECK_DOWN_LEFT;
				break;
			}
			if (image->at<uchar>(Point(lastPoint.x, lastPoint.y + 1)) > 250){
				lastPoint.y++;
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_DOWN;
			}
			else if (image->at<uchar>(Point(min(lastPoint.x + 1, width - 1), lastPoint.y + 1)) > 250){
				lastPoint.x = min(lastPoint.x + 1, width - 1);
				lastPoint.y++;
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_DOWN;
			}
			else if (image->at<uchar>(Point(max(0, lastPoint.x - 1), lastPoint.y + 1)) > 250){
				lastPoint.x = max(0, lastPoint.x - 1);
				lastPoint.y++;
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_DOWN;
			}
			else
			{ // if nothing is found, leave loop
				search_status = DIRECTIONS_CHECK_DOWN_LEFT;
				break;
			}
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == DIRECTIONS_CHECK_DOWN_LEFT)
		{
			if (this->rightTopCorner.y == lastPoint.y)
			{ // if distance is to last turn point is too small
				if (lastPoint.x == width - 1 || skipped_pixels >= max_skip_pixel){
					search_status = DIRECTIONS_SEARCH_DONE;
					this->valid = false;
				}
				else{ // Init skipping
					lastPoint.x++;
					skipped_pixels++;
					search_status = DIRECTIONS_SKIPPING_RIGHT;
				}
			}
			else{
				this->rightBottomCorner = lastPoint;
				search_status = DIRECTIONS_SEARCH_LEFT;
			}
		}

		while (search_status == DIRECTIONS_SEARCH_LEFT || search_status == DIRECTIONS_SKIPPING_LEFT)
		{
			if (lastPoint.x == 0 || (lastPoint.y > 2 && image->at<uchar>(Point(lastPoint.x, lastPoint.y - 1)) && image->at<uchar>(Point(lastPoint.x, lastPoint.y - 2)) > 250 && lastPoint.x < (this->rightBottomCorner.x - 5)))
			{
				search_status = DIRECTIONS_CHECK_LEFT_UP;
				break;
			}
			if (image->at<uchar>(Point(lastPoint.x - 1, lastPoint.y)) > 250){
				lastPoint.x--;
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_LEFT;
			}
			else if (image->at<uchar>(Point(lastPoint.x - 1, min(height - 1, lastPoint.y + 1))) > 250){
				lastPoint.x--;
				lastPoint.y = min(height - 1, lastPoint.y + 1);
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_LEFT;
			}
			else if (image->at<uchar>(Point(lastPoint.x - 1, max(0, lastPoint.y - 1))) > 250){
				lastPoint.x--;
				lastPoint.y = max(0, lastPoint.y - 1);
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_LEFT;
			}
			else{
				search_status = DIRECTIONS_CHECK_LEFT_UP;
				break;
			}
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == DIRECTIONS_CHECK_LEFT_UP)
		{
			if (this->rightBottomCorner.x == lastPoint.x)
			{
				if (lastPoint.y == height - 1 || skipped_pixels >= max_skip_pixel){
					search_status = DIRECTIONS_SEARCH_DONE;
					this->valid = false;
				}
				else{
					lastPoint.y++;
					skipped_pixels++;
					search_status = DIRECTIONS_SKIPPING_DOWN;
				}
			}
			else{
				this->leftBottomCorner = lastPoint;
				search_status = 40;
			}
		}

		while (search_status == DIRECTIONS_SEARCH_UP || search_status == DIRECTIONS_SKIPPING_UP)
		{
			if (search_status == DIRECTIONS_SEARCH_UP && (!(lastPoint.x < (startPoint.x - 1) || lastPoint.x >(startPoint.x + 1) || lastPoint.y < (startPoint.y - 1) || lastPoint.y >(startPoint.y + 1))))
			{ // Check if starting point is reached
				this->valid = true;
				search_status = DIRECTIONS_SEARCH_DONE;
				break;
			}
			if (lastPoint.y == 0 || (lastPoint.x < width - 3 && image->at<uchar>(Point(lastPoint.x + 1, lastPoint.y)) > 250 && image->at<uchar>(Point(lastPoint.x + 2, lastPoint.y)) > 250 && lastPoint.y < (this->leftBottomCorner.y - 5)))
			{
				search_status = DIRECTIONS_CHECK_UP_RIGHT;
				break;
			}
			else if (image->at<uchar>(Point(lastPoint.x, lastPoint.y - 1)) > 250)
			{
				lastPoint.y--;
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_UP;
			}
			else if (image->at<uchar>(Point(max(0, lastPoint.x - 1), lastPoint.y - 1)) > 250)
			{
				lastPoint.x = max(0, lastPoint.x - 1);
				lastPoint.y--;
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_UP;
			}

			else if (image->at<uchar>(Point(min(width - 1, lastPoint.x + 1), lastPoint.y - 1)) > 250)
			{
				lastPoint.x = min(width - 1, lastPoint.x + 1);
				lastPoint.y--;
				skipped_pixels = 0;
				search_status = DIRECTIONS_SEARCH_UP;
			}
			else
			{
				search_status = DIRECTIONS_CHECK_UP_RIGHT;
				break;
			}
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == DIRECTIONS_CHECK_UP_RIGHT){
			if (this->leftBottomCorner.y == lastPoint.y)
			{
				if (lastPoint.x == 0 || skipped_pixels >= max_skip_pixel){
					search_status = DIRECTIONS_SEARCH_DONE;
					this->valid = false;
				}
				else{
					lastPoint.x--;
					skipped_pixels++;
					search_status = DIRECTIONS_SKIPPING_LEFT;
				}
			}
			else{
				this->leftTopCorner = lastPoint;
				search_status = DIRECTIONS_SEARCH_RIGHT_2;
			}
		}
	}

	// Construct all values
	if (this->valid){
		this->center.x = (float)(leftTopCorner.x + rightTopCorner.x + rightBottomCorner.x + leftBottomCorner.x) / 4;
		this->center.y = (float)(leftTopCorner.y + rightTopCorner.y + rightBottomCorner.y + leftBottomCorner.y) / 4;
		this->numberOfNearObjects = 0;

		this->lengthBottomVertex = distance_between_points(rightBottomCorner, leftBottomCorner);
		this->lengthTopVertex = distance_between_points(rightTopCorner, leftTopCorner);
		this->lengthLeftVertex = distance_between_points(leftBottomCorner, leftTopCorner);
		this->lengthRightVertex = distance_between_points(rightBottomCorner, rightTopCorner);
		this->meanVertex = (lengthLeftVertex + lengthRightVertex + lengthTopVertex + lengthBottomVertex) / 4;

		float lengthLeftRight = (lengthLeftVertex + lengthRightVertex) / 2;
		float lengthTopBottom = (lengthTopVertex + lengthBottomVertex) / 2;
		this->size = lengthLeftRight * lengthTopBottom;
	}
}

rectangular::rectangular(vector<Point> list){
	// Extract corner points from list
	int biggerX;
	int biggerY;
	for (int i = 0; i < 4; i++){
		biggerX = 0;
		biggerY = 0;
		for (int j = 0; j < 4; j++){
			if (list.at(i).x < list.at(j).x)
				biggerX++;
			if (list.at(i).y < list.at(j).y)
				biggerY++;
		}
		if (biggerX>1 && biggerY>1)
			leftTopCorner = list.at(i);
		if (biggerX<2 && biggerY>1)
			rightTopCorner = list.at(i);
		if (biggerX>1 && biggerY<2)
			leftBottomCorner = list.at(i);
		if (biggerX<2 && biggerY<2)
			rightBottomCorner = list.at(i);
	}

	// Construct other values
	this->center.x = (float)(leftTopCorner.x + rightTopCorner.x + rightBottomCorner.x + leftBottomCorner.x) / 4;
	this->center.y = (float)(leftTopCorner.y + rightTopCorner.y + rightBottomCorner.y + leftBottomCorner.y) / 4;
	this->numberOfNearObjects = 0;

	this->lengthBottomVertex = distance_between_points(rightBottomCorner, leftBottomCorner);
	this->lengthTopVertex = distance_between_points(rightTopCorner, leftTopCorner);
	this->lengthLeftVertex = distance_between_points(leftBottomCorner, leftTopCorner);
	this->lengthRightVertex = distance_between_points(rightBottomCorner, rightTopCorner);
	this->meanVertex = (lengthLeftVertex + lengthRightVertex + lengthTopVertex + lengthBottomVertex) / 4;

	float lengthLeftRight = (lengthLeftVertex + lengthRightVertex) / 2;
	float lengthTopBottom = (lengthTopVertex + lengthBottomVertex) / 2;
	this->size = lengthLeftRight * lengthTopBottom;
}

vector<Point> rectangular::output_pointlist(){
	vector<Point> output;
	output.push_back(leftTopCorner);
	output.push_back(rightTopCorner);
	output.push_back(rightBottomCorner);
	output.push_back(leftBottomCorner);
	return output;
}

int rectangular::SetCorner(cv::Mat *input, int value){
	if (value > 255 || value < 0)
	{
		printf("Value for set corner is wrong(%d)\n", value);
		return -1;
	}
	for (int i = 0; i < (int)cornerPoints.size(); i++)
	{
		input->at<uchar>(Point(cornerPoints.at(i).x, cornerPoints.at(i).y)) = value;
	}
	input->at<uchar>(rightTopCorner) = (value-70);
	input->at<uchar>(leftTopCorner) = (value - 70);
	input->at<uchar>(rightBottomCorner) = (value - 70);
	input->at<uchar>(leftBottomCorner) = (value - 70);
	return 0;
}

float rectangular::RelationLeftRightVertex(){
	return abs(lengthLeftVertex / lengthRightVertex - 1);
}

float rectangular::RelationTopBottomVertex(){
	return abs(lengthTopVertex / lengthBottomVertex - 1);
}

float rectangular::RelationSquare(){
	float lengthLeftRight = (lengthLeftVertex + lengthRightVertex) / 2;
	float lengthTopBottom = (lengthTopVertex + lengthBottomVertex) / 2;
	return abs(lengthLeftRight / lengthTopBottom - 1);
}

int rectangular::CalcNumberOfNearObjects(std::vector<rectangular> inputList){
	int counter = 0;
	float sizeRelation;
	for (int i = 0; i < (int)inputList.size(); i++){
		sizeRelation = abs(this->size / inputList.at(i).size - 1);
		if (distance_between_points(inputList.at(i).center, this->center) < 8 * this->meanVertex && sizeRelation < 0.5){
			counter++;
		}
	}
	this->numberOfNearObjects = counter;
	return counter;
}

int rectangular::countRectsAround(std::vector<rectangular> inputList){
	for (int i=0; i < (int)inputList.size(); i++){
		Point otherCenter = inputList.at(i).center;
		// Calculate how many rects could between this and the other one, in x and in y direction
		int counterLeftRight = (int)round((this->center.x - otherCenter.x) / (this->meanVertex*1.2)); 
		if (counterLeftRight < 0 && abs(counterLeftRight) > rectsRight) rectsRight = abs(counterLeftRight);
		if (counterLeftRight > 0 && abs(counterLeftRight) > rectsLeft) rectsLeft = abs(counterLeftRight);
		int counterAboveBelow = (int)round((this->center.y - otherCenter.y) / (this->meanVertex*1.2));
		if (counterAboveBelow < 0 && abs(counterAboveBelow) > rectsBelow) rectsBelow = abs(counterAboveBelow);
		if (counterAboveBelow > 0 && abs(counterAboveBelow) > rectsAbove) rectsAbove = abs(counterAboveBelow);
	}
	if ((rectsLeft + rectsRight + rectsBelow + rectsAbove) != 8)
	{
		return 0;
	}
	if ((rectsLeft + rectsRight) > (rectsBelow + rectsAbove))
		return 1;
	else
		return -1;
}

int rectangular::CreatePointlists(vector<Point2f> *obj, vector<Point2f> *scene, int sizeOfRect, int sizeBetweenRects, int orientation){
	if (orientation > 0){
		destRectsLeft = rectsLeft;
		destRectsAbove = rectsAbove;
	}
	else{
		destRectsLeft = rectsAbove;
		destRectsAbove = 3 - rectsLeft;
	}
	
	// Get the keypoints from the good matches
	Point2f point;
	// Src: leftTopCorner
	point.x = (float)this->leftTopCorner.x;
	point.y = (float)this->leftTopCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// Dst: LeftTopCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// Dst: LeftBottomCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	// Src: RightTopCorner
	point.x = (float)this->rightTopCorner.x;
	point.y = (float)this->rightTopCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// Dst: RightTopCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// Dst: LeftTopCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	// Src: RightBottomCorner
	point.x = (float)this->rightBottomCorner.x;
	point.y = (float)this->rightBottomCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// Dst: RightBottomCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// Dst: RightTopCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	// Src: LeftBottomCorner
	point.x = (float)this->leftBottomCorner.x;
	point.y = (float)this->leftBottomCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// Dst: LeftBottomCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// Dst: RightBottomCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	return 0;
}

float distance_between_points(Point input1, Point input2){
	return (float)sqrt((input1.x - input2.x)*(input1.x - input2.x) + (input1.y - input2.y)*(input1.y - input2.y));
}