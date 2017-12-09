#include "inference_panel.h"
#include "ui_inference_panel.h"

inference_panel::inference_panel(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::inference_panel)
{
    ui->setupUi(this);
}

inference_panel::~inference_panel()
{
    delete ui;
}
