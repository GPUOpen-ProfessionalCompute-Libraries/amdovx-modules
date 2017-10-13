#include "inference_viewer.h"
#include "ui_inference_viewer.h"
#include <QPainter>
#include <QTimer>
#include <QTime>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QPushButton>
#include <QMessageBox>

#define WINDOW_TITLE             "Inference Viewer"
#define ICON_SIZE                64
#define ICON_STRIDE              (ICON_SIZE + 8)
#define INFCOM_RUNTIME_OPTIONS   ""

inference_state::inference_state()
{
    // initialize
    wordsLoadDone = false;
    labelLoadDone = false;
    imageLoadDone = false;
    imageLoadCount = 0;
    imagePixmapDone = false;
    imagePixmapCount = 0;
    receiver_thread = nullptr;
    receiver_worker = nullptr;
    mouseClicked = false;
    mouseLeftClickX = 0;
    mouseLeftClickY = 0;
    mouseSelectedImage = -1;
    viewRecentResults = false;
    statusBarFont.setFamily(statusBarFont.defaultFamily());
    statusBarFont.setItalic(true);
    exitButtonFont.setFamily(exitButtonFont.defaultFamily());
    exitButtonFont.setBold(true);
    exitButtonPressed = false;
    exitButtonRect = QRect(0, 0, 0, 0);
    statusBarRect = QRect(0, 0, 0, 0);
    abortLoadingRequested = false;
    QTime time = QTime::currentTime();
    offsetSeconds = 60.0 - (time.second() + time.msec() / 1000.0);
    // dataset config
    dataLabelsFilename = "/Users/radha/work/neuralnets/datasets/ILSVRC2012_img_synset_words.txt";
    dataLabelCount = 1000;
    datasetFilename = "/Users/radha/work/neuralnets/datasets/ILSVRC2012_img_val.txt";
    datasetFolder = "/Users/radha/work/neuralnets/datasets/ILSVRC2012_img_val";
    datasetSize = 2000; // 50000
    datasetStartOffset = 1000;
    inputDim[0] = 224;
    inputDim[1] = 224;
    inputDim[2] = 3;
    inputDim[3] = 32;
    GPUs = 1;
    serverHost = "";
    serverPort = 0;
    maxDatasetSize = 0;
}

inference_viewer::inference_viewer(
        QString serverHost, int serverPort,
        QString labelsFilename, QString dataFilename, QString dataFolder,
        int dimInput[4], int GPUs, int dimOutput[4], int maxDatasetSize,
        QWidget *parent) :
    QWidget(parent),
    ui(new Ui::inference_viewer)
{
    state = new inference_state();
    if(labelsFilename.size() > 0)
        state->dataLabelsFilename = labelsFilename;
    if(dataFilename.size() > 0)
        state->datasetFilename = dataFilename;
    if(dataFolder.size() > 0)
        state->datasetFolder = dataFolder;
    state->maxDatasetSize = maxDatasetSize;
    state->inputDim[0] = dimInput[0];
    state->inputDim[1] = dimInput[1];
    state->inputDim[2] = dimInput[2];
    state->inputDim[3] = dimInput[3];
    state->GPUs = GPUs;
    state->outputDim[0] = dimOutput[0];
    state->outputDim[1] = dimOutput[1];
    state->outputDim[2] = dimOutput[2];
    state->outputDim[3] = dimOutput[3];
    state->serverHost = serverHost;
    state->serverPort = serverPort;
    progress.completed = false;
    progress.errorCode = 0;
    ui->setupUi(this);
    setMinimumWidth(800);
    setMinimumHeight(800);
    setWindowTitle(tr(WINDOW_TITLE));
    showMaximized();

    // start timer for update
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(40);
}

void inference_viewer::startReceiver()
{
    // start receiver thread
    state->receiver_thread = new QThread;
    state->receiver_worker = new inference_receiver(
                state->serverHost, state->serverPort,
                state->GPUs, state->inputDim, state->outputDim, INFCOM_RUNTIME_OPTIONS,
                &state->imageBuffer, &progress);
    state->receiver_worker->moveToThread(state->receiver_thread);
    connect(state->receiver_worker, SIGNAL (error(QString)), this, SLOT (errorString(QString)));
    connect(state->receiver_thread, SIGNAL (started()), state->receiver_worker, SLOT (run()));
    connect(state->receiver_worker, SIGNAL (finished()), state->receiver_thread, SLOT (quit()));
    connect(state->receiver_worker, SIGNAL (finished()), state->receiver_worker, SLOT (deleteLater()));
    connect(state->receiver_thread, SIGNAL (finished()), state->receiver_thread, SLOT (deleteLater()));
    state->receiver_thread->start();
    state->receiver_thread->terminate();
}

