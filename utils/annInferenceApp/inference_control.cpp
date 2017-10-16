#include "inference_control.h"
#include "inference_compiler.h"
#include "inference_comm.h"
#include "assets.h"
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
#include <QCheckBox>
#include <QStyle>
#include <QDesktopServices>

#define CONFIGURATION_CACHE_FILENAME ".annInferenceApp.txt"
#define BUILD_VERSION "alpha1"

inference_control::inference_control(int operationMode_, QWidget *parent)
    : QWidget(parent), connectionSuccessful{ false }, modelType{ 0 }, numModelTypes{ 0 }
{
    setWindowTitle("annInferenceApp");
    setMinimumWidth(800);

    maxGPUs = 1;
    compiler_status.completed = false;
    compiler_status.dimOutput[0] = 0;
    compiler_status.dimOutput[1] = 0;
    compiler_status.dimOutput[2] = 0;
    compiler_status.errorCode = 0;
    operationMode = operationMode_;

    // default configuration
    QGridLayout * controlLayout = new QGridLayout;
    int editSpan = 3;
    int row = 0;

    //////////////
    /// \brief labelIntro
    ///
    QLabel * labelIntro = new QLabel("INFERENCE CONTROL PANEL");
    labelIntro->setStyleSheet("font-weight: bold; color: green");
    labelIntro->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QPushButton * buttonLogo = new QPushButton();
    QPixmap pixmap;
    QByteArray arr(assets::getLogoPngBuf(), assets::getLogoPngLen());
    pixmap.loadFromData(arr);
    buttonLogo->setIcon(pixmap);
    buttonLogo->setIconSize(QSize(64,64));
    buttonLogo->setFixedSize(QSize(64,64));
    controlLayout->addWidget(labelIntro, row, 0, 1, editSpan + 1);
    controlLayout->addWidget(buttonLogo, row, 1 + editSpan, 1, 1, Qt::AlignCenter);
    connect(buttonLogo, SIGNAL(released()), this, SLOT(onLogoClick()));
    row++;

    QFrame * sepHLine1 = new QFrame();
    sepHLine1->setFrameShape(QFrame::HLine);
    sepHLine1->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine1, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelServer
    ///
    QLabel * labelServer = new QLabel("Inference Server");
    labelServer->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelServer, row, 0, 1, 5);
    row++;

    QLabel * labelServerHost = new QLabel("Server:");
    editServerHost = new QLineEdit("localhost");
    editServerPort = new QLineEdit("28282");
    buttonConnect = new QPushButton("Connect");
    editServerPort->setValidator(new QIntValidator(1,65535));
    labelServerHost->setStyleSheet("font-weight: bold; font-style: italic");
    labelServerHost->setAlignment(Qt::AlignLeft);
    connect(buttonConnect, SIGNAL(released()), this, SLOT(connectServer()));
    controlLayout->addWidget(labelServerHost, row, 0, 1, 1);
    controlLayout->addWidget(editServerHost, row, 1, 1, 2);
    controlLayout->addWidget(editServerPort, row, editSpan, 1, 1);
    controlLayout->addWidget(buttonConnect, row, 1 + editSpan, 1, 1);
    row++;
    labelServerStatus = new QLabel("");
    labelServerStatus->setStyleSheet("font-style: italic");
    labelServerStatus->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelServerStatus, row, 1, 1, editSpan);
    QPushButton * exitButton = new QPushButton("Exit");
    controlLayout->addWidget(exitButton, row, 1 + editSpan, 1, 1);
    connect(exitButton, SIGNAL(released()), this, SLOT(exitControl()));
    row++;

    QFrame * sepHLine2 = new QFrame();
    sepHLine2->setFrameShape(QFrame::HLine);
    sepHLine2->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine2, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelCompiler
    ///
    typeModelFile1Label.push_back("Prototxt:");
    typeModelFile2Label.push_back("CaffeModel:");
    typeModelFile1Desc.push_back("Prototxt (*.prototxt)");
    typeModelFile2Desc.push_back("CaffeModel (*.caffemodel)");
    numModelTypes++;
    QLabel * labelCompiler = new QLabel("Inference Compiler");
    labelCompiler->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelCompiler, row, 0, 1, 5);
    row++;
    QLabel * labelModel = new QLabel("CNN Model:");
    comboModelSelect = new QComboBox();
    buttonModelUpload = new QPushButton(tr("Upload"), this);
    comboModelSelect->addItem("Upload a pre-trained Caffe model (i.e., .prototxt and .caffemodel)");
    labelModel->setStyleSheet("font-weight: bold; font-style: italic");
    labelModel->setAlignment(Qt::AlignLeft);
    connect(comboModelSelect, SIGNAL(activated(int)), this, SLOT(modelSelect(int)));
    connect(buttonModelUpload, SIGNAL(released()), this, SLOT(modelUpload()));
    controlLayout->addWidget(labelModel, row, 0, 1, 1);
    controlLayout->addWidget(comboModelSelect, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonModelUpload, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelInputDim = new QLabel("CxHxW(inp):");
    QLineEdit * editDimC = new QLineEdit("3");
    editDimH = new QLineEdit("224");
    editDimW = new QLineEdit("224");
    editDimC->setValidator(new QIntValidator(3,3));
    editDimH->setValidator(new QIntValidator(1,16384));
    editDimW->setValidator(new QIntValidator(1,16384));
    editDimC->setEnabled(false);
    labelInputDim->setStyleSheet("font-weight: bold; font-style: italic");
    labelInputDim->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelInputDim, row, 0, 1, 1);
    controlLayout->addWidget(editDimC, row, 1, 1, 1);
    controlLayout->addWidget(editDimH, row, 2, 1, 1);
    controlLayout->addWidget(editDimW, row, 3, 1, 1);
    row++;
    QLabel * labelOutputDim = new QLabel("CxHxW(out):");
    editOutDimC = new QLineEdit("");
    editOutDimH = new QLineEdit("");
    editOutDimW = new QLineEdit("");
    editOutDimC->setEnabled(false);
    editOutDimH->setEnabled(false);
    editOutDimW->setEnabled(false);
    labelOutputDim->setStyleSheet("font-weight: bold; font-style: italic");
    labelOutputDim->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelOutputDim, row, 0, 1, 1);
    controlLayout->addWidget(editOutDimC, row, 1, 1, 1);
    controlLayout->addWidget(editOutDimH, row, 2, 1, 1);
    controlLayout->addWidget(editOutDimW, row, 3, 1, 1);
    row++;
    labelModelFile1 = new QLabel("--");
    editModelFile1 = new QLineEdit("");
    buttonModelFile1 = new QPushButton(tr("Browse..."), this);
    connect(buttonModelFile1, &QAbstractButton::clicked, this, &inference_control::browseModelFile1);
    labelModelFile1->setStyleSheet("font-weight: bold; font-style: italic");
    labelModelFile1->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelModelFile1, row, 0, 1, 1);
    controlLayout->addWidget(editModelFile1, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonModelFile1, row, 1 + editSpan, 1, 1);
    row++;
    labelModelFile2 = new QLabel("--");
    editModelFile2 = new QLineEdit("");
    buttonModelFile2 = new QPushButton(tr("Browse..."), this);
    connect(buttonModelFile2, &QAbstractButton::clicked, this, &inference_control::browseModelFile2);
    labelModelFile2->setStyleSheet("font-weight: bold; font-style: italic");
    labelModelFile2->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelModelFile2, row, 0, 1, 1);
    controlLayout->addWidget(editModelFile2, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonModelFile2, row, 1 + editSpan, 1, 1);
    row++;
    labelCompilerOptions = new QLabel("--");
    editCompilerOptions = new QLineEdit("");
    labelCompilerOptions->setStyleSheet("font-weight: bold; font-style: italic");
    labelCompilerOptions->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelCompilerOptions, row, 0, 1, 1);
    controlLayout->addWidget(editCompilerOptions, row, 1, 1, editSpan);
    row++;
    connect(editModelFile1, SIGNAL(textChanged(const QString &)), this, SLOT(onChangeModelFile1(const QString &)));
    connect(editModelFile2, SIGNAL(textChanged(const QString &)), this, SLOT(onChangeModelFile2(const QString &)));
    connect(editCompilerOptions, SIGNAL(textChanged(const QString &)), this, SLOT(onChangeCompilerOptions(const QString &)));
    labelCompilerStatus = new QLabel("");
    labelCompilerStatus->setStyleSheet("font-style: italic; color: gray;");
    labelCompilerStatus->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelCompilerStatus, row, 1, 1, editSpan + 1);
    row++;

    QFrame * sepHLine3 = new QFrame();
    sepHLine3->setFrameShape(QFrame::HLine);
    sepHLine3->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine3, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelRuntime
    ///
    QLabel * labelRuntime = new QLabel("Inference Run-time");
    labelRuntime->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelRuntime, row, 0, 1, 5);
    row++;
    QLabel * labelGPUs = new QLabel("GPUs:");
    editGPUs = new QLineEdit("1");
    labelMaxGPUs = new QLabel("");
    buttonRunInference = new QPushButton("Run");
    editGPUs->setValidator(new QIntValidator(1,maxGPUs));
    editGPUs->setEnabled(false);
    labelGPUs->setStyleSheet("font-weight: bold; font-style: italic");
    labelGPUs->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelGPUs, row, 0, 1, 1);
    controlLayout->addWidget(editGPUs, row, 1, 1, 1);
    controlLayout->addWidget(labelMaxGPUs, row, 2, 1, 1);
    controlLayout->addWidget(buttonRunInference, row, 1 + editSpan, 1, 1);
    connect(buttonRunInference, SIGNAL(released()), this, SLOT(runInference()));
    row++;
    QLabel * labelImageLabelsFile = new QLabel("Labels:");
    editImageLabelsFile = new QLineEdit("");
    QPushButton * buttonDataLabels = new QPushButton(tr("Browse..."), this);
    connect(buttonDataLabels, &QAbstractButton::clicked, this, &inference_control::browseDataLabels);
    labelImageLabelsFile->setStyleSheet("font-weight: bold; font-style: italic");
    labelImageLabelsFile->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelImageLabelsFile, row, 0, 1, 1);
    controlLayout->addWidget(editImageLabelsFile, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDataLabels, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelImageFolder = new QLabel("Image Folder:");
    editImageFolder = new QLineEdit("");
    QPushButton * buttonDataFolder = new QPushButton(tr("Browse..."), this);
    connect(buttonDataFolder, &QAbstractButton::clicked, this, &inference_control::browseDataFolder);
    labelImageFolder->setStyleSheet("font-weight: bold; font-style: italic");
    labelImageFolder->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelImageFolder, row, 0, 1, 1);
    controlLayout->addWidget(editImageFolder, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDataFolder, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelImageList = new QLabel("Image List:");
    editImageListFile = new QLineEdit("");
    QPushButton * buttonDataFilename = new QPushButton(tr("Browse..."), this);
    connect(buttonDataFilename, &QAbstractButton::clicked, this, &inference_control::browseDataFilename);
    labelImageList->setStyleSheet("font-weight: bold; font-style: italic");
    labelImageList->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelImageList, row, 0, 1, 1);
    controlLayout->addWidget(editImageListFile, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDataFilename, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelMaxDataSize = new QLabel("Image Count:");
    editMaxDataSize = new QLineEdit("");
    editMaxDataSize->setValidator(new QIntValidator());
    labelMaxDataSize->setStyleSheet("font-weight: bold; font-style: italic");
    labelMaxDataSize->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelMaxDataSize, row, 0, 1, 1);
    controlLayout->addWidget(editMaxDataSize, row, 1, 1, 1);
    checkRepeatImages = nullptr;
    if(operationMode) {
        checkRepeatImages = new QCheckBox("Repeat Images");
        checkRepeatImages->setChecked(true);
        controlLayout->addWidget(checkRepeatImages, row, 2, 1, 1);
    }
    row++;

    setLayout(controlLayout);

    // activate based on configuration
    loadConfig();
    modelSelect(comboModelSelect->currentIndex());

    // start timer for update
    QTimer *timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
    timer->start(1000);
}

