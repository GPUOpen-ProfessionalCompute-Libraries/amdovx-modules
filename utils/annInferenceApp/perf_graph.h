#ifndef PERF_GRAPH_H
#define PERF_GRAPH_H

#include <QDialog>

namespace Ui {
class perf_graph;
}

class perf_graph : public QDialog
{
    Q_OBJECT

public:
    explicit perf_graph(QWidget *parent = 0);
    ~perf_graph();

private:
    Ui::perf_graph *ui;
};

#endif // PERF_GRAPH_H