inference_viewer::~inference_viewer()
{
    inference_receiver::abort();
    delete state;
    delete ui;
}

void inference_viewer::errorString(QString /*err*/)
{
    qDebug("ERROR: inference_viewer: ...");
}

void inference_viewer::mousePressEvent(QMouseEvent * event)
{
    if(event->button() == Qt::LeftButton) {
        int x = event->pos().x();
        int y = event->pos().y();
        state->mouseLeftClickX = x;
        state->mouseLeftClickY = y;
        state->mouseClicked = true;
        state->viewRecentResults = false;
        // check exit button
        state->exitButtonPressed = false;
        if((x >= state->exitButtonRect.x()) &&
           (x < (state->exitButtonRect.x() + state->exitButtonRect.width())) &&
           (y >= state->exitButtonRect.y()) &&
           (y < (state->exitButtonRect.y() + state->exitButtonRect.height())))
        {
            state->exitButtonPressed = true;
        }
    }
}

void inference_viewer::mouseReleaseEvent(QMouseEvent * event)
{
    if(event->button() == Qt::LeftButton) {
        // check exit button
        int x = event->pos().x();
        int y = event->pos().y();
        if((x >= state->exitButtonRect.x()) &&
           (x < (state->exitButtonRect.x() + state->exitButtonRect.width())) &&
           (y >= state->exitButtonRect.y()) &&
           (y < (state->exitButtonRect.y() + state->exitButtonRect.height())))
        {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, windowTitle(), "Do you really want to exit?", QMessageBox::Yes|QMessageBox::No);
            if(reply == QMessageBox::Yes) {
                close();
            }
        }
        state->exitButtonPressed = false;
        // abort loading
        if(!state->imageLoadDone &&
           (x >= state->statusBarRect.x()) &&
           (x < (state->statusBarRect.x() + state->statusBarRect.width())) &&
           (y >= state->statusBarRect.y()) &&
           (y < (state->statusBarRect.y() + state->statusBarRect.height())))
        {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, windowTitle(), "Do you want to abort loading?", QMessageBox::Yes|QMessageBox::No);
            if(reply == QMessageBox::Yes) {
                state->abortLoadingRequested = true;
            }
        }
    }
}

void inference_viewer::keyReleaseEvent(QKeyEvent * event)
{
    if(event->key() == Qt::Key_Escape) {
        state->mouseSelectedImage = -1;
        state->viewRecentResults = false;
        fatalError = "";
    }
    else if(event->key() == Qt::Key_Space) {
        state->viewRecentResults = !state->viewRecentResults;
    }
    else if(event->key() == Qt::Key_Q) {
        close();
    }
}

