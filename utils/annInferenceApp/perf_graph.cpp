#include "perf_graph.h"
#include "ui_perf_graph.h"

perf_graph::perf_graph(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::perf_graph)
{
    ui->setupUi(this);
}

perf_graph::~perf_graph()
{
    delete ui;
}
