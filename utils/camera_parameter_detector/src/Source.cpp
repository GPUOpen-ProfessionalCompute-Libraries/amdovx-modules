#include <vector>

#include <stdio.h>
#include <stdlib.h>  // for strtol
#include <iostream>

#include "calculate_color_correction.h"
#include "apply_color_correction.h"
#include "inputparameters.h"


using namespace cv;
using namespace std;

int main(int argc, char ** argv)
{
	params input_parameter;
	if (input_parameter.read(argc, argv) == -1)
		return -1;
	if (input_parameter.check() == -1)
		return -1;

	if (input_parameter.compute){
		if (calculate_color_correction(input_parameter) == -1)
			return -1;
	}
	else{
		if (apply_color_correction(input_parameter) == -1)
			return -1;
	}

	return 0;
}
