#include "convertfile.h"
#include "pdfrenderer.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QFutureWatcher>
#include <QtConcurrent>

ConvertFile::ConvertFile(QObject *parent)
    : QObject(parent)
{
}

void ConvertFile::pdfToBmp(const QString &pdfPath, const QString &outputDir)
{
    saveDir = outputDir;
    QDir().mkpath(saveDir);
    baseName = QFileInfo(pdfPath).completeBaseName();

    qDebug() << "[Convert] 开始光栅化 PDF:" << pdfPath;

    // 在线程池中运行 PdfRenderer，避免阻塞主线程
    auto *watcher = new QFutureWatcher<QList<QImage>>(this);

    connect(watcher, &QFutureWatcher<QList<QImage>>::finished,
            this, [this, watcher]()
    {
        QList<QImage> images = watcher->result();
        watcher->deleteLater();

        qDebug() << "[Convert] 光栅化完成，共" << images.size() << "页";

        if (images.isEmpty())
        {
            qDebug() << "[Convert] 错误：PdfRenderer 未返回任何页面，BMP 未保存";
            emit finished();
            return;
        }

        for (int i = 0; i < images.size(); ++i)
        {
            // 第 0 页用原始文件名，后续页追加序号
            QString fname = (i == 0)
                ? QString("%1/%2.bmp").arg(saveDir, baseName)
                : QString("%1/%2_%3.bmp").arg(saveDir, baseName).arg(i);

            bool saved = images[i].save(fname, "BMP");
            qDebug() << "[Convert] 保存第" << (i + 1) << "页:"
                     << fname << "结果:" << saved;

            emit progress(i);
        }

        emit finished();
    });

    // 捕获路径副本传入线程
    const QString path = pdfPath;
    watcher->setFuture(QtConcurrent::run([path]()
    {
        return PdfRenderer::renderInDedicatedThread(path, 150.0f);
    }));
}
