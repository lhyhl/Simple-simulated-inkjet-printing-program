#ifndef PRINTRECORD_H
#define PRINTRECORD_H

#include <QWidget>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QListWidgetItem>
#include <QDateTime>
#include <QDebug>

namespace Ui
{
    class PrintRecord;
}

struct PrintRecordItem
{
    QString fileName;
    QString originalFormat;
    QString printMode;
    QString printTime;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["fileName"] = fileName;
        obj["originalFormat"] = originalFormat;
        obj["printMode"] = printMode;
        obj["printTime"] = printTime;
        return obj;
    }

    static PrintRecordItem fromJson(const QJsonObject &obj)
    {
        PrintRecordItem item;
        item.fileName = obj["fileName"].toString();
        item.originalFormat = obj["originalFormat"].toString();
        item.printMode = obj["printMode"].toString();
        item.printTime = obj["printTime"].toString();
        return item;
    }
};

class PrintRecord : public QWidget
{
    Q_OBJECT

public:
    explicit PrintRecord(QWidget *parent = nullptr);
    ~PrintRecord();

    void addRecord(const QString &fileName, const QString &originalFormat, const QString &printMode);
    void loadRecords();
    void saveRecords();

signals:
    void backRequested();

private:
    Ui::PrintRecord *ui;
    QList<PrintRecordItem> m_records;
    QString m_recordFilePath;

    void updateListWidget();
};

#endif // PRINTRECORD_H