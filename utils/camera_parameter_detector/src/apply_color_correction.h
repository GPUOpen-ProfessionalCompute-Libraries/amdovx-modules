#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "color_correction_matrix.hpp"
#include "utils.h"
#include "inputparameters.h"

using namespace cv;
using namespace std;

int apply_color_correction(params parameter);