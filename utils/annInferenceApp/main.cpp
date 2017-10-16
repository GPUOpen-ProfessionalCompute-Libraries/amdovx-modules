#include "inference_control.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    inference_control control(argv[1] ? atoi(argv[1]) : 0);
    control.show();

    return a.exec();
}
