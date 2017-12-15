#ifndef INFERENCE_RECEIVER_H
#define INFERENCE_RECEIVER_H

#include <QObject>
#include <QVector>
#include <QQueue>
#include <QMutex>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <mutex>

struct runtime_receiver_status {
    bool completed;
    int errorCode;
    QString message;
    bool repeat_images;
    bool completed_send;
    bool completed_decode;
    bool completed_load;
    int images_loaded;
    int images_decoded;
    int images_sent;
    int images_received;
};

class inference_receiver : public QObject
{
    Q_OBJECT
public:
    explicit inference_receiver(
            QString serverHost, int serverPort, QString modelName,
            int GPUs, int * inputDim, int * outputDim, const char * runtimeOptions,
            QVector<QByteArray> * imageBuffer,
            runtime_receiver_status * progress,
            QObject *parent = nullptr);
    ~inference_receiver();

    static void abort();
    void setImageCount(int imageCount, int labelCount, QVector<QString> * dataLabels);
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
    QQueue<std::vector<int>> imageTopkLabels;
    QQueue<std::vector<float>> imageTopkConfidence;
    QQueue<QString> imageSummary;
    QElapsedTimer perfTimer;
    int perfImageCount;
    float perfRate;
    QVector<QByteArray> * imageBuffer;
    QVector<QString> * dataLabels;
    QString serverHost;
    int serverPort;
    QString modelName;
    int GPUs;
    int * inputDim;
    int * outputDim;
    const char * runtimeOptions;
    runtime_receiver_status * progress;
};

#endif // INFERENCE_RECEIVER_H
