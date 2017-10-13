#ifndef INFERENCE_CONTROL_H
#define INFERENCE_CONTROL_H

#include "inference_viewer.h"
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

class inference_control : public QWidget
{
    Q_OBJECT
public:
    explicit inference_control(QWidget *parent = nullptr);

signals:

public slots:

protected:
    void CheckServer();
    void RunCompiler();
    void RunViewer();
    void Cancel();
    void browsePrototxt();
    void browseCaffeModel();
    void browseDatasetLabels();
    void browseDatasetFilename();
    void browseDatasetFolder();
    bool isConfigValid(QString& err);
    void saveConfig();

private:
    QLineEdit * editServerHost;
    QLineEdit * editServerPort;
    QLineEdit * editPrototxt;
    QLineEdit * editCaffeModel;
    QLineEdit * editDimN;
    QLineEdit * editDimC;
    QLineEdit * editDimH;
    QLineEdit * editDimW;
    QLineEdit * editGPUs;
    QLineEdit * editCompilerOptions;
    QLineEdit * editLabels;
    QLineEdit * editDataset;
    QLineEdit * editFolder;
    QPushButton * okCompilerButton;
};

#endif // INFERENCE_CONTROL_H