void inference_control::saveConfig()
{
    bool repeat_images = false;
    if(checkRepeatImages && checkRepeatImages->checkState())
        repeat_images = true;
    int maxDataSize = editMaxDataSize->text().toInt();
    if(maxDataSize < 0) {
        repeat_images = true;
        maxDataSize = abs(maxDataSize);
    }
    // save configuration
    QString homeFolder = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0];
    QFile fileObj(homeFolder + "/" + CONFIGURATION_CACHE_FILENAME);
    if(fileObj.open(QIODevice::WriteOnly)) {
        QTextStream fileOutput(&fileObj);
        fileOutput << BUILD_VERSION << endl;
        fileOutput << editServerHost->text() << endl;
        fileOutput << editServerPort->text() << endl;
        fileOutput << lastModelFile1 << endl;
        fileOutput << lastModelFile2 << endl;
        fileOutput << lastDimH << endl;
        fileOutput << lastDimW << endl;
        fileOutput << editGPUs->text() << endl;
        fileOutput << lastCompilerOptions << endl;
        fileOutput << editImageLabelsFile->text() << endl;
        fileOutput << editImageFolder->text() << endl;
        fileOutput << editImageListFile->text() << endl;
        QString text;
        fileOutput << ((maxDataSize > 0) ? text.sprintf("%d", maxDataSize) : "") << endl;
        fileOutput << (repeat_images ? 1 : 0) << endl;
    }
    fileObj.close();
}

