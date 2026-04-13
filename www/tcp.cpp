#include "tcp.h"
#include "convertfile.h"
#include "print.h"
#include <QMessageBox>
#include <QDialog>

Tcp::Tcp(QObject *parent)
    : QObject(parent), m_server(nullptr), m_clientSocket(nullptr), m_isRunning(false), m_buffer()
{
    m_server = new QTcpServer(this);

    connect(m_server, &QTcpServer::newConnection, this, &Tcp::onNewConnection);
}

Tcp::~Tcp()
{
    stopServer();
}

// 打开tcp
bool Tcp::startServer(quint16 port)
{
    if (m_server->listen(QHostAddress::Any, port))
    {
        m_isRunning = true;
        qDebug() << "TCP 服务器已启动，监听端口:" << port;
        return true;
    }
    else
    {
        qDebug() << "TCP 服务器启动失败:" << m_server->errorString();
        emit errorOccurred(m_server->errorString());
        return false;
    }
}

// 停止服务器
void Tcp::stopServer()
{
    if (m_clientSocket)
    {
        m_clientSocket->disconnectFromHost();
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }

    if (m_server)
    {
        m_server->close();
    }

    m_isRunning = false;
    qDebug() << "TCP 服务器已停止";
}

// 判断是否运行
bool Tcp::isRunning() const
{
    return m_isRunning;
}

// 连接客户端
void Tcp::onNewConnection()
{
    if (m_clientSocket)
    {
        m_clientSocket->deleteLater();
    }

    m_clientSocket = m_server->nextPendingConnection();

    qDebug() << "客户端已连接:" << m_clientSocket->peerAddress().toString()
             << ":" << m_clientSocket->peerPort();

    emit clientConnected();

    connect(m_clientSocket, &QTcpSocket::readyRead, this, &Tcp::readData);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &Tcp::onDisconnected);
    connect(m_clientSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError)
            {
                qDebug() << "客户端错误:" << m_clientSocket->errorString();
                emit errorOccurred(m_clientSocket->errorString()); });

    m_clientSocket->write("欢迎连接到 TCP 文件接收服务器！\r\n");
}

// 读取数据
void Tcp::readData()
{
    if (!m_clientSocket)
        return;

    QByteArray data = m_clientSocket->readAll();

    qDebug() << "接收到数据，大小:" << data.size() << "字节";

    if (data.isEmpty())
        return;

    m_buffer.append(data);

    processBuffer();
}

// 客户端断开连接
void Tcp::onDisconnected()
{
    qDebug() << "客户端已断开连接";

    if (m_clientSocket)
    {
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }

    m_buffer.clear();

    emit clientDisconnected();
}

// 解析接收到的 JSON 数据
void Tcp::parseJson(const QJsonDocument &doc)
{
    if (!doc.isObject())
    {
        qDebug() << "JSON 格式错误：不是对象";
        return;
    }

    QJsonObject root = doc.object();

    QString cmd = root["cmd"].toString();

    qDebug() << "收到命令:" << cmd;

    if (cmd == "send_files")
    {
        int count = root["count"].toInt();
        QJsonArray fileArray = root["data"].toArray();

        qDebug() << "收到文件数量:" << count << ", 实际数组大小:" << fileArray.size();

        handleFiles(fileArray);
    }
    else
    {
        qDebug() << "未知命令:" << cmd;
    }
}

// 解析JSON
void Tcp::processBuffer()
{
    qDebug() << "processBuffer 被调用, 缓冲区大小:" << m_buffer.size();

    if (m_buffer.isEmpty())
    {
        return;
    }

    while (m_buffer.size() > 0)
    {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(m_buffer, &parseError);

        if (parseError.error == QJsonParseError::NoError)
        {
            qDebug() << "成功解析JSON，缓冲区剩余:" << m_buffer.size();
            m_buffer.clear();
            parseJson(doc);
            return;
        }
        else if (parseError.error == QJsonParseError::GarbageAtEnd)
        {
            qDebug() << "发现多个JSON片段，尝试提取第一个";
            QByteArray validJson = m_buffer.left(parseError.offset);
            QJsonDocument doc2 = QJsonDocument::fromJson(validJson, &parseError);
            if (parseError.error == QJsonParseError::NoError)
            {
                qDebug() << "提取到有效JSON，长度:" << validJson.size();
                m_buffer = m_buffer.mid(parseError.offset);
                parseJson(doc2);
                continue;
            }
            else
            {
                qDebug() << "无法提取有效JSON，等待更多数据";
                return;
            }
        }
        else
        {
            qDebug() << "JSON 解析失败:" << parseError.errorString() << "，等待更多数据";
            return;
        }
    }
}

