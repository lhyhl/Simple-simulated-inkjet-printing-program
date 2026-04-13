#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    // 发送文件按钮
    void on_SendBtn_clicked();

    // 接收服务器数据
    void readData();

private:

    Ui::MainWindow *ui;

    QTcpSocket *m_clientSocket;   // TCP客户端

    QString fileToBase64(const QString &filePath);

    void sendFilesAsJson(const QList<QString> &filePaths);
};

#endif