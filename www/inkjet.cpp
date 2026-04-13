#include "inkjet.h"
#include <QCoreApplication>
#include <QFileInfo>

// 构造函数：初始化定时器、打印状态变量，并连接定时器超时信号
Inkjet::Inkjet(QObject *parent)
    : QObject(parent), printTimer(new QTimer(this)), targetLabel(nullptr), currentLine(0), isPrinting(false)
{
    connect(printTimer, &QTimer::timeout, this, &Inkjet::onPrintTimer);
}

// 析构函数：确保打印任务被停止，避免资源泄漏
Inkjet::~Inkjet()
{
    stopPrint();
}

// 将彩色图像转换为灰度图像（黑白模式）
// 输入：源图像（任意格式）
// 输出：灰度图像（Format_ARGB32 格式，RGB 通道值相同）
QImage Inkjet::convertToBlackWhite(const QImage &source)
{
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    int width = result.width();
    int height = result.height();

    // 逐像素处理：计算灰度值并赋给 RGB 三个通道
    for (int y = 0; y < height; ++y)
    {
        uchar *line = result.scanLine(y);
        for (int x = 0; x < width; ++x)
        {
            int offset = x * 4; // 每个像素占 4 字节（BGRA）
            // qGray 自动计算亮度（默认权重：0.299R + 0.587G + 0.114B）
            int gray = qGray(line[offset + 2], line[offset + 1], line[offset]);
            line[offset] = gray;     // B
            line[offset + 1] = gray; // G
            line[offset + 2] = gray; // R
            // Alpha 通道保持不变（通常为 255）
        }
    }

    return result;
}

// 将彩色图像模拟为 CMYK 印刷效果
// 该函数模拟油墨在纸张上的叠加效果，并加入点增益（dot gain）以增强真实感
QImage Inkjet::convertToCMYK(const QImage &source)
{
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    int width  = result.width();
    int height = result.height();

    for (int y = 0; y < height; ++y)
    {
        uchar *line = result.scanLine(y);
        for (int x = 0; x < width; ++x)
        {
            int offset = x * 4;
            // Format_ARGB32 在小端系统中内存布局为 [B, G, R, A]
            float r = line[offset + 2] / 255.0f;
            float g = line[offset + 1] / 255.0f;
            float b = line[offset + 0] / 255.0f;

            // 1.计算 K（黑版）= 1 - max(R, G, B)
            float k = 1.0f - qMax(r, qMax(g, b));

            float c, m, yy; // C: 青, M: 品红, Y: 黄
            if (k >= 1.0f)
            {
                // 纯黑色区域：仅使用黑墨，CMY 均为 0
                c = m = yy = 0.0f;
            }
            else
            {
                float denom = 1.0f - k;
                c  = (1.0f - r - k) / denom;
                m  = (1.0f - g - k) / denom;
                yy = (1.0f - b - k) / denom;
            }

            // 2.将 CMYK 值还原为 RGB，模拟油墨印在白纸上的视觉效果
            // 白底反射光 = (1-C)*(1-K) 等
            float r_out = (1.0f - c) * (1.0f - k);
            float g_out = (1.0f - m) * (1.0f - k);
            float b_out = (1.0f - yy) * (1.0f - k);

            // 模拟点增益（dot gain）：油墨在纸张上扩散，导致中间调变暗
            // 此处使用简化模型：v' = v - 0.32 * v * (1 - v)
            auto applyDotGain = [](float v) -> float {
                return v - 0.08f * v * (1.0f - v) * 4.0f;
            };
            r_out = qBound(0.0f, applyDotGain(r_out), 1.0f);
            g_out = qBound(0.0f, applyDotGain(g_out), 1.0f);
            b_out = qBound(0.0f, applyDotGain(b_out), 1.0f);

            // 写回像素数据
            line[offset + 2] = static_cast<uchar>(r_out * 255); // R
            line[offset + 1] = static_cast<uchar>(g_out * 255); // G
            line[offset + 0] = static_cast<uchar>(b_out * 255); // B
        }
    }

    return result;
}

