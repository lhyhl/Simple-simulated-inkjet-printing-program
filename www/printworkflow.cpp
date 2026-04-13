#include "printworkflow.h"

#include <QImage>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
// Windows平台专用：包含PDF渲染头文件
#ifdef Q_OS_WIN
#include "pdfrenderer.h"
#endif

namespace {
/*
 * @brief 构建可打印的BMP文件列表
 *
 * 该函数遍历用户选择的文件，将PDF转换为临时BMP文件，并收集所有BMP的路径。
 *
 * @param selectedFiles 用户选择的文件路径列表
 * @param tempDir 临时目录指针（用于存放转换后的PDF页面）
 * @param outBmpPaths 输出参数：转换后的BMP文件绝对路径列表
 * @param outRecordNames 输出参数：用于记录打印历史的文件名（包含页码信息）
 * @param outRecordFormats 输出参数：原始文件格式（BMP或PDF）
 * @param errorMessage 错误信息输出
 * @return bool 是否构建成功
 */
bool buildPrintableBmpList(const QStringList &selectedFiles,
                           QTemporaryDir *tempDir,
                           QStringList *outBmpPaths,
                           QStringList *outRecordNames,
                           QStringList *outRecordFormats,
                           QString *errorMessage)
{
    outBmpPaths->clear();
    outRecordNames->clear();
    outRecordFormats->clear();

    int pdfJobIndex = 0;// 用于生成临时文件名的计数器

    for (const QString &path : selectedFiles)
    {
        QFileInfo fi(path);
        if (!fi.exists())// 跳过不存在的文件
            continue;

        const QString ext = fi.suffix().toLower();
        // 1. 处理 BMP 文件
        if (ext == "bmp")
        {
            outBmpPaths->append(QDir::toNativeSeparators(fi.absoluteFilePath()));
            outRecordNames->append(fi.fileName());
            outRecordFormats->append(QStringLiteral("BMP"));
        }
         // 2. 处理 PDF 文件
        else if (ext == "pdf")
        {
#ifdef Q_OS_WIN
            // 检查临时目录是否有效
            if (!tempDir || !tempDir->isValid())
            {
                *errorMessage = QObject::tr("内部错误：临时目录无效，无法处理 PDF。");
                return false;
            }

            const QString pdfPath = fi.absoluteFilePath();
            // 在独立线程中渲染PDF（防止UI卡顿），分辨率为150dpi
            QList<QImage> pages = PdfRenderer::renderInDedicatedThread(pdfPath, 150.0f);
            if (pages.isEmpty())
            {
                *errorMessage = QObject::tr("无法渲染 PDF：%1").arg(fi.fileName());
                return false;
            }

            const QString pdfName = fi.fileName();
            // 遍历PDF的每一页
            for (int i = 0; i < pages.size(); ++i)
            {
                 // 生成临时BMP文件名，格式如 www_print_0_0.bmp
                const QString bmpName = QStringLiteral("www_print_%1_%2.bmp")
                                            .arg(pdfJobIndex)
                                            .arg(i);
                const QString bmpPath = tempDir->filePath(bmpName);

                // 保存 QImage 为 BMP 格式
                if (!pages[i].save(bmpPath, "BMP"))
                {
                    *errorMessage = QObject::tr("保存临时 BMP 失败。");
                    return false;
                }
                outBmpPaths->append(QDir::toNativeSeparators(bmpPath));

                // 记录名称处理：如果是单页PDF直接显示文件名，多页则显示“文件名（第X页）”
                if (pages.size() == 1)
                    outRecordNames->append(pdfName);
                else
                    outRecordNames->append(QStringLiteral("%1（第 %2 页）").arg(pdfName).arg(i + 1));
                outRecordFormats->append(QStringLiteral("PDF"));
            }
            ++pdfJobIndex;

            //非Windows平台不支持PDF渲染
#else

            Q_UNUSED(tempDir);
            Q_UNUSED(pdfJobIndex);
            *errorMessage = QObject::tr("当前构建不支持 PDF，请将文件转为 BMP 后再打印。");
            return false;
#endif
        }
    }

    return true;
}

} // namespace

