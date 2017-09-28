# Camera Parameter Detector

## Description
The Camera Parameter Detector can compute certain camera parameters from a particular image. In the first version it can calculate a color correction matrix from an image of a color checker chart.

## Command-line Usage
``` camera_parameter_detector.exe [--input filename] [--help] ... ```

## Use to calculate color correction matrix
An image of a color checker from one camera is needed as input. When using fisheye lenses the color checker should be in the middle of the picture. The chart should be perpendicular to the camera and at least 100x150 pixels big.
If the software is not able to detect the color checker it will return an error.


The color correction matrix has four values per channel: Three values describe the influence of the three input channels and the fourth one is for a bias. The bias is given in a range between 0 and 1.

For using the color correction matrix for Loom, for each camera a color correction matrix should be computed and afterwards the matrixes should be copied together into one file, which can be given to Loom as input. (Compare global attribute 61)


## Available Commands

| Parameter | Description |
| ----------|-------------|
| --input filename | Input image as jpg, raw or any other format supported by OpenCV. For raw height and width need to be specified. |
| --help | Print helptext |
| --output filename | Output image (every format supported by OpenCV): This image will be color corrected |
| --correctionMatrixOpenCV filename | Color correction matrix as readable format for OpenCV or for reimporting in this tool, it will be saved when compute and read in apply mode |
| --correctionMatrixLoom filename | Color correction matrix as readable format for Loom |
| --width value | Specify width e.g. for raw |
| --height value | Specify height e.g. for raw |
| --apply | Instead of calculating the color correction matrix, it will be applied to the input |
| --gamma_corrected | Instead converting the input image to the linear colorspace, all calculations will be done in the gamma corrected colorspace |
| --colorspace space | Define the colorspace, possible options: RGB, YUV, UV |
| --colorchartImg filename |  For debug purpose define a filename to save the image of the cut and rotated colorchart |
| --saveImg | For debug purpose the internal images will be saved in the same folder |
| --showImg | For debug purpose the internal images will be shown |
