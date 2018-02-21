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
#include <QDirIterator>
#include <QFileInfo>
#include <QFileDialog>
#include <QDesktopServices>
#include <QBuffer>

#define WINDOW_TITLE             "Inference Viewer"
#define ICON_SIZE                64
#define ICON_STRIDE              (ICON_SIZE + 8)
#define INFCOM_RUNTIME_OPTIONS   ""

inference_state::inference_state()
{
    // initialize
    dataLabels = nullptr;
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
    buttonFont.setFamily(buttonFont.defaultFamily());
    buttonFont.setBold(true);
    exitButtonPressed = false;
    exitButtonRect = QRect(0, 0, 0, 0);
    saveButtonPressed = false;
    saveButtonRect = QRect(0, 0, 0, 0);
    statusBarRect = QRect(0, 0, 0, 0);
    abortLoadingRequested = false;
    QTime time = QTime::currentTime();
    offsetSeconds = 60.0 - (time.second() + time.msec() / 1000.0);
    // imageData config
    dataLabelCount = 0;
    imageDataSize = 0;
    imageDataStartOffset = 1000;
    inputDim[0] = 224;
    inputDim[1] = 224;
    inputDim[2] = 3;
    GPUs = 1;
    serverHost = "";
    serverPort = 0;
    maxImageDataSize = 0;
}