void inference_control::loadConfig()
{
    // load default configuration
    QString homeFolder = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0];
    QFile fileObj(homeFolder + "/" + CONFIGURATION_CACHE_FILENAME);
    if(fileObj.open(QIODevice::ReadOnly)) {
        QTextStream fileInput(&fileObj);
        QString version = fileInput.readLine();
        if(version == BUILD_VERSION) {
            editServerHost->setText(fileInput.readLine());
            editServerPort->setText(fileInput.readLine());
            editModelFile1->setText(fileInput.readLine());
            editModelFile2->setText(fileInput.readLine());
            editDimH->setText(fileInput.readLine());
            editDimW->setText(fileInput.readLine());
            editGPUs->setText(fileInput.readLine());
            editCompilerOptions->setText(fileInput.readLine());
            editImageLabelsFile->setText(fileInput.readLine());
            editImageFolder->setText(fileInput.readLine());
            editImageListFile->setText(fileInput.readLine());
            editMaxDataSize->setText(fileInput.readLine());
            bool repeat_images = false;
            if(fileInput.readLine() == "1")
                repeat_images = true;
            if(checkRepeatImages) {
                checkRepeatImages->setChecked(repeat_images);
            }
            else if(repeat_images && editMaxDataSize->text().length() > 0 && editMaxDataSize->text()[0] != '-') {
                editMaxDataSize->setText("-" + editMaxDataSize->text());
            }
        }
    }
    fileObj.close();
    // save last options
    lastDimW = editDimW->text();
    lastDimH = editDimH->text();
    lastModelFile1 = editModelFile1->text();
    lastModelFile2 = editModelFile2->text();
    lastCompilerOptions = editCompilerOptions->text();
}

