// Windows 宏
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

// C++/WinRT 投影头文件（来自 Windows 10 SDK）
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

// 链接 Windows Runtime 伞形库
//MSVC 自动在 SDK lib 目录搜索
#pragma comment(lib, "windowsapp")

// Qt 头文件放在 Windows 头文件之后，避免 min/max 宏冲突
#include "pdfrenderer.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <thread>


using namespace winrt;
using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

namespace {


/*
 * @brief 核心渲染实现函数
 *
 * 该函数执行实际的 PDF 加载和页面光栅化操作。
 *
 * @param pdfPath PDF 文件的路径
 * @param dpi 渲染目标 DPI（每英寸点数），用于控制图像清晰度
 * @return QList<QImage> 渲染成功的图像列表
 */
static QList<QImage> renderImpl(const QString &pdfPath, float dpi)
{
    QList<QImage> pages;

    // 转换路径为本地格式
    const QString nativePath = QDir::toNativeSeparators(
        QFileInfo(pdfPath).absoluteFilePath());
    qDebug() << "[PdfRenderer] 开始渲染，路径:" << nativePath;

    try
    {
        // 1. 加载文件
        // hstring() 将 std::wstring 转换为 WinRT 字符串
        // .get() 表示同步等待异步操作完成（阻塞调用）
        auto file = StorageFile::GetFileFromPathAsync(
                        hstring(nativePath.toStdWString())).get();

        // 2. 加载 PDF 文档
        auto doc  = PdfDocument::LoadFromFileAsync(file).get();
        uint32_t pageCount = doc.PageCount();
        qDebug() << "[PdfRenderer] PDF 共" << pageCount << "页";

        // 3. 遍历每一页进行渲染
        for (uint32_t i = 0; i < pageCount; ++i)
        {
            auto page     = doc.GetPage(i);// 获取页面对象

            auto pageSize = page.Size();// 获取页面原始尺寸

            // 4. 计算缩放比例
            // Windows 默认 DPI 为 96，根据传入的 DPI 计算缩放因子
            float scale  = dpi / 96.0f;

            // 计算目标宽度和高度，确保至少为 1 像素
            uint32_t w   = qMax(1u, static_cast<uint32_t>(pageSize.Width  * scale));
            uint32_t h   = qMax(1u, static_cast<uint32_t>(pageSize.Height * scale));
            
            // 限制最大尺寸，确保图像能够完整显示
            const uint32_t maxWidth = 2000;
            const uint32_t maxHeight = 2800;
            
            if (w > maxWidth || h > maxHeight) {
                // 计算缩放因子，保持宽高比
                float scaleFactor = qMin(static_cast<float>(maxWidth) / w, 
                                         static_cast<float>(maxHeight) / h);
                w = static_cast<uint32_t>(w * scaleFactor);
                h = static_cast<uint32_t>(h * scaleFactor);
                qDebug() << "[PdfRenderer] 限制尺寸：" << w << "×" << h;
            }
            
            qDebug() << "[PdfRenderer] 使用原始页面大小渲染：" << w << "×" << h;

            // 5. 创建渲染目标流
            InMemoryRandomAccessStream stream;
            PdfPageRenderOptions opts;
            opts.DestinationWidth(w);// 设置目标宽度
            opts.DestinationHeight(h);// 设置目标高度

            // 执行渲染：将页面绘制到内存流中（异步）
            page.RenderToStreamAsync(stream, opts).get();


            // 6. 读取内存流数据
            stream.Seek(0);// 重置流指针到开头
            DataReader reader(stream.GetInputStreamAt(0));// 创建读取器
            uint32_t byteCount = static_cast<uint32_t>(stream.Size());
            reader.LoadAsync(byteCount).get();// 加载数据到读取器


            // 7. 复制数据到 std::vector
            std::vector<uint8_t> buf(byteCount);
            reader.ReadBytes(winrt::array_view<uint8_t>(
                                 buf.data(), buf.data() + byteCount));

            // 8. 转换为 QImage
            QImage img;
            img.loadFromData(buf.data(), static_cast<int>(byteCount));

            if (!img.isNull())
            {
                qDebug() << "[PdfRenderer] 第" << (i + 1) << "页渲染完成："
                         << img.width() << "×" << img.height();
                pages.append(img);
            }
            else
            {
                qDebug() << "[PdfRenderer] 第" << (i + 1) << "页解码失败";
            }
        }
    }
    // 异常处理：捕获 WinRT 错误
    catch (hresult_error const &e)
    {
        qDebug() << "[PdfRenderer] WinRT 错误 HRESULT=0x"
                 + QString::number(static_cast<uint32_t>(e.code()), 16)
                 + " 信息:" + QString::fromWCharArray(e.message().c_str());
    }
    catch (std::exception const &e)
    {
        qDebug() << "[PdfRenderer] std 异常:" << e.what();
    }
    catch (...)
    {
        qDebug() << "[PdfRenderer] 未知异常";
    }

    return pages;
}

} // namespace


