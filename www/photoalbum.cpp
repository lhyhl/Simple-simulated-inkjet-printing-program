#include "photoalbum.h"
#include "ui_photoalbum.h"
#include "inkjet.h"
#include "printrecord.h"
#include "print.h"
#include <QFileInfo>
#include <QDialog>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>

Photoalbum::Photoalbum(QWidget *parent)
    : QWidget(parent), ui(new Ui::Photoalbum)
{
    ui->setupUi(this);

    // 初始化列表设置
    ui->listPhoto->setIconSize(QSize(100, 100));
    ui->listPhoto->setResizeMode(QListWidget::Adjust);
    ui->listPhoto->setSpacing(10);
    ui->listPhoto->setViewMode(QListWidget::IconMode);
    // 开启右键菜单策略
    ui->listPhoto->setContextMenuPolicy(Qt::CustomContextMenu);

    // 连接双击信号到预览
    connect(ui->listPhoto, &QListWidget::itemDoubleClicked, this, &Photoalbum::onImageDoubleClicked);

    // 连接自定义右键菜单信号
    connect(ui->listPhoto, &QListWidget::customContextMenuRequested,
            this, &Photoalbum::onContextMenuRequested);

    // 手动连接返回按钮
    connect(ui->back_PH, &QPushButton::clicked,
            this, &Photoalbum::backRequested);

    setAcceptDrops(true);

    ui->listPhoto->setAcceptDrops(true);
    ui->listPhoto->viewport()->setAcceptDrops(true);
    // 禁止用户在列表内部拖拽排序，防止破坏链表顺序
    ui->listPhoto->setDragDropMode(QAbstractItemView::NoDragDrop);

    QString projectRoot = QDir::currentPath();
    m_imageDir = QDir::cleanPath(projectRoot + "/../../images");
    m_inkjet = new Inkjet(this);
    loadFixedImages();
}

Photoalbum::~Photoalbum()
{
    delete ui;
}

// 添加图片到album链表
void Photoalbum::addImage(const QString &filePath)
{
    if (!filePath.endsWith(".bmp", Qt::CaseInsensitive))
        return;

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return;

    // 查重
    auto current = album.getHead();
    if (current)
    {
        auto start = current;
        do
        {
            if (current->filePath == filePath)
                return;
            current = album.nextImage(current);
        } while (current != start && current != nullptr);
    }

    // 添加到 Album 链表
    album.addImage(filePath);
    auto newNode = album.getTail();
    if (!newNode)
        return;

    QPixmap pix(filePath);
    if (pix.isNull())
    {
        qDebug() << "BMP 加载失败:" << filePath;
        return;
    }

    QListWidgetItem *item = new QListWidgetItem();
    QSize thumbSize = ui->listPhoto->iconSize();
    if (thumbSize.isEmpty())
        thumbSize = QSize(100, 100);

    QPixmap thumbPix = pix.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    item->setIcon(QIcon(thumbPix));
    item->setToolTip(fileInfo.fileName());

    // 存储数据
    item->setData(Qt::UserRole, QVariant::fromValue<quintptr>(quintptr(newNode.get())));
    item->setData(Qt::UserRole + 1, filePath);

    ui->listPhoto->addItem(item);
};

// 2. 拖拽事件

void Photoalbum::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
        qDebug() << "drag enter";
    }
    else
    {
        event->ignore();
    }
}

// 拖拽松手后的逻辑，将图片保存并刷新屏幕
void Photoalbum::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;

    foreach (QUrl url, urls)
    {
        QString sourcePath = url.toLocalFile();
        if (sourcePath.isEmpty())
            continue;

        QFileInfo fileInfo(sourcePath);
        QString fileName = fileInfo.fileName();

        // 确保目标目录存在
        QDir dir(m_imageDir);
        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        QString destPath = dir.filePath(fileName);

        // 处理重名
        if (QFile::exists(destPath))
        {
            QString baseName = fileInfo.completeBaseName();
            QString suffix = fileInfo.suffix();
            int counter = 1;
            while (QFile::exists(destPath))
            {
                destPath = dir.filePath(baseName + "_" + QString::number(counter) + "." + suffix);
                counter++;
            }
        }

        // 执行复制
        if (QFile::copy(sourcePath, destPath))
        {
            qDebug() << "图片保存成功:" << destPath;

            addImage(destPath);
        }
        else
        {
            qDebug() << "保存图片失败:" << destPath;
            QMessageBox::warning(this, "错误", "无法保存图片到:\n" + destPath);
        }
    }

    event->acceptProposedAction();
}

// 3. 右键菜单逻辑