bool inference_control::isConfigValid(QString& err)
{
    if(editServerPort->text().toInt() <= 0) { err = "Server: invalid port number."; return false; }
    if(comboModelSelect->currentIndex() < numModelTypes) {
        if(!QFileInfo(editModelFile1->text()).isFile()) { err = typeModelFile1Label[comboModelSelect->currentIndex()] + editModelFile1->text() + " file doesn't exist."; return false; }
        if(!QFileInfo(editModelFile2->text()).isFile()) { err = typeModelFile2Label[comboModelSelect->currentIndex()] + editModelFile2->text() + " file doesn't exist."; return false; }
    }
    if(editDimW->text().toInt() <= 0) { err = "Dimensions: width must be positive."; return false; }
    if(editDimH->text().toInt() <= 0) { err = "Dimensions: height must be positive."; return false; }
    if(editGPUs->text().toInt() <= 0) { err = "GPUs: must be positive."; return false; }
    return true;
}

void inference_control::modelSelect(int model)
{
    QString text;
    bool compilationCompleted = (compiler_status.errorCode > 0) && compiler_status.completed;
    int dimOutput[3] = { compiler_status.dimOutput[0], compiler_status.dimOutput[1], compiler_status.dimOutput[2] };
    QString modelName;
    if(model < numModelTypes) {
        // input dimensions
        editDimW->setDisabled(false);
        editDimH->setDisabled(false);
        // model file selection
        buttonModelUpload->setEnabled(false);
        if(connectionSuccessful && editModelFile1->text().length() > 0 && editModelFile2->text().length() > 0) {
            buttonModelUpload->setEnabled(true);
        }
        labelModelFile1->setText(typeModelFile1Label[model]);
        editModelFile1->setText(lastModelFile1);
        editModelFile1->setEnabled(true);
        buttonModelFile1->setEnabled(true);
        labelModelFile2->setText(typeModelFile2Label[model]);
        editModelFile2->setText(lastModelFile2);
        editModelFile2->setEnabled(true);
        buttonModelFile2->setEnabled(true);
        labelCompilerOptions->setText("Options:");
        editCompilerOptions->setReadOnly(false);
        editCompilerOptions->setText(lastCompilerOptions);
        if(compiler_status.completed && compiler_status.errorCode > 0) {
            modelName = compiler_status.message;
        }
    }
    else {
        model -= numModelTypes;
        // already compiled
        compilationCompleted = true;
        dimOutput[0] = modelList[model].outputDim[0];
        dimOutput[1] = modelList[model].outputDim[1];
        dimOutput[2] = modelList[model].outputDim[2];
        // input & output dimensions
        editDimW->setDisabled(true);
        editDimH->setDisabled(true);
        editDimW->setText(text.sprintf("%d", modelList[model].inputDim[0]));
        editDimH->setText(text.sprintf("%d", modelList[model].inputDim[1]));
        // model file selection
        labelModelFile1->setText("--");
        editModelFile1->setEnabled(false);
        editModelFile1->setText("");
        buttonModelFile1->setEnabled(false);
        labelModelFile2->setText("--");
        editModelFile2->setEnabled(false);
        editModelFile2->setText("");
        buttonModelFile2->setEnabled(false);
        buttonModelUpload->setEnabled(false);
        labelCompilerOptions->setText("--");
        editCompilerOptions->setReadOnly(true);
        editCompilerOptions->setText("");
    }
    if(modelName.length() > 0) {
        labelCompilerStatus->setText("[" + modelName + "]*");
    }
    else {
        labelCompilerStatus->setText("");
    }
    // output dimensions
    editOutDimW->setText(dimOutput[0] == 0 ? "" : text.sprintf("%d", dimOutput[0]));
    editOutDimH->setText(dimOutput[1] == 0 ? "" : text.sprintf("%d", dimOutput[1]));
    editOutDimC->setText(dimOutput[2] == 0 ? "" : text.sprintf("%d", dimOutput[2]));
    // enable GPUs
    editGPUs->setEnabled(compilationCompleted);
    // enable run button
    buttonRunInference->setEnabled(false);
    if(compilationCompleted && dimOutput[0] > 0 && dimOutput[1] > 0 && dimOutput[2] > 0 &&
       editImageLabelsFile->text().length() > 0 && editImageFolder->text().length() > 0)
    {
        buttonRunInference->setEnabled(true);
    }
}

