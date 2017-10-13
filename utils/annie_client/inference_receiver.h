#ifndef INFERENCE_RECEIVER_H
#define INFERENCE_RECEIVER_H

#include <QObject>
#include <QVector>
#include <QQueue>
#include <QMutex>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <mutex>

class inference_receiver : public QObject
{
    Q_OBJECT
public:
    explicit inference_receiver(QString serverHost, int serverPort, QVector<QByteArray> * imageBuffer, QObject *parent = nullptr);
    ~inference_receiver();

    static void abort();
    void setImageCount(int imageCount, int labelCount);
    void getReceivedList(QVector<int>& indexQ, QVector<int>& labelQ, QVector<QString>& summaryQ);
    float getPerfImagesPerSecond();

signals:
    void finished();
    void error(QString err);

public slots:
    void run();

private:
    static bool abortRequsted;

private:
    std::mutex mutex;
    int imageCount;
    int labelCount;
    QQueue<int> imageIndex;
    QQueue<int> imageLabel;
    QQueue<QString> imageSummary;
    QElapsedTimer perfTimer;
    int perfImageCount;
    float perfRate;
    QVector<QByteArray> * imageBuffer;
    QString serverHost;
    int serverPort;
};

#endif // INFERENCE_RECEIVER_H