namespace PrintWorkflow {



/*
 * @brief 启动交互式打印流程（用户先选模式，再选文件）
 */
void startInteractivePrint(QWidget *parent, Inkjet *inkjet, PrintRecord *record)
{
    if (!inkjet || !record)
    {
        QMessageBox::warning(parent, "打印", "打印模块未正确初始化，请重启应用。");
        return;
    }

    // 1. 弹窗让用户选择黑白还是彩色打印
    QMessageBox modeBox(parent);
    modeBox.setWindowTitle("打印选项");
    modeBox.setText("请选择打印方式：");
    QPushButton *bwBtn = modeBox.addButton("黑白打印", QMessageBox::AcceptRole);
    QPushButton *cmykBtn = modeBox.addButton("彩色打印 (CMYK)", QMessageBox::AcceptRole);
    QPushButton *cancelBtn = modeBox.addButton("取消", QMessageBox::RejectRole);
    modeBox.setDefaultButton(bwBtn);
    modeBox.exec();

    if (modeBox.clickedButton() == cancelBtn)
        return;

    PrintMode printMode;
    QString printModeText;
    if (modeBox.clickedButton() == bwBtn)
    {
        printMode = PrintMode::BlackWhite;
        printModeText = QStringLiteral("黑白打印");
    }
    else
    {
        printMode = PrintMode::CMYK;
        printModeText = QStringLiteral("彩色打印 (CMYK)");
    }

    // 2. 打开文件选择对话框
    QStringList files = QFileDialog::getOpenFileNames(
        parent,
        "选择要打印的文件",
        QDir::currentPath(),
        "PDF 或 BMP (*.pdf *.bmp);;所有文件 (*.*)");

    if (files.isEmpty())
        return;

     // 过滤非BMP/PDF文件
    QStringList filtered;
    for (const QString &p : files)
    {
        const QString e = QFileInfo(p).suffix().toLower();
        if (e == "pdf" || e == "bmp")
            filtered.append(p);
    }

    if (filtered.isEmpty())
    {
        QMessageBox::information(parent, "打印", "请选择 PDF 或 BMP 文件。");
        return;
    }

     // 3. 检查是否需要创建临时目录（只要有PDF就需要）
    bool needTemp = false;
    for (const QString &p : filtered)
    {
        if (QFileInfo(p).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0)
        {
            needTemp = true;
            break;
        }
    }

    std::unique_ptr<QTemporaryDir> tempDir;
    if (needTemp)
    {
        tempDir = std::make_unique<QTemporaryDir>();
        if (!tempDir->isValid())
        {
            QMessageBox::warning(parent, "打印", "无法创建临时目录，无法处理 PDF。");
            return;
        }
    }

    // 4. 构建可打印列表并启动预览
    QStringList bmpPaths;
    QStringList recordNames;
    QStringList recordFormats;
    QString err;
    if (!buildPrintableBmpList(filtered, tempDir.get(), &bmpPaths, &recordNames, &recordFormats, &err))
    {
        QMessageBox::warning(parent, "打印", err);
        return;
    }

    if (bmpPaths.isEmpty())
    {
        QMessageBox::warning(parent, "打印", "没有可打印的页面。");
        return;
    }

    // 创建预览对话框
    auto *previewDialog = new PrintPreviewDialog(
        bmpPaths,
        recordNames,
        recordFormats,
        parent,
        inkjet,
        record,
        printMode,
        printModeText,
        std::move(tempDir));

    previewDialog->setAttribute(Qt::WA_DeleteOnClose);
    previewDialog->setModal(true);
    previewDialog->show();
}


/*
 * @brief 直接从路径启动预览（通常用于批量打印或外部调用）
 *
 * 与上面的函数逻辑基本一致，但打印模式由外部参数直接指定，无需弹窗选择。
 */
void startPreviewFromPaths(QWidget *parent,
                           Inkjet *inkjet,
                           PrintRecord *record,
                           const QStringList &sourcePaths,
                           PrintMode mode,
                           const QString &printModeText)
{
    if (!inkjet || !record)
    {
        QMessageBox::warning(parent, "打印", "打印模块未正确初始化，请重启应用。");
        return;
    }

    if (sourcePaths.isEmpty())
    {
        QMessageBox::warning(parent, "打印", "没有可打印的文件。");
        return;
    }

    bool needTemp = false;
    for (const QString &p : sourcePaths)
    {
        if (QFileInfo(p).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0)
        {
            needTemp = true;
            break;
        }
    }

    std::unique_ptr<QTemporaryDir> tempDir;
    if (needTemp)
    {
        tempDir = std::make_unique<QTemporaryDir>();
        if (!tempDir->isValid())
        {
            QMessageBox::warning(parent, "打印", "无法创建临时目录，无法处理 PDF。");
            return;
        }
    }

    QStringList bmpPaths;
    QStringList recordNames;
    QStringList recordFormats;
    QString err;
    if (!buildPrintableBmpList(sourcePaths, tempDir.get(), &bmpPaths, &recordNames, &recordFormats, &err))
    {
        QMessageBox::warning(parent, "打印", err);
        return;
    }

    if (bmpPaths.isEmpty())
    {
        QMessageBox::warning(parent, "打印", "没有可打印的页面。");
        return;
    }

    auto *previewDialog = new PrintPreviewDialog(
        bmpPaths,
        recordNames,
        recordFormats,
        parent,
        inkjet,
        record,
        mode,
        printModeText,
        std::move(tempDir));

    previewDialog->setAttribute(Qt::WA_DeleteOnClose);
    previewDialog->setModal(true);
    previewDialog->show();
}

} // namespace PrintWorkflow


