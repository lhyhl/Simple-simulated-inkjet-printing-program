#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QTimer>
#include <QTcpSocket>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ===============================
    // TCP客户端初始化
    // ===============================
    int bufferSize = 1024 * 1024; // 1MB

    m_clientSocket = new QTcpSocket(this);

    // 连接服务器
    m_clientSocket->connectToHost("127.0.0.1", 8888);

    this->setWindowTitle("TCP 客户端 - 正在连接服务器...");

    // 连接成功
    connect(m_clientSocket, &QTcpSocket::connected, this, [this]()
            {
                qDebug() << "已连接服务器";
                this->setWindowTitle("TCP 客户端 - 已连接服务器 127.0.0.1:8888");
            });

    // 接收服务器返回数据
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &MainWindow::readData);

    // 断开连接
    connect(m_clientSocket, &QTcpSocket::disconnected, this, [this]()
            {
                qDebug() << "服务器断开";
                this->setWindowTitle("TCP 客户端 - 服务器断开");
            });

    // 连接失败
    connect(m_clientSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError)
            {
                QMessageBox::warning(this, "连接失败", m_clientSocket->errorString());
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}

/*
 * 发送文件按钮
 * 功能：
 * 选择 pdf/bmp 文件
 * 转为 Base64
 * 封装 JSON
 * 通过 TCP 发送给服务器
 */
void MainWindow::on_SendBtn_clicked()
{
    // ===============================
    // 检查是否连接服务器
    // ===============================

    if (m_clientSocket->state() != QAbstractSocket::ConnectedState)
    {
        QMessageBox::warning(this, "未连接", "当前未连接服务器！");
        return;
    }

    // ===============================
    // 打开文件选择框
    // ===============================

    QString filter =
        "支持的文件 (*.pdf *.bmp);;"
        "PDF (*.pdf);;"
        "BMP (*.bmp)";

    QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        "选择要发送的文件",
        "",
        filter
        );

    if (filePaths.isEmpty())
        return;

    // ===============================
    // 过滤合法文件
    // ===============================

    QList<QString> validFiles;

    for (const QString &path : filePaths)
    {
        QFileInfo info(path);
        QString suffix = info.suffix().toLower();

        if (suffix == "pdf" || suffix == "bmp")
        {
            validFiles.append(path);
        }
        else
        {
            QMessageBox::information(this, "提示", "跳过不支持文件：" + path);
        }
    }

    if (validFiles.isEmpty())
        return;

    // ===============================
    // 发送JSON
    // ===============================

    sendFilesAsJson(validFiles);

    ui->SendBtn->setText("发送成功");

    QTimer::singleShot(2000, this, [this]()
                       {
                           ui->SendBtn->setText("发送文件");
                       });
}

/*
 * 接收服务器返回数据
 */
void MainWindow::readData()
{
    QByteArray data = m_clientSocket->readAll();

    qDebug() << "服务器返回:" << data;

    QMessageBox::information(this, "服务器消息", QString(data));
}

/*
 * 文件转Base64
 */
QString MainWindow::fileToBase64(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "无法打开文件:" << filePath;
        return "";
    }

    QByteArray rawData = file.readAll();

    file.close();

    return rawData.toBase64();
}

/*
 * 封装JSON并发送
 */
void MainWindow::sendFilesAsJson(const QList<QString> &filePaths)
{
    QJsonArray fileArray;

    for (const QString &path : filePaths)
    {
        QFileInfo info(path);

        QString base64Content = fileToBase64(path);

        if (base64Content.isEmpty())
            continue;

        QJsonObject fileObj;

        fileObj["fileName"] = info.fileName();
        fileObj["fileSize"] = info.size();
        fileObj["type"] = info.suffix().toLower();
        fileObj["content"] = base64Content;
        fileObj["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        fileArray.append(fileObj);
    }

    QJsonObject root;

    root["cmd"] = "send_files";
    root["count"] = fileArray.size();
    root["data"] = fileArray;

    QJsonDocument doc(root);

    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    // ===============================
    // 发送给服务器
    // ===============================

    m_clientSocket->write(jsonData);

    qDebug() << "已发送文件 JSON 大小:" << jsonData.size();
}