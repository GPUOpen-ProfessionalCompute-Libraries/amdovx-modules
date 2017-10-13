#include "inference_control.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    inference_control control;
    control.show();

    return a.exec();
}
