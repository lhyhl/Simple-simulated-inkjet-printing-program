#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "photoalbum.h"
#include "bin.h"
#include "tcp.h"
#include "print.h"
#include "printrecord.h"
#include "inkjet.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QStackedWidget *stack;
    QWidget *homeWidget;
    Photoalbum *albumWidget;
    Bin *binWidget;
    Print *printWidget;
    PrintRecord *printRecordWidget;
    Tcp *m_tcpServer;
    Inkjet *m_inkjet;
};