/*
 * @brief 同步渲染（通常不推荐在主线程直接调用）
 *
 * 初始化 WinRT 环境，调用渲染逻辑，然后反初始化。
 * 注意：如果在主线程调用，可能会阻塞 UI。
 */
QList<QImage> PdfRenderer::render(const QString &pdfPath, float dpi)
{
    // 初始化 WinRT 线程公寓（多线程模式）
    // 这是使用 WinRT API 的必要步骤
    try
    {
        init_apartment(apartment_type::multi_threaded);
    }
    catch (hresult_error const &e)
    {
        // 0x80010106 (RPC_E_CHANGED_MODE) 表示公寓模式已由其他线程设置
        if (static_cast<uint32_t>(e.code()) != 0x80010106u)
            qDebug() << "[PdfRenderer] init_apartment 警告:" << e.code();
    }

    // 执行渲染
    QList<QImage> pages = renderImpl(pdfPath, dpi);

    // 反初始化（通常在程序退出时调用，此处调用可能影响其他正在使用的 WinRT 组件）
    try
    {
        uninit_apartment();
    }
    catch (hresult_error const &e)
    {
        qDebug() << "[PdfRenderer] uninit_apartment 警告:" << e.code();
    }
    catch (...)
    {
    }

    return pages;
}


/*
 * @brief 在独立线程中执行渲染
 *
 * 这是 PrintWorkflow 中实际调用的函数。
 * 通过创建新线程来执行渲染，防止阻塞 Qt 的主线程（UI线程）。
 *
 * @param pdfPath 文件路径
 * @param dpi 渲染精度
 * @return QList<QImage> 渲染结果
 */
QList<QImage> PdfRenderer::renderInDedicatedThread(const QString &pdfPath, float dpi)
{
    QList<QImage> pages;

    // 创建工作线程
    // 使用引用捕获 [&pages, pdfPath, dpi] 将参数传入线程
    std::thread worker([&pages, pdfPath, dpi]()
    {

        // 1. 线程内初始化 WinRT
        // 新线程必须独立初始化自己的公寓状态
        try
        {
            init_apartment(apartment_type::multi_threaded);
        }
        catch (hresult_error const &e)
        {
            if (static_cast<uint32_t>(e.code()) != 0x80010106u)
                qDebug() << "[PdfRenderer] [worker] init_apartment 警告:" << e.code();
        }

         // 2. 执行渲染
        pages = renderImpl(pdfPath, dpi);

        // 3. 反初始化（线程结束前）
        try
        {
            uninit_apartment();
        }
        catch (hresult_error const &e)
        {
            qDebug() << "[PdfRenderer] [worker] uninit_apartment 警告:" << e.code();
        }
        catch (...)
        {
        }
    });
    // 立即加入线程（同步等待）
    worker.join();
    return pages;
}
