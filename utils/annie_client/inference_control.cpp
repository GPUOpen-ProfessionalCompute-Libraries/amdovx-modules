#include "inference_control.h"
#include "inference_compiler.h"
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

#define CONFIGURATION_CACHE_FILENAME ".inference_control.txt"
#define BUILD_VERSION "alpha2"

bool inference_control::isConfigValid(QString& err)
{
    if(editServerPort->text().toInt() <= 0) { err = "Server: invalid port number."; return false; }
    if(!QFileInfo(editPrototxt->text()).isFile()) { err = "Prototxt: file doesn't exist."; return false; }
    if(!QFileInfo(editCaffeModel->text()).isFile()) { err = "CaffeModel: file doesn't exist."; return false; }
    if(editDimW->text().toInt() <= 0) { err = "Dimensions: width must be positive."; return false; }
    if(editDimH->text().toInt() <= 0) { err = "Dimensions: height must be positive."; return false; }
    if(editDimC->text().toInt() != 3) { err = "Dimensions: Number of channels must be 3."; return false; }
    if(editDimN->text().toInt() <= 0) { err = "Dimensions: Number of batches must be positive."; return false; }
    if(editGPUs->text().toInt() <= 0) { err = "GPUs: must be positive."; return false; }
    if(!QFileInfo(editLabels->text()).isFile()) { err = "Labels: file doesn't exist."; return false; }
    if(!QFileInfo(editDataset->text()).isFile()) { err = "Dataset: file doesn't exist."; return false; }
    if(!QFileInfo(editFolder->text()).isDir()) { err = "Folder: doesn't exist."; return false; }
    return true;
}

void inference_control::saveConfig()
{
    QString serverHost = editServerHost->text();
    QString serverPort = editServerPort->text();
    QString prototxt = editPrototxt->text();
    QString caffeModel = editCaffeModel->text();
    QString dimN = editDimN->text();
    QString dimC = editDimC->text();
    QString dimH = editDimH->text();
    QString dimW = editDimW->text();
    QString GPUs = editGPUs->text();
    QString compilerOptions = editCompilerOptions->text();
    QString labelsFilename = editLabels->text();
    QString datasetFilename = editDataset->text();
    QString datasetFolder = editFolder->text();
    // save configuration
    QString homeFolder = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0];
    QFile fileObj(homeFolder + "/" + CONFIGURATION_CACHE_FILENAME);
    if(fileObj.open(QIODevice::WriteOnly)) {
        QTextStream fileOutput(&fileObj);
        fileOutput << BUILD_VERSION << endl;
        fileOutput << serverHost << endl;
        fileOutput << serverPort << endl;
        fileOutput << prototxt << endl;
        fileOutput << caffeModel << endl;
        fileOutput << dimN << endl;
        fileOutput << dimC << endl;
        fileOutput << dimH << endl;
        fileOutput << dimW << endl;
        fileOutput << GPUs << endl;
        fileOutput << compilerOptions << endl;
        fileOutput << labelsFilename << endl;
        fileOutput << datasetFilename << endl;
        fileOutput << datasetFolder << endl;
    }
    fileObj.close();
}