inference_viewer::inference_viewer(QString serverHost, int serverPort, QString modelName,
        QVector<QString> * dataLabels, QString dataFilename, QString dataFolder,
        int dimInput[3], int GPUs, int dimOutput[3], int maxImageDataSize,
        bool repeat_images, bool sendScaledImages, int enableSF, int topKValue,
        QWidget *parent) :
    QWidget(parent),
    ui(new Ui::inference_viewer),
    updateTimer{ nullptr }
{
    state = new inference_state();
    state->dataLabels = dataLabels;
    state->dataFolder = dataFolder;
    state->dataFilename = dataFilename;
    state->maxImageDataSize = maxImageDataSize;
    state->inputDim[0] = dimInput[0];
    state->inputDim[1] = dimInput[1];
    state->inputDim[2] = dimInput[2];
    state->GPUs = GPUs;
    state->outputDim[0] = dimOutput[0];
    state->outputDim[1] = dimOutput[1];
    state->outputDim[2] = dimOutput[2];
    state->serverHost = serverHost;
    state->serverPort = serverPort;
    state->modelName = modelName;
    state->sendScaledImages = sendScaledImages;
    state->enableSF = enableSF;
    state->topKValue = topKValue;
    progress.completed = false;
    progress.errorCode = 0;
    progress.repeat_images = repeat_images;
    progress.completed_send = false;
    progress.completed_decode = false;
    progress.completed_load = false;
    progress.images_received = 0;
    progress.images_sent = 0;
    progress.images_decoded = 0;
    progress.images_loaded = 0;

    ui->setupUi(this);
    setMinimumWidth(800);
    setMinimumHeight(800);
    setWindowTitle(tr(WINDOW_TITLE));
    //showMaximized();

    // start timer for update
    timerStopped = false;
    updateTimer = new QTimer(this);
    connect(updateTimer, SIGNAL(timeout()), this, SLOT(update()));
    updateTimer->start(40);
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

void inference_viewer::startReceiver()
{
    // start receiver thread
    state->receiver_thread = new QThread;
    state->receiver_worker = new inference_receiver(
                state->serverHost, state->serverPort, state->modelName,
                state->GPUs, state->inputDim, state->outputDim, INFCOM_RUNTIME_OPTIONS,
                &state->imageBuffer, &progress, state->enableSF, state->topKValue);
    state->receiver_worker->moveToThread(state->receiver_thread);
    connect(state->receiver_worker, SIGNAL (error(QString)), this, SLOT (errorString(QString)));
    connect(state->receiver_thread, SIGNAL (started()), state->receiver_worker, SLOT (run()));
    connect(state->receiver_worker, SIGNAL (finished()), state->receiver_thread, SLOT (quit()));
    connect(state->receiver_worker, SIGNAL (finished()), state->receiver_worker, SLOT (deleteLater()));
    connect(state->receiver_thread, SIGNAL (finished()), state->receiver_thread, SLOT (deleteLater()));
    state->receiver_thread->start();
    state->receiver_thread->terminate();
}

void inference_viewer::terminate()
{
    if(state->receiver_worker && !progress.completed && !progress.errorCode) {
        state->receiver_worker->abort();
        for(int count = 0; count < 10 && !progress.completed; count++) {
            QThread::msleep(100);
        }
    }
    close();
}

void inference_viewer::saveResults()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Inference Results"), nullptr, tr("Text Files (*.txt);;CSV Files (*.csv)"));
    if(fileName.size() > 0) {
        QFile fileObj(fileName);
        if(fileObj.open(QIODevice::WriteOnly)) {
            bool csvFile =
                    QString::compare(QFileInfo(fileName).suffix(), "csv", Qt::CaseInsensitive) ? false : true;
            if(csvFile) {
                if(state->topKValue == 0) {
                    if(state->imageLabel[0] >= 0) {
                        fileObj.write("#FileName,outputLabel,groundTruthLabel,Matched?,outputLabelText,groundTruthLabelText\n");
                    }
                    else {
                        fileObj.write("#FileName,outputLabel,outputLabelText\n");
                    }
                }
                else {
                    if(state->imageLabel[0] >= 0) {
                        if(state->topKValue == 1)
                            fileObj.write("#FileName,outputLabel-1,groundTruthLabel,Matched?,outputLabelText-1,groundTruthLabelText\n");
                        else if(state->topKValue == 2)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,groundTruthLabel,Matched?,outputLabelText-1,outputLabelText-2,groundTruthLabelText\n");
                        else if(state->topKValue == 3)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,outputLabel-3,groundTruthLabel,Matched?,"
                                          "outputLabelText-1,outputLabelText-2,outputLabelText-3,groundTruthLabelText\n");
                        else if(state->topKValue == 4)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,outputLabel-3,outputLabel-4,groundTruthLabel,Matched?,"
                                          "outputLabelText-1,outputLabelText-2,outputLabelText-3,outputLabelText-4,groundTruthLabelText\n");
                        else if(state->topKValue == 5)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,outputLabel-3,outputLabel-4,outputLabel-5,groundTruthLabel,Matched?,"
                                          "outputLabelText-1,outputLabelText-2,outputLabelText-3,outputLabelText-4,outputLabelText-5,groundTruthLabelText\n");
                    }
                    else {
                        fileObj.write("#FileName,outputLabel,outputLabelText\n");
                    }
                }
            }
            else {
                if(state->topKValue == 0) {
                    if(state->imageLabel[0] >= 0) {
                        fileObj.write("#FileName outputLabel groundTruthLabel Matched? #outputLabelText #groundTruthLabelText\n");
                    }
                    else {
                        fileObj.write("#FileName outputLabel #outputLabelText\n");
                    }
                }
                else {
                    if(state->imageLabel[0] >= 0) {
                        if(state->topKValue == 1)
                            fileObj.write("#FileName outputLabel-1 groundTruthLabel Matched? outputLabelText-1 groundTruthLabelText\n");
                        else if(state->topKValue == 2)
                            fileObj.write("#FileName,outputLabel-1 outputLabel-2 groundTruthLabel Matched? outputLabelText-1 outputLabelText-2 groundTruthLabelText\n");
                        else if(state->topKValue == 3)
                            fileObj.write("#FileName outputLabel-1 outputLabel-2 outputLabel-3 groundTruthLabel Matched?,"
                                          "outputLabelText-1 outputLabelText-2 outputLabelText-3 groundTruthLabelText\n");
                        else if(state->topKValue == 4)
                            fileObj.write("#FileName outputLabel-1 outputLabel-2 outputLabel-3 outputLabel-4 groundTruthLabel Matched?,"
                                          "outputLabelText-1 outputLabelText-2 outputLabelText-3 outputLabelText-4 groundTruthLabelText\n");
                        else if(state->topKValue == 5)
                            fileObj.write("#FileName outputLabel-1 outputLabel-2 outputLabel-3 outputLabel-4 outputLabel-5 groundTruthLabel Matched?,"
                                          "outputLabelText-1 outputLabelText-2 outputLabelText-3 outputLabelText-4 outputLabelText-5 groundTruthLabelText\n");
                    }
                    else {
                        fileObj.write("#FileName outputLabel outputLabelText\n");
                    }
                }
            }
            int top1 = 0, top2 = 0, top3 = 0, top4 = 0, top5 = 0;
            int mismatchAll = 0, noTruthTable = 0;
            for(int i = 0; i < state->imageDataSize; i++) {
                int label = state->inferenceResultTop[i];
                int truth = state->imageLabel[i];
                QString text;
                if(csvFile) {
                    if(state->topKValue == 0){
                        if(truth >= 0) {
                            text.sprintf("%s,%d,%d,%s,\"%s\",\"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                         label, truth, label == truth ? "1" : "0",
                                         state->dataLabels ? (*state->dataLabels)[label].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                        }
                        else {
                            text.sprintf("%s,%d,-1,-1,\"%s\",Unknown\n", state->imageDataFilenames[i].toStdString().c_str(), label,
                                         state->dataLabels ? (*state->dataLabels)[label].toStdString().c_str() : "Unknown");
                        }
                    }
                    else{
                        if(truth >= 0) {
                            int match = 0;
                            if(state->topKValue == 1){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                if(truth == label_1) { match = 1; top1++; }
                                else mismatchAll++;
                                text.sprintf("%s,%d,%d,%d,\"%s\",\"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 2){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                if(truth == label_1) { match = 1; top1++; }
                                else if(truth == label_2) { match = 2; top2++; }
                                else mismatchAll++;
                                text.sprintf("%s,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 3){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                if(truth == label_1) { match = 1; top1++; }
                                else if(truth == label_2) { match = 2; top2++; }
                                else if(truth == label_3) { match = 3; top3++; }
                                else mismatchAll++;
                                text.sprintf("%s,%d,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 4){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                if(truth == label_1) { match = 1; top1++; }
                                else if(truth == label_2) { match = 2; top2++; }
                                else if(truth == label_3) { match = 3; top3++; }
                                else if(truth == label_4) { match = 4; top4++; }
                                else mismatchAll++;
                                text.sprintf("%s,%d,%d,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 5){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                int label_5 = state->resultImageLabelTopK[i][3];
                                if(truth == label_1) { match = 1; top1++; }
                                else if(truth == label_2) { match = 2; top2++; }
                                else if(truth == label_3) { match = 3; top3++; }
                                else if(truth == label_4) { match = 4; top4++; }
                                else if(truth == label_5) { match = 5; top5++; }
                                else mismatchAll++;
                                text.sprintf("%s,%d,%d,%d,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4, label_5, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                            }
                        }
                        else {
                            if(state->topKValue == 1){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                text.sprintf("%s,%d,-1,-1,\"%s\",Unknown\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 2){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                text.sprintf("%s,%d,%d,-1,-1,\"%s\",\"%s\",Unknown\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 3){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                text.sprintf("%s,%d,%d,%d,-1,-1,\"%s\",\"%s\",\"%s\",Unknown\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 4){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                text.sprintf("%s,%d,%d,%d,%d,-1,-1,\"%s\",\"%s\",\"%s\",\"%s\",Unknown\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown");
                            }
                            else if(state->topKValue == 5){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                int label_5 = state->resultImageLabelTopK[i][3];
                                text.sprintf("%s,%d,%d,%d,%d,%d,-1,-1,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",Unknown\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4, label_5,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]].toStdString().c_str() : "Unknown");
                            }
                            noTruthTable++;
                        }
                    }
                }
                else {
                    if(truth >= 0) {
                        int match = 0;
                        if(state->topKValue == 1){
                            int label_1 = state->resultImageLabelTopK[i][0];
                            if(truth == label_1) match = 1;
                            text.sprintf("%s %d %d %d \"%s\" \"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                         label_1, truth, match,
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                        }
                        else if(state->topKValue == 2){
                            int label_1 = state->resultImageLabelTopK[i][0];
                            int label_2 = state->resultImageLabelTopK[i][1];
                            if(truth == label_1) match = 1;
                            else if(truth == label_2) match = 2;
                            text.sprintf("%s %d %d %d %d \"%s\" \"%s\" \"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                         label_1, label_2, truth, match,
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                        }
                        else if(state->topKValue == 3){
                            int label_1 = state->resultImageLabelTopK[i][0];
                            int label_2 = state->resultImageLabelTopK[i][1];
                            int label_3 = state->resultImageLabelTopK[i][2];
                            if(truth == label_1) match = 1;
                            else if(truth == label_2) match = 2;
                            else if(truth == label_3) match = 3;
                            text.sprintf("%s %d %d %d %d %d \"%s\" \"%s\" \"%s\" \"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                         label_1, label_2, label_3, truth, match,
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                        }
                        else if(state->topKValue == 4){
                            int label_1 = state->resultImageLabelTopK[i][0];
                            int label_2 = state->resultImageLabelTopK[i][1];
                            int label_3 = state->resultImageLabelTopK[i][2];
                            int label_4 = state->resultImageLabelTopK[i][3];
                            if(truth == label_1) match = 1;
                            else if(truth == label_2) match = 2;
                            else if(truth == label_3) match = 3;
                            else if(truth == label_4) match = 4;
                            text.sprintf("%s %d %d %d %d %d %d \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                         label_1, label_2, label_3, label_4, truth, match,
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                        }
                        else if(state->topKValue == 5){
                            int label_1 = state->resultImageLabelTopK[i][0];
                            int label_2 = state->resultImageLabelTopK[i][1];
                            int label_3 = state->resultImageLabelTopK[i][2];
                            int label_4 = state->resultImageLabelTopK[i][3];
                            int label_5 = state->resultImageLabelTopK[i][3];
                            if(truth == label_1) match = 1;
                            else if(truth == label_2) match = 2;
                            else if(truth == label_3) match = 3;
                            else if(truth == label_4) match = 4;
                            else if(truth == label_5) match = 5;
                            text.sprintf("%s %d %d %d %d %d %d %d \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", state->imageDataFilenames[i].toStdString().c_str(),
                                         label_1, label_2, label_3, label_4, label_5, truth, match,
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]].toStdString().c_str() : "Unknown",
                                         state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                        }
                    }
                    else {
                        text.sprintf("%s %d #%s\n", state->imageDataFilenames[i].toStdString().c_str(), label,
                                     state->dataLabels ? (*state->dataLabels)[label].toStdString().c_str() : "Unknown");
                    }
                }
                fileObj.write(text.toStdString().c_str());
            }
            if(state->topKValue > 0 && csvFile){
                QString text;
                int netSummaryImages =  state->imageDataSize - noTruthTable;
                text.sprintf("\n\n\n***********SUMMARY***********\n");
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Images without ground Truth,%d\n",noTruthTable);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Images with ground Truth,%d\n",netSummaryImages);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Total image set for inference,%d\n",state->imageDataSize);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\nTotal Top K match,%d\n",(top1+top2+top3+top4+top5));
                fileObj.write(text.toStdString().c_str());
                float accuracyPer = ((top1+top2+top3+top4+top5)/netSummaryImages)*100;
                text.sprintf("Inference Accuracy on Top K %,%.2f\n",accuracyPer);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Total mismatch,%d\n",mismatchAll);
                fileObj.write(text.toStdString().c_str());
                accuracyPer = ((mismatchAll)/netSummaryImages)*100;
                text.sprintf("Inference mismatch %,%.2f\n",accuracyPer);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\n*****Top1*****\n");
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Top1 matches,%d\n",top1);
                fileObj.write(text.toStdString().c_str());
                accuracyPer = (top1/netSummaryImages)*100;
                text.sprintf("Top1 match %,%.2f\n",accuracyPer);
                fileObj.write(text.toStdString().c_str());
                if(state->topKValue > 1){
                    text.sprintf("\n*****Top2*****\n");
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("Top2 matches,%d\n",top2);
                    fileObj.write(text.toStdString().c_str());
                    accuracyPer = (top2/netSummaryImages)*100;
                    text.sprintf("Top2 match %,%.2f\n",accuracyPer);
                    fileObj.write(text.toStdString().c_str());
                }
                if(state->topKValue > 2){
                    text.sprintf("\n*****Top3*****\n");
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("Top3 matches,%d\n",top3);
                    fileObj.write(text.toStdString().c_str());
                    accuracyPer = (top3/netSummaryImages)*100;
                    text.sprintf("Top3 match %,%.2f\n",accuracyPer);
                    fileObj.write(text.toStdString().c_str());
                }
                if(state->topKValue > 3){
                    text.sprintf("\n*****Top4*****\n");
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("Top4 matches,%d\n",top4);
                    fileObj.write(text.toStdString().c_str());
                    accuracyPer = (top4/netSummaryImages)*100;
                    text.sprintf("Top4 match %,%.2f\n",accuracyPer);
                    fileObj.write(text.toStdString().c_str());
                }
                if(state->topKValue > 4){
                    text.sprintf("\n*****Top5*****\n");
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("Top5 matches,%d\n",top5);
                    fileObj.write(text.toStdString().c_str());
                    accuracyPer = (top5/netSummaryImages)*100;
                    text.sprintf("Top5 match %,%.2f\n",accuracyPer);
                    fileObj.write(text.toStdString().c_str());
                }
            }
            fileObj.close();
            QDesktopServices::openUrl(QUrl("file://" + fileName));
        }
        else {
            fatalError.sprintf("ERROR: unable to create: %s", fileName.toStdString().c_str());
        }
    }
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
        // check save button
        state->saveButtonPressed = false;
        if((x >= state->saveButtonRect.x()) &&
           (x < (state->saveButtonRect.x() + state->saveButtonRect.width())) &&
           (y >= state->saveButtonRect.y()) &&
           (y < (state->saveButtonRect.y() + state->saveButtonRect.height())))
        {
            state->saveButtonPressed = true;
        }
    }
}

void inference_viewer::mouseReleaseEvent(QMouseEvent * event)
{
    if(event->button() == Qt::LeftButton) {
        // check for exit and save buttons
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
                terminate();
            }
        }
        else if((x >= state->saveButtonRect.x()) &&
                (x < (state->saveButtonRect.x() + state->saveButtonRect.width())) &&
                (y >= state->saveButtonRect.y()) &&
                (y < (state->saveButtonRect.y() + state->saveButtonRect.height())))
        {
            saveResults();
        }
        state->exitButtonPressed = false;
        state->saveButtonPressed = false;
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
        state->abortLoadingRequested = true;
        state->mouseSelectedImage = -1;
        state->viewRecentResults = false;
        fatalError = "";
    }
    else if(event->key() == Qt::Key_Space) {
        state->viewRecentResults = !state->viewRecentResults;
    }
    else if(event->key() == Qt::Key_Q) {
        terminate();
    }
    else if(event->key() == Qt::Key_A) {
        if(progress.completed_decode && state->receiver_worker) {
                state->receiver_worker->abort();
        }
    }
    else if(event->key() == Qt::Key_S) {
        saveResults();
    }
}

