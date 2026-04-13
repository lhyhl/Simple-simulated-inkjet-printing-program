#ifndef LINKLIST_H
#define LINKLIST_H

#include <QString>
#include <QPixmap>
#include <memory>


// 图片节点
struct ImageNode {
    QString filePath;                     // 图片路径
    QPixmap pixmap;                       // 缩略图/图片对象
    std::shared_ptr<ImageNode> prev;      // 前驱节点
    std::shared_ptr<ImageNode> next;      // 后继节点

    ImageNode(const QString &path);
};


//双向循环链表（相册）
class Album {
private:
    std::shared_ptr<ImageNode> head;
    std::shared_ptr<ImageNode> tail;

public:
    Album();

    void addImage(const QString &path);                          // 添加图片
    std::shared_ptr<ImageNode> deleteImage(std::shared_ptr<ImageNode> node); // 删除图片
    std::shared_ptr<ImageNode> nextImage(std::shared_ptr<ImageNode> node);   // 下一张
    std::shared_ptr<ImageNode> prevImage(std::shared_ptr<ImageNode> node);   // 上一张
    void printAlbum(int count=10);                                // 处理成灰色
    std::shared_ptr<ImageNode> getHead();
    std::shared_ptr<ImageNode> getTail();
};



// 双向非循环链表（回收站）
class RecycleBin {
private:
    std::shared_ptr<ImageNode> head;
    std::shared_ptr<ImageNode> tail;

public:
    RecycleBin();

    void add(std::shared_ptr<ImageNode> node);                   // 添加到回收站
    std::shared_ptr<ImageNode> restore(std::shared_ptr<ImageNode> node); // 恢复
    void remove(std::shared_ptr<ImageNode> node);                // 彻底删除
    void printRecycle();                                         // 处理成灰色
    std::shared_ptr<ImageNode> getHead();
    std::shared_ptr<ImageNode> BinnextImage(std::shared_ptr<ImageNode> node);   // 下一张
    std::shared_ptr<ImageNode> BinprevImage(std::shared_ptr<ImageNode> node);   // 上一张
};

#endif