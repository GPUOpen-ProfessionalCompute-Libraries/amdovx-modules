#include <opencv2/core/core.hpp>
#include <vector>
#include <cmath>

using namespace cv;
using namespace std;

enum directions {
	DIRECTIONS_SEARCH_DONE      = 0,
	DIRECTIONS_SEARCH_RIGHT     = 10,
	DIRECTIONS_CHECK_RIGHT_DOWN = 11,
	DIRECTIONS_SKIPPING_RIGHT   = 12,
	DIRECTIONS_SEARCH_RIGHT_2   = 15,
	DIRECTIONS_CHECK_RIGHT_2    = 16,
	DIRECTIONS_SEARCH_DOWN      = 20,
	DIRECTIONS_CHECK_DOWN_LEFT  = 21,
	DIRECTIONS_SKIPPING_DOWN    = 22,
	DIRECTIONS_SEARCH_LEFT      = 30,
	DIRECTIONS_CHECK_LEFT_UP    = 31,
	DIRECTIONS_SKIPPING_LEFT    = 32,
	DIRECTIONS_SEARCH_UP        = 40,
	DIRECTIONS_CHECK_UP_RIGHT   = 41,
	DIRECTIONS_SKIPPING_UP      = 42,

};

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

	// Constructors:
	rectangular(Mat *image, int x, int y);
	rectangular(vector<Point> list);
	
	// General functions:
	int SetCorner(Mat *input, int value);
	int CalcNumberOfNearObjects(std::vector<rectangular> inputList);
	int countRectsAround(std::vector<rectangular> inputList); //Return 1 if RightLeft is more than AboveBelow and -1 otherwise
	int CreatePointlists(vector<Point2f> *obj, vector<Point2f> *scene, int sizeOfRect, int sizeBetweenRects, int orientation);
	vector<Point> output_pointlist();
	
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