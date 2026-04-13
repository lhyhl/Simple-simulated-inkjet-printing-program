#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "printworkflow.h"
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QPushButton>
#include "inkjet.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    stack = new QStackedWidget(this);

    homeWidget = ui->centralwidget;
    stack->addWidget(homeWidget);

    albumWidget = new Photoalbum();
    stack->addWidget(albumWidget);

    binWidget = new Bin();
    stack->addWidget(binWidget);

    m_inkjet = new Inkjet(this);

    printWidget = new Print();
    stack->addWidget(printWidget);

    printRecordWidget = new PrintRecord();
    stack->addWidget(printRecordWidget);

    // 与 TCP 打印共用同一 Inkjet；记录写入「打印记录」页，勿在 Print 内再 new 带界面的 PrintRecord
    printWidget->setInkjet(m_inkjet);
    printWidget->setPrintRecord(printRecordWidget);

    setCentralWidget(stack);

    m_tcpServer = new Tcp(this);

    if (m_tcpServer->startServer(8888))
    {
        QMessageBox::information(this, "TCP 服务器", "TCP 文件接收服务器已启动\n监听端口：8888");
    }
    else
    {
        QMessageBox::warning(this, "TCP 服务器", "TCP 服务器启动失败！");
    }

    connect(m_tcpServer, &Tcp::clientConnected, this, [this]()
            { qDebug() << "客户端已连接"; });

    connect(m_tcpServer, &Tcp::clientDisconnected, this, [this]()
            { qDebug() << "客户端已断开"; });

    connect(m_tcpServer, &Tcp::fileReceived, this, [this](const QString &fileName, const QString &fileType, int fileSize)
            {
        qDebug() << "收到文件:" << fileName << "类型:" << fileType << "大小:" << fileSize;

        if (fileType.toLower() == "bmp") {
            QString savePath = QDir::currentPath() + "/received_files/" + fileName;
            albumWidget->addImage(savePath);
        }

        QMessageBox::information(this, "文件接收成功",
            QString("收到文件：%1\n类型：%2\n大小：%3 字节").arg(fileName).arg(fileType).arg(fileSize)); });

    connect(m_tcpServer, &Tcp::fileReceivedWithPath, this, [this](const QString &filePath, const QString &originalType)
            {
        qDebug() << "收到文件 (带路径):" << filePath << "原始类型:" << originalType;

        if (filePath.endsWith(".bmp", Qt::CaseInsensitive)) {
            albumWidget->addImage(filePath);
        }

        QMessageBox msgBox(this);
        msgBox.setWindowTitle("收到文件");
        msgBox.setText(QString("收到文件：%1\n原始格式：%2").arg(QFileInfo(filePath).fileName()).arg(originalType));
        msgBox.setInformativeText("请选择操作:");

        QAbstractButton *bwPrintBtn = msgBox.addButton("黑白打印", QMessageBox::AcceptRole);
        QAbstractButton *cmykPrintBtn = msgBox.addButton("彩色打印 (CMYK)", QMessageBox::AcceptRole);
        QAbstractButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);

        msgBox.exec();

        QAbstractButton *clicked = msgBox.clickedButton();

        if (clicked == bwPrintBtn || clicked == cmykPrintBtn) {
            QString mode;
            PrintMode printMode;
            if (clicked == bwPrintBtn) {
                mode = "黑白打印";
                printMode = PrintMode::BlackWhite;
            } else {
                mode = "彩色打印 (CMYK)";
                printMode = PrintMode::CMYK;
            }

            qDebug() << "准备打印图片:" << filePath << "打印模式:" << mode;

            PrintWorkflow::startPreviewFromPaths(
                this,
                m_inkjet,
                printRecordWidget,
                QStringList() << filePath,
                printMode,
                mode);
        } });

    connect(m_tcpServer, &Tcp::errorOccurred, this, [this](const QString &error)
            { QMessageBox::warning(this, "TCP 错误", error); });

    connect(ui->albumBtn, &QPushButton::clicked, [=]()
            { stack->setCurrentWidget(albumWidget); });

    connect(albumWidget, &Photoalbum::backRequested, [=]()
            { stack->setCurrentWidget(homeWidget); });

    connect(ui->binBtn, &QPushButton::clicked, [=]()
            { stack->setCurrentWidget(binWidget); });

    connect(binWidget, &Bin::BinbackRequested, [=]()
            { stack->setCurrentWidget(homeWidget); });

    connect(binWidget, &Bin::imageRestored, albumWidget, [this](const QString &filePath)
            {
        stack->setCurrentWidget(albumWidget);
        albumWidget->addImage(filePath); });

    connect(ui->PrintBtn, &QPushButton::clicked, [=]()
            { stack->setCurrentWidget(printWidget); });

    connect(printWidget, &Print::backRequested, [=]()
            { stack->setCurrentWidget(homeWidget); });

    connect(ui->PrintRecordBtn, &QPushButton::clicked, [=]()
            { stack->setCurrentWidget(printRecordWidget); });

    connect(printRecordWidget, &PrintRecord::backRequested, [=]()
            { stack->setCurrentWidget(homeWidget); });
}

MainWindow::~MainWindow()
{
    delete ui;
}