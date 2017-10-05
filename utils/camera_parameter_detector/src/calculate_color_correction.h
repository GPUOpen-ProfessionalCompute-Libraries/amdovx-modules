#include<vector>
#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>

#include "utils.h"
#include "rectangular_list.hpp"
#include "color_correction_matrix.hpp"
#include "inputparameters.h"

using namespace cv;
using namespace std;

int calculate_color_correction(params parameter);