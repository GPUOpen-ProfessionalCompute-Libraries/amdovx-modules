#include "inference_receiver.h"
#include <QThread>
#include <QTcpSocket>

bool inference_receiver::abortRequsted = false;

void inference_receiver::abort()
{
    abortRequsted = true;
}

inference_receiver::inference_receiver(QString serverHost_, int serverPort_, QVector<QByteArray> * imageBuffer_, QObject *parent) : QObject(parent)
{
    perfRate = 0;
    perfImageCount = 0;
    perfTimer.start();
    imageCount = 0;
    labelCount = 0;
    imageBuffer = imageBuffer_;
    serverHost = serverHost_;
    serverPort = serverPort_;
}

inference_receiver::~inference_receiver()
{

}

void inference_receiver::getReceivedList(QVector<int>& indexQ, QVector<int>& labelQ, QVector<QString>& summaryQ)
{
    std::lock_guard<std::mutex> guard(mutex);
    while(imageIndex.length() > 0) {
        indexQ.push_back(imageIndex.front());
        labelQ.push_back(imageLabel.front());
        summaryQ.push_back(imageSummary.front());
        imageIndex.pop_front();
        imageLabel.pop_front();
        imageSummary.pop_front();
    }
}

void inference_receiver::run()
{
    // connect to the server for inference run-time mode
    //    - configure the connection in inference run-time mode
    //    - keep sending images and tag if server can accept more work
    //    - when results are received add the results to imageIndex, imageLabel, imageSummary queues

    // TODO: added below just for GUI testing
    int counter = 0;
    while(!abortRequsted) {
        if(imageCount > 0) {
            std::lock_guard<std::mutex> guard(mutex);
            int index = counter % imageCount;
            imageIndex.push_back(index);
            imageLabel.push_back(qrand() % labelCount);
            imageSummary.push_back("Further information not available yet");
            counter++;
            if(counter >= imageCount)
                counter = 0;
            perfImageCount++;
        }
        QThread::msleep(2);
    }

    emit finished();
}

float inference_receiver::getPerfImagesPerSecond()
{
    std::lock_guard<std::mutex> guard(mutex);
    qint64 msec = perfTimer.elapsed();
    if(msec > 2000) {
        perfRate = (float)perfImageCount * 1000.0 / (float)msec;
        perfImageCount = 0;
        perfTimer.start();
    }
    return perfRate;
}

void inference_receiver::setImageCount(int imageCount_, int labelCount_)
{
    imageCount = imageCount_;
    labelCount = labelCount_;
}
