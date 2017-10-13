#ifndef INFERENCE_COMPILER_H
#define INFERENCE_COMPILER_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <mutex>

struct model_uploader_status {
    bool completed;
    float prototxtUploadProgress;
    float caffeModelUploadProgress;
    float compilationProgress;
    QString message;
};

class inference_model_uploader : public QObject
{
    Q_OBJECT
public:
    explicit inference_model_uploader(
            QString serverHost, int serverPort,
            QString prototxt, QString caffeModel,
            int n, int c, int h, int w, int gpuCount,
            QString compilerOptions,
            model_uploader_status * progress,
            QObject *parent = nullptr);
    ~inference_model_uploader();

    static void abort();

signals:
    void finished();
    void error(QString err);

public slots:
    void run();

private:
    static bool abortRequsted;

private:
    std::mutex mutex;
    // config
    QString serverHost;
    int serverPort;
    QString prototxt;
    QString caffeModel;
    int dimN;
    int dimC;
    int dimH;
    int dimW;
    int gpuCount;
    QString compilerOptions;
    model_uploader_status * progress;
};

class inference_compiler : public QWidget
{
    Q_OBJECT
public:
    explicit inference_compiler(
            QString serverHost, int serverPort,
            QString prototxt, QString caffeModel,
            int n, int c, int h, int w, int gpuCount,
            QString compilerOptions,
            QWidget *parent = nullptr);

protected:
    void Ok();
    void Cancel();
    void startModelUploader();

signals:

public slots:
    void tick();
    void errorString(QString err);

private:
    // config
    QString serverHost;
    int serverPort;
    QString prototxt;
    QString caffeModel;
    int dimN;
    int dimC;
    int dimH;
    int dimW;
    int gpuCount;
    QString compilerOptions;
    // status
    QLabel * labelStatus;
    QLineEdit * editPrototxtUploadProgress;
    QLineEdit * editCaffeModelUploadProgress;
    QLineEdit * editCompilerProgress;
    QLineEdit * editCompilerMessage;
    QPushButton * okCompilerButton;
    QPushButton * cancelCompilerButton;
    model_uploader_status progress;
    inference_model_uploader * worker;
};

#endif // INFERENCE_COMPILER_H