/*
 * @brief PrintPreviewDialog 构造函数
 *
 * 初始化预览界面的UI布局，包括滚动区域、图片标签、页码显示和控制按钮。
 */
PrintPreviewDialog::PrintPreviewDialog(const QStringList &bmpPaths,
                                       const QStringList &recordFileNames,
                                       const QStringList &recordOriginalFormats,
                                       QWidget *parent,
                                       Inkjet *inkjet,
                                       PrintRecord *printRecord,
                                       PrintMode printMode,
                                       const QString &printModeText,
                                       std::unique_ptr<QTemporaryDir> tempDir)
    : QDialog(parent),
      m_imagePaths(bmpPaths),
      m_recordFileNames(recordFileNames),
      m_recordOriginalFormats(recordOriginalFormats),
      m_currentIndex(0),
      m_inkjet(inkjet),
      m_printRecord(printRecord),
      m_printMode(printMode),
      m_printModeText(printModeText),
      m_tempDir(std::move(tempDir)),
      m_imagesSaved(false)
{
    Q_ASSERT(m_imagePaths.size() == m_recordFileNames.size());
    Q_ASSERT(m_imagePaths.size() == m_recordOriginalFormats.size());

    setWindowTitle("打印预览");
    resize(900, 700);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 图片显示区域（带滚动条）
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setAlignment(Qt::AlignCenter);
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(400, 300);
    scrollArea->setWidget(m_imageLabel);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);

    // 页码标签
    m_pageLabel = new QLabel(this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_pageLabel);

    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_previousBtn = new QPushButton("上一张", this);
    m_nextBtn = new QPushButton("下一张", this);
    m_printBtn = new QPushButton("打印", this);
    m_closeBtn = new QPushButton("关闭", this);

    buttonLayout->addWidget(m_previousBtn);
    buttonLayout->addWidget(m_nextBtn);
    buttonLayout->addWidget(m_printBtn);
    buttonLayout->addWidget(m_closeBtn);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // 连接按钮信号
    connect(m_previousBtn, &QPushButton::clicked, this, &PrintPreviewDialog::onPreviousClicked);
    connect(m_nextBtn, &QPushButton::clicked, this, &PrintPreviewDialog::onNextClicked);
    connect(m_printBtn, &QPushButton::clicked, this, &PrintPreviewDialog::onPrintClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &PrintPreviewDialog::onCloseClicked);

    updateDisplay();// 初始化显示第一张图片
    QTimer::singleShot(0, this, &PrintPreviewDialog::updateDisplay);
}

PrintPreviewDialog::~PrintPreviewDialog() = default;


/*
 * @brief 更新图片显示
 *
 * 根据当前索引加载图片并进行缩放以适应窗口。
 */
void PrintPreviewDialog::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_imagePaths.size())
    {
        m_currentIndex = index;
        updateDisplay();
    }
}

