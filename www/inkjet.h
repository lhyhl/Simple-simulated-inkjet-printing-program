#ifndef INKJET_H
#define INKJET_H

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QColor>
#include <QRgb>
#include <QTimer>
#include <QLabel>
#include <QPointer>
#include <QVBoxLayout>
#include <QDialog>
#include <QScrollArea>

enum class PrintMode
{
    BlackWhite,
    CMYK
};

class Inkjet : public QObject
{
    Q_OBJECT

public:
    explicit Inkjet(QObject *parent = nullptr);
    ~Inkjet();

    static QImage convertToBlackWhite(const QImage &source);
    static QImage convertToCMYK(const QImage &source);
    // 从文件路径加载图像后打印（兼容原有调用方）
    void printImageWithEffect(const QString &imagePath, PrintMode mode, QLabel *displayLabel);

    // 直接接受已光栅化的 QImage，逐行送入打印模拟（无中间文件）
    void printImage(const QImage &sourceImage, PrintMode mode, QLabel *displayLabel);

    void stopPrint();

signals:
    void printFinished(const QPixmap &result);
    void printError(const QString &error);
    void printProgress(int percent);

public slots:

private:
    QTimer *printTimer;
    QImage currentImage;
    QImage displayImage;
    QPointer<QLabel> targetLabel;
    int currentLine;
    bool isPrinting;

private slots:
    void onPrintTimer();
};

#endif // INKJET_H
