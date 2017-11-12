#include "inference_control.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    int enable_repeat_images = 0;
    if(argv[1]) enable_repeat_images = atoi(argv[1]);
    QApplication a(argc, argv);
    inference_control control(enable_repeat_images);
    control.show();

    return a.exec();
}