void inference_control::tick()
{
    modelSelect(comboModelSelect->currentIndex());
}

void inference_control::onChangeModelFile1(const QString & text)
{
    if(comboModelSelect->currentIndex() == 0) {
        lastModelFile1 =  text;
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::onChangeModelFile2(const QString & text)
{
    if(comboModelSelect->currentIndex() == 0) {
        lastModelFile2 =  text;
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::onChangeCompilerOptions(const QString & text)
{
    if(comboModelSelect->currentIndex() == 0)
        lastCompilerOptions =  text;
}

void inference_control::browseModelFile1()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, typeModelFile1Desc[modelType]);
    if(fileName.size() > 0) {
        editModelFile1->setText(fileName);
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::browseModelFile2()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, typeModelFile2Desc[modelType]);
    if(fileName.size() > 0) {
        editModelFile2->setText(fileName);
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::browseDataLabels()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Labels File"), nullptr, tr("Labels Text (*.txt)"));
    if(fileName.size() > 0)
        editImageLabelsFile->setText(fileName);
}

void inference_control::browseDataFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Image Folder"), nullptr,
                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.size() > 0)
        editImageFolder->setText(dir);
}

void inference_control::browseDataFilename()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image List File"), nullptr, tr("Image List Text (*.txt)"));
    if(fileName.size() > 0)
        editImageListFile->setText(fileName);
}

