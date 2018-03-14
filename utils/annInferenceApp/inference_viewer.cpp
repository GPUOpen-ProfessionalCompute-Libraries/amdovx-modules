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
#include <sstream>

#define WINDOW_TITLE             "Inference Viewer"
#define ICON_SIZE                64
#define ICON_STRIDE              (ICON_SIZE + 8)
#define INFCOM_RUNTIME_OPTIONS   ""

inference_state::inference_state()
{
    // initialize
    dataLabels = nullptr;
    dataHierarchy = nullptr;
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
        QVector<QString> * dataLabels, QVector<QString> * dataHierarchy, QString dataFilename, QString dataFolder,
        int dimInput[3], int GPUs, int dimOutput[3], int maxImageDataSize,
        bool repeat_images, bool sendScaledImages, int enableSF, int topKValue,
        QWidget *parent) :
    QWidget(parent),
    ui(new Ui::inference_viewer),
    updateTimer{ nullptr }
{
    state = new inference_state();
    state->dataLabels = dataLabels;
    state->dataHierarchy = dataHierarchy;
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
                            fileObj.write("#FileName,outputLabel-1,groundTruthLabel,Matched?,outputLabelText-1,groundTruthLabelText,Prob-1\n");
                        else if(state->topKValue == 2)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,groundTruthLabel,Matched?,outputLabelText-1,outputLabelText-2,groundTruthLabelText,Prob-1,Prob-2\n");
                        else if(state->topKValue == 3)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,outputLabel-3,groundTruthLabel,Matched?,"
                                          "outputLabelText-1,outputLabelText-2,outputLabelText-3,groundTruthLabelText,Prob-1,Prob-2,Prob-3\n");
                        else if(state->topKValue == 4)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,outputLabel-3,outputLabel-4,groundTruthLabel,Matched?,"
                                          "outputLabelText-1,outputLabelText-2,outputLabelText-3,outputLabelText-4,groundTruthLabelText,Prob-1,Prob-2,Prob-3,Prob-4\n");
                        else if(state->topKValue == 5)
                            fileObj.write("#FileName,outputLabel-1,outputLabel-2,outputLabel-3,outputLabel-4,outputLabel-5,groundTruthLabel,Matched?,"
                                          "outputLabelText-1,outputLabelText-2,outputLabelText-3,outputLabelText-4,outputLabelText-5,groundTruthLabelText,Prob-1,Prob-2,Prob-3,Prob-4,Prob-5\n");
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
            if(state->topKValue > 0){
                state->top1Count = state->top2Count = state->top3Count = state->top4Count = state->top5Count = 0;
                state->totalNoGroundTruth = state->totalMismatch = 0;
                state->top1TotProb = state->top2TotProb = state->top3TotProb = state->top4TotProb = state->top5TotProb = state->totalFailProb = 0;
                for(int j = 0; j < 100; j++){
                    state->topKPassFail[j][0] = state->topKPassFail[j][1] = 0;
                    for(int k = 0; k < 12; k++) state->topKHierarchyPassFail[j][k] = 0;
                }
                for(int j = 0; j < 1000; j++){
                    for(int k = 0; k < 6; k++) state->topLabelMatch[j][k] = 0;
                }
            }
            for(int i = 0; i < state->imageDataSize; i++) {
                int label = state->inferenceResultTop[i];
                int truth = state->imageLabel[i];
                float prob_1 = 0;
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
                                prob_1 = state->resultImageProbTopK[i][0];
                                if(truth == label_1) { match = 1; state->top1Count++; state->top1TotProb += prob_1; }
                                else { state->totalMismatch++; state->totalFailProb += prob_1; }
                                text.sprintf("%s,%d,%d,%d,\"%s\",\"%s\",%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown",
                                             prob_1);
                            }
                            else if(state->topKValue == 2){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                if(truth == label_1) { match = 1; state->top1Count++; state->top1TotProb += prob_1; }
                                else if(truth == label_2) { match = 2; state->top2Count++; state->top2TotProb += prob_2;  }
                                else { state->totalMismatch++; state->totalFailProb += prob_1; }
                                text.sprintf("%s,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2);
                            }
                            else if(state->topKValue == 3){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                float prob_3 = state->resultImageProbTopK[i][2];
                                if(truth == label_1) { match = 1; state->top1Count++; state->top1TotProb += prob_1; }
                                else if(truth == label_2) { match = 2; state->top2Count++; state->top2TotProb += prob_2; }
                                else if(truth == label_3) { match = 3; state->top3Count++; state->top3TotProb += prob_3; }
                                else { state->totalMismatch++; state->totalFailProb += prob_1; }
                                text.sprintf("%s,%d,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",%.4f,%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2,prob_3);
                            }
                            else if(state->topKValue == 4){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                float prob_3 = state->resultImageProbTopK[i][2];
                                float prob_4 = state->resultImageProbTopK[i][3];
                                if(truth == label_1) { match = 1; state->top1Count++; state->top1TotProb += prob_1; }
                                else if(truth == label_2) { match = 2; state->top2Count++; state->top2TotProb += prob_2; }
                                else if(truth == label_3) { match = 3; state->top3Count++; state->top3TotProb += prob_3; }
                                else if(truth == label_4) { match = 4; state->top4Count++; state->top4TotProb += prob_4; }
                                else { state->totalMismatch++; state->totalFailProb += prob_1; }
                                text.sprintf("%s,%d,%d,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%.4f,%.4f,%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2,prob_3,prob_4);
                            }
                            else if(state->topKValue == 5){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                int label_5 = state->resultImageLabelTopK[i][4];
                                prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                float prob_3 = state->resultImageProbTopK[i][2];
                                float prob_4 = state->resultImageProbTopK[i][3];
                                float prob_5 = state->resultImageProbTopK[i][4];

                                if(truth == label_1) {
                                    match = 1; state->top1Count++; state->top1TotProb += prob_1;
                                    state->topLabelMatch[truth][0]++; state->topLabelMatch[truth][1]++;
                                }
                                else if(truth == label_2) {
                                    match = 2; state->top2Count++; state->top2TotProb += prob_2;
                                    state->topLabelMatch[truth][0]++; state->topLabelMatch[truth][2]++;
                                }
                                else if(truth == label_3) {
                                    match = 3; state->top3Count++; state->top3TotProb += prob_3;
                                    state->topLabelMatch[truth][0]++; state->topLabelMatch[truth][3]++;
                                }
                                else if(truth == label_4) {
                                    match = 4; state->top4Count++; state->top4TotProb += prob_4;
                                    state->topLabelMatch[truth][0]++; state->topLabelMatch[truth][4]++;
                                }
                                else if(truth == label_5) {
                                    match = 5; state->top5Count++; state->top5TotProb += prob_5;
                                    state->topLabelMatch[truth][0]++; state->topLabelMatch[truth][5]++;
                                }
                                else {
                                    state->totalMismatch++; state->totalFailProb += prob_1;
                                    state->topLabelMatch[truth][0]++;
                                }
                                text.sprintf("%s,%d,%d,%d,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%.4f,%.4f,%.4f,%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4, label_5, truth, match,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2,prob_3,prob_4,prob_5);
                            }

                            if(truth == label) {
                                int count = 0;
                                for(float f = 0;f < 1; f=f+0.01){
                                    if((prob_1 < (f + 0.01)) && prob_1 > f){
                                        state->topKPassFail[count][0]++;
                                        if(state->dataHierarchy->size()){
                                            state->topKHierarchyPassFail[count][0]++;
                                            state->topKHierarchyPassFail[count][2]++;
                                            state->topKHierarchyPassFail[count][4]++;
                                            state->topKHierarchyPassFail[count][6]++;
                                            state->topKHierarchyPassFail[count][8]++;
                                            state->topKHierarchyPassFail[count][10]++;
                                        }
                                    }
                                    count++;
                                }
                            }
                            else{
                                int count = 0;
                                for(float f = 0;f < 1; f=f+0.01){
                                    if((prob_1 < (f + 0.01)) && prob_1 > f){
                                        state->topKPassFail[count][1]++;
                                        if(state->dataHierarchy->size()){
                                            int catCount = 0;
                                            QString truthHierarchy = state->dataHierarchy ? (*state->dataHierarchy)[truth] : "Unknown";
                                            QString resultHierarchy = state->dataHierarchy ? (*state->dataHierarchy)[label] : "Unknown";
                                            std::string input_result = resultHierarchy.toStdString().c_str();
                                            std::string input_truth = truthHierarchy.toStdString().c_str();
                                            std::istringstream ss_result(input_result);
                                            std::istringstream ss_truth(input_truth);
                                            std::string token_result, token_truth;
                                            int previousTruth = 0;
                                            while(std::getline(ss_result, token_result, ',') && std::getline(ss_truth, token_truth, ',')) {
                                                if(token_truth.size() && (token_truth == token_result)){
                                                    state->topKHierarchyPassFail[count][catCount*2]++;
                                                    previousTruth = 1;
                                                }
                                                else if( previousTruth == 1 && (!token_result.size() && !token_truth.size())){
                                                    state->topKHierarchyPassFail[count][catCount*2]++;
                                                }
                                                else{
                                                    state->topKHierarchyPassFail[count][catCount*2 + 1]++;
                                                    previousTruth = 0;
                                                }
                                                catCount++;
                                            }
                                        }
                                    }
                                    count++;
                                }
                            }
                        }
                        else {
                            if(state->topKValue == 1){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                float prob_1 = state->resultImageProbTopK[i][0];
                                text.sprintf("%s,%d,-1,-1,\"%s\",Unknown,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             prob_1);
                            }
                            else if(state->topKValue == 2){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                float prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                text.sprintf("%s,%d,%d,-1,-1,\"%s\",\"%s\",Unknown,%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2);
                            }
                            else if(state->topKValue == 3){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                float prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                float prob_3 = state->resultImageProbTopK[i][2];
                                text.sprintf("%s,%d,%d,%d,-1,-1,\"%s\",\"%s\",\"%s\",Unknown,%.4f,%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2,prob_3);
                            }
                            else if(state->topKValue == 4){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                float prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                float prob_3 = state->resultImageProbTopK[i][2];
                                float prob_4 = state->resultImageProbTopK[i][3];
                                text.sprintf("%s,%d,%d,%d,%d,-1,-1,\"%s\",\"%s\",\"%s\",\"%s\",Unknown,%.4f,%.4f,%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2,prob_3,prob_4);
                            }
                            else if(state->topKValue == 5){
                                int label_1 = state->resultImageLabelTopK[i][0];
                                int label_2 = state->resultImageLabelTopK[i][1];
                                int label_3 = state->resultImageLabelTopK[i][2];
                                int label_4 = state->resultImageLabelTopK[i][3];
                                int label_5 = state->resultImageLabelTopK[i][4];
                                float prob_1 = state->resultImageProbTopK[i][0];
                                float prob_2 = state->resultImageProbTopK[i][1];
                                float prob_3 = state->resultImageProbTopK[i][2];
                                float prob_4 = state->resultImageProbTopK[i][3];
                                float prob_5 = state->resultImageProbTopK[i][4];
                                text.sprintf("%s,%d,%d,%d,%d,%d,-1,-1,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",Unknown,%.4f,%.4f,%.4f,%.4f,%.4f\n", state->imageDataFilenames[i].toStdString().c_str(),
                                             label_1, label_2, label_3, label_4, label_5,
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown",
                                             state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]].toStdString().c_str() : "Unknown",
                                             prob_1,prob_2,prob_3,prob_4,prob_5);
                            }
                            state->totalNoGroundTruth++;
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
            fileObj.close();
            if(state->topKValue > 0 && csvFile){
                saveSummary(fileName);
                // experimental HTML output
                if(state->topKValue == 5){
                    QString htmlFileName = fileName;
                    htmlFileName.replace(".csv",".html");
                    saveHTML(htmlFileName, true);
                    //QDesktopServices::openUrl(QUrl("file://" + htmlFileName));
                }
            }
            QDesktopServices::openUrl(QUrl("file://" + fileName));
        }
        else {
            fatalError.sprintf("ERROR: unable to create: %s", fileName.toStdString().c_str());
        }
    }
}

void inference_viewer::saveSummary(QString fileName)
{
    if(fileName.size() > 0) {
        QFile fileObj(fileName);
        if(fileObj.open(QIODevice::Append)) {
            fileObj.write("\n\n ***************** INFERENCE SUMMARY ***************** \n\n");

            QString text;
            int netSummaryImages =  state->imageDataSize - state->totalNoGroundTruth;
            float passProb = (state->top1TotProb+state->top2TotProb+state->top3TotProb+state->top4TotProb+state->top5TotProb);
            int passCount = (state->top1Count+state->top2Count+state->top3Count+state->top4Count+state->top5Count);
            float avgPassProb = passProb/passCount;

            text.sprintf("Images without ground Truth,%d\n",state->totalNoGroundTruth);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("Images with ground Truth,%d\n",netSummaryImages);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("Total image set for inference,%d\n",state->imageDataSize);
            fileObj.write(text.toStdString().c_str());

            text.sprintf("\nTotal Top K match,%d\n",passCount);
            fileObj.write(text.toStdString().c_str());
            float accuracyPer = ((float)passCount / netSummaryImages);
            text.sprintf("Inference Accuracy on Top K, %.2f\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("Average Pass Probability for Top K, %.2f\n\n",avgPassProb);
            fileObj.write(text.toStdString().c_str());

            text.sprintf("Total mismatch,%d\n",state->totalMismatch);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->totalMismatch/netSummaryImages);
            text.sprintf("Inference mismatch Percentage, %.2f\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("Average mismatch Probability for Top 1, %.4f\n",state->totalFailProb/state->totalMismatch);
            fileObj.write(text.toStdString().c_str());

            text.sprintf("\n*****Top1*****\n");
            fileObj.write(text.toStdString().c_str());
            text.sprintf("Top1 matches,%d\n",state->top1Count);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top1Count/netSummaryImages);
            text.sprintf("Top1 match Percentage, %.2f\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            if(state->top1Count){
                text.sprintf("Avg Top1 pass prob, %.4f\n",state->top1TotProb/state->top1Count);
                fileObj.write(text.toStdString().c_str());
            }
            if(state->topKValue > 1){
                text.sprintf("\n*****Top2*****\n");
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Top2 matches,%d\n",state->top2Count);
                fileObj.write(text.toStdString().c_str());
                accuracyPer = ((float)state->top2Count/netSummaryImages);
                text.sprintf("Top2 match Percentage,%.2f\n",(accuracyPer*100));
                fileObj.write(text.toStdString().c_str());
                if(state->top2Count){
                    text.sprintf("Avg Top2 pass prob, %.4f\n",state->top2TotProb/state->top2Count);
                    fileObj.write(text.toStdString().c_str());
                }
            }
            if(state->topKValue > 2){
                text.sprintf("\n*****Top3*****\n");
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Top3 matches,%d\n",state->top3Count);
                fileObj.write(text.toStdString().c_str());
                accuracyPer = ((float)state->top3Count/netSummaryImages);
                text.sprintf("Top3 match Percentage, %.2f\n",(accuracyPer*100));
                fileObj.write(text.toStdString().c_str());
                if(state->top3Count){
                    text.sprintf("Avg Top3 pass prob, %.4f\n",state->top3TotProb/state->top3Count);
                    fileObj.write(text.toStdString().c_str());
                }
            }
            if(state->topKValue > 3){
                text.sprintf("\n*****Top4*****\n");
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Top4 matches,%d\n",state->top4Count);
                fileObj.write(text.toStdString().c_str());
                accuracyPer = ((float)state->top4Count/netSummaryImages);
                text.sprintf("Top4 match Percentage, %.2f\n",(accuracyPer*100));
                fileObj.write(text.toStdString().c_str());
                if(state->top4Count){
                    text.sprintf("Avg Top4 pass prob, %.4f\n",state->top4TotProb/state->top4Count);
                    fileObj.write(text.toStdString().c_str());
                }
            }
            if(state->topKValue > 4){
                text.sprintf("\n*****Top5*****\n");
                fileObj.write(text.toStdString().c_str());
                text.sprintf("Top5 matches,%d\n",state->top5Count);
                fileObj.write(text.toStdString().c_str());
                accuracyPer = ((float)state->top5Count/netSummaryImages);
                text.sprintf("Top5 match Percentage, %.2f\n",(accuracyPer*100));
                fileObj.write(text.toStdString().c_str());
                if(state->top5Count){
                    text.sprintf("Avg Top5 pass prob, %.4f\n",state->top5TotProb/state->top5Count);
                    fileObj.write(text.toStdString().c_str());
                }
            }
            float f=0.99;
            fileObj.write("\n********Pass/Fail in Probability Range********\n");
            fileObj.write("\nProbability,Pass,Fail,,cat-1 pass,cat-1 fail,,cat-2 pass, cat-2 fail,,cat-3 pass,cat-3 fail,,cat-4 pass,cat-4 fail,,cat-5 pass,cat-5 fail,,cat-6 pass,cat-6 fail\n");
            for(int i = 99; i >= 0; i--){
                text.sprintf("%.2f,%d,%d,,%d,%d,,%d,%d,,%d,%d,,%d,%d,,%d,%d,,%d,%d\n",f,state->topKPassFail[i][0],state->topKPassFail[i][1],
                        state->topKHierarchyPassFail[i][0],state->topKHierarchyPassFail[i][1],
                        state->topKHierarchyPassFail[i][2],state->topKHierarchyPassFail[i][3],
                        state->topKHierarchyPassFail[i][4],state->topKHierarchyPassFail[i][5],
                        state->topKHierarchyPassFail[i][6],state->topKHierarchyPassFail[i][7],
                        state->topKHierarchyPassFail[i][8],state->topKHierarchyPassFail[i][9],
                        state->topKHierarchyPassFail[i][10],state->topKHierarchyPassFail[i][11]
                        );
                fileObj.write(text.toStdString().c_str());
                f=f-0.01;
               }
            if(state->topKValue > 4){
            fileObj.write("\n******** Labels Count ********\n");
            fileObj.write("\nLabel,Images in DataBase, Matched with Top1, Matched with Top2, Matched with Top3, Matched with Top4, Matched with Top5,Label Description\n");
            for(int i = 0; i < 1000; i++){
                text.sprintf("%d,%d,%d,%d,%d,%d,%d,\"%s\"\n",i, state->topLabelMatch[i][0],
                        state->topLabelMatch[i][1],
                        state->topLabelMatch[i][2],
                        state->topLabelMatch[i][3],
                        state->topLabelMatch[i][4],
                        state->topLabelMatch[i][5],
                        state->dataLabels ? (*state->dataLabels)[i].toStdString().c_str() : "Unknown"
                        );
                fileObj.write(text.toStdString().c_str());
               }
            }
            fileObj.close();
        }
    }
}

void inference_viewer::saveHTML(QString fileName, bool exportTool)
{
    if(exportTool){
        QString FolderName = fileName;
        FolderName.replace(".html","-ToolKit");
        struct stat st = {};
        if (stat(FolderName.toStdString().c_str(), &st) == -1) {
            mkdir(FolderName.toStdString().c_str(), 0700);
            QString ImageFolderName = FolderName; ImageFolderName +="/images";
            QString IconFolderName = FolderName; IconFolderName +="/icons";
            std::experimental::filesystem::copy(state->dataFolder.toStdString().c_str(), ImageFolderName.toStdString().c_str(), std::experimental::filesystem::copy_options::recursive);
            std::experimental::filesystem::copy("./../annInferenceApp/images", IconFolderName.toStdString().c_str(), std::experimental::filesystem::copy_options::recursive);
        }
        FolderName += "/index.html";
        fileName = FolderName;
    }
    if(fileName.size() > 0) {
        QFile fileObj(fileName);
        if(fileObj.open(QIODevice::WriteOnly)) {
            fileObj.write("<!DOCTYPE HTML PUBLIC \" -//W3C//DTD HTML 4.0 Transitional//EN\">\n");
            fileObj.write("\n<html>\n");
            fileObj.write("<head>\n");
            fileObj.write("\n\t<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\"/>\n");
            fileObj.write("\t<title>AMD Inference ToolKit</title>\n");
            if(exportTool){
                fileObj.write("\t<link rel=\"icon\" href=\"icons/vega_icon_150.png\"/>\n");
            }
            fileObj.write("\n\t<style type=\"text/css\">\n");
            fileObj.write("\t\n");
            fileObj.write("\tbody,div,table,thead,tbody,tfoot,tr,th,td,p { font-family:\"Liberation Sans\"; font-size:x-small }\n");
            fileObj.write("\ta.comment-indicator:hover + comment { background:#ffd; position:absolute; display:block; border:1px solid black; padding:0.5em;  }\n");
            fileObj.write("\ta.comment-indicator { background:red; display:inline-block; border:1px solid black; width:0.5em; height:0.5em;  }\n");
            fileObj.write("\tcomment { display:none;  } tr:nth-of-type(odd) { background-color:#f2f2f2;}\n");
            fileObj.write("\t\n");
            fileObj.write("\t#myImg { border-radius: 5px; cursor: pointer; transition: 0.3s; }\n");
            fileObj.write("\t#myImg:hover { opacity: 0.7; }\n");
            fileObj.write("\t.modal{ display: none; position: fixed; z-index: 1; padding-top: 100px; left: 0; top: 0;width: 100%;\n");
            fileObj.write("\t		height: 100%; overflow: auto; background-color: rgb(0,0,0); background-color: rgba(0,0,0,0.9); }\n");
            fileObj.write("\t.modal-content { margin: auto; display: block; width: 80%; max-width: 500px; }\n");
            fileObj.write("\t#caption { margin: auto; display: block; width: 80%; max-width: 700px; text-align: center; color: white;font-size: 18px; padding: 10px 0; height: 150px;}\n");
            fileObj.write("\t.modal-content, #caption {  -webkit-animation-name: zoom;  -webkit-animation-duration: 0.6s;\n");
            fileObj.write("\t							animation-name: zoom; animation-duration: 0.6s; }\n");
            fileObj.write("\t@-webkit-keyframes zoom {  from { -webkit-transform:scale(0) }  to { -webkit-transform:scale(1) }}\n");
            fileObj.write("\t@keyframes zoom {    from {transform:scale(0)}     to {transform:scale(1) }}\n");
            fileObj.write("\t.close { position: absolute; top: 15px; right: 35px; color: #f1f1f1; font-size: 40px; font-weight: bold; transition: 0.3s; }\n");
            fileObj.write("\t.close:hover,.close:focus { color: #bbb; text-decoration: none; cursor: pointer; }\n");
            fileObj.write("\t@media only screen and (max-width: 400px){ .modal-content {     width: 100%; } }\n");
            fileObj.write("\t\n");
            fileObj.write("\tbody { font-family: \"Lato\", sans-serif;}\n");
            fileObj.write("\t.sidenav { height: 100%; width: 0; position: fixed; z-index: 1; top: 0; left: 0; background-color: #111;\n");
            fileObj.write("\t\t overflow-x: hidden;    transition: 0.5s; padding-top: 60px;}\n");
            fileObj.write("\t.sidenav a { padding: 8px 8px 8px 32px; text-decoration: none; font-size: 25px; color: #818181; display: block; transition: 0.3s;}\n");
            fileObj.write("\t.sidenav a:hover { color: #f1f1f1;}\n");
            fileObj.write("\t.sidenav .closebtn {  position: absolute; top: 0; right: 25px; font-size: 36px; margin-left: 50px;}\n");
            fileObj.write("\t#main {  transition: margin-left .5s;  padding: 16px; }\n");
            fileObj.write("\t@media screen and (max-height: 450px) { .sidenav {padding-top: 15px;} .sidenav a {font-size: 18px;} }\n");
            fileObj.write("\t\n");
            fileObj.write("\tbody {margin:0;}\n");
            fileObj.write("\t.navbar {  overflow: hidden;  background-color: #333;  position: fixed;  top: 0;  width: 100%;}\n");
            fileObj.write("\t.navbar a {  float: left;  display: block;  color: #f2f2f2;  text-align: center;  padding: 14px 16px;  text-decoration: none;  font-size: 17px; }\n");
            fileObj.write("\t.navbar a:hover {  background: #ddd;  color: black;}\n");
            fileObj.write("\t.main {  padding: 16px;  margin-top: 30px; }\n");
            fileObj.write("\t\n");
            fileObj.write("\tselect {-webkit-appearance: none; -moz-appearance: none; text-indent: 0px; text-overflow: ''; color:maroon; }\n");
            fileObj.write("\t\n");
            fileObj.write("\t</style>\n");
            fileObj.write("\n</head>\n");
            fileObj.write("\n\n<body>\n");
            fileObj.write("\t\n");
            fileObj.write("\t<div id=\"myModal\" class=\"modal\"> <span class=\"close\">&times;</span>  <img class=\"modal-content\" id=\"img01\">  <div id=\"caption\"></div> </div>\n");
            fileObj.write("\t\n");
            fileObj.write("\t<div id=\"mySidenav\" class=\"sidenav\">\n");
            fileObj.write("\t<a href=\"javascript:void(0)\" class=\"closebtn\" onclick=\"closeNav()\">&times;</a>\n");
            fileObj.write("\t<A HREF=\"#table1\"><font size=\"5\">Summary</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table0\"><font size=\"5\">Results</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table2\"><font size=\"5\">Hierarchy</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table3\"><font size=\"5\">Graphs</font></A><br>\n");
            fileObj.write("\t</div>\n");
            fileObj.write("\t\n");
            fileObj.write("\t<script>\n");
            fileObj.write("\t\tfunction openNav() {\n");
            fileObj.write("\t\t\tdocument.getElementById(\"mySidenav\").style.width = \"250px\";\n");
            fileObj.write("\t\t\tdocument.getElementById(\"main\").style.marginLeft = \"250px\";}\n");
            fileObj.write("\t\tfunction closeNav() {\n");
            fileObj.write("\t\t\tdocument.getElementById(\"mySidenav\").style.width = \"0\";\n");
            fileObj.write("\t\t\tdocument.getElementById(\"main\").style.marginLeft= \"0\";}\n");
            fileObj.write("\t\tfunction myreload() { location.reload();}\n");
            fileObj.write("\t\n");
            fileObj.write("\t function sortTable(coloum,descending) {\n");
            fileObj.write("\t      var table, rows, switching, i, x, y, shouldSwitch;\n");
            fileObj.write("\t      table = document.getElementById(id=\"resultsTable\"); switching = true;\n");
            fileObj.write("\t       while (switching) {	switching = false; rows = table.getElementsByTagName(\"TR\");\n");
            fileObj.write("\t         for (i = 1; i < (rows.length - 1); i++) { shouldSwitch = false;\n");
            fileObj.write("\t            x = rows[i].getElementsByTagName(\"TD\")[coloum];\n");
            fileObj.write("\t            y = rows[i + 1].getElementsByTagName(\"TD\")[coloum];\n");
            fileObj.write("\t            if(descending){if (x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {\n");
            fileObj.write("\t                   shouldSwitch= true;	break;}}\n");
            fileObj.write("\t            else{if (x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {\n");
            fileObj.write("\t                    shouldSwitch= true;	break;}}}\n");
            fileObj.write("\t            if (shouldSwitch) {	rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);\n");
            fileObj.write("\t                    switching = true;}}}\n");
            fileObj.write("\t\n");
            fileObj.write("\t</script>\n");
            fileObj.write("\t\n");
            fileObj.write("\t<div class=\"navbar\">\n");
            fileObj.write("\t<a href=\"#\">\n");
            fileObj.write("\t<div id=\"main\">\n");
            fileObj.write("\t<span style=\"font-size:30px;cursor:pointer\" onclick=\"openNav()\">&#9776; Views</span>\n");
            fileObj.write("\t</div></a>\n");
            fileObj.write("\t<a href=\"https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-modules\" target=\"_blank\">\n");
            fileObj.write("\t<img \" src=\"https://assets-cdn.github.com/images/modules/logos_page/GitHub-Mark.png\" alt=\"AMD Inference ToolKit\" width=\"60\" height=\"61\" />\n");
            fileObj.write("\t</a>\n");
            fileObj.write("\t<center>\n");
            fileObj.write("\t<img \" src=\"icons/AIToolKit_400x90.png\" alt=\"AMD Inference ToolKit\" /> \n");
            fileObj.write("\t</center>\n");
            fileObj.write("\t</div>\n");
            fileObj.write("\t\n");
            fileObj.write("\t\n");
            fileObj.write("\t\n");
            fileObj.write("\t\n");
            fileObj.write("\t\n");
            fileObj.write("<A NAME=\"table0\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>Inference Results</em></font></h1></A>\n");
            fileObj.write("<table id=\"resultsTable\" cellspacing=\"0\" border=\"0\">\n");
            fileObj.write("\t<colgroup width=\"351\"></colgroup>\n");
            fileObj.write("\t<colgroup span=\"5\" width=\"92\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"114\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"74\"></colgroup>\n");
            fileObj.write("\t<colgroup span=\"6\" width=\"811\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"63\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"51\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"74\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"63\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"51\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"74\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"63\"></colgroup>\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t\t<td height=\"17\" align=\"center\"><font color=\"Maroon\"><b>#Image</b></font></td>\n");
            fileObj.write("\t\t<td height=\"17\" align=\"center\"><font color=\"Maroon\"><b>#FileName</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>TOP-1</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>TOP-2</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>TOP-3</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>TOP-4</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>TOP-5</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><select>\n");
            fileObj.write("\t\t<option onclick=\"myreload()\"><font color=\"Maroon\">groundTruth</font></option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(7,0)\" >groundTruth&#9650;</option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(7,1)\" >groundTruth&#9660;</option>\n");
            fileObj.write("\t\t</select></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><select>\n");
            fileObj.write("\t\t<option onclick=\"myreload()\"><font color=\"Maroon\">Matched</font></option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(8,0)\" >Matched&#9650;</option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(8,1)\" >Matched&#9660;</option>\n");
            fileObj.write("\t\t</select></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>outputLabelText-1</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>outputLabelText-2</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>outputLabelText-3</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>outputLabelText-4</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>outputLabelText-5</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\"><b>groundTruthLabelText</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><select>\n");
            fileObj.write("\t\t<option onclick=\"myreload()\"><font color=\"Maroon\">Prob-1</font></option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(15,0)\" >Prob-1&#9650;</option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(15,1)\" >Prob-1&#9660;</option>\n");
            fileObj.write("\t\t</select></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><select>\n");
            fileObj.write("\t\t<option onclick=\"myreload()\"><font color=\"Maroon\">Prob-2</font></option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(16,0)\" >Prob-2&#9650;</option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(16,1)\" >Prob-2&#9660;</option>\n");
            fileObj.write("\t\t</select></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><select>\n");
            fileObj.write("\t\t<option onclick=\"myreload()\"><font color=\"Maroon\">Prob-3</font></option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(17,0)\" >Prob-3&#9650;</option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(17,1)\" >Prob-3&#9660;</option>\n");
            fileObj.write("\t\t</select></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><select>\n");
            fileObj.write("\t\t<option onclick=\"myreload()\"><font color=\"Maroon\">Prob-4</font></option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(18,0)\" >Prob-4&#9650;</option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(18,1)\" >Prob-4&#9660;</option>\n");
            fileObj.write("\t\t</select></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><select>\n");
            fileObj.write("\t\t<option onclick=\"myreload()\"><font color=\"Maroon\">Prob-5</font></option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(19,0)\" >Prob-5&#9650;</option>\n");
            fileObj.write("\t\t<option onclick=\"sortTable(19,1)\" >Prob-5&#9660;</option>\n");
            fileObj.write("\t\t</select></font></td>\n");
            fileObj.write("\t\t</tr>\n");
            for(int i = 0; i < state->imageDataSize; i++) {
                fileObj.write("\t\t<tr>\n");
                int truth = state->imageLabel[i];
                QString text,labelTxt_1 ,labelTxt_2, labelTxt_3, labelTxt_4, labelTxt_5, truthLabel;
                int match = 0;
                int label_1 = state->resultImageLabelTopK[i][0];
                int label_2 = state->resultImageLabelTopK[i][1];
                int label_3 = state->resultImageLabelTopK[i][2];
                int label_4 = state->resultImageLabelTopK[i][3];
                int label_5 = state->resultImageLabelTopK[i][4];
                float prob_1 = state->resultImageProbTopK[i][0];
                float prob_2 = state->resultImageProbTopK[i][1];
                float prob_3 = state->resultImageProbTopK[i][2];
                float prob_4 = state->resultImageProbTopK[i][3];
                float prob_5 = state->resultImageProbTopK[i][4];
                labelTxt_1 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]] : "Unknown";
                labelTxt_1 = labelTxt_1.replace(QRegExp("n[0-9]{8}"),"");
                labelTxt_2 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]] : "Unknown";
                labelTxt_2 = labelTxt_2.replace(QRegExp("n[0-9]{8}"),"");
                labelTxt_3 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]] : "Unknown";
                labelTxt_3 = labelTxt_3.replace(QRegExp("n[0-9]{8}"),"");
                labelTxt_4 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]] : "Unknown";
                labelTxt_4 = labelTxt_4.replace(QRegExp("n[0-9]{8}"),"");
                labelTxt_5 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]] : "Unknown";
                labelTxt_5 = labelTxt_5.replace(QRegExp("n[0-9]{8}"),"");
                if(truth >= 0)
                {
                    if(truth == label_1) { match = 1; }
                    else if(truth == label_2) { match = 2; }
                    else if(truth == label_3) { match = 3; }
                    else if(truth == label_4) { match = 4; }
                    else if(truth == label_5) { match = 5; }
                    truthLabel = state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown";
                    truthLabel = truthLabel.replace(QRegExp("n[0-9]{8}"),"");
                    if(!exportTool){
                    text.sprintf("\t\t<td height=\"17\" align=\"center\"><img id=\"myImg%d\" src=\"file://%s/%s\"alt=\"<b>GROUND TRUTH:</b> %s<br><b>CLASSIFIED AS:</b> %s\"width=\"30\" height=\"30\"></td>\n",
                                 i,state->dataFolder.toStdString().c_str(),state->imageDataFilenames[i].toStdString().c_str(),
                                 truthLabel.toStdString().c_str(),labelTxt_1.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td height=\"17\" align=\"center\"><a href=\"file://%s/%s\" target=\"_blank\">%s</a></td>\n",state->dataFolder.toStdString().c_str(),
                                 state->imageDataFilenames[i].toStdString().c_str(),state->imageDataFilenames[i].toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    }
                    else{
                        text.sprintf("\t\t<td height=\"17\" align=\"center\"><img id=\"myImg%d\" src=\"images/%s\"alt=\"<b>GROUND TRUTH:</b> %s<br><b>CLASSIFIED AS:</b> %s\"width=\"30\" height=\"30\"></td>\n",
                                     i, state->imageDataFilenames[i].toStdString().c_str(),
                                     truthLabel.toStdString().c_str(),labelTxt_1.toStdString().c_str());
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td height=\"17\" align=\"center\"><a href=\"images/%s\" target=\"_blank\">%s</a></td>\n",
                                     state->imageDataFilenames[i].toStdString().c_str(),state->imageDataFilenames[i].toStdString().c_str());
                        fileObj.write(text.toStdString().c_str());
                    }
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_5);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",truth);
                    fileObj.write(text.toStdString().c_str());
                    if(match)
                        text.sprintf("\t\t<td align=\"center\"><font color=\"green\"><b>%d</b></font></td>\n",match);
                    else
                        text.sprintf("\t\t<td align=\"center\"><font color=\"red\"><b>%d</b></font></td>\n",match);
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_1.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_2.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_3.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_4.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_5.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",truthLabel.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</b></td>\n",prob_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_5);
                    fileObj.write(text.toStdString().c_str());
                }
                else {
                    if(!exportTool){
                        text.sprintf("\t\t<td height=\"17\" align=\"center\"><img id=\"myImg%d\" src=\"file://%s/%s\"alt=\"<b>CLASSIFIED AS:</b> %s\"width=\"30\" height=\"30\"></td>\n",i,state->dataFolder.toStdString().c_str(),
                                     state->imageDataFilenames[i].toStdString().c_str(),labelTxt_1.toStdString().c_str());
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td height=\"17\" align=\"center\"><a href=\"file://%s/%s\" target=\"_blank\">%s</a></td>\n",state->dataFolder.toStdString().c_str(),
                                     state->imageDataFilenames[i].toStdString().c_str(),state->imageDataFilenames[i].toStdString().c_str());
                    }
                    else{
                        text.sprintf("\t\t<td height=\"17\" align=\"center\"><img id=\"myImg%d\" src=\"images/%s\"alt=\"<b><b>CLASSIFIED AS:</b> %s\"width=\"30\" height=\"30\"></td>\n",i,
                                     state->imageDataFilenames[i].toStdString().c_str(),labelTxt_1.toStdString().c_str());
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td height=\"17\" align=\"center\"><a href=\"images/%s\" target=\"_blank\">%s</a></td>\n",
                                     state->imageDataFilenames[i].toStdString().c_str(),state->imageDataFilenames[i].toStdString().c_str());
                        fileObj.write(text.toStdString().c_str());
                    }
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%d</td>\n",label_5);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">-1</td>\n");
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"blue\"><b>-1</b></font></td>\n");
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][0]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_1.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_2.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_3.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_4.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    //text.sprintf("\t\t<td align=\"left\">%s</td>\n",state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]].toStdString().c_str() : "Unknown");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_5.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\"><b>Unknown</b></td>\n");
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\">%.4f</td>\n",prob_5);
                    fileObj.write(text.toStdString().c_str());
                }
                fileObj.write("\t\t</tr>\n");
                fileObj.write("\t\t\n");
                fileObj.write("\t\t<script>\n");
                fileObj.write("\t\tvar modal = document.getElementById('myModal');\n");
                text.sprintf("\t\tvar img1 = document.getElementById('myImg%d');\n",i);
                fileObj.write(text.toStdString().c_str());
                fileObj.write("\t\tvar modalImg = document.getElementById(\"img01\");\n");
                fileObj.write("\t\tvar captionText = document.getElementById(\"caption\");\n");
                fileObj.write("\t\timg1.onclick = function(){ modal.style.display = \"block\"; modalImg.src = this.src; captionText.innerHTML = this.alt; }\n");
                fileObj.write("\t\tvar span = document.getElementsByClassName(\"close\")[0];\n");
                fileObj.write("\t\tspan.onclick = function() { modal.style.display = \"none\"; }\n");
                fileObj.write("\t\t</script>\n");
                fileObj.write("\t\t\n");
            }
            fileObj.write("</table>\n");
            fileObj.write("<A NAME=\"table1\"><h1><font color=\"Maroon\">2: <em>Inference Summary</em></font></h1></A>\n");
            QString text;
            int netSummaryImages =  state->imageDataSize - state->totalNoGroundTruth;
            float passProb = (state->top1TotProb+state->top2TotProb+state->top3TotProb+state->top4TotProb+state->top5TotProb);
            int passCount = (state->top1Count+state->top2Count+state->top3Count+state->top4Count+state->top5Count);
            float avgPassProb = passProb/passCount;

            text.sprintf("<h3>&emsp;&emsp;<font color=\"blue\">Images without ground Truth:</font> %d</h3>\n",state->totalNoGroundTruth);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("<h3>&emsp;&emsp;<font color=\"blue\">Images with ground Truth:</font> %d</h3>\n",netSummaryImages);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("<h3>&emsp;&emsp;<font color=\"blue\">Total image set for inference:</font> %d</h3><br>\n",state->imageDataSize);
            fileObj.write(text.toStdString().c_str());

            text.sprintf("\n<h3>&emsp;&emsp;Total Top K match: %d</h3>\n",passCount);
            fileObj.write(text.toStdString().c_str());
            float accuracyPer = ((float)passCount / netSummaryImages);
            text.sprintf("<h3>&emsp;&emsp;Inference Accuracy on Top K: %.2f Percent</h3>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("<h3>&emsp;&emsp;Average Pass Probability for Top K: %.2f</h3><br>\n\n",avgPassProb);
            fileObj.write(text.toStdString().c_str());

            text.sprintf("<h3>&emsp;&emsp;<font color=\"blue\">Total mismatch:</font> %d\n",state->totalMismatch);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->totalMismatch/netSummaryImages);
            text.sprintf("<h3>&emsp;&emsp;<font color=\"blue\">Inference mismatch Percentage:</font> %.2f Percent</h3>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("<h3>&emsp;&emsp;<font color=\"blue\">Average mismatch Probability for Top 1:</font> %.4f</h3><br>\n",state->totalFailProb/state->totalMismatch);
            fileObj.write(text.toStdString().c_str());


            fileObj.write("<table cellspacing=\"4\" border=\"1\" style=\"margin-left: 2cm;\">\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 1 Match</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 1 Match %</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 2 Match</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 2 Match %</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 3 Match</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 3 Match %</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 4 Match</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 4 Match %</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 5 Match</b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b>Top 5 Match %</b></td>\n");
            fileObj.write("\t\t</tr>\n");

            fileObj.write("\t<tr>\n");
            text.sprintf("\t\t<td align=\"center\"><b>%d</b></td>\n",state->top1Count);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top1Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><b>%.2f</b></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><b>%d</b></td>\n",state->top2Count);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top2Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><b>%.2f</b></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><b>%d</b></td>\n",state->top3Count);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top3Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><b>%.2f</b></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><b>%d</b></td>\n",state->top4Count);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top4Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><b>%.2f</b></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><b>%d</b></td>\n",state->top5Count);
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top5Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><b>%.2f</b></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t\t</tr>\n");


            fileObj.write("</table>\n");
            fileObj.write("<A NAME=\"table2\"><h1><font color=\"Maroon\">3: <em>Hierarchy Summary</em></font></h1></A>\n");
            fileObj.write("<table cellspacing=\"0\" border=\"0\">\n");
            fileObj.write("\t<colgroup width=\"351\"></colgroup>\n");
            fileObj.write("\t<colgroup span=\"5\" width=\"92\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"114\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"74\"></colgroup>\n");
            fileObj.write("\t<colgroup span=\"6\" width=\"811\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"63\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"51\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"74\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"63\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"51\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"74\"></colgroup>\n");
            fileObj.write("\t<colgroup width=\"63\"></colgroup>\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t\t<td height=\"17\" align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b></b></td>\n");
            fileObj.write("\t\t</tr>\n");

            //Dump values here in a loop
            fileObj.write("\t\t<tr>\n");
            fileObj.write("\t\t</tr>\n");

            fileObj.write("</table>\n");

            fileObj.write("\n</body>\n");
            fileObj.write("\n</html>\n");

        }
        fileObj.close();
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
        float resultProb = -1;
        if(state->topKValue) resultProb = state->resultImageProbTopK[index][0];
        QString resultSummary = state->inferenceResultSummary[index];
        QString resultHierarchy, truthHierarchy;
        if(state->dataHierarchy->size()){
            if(truthLabel != -1) truthHierarchy = state->dataHierarchy ? (*state->dataHierarchy)[truthLabel] : "Unknown";
            resultHierarchy = state->dataHierarchy ? (*state->dataHierarchy)[resultLabel] : "Unknown";
        }
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
                if(state->dataHierarchy->size()){
                    QString textHierarchy, textHierarchyLabel;
                    std::string input = resultHierarchy.toStdString().c_str();
                    std::istringstream ss(input);
                    std::string token;
                    textHierarchy = "Label Hierarchy:";
                    while(std::getline(ss, token, ',')) {
                        textHierarchyLabel = "";
                        if(token.size())
                            textHierarchyLabel.sprintf("-> %s", token.c_str());

                        textHierarchy += textHierarchyLabel;
                    }
                    painter.drawText(QRect(x + 4 + ICON_SIZE * 2 + 4, y + 4 + fontMetrics.height() + 24, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, textHierarchy);
                }
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
                if(state->dataHierarchy->size()){
                    QString textHierarchy, textHierarchyLabel;
                    std::string input_result = resultHierarchy.toStdString().c_str();
                    std::string input_truth = truthHierarchy.toStdString().c_str();
                    std::istringstream ss_result(input_result);
                    std::istringstream ss_truth(input_truth);
                    std::string token_result, token_truth;
                    textHierarchy = "Label Hierarchy:";
                    while(std::getline(ss_result, token_result, ',') && std::getline(ss_truth, token_truth, ',')) {
                        textHierarchyLabel = "";
                        if(token_truth.size() && (token_truth == token_result))
                            textHierarchyLabel.sprintf("->%s", token_result.c_str());

                        textHierarchy += textHierarchyLabel;
                    }
                    if(textHierarchy == "Label Hierarchy:"){ textHierarchy += "NO MATCH"; painter.setPen(Qt::red);}
                    else { painter.setPen(Qt::darkGreen); }
                    painter.drawText(QRect(x + 4 + ICON_SIZE * 2 + 4, y + 4 + fontMetrics.height() + 24, w - 8, fontMetrics.height()), Qt::AlignLeft | Qt::AlignTop, textHierarchy);
                }
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
        if(state->topKValue)
            text.sprintf("classified as [label=%d Prob=%.3f] ", resultLabel, resultProb);
        else
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