inference_control::inference_control(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("Inference Control");
    setMinimumWidth(800);

    // load default configuration
    QString lastServerHost = "(pick hostname)";
    QString lastServerPort = "28282";
    QString lastPrototxt = "(pick VGG_ILSVRC_16_layers.prototxt)";
    QString lastCaffeModel = "(pick VGG_ILSVRC_16_layers.caffemodel)";
    QString lastDimN = "32";
    QString lastDimC = "3";
    QString lastDimH = "224";
    QString lastDimW = "224";
    QString lastGPUs = "1";
    QString lastCompilerOptions = "";
    QString lastLabelsFilename = "(pick ILSVRC2012_img_synset_words.txt)";
    QString lastDatasetFilename = "(pick ILSVRC2012_img_val.txt)";
    QString lastDatasetFolder = "(pick ILSVRC2012_img_val/)";
    QString homeFolder = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0];
    QFile fileObj(homeFolder + "/" + CONFIGURATION_CACHE_FILENAME);
    if(fileObj.open(QIODevice::ReadOnly)) {
        QTextStream fileInput(&fileObj);
        QString version = fileInput.readLine();
        if(version == BUILD_VERSION) {
            lastServerHost = fileInput.readLine();
            lastServerPort = fileInput.readLine();
            lastPrototxt = fileInput.readLine();
            lastCaffeModel = fileInput.readLine();
            lastDimN = fileInput.readLine();
            lastDimC = fileInput.readLine();
            lastDimH = fileInput.readLine();
            lastDimW = fileInput.readLine();
            lastGPUs = fileInput.readLine();
            lastCompilerOptions = fileInput.readLine();
            lastLabelsFilename = fileInput.readLine();
            lastDatasetFilename = fileInput.readLine();
            lastDatasetFolder = fileInput.readLine();
        }
    }
    fileObj.close();
    if(lastServerPort.toInt() <= 0) lastServerPort = "28282";
    if(lastDimW.toInt() <= 0) lastDimW = "224";
    if(lastDimH.toInt() <= 0) lastDimH = "224";
    if(lastDimC.toInt() != 3) lastDimC = "3";
    if(lastDimN.toInt() <= 0) lastDimN = "32";
    if(lastGPUs.toInt() <= 0) lastGPUs = "1";

    QGridLayout * controlLayout = new QGridLayout;
    int editSpan = 5;
    int row = 0;

    //////////////
    /// \brief labelIntro
    ///
    QLabel * labelIntro = new QLabel("ANNIE Inference Demo");
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
    /// \brief labelServer
    ///
    QLabel * labelServer = new QLabel("Inference Server");
    labelServer->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelServer, row, 0, 1, 5);
    row++;

    QLabel * labelServerHost = new QLabel("Server:");
    editServerHost = new QLineEdit(lastServerHost);
    editServerPort = new QLineEdit(lastServerPort);
    editServerPort->setValidator(new QIntValidator(1,65535));
    labelServerHost->setStyleSheet("font-weight: bold; font-style: italic");
    labelServerHost->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelServerHost, row, 0, 1, 1);
    controlLayout->addWidget(editServerHost, row, 1, 1, 3);
    controlLayout->addWidget(editServerPort, row, 4, 1, 1);
    row++;

    QDialogButtonBox * serverButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(serverButtonBox, &QDialogButtonBox::accepted, this, &inference_control::CheckServer);
    connect(serverButtonBox, &QDialogButtonBox::rejected, this, &inference_control::Cancel);
    QPushButton * okServerButton = serverButtonBox->button(QDialogButtonBox::Ok);
    QPushButton * cancelServerButton = serverButtonBox->button(QDialogButtonBox::Cancel);
    okServerButton->setText("Check Server Connection");
    cancelServerButton->setText("Exit");
    controlLayout->addWidget(serverButtonBox, row, (1 + editSpan)/2, 1, 1);
    row++;

    QFrame * sepHLine2 = new QFrame();
    sepHLine2->setFrameShape(QFrame::HLine);
    sepHLine2->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine2, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelCompiler
    ///
    QLabel * labelCompiler = new QLabel("Inference Compiler");
    labelCompiler->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelCompiler, row, 0, 1, 5);
    row++;
    QLabel * labelPrototxt = new QLabel("Prototxt:");
    editPrototxt = new QLineEdit(lastPrototxt);
    QPushButton * buttonPrototxt = new QPushButton(tr("Browse..."), this);
    connect(buttonPrototxt, &QAbstractButton::clicked, this, &inference_control::browsePrototxt);
    labelPrototxt->setStyleSheet("font-weight: bold; font-style: italic");
    labelPrototxt->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelPrototxt, row, 0, 1, 1);
    controlLayout->addWidget(editPrototxt, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonPrototxt, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelCaffeModel = new QLabel("CaffeModel:");
    editCaffeModel = new QLineEdit(lastCaffeModel);
    QPushButton * buttonCaffeModel = new QPushButton(tr("Browse..."), this);
    connect(buttonCaffeModel, &QAbstractButton::clicked, this, &inference_control::browseCaffeModel);
    labelCaffeModel->setStyleSheet("font-weight: bold; font-style: italic");
    labelCaffeModel->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelCaffeModel, row, 0, 1, 1);
    controlLayout->addWidget(editCaffeModel, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonCaffeModel, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelInputDim = new QLabel("Dimensions:");
    editDimN = new QLineEdit(lastDimN);
    editDimC = new QLineEdit(lastDimC);
    editDimH = new QLineEdit(lastDimH);
    editDimW = new QLineEdit(lastDimW);
    editDimN->setValidator(new QIntValidator(1,256));
    editDimC->setValidator(new QIntValidator(3,3));
    editDimH->setValidator(new QIntValidator(1,16384));
    editDimW->setValidator(new QIntValidator(1,16384));
    editDimC->setReadOnly(true);
    labelInputDim->setStyleSheet("font-weight: bold; font-style: italic");
    labelInputDim->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelInputDim, row, 0, 1, 1);
    controlLayout->addWidget(editDimN, row, 1, 1, 1);
    controlLayout->addWidget(editDimC, row, 2, 1, 1);
    controlLayout->addWidget(editDimH, row, 3, 1, 1);
    controlLayout->addWidget(editDimW, row, 4, 1, 1);
    row++;
    QLabel * labelGPUs = new QLabel("GPUs:");
    editGPUs = new QLineEdit(lastGPUs);
    editGPUs->setValidator(new QIntValidator(1,8));
    labelGPUs->setStyleSheet("font-weight: bold; font-style: italic");
    labelGPUs->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelGPUs, row, 0, 1, 1);
    controlLayout->addWidget(editGPUs, row, 1, 1, 1);
    row++;
    QLabel * labelOptions = new QLabel("Options:");
    editCompilerOptions = new QLineEdit(lastCompilerOptions);
    labelOptions->setStyleSheet("font-weight: bold; font-style: italic");
    labelOptions->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelOptions, row, 0, 1, 1);
    controlLayout->addWidget(editCompilerOptions, row, 1, 1, editSpan);
    row++;

    QDialogButtonBox * compilerButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(compilerButtonBox, &QDialogButtonBox::accepted, this, &inference_control::RunCompiler);
    connect(compilerButtonBox, &QDialogButtonBox::rejected, this, &inference_control::Cancel);
    okCompilerButton = compilerButtonBox->button(QDialogButtonBox::Ok);
    QPushButton * cancelCompilerButton = compilerButtonBox->button(QDialogButtonBox::Cancel);
    okCompilerButton->setText("Start Inference Compiler");
    cancelCompilerButton->setText("Exit");
    controlLayout->addWidget(compilerButtonBox, row, (1 + editSpan)/2, 1, 1);
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
    QLabel * labelLabels = new QLabel("Labels:");
    editLabels = new QLineEdit(lastLabelsFilename);
    QPushButton * buttonDatasetLabels = new QPushButton(tr("Browse..."), this);
    connect(buttonDatasetLabels, &QAbstractButton::clicked, this, &inference_control::browseDatasetLabels);
    labelLabels->setStyleSheet("font-weight: bold; font-style: italic");
    labelLabels->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelLabels, row, 0, 1, 1);
    controlLayout->addWidget(editLabels, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDatasetLabels, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelDataset = new QLabel("Dataset:");
    editDataset = new QLineEdit(lastDatasetFilename);
    QPushButton * buttonDatasetFilename = new QPushButton(tr("Browse..."), this);
    connect(buttonDatasetFilename, &QAbstractButton::clicked, this, &inference_control::browseDatasetFilename);
    labelDataset->setStyleSheet("font-weight: bold; font-style: italic");
    labelDataset->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelDataset, row, 0, 1, 1);
    controlLayout->addWidget(editDataset, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDatasetFilename, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelFolder = new QLabel("Folder:");
    editFolder = new QLineEdit(lastDatasetFolder);
    QPushButton * buttonDatasetFolder = new QPushButton(tr("Browse..."), this);
    connect(buttonDatasetFolder, &QAbstractButton::clicked, this, &inference_control::browseDatasetFolder);
    labelFolder->setStyleSheet("font-weight: bold; font-style: italic");
    labelFolder->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelFolder, row, 0, 1, 1);
    controlLayout->addWidget(editFolder, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDatasetFolder, row, 1 + editSpan, 1, 1);
    row++;

    QDialogButtonBox * runtimeButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(runtimeButtonBox, &QDialogButtonBox::accepted, this, &inference_control::RunViewer);
    connect(runtimeButtonBox, &QDialogButtonBox::rejected, this, &inference_control::Cancel);
    QPushButton * okRuntimeButton = runtimeButtonBox->button(QDialogButtonBox::Ok);
    QPushButton * cancelRuntimeButton = runtimeButtonBox->button(QDialogButtonBox::Cancel);
    okRuntimeButton->setText("Start Inference Run-time");
    cancelRuntimeButton->setText("Exit");
    controlLayout->addWidget(runtimeButtonBox, row, (1 + editSpan)/2, 1, 1);
    row++;

    setLayout(controlLayout);
}

