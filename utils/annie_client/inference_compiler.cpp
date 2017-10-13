#include "inference_compiler.h"
#include "inference_control.h"
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QIntValidator>
#include <QMessageBox>
#include <QFileInfo>
#include <QFrame>
#include <QTcpSocket>
#include <QTimer>
#include <QThread>

bool inference_model_uploader::abortRequsted = false;

void inference_model_uploader::abort()
{
    abortRequsted = true;
}

inference_model_uploader::inference_model_uploader(
        QString serverHost_, int serverPort_,
        QString prototxt_, QString caffeModel_,
        int n, int c, int h, int w, int gpuCount_,
        QString compilerOptions_,
        model_uploader_status * progress_,
        QObject *parent) : QObject(parent)
{
    serverHost = serverHost_;
    serverPort = serverPort_;
    prototxt = prototxt_;
    caffeModel = caffeModel_;
    dimN = n;
    dimC = c;
    dimH = h;
    dimW = w;
    gpuCount = gpuCount_;
    compilerOptions = compilerOptions_;
    progress = progress_;
}

inference_model_uploader::~inference_model_uploader()
{

}

void inference_model_uploader::run()
{
    // connect to the server for inference compiler mode
    //    - configure the connection in inference compiler mode
    //    - upload prototxt, caffemode, and configuration parameters
    //    - update status of remote compilation process

    // TODO: added below just for GUI testing
    int counter = 0;
    while(!abortRequsted) {
        counter++;
        progress->prototxtUploadProgress = 1.0/counter;
        progress->caffeModelUploadProgress = counter/16.0;
        progress->compilationProgress = counter;
        progress->message.sprintf("Message 101 with tick at %d", counter);
        if(counter == 1000) progress->completed = true;
        QThread::msleep(2);
    }

    emit finished();
}

void inference_compiler::startModelUploader()
{
    // start receiver thread
    QThread * thread = new QThread;
    worker = new inference_model_uploader(serverHost, serverPort,
                        prototxt, caffeModel, dimN, dimC, dimH, dimW,
                        gpuCount, compilerOptions,
                        &progress);
    worker->moveToThread(thread);
    connect(worker, SIGNAL (error(QString)), this, SLOT (errorString(QString)));
    connect(thread, SIGNAL (started()), worker, SLOT (run()));
    connect(worker, SIGNAL (finished()), thread, SLOT (quit()));
    connect(worker, SIGNAL (finished()), worker, SLOT (deleteLater()));
    connect(thread, SIGNAL (finished()), thread, SLOT (deleteLater()));
    thread->start();
    thread->terminate();
}

