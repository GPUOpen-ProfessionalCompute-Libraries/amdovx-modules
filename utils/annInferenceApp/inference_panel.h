#ifndef INFERENCE_PANEL_H
#define INFERENCE_PANEL_H

#include <QDialog>

namespace Ui {
class inference_panel;
}

class inference_panel : public QDialog
{
    Q_OBJECT

public:
    explicit inference_panel(QWidget *parent = 0);
    ~inference_panel();

private:
    Ui::inference_panel *ui;
};

#endif // INFERENCE_PANEL_H