void inference_control::browsePrototxt()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, tr("Caffe ProtoTxt (*.prototxt)"));
    if(fileName.size() > 0)
        editPrototxt->setText(fileName);
}

void inference_control::browseCaffeModel()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, tr("Caffe Model (*.caffemodel)"));
    if(fileName.size() > 0)
        editCaffeModel->setText(fileName);
}

void inference_control::browseDatasetLabels()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, tr("Text (*.txt)"));
    if(fileName.size() > 0)
        editLabels->setText(fileName);
}

void inference_control::browseDatasetFilename()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, tr("Text (*.txt)"));
    if(fileName.size() > 0)
        editDataset->setText(fileName);
}

void inference_control::browseDatasetFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), nullptr,
                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.size() > 0)
        editFolder->setText(dir);
}

void inference_control::Cancel()
{
    close();
}

void inference_control::CheckServer()
{
    // check configuration
    QString err;
    if(!isConfigValid(err)) {
        QMessageBox::critical(this, windowTitle(), err, QMessageBox::Ok);
        return;
    }
    // save configuration
    saveConfig();

    // check TCP connection
    QTcpSocket * tcpSocket = new QTcpSocket(this);
    tcpSocket->connectToHost(editServerHost->text(), editServerPort->text().toInt());
    if(tcpSocket->waitForConnected(3000)) {
        QMessageBox::information(this, windowTitle(),
            "OK: Connected to " + editServerHost->text() + ":" + editServerPort->text(), QMessageBox::Ok);
    }
    else {
        QMessageBox::critical(this, windowTitle(),
            "ERROR: Unable to connect to " + editServerHost->text() + ":" + editServerPort->text(), QMessageBox::Ok);
    }
    tcpSocket->close();
}