void inference_viewer::paintEvent(QPaintEvent *)
{
    // configuration
    int imageCountLimitForInferenceStart = std::min(state->imageDataSize, state->imageDataStartOffset);
    const int loadBatchSize = 60;
    const int scaleBatchSize = 30;

    // calculate window layout
    QString exitButtonText = " Close ";
    QString saveButtonText = " Save ";
    QFontMetrics fontMetrics(state->statusBarFont);
    int buttonTextWidth = fontMetrics.width(exitButtonText);
    int statusBarX = 8 + 2*(10 + buttonTextWidth + 10 + 8), statusBarY = 8;
    int statusBarWidth = width() - 8 - statusBarX;
    int statusBarHeight = fontMetrics.height() + 16;
    int imageX = 8;
    int imageY = statusBarY + statusBarHeight + 8;
    state->saveButtonRect = QRect(8, statusBarY, 10 + buttonTextWidth + 10, statusBarHeight);
    state->exitButtonRect = QRect(8 + (10 + buttonTextWidth + 10 + 8), statusBarY, 10 + buttonTextWidth + 10, statusBarHeight);
    state->statusBarRect = QRect(statusBarX, statusBarY, statusBarWidth, statusBarHeight);
    QColor statusBarColorBackground(192, 192, 192);
    QColor statusBarColorLoad(255, 192, 64);
    QColor statusBarColorDecode(128, 192, 255);
    QColor statusBarColorQueue(128, 255, 128);
    QColor statusTextColor(0, 0, 128);
    QColor buttonBorderColor(128, 16, 32);
    QColor buttonBrushColor(150, 150, 175);
    QColor buttonBrushColorPressed(192, 175, 175);
    QColor buttonTextColor(128, 0, 0);

    // status bar info
    QString statusText;

    // message from initialization
    if(progress.message.length() > 0) {
        statusText = "[";
        statusText += progress.message;
        statusText += "] ";
    }

    if(!state->labelLoadDone) {
        if(state->dataFilename.length() == 0) {
            int count = 0;
            QDirIterator dirIt(state->dataFolder, QDirIterator::NoIteratorFlags);
            while (dirIt.hasNext()) {
                dirIt.next();
                QFileInfo fileInfo = QFileInfo(dirIt.filePath());
                if (fileInfo.isFile() &&
                        (QString::compare(fileInfo.suffix(), "jpeg", Qt::CaseInsensitive) ||
                         QString::compare(fileInfo.suffix(), "jpg", Qt::CaseInsensitive)  ||
                         QString::compare(fileInfo.suffix(), "png", Qt::CaseInsensitive)))
                {
                    state->imageDataFilenames.push_back(dirIt.filePath());
                    state->imageLabel.push_back(-1);
                    state->inferenceResultTop.push_back(-1);
                    state->inferenceResultSummary.push_back("Not available");
                    count++;
                    if(state->maxImageDataSize > 0 && count == state->maxImageDataSize)
                        break;
                }
            }
        }
        else {
            QFile fileObj(state->dataFilename);
            if(fileObj.open(QIODevice::ReadOnly)) {
                bool csvFile = QString::compare(QFileInfo(state->dataFilename).suffix(), "csv", Qt::CaseInsensitive) ? false : true;
                QTextStream fileInput(&fileObj);
                int count = 0;
                while (!fileInput.atEnd()) {
                    QString line = fileInput.readLine().trimmed();
                    if(line.size() == 0 || line[0] == '#')
                        continue;
                    QStringList words = line.split(csvFile ? "," : " ");
                    int label = -1;
                    if(words.size() < 1) {
                        fatalError.sprintf("ERROR: incorrectly formatted text at %s#%d: %s", state->dataFilename.toStdString().c_str(), state->imageDataFilenames.size()+1, line.toStdString().c_str());
                        break;
                    }
                    if(words.size() > 1) {
                        label = words[1].toInt();
                    }
                    state->imageDataFilenames.push_back(words[0].trimmed());
                    state->imageLabel.push_back(label);
                    state->inferenceResultTop.push_back(-1);
                    state->inferenceResultSummary.push_back("Not available");
                    count++;
                    if(state->maxImageDataSize > 0 && count == state->maxImageDataSize)
                        break;
                }
            }
            else {
                fatalError.sprintf("ERROR: unable to open: %s", state->dataFilename.toStdString().c_str());
            }
        }
        state->imageDataSize = state->imageDataFilenames.size();
        state->labelLoadDone = true;
        if(state->imageDataSize == 0) {
            fatalError.sprintf("ERROR: no image files detected");
        }
    }
    if(!state->imageLoadDone) {
        for(int i = 0; i < loadBatchSize; i++) {
            if(state->imageLoadCount == state->imageDataSize || state->abortLoadingRequested) {
                state->imageDataSize = state->imageLoadCount;
                state->imageLoadDone = true;
                progress.images_loaded = state->imageLoadCount;
                progress.completed_load = true;
                break;
            }
            QString fileName = state->imageDataFilenames[state->imageLoadCount];
            if(fileName[0] != '/') {
                fileName = state->dataFolder + "/" + fileName;
            }
            QFile fileObj(fileName);
            if(!fileObj.open(QIODevice::ReadOnly)) {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::critical(this, windowTitle(), "Do you want to continue?\n\nERROR: Unable to open: " + fileName, QMessageBox::Yes|QMessageBox::No);
                if(reply == QMessageBox::No) {
                    exit(1);
                    return;
                }
                state->imageDataSize = state->imageLoadCount;
                state->imageLoadDone = true;
                progress.images_loaded = state->imageLoadCount;
                progress.completed_load = true;
                break;
            }
            QByteArray byteArray = fileObj.readAll();
            state->imageBuffer.push_back(byteArray);
            state->imageLoadCount++;
            progress.images_loaded = state->imageLoadCount;
        }
    }
    if(state->imageLoadCount > 0 && !state->imagePixmapDone) {
        for(int i = 0; i < scaleBatchSize; i++) {
            if(state->imageLoadDone && state->imagePixmapCount == state->imageLoadCount) {
                state->imagePixmapDone = true;
                progress.images_decoded = state->imagePixmapCount;
                progress.completed_decode = true;
                break;
            }
            if(state->imagePixmapCount >= state->imageBuffer.size()) {
                break;
            }
            QPixmap pixmap;
            if(!pixmap.loadFromData(state->imageBuffer[state->imagePixmapCount])) {
                state->imagePixmapDone = true;
                progress.images_decoded = state->imagePixmapCount;
                progress.completed_decode = true;
                break;
            }
            if(state->sendScaledImages) {
                QBuffer buffer(&state->imageBuffer[state->imagePixmapCount]);
                auto scaled = pixmap.scaled(state->inputDim[0], state->inputDim[1], Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                scaled.save(&buffer, "PNG");
            }
            state->imagePixmap.push_back(pixmap.scaled(ICON_SIZE, ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            state->imagePixmapCount++;
            progress.images_decoded = state->imagePixmapCount;
        }
        if(state->receiver_worker)
            state->receiver_worker->setImageCount(state->imagePixmapCount, state->dataLabels ? state->dataLabels->size() : 0, state->dataLabels);
    }

    if(!state->receiver_worker && state->imagePixmapCount >= imageCountLimitForInferenceStart && state->imageDataSize > 0) {
        startReceiver();
        state->receiver_worker->setImageCount(state->imagePixmapCount, state->dataLabels ? state->dataLabels->size() : 0, state->dataLabels);
    }

    // initialize painter object
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // draw status bar
    int subBarHeight = statusBarHeight * 70 / 100;
    float progressLoad = (float) state->imageLoadCount / std::max(1, state->imageDataSize);
    float progressDecode = (float) state->imagePixmapCount / std::max(1, state->imageLoadCount);
    if(state->imagePixmapDone) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(statusBarColorDecode);
        painter.drawRect(state->statusBarRect);
        if(progress.repeat_images) {
            QString text;
            text.sprintf("Cycling through %d images from the image list [processed %d images]", state->imagePixmapCount, progress.images_received);
            statusText += text;
        }
        else if (progress.completed) {
            if(progress.errorCode) {
                QString text;
                text.sprintf("Completed: %d/%d images have been processed [error %d]", progress.images_received, state->imagePixmapCount, progress.errorCode);
                statusText += text;
            }
            else {
                QString text;
                text.sprintf("Completed: %d/%d images have been processed", progress.images_received, state->imagePixmapCount);
                statusText += text;
            }
            if(!timerStopped && updateTimer) {
                // TODO: something is wrong with timer and paint event triggers
                timerStopped = true;
                updateTimer->setInterval(1000);
            }
        }
        else {
            QString text;
            text.sprintf("Processing: [scheduled %d/%d images] [processed %d/%d images]",
                               progress.images_sent, state->imagePixmapCount,
                               progress.images_received, state->imagePixmapCount);
            statusText += text;
        }
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
            text.sprintf(" loaded[%d/%d]", state->imageLoadCount, state->imageDataSize);
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
        if(!progress.repeat_images) {
            QString text;
            if(progress.images_sent > 0) {
                text.sprintf(" sent[%d/%d]", progress.images_sent, state->imagePixmapCount);
                statusText += text;
            }
            if(progress.images_received > 0) {
                text.sprintf(" inferred[%d/%d]", progress.images_received, state->imagePixmapCount);
                statusText += text;
            }
        }
    }

    // get image render list from receiver
    if(state->receiver_worker) {
        state->receiver_worker->getReceivedList(state->resultImageIndex, state->resultImageLabel, state->resultImageSummary,
                                                state->resultImageLabelTopK, state->resultImageProbTopK);
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

    // render exit and save buttons
    setFont(state->buttonFont);
    painter.setPen(buttonBorderColor);
    painter.setBrush(state->exitButtonPressed ? buttonBrushColorPressed : buttonBrushColor);
    painter.drawRoundedRect(state->exitButtonRect, 4, 4);
    painter.drawRoundedRect(state->saveButtonRect, 4, 4);
    painter.setPen(buttonTextColor);
    painter.drawText(state->exitButtonRect, Qt::AlignCenter, exitButtonText);
    painter.drawText(state->saveButtonRect, Qt::AlignCenter, saveButtonText);

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
            bool enableZoomInEffect = true;
            if(!progress.repeat_images)
                enableZoomInEffect = false;
            else if(imageCount < (numCols * numRows * 3 / 4))
                enableZoomInEffect = false;
            if(enableZoomInEffect && (row / 2) < (numRows / 4)) {
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
                if(state->imageLabel[index] >= 0) {
                    int cx = x + w, cy = y + h;
                    if(state->inferenceResultTop[index] == state->imageLabel[index]) {
                        painter.setPen(Qt::green);
                        painter.setBrush(Qt::darkGreen);
                        QPoint points[] = {
                            { cx - 10, cy -  4 },
                            { cx -  8, cy -  6 },
                            { cx -  6, cy -  4 },
                            { cx -  4, cy - 10 },
                            { cx -  2, cy -  8 },
                            { cx -  5, cy -  2 },
                        };
                        painter.drawConvexPolygon(points, sizeof(points)/sizeof(points[0]));
                        painter.setPen(Qt::darkGreen);
                    }
                    else {
                        painter.setPen(Qt::red);
                        painter.setBrush(Qt::white);
                        QPoint points[] = {
                            { cx - 10, cy -  9 },
                            { cx -  9, cy - 10 },
                            { cx -  6, cy -  7 },
                            { cx -  3, cy - 10 },
                            { cx -  2, cy -  9 },
                            { cx -  5, cy -  6 },
                            { cx -  2, cy -  3 },
                            { cx -  3, cy -  2 },
                            { cx -  6, cy -  5 },
                            { cx -  9, cy -  2 },
                            { cx - 10, cy -  3 },
                            { cx -  7, cy -  6 },
                        };
                        painter.drawConvexPolygon(points, sizeof(points)/sizeof(points[0]));
                        painter.setPen(Qt::red);
                    }
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(x, y, w, h);
                }
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
    QFont font = state->statusBarFont;
    font.setBold(true);
    font.setItalic(false);
    if(state->mouseSelectedImage >= 0) {
        int index = state->mouseSelectedImage;
        int truthLabel = state->imageLabel[index];
        QString truthSummary = "";
        if(truthLabel >= 0) {
            truthSummary = state->dataLabels ? (*state->dataLabels)[truthLabel] : "Unknown";
        }
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
        if(truthLabel >= 0) {
            int cx = x + 4 + ICON_SIZE * 2 + 16;
            int cy = y + 4 + ICON_SIZE     + 12;
            if(truthLabel == resultLabel) {
                painter.setPen(Qt::darkGreen);
                painter.setBrush(Qt::green);
                QPoint points[] = {
                    { cx - 10, cy -  4 },
                    { cx -  8, cy -  6 },
                    { cx -  6, cy -  4 },
                    { cx -  4, cy - 10 },
                    { cx -  2, cy -  8 },
                    { cx -  5, cy -  2 },
                };
                painter.drawConvexPolygon(points, sizeof(points)/sizeof(points[0]));
                setFont(font);
                painter.setPen(Qt::darkGreen);
                painter.drawText(QRect(cx, cy - 6 - fontMetrics.height() / 2, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, "MATCHED");
                painter.setPen(Qt::darkGreen);
            }
            else {
                painter.setPen(Qt::red);
                painter.setBrush(Qt::white);
                QPoint points[] = {
                    { cx - 10, cy -  9 },
                    { cx -  9, cy - 10 },
                    { cx -  6, cy -  7 },
                    { cx -  3, cy - 10 },
                    { cx -  2, cy -  9 },
                    { cx -  5, cy -  6 },
                    { cx -  2, cy -  3 },
                    { cx -  3, cy -  2 },
                    { cx -  6, cy -  5 },
                    { cx -  9, cy -  2 },
                    { cx - 10, cy -  3 },
                    { cx -  7, cy -  6 },
                };
                painter.drawConvexPolygon(points, sizeof(points)/sizeof(points[0]));
                setFont(font);
                painter.setPen(Qt::darkRed);
                painter.drawText(QRect(cx, cy - 6 - fontMetrics.height() / 2, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, "MISMATCHED");
                painter.setPen(Qt::red);
            }
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(x + 4, y + 4, ICON_SIZE * 2, ICON_SIZE * 2);
        }
        painter.setPen(statusTextColor);
        painter.setBrush(Qt::white);
        QString text;
        setFont(font);
        painter.setPen(Qt::black);
        text.sprintf("IMAGE#%d", index + 1);
        painter.drawText(QRect(x + 4 + ICON_SIZE * 2 + 4, y + 4, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, text);
        font.setBold(false);
        font.setItalic(false);
        setFont(font);
        painter.setPen(Qt::blue);
        text.sprintf("classified as [label=%d] ", resultLabel);
        text += resultSummary;
        painter.drawText(QRect(x + 4 + ICON_SIZE * 2 + 4, y + 4 + fontMetrics.height() + 8, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, text);
        if(truthLabel >= 0) {
            font.setItalic(true);
            setFont(font);
            painter.setPen(Qt::gray);
            text.sprintf("ground truth: [label=%d] ", truthLabel);
            text += truthSummary;
            painter.drawText(QRect(x + 4, y + 4 + ICON_SIZE * 2 + 4, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, text);
        }
        else {
            font.setItalic(true);
            setFont(font);
            painter.setPen(Qt::gray);
            text.sprintf("ground truth: not available");
            text += truthSummary;
            painter.drawText(QRect(x + 4, y + 4 + ICON_SIZE * 2 + 4, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, text);
        }
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
