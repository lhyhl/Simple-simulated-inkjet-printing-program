#ifndef PHOTOALBUM_H
#define PHOTOALBUM_H

#include <QMainWindow>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDragEnterEvent>
#include <QStackedWidget>
#include <QDropEvent>
#include <QPoint>
#include <QString>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPixmap>
#include <QIcon>
#include <QFileInfo>
#include <QFile>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QFrame>
#include <QPainter>
#include <QWheelEvent>
#include <QObject>
#include <QEvent>
#include <QDebug>
#include <QUrl>
#include <QMimeData>
#include <QGraphicsItem>
#include <QDir>


#include "linklist.h"
#include "inkjet.h"
#include "printrecord.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Photoalbum; }
QT_END_NAMESPACE

class Photoalbum : public QWidget {
    Q_OBJECT

public:
    explicit Photoalbum(QWidget *parent = nullptr);
    ~Photoalbum();

    // 手动添加图片的方法
    void addImage(const QString &filePath);
    // 获取图片存储目录的绝对路径
    QString getImageDirPath() const;

    // 加载固定文件夹下的所有 BMP 图片
    void loadFixedImages();

    //显示图片查看器
    void showImageViewer(std::shared_ptr<ImageNode> currentNode);

protected:
    // 拖放事件重写
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // 右键菜单槽函数
    void onContextMenuRequested(const QPoint &pos);

    // 双击预览槽函数
    void onImageDoubleClicked(QListWidgetItem *item);


private:
    // 执行删除操作
    void performDelete(QListWidgetItem *item);

    //根据路径查找节点
    std::shared_ptr<ImageNode> findNodeByPath(const QString &path);



    // 成员变量
    Ui::Photoalbum *ui;
    Album album;       // 主相册链表
    RecycleBin recycleBin; // 回收站链表
    QString m_imageDir; // 存储相册图片的目录路径
    Inkjet *m_inkjet;
signals:
    void backRequested();   // 返回主界面信号
};

#endif // PHOTOALBUM_H