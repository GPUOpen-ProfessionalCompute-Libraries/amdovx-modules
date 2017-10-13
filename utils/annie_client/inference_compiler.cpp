#include "inference_compiler.h"
#include "inference_control.h"
#include "inference_comm.h"
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
        int n, int c, int h, int w, int GPUs_,
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
    GPUs = GPUs_;
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

#if INFCOM_ENABLED
    QTcpSocket * tcpSocket = new QTcpSocket(this);
    tcpSocket->connectToHost(serverHost, serverPort);
    if(tcpSocket->waitForConnected(3000)) {
        while(tcpSocket->state() == QAbstractSocket::ConnectedState) {
            if(abortRequsted)
                break;
            if(tcpSocket->waitForReadyRead()) {
                InfComCommand cmd;
                if(tcpSocket->bytesAvailable() >= (qint64)sizeof(cmd) &&
                   tcpSocket->read((char *)&cmd, sizeof(cmd)) == sizeof(cmd))
                {
                    if(cmd.magic != INFCOM_MAGIC) {
                        progress->errorCode = -1;
                        progress->message.sprintf("ERROR: got invalid magic 0x%08x", cmd.magic);
                        break;
                    }
                    auto send = [](QTcpSocket * sock, model_uploader_status * progress, const void * buf, size_t len) -> bool {
                        sock->write((const char *)buf, len);
                        if(!sock->waitForBytesWritten(3000)) {
                            progress->errorCode = -1;
                            progress->message.sprintf("ERROR: write(%ld) failed", len);
                            return false;
                        }
                        return true;
                    };
                    auto sendFile = [](QTcpSocket * sock, model_uploader_status * progress, int command, QString fileName, int * completed) -> bool {
                        QFile fileObj(fileName);
                        if(!fileObj.open(QIODevice::ReadOnly)) {
                            progress->errorCode = -1;
                            progress->message.sprintf("ERROR: unable to open: %s", fileName.toStdString().c_str());
                            return false;
                        }
                        QByteArray byteArray = fileObj.readAll();
                        InfComCommand reply = {
                            INFCOM_MAGIC, command,
                            { byteArray.size(), 0 },
                            { 0 }
                        };
                        QStringList text = fileName.split("/");
                        strncpy(reply.message, text[text.size()-1].toStdString().c_str(), sizeof(reply.message));
                        sock->write((const char *)&reply, sizeof(reply));
                        if(!sock->waitForBytesWritten()) {
                            progress->errorCode = -1;
                            progress->message.sprintf("ERROR: sendFile: write(header:%d) - %s", byteArray.size(), fileName.toStdString().c_str());
                            return false;
                        }
                        const char * buf = byteArray.constData();
                        int len = byteArray.size();
                        int pos = 0;
                        while(pos < len) {
                            if(abortRequsted)
                                break;
                            int pktSize = std::min(INFCOM_MAX_PACKET_SIZE, len-pos);
                            sock->write(&buf[pos], pktSize);
                            if(!sock->waitForBytesWritten()) {
                                progress->errorCode = -1;
                                progress->message.sprintf("ERROR: sendFile: write(data:%d) failed after %d/%d bytes - %s", pktSize, pos, len, fileName.toStdString().c_str());
                                return false;
                            }
                            pos += pktSize;
                            *completed = (int)((float)pos * 100.0 / len + 0.5);
                        }
                        int eofMarked = INFCOM_EOF_MARKER;
                        sock->write((const char *)&eofMarked, sizeof(eofMarked));
                        if(!sock->waitForBytesWritten()) {
                            progress->errorCode = -1;
                            progress->message.sprintf("ERROR: sendFile: write(eofMarked:%ld) - %s", sizeof(eofMarked), fileName.toStdString().c_str());
                            return false;
                        }
                        *completed = (int)((float)pos * 100.0 / len + 0.5);
                        return true;
                    };
                    if(cmd.command == INFCOM_CMD_DONE) {
                        break;
                    }
                    else if(cmd.command == INFCOM_CMD_SEND_MODE) {
                        InfComCommand reply = {
                            INFCOM_MAGIC, INFCOM_CMD_SEND_MODE,
                            { INFCOM_MODE_COMPILER, GPUs, dimW, dimH, dimC, dimN },
                            { 0 }
                        };
                        strncpy(reply.message, compilerOptions.toStdString().c_str(), sizeof(reply.message));
                        if(!send(tcpSocket, progress, &reply, sizeof(reply)))
                            break;
                    }
                    else if(cmd.command == INFCOM_CMD_SEND_PROTOTXT) {
                        if(!sendFile(tcpSocket, progress, INFCOM_CMD_SEND_PROTOTXT, prototxt, &progress->prototxtUploadProgress))
                            break;
                    }
                    else if(cmd.command == INFCOM_CMD_SEND_CAFFEMODEL) {
                        if(!sendFile(tcpSocket, progress, INFCOM_CMD_SEND_CAFFEMODEL, caffeModel, &progress->caffeModelUploadProgress))
                            break;
                    }
                    else if(cmd.command == INFCOM_CMD_COMPILER_STATUS) {
                        progress->completed = (cmd.data[0] != 0) ? true : false;
                        progress->errorCode = cmd.data[0];
                        progress->compilationProgress = cmd.data[1];
                        progress->dimOutput[0] = cmd.data[2];
                        progress->dimOutput[1] = cmd.data[3];
                        progress->dimOutput[2] = cmd.data[4];
                        progress->dimOutput[3] = cmd.data[5];
                        progress->message = cmd.message;
                        if(progress->completed)
                            break;
                    }
                    else {
                        progress->errorCode = -1;
                        progress->message.sprintf("ERROR: got invalid command 0x%08x", cmd.command);
                        break;
                    }
                }
            }
        }
    }
    else {
        progress->errorCode = -1;
        progress->message.sprintf("ERROR: Unable to connect to %s:%d", serverHost.toStdString().c_str(), serverPort);
    }
    if(abortRequsted)
        progress->message += " [aborted]";
    tcpSocket->close();
    progress->completed = true;