void inference_viewer::paintEvent(QPaintEvent *)
{
    // configuration
    int imageCountLimitForInferenceStart = std::min(state->datasetSize, state->datasetStartOffset);
    const int loadBatchSize = 60;
    const int scaleBatchSize = 30;

    // calculate window layout
    QString exitButtonText = " Close ";
    QFontMetrics fontMetrics(state->statusBarFont);
    int buttonTextWidth = fontMetrics.width(exitButtonText);
    int statusBarX = 8 + 10 + buttonTextWidth + 10 + 8, statusBarY = 8;
    int statusBarWidth = width() - 8 - statusBarX;
    int statusBarHeight = fontMetrics.height() + 16;
    int imageX = 8;
    int imageY = statusBarY + statusBarHeight + 8;
    state->exitButtonRect = QRect(8, statusBarY, 10 + buttonTextWidth + 10, statusBarHeight);
    state->statusBarRect = QRect(statusBarX, statusBarY, statusBarWidth, statusBarHeight);
    QColor statusBarColorBackground(192, 192, 192);
    QColor statusBarColorLoad(255, 192, 64);
    QColor statusBarColorDecode(128, 192, 255);
    QColor statusBarColorQueue(128, 255, 128);
    QColor statusTextColor(0, 0, 128);
    QColor exitButtonBorderColor(128, 16, 32);
    QColor exitButtonBrushColor(150, 150, 175);
    QColor exitButtonBrushColorPressed(192, 175, 175);
    QColor exitButtonTextColor(128, 0, 0);

    // status bar info
    QString statusText;

    // load labels and images
    if(!state->wordsLoadDone) {
        QFile fileObj(state->dataLabelsFilename);
        if(fileObj.open(QIODevice::ReadOnly)) {
            QTextStream fileInput(&fileObj);
            while (!fileInput.atEnd()) {
                QString line = fileInput.readLine();
                if(line.size() > 0)
                    state->labelName.push_back(line);
            }
            qDebug("OK: loaded words for %d labels from %s", state->labelName.size(), state->dataLabelsFilename.toStdString().c_str());
            if(state->labelName.size() != state->outputDim[2]) {
                fatalError.sprintf("ERROR: need %d labels in %s: found %d", state->outputDim[2],
                        state->dataLabelsFilename.toStdString().c_str(), state->labelName.size());
            }
            state->dataLabelCount = state->labelName.size();
        }
        else {
            fatalError.sprintf("ERROR: unable to open: %s", state->dataLabelsFilename.toStdString().c_str());
        }
        state->wordsLoadDone = true;
    }
    if(!state->labelLoadDone) {
        QFile fileObj(state->datasetFilename);
        if(fileObj.open(QIODevice::ReadOnly)) {
            QTextStream fileInput(&fileObj);
            int count = 0;
            while (!fileInput.atEnd()) {
                QString line = fileInput.readLine();
                if(line.size() == 0)
                    continue;
                QStringList words = line.split(" ");
                if(words.size() != 2) {
                    fatalError.sprintf("ERROR: incorrectly formatted text at %s#%d: %s", state->datasetFilename.toStdString().c_str(), state->datasetImageFilenames.size()+1, line.toStdString().c_str());
                    break;
                }
                int label = words[1].toInt();
                state->datasetImageFilenames.push_back(words[0]);
                state->imageLabel.push_back(label);
                state->inferenceResultTop.push_back(-1);
                state->inferenceResultSummary.push_back("Not available");
                count++;
                if(state->maxDatasetSize > 0 && count == state->maxDatasetSize)
                    break;
            }
            qDebug("OK: loaded labels for %d images from %s", state->imageLabel.size(), state->datasetFilename.toStdString().c_str());
        }
        else {
            qDebug("ERROR: unable to open: %s", state->datasetFilename.toStdString().c_str());
        }
        state->datasetSize = state->datasetImageFilenames.size();
        state->labelLoadDone = true;
    }
    if(!state->imageLoadDone) {
        for(int i = 0; i < loadBatchSize; i++) {
            if(state->imageLoadCount == state->datasetSize || state->abortLoadingRequested) {
                state->datasetSize = state->imageLoadCount;
                state->imageLoadDone = true;
                break;
            }
            QString fileName = state->datasetFolder + "/" + state->datasetImageFilenames[state->imageLoadCount];
            QFile fileObj(fileName);
            if(!fileObj.open(QIODevice::ReadOnly)) {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::critical(this, windowTitle(), "Do you want to continue?\n\nERROR: Unable to open: " + fileName, QMessageBox::Yes|QMessageBox::No);
                if(reply == QMessageBox::No) {
                    exit(1);
                    return;
                }
                state->datasetSize = state->imageLoadCount;
                state->imageLoadDone = true;
                break;
            }
            QByteArray byteArray = fileObj.readAll();
            state->imageBuffer.push_back(byteArray);
            state->imageLoadCount++;
        }
    }
    if(state->imageLoadCount > 0 && !state->imagePixmapDone) {
        for(int i = 0; i < scaleBatchSize; i++) {
            if(state->imageLoadDone && state->imagePixmapCount == state->imageLoadCount) {
                state->imagePixmapDone = true;
                break;
            }
            if(state->imagePixmapCount >= state->imageBuffer.size()) {
                break;
            }
            QPixmap pixmap;
            if(!pixmap.loadFromData(state->imageBuffer[state->imagePixmapCount])) {
                state->imagePixmapDone = true;
                break;
            }
            state->imagePixmap.push_back(pixmap.scaled(ICON_SIZE, ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            state->imagePixmapCount++;
        }
        if(state->receiver_worker)
            state->receiver_worker->setImageCount(state->imagePixmapCount, state->labelName.size());
    }

    if(!state->receiver_worker && state->imagePixmapCount >= imageCountLimitForInferenceStart) {
        startReceiver();
        state->receiver_worker->setImageCount(state->imagePixmapCount, state->labelName.size());
    }

    // initialize painter object
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // draw status bar
    int subBarHeight = statusBarHeight * 70 / 100;
    float progressLoad = (float) state->imageLoadCount / std::max(1, state->datasetSize);
    float progressDecode = (float) state->imagePixmapCount / std::max(1, state->imageLoadCount);
    if(state->imagePixmapDone) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(statusBarColorDecode);
        painter.drawRect(state->statusBarRect);
        statusText.sprintf("Cycling through %d images from the dataset", state->imagePixmapCount);
    }
    else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(statusBarColorBackground);
        painter.drawRect(state->statusBarRect);
        if(progressLoad > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(statusBarColorLoad);
            painter.drawRect(QRect(state->statusBarRect.x(), state->statusBarRect.y(),
                                   state->statusBarRect.width() * progressLoad, statusBarHeight));
            QString text;
            text.sprintf(" loaded[%d/%d]", state->imageLoadCount, state->datasetSize);
            statusText += text;
        }
        if(progressDecode > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(statusBarColorDecode);
            painter.drawRect(QRect(state->statusBarRect.x(), state->statusBarRect.y() + (statusBarHeight - subBarHeight) / 2,
                                   state->statusBarRect.width() * progressLoad * progressDecode, subBarHeight));
            QString text;
            text.sprintf(" decoded[%d/%d]", state->imagePixmapCount, state->imageLoadCount);
            statusText += text;
        }
    }

    // get image render list from receiver
    if(state->receiver_worker) {
        state->receiver_worker->getReceivedList(state->resultImageIndex, state->resultImageLabel, state->resultImageSummary);
    }

    // trim the render list to viewable area
    int numCols = (width() - imageX) / ICON_STRIDE * 4;
    int numRows = (height() - imageY) / (ICON_STRIDE / 4);
    int imageCount = state->resultImageIndex.size();
    int imageRows = imageCount / numCols;
    int imageCols = imageCount % numCols;
    if(state->resultImageIndex.size() > 0) {
        while(imageRows >= numRows) {
            state->resultImageIndex.erase(state->resultImageIndex.begin(), state->resultImageIndex.begin() + 4 * numCols);
            state->resultImageLabel.erase(state->resultImageLabel.begin(), state->resultImageLabel.begin() + 4 * numCols);
            state->resultImageSummary.erase(state->resultImageSummary.begin(), state->resultImageSummary.begin() + 4 * numCols);
            imageCount = state->resultImageIndex.size();
            imageRows = imageCount / numCols;
            imageCols = imageCount % numCols;
        }
        // get received image/rate
        float imagesPerSec = state->receiver_worker->getPerfImagesPerSecond();
        if(imagesPerSec > 0) {
            QString text;
            text.sprintf("... %.1f images/sec", imagesPerSec);
            statusText += text;
        }
    }

    // render exit button
    setFont(state->exitButtonFont);
    painter.setPen(exitButtonBorderColor);
    painter.setBrush(state->exitButtonPressed ? exitButtonBrushColorPressed : exitButtonBrushColor);
    painter.drawRoundedRect(state->exitButtonRect, 4, 4);
    painter.setPen(exitButtonTextColor);
    painter.drawText(state->exitButtonRect, Qt::AlignCenter, exitButtonText);

    // in case fatal error, replace status text with the error message
    if(fatalError.length() > 0)
        statusText = fatalError;

    // render status bar text
    if (statusText.length() > 0) {
        setFont(state->statusBarFont);
        painter.setPen(statusTextColor);
        painter.drawText(state->statusBarRect, Qt::AlignCenter, statusText);
    }
    painter.setPen(statusTextColor);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(state->statusBarRect);

    // render image list
    if(state->resultImageIndex.size() > 0) {
        for(int item = 0; item < imageCount; item++) {
            // update image data
            int index = state->resultImageIndex[item];
            int label = state->resultImageLabel[item];
            QString summary = state->resultImageSummary[item];
            state->inferenceResultTop[index] = label;
            state->inferenceResultSummary[index] = summary;
            // draw image
            int col = item % numCols;
            int row = item / numCols;
            int x = imageX + col * ICON_STRIDE / 4;
            int y = imageY + row * ICON_STRIDE / 4;
            int w = ICON_SIZE / 4;
            int h = ICON_SIZE / 4;
            bool enableImageDraw = false;
            if((row / 2) < (numRows / 4)) {
                if((row % 2) == 0 && (col % 2) == 0) {
                    w += ICON_STRIDE / 4;
                    h += ICON_STRIDE / 4;
                    if ((row / 4) < (numRows / 16)) {
                        if((row % 4) == 0 && (col % 4) == 0) {
                            w += ICON_STRIDE / 2;
                            h += ICON_STRIDE / 2;
                            enableImageDraw = true;
                        }
                    }
                    else {
                        enableImageDraw = true;
                    }
                }
            }
            else {
                enableImageDraw = true;
            }
            if(enableImageDraw) {
                painter.drawPixmap(x, y, w, h, state->imagePixmap[index]);
                if(state->imageLabel[index] == state->inferenceResultTop[index]) {
                    painter.setPen(Qt::darkGreen);
                }
                else {
                    painter.setPen(Qt::red);
                }
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(x, y, w, h);
                if(state->mouseClicked &&
                        (state->mouseLeftClickX >= x) && (state->mouseLeftClickX < (x + w)) &&
                        (state->mouseLeftClickY >= y) && (state->mouseLeftClickY < (y + h)))
                {
                    // mark the selected image for viewing
                    state->mouseClicked = false;
                    state->viewRecentResults = false;
                    state->mouseSelectedImage = index;
                }
            }
            if(state->viewRecentResults) {
                state->mouseSelectedImage = index;
            }
        }
    }
    state->mouseClicked = false;

    // view inference results
    if(state->mouseSelectedImage >= 0) {
        int index = state->mouseSelectedImage;
        int truthLabel = state->imageLabel[index];
        QString truthSummary = state->labelName[truthLabel];
        int resultLabel = state->inferenceResultTop[index];
        QString resultSummary = state->inferenceResultSummary[index];
        int w = 4 + ICON_SIZE * 2 + 600;
        int h = 4 + ICON_SIZE * 2 + 4 + fontMetrics.height() + 4;
        int x = width()/2 - w/2;
        int y = height()/2 - h/2;
        painter.setPen(Qt::black);
        painter.setBrush(Qt::white);
        painter.drawRect(x, y, w, h);
        painter.drawPixmap(x + 4, y + 4, ICON_SIZE * 2, ICON_SIZE * 2, state->imagePixmap[index]);
        painter.setBrush(Qt::NoBrush);
        if(truthLabel == resultLabel) {
            painter.setPen(Qt::darkGreen);
        }
        else {
            painter.setPen(Qt::red);
        }
        painter.drawRect(x + 4, y + 4, ICON_SIZE * 2, ICON_SIZE * 2);
        painter.setPen(statusTextColor);
        painter.setBrush(Qt::white);
        QString text;
        QFont font = state->statusBarFont;
        font.setBold(true);
        font.setItalic(false);
        setFont(font);
        painter.setPen(Qt::black);
        text.sprintf("IMAGE#%d", index);
        painter.drawText(QRect(x + 4 + ICON_SIZE * 2 + 4, y + 4, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, text);
        font.setBold(false);
        font.setItalic(false);
        setFont(font);
        painter.setPen(Qt::blue);
        text.sprintf("result[label=%d] ", resultLabel);
        text += resultSummary;
        painter.drawText(QRect(x + 4 + ICON_SIZE * 2 + 4, y + 4 + fontMetrics.height() + 8, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, text);
        font.setItalic(true);
        setFont(font);
        painter.setPen(Qt::gray);
        text.sprintf("truth[label=%d] ", truthLabel);
        text += truthSummary;
        painter.drawText(QRect(x + 4, y + 4 + ICON_SIZE * 2 + 4, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, text);
    }

    // render clock
    static const QPoint secondHand[3] = {
        QPoint(8, 12),
        QPoint(-8, 12),
        QPoint(0, -80)
    };
    QColor secondColor(32, 32, 32, 192);
    QTime time = QTime::currentTime();
    painter.save();
    painter.translate(statusBarX + statusBarWidth - statusBarHeight / 2, statusBarY + statusBarHeight / 2);
    painter.scale(statusBarHeight / 2 / 100.0, statusBarHeight / 2 / 100.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(secondColor);
    painter.rotate(6.0 * (state->offsetSeconds + time.second() + time.msec() / 1000.0));
    painter.drawConvexPolygon(secondHand, 3);
    painter.restore();
}
