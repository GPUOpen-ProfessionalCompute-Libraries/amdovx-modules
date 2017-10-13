#ifndef INFERENCE_VIEWER_H
#define INFERENCE_VIEWER_H

#include "inference_receiver.h"
#include <QWidget>
#include <QFont>
#include <QThread>
#include <QVector>

namespace Ui {
class inference_viewer;
}

class inference_state {
public:
    inference_state();
    // configuration
    QFont statusBarFont;
    QFont exitButtonFont;
    // image dataset
    bool wordsLoadDone;
    bool labelLoadDone;
    bool imageLoadDone;
    int imageLoadCount;
    bool imagePixmapDone;
    int imagePixmapCount;
    QVector<QByteArray> imageBuffer;
    QVector<QPixmap> imagePixmap;
    QVector<int> imageLabel;
    QVector<QString> labelName;
    QVector<int> inferenceResultTop;
    QVector<QString> inferenceResultSummary;
    // receiver
    QThread * receiver_thread;
    inference_receiver * receiver_worker;
    // rendering state
    float offsetSeconds;
    QVector<int> resultImageIndex;
    QVector<int> resultImageLabel;
    QVector<QString> resultImageSummary;
    // mouse events
    bool abortLoadingRequested;
    bool exitButtonPressed;
    QRect exitButtonRect;
    QRect statusBarRect;
    bool mouseClicked;
    int mouseLeftClickX;
    int mouseLeftClickY;
    int mouseSelectedImage;
    bool viewRecentResults;
    QString dataLabelsFilename;
    int dataLabelCount;
    QString datasetFilename;
    QString datasetFolder;
    int datasetSize;
    int datasetStartOffset;
    QVector<QString> datasetImageFilenames;
    int inputDim[4];
    int numGPUs;
    QString serverHost;
    int serverPort;
};

class inference_viewer : public QWidget
{
    Q_OBJECT

public:
    explicit inference_viewer(
            QString serverHost, int serverPort,
            QString labelsFilename, QString dataFilename, QString dataFolder,
            int dim[4], int gpuCount,
            QWidget *parent = 0);
    ~inference_viewer();

public slots:
    void errorString(QString err);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void keyReleaseEvent(QKeyEvent *) override;
private:
    void startReceiver();

private:
    // ui
    Ui::inference_viewer *ui;
    // state
    inference_state * state;
};

#endif // INFERENCE_VIEWER_H