void inference_control::exitControl()
{
    close();
}

void inference_control::connectServer()
{
    // check configuration
    QString err;
    if(editServerHost->text().length() <= 0 || editServerPort->text().toInt() <= 0) {
        QMessageBox::critical(this, windowTitle(), "Server host/port is not valid", QMessageBox::Ok);
        return;
    }
    // save configuration
    saveConfig();

    // check TCP connection
    QTcpSocket * tcpSocket = new QTcpSocket(this);
    tcpSocket->connectToHost(editServerHost->text(), editServerPort->text().toInt());
    QString status;
    connectionSuccessful = false;
    status = "ERROR: Unable to connect to " + editServerHost->text() + ":" + editServerPort->text();
    if(tcpSocket->waitForConnected(3000)) {
        int pendingModelCount = 0;
        while(tcpSocket->state() == QAbstractSocket::ConnectedState) {
            bool receivedCommand = false;
            if(tcpSocket->waitForReadyRead()) {
                InfComCommand cmd;
                while(tcpSocket->bytesAvailable() >= (qint64)sizeof(cmd) &&
                      tcpSocket->read((char *)&cmd, sizeof(cmd)) == sizeof(cmd))
                {
                    receivedCommand = true;
                    if(cmd.magic != INFCOM_MAGIC) {
                        status.sprintf("ERROR: got invalid magic 0x%08x", cmd.magic);
                        break;
                    }
                    auto send = [](QTcpSocket * sock, QString& status, const void * buf, size_t len) -> bool {
                        sock->write((const char *)buf, len);
                        if(!sock->waitForBytesWritten(3000)) {
                            status.sprintf("ERROR: write(%ld) failed", len);
                            return false;
                        }
                        return true;
                    };
                    if(cmd.command == INFCOM_CMD_DONE) {
                        break;
                    }
                    else if(cmd.command == INFCOM_CMD_SEND_MODE) {
                        InfComCommand reply = {
                            INFCOM_MAGIC, INFCOM_CMD_SEND_MODE,
                            { INFCOM_MODE_CONFIGURE },
                            { 0 }
                        };
                        if(!send(tcpSocket, status, &reply, sizeof(reply)))
                            break;
                    }
                    else if(cmd.command == INFCOM_CMD_CONFIG_INFO) {
                        pendingModelCount = cmd.data[0];
                        maxGPUs = cmd.data[1];
                        QString text;
                        editGPUs->setText(text.sprintf("%d", maxGPUs));
                        editGPUs->setValidator(new QIntValidator(1,maxGPUs));
                        labelMaxGPUs->setText(text.sprintf("(upto %d)", maxGPUs));
                        while(comboModelSelect->count() > 1)
                            comboModelSelect->removeItem(1);
                        modelList.clear();
                        connectionSuccessful = true;
                        status = "OK: Connected to " + editServerHost->text() + ":" + editServerPort->text();
                        if(pendingModelCount <= 0) {
                            InfComCommand reply = {
                                INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
                            };
                            if(!send(tcpSocket, status, &reply, sizeof(reply)))
                                break;
                        }
                    }
                    else if(cmd.command == INFCOM_CMD_MODEL_INFO) {
                        InfComModelInfo info = { { 0 }, { 0 }, { 0 } };
                        info.inputDim[0] = cmd.data[0];
                        info.inputDim[1] = cmd.data[1];
                        info.inputDim[2] = cmd.data[2];
                        info.outputDim[0] = cmd.data[3];
                        info.outputDim[1] = cmd.data[4];
                        info.outputDim[2] = cmd.data[5];
                        strncpy(info.name, cmd.message, sizeof(info.name));
                        modelList.push_back(info);
                        comboModelSelect->addItem(info.name);
                        pendingModelCount--;
                        if(pendingModelCount <= 0) {
                            InfComCommand reply = {
                                INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
                            };
                            if(!send(tcpSocket, status, &reply, sizeof(reply)))
                                break;
                        }
                    }
                    else {
                        status.sprintf("ERROR: got invalid command received 0x%08x", cmd.command);
                        break;
                    }
                }
            }
            if(!receivedCommand) {
                QThread::msleep(2);
            }
        }
    }
    tcpSocket->close();
    labelServerStatus->setText(status);

    // update status
    if(comboModelSelect->currentIndex() > modelList.length()) {
        comboModelSelect->setCurrentIndex(modelList.length() - 1);
    }
    modelSelect(comboModelSelect->currentIndex());
}