#else
   int counter = 0;
   while(!abortRequsted) {
       counter++;
       progress->prototxtUploadProgress = 1.0/counter;
       progress->caffeModelUploadProgress = counter/16.0;
       progress->compilationProgress = counter;
       progress->message.sprintf("Message 101 with tick at %d", counter);
       if(counter == 600) {
           progress->dimOutput[0] = 1;
           progress->dimOutput[1] = 1;
           progress->dimOutput[2] = 1000;
           progress->dimOutput[3] = 32;
       }
       if(counter == 1000) progress->completed = true;
       QThread::msleep(2);
   }
#endif

    emit finished();
}

void inference_compiler::startModelUploader()
{
    // start receiver thread
    QThread * thread = new QThread;
    worker = new inference_model_uploader(serverHost, serverPort,
                        prototxt, caffeModel, dimN, dimC, dimH, dimW,
                        GPUs, compilerOptions,
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
        int n, int c, int h, int w, int GPUs_,
        QString compilerOptions_,
        int * dimOutput,
        bool * completed_,
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
    GPUs = GPUs_;
    compilerOptions = compilerOptions_;
    completed = completed_;

    // status
    worker = nullptr;
    progress.completed = false;
    progress.errorCode = 0;
    progress.prototxtUploadProgress = 0;
    progress.caffeModelUploadProgress = 0;
    progress.compilationProgress = 0;
    progress.message = "";
    progress.dimOutput = dimOutput;

    // GUI

    QGridLayout * controlLayout = new QGridLayout;
    int editSpan = 5;
    int row = 0;

    //////////////
    /// \brief labelIntro
    ///
    QString text;
    QLabel * labelIntro = new QLabel("Inference Compiler: remote connection to " + serverHost + ":" + text.sprintf("%d", serverPort));
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

    QLabel * labelInputDim = new QLabel("NCHW(inp):");
    editDimN = new QLineEdit(text.sprintf("%d", dimN));
    editDimC = new QLineEdit(text.sprintf("%d", dimC));
    editDimH = new QLineEdit(text.sprintf("%d", dimH));
    editDimW = new QLineEdit(text.sprintf("%d", dimW));
    editDimN->setReadOnly(true);
    editDimC->setReadOnly(true);
    editDimH->setReadOnly(true);
    editDimW->setReadOnly(true);
    labelInputDim->setStyleSheet("font-weight: bold; font-style: italic");
    labelInputDim->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelInputDim, row, 0, 1, 1);
    controlLayout->addWidget(editDimN, row, 1, 1, 1);
    controlLayout->addWidget(editDimC, row, 2, 1, 1);
    controlLayout->addWidget(editDimH, row, 3, 1, 1);
    controlLayout->addWidget(editDimW, row, 4, 1, 1);
    row++;

    QLabel * labelOutputDim = new QLabel("NCHW(out):");
    editOutDimN = new QLineEdit("");
    editOutDimC = new QLineEdit("");
    editOutDimH = new QLineEdit("");
    editOutDimW = new QLineEdit("");
    editOutDimN->setReadOnly(true);
    editOutDimC->setReadOnly(true);
    editOutDimH->setReadOnly(true);
    editOutDimW->setReadOnly(true);
    labelOutputDim->setStyleSheet("font-weight: bold; font-style: italic");
    labelOutputDim->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelOutputDim, row, 0, 1, 1);
    controlLayout->addWidget(editOutDimN, row, 1, 1, 1);
    controlLayout->addWidget(editOutDimC, row, 2, 1, 1);
    controlLayout->addWidget(editOutDimH, row, 3, 1, 1);
    controlLayout->addWidget(editOutDimW, row, 4, 1, 1);
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
        if(progress.errorCode == 0)
            okCompilerButton->setEnabled(true);
        okCompilerButton->setText("Close");
        cancelCompilerButton->setText("Exit");
        *completed = true;
    }
    QString text;
    editPrototxtUploadProgress->setText(text.sprintf("%d%%", progress.prototxtUploadProgress));
    editCaffeModelUploadProgress->setText(text.sprintf("%d%%", progress.caffeModelUploadProgress));
    editCompilerProgress->setText(text.sprintf("%d%%", progress.compilationProgress));
    editOutDimW->setText(text.sprintf("%d", progress.dimOutput[0]));
    editOutDimH->setText(text.sprintf("%d", progress.dimOutput[1]));
    editOutDimC->setText(text.sprintf("%d", progress.dimOutput[2]));
    editOutDimN->setText(text.sprintf("%d", progress.dimOutput[3]));
    if(progress.errorCode < 0) {
        editCompilerMessage->setText(text.sprintf("[E%d] %s", progress.errorCode, progress.message.toStdString().c_str()));
    }
    else {
        editCompilerMessage->setText(text.sprintf("%s%s",
                      *completed ? "[completed] " : "", progress.message.toStdString().c_str()));
    }
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
