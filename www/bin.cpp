#include "bin.h"
#include "ui_bin.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPixmap>
#include <QIcon>
#include <QVBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QWheelEvent>
#include <QObject>
#include <QEvent>
#include <QDialog>
#include <QShowEvent>

/*
 * 界面与数据加载：从指定目录加载图片并显示。
 * 文件操作：实现右键菜单，处理还原（移动文件）和彻底删除。
 * 图片预览：实现了一个支持滚轮缩放的自定义图片查看器
 */


Bin::Bin(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Bin)
    , m_isInitialized(false)
{
    ui->setupUi(this);

    // --- 初始化设置 ---
    ui->listBin->setIconSize(QSize(100, 100));
    ui->listBin->setResizeMode(QListWidget::Adjust);
    ui->listBin->setSpacing(10);
    ui->listBin->setViewMode(QListWidget::IconMode);
    ui->listBin->setDragDropMode(QAbstractItemView::NoDragDrop);

    // --- 路径初始化 ---
    QString projectRoot = QDir::currentPath();
    m_recycleDir = QDir::cleanPath(projectRoot + "/../../recycle");
    m_targetImageDir = QDir::cleanPath(projectRoot + "/../../images");

    // --- 信号连接 ---
    connect(ui->back_BIN, &QPushButton::clicked, this, [this](bool){
        emit BinbackRequested();
    });

    // 右键菜单：启用自定义上下文菜单
    ui->listBin->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listBin, &QListWidget::customContextMenuRequested,
            this, &Bin::onCustomContextMenuRequested);

    // 双击事件：用于预览图片
    connect(ui->listBin, &QListWidget::itemDoubleClicked, this, &Bin::onImageDoubleClicked);
}

/*
 * @brief 从回收站目录加载图片到列表
 *
 * 扫描 m_recycleDir 目录下的所有 .bmp 文件，
 * 将其添加到内部链表 m_recycleBin 和 UI 列表控件中。
 */
void Bin::loadFromRecycleDir() {
    ui->listBin->clear();// 清空当前显示
    m_recycleBin = RecycleBin();// 重置内部数据链表

    QDir dir(m_recycleDir);
    if (!dir.exists()) {
        qDebug() << "回收站目录不存在:" << m_recycleDir;
        return;
    }

    // 设置过滤器：只显示 .bmp 文件
    dir.setNameFilters(QStringList() << "*.bmp");
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();

    for (const QFileInfo &info : fileList) {
        QString fullPath = info.absoluteFilePath();

        // 1. 添加到内部数据链表
        auto node = std::make_shared<ImageNode>(fullPath);
        m_recycleBin.add(node);

         // 2. 加载缩略图并添加到 UI 列表
        QPixmap pix(fullPath);
        if (pix.isNull()) continue;

        // 缩放图片以适应 100x100 的图标区域，保持宽高比
        QListWidgetItem *item = new QListWidgetItem();
        item->setIcon(QIcon(pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        item->setToolTip(info.fileName());// 鼠标悬停显示文件名
        item->setData(Qt::UserRole + 1, fullPath); // 使用 UserRole 存储文件绝对路径
        ui->listBin->addItem(item);
    }
    
    m_isInitialized = true;
}

Bin::~Bin() {
    delete ui;
}


/*
 * @brief 窗口显示事件
 *
 * 每次窗口显示时（例如从其他页面切换回来），重新加载回收站内容，
 * 确保显示的是最新的文件状态。
 */
void Bin::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    loadFromRecycleDir();
}

/*
 * @brief 处理右键菜单请求
 *
 * 在指定位置弹出菜单，提供“还原”和“彻底删除”选项。
 */
void Bin::onCustomContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = ui->listBin->itemAt(pos);
    if (!item) return;// 点击在空白处则不显示

    QMenu menu(this);
    QAction *restoreAction = menu.addAction("还原");
    QAction *deleteAction = menu.addAction("彻底删除");

    // 执行菜单并获取用户选择
    QAction *selectedAction = menu.exec(ui->listBin->viewport()->mapToGlobal(pos));
    if (selectedAction == restoreAction) {
        RestoreImage(item);
    } else if (selectedAction == deleteAction) {
        PermanentDelete(item);
    }
}