inference_compiler::inference_compiler(
        QString serverHost_, int serverPort_,
        QString prototxt_, QString caffeModel_,
        int n, int c, int h, int w, int gpuCount_,
        QString compilerOptions_,
        QWidget *parent) : QWidget(parent)
{
    setWindowTitle("Inference Compiler");
    setMinimumWidth(800);

    serverHost = serverHost_;
    serverPort = serverPort_;
    prototxt = prototxt_;
    caffeModel = caffeModel_;
    dimN = n;
    dimC = c;
    dimH = h;
    dimW = w;
    gpuCount = gpuCount_;
    compilerOptions = compilerOptions_;

    // status
    worker = nullptr;
    progress.completed = false;
    progress.prototxtUploadProgress = 0;
    progress.caffeModelUploadProgress = 0;
    progress.compilationProgress = 0;
    progress.message = "";

    // GUI

    QGridLayout * controlLayout = new QGridLayout;
    int editSpan = 5;
    int row = 0;

    //////////////
    /// \brief labelIntro
    ///
    QString port; port.sprintf("%d", serverPort);
    QLabel * labelIntro = new QLabel("Inference Compiler: remote connection to " + serverHost + ":" + port);
    labelIntro->setStyleSheet("font-weight: bold; color: green");
    labelIntro->setAlignment(Qt::AlignCenter);
    controlLayout->addWidget(labelIntro, row, 0, 1, editSpan + 2);
    row++;

    QFrame * sepHLine1 = new QFrame();
    sepHLine1->setFrameShape(QFrame::HLine);
    sepHLine1->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine1, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelStatus
    ///
    QLabel * labelH1 = new QLabel("prototxt upload");
    QLabel * labelH2 = new QLabel("caffemodel upload");
    QLabel * labelH3 = new QLabel("compilation");
    labelH1->setStyleSheet("font-weight: bold; font-style: italic");
    labelH2->setStyleSheet("font-weight: bold; font-style: italic");
    labelH3->setStyleSheet("font-weight: bold; font-style: italic");
    labelH1->setAlignment(Qt::AlignCenter);
    labelH2->setAlignment(Qt::AlignCenter);
    labelH3->setAlignment(Qt::AlignCenter);
    controlLayout->addWidget(labelH1, row, 1, 1, 1);
    controlLayout->addWidget(labelH2, row, 2, 1, 1);
    controlLayout->addWidget(labelH3, row, 3, 1, 1);
    row++;

    labelStatus = new QLabel("Progress:");
    editPrototxtUploadProgress = new QLineEdit("");
    editCaffeModelUploadProgress = new QLineEdit("");
    editCompilerProgress = new QLineEdit("");
    editPrototxtUploadProgress->setReadOnly(true);
    editCaffeModelUploadProgress->setReadOnly(true);
    editCompilerProgress->setReadOnly(true);
    labelStatus->setStyleSheet("font-weight: bold; font-style: italic");
    labelStatus->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelStatus, row, 0, 1, 1);
    controlLayout->addWidget(editPrototxtUploadProgress, row, 1, 1, 1);
    controlLayout->addWidget(editCaffeModelUploadProgress, row, 2, 1, 1);
    controlLayout->addWidget(editCompilerProgress, row, 3, 1, 1);
    row++;

    QLabel * labelMessage = new QLabel("Message:");
    editCompilerMessage = new QLineEdit("");
    editCompilerMessage->setReadOnly(true);
    labelMessage->setStyleSheet("font-weight: bold; font-style: italic");
    labelMessage->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelMessage, row, 0, 1, 1);
    controlLayout->addWidget(editCompilerMessage, row, 1, 1, editSpan);
    row++;

    //////////////
    /// \brief compilerButtonBox
    ///
    QDialogButtonBox * compilerButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(compilerButtonBox, &QDialogButtonBox::accepted, this, &inference_compiler::Ok);
    connect(compilerButtonBox, &QDialogButtonBox::rejected, this, &inference_compiler::Cancel);
    okCompilerButton = compilerButtonBox->button(QDialogButtonBox::Ok);
    cancelCompilerButton = compilerButtonBox->button(QDialogButtonBox::Cancel);
    okCompilerButton->setText("Running...");
    okCompilerButton->setEnabled(false);
    cancelCompilerButton->setText("Abort");
    controlLayout->addWidget(compilerButtonBox, row, (1 + editSpan)/2, 1, 1);
    row++;

    setLayout(controlLayout);

    // start timer for update
    QTimer *timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
    timer->start(200);

    // start model uploader
    startModelUploader();
}

void inference_compiler::errorString(QString err)
{
    qDebug("inference_compiler: %s", err.toStdString().c_str());
}

void inference_compiler::tick()
{
    // update Window
    if(progress.completed) {
        okCompilerButton->setEnabled(true);
        okCompilerButton->setText("Close");
        cancelCompilerButton->setText("Exit");
    }
    QString text;
    editPrototxtUploadProgress->setText(text.sprintf("%g", progress.prototxtUploadProgress));
    editCaffeModelUploadProgress->setText(text.sprintf("%g", progress.caffeModelUploadProgress));
    editCompilerProgress->setText(text.sprintf("%g", progress.compilationProgress));
    editCompilerMessage->setText(progress.message);
}

void inference_compiler::Ok()
{
    inference_model_uploader::abort();
    close();
}

void inference_compiler::Cancel()
{
    inference_model_uploader::abort();
    exit(1);
}