void inference_control::RunCompiler()
{
    // check configuration
    QString err;
    if(!isConfigValid(err)) {
        QMessageBox::critical(this, windowTitle(), err, QMessageBox::Ok);
        return;
    }
    // save configuration
    saveConfig();

    // disable compiler button
    okCompilerButton->setEnabled(false);

    // start compiler
    inference_compiler * compiler = new inference_compiler(
                editServerHost->text(), editServerPort->text().toInt(),
                editPrototxt->text(), editCaffeModel->text(),
                editDimN->text().toInt(),
                editDimC->text().toInt(),
                editDimH->text().toInt(),
                editDimW->text().toInt(),
                editGPUs->text().toInt(),
                editCompilerOptions->text());
    compiler->show();
}

void inference_control::RunViewer()
{
    // check configuration
    QString err;
    if(!isConfigValid(err)) {
        QMessageBox::critical(this, windowTitle(), err, QMessageBox::Ok);
        return;
    }
    // save configuration
    saveConfig();

    // start viewer
    int dim[4] = { editDimW->text().toInt(), editDimH->text().toInt(), editDimC->text().toInt(), editDimN->text().toInt() };
    int gpuCount = editGPUs->text().toInt();
    inference_viewer * viewer = new inference_viewer(
                editServerHost->text(), editServerPort->text().toInt(),
                editLabels->text(), editDataset->text(), editFolder->text(),
                dim, gpuCount);
    viewer->show();
    close();
}
