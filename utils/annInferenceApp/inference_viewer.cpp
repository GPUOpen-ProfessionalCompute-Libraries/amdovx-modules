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
    // perf results
    perfButtonRect = QRect(0, 0, 0, 0);
    perfButtonPressed = false;
    startTime =  "";
}

inference_viewer::inference_viewer(QString serverHost, int serverPort, QString modelName,
        QVector<QString> * dataLabels, QVector<QString> * dataHierarchy, QString dataFilename, QString dataFolder,
        int dimInput[3], int GPUs, int dimOutput[3], int maxImageDataSize,
        bool repeat_images, bool sendScaledImages, int sendFileName_, int topKValue,
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
    state->sendFileName = sendFileName_;
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

    // summary date and time
    QDateTime curtim = QDateTime::currentDateTime();
    QString abbr = curtim.timeZoneAbbreviation();
    const QDateTime now = QDateTime::currentDateTime();
    QString DateTime_test = now.toString("yyyy-MM-dd hh:mm:ss");
    state->startTime.sprintf("%s %s",DateTime_test.toStdString().c_str(),abbr.toStdString().c_str());
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
                &state->imageBuffer, &progress, state->sendFileName, state->topKValue, &state->shadowFileBuffer);
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

void inference_viewer::showPerfResults()
{
    state->performance.setModelName(state->modelName);
    state->performance.setStartTime(state->startTime);
    state->performance.setNumGPU(state->GPUs);
    state->performance.show();

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
                    for(int k = 0; k < 7; k++) state->topLabelMatch[j][k] = 0;
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

                                if(truth != label_1) { state->topLabelMatch[label_1][6]++; }
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
            fileObj.write("\nProbability,Pass,Fail,cat-1 pass,cat-1 fail,cat-2 pass, cat-2 fail,"
                          "cat-3 pass,cat-3 fail,cat-4 pass,cat-4 fail,cat-5 pass,cat-5 fail,cat-6 pass,cat-6 fail\n");
            for(int i = 99; i >= 0; i--){
                text.sprintf("%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",f,state->topKPassFail[i][0],state->topKPassFail[i][1],
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
            fileObj.write("\nLabel,Images in DataBase, Matched with Top1, Matched with Top2, "
                          "Matched with Top3, Matched with Top4, Matched with Top5,Top1 Label Match, Label Description\n");
            for(int i = 0; i < 1000; i++){
                text.sprintf("%d,%d,%d,%d,%d,%d,%d,%d,\"%s\"\n",i, state->topLabelMatch[i][0],
                        state->topLabelMatch[i][1],
                        state->topLabelMatch[i][2],
                        state->topLabelMatch[i][3],
                        state->topLabelMatch[i][4],
                        state->topLabelMatch[i][5],
                        state->topLabelMatch[i][6],
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
            fileObj.write("\t<title>AMD Dataset Analysis Tool</title>\n");
            if(exportTool){
                fileObj.write("\t<link rel=\"icon\" href=\"icons/vega_icon_150.png\"/>\n");
            }
            // page style
            fileObj.write("\n\t<style type=\"text/css\">\n");
            fileObj.write("\t\n");
            fileObj.write("\tbody,div,table,thead,tbody,tfoot,tr,th,td,p { font-family:\"Liberation Sans\"; font-size:x-small }\n");
            fileObj.write("\ta.comment-indicator:hover + comment { background:#ffd; position:absolute; display:block; border:1px solid black; padding:0.5em;  }\n");
            fileObj.write("\ta.comment-indicator { background:red; display:inline-block; border:1px solid black; width:0.5em; height:0.5em;  }\n");
            fileObj.write("\tcomment { display:none;  } tr:nth-of-type(odd) { background-color:#f2f2f2;}\n");
            fileObj.write("\t\n");
            fileObj.write("\t#myImg { border-radius: 5px; cursor: pointer; transition: 0.3s; }\n");
            fileObj.write("\t#myImg:hover { opacity: 0.7; }\n");
            fileObj.write("\t.modal{ display: none; position: fixed; z-index: 8; padding-top: 100px; left: 0; top: 0;width: 100%;\n");
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
            fileObj.write("\t.sidenav { height: 100%; width: 0; position: fixed; z-index: 7; top: 0; left: 0; background-color: #111;\n");
            fileObj.write("\t\t overflow-x: hidden;    transition: 0.5s; padding-top: 60px;}\n");
            fileObj.write("\t.sidenav a { padding: 8px 8px 8px 32px; text-decoration: none; font-size: 25px; color: #818181; display: block; transition: 0.3s;}\n");
            fileObj.write("\t.sidenav a:hover { color: #f1f1f1;}\n");
            fileObj.write("\t.sidenav .closebtn {  position: absolute; top: 0; right: 25px; font-size: 36px; margin-left: 50px;}\n");
            fileObj.write("\t#main {  transition: margin-left .5s;  padding: 16px; }\n");
            fileObj.write("\t@media screen and (max-height: 450px) { .sidenav {padding-top: 15px;} .sidenav a {font-size: 18px;} }\n");
            fileObj.write("\t\n");
            fileObj.write("\tbody {margin:0;}\n");
            fileObj.write("\t.navbar {  overflow: hidden;  background-color: #333;  position: fixed; z-index: 6;  top: 0;  width: 100%;}\n");
            fileObj.write("\t.navbar a {  float: left;  display: block;  color: #f2f2f2;  text-align: center;  padding: 14px 16px;  text-decoration: none;  font-size: 17px; }\n");
            fileObj.write("\t.navbar a:hover {  background: #ddd;  color: black;}\n");
            fileObj.write("\t.main {  padding: 16px;  margin-top: 30px; }\n");
            fileObj.write("\t\n");
            fileObj.write("\tselect {-webkit-appearance: none; -moz-appearance: none; text-indent: 0px; text-overflow: ''; color:maroon; }\n");
            fileObj.write("\t\n");
            fileObj.write("\t.tooltip { position: relative; display: inline-block;}\n");
            fileObj.write("\t.tooltip .tooltiptext { visibility: hidden; width: 150px; background-color: black; color: gold;\n");
            fileObj.write("\t\ttext-align: center;  border-radius: 6px;  padding: 5px; position: absolute; z-index: 3;}\n");
            fileObj.write("\t.tooltip:hover .tooltiptext { visibility: visible;}\n");
            fileObj.write("\t\n");
            fileObj.write("\t\t.footer { position: fixed; left: 0;    bottom: 0;  width: 100%;    background-color: #333;  color: white;  text-align: center;}\n");
            fileObj.write("\t\n");
            fileObj.write("\t</style>\n");
            fileObj.write("\n</head>\n");
            fileObj.write("\n\n<body>\n");
            fileObj.write("\t\n");
            fileObj.write("\t<div id=\"myModal\" class=\"modal\"> <span class=\"close\">&times;</span>  <img class=\"modal-content\" id=\"img01\">  <div id=\"caption\"></div> </div>\n");
            fileObj.write("\t\n");

            // table content order
            fileObj.write("\t<div id=\"mySidenav\" class=\"sidenav\">\n");
            fileObj.write("\t<a href=\"javascript:void(0)\" class=\"closebtn\" onclick=\"closeNav()\">&times;</a>\n");
            fileObj.write("\t<A HREF=\"#table0\"><font size=\"5\">Summary</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table1\"><font size=\"5\">Graphs</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table2\"><font size=\"5\">Hierarchy</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table3\"><font size=\"5\">Labels</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table4\"><font size=\"5\">Image Results</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table5\"><font size=\"5\">Compare</font></A><br>\n");
            //fileObj.write("\t<A HREF=\"#table6\"><font size=\"5\">Error Suspects</font></A><br>\n");
            fileObj.write("\t<A HREF=\"#table7\"><font size=\"5\">Help</font></A><br>\n");
            fileObj.write("\t</div>\n");
            fileObj.write("\t\n");

            // scripts
            fileObj.write("\t<script>\n");
            fileObj.write("\t\tfunction openNav() {\n");
            fileObj.write("\t\t\tdocument.getElementById(\"mySidenav\").style.width = \"250px\";\n");
            fileObj.write("\t\t\tdocument.getElementById(\"main\").style.marginLeft = \"250px\";}\n");
            fileObj.write("\t\tfunction closeNav() {\n");
            fileObj.write("\t\t\tdocument.getElementById(\"mySidenav\").style.width = \"0\";\n");
            fileObj.write("\t\t\tdocument.getElementById(\"main\").style.marginLeft= \"0\";}\n");
            fileObj.write("\t\tfunction myreload() { location.reload();}\n");
            fileObj.write("\t\n");
            fileObj.write("\t\tfunction sortTable(coloum,descending) {\n");
            fileObj.write("\t\tvar table, rows, switching, i, x, y, shouldSwitch;\n");
            fileObj.write("\t\ttable = document.getElementById(id=\"resultsTable\"); switching = true;\n");
            fileObj.write("\t\twhile (switching) {	switching = false; rows = table.getElementsByTagName(\"TR\");\n");
            fileObj.write("\t\t\tfor (i = 1; i < (rows.length - 1); i++) { shouldSwitch = false;\n");
            fileObj.write("\t\t\t\tx = rows[i].getElementsByTagName(\"TD\")[coloum];\n");
            fileObj.write("\t\t\t\ty = rows[i + 1].getElementsByTagName(\"TD\")[coloum];\n");
            fileObj.write("\t\t\t\tif(descending){if (x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {\n");
            fileObj.write("\t\t\t\t\tshouldSwitch= true;	break;}}\n");
            fileObj.write("\t\t\t\telse{if (x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {\n");
            fileObj.write("\t\t\t\t\tshouldSwitch= true;	break;}}}\n");
            fileObj.write("\t\t\t\tif (shouldSwitch) {	rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);\n");
            fileObj.write("\t\t\t\t\tswitching = true;}}}\n");
            fileObj.write("\t\n");
            fileObj.write("\t\n");
            fileObj.write("\t\tfunction sortLabelsTable(coloum,descending) {\n");
            fileObj.write("\t\tvar table, rows, switching, i, x, y, shouldSwitch;\n");
            fileObj.write("\t\ttable = document.getElementById(id=\"labelsTable\"); switching = true;\n");
            fileObj.write("\t\twhile (switching) {	switching = false; rows = table.getElementsByTagName(\"TR\");\n");
            fileObj.write("\t\t\tfor (i = 1; i < (rows.length - 1); i++) { shouldSwitch = false;\n");
            fileObj.write("\t\t\t\tx = rows[i].getElementsByTagName(\"TD\")[coloum];\n");
            fileObj.write("\t\t\t\ty = rows[i + 1].getElementsByTagName(\"TD\")[coloum];\n");
            fileObj.write("\t\t\t\tif(descending){if (x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {\n");
            fileObj.write("\t\t\t\t\tshouldSwitch= true;	break;}}\n");
            fileObj.write("\t\t\t\telse{if (x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {\n");
            fileObj.write("\t\t\t\t\tshouldSwitch= true;	break;}}}\n");
            fileObj.write("\t\t\t\tif (shouldSwitch) {	rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);\n");
            fileObj.write("\t\t\t\t\tswitching = true;}}}\n");
            fileObj.write("\t\n");
            fileObj.write("\t</script>\n");
            fileObj.write("\t<script src=\"https://www.kryogenix.org/code/browser/sorttable/sorttable.js\"></script>\n");
            fileObj.write("\t\n");
            fileObj.write("\t<script>\n");
            fileObj.write("\t\tfunction filterResultTable(rowNum, DataVar) {\n");
            fileObj.write("\t\tvar input, filter, table, tr, td, i;\n");
            fileObj.write("\t\tinput = document.getElementById(DataVar);\n");
            fileObj.write("\t\tfilter = input.value.toUpperCase();\n");
            fileObj.write("\t\ttable = document.getElementById(\"resultsTable\");\n");
            fileObj.write("\t\ttr = table.getElementsByTagName(\"tr\");\n");
            fileObj.write("\t\tfor (i = 1; i < tr.length; i++) {\n");
            fileObj.write("\t\ttd = tr[i].getElementsByTagName(\"td\")[rowNum];\n");
            fileObj.write("\t\tif (td) { if (td.innerHTML.toUpperCase().indexOf(filter) > -1) {tr[i].style.display = \"\"; }\n");
            fileObj.write("\t\telse { tr[i].style.display = \"none\";}}}}\n");
            fileObj.write("\t</script>\n");
            fileObj.write("\t\n");
            fileObj.write("\t\n");
            fileObj.write("\t<script>\n");
            fileObj.write("\t\tfunction filterLabelTable(rowNum, DataVar) {\n");
            fileObj.write("\t\tvar input, filter, table, tr, td, i;\n");
            fileObj.write("\t\tinput = document.getElementById(DataVar);\n");
            fileObj.write("\t\tfilter = input.value.toUpperCase();\n");
            fileObj.write("\t\ttable = document.getElementById(\"labelsTable\");\n");
            fileObj.write("\t\ttr = table.getElementsByTagName(\"tr\");\n");
            fileObj.write("\t\tfor (i = 1; i < tr.length; i++) {\n");
            fileObj.write("\t\ttd = tr[i].getElementsByTagName(\"td\")[rowNum];\n");
            fileObj.write("\t\tif (td) { if (td.innerHTML.toUpperCase().indexOf(filter) > -1) {tr[i].style.display = \"\"; }\n");
            fileObj.write("\t\telse { tr[i].style.display = \"none\";}}}}\n");
            fileObj.write("\t</script>\n");
            fileObj.write("\t\n");
            fileObj.write("\n");
            fileObj.write("\t<script>\n");
            fileObj.write("\t\tfunction clearLabelFilter() {\n");
            fileObj.write("\t\tdocument.getElementById('Label ID').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Label Description').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Images in DataBase').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched Top1 %').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched Top5 %').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched 1st').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched 2nd').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched 3th').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched 4th').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched 5th').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Misclassified Top1 Label').value = ''\n");
            fileObj.write("\t\tfilterLabelTable(0,'Label ID') }\n");
            fileObj.write("\t</script>\n");
            fileObj.write("\n");
            fileObj.write("\n");
            fileObj.write("\t<script>\n");
            fileObj.write("\t\tfunction clearResultFilter() {\n");
            fileObj.write("\t\tdocument.getElementById('GroundTruthText').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('GroundTruthID').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Matched').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top1').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top1Prob').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Text1').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top2').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top2Prob').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top3').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top3Prob').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top4').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top4Prob').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top5').value = ''\n");
            fileObj.write("\t\tdocument.getElementById('Top5Prob').value = ''\n");
            fileObj.write("\t\tfilterResultTable(2,'GroundTruthText') }\n");
            fileObj.write("\t</script>\n");
            fileObj.write("\n");
            fileObj.write("\t<script>\n");
            fileObj.write("\t\tfunction findGroundTruthLabel(label,labelID) {\n");
            fileObj.write("\t\tclearResultFilter();\n");
            fileObj.write("\t\tdocument.getElementById('GroundTruthText').value = label;\n");
            fileObj.write("\t\tdocument.getElementById('GroundTruthID').value = labelID;\n");
            fileObj.write("\t\tandResultFilter();\n");
            fileObj.write("\t\twindow.location.href = '#table4';\n");
            fileObj.write("\t\t}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction findMisclassifiedGroundTruthLabel(label) {\n");
            fileObj.write("\t\tclearResultFilter();\n");
            fileObj.write("\t\tdocument.getElementById('Text1').value = label;\n");
            fileObj.write("\t\tfilterResultTable(10,'Text1');\n");
            fileObj.write("\t\twindow.location.href = '#table4';\n");
            fileObj.write("\t\t}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction highlightRow(obj){\n");
            fileObj.write("\t\tvar tr=obj; while (tr.tagName.toUpperCase()!='TR'&&tr.parentNode){  tr=tr.parentNode;}\n");
            fileObj.write("\t\tif (!tr.col){tr.col=tr.bgColor; } if (obj.checked){  tr.bgColor='#d5f5e3';}\n");
            fileObj.write("\t\telse {  tr.bgColor=tr.col;}}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction goToImageResults() { window.location.href = '#table4';}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction findImagesWithNoGroundTruthLabel() {\n");
            fileObj.write("\t\tclearResultFilter();\n");
            fileObj.write("\t\tdocument.getElementById('GroundTruthID').value = '-1';\n");
            fileObj.write("\t\tfilterResultTable(3,'GroundTruthID');\n");
            fileObj.write("\t\twindow.location.href = '#table4';\n");
            fileObj.write("\t\t}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction findImageMisMatch() {\n");
            fileObj.write("\t\tclearResultFilter();\n");
            fileObj.write("\t\tdocument.getElementById('Matched').value = '0';\n");
            fileObj.write("\t\tfilterResultTable(9,'Matched');\n");
            fileObj.write("\t\twindow.location.href = '#table4';\n");
            fileObj.write("\t\t}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction findTopKMatch() {\n");
            fileObj.write("\t\tclearResultFilter();\n");
            fileObj.write("\t\tdocument.getElementById('Matched').value = '0';\n");
            fileObj.write("\t\tnotResultFilter();\n");
            fileObj.write("\t\twindow.location.href = '#table4';\n");
            fileObj.write("\t\t}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction filterResultTableInverse(rowNum, DataVar) {\n");
            fileObj.write("\t\tvar input, filter, table, tr, td, i;\n");
            fileObj.write("\t\tinput = document.getElementById(DataVar);\n");
            fileObj.write("\t\tfilter = input.value.toUpperCase();\n");
            fileObj.write("\t\ttable = document.getElementById(\"resultsTable\");\n");
            fileObj.write("\t\ttr = table.getElementsByTagName(\"tr\");\n");
            fileObj.write("\t\tfor (i = 1; i < tr.length; i++) {\n");
            fileObj.write("\t\ttd = tr[i].getElementsByTagName(\"td\")[rowNum];\n");
            fileObj.write("\t\tif (td) { if (td.innerHTML.toUpperCase().indexOf(filter) <= -1) {tr[i].style.display = \"\"; }\n");
            fileObj.write("\t\telse { tr[i].style.display = \"none\";}}}}\n");
            fileObj.write("\t\tfunction findImagesWithGroundTruthLabel(){\n");
            fileObj.write("\t\tclearResultFilter();\n");
            fileObj.write("\t\tdocument.getElementById('Matched').value = '-1';\n");
            fileObj.write("\t\tfilterResultTableInverse(9, 'Matched')\n");
            fileObj.write("\t\twindow.location.href = '#table4';\n");
            fileObj.write("\t\t}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction notResultFilter( ) {\n");
            fileObj.write("\t\tvar input, filter, table, tr, td, i, rowNum, count;\n");
            fileObj.write("\t\tcount=0;\n");
            fileObj.write("\t\tif(document.getElementById('GroundTruthText').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('GroundTruthText');	rowNum = 2;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('GroundTruthID').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('GroundTruthID'); rowNum = 3;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Matched').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Matched');	rowNum = 9;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top1').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top1'); rowNum = 4;count++; }\n");
            fileObj.write("\t\tif(document.getElementById('Top1Prob').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top1Prob');rowNum = 15;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Text1').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Text1');rowNum = 10;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top2').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top2');rowNum = 5;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top2Prob').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top2Prob');rowNum = 16;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top3').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top3');rowNum = 6;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top3Prob').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top3Prob');rowNum = 17;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top4').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top4');rowNum = 7;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top4Prob').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top4Prob');rowNum = 18;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top5').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top5');rowNum = 8;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('Top5Prob').value != ''){\n");
            fileObj.write("\t\tinput = document.getElementById('Top5Prob');rowNum = 19;count++;}\n");
            fileObj.write("\t\tif(count == 0){alert(\"Not Filter ERROR: No filter variable entered\");}\n");
            fileObj.write("\t\telse if(count > 1){\n");
            fileObj.write("\t\talert(\"Not Filter ERROR: Only one variable filtering supported. Use Clear Filter and enter one filter variable\");}\n");
            fileObj.write("\t\tfilter = input.value.toUpperCase();\n");
            fileObj.write("\t\ttable = document.getElementById(\"resultsTable\");\n");
            fileObj.write("\t\ttr = table.getElementsByTagName(\"tr\");\n");
            fileObj.write("\t\tfor (i = 1; i < tr.length; i++) {\n");
            fileObj.write("\t\ttd = tr[i].getElementsByTagName(\"td\")[rowNum];\n");
            fileObj.write("\t\tif (td) { if (td.innerHTML.toUpperCase().indexOf(filter) <= -1) {tr[i].style.display = \"\"; }\n");
            fileObj.write("\t\telse { tr[i].style.display = \"none\";}}}}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction andResultFilter( ) {\n");
            fileObj.write("\t\tvar inputOne, inputTwo, filterOne, filterTwo, table, tr, tdOne, tdTwo, i, rowNumOne, rowNumTwo,count;\n");
            fileObj.write("\t\tcount=0;\n");
            fileObj.write("\t\trowNumOne=0;\n");
            fileObj.write("\t\tif(document.getElementById('GroundTruthText').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('GroundTruthText');	rowNumOne = 2;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('GroundTruthID').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('GroundTruthID'); rowNumOne = 3;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Matched').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Matched');	rowNumOne = 9;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top1').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top1'); rowNumOne = 4;count++; }\n");
            fileObj.write("\t\telse if(document.getElementById('Top1Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top1Prob');rowNumOne = 15;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Text1').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Text1');rowNumOne = 10;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top2');rowNumOne = 5;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top2Prob');rowNumOne = 16;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top3');rowNumOne = 6;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top3Prob');rowNumOne = 17;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top4');rowNumOne = 7;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top4Prob');rowNumOne = 18;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top5');rowNumOne = 8;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top5Prob');rowNumOne = 19;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('GroundTruthText').value != '' && rowNumOne  != 2){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('GroundTruthText');	rowNumTwo = 2;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('GroundTruthID').value != '' && rowNumOne  != 3){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('GroundTruthID'); rowNumTwo = 3;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Matched').value != '' && rowNumOne  != 9){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Matched');	rowNumTwo = 9;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top1').value != '' && rowNumOne  != 4){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top1'); rowNumTwo = 4;count++; }\n");
            fileObj.write("\t\telse if(document.getElementById('Top1Prob').value != '' && rowNumOne  != 215){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top1Prob');rowNumTwo = 15;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Text1').value != '' && rowNumOne  != 10){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Text1');rowNumTwo = 10;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2').value != '' && rowNumOne  != 5){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top2');rowNumTwo = 5;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2Prob').value != '' && rowNumOne  != 16){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top2Prob');rowNumTwo = 16;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3').value != '' && rowNumOne  != 6){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top3');rowNumTwo = 6;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3Prob').value != '' && rowNumOne  != 17){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top3Prob');rowNumTwo = 17;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4').value != '' && rowNumOne  != 7){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top4');rowNumTwo = 7;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4Prob').value != '' && rowNumOne  != 18){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top4Prob');rowNumTwo = 18;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5').value != '' && rowNumOne  != 8){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top5');rowNumTwo = 8;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5Prob').value != '' && rowNumOne  != 19){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top5Prob');rowNumTwo = 19;count++;}\n");
            fileObj.write("\t\tif(count == 0){alert(\"AND Filter ERROR: No filter variable entered\");}\n");
            fileObj.write("\t\telse if(count == 1){alert(\"AND Filter ERROR: Enter two variables\");}\n");
            fileObj.write("\t\telse if(count > 2){\n");
            fileObj.write("\t\talert(\"AND Filter ERROR: Only two variable filtering supported. Use Clear Filter and enter two filter variable\");}\n");
            fileObj.write("\t\tfilterOne = inputOne.value.toUpperCase();\n");
            fileObj.write("\t\tfilterTwo = inputTwo.value.toUpperCase();\n");
            fileObj.write("\t\ttable = document.getElementById(\"resultsTable\");\n");
            fileObj.write("\t\ttr = table.getElementsByTagName(\"tr\");\n");
            fileObj.write("\t\tfor (i = 1; i < tr.length; i++) {\n");
            fileObj.write("\t\ttdOne = tr[i].getElementsByTagName(\"td\")[rowNumOne];\n");
            fileObj.write("\t\ttdTwo = tr[i].getElementsByTagName(\"td\")[rowNumTwo];\n");
            fileObj.write("\t\tif (tdOne && tdTwo) { \n");
            fileObj.write("\t\tif (tdOne.innerHTML.toUpperCase().indexOf(filterOne) > -1 && tdTwo.innerHTML.toUpperCase().indexOf(filterTwo) > -1) \n");
            fileObj.write("\t\t{tr[i].style.display = \"\";}\n");
            fileObj.write("\t\telse { tr[i].style.display = \"none\";}}}}\n");
            fileObj.write("\n");
            fileObj.write("\t\tfunction orResultFilter( ) {\n");
            fileObj.write("\t\tvar inputOne, inputTwo, filterOne, filterTwo, table, tr, tdOne, tdTwo, i, rowNumOne, rowNumTwo, count;\n");
            fileObj.write("\t\tcount=0;\n");
            fileObj.write("\t\trowNumOne=0;\n");
            fileObj.write("\t\tif(document.getElementById('GroundTruthText').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('GroundTruthText');	rowNumOne = 2;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('GroundTruthID').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('GroundTruthID'); rowNumOne = 3;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Matched').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Matched');	rowNumOne = 9;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top1').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top1'); rowNumOne = 4;count++; }\n");
            fileObj.write("\t\telse if(document.getElementById('Top1Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top1Prob');rowNumOne = 15;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Text1').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Text1');rowNumOne = 10;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top2');rowNumOne = 5;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top2Prob');rowNumOne = 16;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top3');rowNumOne = 6;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top3Prob');rowNumOne = 17;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top4');rowNumOne = 7;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top4Prob');rowNumOne = 18;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top5');rowNumOne = 8;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5Prob').value != ''){\n");
            fileObj.write("\t\tinputOne = document.getElementById('Top5Prob');rowNumOne = 19;count++;}\n");
            fileObj.write("\t\tif(document.getElementById('GroundTruthText').value != '' && rowNumOne  != 2){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('GroundTruthText');	rowNumTwo = 2;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('GroundTruthID').value != '' && rowNumOne  != 3){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('GroundTruthID'); rowNumTwo = 3;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Matched').value != '' && rowNumOne  != 9){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Matched');	rowNumTwo = 9;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top1').value != '' && rowNumOne  != 4){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top1'); rowNumTwo = 4;count++; }\n");
            fileObj.write("\t\telse if(document.getElementById('Top1Prob').value != '' && rowNumOne  != 215){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top1Prob');rowNumTwo = 15;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Text1').value != '' && rowNumOne  != 10){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Text1');rowNumTwo = 10;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2').value != '' && rowNumOne  != 5){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top2');rowNumTwo = 5;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top2Prob').value != '' && rowNumOne  != 16){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top2Prob');rowNumTwo = 16;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3').value != '' && rowNumOne  != 6){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top3');rowNumTwo = 6;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top3Prob').value != '' && rowNumOne  != 17){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top3Prob');rowNumTwo = 17;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4').value != '' && rowNumOne  != 7){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top4');rowNumTwo = 7;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top4Prob').value != '' && rowNumOne  != 18){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top4Prob');rowNumTwo = 18;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5').value != '' && rowNumOne  != 8){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top5');rowNumTwo = 8;count++;}\n");
            fileObj.write("\t\telse if(document.getElementById('Top5Prob').value != '' && rowNumOne  != 19){\n");
            fileObj.write("\t\tinputTwo = document.getElementById('Top5Prob');rowNumTwo = 19;count++;}\n");
            fileObj.write("\t\tif(count == 0){alert(\"OR Filter ERROR: No filter variable entered\");}\n");
            fileObj.write("\t\telse if(count == 1){alert(\"OR Filter ERROR: Enter two variables\");}\n");
            fileObj.write("\t\telse if(count > 2){\n");
            fileObj.write("\t\talert(\"OR Filter ERROR: Only two variable filtering supported. Use Clear Filter and enter two filter variable\");}\n");
            fileObj.write("\t\tfilterOne = inputOne.value.toUpperCase();\n");
            fileObj.write("\t\tfilterTwo = inputTwo.value.toUpperCase();\n");
            fileObj.write("\t\ttable = document.getElementById(\"resultsTable\");\n");
            fileObj.write("\t\ttr = table.getElementsByTagName(\"tr\");\n");
            fileObj.write("\t\tfor (i = 1; i < tr.length; i++) {\n");
            fileObj.write("\t\ttdOne = tr[i].getElementsByTagName(\"td\")[rowNumOne];\n");
            fileObj.write("\t\ttdTwo = tr[i].getElementsByTagName(\"td\")[rowNumTwo];\n");
            fileObj.write("\t\tif (tdOne && tdTwo) { \n");
            fileObj.write("\t\tif (tdOne.innerHTML.toUpperCase().indexOf(filterOne) > -1 || tdTwo.innerHTML.toUpperCase().indexOf(filterTwo) > -1) \n");
            fileObj.write("\t\t{tr[i].style.display = \"\";}\n");
            fileObj.write("\t\telse { tr[i].style.display = \"none\";}}}}\n");
            fileObj.write("\n");
            fileObj.write("\t</script>\n");
            fileObj.write("\n");


            // graph script
            QString text;
            int netSummaryImages =  state->imageDataSize - state->totalNoGroundTruth;
            int passCount = (state->top1Count+state->top2Count+state->top3Count+state->top4Count+state->top5Count);
            fileObj.write("\t<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n");
            fileObj.write("\t<script type=\"text/javascript\">\n");
            fileObj.write("\t\n");
            // overall summary
            fileObj.write("\tgoogle.charts.load('current', {'packages':['bar']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawChart);\n");
            fileObj.write("\tfunction drawChart(){\n");
            fileObj.write("\tvar data = google.visualization.arrayToDataTable([\n");
            fileObj.write("\t['  '     ,  'Match'  , 'Mismatch', 'No Label' ],\n");
            text.sprintf("\t['Summary',   %d     , %d        , %d         ]\n",passCount,state->totalMismatch,state->totalNoGroundTruth);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = { title: 'Overall Result Summary', vAxis: { title: 'Images' }, width: 800, height: 400 };\n");
            fileObj.write("\tvar chart = new google.charts.Bar(document.getElementById('Model_Stats'));\n");
            fileObj.write("\tchart.draw(data, google.charts.Bar.convertOptions(options));}\n");
            fileObj.write("\t\n");
            // TopK pass fail summary
            fileObj.write("\tgoogle.charts.load('current', {'packages':['corechart']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawTopKResultChart);\n");
            fileObj.write("\tfunction drawTopKResultChart() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('string', 'Top K');\n");
            fileObj.write("\tdata.addColumn('number', 'Matchs');\n");
            fileObj.write("\tdata.addRows([\n");
            text.sprintf("\t[ 'Matched Top%d Choice', %d  ],\n",state->topKValue,passCount);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t[ 'MisMatched', %d  ]]);\n",state->totalMismatch);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\tvar options = { title:'Image Match/Mismatch Summary', width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.PieChart(document.getElementById('topK_result_chart_div'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // topK summary
            fileObj.write("\tgoogle.charts.load('current', {'packages':['corechart']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawResultChart);\n");
            fileObj.write("\tfunction drawResultChart() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('string', 'Top K');\n");
            fileObj.write("\tdata.addColumn('number', 'Matchs');\n");
            fileObj.write("\tdata.addRows([\n");
            text.sprintf("\t[ 'Matched 1st Choice', %d  ],\n",state->top1Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t[ 'Matched 2nd Choice', %d  ],\n",state->top2Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t[ 'Matched 3rd Choice', %d  ],\n",state->top3Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t[ 'Matched 4th Choice', %d  ],\n",state->top4Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t[ 'Matched 5th Choice', %d  ]]);\n",state->top5Count);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\tvar options = { title:'Image Matches', width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.PieChart(document.getElementById('result_chart_div'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // Cummulative Success/Failure
            fileObj.write("\tgoogle.charts.load('current', {packages: ['corechart', 'line']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawPassFailGraph);\n");
            fileObj.write("\tfunction drawPassFailGraph() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('number', 'X');\n");
            fileObj.write("\tdata.addColumn('number', 'Match');\n");
            fileObj.write("\tdata.addColumn('number', 'Mismatch');\n");
            fileObj.write("\tdata.addRows([\n");
            fileObj.write("\t[1, 0, 0],\n");
            float fVal=0.99;
            float sumPass = 0, sumFail = 0;
            for(int i = 99; i >= 0; i--){
                sumPass = sumPass + state->topKPassFail[i][0];
                sumFail = sumFail + state->topKPassFail[i][1];
                if(i == 0){
                    text.sprintf("\t[%.2f,   %.4f,    %.4f]\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                else{
                    text.sprintf("\t[%.2f,   %.4f,    %.4f],\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                fVal=fVal-0.01;
            }
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = {  title:'Cummulative Success/Failure', hAxis: { title: 'Confidence', direction: '-1' }, vAxis: {title: 'Percentage of Dataset'}, series: { 0.01: {curveType: 'function'} }, width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.LineChart(document.getElementById('pass_fail_chart'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // Cummulative L1 Success/Failure
            fileObj.write("\tgoogle.charts.load('current', {packages: ['corechart', 'line']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawL1PassFailGraph);\n");
            fileObj.write("\tfunction drawL1PassFailGraph() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('number', 'X');\n");
            fileObj.write("\tdata.addColumn('number', 'L1 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L1 Mismatch');\n");
            fileObj.write("\tdata.addRows([\n");
            fileObj.write("\t[1, 0, 0],\n");
            fVal=0.99;
            sumPass = 0;
            sumFail = 0;
            for(int i = 99; i >= 0; i--){
                sumPass = sumPass + state->topKHierarchyPassFail[i][0];
                sumFail = sumFail + state->topKHierarchyPassFail[i][1];
                if(i == 0){
                    text.sprintf("\t[%.2f,   %.4f,    %.4f]\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                else{
                    text.sprintf("\t[%.2f,   %.4f,    %.4f],\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                fVal=fVal-0.01;
            }
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = {  title:'Cummulative L1 Success/Failure', hAxis: { title: 'Confidence', direction: '-1' }, vAxis: {title: 'Percentage of Dataset'}, series: { 0.01: {curveType: 'function'} }, width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.LineChart(document.getElementById('L1_pass_fail_chart'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // Cummulative L2 Success/Failure
            fileObj.write("\tgoogle.charts.load('current', {packages: ['corechart', 'line']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawL2PassFailGraph);\n");
            fileObj.write("\tfunction drawL2PassFailGraph() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('number', 'X');\n");
            fileObj.write("\tdata.addColumn('number', 'L2 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L2 Mismatch');\n");
            fileObj.write("\tdata.addRows([\n");
            fileObj.write("\t[1, 0, 0],\n");
            fVal=0.99;
            sumPass = 0;
            sumFail = 0;
            for(int i = 99; i >= 0; i--){
                sumPass = sumPass + state->topKHierarchyPassFail[i][2];
                sumFail = sumFail + state->topKHierarchyPassFail[i][3];
                if(i == 0){
                    text.sprintf("\t[%.2f,   %.4f,    %.4f]\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                else{
                    text.sprintf("\t[%.2f,   %.4f,    %.4f],\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                fVal=fVal-0.01;
            }
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = {  title:'Cummulative L2 Success/Failure', hAxis: { title: 'Confidence', direction: '-1' }, vAxis: {title: 'Percentage of Dataset'}, series: { 0.01: {curveType: 'function'} }, width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.LineChart(document.getElementById('L2_pass_fail_chart'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // Cummulative L3 Success/Failure
            fileObj.write("\tgoogle.charts.load('current', {packages: ['corechart', 'line']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawL3PassFailGraph);\n");
            fileObj.write("\tfunction drawL3PassFailGraph() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('number', 'X');\n");
            fileObj.write("\tdata.addColumn('number', 'L3 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L3 Mismatch');\n");
            fileObj.write("\tdata.addRows([\n");
            fileObj.write("\t[1, 0, 0],\n");
            fVal=0.99;
            sumPass = 0;
            sumFail = 0;
            for(int i = 99; i >= 0; i--){
                sumPass = sumPass + state->topKHierarchyPassFail[i][4];
                sumFail = sumFail + state->topKHierarchyPassFail[i][5];
                if(i == 0){
                    text.sprintf("\t[%.2f,   %.4f,    %.4f]\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                else{
                    text.sprintf("\t[%.2f,   %.4f,    %.4f],\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                fVal=fVal-0.01;
            }
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = {  title:'Cummulative L3 Success/Failure', hAxis: { title: 'Confidence', direction: '-1' }, vAxis: {title: 'Percentage of Dataset'}, series: { 0.01: {curveType: 'function'} }, width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.LineChart(document.getElementById('L3_pass_fail_chart'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // Cummulative L4 Success/Failure
            fileObj.write("\tgoogle.charts.load('current', {packages: ['corechart', 'line']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawL4PassFailGraph);\n");
            fileObj.write("\tfunction drawL4PassFailGraph() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('number', 'X');\n");
            fileObj.write("\tdata.addColumn('number', 'L4 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L4 Mismatch');\n");
            fileObj.write("\tdata.addRows([\n");
            fileObj.write("\t[1, 0, 0],\n");
            fVal=0.99;
            sumPass = 0;
            sumFail = 0;
            for(int i = 99; i >= 0; i--){
                sumPass = sumPass + state->topKHierarchyPassFail[i][6];
                sumFail = sumFail + state->topKHierarchyPassFail[i][7];
                if(i == 0){
                    text.sprintf("\t[%.2f,   %.4f,    %.4f]\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                else{
                    text.sprintf("\t[%.2f,   %.4f,    %.4f],\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                fVal=fVal-0.01;
            }
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = {  title:'Cummulative L4 Success/Failure', hAxis: { title: 'Confidence', direction: '-1' }, vAxis: {title: 'Percentage of Dataset'}, series: { 0.01: {curveType: 'function'} }, width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.LineChart(document.getElementById('L4_pass_fail_chart'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // Cummulative L5 Success/Failure
            fileObj.write("\tgoogle.charts.load('current', {packages: ['corechart', 'line']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawL5PassFailGraph);\n");
            fileObj.write("\tfunction drawL5PassFailGraph() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('number', 'X');\n");
            fileObj.write("\tdata.addColumn('number', 'L5 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L5 Mismatch');\n");
            fileObj.write("\tdata.addRows([\n");
            fileObj.write("\t[1, 0, 0],\n");
            fVal=0.99;
            sumPass = 0;
            sumFail = 0;
            for(int i = 99; i >= 0; i--){
                sumPass = sumPass + state->topKHierarchyPassFail[i][8];
                sumFail = sumFail + state->topKHierarchyPassFail[i][9];
                if(i == 0){
                    text.sprintf("\t[%.2f,   %.4f,    %.4f]\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                else{
                    text.sprintf("\t[%.2f,   %.4f,    %.4f],\n",fVal,(sumPass/netSummaryImages),(sumFail/netSummaryImages));
                    fileObj.write(text.toStdString().c_str());
                }
                fVal=fVal-0.01;
            }
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = {  title:'Cummulative L5 Success/Failure', hAxis: { title: 'Confidence', direction: '-1' }, vAxis: {title: 'Percentage of Dataset'}, series: { 0.01: {curveType: 'function'} }, width:750, height:400 };\n");
            fileObj.write("\tvar chart = new google.visualization.LineChart(document.getElementById('L5_pass_fail_chart'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            // Cummulative Hierarchy Success/Failure
            fileObj.write("\tgoogle.charts.load('current', {packages: ['corechart', 'line']});\n");
            fileObj.write("\tgoogle.charts.setOnLoadCallback(drawHierarchyPassFailGraph);\n");
            fileObj.write("\tfunction drawHierarchyPassFailGraph() {\n");
            fileObj.write("\tvar data = new google.visualization.DataTable();\n");
            fileObj.write("\tdata.addColumn('number', 'X');\n");
            fileObj.write("\tdata.addColumn('number', 'L1 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L1 Mismatch');\n");
            fileObj.write("\tdata.addColumn('number', 'L2 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L2 Mismatch');\n");
            fileObj.write("\tdata.addColumn('number', 'L3 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L3 Mismatch');\n");
            fileObj.write("\tdata.addColumn('number', 'L4 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L4 Mismatch');\n");
            fileObj.write("\tdata.addColumn('number', 'L5 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L5 Mismatch');\n");
            fileObj.write("\tdata.addColumn('number', 'L6 Match');\n");
            fileObj.write("\tdata.addColumn('number', 'L6 Mismatch');\n");
            fileObj.write("\tdata.addRows([\n");
            fileObj.write("\t[1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,],\n");
            fVal=0.99;
            float l1Pass = 0, l1Fail = 0;
            float l2Pass = 0, l2Fail = 0;
            float l3Pass = 0, l3Fail = 0;
            float l4Pass = 0, l4Fail = 0;
            float l5Pass = 0, l5Fail = 0;
            float l6Pass = 0, l6Fail = 0;
            for(int i = 99; i >= 0; i--){
                l1Pass = l1Pass + state->topKHierarchyPassFail[i][0];
                l1Fail = l1Fail + state->topKHierarchyPassFail[i][1];
                l2Pass = l2Pass + state->topKHierarchyPassFail[i][2];
                l2Fail = l2Fail + state->topKHierarchyPassFail[i][3];
                l3Pass = l3Pass + state->topKHierarchyPassFail[i][4];
                l3Fail = l3Fail + state->topKHierarchyPassFail[i][5];
                l4Pass = l4Pass + state->topKHierarchyPassFail[i][6];
                l4Fail = l4Fail + state->topKHierarchyPassFail[i][7];
                l5Pass = l5Pass + state->topKHierarchyPassFail[i][8];
                l5Fail = l5Fail + state->topKHierarchyPassFail[i][9];
                l6Pass = l6Pass + state->topKHierarchyPassFail[i][10];
                l6Fail = l6Fail + state->topKHierarchyPassFail[i][11];
                if(i == 0){
                    text.sprintf("\t[%.2f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f]\n",fVal,
                                 (l1Pass/netSummaryImages),(l1Fail/netSummaryImages),
                                 (l2Pass/netSummaryImages),(l2Fail/netSummaryImages),
                                 (l3Pass/netSummaryImages),(l3Fail/netSummaryImages),
                                 (l4Pass/netSummaryImages),(l4Fail/netSummaryImages),
                                 (l5Pass/netSummaryImages),(l5Fail/netSummaryImages),
                                 (l6Pass/netSummaryImages),(l6Fail/netSummaryImages)
                                 );
                    fileObj.write(text.toStdString().c_str());
                }
                else{
                    text.sprintf("\t[%.2f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f,   %.4f,    %.4f],\n",fVal,
                                 (l1Pass/netSummaryImages),(l1Fail/netSummaryImages),
                                 (l2Pass/netSummaryImages),(l2Fail/netSummaryImages),
                                 (l3Pass/netSummaryImages),(l3Fail/netSummaryImages),
                                 (l4Pass/netSummaryImages),(l4Fail/netSummaryImages),
                                 (l5Pass/netSummaryImages),(l5Fail/netSummaryImages),
                                 (l6Pass/netSummaryImages),(l6Fail/netSummaryImages)
                                 );
                    fileObj.write(text.toStdString().c_str());
                }
                fVal=fVal-0.01;
            }
            fileObj.write("\t]);\n");
            fileObj.write("\tvar options = {  title:'Cummulative Hierarchy Levels Success/Failure', hAxis: { title: 'Confidence', direction: '-1' }, vAxis: {title: 'Percentage of Dataset'}, series: { 0.01: {curveType: 'function'} }, width:1400, height:800 };\n");
            fileObj.write("\tvar chart = new google.visualization.LineChart(document.getElementById('Hierarchy_pass_fail_chart'));\n");
            fileObj.write("\tchart.draw(data, options);}\n");
            fileObj.write("\t\n");
            fileObj.write("\t</script>\n");

            // Top view header
            fileObj.write("\t<div class=\"navbar\">\n");
            fileObj.write("\t<a href=\"#\">\n");
            fileObj.write("\t<div id=\"main\">\n");
            fileObj.write("\t<span style=\"font-size:30px;cursor:pointer\" onclick=\"openNav()\">&#9776; Views</span>\n");
            fileObj.write("\t</div></a>\n");
            if(exportTool){
                fileObj.write("\t<a href=\"https://www.amd.com/en\" target=\"_blank\">\n");
                fileObj.write("\t<img \" src=\"icons/small_amd_logo.png\" alt=\"AMD\" /></a>\n");
                fileObj.write("\t<a href=\"https://gpuopen.com/\" target=\"_blank\">\n");
                fileObj.write("\t<img \" src=\"icons/small_radeon_logo.png\" alt=\"GPUopen\" /></a>\n");
                fileObj.write("\t<a href=\"https://github.com/GPUOpen-ProfessionalCompute-Libraries/amdovx-modules#amd-openvx-modules-amdovx-modules\" target=\"_blank\">\n");
                fileObj.write("\t<img \" src=\"icons/small_github_logo.png\" alt=\"AMD GitHub\" /></a>\n");
                fileObj.write("\t<img \" src=\"icons/ADAT_500x100.png\" alt=\"AMD Inference ToolKit\" hspace=\"100\" height=\"90\"/> \n");
            }
            fileObj.write("\t</div>\n");
            fileObj.write("\t\n");

            // Overall Summary
            fileObj.write("\t<!-- Overall Summary -->\n");
            float passProb = (state->top1TotProb+state->top2TotProb+state->top3TotProb+state->top4TotProb+state->top5TotProb);
            float avgPassProb = passProb/passCount;

            text.sprintf("\t<A NAME=\"table0\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>%s Overall Summary</em></font></h1></A>\n",state->modelName.toStdString().c_str());
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t<table align=\"center\">\n");
            fileObj.write("\t<col width=\"265\">\n");
            fileObj.write("\t<col width=\"50\">\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t<td><font color=\"black\" size=\"4\">Images <b>With Ground Truth</b></font></td>\n");
            text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\" onclick=\"findImagesWithGroundTruthLabel()\"><b>%d</b></font></td>\n",netSummaryImages);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t</tr>\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t<td><font color=\"black\" size=\"4\">Images <b>Without Ground Truth</b></font></td>\n");
            text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\" onclick=\"findImagesWithNoGroundTruthLabel()\"><b>%d</b></font></td>\n",state->totalNoGroundTruth);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t</tr>\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t<td><font color=\"black\" size=\"4\"><b>Total Images</b></font></td>\n");
            text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\" onclick=\"clearResultFilter();goToImageResults();\"><b>%d</b></font></td>\n",state->imageDataSize);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t</tr>\n");
            fileObj.write("\t</table>\n<br><br><br>\n");
            fileObj.write("\t<table align=\"center\">\n \t<col width=\"300\">\n \t<col width=\"100\">\n \t<col width=\"350\">\n \t<col width=\"100\">\n<tr>\n");
            text.sprintf("\t<td><font color=\"black\" size=\"4\">Total <b>Top %d Match</b></font></td>\n\n",state->topKValue);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t <td align=\"center\"><font color=\"black\" size=\"4\" onclick=\"findTopKMatch()\"><b>%d</b></font></td>\n",passCount);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t<td><font color=\"black\" size=\"4\">Total <b>Mismatch</b></font></td>\n");
            text.sprintf("\t <td align=\"center\"><font color=\"black\" size=\"4\" onclick=\"findImageMisMatch()\"><b>%d</b></font></td>\n",state->totalMismatch);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t</tr>\n<tr>\n");
            text.sprintf("\t<td><font color=\"black\" size=\"4\"><b>Accuracy on Top %d</b></font></td>\n",state->topKValue);
            fileObj.write(text.toStdString().c_str());
            float accuracyPer = ((float)passCount / netSummaryImages);
            text.sprintf("\t <td align=\"center\"><font color=\"black\" size=\"4\"><b>%.2f %%</b></font></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t<td><font color=\"black\" size=\"4\"><b>Mismatch Percentage</b></font></td>\n");
            accuracyPer = ((float)state->totalMismatch/netSummaryImages);
            text.sprintf("\t <td align=\"center\"><font color=\"black\" size=\"4\"><b>%.2f %%</b></font></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t</tr>\n<tr>\n");
            text.sprintf("\t<td><font color=\"black\" size=\"4\">Average Pass Confidence for Top %d</font></td>\n",state->topKValue);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t <td align=\"center\"><font color=\"black\" size=\"4\"><b>%.2f %%</b></font></td>\n",(avgPassProb*100));
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t<td><font color=\"black\" size=\"4\">Average mismatch Confidence for Top 1</font></td>\n");
            text.sprintf("\t <td align=\"center\"><font color=\"black\" size=\"4\"><b>%.2f %%</b></font></td>\n",((state->totalFailProb/state->totalMismatch)*100));
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t</tr>\n</table>\n<br><br><br>\n");
            // topK result
            fileObj.write("\t<table align=\"center\" style=\"width: 40%\">\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"3\"><b>1st Match</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"3\"><b>2nd Match</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"3\"><b>3rd Match</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"3\"><b>4th Match</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"3\"><b>5th Match</b></font></td>\n");
            fileObj.write("\t\t</tr>\n");
            fileObj.write("\t<tr>\n");
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%d</b></font></td>\n",state->top1Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%d</b></font></td>\n",state->top2Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%d</b></font></td>\n",state->top3Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%d</b></font></td>\n",state->top4Count);
            fileObj.write(text.toStdString().c_str());
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%d</b></font></td>\n",state->top5Count);
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t\t</tr>\n");
            fileObj.write("\t<tr>\n");
            accuracyPer = ((float)state->top1Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%.2f %%</b></font></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top2Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%.2f %%</b></font></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top3Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%.2f %%</b></font></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top4Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%.2f %%</b></font></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            accuracyPer = ((float)state->top5Count/netSummaryImages);
            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>%.2f %%</b></font></td>\n",(accuracyPer*100));
            fileObj.write(text.toStdString().c_str());
            fileObj.write("\t\t</tr>\n");
            fileObj.write("</table>\n");

            // summary date and time
            QDateTime curtim = QDateTime::currentDateTime();
            QString abbr = curtim.timeZoneAbbreviation();
            //printf("TimeZone: %s",abbr.toStdString().c_str());
            //QString DateTime = QDate::currentDate().toString();//QDateTime::currentMSecsSinceEpoch();
            //text.sprintf("\t<h1 align=\"center\"><font color=\"DodgerBlue\" size=\"4\"><br><em>Summary Generated On: </font><font color=\"black\" size=\"4\">%s</font></em></h1>\n",DateTime.toStdString().c_str());
            //fileObj.write(text.toStdString().c_str());
            const QDateTime now = QDateTime::currentDateTime();
            QString DateTime_test = now.toString("yyyy-MM-dd hh:mm:ss");
            text.sprintf("\t<h1 align=\"center\"><font color=\"DodgerBlue\" size=\"4\"><br><em>Summary Generated On: </font><font color=\"black\" size=\"4\">%s %s</font></em></h1>\n",DateTime_test.toStdString().c_str(),abbr.toStdString().c_str());
            fileObj.write(text.toStdString().c_str());

            // Graph
            fileObj.write("\t<!-- Graph Summary -->\n");
            fileObj.write("<A NAME=\"table1\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>Graphs</em></font></h1></A>\n");
            fileObj.write("\t<center><div id=\"Model_Stats\" style=\"border: 1px solid #ccc\"></div></center>\n");
            fileObj.write("\t<table align=\"center\" style=\"width: 90%\">\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t <td><center><div id=\"result_chart_div\" style=\"border: 0px solid #ccc\"></div></center></td>\n");
            fileObj.write("\t <td><center><div id=\"topK_result_chart_div\" style=\"border: 0px solid #ccc\"></div></center></td>\n");
            fileObj.write("\t</tr>\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t <td><center><div id=\"pass_fail_chart\" style=\"border: 0px solid #ccc\" ></div></center> </td>\n");
            fileObj.write("\t <td><center><div id=\"L1_pass_fail_chart\" style=\"border: 0px solid #ccc\" ></div></center> </td>\n");
            fileObj.write("\t</tr>\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t <td><center><div id=\"L2_pass_fail_chart\" style=\"border: 0px solid #ccc\" ></div></center> </td>\n");
            fileObj.write("\t <td><center><div id=\"L3_pass_fail_chart\" style=\"border: 0px solid #ccc\" ></div></center> </td>\n");
            fileObj.write("\t</tr>\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t <td><center><div id=\"L4_pass_fail_chart\" style=\"border: 0px solid #ccc\" ></div></center> </td>\n");
            fileObj.write("\t <td><center><div id=\"L5_pass_fail_chart\" style=\"border: 0px solid #ccc\" ></div></center> </td>\n");
            fileObj.write("\t</tr>\n");
            fileObj.write("\t</table>\n");
            fileObj.write("\t\n");
            fileObj.write("\t <td><center><div id=\"Hierarchy_pass_fail_chart\" style=\"border: 0px solid #ccc\" ></div></center> </td>\n");
            fileObj.write("\t\n");

            // hierarchy
            fileObj.write("\t<!-- hierarchy Summary -->\n");
            fileObj.write("<A NAME=\"table2\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>Hierarchy Summary (by Confidence level)</em></font></h1></A>\n");
            fileObj.write("\t<table align=\"center\" style=\"width: 80%\">\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Confidence</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Pass</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Fail</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 1 Pass</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 1 Fail</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 2 Pass</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 2 Fail</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 3 Pass</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 3 Fail</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 4 Pass</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 4 Fail</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 5 Pass</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 5 Fail</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 6 Pass</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Category 6 Fail</b></font></td>\n");
            fileObj.write("\t\t</tr>\n");

            float f=0.99;
            for(int i = 99; i >= 0; i--){
                fileObj.write("\t\t<tr>\n");
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%.2f</b></font></td>\n",f);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKPassFail[i][0]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKPassFail[i][1]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][0]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][1]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][2]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][3]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][4]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][5]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][6]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][7]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][8]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][9]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][10]);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topKHierarchyPassFail[i][11]);
                fileObj.write(text.toStdString().c_str());
                fileObj.write("\t\t</tr>\n");
                f=f-0.01;
               }
            fileObj.write("</table>\n");

            // label
            fileObj.write("\t<!-- Label Summary -->\n");
            fileObj.write("<A NAME=\"table3\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>Label Summary (stats per image class)</em></font></h1></A>\n");
            fileObj.write("\t\t<table id=\"filterLabelTable\" align=\"center\" cellspacing=\"2\" border=\"0\" style=\"width: 70%\">\n");
            fileObj.write("\t\t<tr>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Label ID\" onkeyup=\"filterLabelTable(0,id)\" placeholder=\"Label ID\" title=\"Label ID\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Label Description\" onkeyup=\"filterLabelTable(1,id)\" placeholder=\"Label Description\" title=\"Label Description\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Images in DataBase\" onkeyup=\"filterLabelTable(2,id)\" placeholder=\"Images in DataBase\" title=\"Images in DataBase\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Matched Top1 %\" onkeyup=\"filterLabelTable(3,id)\" placeholder=\"Matched Top1 %\" title=\"Matched Top1 %\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Matched Top5 %\" onkeyup=\"filterLabelTable(4,id)\" placeholder=\"Matched Top5 %\" title=\"Matched Top5 %\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Matched 1st\" onkeyup=\"filterLabelTable(5,id)\" placeholder=\"Matched 1st\" title=\"Matched 1st\"></td>\n");
            fileObj.write("\t\t</tr>\n\t\t<tr>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Matched 2nd\" onkeyup=\"filterLabelTable(6,id)\" placeholder=\"Matched 2nd\" title=\"Matched 2nd\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Matched 3th\" onkeyup=\"filterLabelTable(7,id)\" placeholder=\"Matched 3th\" title=\"Matched 3th\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Matched 4th\" onkeyup=\"filterLabelTable(8,id)\" placeholder=\"Matched 4th\" title=\"Matched 4th\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Matched 5th\" onkeyup=\"filterLabelTable(9,id)\" placeholder=\"Matched 5th\" title=\"Matched 5th\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"14\" id=\"Misclassified Top1 Label\" onkeyup=\"filterLabelTable(10,id)\"placeholder=\"Misclassified Top1 Label\" title=\"Misclassified Top1 Label\"></td>\n");
            fileObj.write("\t\t<td align=\"center\"><button style=\"background-color:yellow;\" onclick=\"clearLabelFilter()\">Clear Filter</button></td>\n");
            fileObj.write("\t\t</tr>\n");
            fileObj.write("\t\t</table>\n");
            fileObj.write("\t\t<br>\n");
            fileObj.write("\t\t\n");
            if(state->topKValue > 4){
                fileObj.write("\t<table class=\"sortable\" id=\"labelsTable\" align=\"center\">\n");
                fileObj.write("\t<col width=\"80\">\n");
                fileObj.write("\t<col width=\"200\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"100\">\n");
                fileObj.write("\t<col width=\"150\">\n");
                fileObj.write("\t<col width=\"60\">\n");

                fileObj.write("\t<tr>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Label ID</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Label Description</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Images in DataBase</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Matched Top1 %</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Matched Top5 %</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Matched 1st</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Matched 2nd</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Matched 3rd</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Matched 4th</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"maroon\" size=\"3\"><b>Matched 5th</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"blue\" size=\"3\"><b>Misclassified Top1 Label</b></font></td>\n");
                fileObj.write("\t<td align=\"center\"><font color=\"black\" size=\"3\"><b>Check</b></font></td>\n");
                fileObj.write("\t\t</tr>\n");
                int totalLabelsFound = 0, totalImagesWithLabelFound = 0;
                int totalLabelsUnfounded = 0, totalImagesWithLabelNotFound = 0;
                int totalLabelsNeverfound = 0, totalImagesWithFalseLabelFound = 0;
                for(int i = 0; i < 1000; i++){
                    if(state->topLabelMatch[i][0] || state->topLabelMatch[i][6]){
                        QString labelTxt = state->dataLabels ? (*state->dataLabels)[i] : "Unknown";
                        labelTxt = labelTxt.replace(QRegExp("n[0-9]{8}"),"");
                        labelTxt = labelTxt.toLower();
                        fileObj.write("\t<tr>\n");
                        text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\" onclick=\"findGroundTruthLabel('%s',%d)\"><b>%d</b></font></td>\n",labelTxt.toStdString().c_str(),i,i);
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td align=\"left\" onclick=\"findGroundTruthLabel('%s',%d)\"><b>%s</b></td>\n",labelTxt.toStdString().c_str(),i,labelTxt.toStdString().c_str());
                        fileObj.write(text.toStdString().c_str());
                        if(state->topLabelMatch[i][0]){
                            float top1 = 0, top5 = 0;
                            top1 = float(state->topLabelMatch[i][1]);
                            top1 = ((top1/state->topLabelMatch[i][0])*100);
                            top5 = float(state->topLabelMatch[i][1]+state->topLabelMatch[i][2]+state->topLabelMatch[i][3]+state->topLabelMatch[i][4]+state->topLabelMatch[i][5]);
                            top5 = (float(top5/state->topLabelMatch[i][0])*100);
                            int imagesFound = (state->topLabelMatch[i][1]+state->topLabelMatch[i][2]+state->topLabelMatch[i][3]+state->topLabelMatch[i][4]+state->topLabelMatch[i][5]);
                            totalImagesWithLabelFound += imagesFound;
                            totalImagesWithLabelNotFound += state->topLabelMatch[i][0] - imagesFound;
                            if(top5 == 100.00){
                                text.sprintf("\t\t<td align=\"center\"><font color=\"green\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][0]);
                                fileObj.write(text.toStdString().c_str());
                            }
                            else{
                                text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][0]);
                                fileObj.write(text.toStdString().c_str());
                            }
                            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%.2f</b></font></td>\n",top1);
                            fileObj.write(text.toStdString().c_str());
                            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%.2f</b></font></td>\n",top5);
                            fileObj.write(text.toStdString().c_str());
                            totalLabelsFound++;
                            if(top5 == 0.00) totalLabelsNeverfound++;
                        }
                        else{
                            text.sprintf("\t\t<td align=\"center\"><font color=\"red\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][0]);
                            fileObj.write(text.toStdString().c_str());
                            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>0.00</b></font></td>\n");
                            fileObj.write(text.toStdString().c_str());
                            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>0.00</b></font></td>\n");
                            fileObj.write(text.toStdString().c_str());
                            totalLabelsUnfounded++;
                        }
                        text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][1]);
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][2]);
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][3]);
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][4]);
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][5]);
                        fileObj.write(text.toStdString().c_str());
                        if(state->topLabelMatch[i][0] && state->topLabelMatch[i][6] ){
                            text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][6]);
                            fileObj.write(text.toStdString().c_str());
                        }
                        else{
                            if(state->topLabelMatch[i][0]){
                                text.sprintf("\t\t<td align=\"center\"><font color=\"green\" size=\"2\"><b>%d</b></font></td>\n",state->topLabelMatch[i][6]);
                                fileObj.write(text.toStdString().c_str());
                            }
                            else{
                                totalImagesWithFalseLabelFound += state->topLabelMatch[i][6];
                                text.sprintf("\t\t<td align=\"center\"><font color=\"red\" size=\"2\" onclick=\"findMisclassifiedGroundTruthLabel('%s')\"><b>%d</b></font></td>\n",labelTxt.toStdString().c_str(),state->topLabelMatch[i][6]);
                                fileObj.write(text.toStdString().c_str());
                            }
                        }
                        text.sprintf("\t\t<td align=\"center\"><input id=\"id_%d\" name=\"id[%d]\" type=\"checkbox\" value=\"%d\" onClick=\"highlightRow(this);\"></td>\n",i,i,i);
                        fileObj.write(text.toStdString().c_str());
                        fileObj.write("\t\t</tr>\n");
                    }
                }
                fileObj.write("</table>\n");
                fileObj.write("<h1 align=\"center\"><font color=\"DodgerBlue\" size=\"4\"><br><em>Label Summary</em></font></h1>\n");
                fileObj.write("\t<table align=\"center\">\n");
                fileObj.write("\t<col width=\"350\">\n");
                fileObj.write("\t<col width=\"50\">\n");
                fileObj.write("\t<col width=\"150\">\n");
                fileObj.write("\t<tr>\n");
                fileObj.write("\t<td><font color=\"black\" size=\"4\">Labels in Ground Truth <b>found</b></font></td>\n");
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b></font></td>\n",(totalLabelsFound-totalLabelsNeverfound));
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b> images</font></td>\n",totalImagesWithLabelFound);
                fileObj.write(text.toStdString().c_str());
                fileObj.write("\t</tr>\n");
                fileObj.write("\t<tr>\n");
                fileObj.write("\t<td><font color=\"black\" size=\"4\">Labels in Ground Truth <b>not found</b></font></td>\n");
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b></font></td>\n",totalLabelsNeverfound);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b> images</font></td>\n",totalImagesWithLabelNotFound);
                fileObj.write(text.toStdString().c_str());
                fileObj.write("\t</tr>\n");
                fileObj.write("\t<tr>\n");
                fileObj.write("\t<td><font color=\"black\" size=\"4\"><b>Total</b> Labels in Ground Truth</font></td>\n");
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b></font></td>\n",totalLabelsFound);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b> images</font></td>\n",(totalImagesWithLabelFound+totalImagesWithLabelNotFound));
                fileObj.write(text.toStdString().c_str());
                fileObj.write("\t</tr>\n");
                fileObj.write("</table>\n");
                fileObj.write("\t<br><br><table align=\"center\">\n");
                fileObj.write("\t<col width=\"400\">\n");
                fileObj.write("\t<col width=\"50\">\n");
                fileObj.write("\t<col width=\"150\">\n");
                fileObj.write("\t<tr>\n");
                fileObj.write("\t<td><font color=\"black\" size=\"4\">Labels <b>not in Ground Truth</b> found in 1st Match</font></td>\n");
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b></font></td>\n",totalLabelsUnfounded);
                fileObj.write(text.toStdString().c_str());
                text.sprintf("\t<td align=\"center\"><font color=\"black\" size=\"4\"><b>%d</b> images</font></td>\n",totalImagesWithFalseLabelFound);
                fileObj.write(text.toStdString().c_str());
                fileObj.write("\t</tr>\n");
                fileObj.write("</table>\n");
            }

            // Image result
            fileObj.write("\t<!-- Image Summary -->\n");
            fileObj.write("<A NAME=\"table4\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>Image Results</em></font></h1></A>\n");
            fileObj.write("\t\t<table id=\"filterTable\" align=\"center\" cellspacing=\"2\" border=\"0\" style=\"width: 60%\">\n");
            fileObj.write("\t\t<tr>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"GroundTruthText\" onkeyup=\"filterResultTable(2,id)\" placeholder=\"Ground Truth Text\" title=\"Ground Truth Text\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"GroundTruthID\" onkeyup=\"filterResultTable(3,id)\" placeholder=\"Ground Truth ID\" title=\"Ground Truth ID\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" maxlength=\"2\" id=\"Matched\" onkeyup=\"filterResultTable(9,id)\" placeholder=\"Matched\" title=\"Type in a name\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top1\" onkeyup=\"filterResultTable(4,id)\" placeholder=\"1st Match\" title=\"1st Match\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top1Prob\" onkeyup=\"filterResultTable(15,id)\" placeholder=\"1st Match Conf\" title=\"1st Match Prob\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Text1\" onkeyup=\"filterResultTable(10,id)\" placeholder=\"Text 1\" title=\"Text1\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top2\" onkeyup=\"filterResultTable(5,id)\" placeholder=\"2nd Match\" title=\"2nd Match\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top2Prob\" onkeyup=\"filterResultTable(16,id)\" placeholder=\"2nd Match Conf\" title=\"2nd Match Prob\"></td>\n");
            fileObj.write("\t\t</tr>\n\t\t<tr>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top3\" onkeyup=\"filterResultTable(6,id)\" placeholder=\"3rd Match\" title=\"3rd Match\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top3Prob\" onkeyup=\"filterResultTable(17,id)\" placeholder=\"3rd Match Conf\" title=\"3rd Match Prob\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top4\" onkeyup=\"filterResultTable(7,id)\" placeholder=\"4th Match\" title=\"4th Match\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top4Prob\" onkeyup=\"filterResultTable(18,id)\" placeholder=\"4th Match Conf\" title=\"4th Match Prob\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top5\" onkeyup=\"filterResultTable(8,id)\" placeholder=\"5th Match\" title=\"5th Match\"></td>\n");
            fileObj.write("\t\t<td><input type=\"text\" size=\"10\" id=\"Top5Prob\" onkeyup=\"filterResultTable(19,id)\" placeholder=\"5th Match Conf\" title=\"5th Match Prob\"></td>\n");
            fileObj.write("\t\t<td></td>\n");
            fileObj.write("\t\t<td align=\"center\"><button style=\"background-color:yellow;\" onclick=\"clearResultFilter()\">Clear Filter</button></td>\n");
            fileObj.write("\t\t</tr>\n");
            fileObj.write("\t\t<tr>\n");
            fileObj.write("\t\t<td align=\"center\"><button style=\"background-color:salmon;\" onclick=\"notResultFilter()\">Not Filter</button></td>\n");
            fileObj.write("\t\t<td align=\"center\"><button style=\"background-color:salmon;\" onclick=\"andResultFilter()\">AND Filter</button></td>\n");
            fileObj.write("\t\t<td align=\"center\"><button style=\"background-color:salmon;\" onclick=\"orResultFilter()\">OR Filter</button></td>\n");
            fileObj.write("\t\t</tr>\n");
            fileObj.write("\t\t</table>\n");
            fileObj.write("\t\t<br>\n");
            fileObj.write("\t\t\n");
            fileObj.write("<table id=\"resultsTable\" class=\"sortable\" align=\"center\" cellspacing=\"2\" border=\"0\" style=\"width: 98%\">\n");
            fileObj.write("\t<tr>\n");
            fileObj.write("\t\t<td height=\"17\" align=\"center\"><font color=\"Maroon\" size=\"2\" ><b>Image</b></font></td>\n");
            fileObj.write("\t\t<td height=\"17\" align=\"center\"><font color=\"Maroon\" size=\"2\"><b>FileName</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Ground Truth Text</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b><div class=\"tooltip\"><font color=\"Maroon\" size=\"2\">Ground Truth</font><span class=\"tooltiptext\">Input Image Label. Click on the Text to Sort</span></div></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b><div class=\"tooltip\"><font color=\"Maroon\" size=\"2\">1st</font><span class=\"tooltiptext\">Result With Highest Confidence. Click on the Text to Sort</span></div></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>2nd</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>3rd</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>4th</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>5th</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b><div class=\"tooltip\"><font color=\"Maroon\" size=\"2\">Matched</font><span class=\"tooltiptext\">TopK Result Matched with Ground Truth. Click on the Text to Sort</span></div></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Text-1</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Text-2</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Text-3</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Text-4</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Text-5</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><b><div class=\"tooltip\"><font color=\"Maroon\" size=\"2\">Conf-1</font><span class=\"tooltiptext\">Confidence of the Top Match. Click on the Text to Sort</span></div></b></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Conf-2</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Conf-3</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Conf-4</b></font></td>\n");
            fileObj.write("\t\t<td align=\"center\"><font color=\"Maroon\" size=\"2\"><b>Conf-5</b></font></td>\n");
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
                labelTxt_1 = labelTxt_1.replace(QRegExp("n[0-9]{8}"),"");labelTxt_1 = labelTxt_1.toLower();
                labelTxt_2 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][1]] : "Unknown";
                labelTxt_2 = labelTxt_2.replace(QRegExp("n[0-9]{8}"),"");labelTxt_2 = labelTxt_2.toLower();
                labelTxt_3 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][2]] : "Unknown";
                labelTxt_3 = labelTxt_3.replace(QRegExp("n[0-9]{8}"),"");labelTxt_3 = labelTxt_3.toLower();
                labelTxt_4 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][3]] : "Unknown";
                labelTxt_4 = labelTxt_4.replace(QRegExp("n[0-9]{8}"),"");labelTxt_4 = labelTxt_4.toLower();
                labelTxt_5 = state->dataLabels ? (*state->dataLabels)[ state->resultImageLabelTopK[i][4]] : "Unknown";
                labelTxt_5 = labelTxt_5.replace(QRegExp("n[0-9]{8}"),"");labelTxt_5 = labelTxt_5.toLower();
                if(truth >= 0)
                {
                    if(truth == label_1) { match = 1; }
                    else if(truth == label_2) { match = 2; }
                    else if(truth == label_3) { match = 3; }
                    else if(truth == label_4) { match = 4; }
                    else if(truth == label_5) { match = 5; }
                    truthLabel = state->dataLabels ? (*state->dataLabels)[truth].toStdString().c_str() : "Unknown";
                    truthLabel = truthLabel.replace(QRegExp("n[0-9]{8}"),"");
                    truthLabel = truthLabel.toLower();
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
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",truthLabel.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",truth);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_5);
                    fileObj.write(text.toStdString().c_str());
                    if(match)
                        text.sprintf("\t\t<td align=\"center\"><font color=\"green\" size=\"2\"><b>%d</b></font></td>\n",match);
                    else
                        text.sprintf("\t\t<td align=\"center\"><font color=\"red\" size=\"2\"><b>%d</b></font></td>\n",match);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_1.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_2.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_3.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_4.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_5.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_5);
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
                    fileObj.write("\t\t<td align=\"left\"><b>unknown</b></td>\n");
                    fileObj.write("\t\t<td align=\"center\">-1</td>\n");
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%d</font></td>\n",label_5);
                    fileObj.write(text.toStdString().c_str());
                    fileObj.write("\t\t<td align=\"center\"><font color=\"blue\"><b>-1</b></font></td>\n");
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_1.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_2.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_3.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_4.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"left\">%s</td>\n",labelTxt_5.toStdString().c_str());
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_1);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_2);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_3);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_4);
                    fileObj.write(text.toStdString().c_str());
                    text.sprintf("\t\t<td align=\"center\"><font color=\"black\" size=\"2\">%.4f</font></td>\n",prob_5);
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
                fileObj.write("\t\tvar span = document.getElementsByClassName(\"modal\")[0];\n");
                fileObj.write("\t\tspan.onclick = function() { modal.style.display = \"none\"; }\n");
                fileObj.write("\t\t</script>\n");
                fileObj.write("\t\t\n");
            }
            fileObj.write("</table>\n");

            // Compare result summary
            fileObj.write("\t<!-- Compare ResultSummary -->\n");
            fileObj.write("<A NAME=\"table5\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>Compare Results Summary</em></font></h1></A>\n");

            if(true){
                QString SummaryFileName;
                QString FolderName = QDir::homePath();
                FolderName += "/.annAppCompare";
                struct stat st = {};
                if (stat(FolderName.toStdString().c_str(), &st) == -1) {
                    mkdir(FolderName.toStdString().c_str(), 0700);
                }
                QString ModelFolderName = FolderName; ModelFolderName +="/"; ModelFolderName += state->modelName.toStdString().c_str();
                if (stat(ModelFolderName.toStdString().c_str(), &st) == -1) {
                    mkdir(ModelFolderName.toStdString().c_str(), 0700);
                }
                SummaryFileName = FolderName; SummaryFileName += "/modelRunHistoryList.csv";
                // write summary details into csv
                if(SummaryFileName.size() > 0) {
                    QFileInfo check_file(SummaryFileName);
                    QFile fileSummaryObj(SummaryFileName);
                    if (check_file.exists() && check_file.isFile()){
                        QFile fileSummaryObj(SummaryFileName);
                        if(fileSummaryObj.open(QIODevice::Append)) {
                            text.sprintf("%s, %s, %d, %d, %d\n",state->modelName.toStdString().c_str(),state->dataFolder.toStdString().c_str(),state->imageDataSize, passCount, state->totalMismatch);
                            fileSummaryObj.write(text.toStdString().c_str());
                        }
                    }
                    else{
                        if(fileSummaryObj.open(QIODevice::WriteOnly)) {
                            fileSummaryObj.write("Model Name, Image DataBase, Number Of Images, Match, MisMatch\n");
                            text.sprintf("%s, %s, %d, %d, %d\n",state->modelName.toStdString().c_str(),state->dataFolder.toStdString().c_str(),state->imageDataSize, passCount, state->totalMismatch);
                            fileSummaryObj.write(text.toStdString().c_str());
                        }
                    }
                }
                // write into HTML
                FILE * fp = fopen(SummaryFileName.toStdString().c_str(), "r");
                if (fp == NULL) {
                    printf("ERROR::error while opening the file -- %s\n", SummaryFileName.toStdString().c_str());
                    return;
                }
                char modelLine[2048];
                char modelInfo[5][1024];
                ModelMasterInfo modelMaster[100];

                fileObj.write("\t<!-- Compare Graph Script -->\n");
                fileObj.write("\t<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n");
                fileObj.write("\t<script type=\"text/javascript\">\n");
                fileObj.write("\t\n");
                int lineNumber = 0;
                while (fgets(modelLine, 2048, fp) != NULL)
                {
                    int j = 0, ctr = 0;
                    for (int i = 0; i <= ((int)strlen(modelLine)); i++)
                    {
                        if (modelLine[i] == ',' || modelLine[i] == '\0')
                        {
                            modelInfo[ctr][j] = '\0';
                            ctr++;  //for next model info
                            j = 0;  //for next model info, init index to 0
                        }
                        else
                        {
                            modelInfo[ctr][j] = modelLine[i];
                            j++;
                        }
                        if (ctr > 5) {
                            printf("ERROR: model Info File ERROR count: %d\n", ctr);
                            return;
                        }
                    }
                    if (ctr != 5) {
                        printf("ERROR: model Info File ERROR -- count: %d\n", ctr);
                        return;
                    }
                    if(lineNumber){
                        int matchedImages, mismatchedImages;
                        QString modelName = modelInfo[0];
                        matchedImages = atoi(modelInfo[3]);
                        mismatchedImages = atoi(modelInfo[4]);

                        modelMaster[lineNumber].name = modelName;
                        modelMaster[lineNumber].matched = matchedImages;
                        modelMaster[lineNumber].mismatched = mismatchedImages;

                        fileObj.write("\tgoogle.charts.load('current', {'packages':['bar']});\n");
                        text.sprintf("\tgoogle.charts.setOnLoadCallback(drawChart_%d);\n",lineNumber);
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\tfunction drawChart_%d(){\n",lineNumber);
                        fileObj.write(text.toStdString().c_str());
                        fileObj.write("\tvar data = google.visualization.arrayToDataTable([\n");
                        fileObj.write("\t['  '     ,  'Match'  , 'Mismatch'],\n");
                        text.sprintf("\t['Summary',   %d     , %d        ]\n",matchedImages,mismatchedImages);
                        fileObj.write(text.toStdString().c_str());
                        fileObj.write("\t]);\n");
                        text.sprintf("\tvar options = { title: '%s Overall Result Summary', vAxis: { title: 'Images' }, width: 400, height: 400 };\n",modelName.toStdString().c_str());
                        fileObj.write(text.toStdString().c_str());
                        text.sprintf("\tvar chart = new google.charts.Bar(document.getElementById('Model_Stats_%d'));\n",lineNumber);
                        fileObj.write(text.toStdString().c_str());
                        fileObj.write("\tchart.draw(data, google.charts.Bar.convertOptions(options));}\n");
                        fileObj.write("\t\n");
                    }
                    lineNumber++;
                }
                fclose(fp);
                fileObj.write("\t\n");

                // draw combined graph
                fileObj.write("\tgoogle.charts.load('current', {'packages':['bar']});\n");
                fileObj.write("\tgoogle.charts.setOnLoadCallback(drawChart_master);\n");
                fileObj.write("\tfunction drawChart_master(){\n");
                fileObj.write("\tvar data = google.visualization.arrayToDataTable([\n");
                fileObj.write("\t['Model'   ,'Match',   'Mismatch'],\n");
                for(int i = 1; i < lineNumber; i++){
                    text.sprintf("\t['%s'   ,%d ,   %d]",modelMaster[i].name.toStdString().c_str(),modelMaster[i].matched, modelMaster[i].mismatched);
                    fileObj.write(text.toStdString().c_str());
                    if(i != (lineNumber-1)) fileObj.write(",\n"); else fileObj.write("\n");
                }
                fileObj.write("\t]);\n");
                fileObj.write("\tvar options = { title: 'Overall Result Summary', vAxis: { title: 'Images' }, width: 800, height: 600, bar: { groupWidth: '30%' }, isStacked: true , series: { 0:{color:'green'},1:{color:'Indianred'} }};\n");
                fileObj.write("\tvar chart = new google.charts.Bar(document.getElementById('Model_Stats_master'));\n");
                fileObj.write("\tchart.draw(data, google.charts.Bar.convertOptions(options));}\n");
                fileObj.write("\t\n");
                fileObj.write("\t</script>\n");

                // draw graph
                fileObj.write("\t\n");
                fileObj.write("\t\n");
                fileObj.write("\t<center><div id=\"Model_Stats_master\" style=\"border: 1px solid #ccc\"></div></center>\n");
                fileObj.write("\t\n");
                fileObj.write("\t<table align=\"center\" style=\"width: 90%\">\n");
                fileObj.write("\t<tr>\n");
                for(int i = 1; i < lineNumber; i++){
                    fileObj.write("\t\n");
                    text.sprintf("\t<td><center><div id=\"Model_Stats_%d\" style=\"border: 1px solid #ccc\"></div></center></td>\n",i);
                    fileObj.write(text.toStdString().c_str());
                    if(i%3 == 0){
                        fileObj.write("\t</tr>\n");
                        fileObj.write("\t<tr>\n");
                    }
                }
                fileObj.write("\t</tr>\n");
                fileObj.write("\t</table>\n");
            }

            // Error Suspects
            //fileObj.write("\t<!-- Error Suspects -->\n");
            //fileObj.write("<A NAME=\"table6\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>Error Suspects</em></font></h1></A>\n");
            //fileObj.write("\t\n");

            // HELP
            fileObj.write("\t<!-- HELP -->\n");
            fileObj.write("<A NAME=\"table7\"><h1 align=\"center\"><font color=\"DodgerBlue\" size=\"6\"><br><br><br><em>HELP</em></font></h1></A>\n");
            fileObj.write("\t\n");
            fileObj.write("\t<table align=\"center\" style=\"width: 50%\">\n");
            fileObj.write("\t<tr><td>\n");
            fileObj.write("\t<h1 align=\"center\">AMD Neural Net ToolKit</h1>\n");
            fileObj.write("\t</td></tr><tr><td>\n");
            fileObj.write("\t<p>AMD Neural Net ToolKit is a comprehensive set of help tools for neural net creation, development, training and\n");
            fileObj.write("\tdeployment. The ToolKit provides you with help tools to design, develop, quantize, prune, retrain, and infer your neural\n");
            fileObj.write("\tnetwork work in any framework. The ToolKit is designed help you deploy your work to any AMD or 3rd party hardware, from \n");
            fileObj.write("\tembedded to servers.</p>\n");
            fileObj.write("\t<p>AMD Neural Net ToolKit provides you with tools for accomplishing your tasks throughout the whole neural net life-cycle,\n");
            fileObj.write("\tfrom creating a model to deploying them for your target platforms.</p>\n");
            fileObj.write("\t<h2 >List of Features Available in this release</h2>\n");
            fileObj.write("\t<ul>\n");
            fileObj.write("\t<li>Overall Summary</li>\n");
            fileObj.write("\t<li>Graphs</li>\n");
            fileObj.write("\t<li>Hierarchy</li>\n");
            fileObj.write("\t<li>Labels</li>\n");
            fileObj.write("\t<li>Image Results</li>\n");
            fileObj.write("\t<li>Compare</li>\n");
            fileObj.write("\t<li>Help</li>\n");
            fileObj.write("\t</ul>\n");
            fileObj.write("\t<h3 >Overall Summary</h3>\n");
            fileObj.write("\t<p>This section summarizes the results for the current session, with information on the dataset and the model.\n");
            fileObj.write("\tThe section classifies the dataset into images with or without ground truth and only considers the images with ground truth \n");
            fileObj.write("\tfor analysis to avoid skewing the results.</p>\n");
            fileObj.write("\t<p>The summary calculates all the metrics to evaluate the current run session, helps evaluate the quality of the data set,\n");
            fileObj.write("\taccuracy of the current version of the model and links all the high level result to individual images to help the user to \n");
            fileObj.write("\tquickly analyze and detect if there are any problems.</p>\n");
            fileObj.write("\t<p>The summary also timestamps the results to avoid confusion with different iterations.</p>\n");
            fileObj.write("\t<h3>Graphs</h3>\n");
            fileObj.write("\t<p>The graph section allows the user to visualize the dataset and model accurately. The graphs can help detect any\n");
            fileObj.write("\tanomalies with the data or the model from a higher level. The graphs can be saved or shared with others.</p>\n");
            fileObj.write("\t<h3 >Hierarchy</h3>\n");
            fileObj.write("\t<p>This section has AMD proprietary hierarchical result analysis. Please contact us to get more information.</p>\n");
            fileObj.write("\t<h3 >Labels</h3>\n");
            fileObj.write("\t<p>Label section is the summary of all the classes the model has been trained to detect. The Label Summary presents the\n");
            fileObj.write("\thighlights of all the classes of images available in the database. The summary reports if the classes are found or not \n");
            fileObj.write("\tfound.</p>\n");
            fileObj.write("\t<p>Click on any of the label description and zoom into all the images from that class in the database.</p>\n");
            fileObj.write("\t<h3 >Image Results</h3>\n");
            fileObj.write("\t<p>The Image results has all the low level information about each of the individual images in the database. It reports on \n");
            fileObj.write("\tthe results obtained for the image in the session and allows quick view of the image.</p>\n");
            fileObj.write("\t<h3 >Compare</h3>\n");
            fileObj.write("\t<p>This section compares the results of a database or the model between different sessions. If the database was tested with\n");
            fileObj.write("\tdifferent models, this section reports and compares results among them.</p>\n");
            fileObj.write("\t</td></tr>\n");
            fileObj.write("\t</table>\n");
            fileObj.write("\t<br><br><br>\n");
            fileObj.write("\t\t<div class=\"footer\"> <p>©2018 Advanced Micro Devices, Inc</p></div>\n");
            fileObj.write("\t\n");
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
        // check perf button
        state->perfButtonPressed = false;
        if((x >= state->perfButtonRect.x()) &&
           (x < (state->perfButtonRect.x() + state->perfButtonRect.width())) &&
           (y >= state->perfButtonRect.y()) &&
           (y < (state->perfButtonRect.y() + state->perfButtonRect.height())))
        {
            state->perfButtonPressed = true;
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
        else if((x >= state->perfButtonRect.x()) &&
                (x < (state->perfButtonRect.x() + state->perfButtonRect.width())) &&
                (y >= state->perfButtonRect.y()) &&
                (y < (state->perfButtonRect.y() + state->perfButtonRect.height())))
        {
            //TBD Function
            showPerfResults();
        }
        state->exitButtonPressed = false;
        state->saveButtonPressed = false;
        state->perfButtonPressed = false;
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
    else if(event->key() == Qt::Key_P) {
        showPerfResults();
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
    QString perfButtonText = "Performance";
    QFontMetrics fontMetrics(state->statusBarFont);
    int buttonTextWidth = fontMetrics.width(perfButtonText);
    int statusBarX = 8 + 3*(10 + buttonTextWidth + 10 + 8), statusBarY = 8;
    int statusBarWidth = width() - 8 - statusBarX;
    int statusBarHeight = fontMetrics.height() + 16;
    int imageX = 8;
    int imageY = statusBarY + statusBarHeight + 8;
    state->saveButtonRect = QRect(8, statusBarY, 10 + buttonTextWidth + 10, statusBarHeight);
    state->exitButtonRect = QRect(8 + (10 + buttonTextWidth + 10 + 8), statusBarY, 10 + buttonTextWidth + 10, statusBarHeight);
    state->perfButtonRect = QRect(8 + (70 + buttonTextWidth + 65 + 16), statusBarY, 10 + buttonTextWidth + 10, statusBarHeight);
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

            if(state->sendFileName){
                // extract only the last folder and filename for shadow
                QStringList fileNameList = fileName.split("/");
                QString subFileName = fileNameList.at(fileNameList.size()- 2) + "/" + fileNameList.last();
                //printf("Inference viewer adding file %s to shadow array of size %d\n", subFileName.toStdString().c_str(), byteArray.size());
                state->shadowFileBuffer.push_back(subFileName);
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
        state->performance.updateFPSValue(imagesPerSec);
        state->performance.updateTotalImagesValue(progress.images_received);
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
    painter.drawRoundedRect(state->perfButtonRect, 4, 4);
    painter.setPen(buttonTextColor);
    painter.drawText(state->exitButtonRect, Qt::AlignCenter, exitButtonText);
    painter.drawText(state->saveButtonRect, Qt::AlignCenter, saveButtonText);
    painter.drawText(state->perfButtonRect, Qt::AlignCenter, perfButtonText);

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
