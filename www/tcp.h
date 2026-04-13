#ifndef TCP_H
#define TCP_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QEventLoop>

class Tcp : public QObject
{
    Q_OBJECT

public:
    explicit Tcp(QObject *parent = nullptr);
    ~Tcp();

    // 启动服务器监听
    bool startServer(quint16 port = 8888);
    // 停止服务器
    void stopServer();
    // 获取服务器状态
    bool isRunning() const;

signals:
    void clientConnected();
    void clientDisconnected();
    void fileReceived(const QString &fileName, const QString &fileType, int fileSize);
    void fileReceivedWithPath(const QString &filePath, const QString &originalType);
    void errorOccurred(const QString &error);

private slots:
    // 有新客户端连接
    void onNewConnection();
    // 接收客户端数据
    void readData();
    // 客户端断开连接
    void onDisconnected();

private:
    // 解析接收到的 JSON 数据
    void parseJson(const QJsonDocument &doc);
    // 处理文件数据
    void handleFiles(const QJsonArray &fileArray);
    // Base64 解码并保存文件
    bool saveFileFromBase64(const QString &fileName, const QString &base64Content, const QString &saveDir);

    void processBuffer();

private:
    QTcpServer *m_server;
    QTcpSocket *m_clientSocket;
    bool m_isRunning;
    QByteArray m_buffer;
};

#endif // TCP_H
