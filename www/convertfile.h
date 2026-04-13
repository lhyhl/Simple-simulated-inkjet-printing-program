#ifndef CONVERTFILE_H
#define CONVERTFILE_H

#include <QObject>
#include <QString>
#include <QImage>

class ConvertFile : public QObject
{
    Q_OBJECT

public:
    explicit ConvertFile(QObject *parent = nullptr);

    // 将 PDF 光栅化后保存为 BMP，完成后发出 finished()
    void pdfToBmp(const QString &pdfPath, const QString &outputDir);

signals:
    void progress(int page);
    void finished();

private:
    QString saveDir;
    QString baseName;
};

#endif // CONVERTFILE_H
