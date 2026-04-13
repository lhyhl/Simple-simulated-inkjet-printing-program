#include "printrecord.h"
#include "ui_printrecord.h"
#include <QDir>


/*
 * @brief 构造函数
 *
 * 初始化 UI 组件，连接“返回”按钮信号，并加载已有的打印记录。
 *
 * @param parent 父级 QWidget
 */
PrintRecord::PrintRecord(QWidget *parent)
    : QWidget(parent), ui(new Ui::PrintRecord)
{
    ui->setupUi(this);

    // 连接返回按钮点击信号到自定义的 backRequested 信号
    connect(ui->BackBtn, &QPushButton::clicked,
            this, &PrintRecord::backRequested);

     // 设置记录文件路径为程序当前目录下的 JSON 文件
    m_recordFilePath = QDir::currentPath() + "/print_records.json";
    // 尝试从文件加载历史记录
    loadRecords();
}

/*
 * @brief 析构函数
 *
 * 在对象销毁前，确保将内存中的最新记录保存回文件，
 * 然后释放 UI 资源。
 */
PrintRecord::~PrintRecord()
{
    saveRecords();  // 保存数据
    delete ui;      // 释放 UI 内存
}


/*
 * @brief 添加一条新的打印记录
 *
 * 创建包含文件名、格式、模式和当前时间的记录对象，
 * 添加到内存列表中，刷新界面显示，并立即持久化到磁盘。
 *
 * @param fileName 打印的文件名
 * @param originalFormat 原始文件格式 (如 PDF/BMP)
 * @param printMode 打印模式 (如 黑白/彩色)
 */
void PrintRecord::addRecord(const QString &fileName, const QString &originalFormat, const QString &printMode)
{
    PrintRecordItem record;
    record.fileName = fileName;
    record.originalFormat = originalFormat;
    record.printMode = printMode;
    // 生成当前时间字符串，格式：2026-03-30 14:30:00
    record.printTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // 1. 添加到内存列表
    m_records.append(record);
    // 2. 更新列表控件显示
    updateListWidget();
    // 3. 立即保存到文件（确保不丢失）
    saveRecords();
}

/*
 * @brief 从 JSON 文件加载历史记录
 *
 * 读取文件内容，解析 JSON 数组，并将每个对象转换为 PrintRecordItem
 * 存入内存列表 m_records。
 */
void PrintRecord::loadRecords()
{
    QFile file(m_recordFilePath);

    // 尝试只读打开文件
    if (!file.open(QIODevice::ReadOnly))
    {
        // 如果文件不存在
        return;
    }

    // 读取全部数据
    QByteArray data = file.readAll();
    file.close();

    // 解析 JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    // 检查 JSON 根节点是否为数组
    if (!doc.isArray())
    {
        return;
    }

    QJsonArray array = doc.array();
    // 遍历数组中的每个元素
    for (const QJsonValue &value : array)
    {
        if (value.isObject())
        {
            // 利用 PrintRecordItem 的静态方法将 JSON 对象转为 C++ 对象
            m_records.append(PrintRecordItem::fromJson(value.toObject()));
        }
    }

    // 加载完成后刷新界面
    updateListWidget();
}


/*
 * @brief 将内存记录保存到 JSON 文件
 *
 * 将内存中的 m_records 列表转换为 JSON 数组，
 * 序列化后写入磁盘文件。
 */
void PrintRecord::saveRecords()
{
    QJsonArray array;
    // 遍历内存记录列表
    for (const PrintRecordItem &record : m_records)
    {
        // 将 C++ 对象转换为 JSON 对象并添加到数组
        array.append(record.toJson());
    }

    // 构建 JSON 文档
    QJsonDocument doc(array);

    // 尝试写入打开文件
    QFile file(m_recordFilePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }

    // 写入格式化的 JSON 数据
    file.write(doc.toJson());
    file.close();
}

/*
 * @brief 更新列表控件 (View)
 *
 * 清空当前列表，并根据 m_records 中的数据重新生成列表项。
 * 这是一个典型的“同步 Model 到 View”的操作。
 */
void PrintRecord::updateListWidget()
{
     // 1. 清空旧数据
    ui->listWidget->clear();

    // 2. 遍历内存数据模型
    for (const PrintRecordItem &record : m_records)
    {
        // 3. 格式化显示文本
        // 格式示例： "document.pdf | PDF | 彩色打印 | 2026-03-30 14:30:00"
        QString displayText = QString("%1 | %2 | %3 | %4")
                                  .arg(record.fileName)
                                  .arg(record.originalFormat)
                                  .arg(record.printMode)
                                  .arg(record.printTime);
        // 4. 添加新项
        ui->listWidget->addItem(displayText);
    }
}