#include <opencv2/core/core.hpp>
#include <vector>
#include <cmath>

using namespace cv;
using namespace std;

class rectangular
{
public:
	// flags:
	bool valid;

	// Points
	Point rightTopCorner;
	Point leftTopCorner;
	Point rightBottomCorner;
	Point leftBottomCorner;
	Point2f center;

	// Other values
	int numberOfNearObjects;
	float size;
	float meanVertex;

	// Constructor:
	rectangular(Mat *image, int x, int y);
	rectangular(vector<Point>);

	vector<Point> output_pointlist();

	// General functions:
	int SetCorner(Mat *input, int value);
	int CalcNumberOfNearObjects(std::vector<rectangular> inputList);
	int countRectsAround(std::vector<rectangular> inputList); //Return 1 if RightLeft is more than AboveBelow and -1 otherwise
	int CreatePointlists(vector<Point2f> *obj, vector<Point2f> *scene, int sizeOfRect, int sizeBetweenRects, int orientation);
	
	// Vertex Relation functions
	float RelationLeftRightVertex();
	float RelationTopBottomVertex();
	float RelationSquare();	

private:
	// Internal data:
	vector<Point> cornerPoints;
	int rectsLeft = 0;
	int rectsRight = 0;
	int rectsAbove = 0;
	int rectsBelow = 0;
	int destRectsLeft = 0;
	int destRectsAbove = 0;

	// vertex lengths:
	float lengthLeftVertex;
	float lengthRightVertex;
	float lengthTopVertex;
	float lengthBottomVertex;
};

float distance_between_points(Point input1, Point input2);