void Photoalbum::onContextMenuRequested(const QPoint &pos)
{
    // 获取点击位置的 Item
    QListWidgetItem *item = ui->listPhoto->itemAt(pos);
    if (!item)
    {
        return; // 点击空白处不显示菜单
    }

    // 创建菜单
    QMenu menu(this);
    QAction *deleteAction = menu.addAction("删除图片");
    QAction *bwPrintAction = menu.addAction("黑白打印");
    QAction *cmykPrintAction = menu.addAction("彩色打印 (CMYK)");

    // 执行菜单，等待用户选择
    QAction *selectedAction = menu.exec(ui->listPhoto->viewport()->mapToGlobal(pos));

    if (selectedAction == deleteAction)
    {
        performDelete(item);
    }
    else if (selectedAction == bwPrintAction || selectedAction == cmykPrintAction)
    {
        QString path = item->data(Qt::UserRole + 1).toString();
        QString fileName = QFileInfo(path).fileName();
        QString originalFormat = path.endsWith(".bmp", Qt::CaseInsensitive) ? "BMP" : "OTHER";

        QString mode;
        PrintMode printMode;
        if (selectedAction == bwPrintAction)
        {
            mode = "黑白打印";
            printMode = PrintMode::BlackWhite;
        }
        else
        {
            mode = "彩色打印 (CMYK)";
            printMode = PrintMode::CMYK;
        }

        qDebug() << "准备打印图片:" << path;

        QDialog *previewDlg = new QDialog(this);
        previewDlg->setWindowTitle(QString("打印效果 - %1").arg(fileName));
        previewDlg->resize(900, 700);
        QVBoxLayout *layout = new QVBoxLayout(previewDlg);

        QScrollArea *scrollArea = new QScrollArea(previewDlg);
        QLabel *imgLabel = new QLabel(previewDlg);
        imgLabel->setAlignment(Qt::AlignCenter);
        imgLabel->setMinimumSize(800, 600);
        imgLabel->setStyleSheet("background-color: white; border: 1px solid gray;");
        scrollArea->setWidget(imgLabel);
        layout->addWidget(scrollArea);

        QLabel *progressLabel = new QLabel("打印进度：0%", previewDlg);
        progressLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(progressLabel);

        QPushButton *closeBtn = new QPushButton("关闭", previewDlg);
        closeBtn->setEnabled(false);
        layout->addWidget(closeBtn);

        previewDlg->setAttribute(Qt::WA_DeleteOnClose);
        previewDlg->show();

        connect(previewDlg, &QDialog::finished, m_inkjet,
                [inkjet = m_inkjet](int) { inkjet->stopPrint(); });
        connect(previewDlg, &QObject::destroyed, m_inkjet,
                [inkjet = m_inkjet]() { inkjet->stopPrint(); });

        m_inkjet->printImageWithEffect(path, printMode, imgLabel);

        connect(m_inkjet, &Inkjet::printProgress, previewDlg, [progressLabel](int percent)
                { progressLabel->setText(QString("打印进度：%1%").arg(percent)); });

        connect(m_inkjet, &Inkjet::printFinished, previewDlg, [previewDlg, closeBtn, fileName, originalFormat, mode](const QPixmap &result)
                {
            Q_UNUSED(result);
            closeBtn->setEnabled(true);
            connect(closeBtn, &QPushButton::clicked, previewDlg, &QDialog::accept);

            PrintRecord printRecord;
            printRecord.addRecord(fileName, originalFormat, mode); });

        connect(m_inkjet, &Inkjet::printError, previewDlg, [this, previewDlg, closeBtn](const QString &error)
                {
            closeBtn->setEnabled(true);
            connect(closeBtn, &QPushButton::clicked, previewDlg, &QDialog::accept);
            QMessageBox::warning(this, "打印错误", error); });
    }
}

