#include<vector>
#include<opencv2/core/core.hpp>
#include <opencv2/video.hpp>
#include<opencv2/highgui/highgui.hpp>
#include "inputparameters.h"


using namespace cv;
using namespace std;

Mat read_input(params parameter);

vector<Vec3f> create_real_colors();

Vec3f convert2float(Vec3b input);
Mat degamma(Mat image);
vector<Vec3f> degamma(vector<Vec3f> input);
Vec3f degamma(Vec3b intensity_in);
Mat gamma(Mat image);
Vec3b gamma(Vec3f intensity_in);

int show_image(string name, Mat *image);