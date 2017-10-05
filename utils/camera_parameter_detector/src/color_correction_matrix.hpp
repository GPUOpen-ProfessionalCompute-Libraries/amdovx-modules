#include<vector>
#include<opencv2/core/core.hpp>

using namespace cv;
using namespace std;

#ifndef Color_Correction_Matrix
#define Color_Correction_Matrix
class color_correction_matrix
{
public:

	color_correction_matrix();
	color_correction_matrix(char* filename);
	int set_colorspace(int value);
	int calculate(vector<Vec3f>real_colors, vector<Vec3f> image_colors);
	int apply(Mat_<Vec3f> *image);
	int print();
	int save_to_CV_file(char* filename);
	int save_to_LOOM_file(char* filename);	

private:
	Mat values;
	Mat_<float> colorspace_conversion_matrix;
	int calculation_mode;

	int colorspace_conversion(vector<Vec3f> *input);
	Vec3f colorspace_conversion(Vec3f input);
	int change_colorspace();
};

#endif

Mat construct3xk_from_vectorlist(vector<Vec3f> input);
Mat construct2xk_from_vectorlist(vector<Vec3f> input, int col1, int col2);
Mat construct4xk_from_vectorlist(vector<Vec3f> input);
Mat construct3xk_from_vectorlist(vector<Vec3f> input, int col1, int col2);

Mat_<float> YUV_Colorspace();
Mat_<float> RGB_Colorspace();
Mat_<float> XYZ_Colorspace();