// 执行删除逻辑 (链表操作 + UI 更新)
void Photoalbum::performDelete(QListWidgetItem *item)
{
    if (!item)
        return;

    // 确认对话框
    int ret = QMessageBox::question(this, "确认删除",
                                    "确定要将这张图片移入回收站吗？",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    QString path = item->data(Qt::UserRole + 1).toString();

    auto targetNode = findNodeByPath(path);

    if (!targetNode)
        return;

    // 回收站目录
    QString recycleDir = QDir::cleanPath(QDir::currentPath() + "/../../recycle");

    QDir dir(recycleDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    QString fileName = QFileInfo(path).fileName();

    QString newPath = dir.filePath(fileName);

    // 处理重名
    int counter = 1;
    while (QFile::exists(newPath))
    {
        newPath = dir.filePath(
            QFileInfo(path).completeBaseName() + "_" + QString::number(counter) + ".bmp");
        counter++;
    }

    // 移动文件
    if (QFile::rename(path, newPath))
    {
        auto removedNode = album.deleteImage(targetNode);

        if (removedNode)
        {
            removedNode->filePath = newPath;

            recycleBin.add(removedNode);

            delete item;

            QMessageBox::information(this, "提示", "图片已移至回收站");
        }
    }
    else
    {
        QMessageBox::warning(this, "错误", "文件移动失败");
    }
}

// 通过路径查找节点
std::shared_ptr<ImageNode> Photoalbum::findNodeByPath(const QString &path)
{
    auto head = album.getHead();
    if (!head)
        return nullptr;

    auto curr = head;
    do
    {
        if (curr->filePath == path)
        {
            return curr;
        }
        curr = album.nextImage(curr);
    } while (curr != head && curr != nullptr);

    return nullptr;
}

// 4. 预览功能 (滚轮缩放)

void Photoalbum::onImageDoubleClicked(QListWidgetItem *item)
{
    QString path = item->data(Qt::UserRole + 1).toString();
    auto currentNode = findNodeByPath(path);
    if (!currentNode)
        return;

    showImageViewer(currentNode);
}

// 滚轮逻辑
class WheelScaleFilter : public QObject
{
public:
    WheelScaleFilter(QGraphicsView *v) : view(v) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel)
        {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
            double factor = 1.15;
            if (wheelEvent->angleDelta().y() > 0)
            {
                view->scale(factor, factor);
            }
            else
            {
                view->scale(1.0 / factor, 1.0 / factor);
            }
            return true; // 吃掉事件，不再传递
        }

        return QObject::eventFilter(obj, event);
    }

private:
    QGraphicsView *view;
};

// 预览图片
void Photoalbum::showImageViewer(std::shared_ptr<ImageNode> currentNode)
{
    QDialog *viewerDialog = new QDialog(this);
    viewerDialog->setAttribute(Qt::WA_DeleteOnClose);
    viewerDialog->setWindowTitle("预览 - " + QFileInfo(currentNode->filePath).fileName());

    viewerDialog->resize(1000, 800);
    viewerDialog->setStyleSheet("background-color: #2b2b2b;");

    QVBoxLayout *layout = new QVBoxLayout(viewerDialog);
    layout->setContentsMargins(0, 0, 0, 0);

    QGraphicsScene *scene = new QGraphicsScene();
    scene->setBackgroundBrush(QColor("#2b2b2b"));

    QGraphicsView *view = new QGraphicsView(scene, viewerDialog);
    view->setRenderHint(QPainter::Antialiasing);
    view->setRenderHint(QPainter::SmoothPixmapTransform);
    view->setDragMode(QGraphicsView::ScrollHandDrag);
    view->setCursor(Qt::OpenHandCursor);
    view->setAlignment(Qt::AlignCenter);
    view->setFrameShape(QFrame::NoFrame);

    // 允许视图背景填充整个区域
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 加载图片
    QPixmap originalPix(currentNode->filePath);
    if (originalPix.isNull())
    {
        QMessageBox::critical(this, "错误", "无法加载图片文件！");
        delete viewerDialog;
        return;
    }

    QGraphicsPixmapItem *pixmapItem = new QGraphicsPixmapItem(originalPix);
    pixmapItem->setTransformationMode(Qt::SmoothTransformation);

    scene->addItem(pixmapItem);

    // 设置场景大小
    scene->setSceneRect(pixmapItem->boundingRect());

    // 自动适应窗口大小
    view->fitInView(pixmapItem, Qt::KeepAspectRatio);

    layout->addWidget(view);
    viewerDialog->setLayout(layout);

    // 安装滚轮过滤器
    WheelScaleFilter *filter = new WheelScaleFilter(view);
    view->installEventFilter(filter);

    viewerDialog->show();
    view->fitInView(pixmapItem, Qt::KeepAspectRatio);
}

// 读取文件内的BMP图片
void Photoalbum::loadFixedImages()
{
    QDir dir(m_imageDir);

    // 如果目录不存在，尝试创建它，或者直接返回
    if (!dir.exists())
    {
        qDebug() << "图片目录不存在，正在创建:" << m_imageDir;
        if (!dir.mkpath("."))
        {
            qDebug() << "创建目录失败!";
            return;
        }
    }

    // 设置过滤器：只查找 .bmp 文件
    dir.setNameFilters(QStringList() << "*.bmp");
    // 只查找当前目录
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);

    QFileInfoList fileList = dir.entryInfoList();

    if (fileList.isEmpty())
    {
        qDebug() << "固定文件夹中没有找到 BMP 图片";
        return;
    }

    qDebug() << "找到" << fileList.size() << "张 BMP 图片，开始加载...";

    // 遍历文件并添加到相册
    for (const QFileInfo &info : fileList)
    {
        QString fullPath = info.absoluteFilePath();
        // 添加图片
        addImage(fullPath);
    }

    qDebug() << "固定图片加载完成";
}