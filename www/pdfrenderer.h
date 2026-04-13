#ifndef PDFRENDERER_H
#define PDFRENDERER_H

#include <QImage>
#include <QList>
#include <QString>

// 使用 Windows 10 内置的 Windows.Data.Pdf WinRT API
// 将 PDF 每一页直接光栅化为 QImage，无需截图
//
// 重要：render() 必须在「非 STA」线程中调用。
// 从 GUI 线程请使用 renderInDedicatedThread()（内部新建 std::thread，避免 Debug 断言）。
class PdfRenderer
{
public:
    static QList<QImage> render(const QString &pdfPath, float dpi = 150.0f);

    // 在独立 OS 线程中调用 render()，可从 Qt 主线程同步等待（join），不触发 WinRT STA 断言
    static QList<QImage> renderInDedicatedThread(const QString &pdfPath, float dpi = 150.0f);
};

#endif // PDFRENDERER_H
