#include<vector>
#include<opencv2/core/core.hpp>
#include <opencv2/video.hpp>
#include<opencv2/highgui/highgui.hpp>

#include "rect.hpp"

using namespace cv;
using namespace std;



class rectangular_list
{
public:
	bool valid = true;
	int create_from_image(Mat *image);
	int create_from_image_with_OpenCV(Mat image);
	int drawSquares(Mat& image, char *wndname);
	int size();
	int check_vertex(Mat *image);
	int check_near_objects(Mat *image);

	int find_orientation();

	int create_obj_scene_list(int size_of_rect);

	int create_transformation_parameters(Mat input_image, Mat *output_image, Mat *output_image_rotated);

	vector<Vec3f> generate_color_vector(Mat_<float> image);

private:
	// lists
	vector<rectangular> rect_list;
	vector<vector<Point>> square_list;
	vector<Point2f> obj_list;
	vector<Point2f> scene_list;

	// values
	int maxNearObjects = 0;
	int orientation = 0;
	float mean_size = 0;
	float mean_vertex = 0;
	int size_of_rect;
	int size_between_rect;
	Mat_<float> transformation_parameters;

	// functions
	int calc_maxNearObjects();
	int check_resulting_list(char *text);

};

Vec3f calculate_color(Point center, Mat Transformation, Mat image, int predicted_size);
static double angle(Point pt1, Point pt2, Point pt0);