void PrintPreviewDialog::updateDisplay()
{
    if (m_imagePaths.isEmpty())
    {
        m_imageLabel->clear();
        m_imageLabel->setText("没有可显示的图片");// 清除文本
        m_pageLabel->setText("");
        return;
    }

    const QString currentPath = m_imagePaths.at(m_currentIndex);
    QPixmap pixmap(currentPath);

    if (pixmap.isNull())
    {
        m_imageLabel->clear();
        m_imageLabel->setText(QString("无法加载图片: %1").arg(currentPath));
    }
    else
    {
        m_imageLabel->setText(QString());
        const QSize sz = m_imageLabel->size();

        // 如果Label有有效大小，则缩放图片保持宽高比
        if (sz.width() > 10 && sz.height() > 10)
            m_imageLabel->setPixmap(pixmap.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            m_imageLabel->setPixmap(pixmap);
    }

    // 更新页码文本
    m_pageLabel->setText(QString("第 %1 / %2 张")
                             .arg(m_currentIndex + 1)
                             .arg(m_imagePaths.size()));

    // 根据位置启用/禁用翻页按钮
    m_previousBtn->setEnabled(m_currentIndex > 0);
    m_nextBtn->setEnabled(m_currentIndex < m_imagePaths.size() - 1);
}


//上一页
void PrintPreviewDialog::onPreviousClicked()
{
    if (m_currentIndex > 0)
    {
        m_currentIndex--;
        updateDisplay();
    }
}


//下一页
void PrintPreviewDialog::onNextClicked()
{
    if (m_currentIndex < m_imagePaths.size() - 1)
    {
        m_currentIndex++;
        updateDisplay();
    }
}

/*
 * @brief 打印按钮点击槽函数
 *
 * 核心打印逻辑：创建一个结果显示窗口，调用Inkjet类进行模拟打印，
 * 并在过程中更新进度条和处理结果。
 */
void PrintPreviewDialog::onPrintClicked()
{
    const QString currentPath = m_imagePaths.at(m_currentIndex);
    const QString fileName = m_recordFileNames.at(m_currentIndex);
    const QString originalFormat = m_recordOriginalFormats.at(m_currentIndex);

    // 将所有BMP图片添加到相册（仅保存一次）
    if (!m_imagesSaved) {
        QString albumDir = QDir::currentPath() + "/../../images";
        QDir dir(albumDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        // 遍历所有图片路径，保存到相册
        for (const QString &imagePath : m_imagePaths) {
            // 生成保存路径
            QString savePath = dir.filePath(QFileInfo(imagePath).fileName());
            
            // 处理重名文件
            int counter = 1;
            while (QFile::exists(savePath)) {
                QString baseName = QFileInfo(imagePath).completeBaseName();
                QString suffix = QFileInfo(imagePath).suffix();
                savePath = dir.filePath(baseName + "_" + QString::number(counter) + "." + suffix);
                counter++;
            }
            
            // 复制文件到相册
            if (QFile::copy(imagePath, savePath)) {
                qDebug() << "[PrintWorkflow] 图片已保存到相册：" << savePath;
            } else {
                qDebug() << "[PrintWorkflow] 保存图片到相册失败：" << imagePath;
            }
        }
        
        // 标记已保存，防止重复保存
        m_imagesSaved = true;
    }
    
    // 1. 创建打印结果对话框
    QDialog *resultDialog = new QDialog(this);
    resultDialog->setWindowTitle(QString("打印效果 - %1").arg(fileName));
    resultDialog->resize(1200, 900); // 增大对话框尺寸

    QVBoxLayout *resultLayout = new QVBoxLayout(resultDialog);
    resultLayout->setContentsMargins(10, 10, 10, 10);

    // 结果显示区域（带白底和边框，模拟纸张）
    QScrollArea *resultScroll = new QScrollArea(resultDialog);
    resultScroll->setAlignment(Qt::AlignCenter);
    resultScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    resultScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    resultScroll->setWidgetResizable(false); // 禁用自动调整大小，确保滚动条能够显示
    resultScroll->setMinimumSize(1180, 800); // 设置滚动区域的最小尺寸
    
    QLabel *resultLabel = new QLabel(resultDialog);
    resultLabel->setAlignment(Qt::AlignCenter);
    resultLabel->setMinimumSize(1000, 1400); // 增大标签的最小高度，确保滚动条能够显示
    resultLabel->setStyleSheet("background-color: white; border: 1px solid gray;");
    resultScroll->setWidget(resultLabel);
    resultLayout->addWidget(resultScroll);

    // 进度条
    QLabel *progressLabel = new QLabel("打印进度：0%", resultDialog);
    progressLabel->setAlignment(Qt::AlignCenter);
    resultLayout->addWidget(progressLabel);

    // 图片切换按钮布局
    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->addStretch();
    
    QPushButton *prevBtn = new QPushButton("上一张", resultDialog);
    QPushButton *nextBtn = new QPushButton("下一张", resultDialog);
    
    // 禁用导航按钮，打印完成后启用
    prevBtn->setEnabled(false);
    nextBtn->setEnabled(false);
    
    navLayout->addWidget(prevBtn);
    navLayout->addWidget(nextBtn);
    navLayout->addStretch();
    resultLayout->addLayout(navLayout);

    // 关闭按钮（初始禁用，打印完成后启用）
    QPushButton *closeResultBtn = new QPushButton("关闭", resultDialog);
    closeResultBtn->setEnabled(false);
    resultLayout->addWidget(closeResultBtn);

    // 保存当前图片索引
    int *currentImageIndex = new int(m_currentIndex);
    
    resultDialog->setAttribute(Qt::WA_DeleteOnClose);
    resultDialog->show();

    // 2. 连接生命周期信号：对话框关闭或销毁时停止打印
    connect(resultDialog, &QDialog::finished, m_inkjet,
            [inkjet = m_inkjet](int) { inkjet->stopPrint(); });
    connect(resultDialog, &QObject::destroyed, m_inkjet,
            [inkjet = m_inkjet]() { inkjet->stopPrint(); });
    
    // 3. 连接对话框销毁信号，释放内存
    connect(resultDialog, &QObject::destroyed, [currentImageIndex]() {
        delete currentImageIndex;
    });

    // 3. 启动打印（Inkjet类会通过定时器模拟逐行打印效果）
    m_inkjet->printImageWithEffect(currentPath, m_printMode, resultLabel);

    // 4. 连接进度信号
    connect(m_inkjet, &Inkjet::printProgress, resultDialog,
            [progressLabel](int percent)
            { progressLabel->setText(QString("打印进度：%1%").arg(percent)); });

    const QString modeText = m_printModeText;

    // 5. 连接打印完成信号
    connect(m_inkjet, &Inkjet::printFinished, resultDialog,
            [this, resultDialog, closeResultBtn, prevBtn, nextBtn, currentImageIndex, fileName, originalFormat, modeText](const QPixmap &result)
            {
                Q_UNUSED(result);
                closeResultBtn->setEnabled(true);
                prevBtn->setEnabled(*currentImageIndex > 0);
                nextBtn->setEnabled(*currentImageIndex < m_imagePaths.size() - 1);
                connect(closeResultBtn, &QPushButton::clicked, resultDialog, &QDialog::accept);
                
                // 打印成功，添加记录
                m_printRecord->addRecord(fileName, originalFormat, modeText);
            });

    // 6. 连接错误信号
    connect(m_inkjet, &Inkjet::printError, resultDialog,
            [this, resultDialog, closeResultBtn, prevBtn, nextBtn, currentImageIndex](const QString &error)
            {
                closeResultBtn->setEnabled(true);
                prevBtn->setEnabled(*currentImageIndex > 0);
                nextBtn->setEnabled(*currentImageIndex < m_imagePaths.size() - 1);
                connect(closeResultBtn, &QPushButton::clicked, resultDialog, &QDialog::accept);
                QMessageBox::warning(this, "打印错误", error);
            });

    // 7. 连接图片切换按钮信号
    connect(prevBtn, &QPushButton::clicked, resultDialog, [this, resultDialog, resultLabel, prevBtn, nextBtn, currentImageIndex, modeText]() {
        if (*currentImageIndex > 0) {
            (*currentImageIndex)--;
            const QString newPath = m_imagePaths.at(*currentImageIndex);
            const QString newFileName = m_recordFileNames.at(*currentImageIndex);
            resultDialog->setWindowTitle(QString("打印效果 - %1").arg(newFileName));
            
            // 停止当前打印任务
            m_inkjet->stopPrint();
            
            // 重新渲染新图片
            m_inkjet->printImageWithEffect(newPath, m_printMode, resultLabel);
            
            // 更新按钮状态
            prevBtn->setEnabled(*currentImageIndex > 0);
            nextBtn->setEnabled(*currentImageIndex < m_imagePaths.size() - 1);
        }
    });

    connect(nextBtn, &QPushButton::clicked, resultDialog, [this, resultDialog, resultLabel, prevBtn, nextBtn, currentImageIndex, modeText]() {
        if (*currentImageIndex < m_imagePaths.size() - 1) {
            (*currentImageIndex)++;
            const QString newPath = m_imagePaths.at(*currentImageIndex);
            const QString newFileName = m_recordFileNames.at(*currentImageIndex);
            resultDialog->setWindowTitle(QString("打印效果 - %1").arg(newFileName));
            
            // 停止当前打印任务
            m_inkjet->stopPrint();
            
            // 重新渲染新图片
            m_inkjet->printImageWithEffect(newPath, m_printMode, resultLabel);
            
            // 更新按钮状态
            prevBtn->setEnabled(*currentImageIndex > 0);
            nextBtn->setEnabled(*currentImageIndex < m_imagePaths.size() - 1);
        }
    });
}

void PrintPreviewDialog::onCloseClicked()
{
    accept();
}