// 从文件路径加载图像并启动打印（带打印模式选择）
void Inkjet::printImageWithEffect(const QString &imagePath, PrintMode mode, QLabel *displayLabel)
{
    qDebug() << "[Inkjet] 进入 printImageWithEffect, 路径:" << imagePath;

    // 检查文件是否存在
    QFileInfo fileInfo(imagePath);
    qDebug() << "[Inkjet] 文件存在:" << fileInfo.exists() << "绝对路径:" << fileInfo.absoluteFilePath();

    // 如果正在打印，先停止之前的任务
    if (isPrinting)
    {
        qDebug() << "[Inkjet] 正在打印，先停止";
        stopPrint();
    }

    // 尝试加载图像
    QPixmap pixmap(imagePath);
    qDebug() << "[Inkjet] 图片加载完成，isNull:" << pixmap.isNull() << "尺寸:" << pixmap.size();

    if (pixmap.isNull())
    {
        // 若失败，尝试使用绝对路径重新加载
        QPixmap pixmapAbs(fileInfo.absoluteFilePath());
        qDebug() << "[Inkjet] 尝试绝对路径加载，isNull:" << pixmapAbs.isNull() << "尺寸:" << pixmapAbs.size();

        if (!pixmapAbs.isNull())
        {
            pixmap = pixmapAbs;
        }
        else
        {
            emit printError(QString("无法加载图片：%1\n文件存在：%2").arg(imagePath).arg(fileInfo.exists()));
            return;
        }
    }

    qDebug() << "[Inkjet] 图片加载成功，尺寸:" << pixmap.size() << "路径:" << imagePath;

    // 限制图像尺寸，确保能够完整显示
    const int maxWidth = 1200;
    const int maxHeight = 1600;
    
    if (pixmap.width() > maxWidth || pixmap.height() > maxHeight) {
        pixmap = pixmap.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        qDebug() << "[Inkjet] 限制图像尺寸：" << pixmap.size();
    }

    QImage image = pixmap.toImage();

    // 根据打印模式进行颜色转换
    if (mode == PrintMode::BlackWhite)
        currentImage = convertToBlackWhite(image);
    else
        currentImage = convertToCMYK(image);

    // 初始化打印状态
    targetLabel = displayLabel;
    displayImage = QImage(currentImage.size(), QImage::Format_ARGB32);
    displayImage.fill(Qt::white); // 初始为白底

    currentLine = 0;
    isPrinting = true;

    qDebug() << "[Inkjet] 开始打印，图片尺寸:" << currentImage.width() << "x" << currentImage.height() << "定时器间隔:2ms";

    printTimer->start(2); // 每 2ms 触发一次，模拟逐行打印
}

// 直接从 QImage 对象启动打印（适用于已加载或处理过的图像）
void Inkjet::printImage(const QImage &sourceImage, PrintMode mode, QLabel *displayLabel)
{
    // 如果正在打印，先停止
    if (isPrinting)
        stopPrint();

    // 检查输入图像是否有效
    if (sourceImage.isNull())
    {
        emit printError("传入图像为空");
        return;
    }

    // 根据模式进行颜色处理
    if (mode == PrintMode::BlackWhite)
        currentImage = convertToBlackWhite(sourceImage);
    else
        currentImage = convertToCMYK(sourceImage);

    // 初始化打印状态
    targetLabel  = displayLabel;
    displayImage = QImage(currentImage.size(), QImage::Format_ARGB32);
    displayImage.fill(Qt::white);

    currentLine = 0;
    isPrinting  = true;

    qDebug() << "[Inkjet] 直接打印图像，尺寸:"
             << currentImage.width() << "×" << currentImage.height();

    printTimer->start(2);
}

// 停止当前打印任务
void Inkjet::stopPrint()
{
    printTimer->stop();
    isPrinting = false;
    targetLabel.clear(); // 清除对 QLabel 的引用，避免悬空指针
}

// 定时器槽函数：每次触发绘制若干行图像，模拟打印过程
void Inkjet::onPrintTimer()
{
    // 检查打印状态和目标控件是否有效
    if (!isPrinting || targetLabel.isNull())
    {
        if (isPrinting && targetLabel.isNull())
            stopPrint();
        return;
    }

    int linesPerTick = 20; // 每次定时器触发绘制 20 行，提高流畅度
    int height = currentImage.height();
    int width = currentImage.width();

    qDebug() << "定时器触发，当前行:" << currentLine << "/" << height;

    // 逐行复制像素数据到显示图像
    // for (int i = 0; i < linesPerTick && currentLine < height; ++i)
    // {
    //     uchar *srcLine = currentImage.scanLine(currentLine);
    //     uchar *dstLine = displayImage.scanLine(currentLine);
    //     memcpy(dstLine, srcLine, width * 4); // 复制一行（4 字节/像素）
    //     currentLine++;
    // }

    // 逐行复制像素数据到显示图像
    int linesToDraw = qMin(linesPerTick, height - currentLine);
    for (int i = 0; i < linesToDraw; ++i)
    {
        // 获取当前行的指针
        uchar *srcLine = currentImage.scanLine(currentLine);
        uchar *dstLine = displayImage.scanLine(currentLine);

        // 复制一行数据
        memcpy(dstLine, srcLine, width * 4);

        // 行数递增
        currentLine++;
    }

    // 计算并发送进度百分比
    int percent = (currentLine * 100) / height;
    emit printProgress(percent);

    // 再次检查目标控件是否仍有效（防止在打印过程中被销毁）
    if (targetLabel.isNull())
    {
        stopPrint();
        return;
    }

    // 更新界面显示
    targetLabel->setPixmap(QPixmap::fromImage(displayImage));
    targetLabel->repaint(); // 强制重绘，确保及时显示

    qDebug() << "已更新 label, 当前行:" << currentLine;

    // 打印完成
    if (currentLine >= height)
    {
        stopPrint();
        emit printFinished(QPixmap::fromImage(displayImage)); // 发送完成信号
    }
}