#include<vector>
#include<opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>


using namespace cv;
using namespace std;

vector<Vec3f> create_real_colors();

Vec3f convert2float(Vec3b input);
Mat degamma(Mat image);
vector<Vec3f> degamma(vector<Vec3f> input);
Vec3f degamma(Vec3b intensity_in);
Mat gamma(Mat image);
Vec3b gamma(Vec3f intensity_in);

Vec3f colorspace_conversion(Mat_<float> conversion_matrix, Vec3f input);
int colorspace_conversion(Mat_<float> conversion_matrix, vector<Vec3f> *input);
Mat_<float> YUV_Colorspace();
Mat_<float> RGB_Colorspace();
Mat_<float> XYZ_Colorspace();

int show_image(char* name, Mat *image);