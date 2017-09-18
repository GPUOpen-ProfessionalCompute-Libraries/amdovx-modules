#include "rect.hpp"

// Public ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
rectangular::rectangular(Mat *image, int x, int y){
	// Set parameters for search
	int max_skip_pixel = 2;
	int width = image->cols;
	int height = image->rows;

	// Search for corners
	Point lastPoint;
	lastPoint.x = x;
	lastPoint.y = y;

	this->leftTopCorner = lastPoint;
	this->cornerPoints.push_back(lastPoint);

	Point startPoint = lastPoint;
	this->valid = true;

	int skipped_pixels = 0;
	int search_status = 10; 
	// 0: Search Done, 10: Search right, 20: Search Down, 30: Search Left, 40: Search Up
	//                 11: Check right > Down, 21: Check Down > Left
	//                 12: Skipping right
	//                 13: Remove skipped pixels
	//                 15: Search Right Again
	//                 16: Check right

	while (search_status != 0){
		while (search_status == 10 || search_status == 12 || search_status == 15){ // Search Right
			if (search_status == 15 && (!(lastPoint.x < (startPoint.x - 1) || lastPoint.x >(startPoint.x + 1) || lastPoint.y < (startPoint.y - 1) || lastPoint.y >(startPoint.y + 1))))
			{
				this->valid = true;
				search_status = 0;
				break;
			}
			if (lastPoint.x == width - 1 || (lastPoint.y < height - 3 && image->at<uchar>(Point(lastPoint.x, lastPoint.y + 1)) > 250 && image->at<uchar>(Point(lastPoint.x, lastPoint.y + 2)) > 250 && lastPoint.x > (this->leftTopCorner.x + 5)))
			{
				if (search_status == 15)
				{
					this->valid = false;
					search_status = 0;
					break;
				}
				else{
					search_status = 11;
					break;
				}
			}
			else if (image->at<uchar>(Point(lastPoint.x + 1, lastPoint.y)) > 250)
			{
				lastPoint.x++;
				skipped_pixels = 0;
				search_status = (search_status == 12) ? 10 : search_status;
			}
			else if (image->at<uchar>(Point(lastPoint.x + 1, max(0, lastPoint.y - 1))) > 250)
			{
				lastPoint.x++;
				lastPoint.y = max(0, lastPoint.y - 1);
				skipped_pixels = 0;
				search_status = (search_status == 12) ? 10 : search_status;
			}			
			else if (image->at<uchar>(Point(lastPoint.x + 1, min(height - 1, lastPoint.y + 1))) > 250)
			{
				lastPoint.x++;
				lastPoint.y = min(height - 1, lastPoint.y + 1);
				skipped_pixels = 0;
				search_status = (search_status == 12) ? 10 : search_status;
			}
			else if (search_status == 10)
			{
				search_status = 11;
				break;
			}
			else if (search_status == 12){
				search_status = 11;
				break;
			}
			else if (search_status == 15){
				search_status = 16;
				break;
			}				
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == 11)
		{
			if (this->leftTopCorner.x+5 > lastPoint.x)
			{
				search_status = 0;
				this->valid = false;
			}
			else{
				this->rightTopCorner = lastPoint;
				search_status = 20;
			}
		}

		if (search_status == 16)
		{
			if (this->leftTopCorner.x == lastPoint.x)
			{
				if (lastPoint.y == 0 || skipped_pixels >= max_skip_pixel){
					search_status = 0;
					this->valid = false;
				}
				else{
					lastPoint.y--;
					skipped_pixels = 1;
					search_status = 42;
				}
			}
			else{
				search_status = 0;
				this->valid = false;
			}
		}

		while (search_status == 20 || search_status == 22){ // Search Down
			if (lastPoint.y == height - 1 || (lastPoint.x > 2 && image->at<uchar>(Point(lastPoint.x - 1, lastPoint.y)) > 250 && image->at<uchar>(Point(lastPoint.x - 2, lastPoint.y)) > 250 && lastPoint.y > (this->rightTopCorner.y + 5)))
			{
				search_status = 21;
				break;
			}
			if (image->at<uchar>(Point(lastPoint.x, lastPoint.y + 1)) > 250)
			{
				lastPoint.y++;
				skipped_pixels = 0;
				search_status = 20;
			}
			else if (image->at<uchar>(Point(min(lastPoint.x + 1, width - 1), lastPoint.y + 1)) > 250)
			{
				lastPoint.x = min(lastPoint.x + 1, width - 1);
				lastPoint.y++;
				skipped_pixels = 0;
				search_status = 20;
			}
			else if (image->at<uchar>(Point(max(0, lastPoint.x - 1), lastPoint.y + 1)) > 250)
			{
				lastPoint.x = max(0, lastPoint.x - 1);
				lastPoint.y++;
				skipped_pixels = 0;
				search_status = 20;
			}			
			else if (search_status == 20)
			{
				search_status = 21;
				break;
			}
			else if (search_status == 22){
				search_status = 21;
				break;
			}
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == 21)
		{
			if (this->rightTopCorner.y == lastPoint.y)
			{
				if (lastPoint.x == width-1 || skipped_pixels >= max_skip_pixel){
					search_status = 0;
					this->valid = false;
				}
				else{
					lastPoint.x++;
					skipped_pixels++;
					search_status = 12;
				}			
			}
			else{
				this->rightBottomCorner = lastPoint;
				search_status = 30;
			}
		}

		while (search_status == 30 || search_status == 32)
		{
			if (lastPoint.x == 0 || (lastPoint.y > 2 && image->at<uchar>(Point(lastPoint.x, lastPoint.y - 1)) && image->at<uchar>(Point(lastPoint.x, lastPoint.y - 2)) > 250 && lastPoint.x < (this->rightBottomCorner.x - 5)))
			{
				search_status = 31;
				break;
			}
			if (image->at<uchar>(Point(lastPoint.x - 1, lastPoint.y)) > 250)
			{
				lastPoint.x--;
				skipped_pixels = 0;
				search_status = 30;
			}	
			else if (image->at<uchar>(Point(lastPoint.x - 1, min(height - 1, lastPoint.y + 1))) > 250)
			{
				lastPoint.x--;
				lastPoint.y = min(height - 1, lastPoint.y + 1);
				skipped_pixels = 0;
				search_status = 30;
			}
			else if (image->at<uchar>(Point(lastPoint.x - 1, max(0, lastPoint.y - 1))) > 250)
			{
				lastPoint.x--;
				lastPoint.y = max(0, lastPoint.y - 1);
				skipped_pixels = 0;
				search_status = 30;
			}
			else if (search_status == 30)
			{
				search_status = 31;
				break;
			}
			else if (search_status == 32){
				search_status = 31;
				break;
			}
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == 31)
		{
			if (this->rightBottomCorner.x == lastPoint.x)
			{
				if (lastPoint.y == height - 1 || skipped_pixels >= max_skip_pixel){
					search_status = 0;
					this->valid = false;
				}
				else{
					lastPoint.y++;
					skipped_pixels++;
					search_status = 22;
				}
			}
			else{
				this->leftBottomCorner = lastPoint;
				search_status = 40;
			}
		}

		while (search_status == 40 || search_status == 42)
		{
			if (search_status == 40 && (!(lastPoint.x < (startPoint.x - 1) || lastPoint.x >(startPoint.x + 1) || lastPoint.y < (startPoint.y - 1) || lastPoint.y >(startPoint.y + 1))))
			{
				this->valid = true;
				search_status = 0;
				break;
			}
			if (lastPoint.y == 0 || (lastPoint.x < width - 3 && image->at<uchar>(Point(lastPoint.x + 1, lastPoint.y)) > 250 && image->at<uchar>(Point(lastPoint.x + 2, lastPoint.y)) > 250 && lastPoint.y < (this->leftBottomCorner.y - 5)))
			{
				search_status = 41;
				break;
			}
			else if (image->at<uchar>(Point(lastPoint.x, lastPoint.y - 1)) > 250)
			{
				lastPoint.y--;
				skipped_pixels = 0;
				search_status = 40;
			}
			else if (image->at<uchar>(Point(max(0, lastPoint.x - 1), lastPoint.y - 1)) > 250)
			{
				lastPoint.x = max(0, lastPoint.x - 1);
				lastPoint.y--;
				skipped_pixels = 0;
				search_status = 40;
			}
			
			else if (image->at<uchar>(Point(min(width - 1, lastPoint.x + 1), lastPoint.y - 1)) > 250)
			{
				lastPoint.x = min(width - 1, lastPoint.x + 1);
				lastPoint.y--;
				skipped_pixels = 0;
				search_status = 40;
			}
			else if (search_status == 40)
			{
				search_status = 41;
				break;
			}
			else if (search_status == 42){
				search_status = 41;
				break;
			}
			this->cornerPoints.push_back(lastPoint);
		}

		if (search_status == 41)
		{
			if (this->leftBottomCorner.y == lastPoint.y)
			{
				if (lastPoint.x == 0 || skipped_pixels >= max_skip_pixel){
					search_status = 0;
					this->valid = false;
				}
				else{
					lastPoint.x--;
					skipped_pixels++;
					search_status = 32;
				}
			}
			else{
				this->leftTopCorner = lastPoint;
				search_status = 15;
			}
		}
	}

	// Construct all values
	if (this->valid){
		this->center.x = (float)(leftTopCorner.x + rightTopCorner.x + rightBottomCorner.x + leftBottomCorner.x) / 4;
		this->center.y = (float)(leftTopCorner.y + rightTopCorner.y + rightBottomCorner.y + leftBottomCorner.y) / 4;
		this->numberOfNearObjects = 0;

		this->lengthBottomVertex = distance_between_points(rightBottomCorner,leftBottomCorner);
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


	/*leftTopCorner = list.at(0);
	rightTopCorner = list.at(3);
	rightBottomCorner = list.at(2);
	leftBottomCorner = list.at(1);*/

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
	for (int i = 0; i < cornerPoints.size(); i++)
	{
		input->at<uchar>(Point(cornerPoints.at(i).x, cornerPoints.at(i).y)) = value;
		//input.at<uchar>(cv::Point(cornerPoints.at(i).x, cornerPoints.at(i).y)) = 50;
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
	for (int i = 0; i < inputList.size(); i++){
		sizeRelation = abs(this->size / inputList.at(i).size - 1);
		if (distance_between_points(inputList.at(i).center, this->center) < 8 * this->meanVertex && sizeRelation < 0.5){
			counter++;
		}
	}
	this->numberOfNearObjects = counter;
	return counter;
}

int rectangular::countRectsAround(std::vector<rectangular> inputList){
	for (int i=0; i < inputList.size(); i++){
		Point otherCenter = inputList.at(i).center;
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
	// leftTopCorner
	point.x = (float)this->leftTopCorner.x;
	point.y = (float)this->leftTopCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// LeftTopCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// LeftBottomCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	// RightTopCorner
	point.x = (float)this->rightTopCorner.x;
	point.y = (float)this->rightTopCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// RightTopCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// LeftTopCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	// RightBottomCorner
	point.x = (float)this->rightBottomCorner.x;
	point.y = (float)this->rightBottomCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// RightBottomCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// RightTopCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)this->destRectsAbove * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	// LeftBottomCorner
	point.x = (float)this->leftBottomCorner.x;
	point.y = (float)this->leftBottomCorner.y;
	scene->push_back(point);
	if (orientation > 0){
		// LeftBottomCorner
		point.x = (float)this->destRectsLeft * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	else{
		// RightBottomCorner
		point.x = (float)(this->destRectsLeft + 1) * sizeOfRect + (float)max(0, this->destRectsLeft - 1) * sizeBetweenRects;
		point.y = (float)(this->destRectsAbove + 1) * sizeOfRect + (float)max(0, this->destRectsAbove - 1) * sizeBetweenRects;
	}
	obj->push_back(point);

	return 0;
}

float distance_between_points(Point input1, Point input2){
	return (float)sqrt((input1.x - input2.x)*(input1.x - input2.x) + (input1.y - input2.y)*(input1.y - input2.y));
}