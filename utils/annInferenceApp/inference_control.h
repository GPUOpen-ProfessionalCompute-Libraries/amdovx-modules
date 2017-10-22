#ifndef INFERENCE_CONTROL_H
#define INFERENCE_CONTROL_H

#include "inference_viewer.h"
#include "infcom.h"
#include "inference_compiler.h"
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>

class inference_control : public QWidget
{
    Q_OBJECT
public:
    explicit inference_control(int operationMode, QWidget *parent = nullptr);

signals:

public slots:
    void tick();
    void connectServer();
    void modelSelect(int model);
    void browseModelFile1();
    void browseModelFile2();
    void modelUpload();
    void runInference();
    void exitControl();
    void onChangeDimH(const QString &);
    void onChangeDimW(const QString &);
    void onChangeModelFile1(const QString &);
    void onChangeModelFile2(const QString &);
    void onChangeCompilerOptions(const QString &);
    void onLogo1Click();
    void onLogo2Click();

protected:
    void browseDataLabels();
    void browseDataFilename();
    void browseDataFolder();
    bool isConfigValid(QString& err);
    void saveConfig();
    void loadConfig();

private:
    struct ModelInfo {
        QString name;
        int inputDim[3];
        int outputDim[3];
    };
    QLineEdit * editServerHost;
    QLineEdit * editServerPort;
    QPushButton * buttonConnect;
    QLabel * labelServerStatus;
    QComboBox * comboModelSelect;
    QPushButton * buttonModelUpload;
    QLineEdit * editDimH;
    QLineEdit * editDimW;
    QLineEdit * editOutDimC;
    QLineEdit * editOutDimH;
    QLineEdit * editOutDimW;
    QLabel * labelModelFile1;
    QLabel * labelModelFile2;
    QLabel * labelCompilerOptions;
    QLineEdit * editModelFile1;
    QLineEdit * editModelFile2;
    QLineEdit * editCompilerOptions;
    QPushButton * buttonModelFile1;
    QPushButton * buttonModelFile2;
    QLabel * labelCompilerStatus;
    QLineEdit * editGPUs;
    QLabel * labelMaxGPUs;
    QPushButton * buttonRunInference;
    QLineEdit * editImageLabelsFile;
    QLineEdit * editImageFolder;
    QLineEdit * editImageListFile;
    QLineEdit * editMaxDataSize;
    QCheckBox * checkRepeatImages;
    inference_compiler_status compiler_status;
    bool operationMode;
    bool connectionSuccessful;
    int modelType;
    int numModelTypes;
    int maxGPUs;
    QVector<QString> * dataLabels;
    QVector<QString> typeModelFile1Label;
    QVector<QString> typeModelFile1Desc;
    QVector<QString> typeModelFile2Label;
    QVector<QString> typeModelFile2Desc;
    QVector<ModelInfo> modelList;
    QString lastModelFile1;
    QString lastModelFile2;
    QString lastCompilerOptions;
    QString lastDimW;
    QString lastDimH;
};

#endif // INFERENCE_CONTROL_H