//执行彻底删除逻辑
void Bin::PermanentDelete(QListWidgetItem *item)
{
    if (!item) return;

    // 二次确认弹窗
    int ret = QMessageBox::question(this, "确认删除",
                                    "确定要将这张图片彻底删除吗？",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // 获取文件路径
    QString path = item->data(Qt::UserRole + 1).toString();
    // 在内部链表中查找对应节点
    auto targetNode = BinfindNodeByPath(path);

    if (!targetNode) return;

    // 1. 从逻辑链表中移除
    m_recycleBin.remove(targetNode);

    // 2. 从物理磁盘删除
    if (QFile::remove(path)) {
        delete item;// 从 UI 移除
        QMessageBox::information(this, "提示", "图片已彻底删除");
    } else {
        QMessageBox::warning(this, "错误", "文件删除失败");
    }
}



//还原图片操作
void Bin::RestoreImage(QListWidgetItem *item)
{
    if (!item) return;

    QString path = item->data(Qt::UserRole + 1).toString();
    auto targetNode = BinfindNodeByPath(path);

    if (!targetNode) return;

    // 确保目标目录存在
    QDir dir(m_targetImageDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString fileName = QFileInfo(path).fileName();
    QString newPath = dir.filePath(fileName);

    // 处理重名文件逻辑

    int counter = 1;
    while (QFile::exists(newPath)) {
        QString baseName = QFileInfo(path).completeBaseName();
        QString suffix = QFileInfo(path).suffix();
        newPath = dir.filePath(baseName + "_" + QString::number(counter) + "." + suffix);
        counter++;
    }

    // 执行移动（重命名）操作
    if (QFile::rename(path, newPath)) {
        m_recycleBin.remove(targetNode); // 从逻辑链表移除
        delete item;// 从 UI 移除
        
        emit imageRestored(newPath);// 发送还原信号（通知其他模块更新）
        
        QMessageBox::information(this, "提示", "图片已还原到相册");
    } else {
        QMessageBox::warning(this, "错误", "文件移动失败");
    }
}





//通过路径查找节点
std::shared_ptr<ImageNode> Bin::BinfindNodeByPath(const QString &path) {
    auto head = m_recycleBin.getHead();
    if (!head) return nullptr;

    auto curr = head;
    // 循环遍历循环链表
    do {
        if (curr->filePath == path) {
            return curr;
        }
        curr = m_recycleBin.BinnextImage(curr);
    } while (curr != head && curr != nullptr);

    return nullptr;
}


/*
 * @brief 双击图片项槽函数
 *
 * 获取路径，查找节点，并打开预览窗口。
 */
void Bin::onImageDoubleClicked(QListWidgetItem *item)
{
    QString path = item->data(Qt::UserRole + 1).toString();
    auto currentNode = BinfindNodeByPath(path);
    if (!currentNode) return;

    showImageViewer(currentNode);
}


/*
 * @brief 滚轮缩放事件过滤器
 *
 * 这是一个辅助类，用于拦截 QGraphicsView 的滚轮事件，
 * 实现鼠标滚轮控制图片缩放的功能。
 */
class BinWheelScaleFilter : public QObject {
public:
    BinWheelScaleFilter(QGraphicsView *v) : view(v) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            double factor = 1.15;// 缩放系数
            // 向上滚动放大，向下滚动缩小
            if (wheelEvent->angleDelta().y() > 0) {
                view->scale(factor, factor);
            } else {
                view->scale(1.0 / factor, 1.0 / factor);
            }
            return true;// 拦截事件，不再向下传递
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QGraphicsView *view;
};


/*
 * @brief 显示图片查看器
 *
 * 创建一个模态对话框，使用 QGraphicsView 显示高清大图，
 * 并支持拖拽平移和滚轮缩放。
 */
void Bin::showImageViewer(std::shared_ptr<ImageNode> currentNode) {
    QDialog *viewerDialog = new QDialog(this);
    viewerDialog->setAttribute(Qt::WA_DeleteOnClose);// 关闭时自动释放内存
    viewerDialog->setWindowTitle("预览 - " + QFileInfo(currentNode->filePath).fileName());

    viewerDialog->resize(1000, 800);
    viewerDialog->setStyleSheet("background-color: #2b2b2b;");// 深色背景

    QVBoxLayout *layout = new QVBoxLayout(viewerDialog);
    layout->setContentsMargins(0, 0, 0, 0);// 无边距

    // --- 设置 Graphics Scene ---
    QGraphicsScene *scene = new QGraphicsScene();
    scene->setBackgroundBrush(QColor("#2b2b2b"));

     // --- 设置 Graphics View ---
    QGraphicsView *view = new QGraphicsView(scene, viewerDialog);
    view->setRenderHint(QPainter::Antialiasing);// 开启抗锯齿
    view->setRenderHint(QPainter::SmoothPixmapTransform);// 开启平滑变换
    view->setDragMode(QGraphicsView::ScrollHandDrag);// 开启手型拖拽模式
    view->setCursor(Qt::OpenHandCursor);
    view->setAlignment(Qt::AlignCenter);
    view->setFrameShape(QFrame::NoFrame);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // --- 加载图片 ---
    QPixmap originalPix(currentNode->filePath);
    if (originalPix.isNull()) {
        QMessageBox::critical(this, "错误", "无法加载图片文件！");
        delete viewerDialog;
        return;
    }

    // 将图片添加到场景中
    QGraphicsPixmapItem *pixmapItem = new QGraphicsPixmapItem(originalPix);
    pixmapItem->setTransformationMode(Qt::SmoothTransformation);

    scene->addItem(pixmapItem);
    scene->setSceneRect(pixmapItem->boundingRect());

     // --- 初始化视图 ---
    view->fitInView(pixmapItem, Qt::KeepAspectRatio);// 适应窗口大小

    layout->addWidget(view);
    viewerDialog->setLayout(layout);

    // --- 安装滚轮事件过滤器 ---
    BinWheelScaleFilter *filter = new BinWheelScaleFilter(view);
    view->installEventFilter(filter);

    viewerDialog->show();
    view->fitInView(pixmapItem, Qt::KeepAspectRatio);// 确保显示居中
}