// 处理文件数据
void Tcp::handleFiles(const QJsonArray &fileArray)
{
    QString saveDir = QDir::currentPath() + "/received_files";

    qDebug() << "当前工作目录:" << QDir::currentPath();
    qDebug() << "文件保存目录:" << saveDir;

    QDir dir;
    if (!dir.exists(saveDir))
    {
        bool created = dir.mkpath(saveDir);
        qDebug() << "创建目录结果:" << created;
    }
    else
    {
        qDebug() << "目录已存在";
    }

    QStringList receivedFilePaths;
    QStringList pdfFiles;

    for (const QJsonValue &value : fileArray)
    {
        QJsonObject fileObj = value.toObject();

        QString fileName = fileObj["fileName"].toString();
        QString fileType = fileObj["type"].toString();
        int fileSize = fileObj["fileSize"].toInt();
        QString base64Content = fileObj["content"].toString();
        QString time = fileObj["time"].toString();

        qDebug() << "处理文件:" << fileName
                 << "类型:" << fileType
                 << "大小:" << fileSize
                 << "Base64长度:" << base64Content.length()
                 << "时间:" << time;

        if (fileType.toLower() != "pdf" && fileType.toLower() != "bmp")
        {
            qDebug() << "跳过不支持的文件类型:" << fileType;
            continue;
        }

        if (saveFileFromBase64(fileName, base64Content, saveDir))
        {
            qDebug() << "文件保存成功:" << fileName;
            emit fileReceived(fileName, fileType, fileSize);

            QString savedPath = saveDir + "/" + fileName;
            receivedFilePaths.append(savedPath);

            if (fileType.toLower() == "pdf")
            {
                pdfFiles.append(savedPath);
            }
        }
        else
        {
            qDebug() << "文件保存失败:" << fileName;
        }
    }

    for (const QString &pdfPath : pdfFiles)
    {
        ConvertFile converter;
        QObject::connect(&converter, &ConvertFile::finished, [this, pdfPath]()
                         {
            QString bmpPath = pdfPath;
            bmpPath.replace(QRegExp("\\.pdf$", Qt::CaseInsensitive), ".bmp");
            emit fileReceivedWithPath(bmpPath, "PDF"); });
        QEventLoop loop;
        QObject::connect(&converter, &ConvertFile::finished, &loop, &QEventLoop::quit);
        converter.pdfToBmp(pdfPath, QFileInfo(pdfPath).dir().path());
        loop.exec();
    }

    for (const QString &filePath : receivedFilePaths)
    {
        if (filePath.endsWith(".bmp", Qt::CaseInsensitive))
        {
            emit fileReceivedWithPath(filePath, "BMP");
        }
    }
}

// Base64 解码并保存文件
bool Tcp::saveFileFromBase64(const QString &fileName, const QString &base64Content, const QString &saveDir)
{
    if (base64Content.isEmpty())
    {
        qDebug() << "Base64 内容为空:" << fileName;
        return false;
    }

    QString filePath = QDir::toNativeSeparators(saveDir + "/" + fileName);
    qDebug() << "完整文件路径:" << filePath;

    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "无法创建文件:" << filePath << "错误:" << file.errorString();
        return false;
    }

    QByteArray decodedData = QByteArray::fromBase64(base64Content.toUtf8());

    qDebug() << "Base64解码前长度:" << base64Content.length() << "解码后长度:" << decodedData.size();

    if (decodedData.isEmpty())
    {
        qDebug() << "Base64 解码失败:" << fileName;
        file.close();
        return false;
    }

    qint64 bytesWritten = file.write(decodedData);

    file.close();

    if (bytesWritten == -1)
    {
        qDebug() << "文件写入失败:" << fileName << "错误:" << file.errorString();
        return false;
    }

    qDebug() << "文件已保存:" << filePath << "实际大小:" << bytesWritten << "字节";

    return true;
}
