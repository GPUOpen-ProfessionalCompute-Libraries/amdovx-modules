#include<vector>
#include<opencv2/core/core.hpp>

using namespace cv;
using namespace std;

#ifndef Color_Correction_Matrix
#define Color_Correction_Matrix
class color_correction_matrix
{
public:

	color_correction_matrix(vector<Vec3f>real_colors, vector<Vec3f> image_colors, int mode);
	color_correction_matrix(char* filename);
	int apply(Mat_<Vec3f> *image);
	int print();
	int save_to_CV_file(char* filename);
	int save_to_LOOM_file(char* filename);

	int change_colorspace(Mat_<float> conversion_matrix_to_new);

private:
	Mat values;
};

#endif

Mat construct3xk_from_vectorlist(vector<Vec3f> input);
Mat construct2xk_from_vectorlist(vector<Vec3f> input, int col1, int col2);
Mat construct4xk_from_vectorlist(vector<Vec3f> input);
Mat construct3xk_from_vectorlist(vector<Vec3f> input, int col1, int col2);