void inference_control::modelUpload()
{
    // check configuration
    QString err;
    if(!isConfigValid(err)) {
        QMessageBox::critical(this, windowTitle(), err, QMessageBox::Ok);
        return;
    }
    buttonModelUpload->setEnabled(false);

    // save configuration
    saveConfig();

    // start compiler
    inference_compiler * compiler = new inference_compiler(
                true,
                editServerHost->text(), editServerPort->text().toInt(),
                3,
                editDimH->text().toInt(),
                editDimW->text().toInt(),
                editModelFile1->text(), editModelFile2->text(),
                editCompilerOptions->text(),
                &compiler_status);
    compiler->show();
}

void inference_control::runInference()
{
    // check configuration
    QString err;
    if(isConfigValid(err)) {
        if(!QFileInfo(editImageLabelsFile->text()).isFile())
            err = "Labels: file doesn't exist: " + editImageLabelsFile->text();
        else if(!QFileInfo(editImageFolder->text()).isDir())
            err = "Image Folder: doesn't exist: " + editImageFolder->text();
    }
    if(err.length() > 0) {
        QMessageBox::critical(this, windowTitle(), err, QMessageBox::Ok);
        return;
    }

    // save configuration
    saveConfig();

    // start viewer
    QString modelName = comboModelSelect->currentText();
    if(comboModelSelect->currentIndex() < numModelTypes) {
        modelName = compiler_status.message;
    }
    int dimInput[3] = { editDimW->text().toInt(), editDimH->text().toInt(), 3 };
    int dimOutput[3] = { editOutDimW->text().toInt(), editOutDimH->text().toInt(), editOutDimC->text().toInt() };
    bool repeat_images = false;
    if(checkRepeatImages && checkRepeatImages->checkState())
        repeat_images = true;
    int maxDataSize = editMaxDataSize->text().toInt();
    if(maxDataSize < 0) {
        repeat_images = true;
        maxDataSize = abs(maxDataSize);
    }
    inference_viewer * viewer = new inference_viewer(
                editServerHost->text(), editServerPort->text().toInt(), modelName,
                editImageLabelsFile->text(), editImageListFile->text(), editImageFolder->text(),
                dimInput, editGPUs->text().toInt(), dimOutput, maxDataSize, repeat_images);
    viewer->show();
    close();
}

void inference_control::onLogoClick()
{
    qDebug("clicked");
    QDesktopServices::openUrl(QUrl("https://instinct.radeon.com/en/"));
}
