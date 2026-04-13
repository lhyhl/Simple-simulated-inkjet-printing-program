
#include "linklist.h"

ImageNode::ImageNode(const QString &path) : filePath(path)
{
    pixmap.load(path);
}

//Album链表初始化(双向循环链表）
Album::Album() : head(nullptr),tail(nullptr) {}


//添加图片(尾插法)
void Album::addImage(const QString &path)
{
    auto node = std::make_shared<ImageNode>(path);
    if(!head)
    {
        head = tail = node ;
        head->next = head->prev = head;
    }else{
        node->prev = tail;
        node->next= head;
        tail->next = node ;
        head->prev =node;
        tail =node ;
    }
}

//删除图片
std::shared_ptr<ImageNode> Album::deleteImage(std::shared_ptr<ImageNode> node)
{
    //空指针检查
    if(!node)
        return nullptr;
    //处理唯一节点的情况
    if(node == head && node ==tail){
        head = tail =nullptr;
    }else{
        //处理多节点的情况
        node->prev->next = node->next;
        node->next->prev = node->prev;

        //如果删除的是头节点更新head指向新的头节点（第二个节点）
        if(node == head) head = node->next;

        //如果删除的是尾节点更新next指向新的尾节点（倒数第二个节）
        if(node == tail) tail = node->prev;
    }
    //将摘除的node的前驱和后继都设置为空，防止被删掉
    node->prev =node->next = nullptr;
    return node ;
}


//获取写下一张图片节点
std::shared_ptr<ImageNode>Album:: nextImage(std::shared_ptr<ImageNode> node)
{
    return node ? node->next : nullptr;
}


//获取上一张图片节点
std::shared_ptr<ImageNode> Album::prevImage(std::shared_ptr<ImageNode> node)
{
    return node ? node->prev : nullptr;
}

//循环输出
void Album::printAlbum(int count)
{

}



std::shared_ptr<ImageNode>Album:: getHead(){return head;}

std::shared_ptr<ImageNode>Album:: getTail() { return tail; }


//RecycleBin链表初始化(双向非循环链表）
RecycleBin::RecycleBin() : head(nullptr),tail(nullptr) {}


void RecycleBin::add(std::shared_ptr<ImageNode> node)
{
    //空节点检查
    if(!node) return;
    //断除旧连接关系
    node->next = nullptr;
    node->prev = tail;

    //连接到回收站链表
    if(tail)
        tail->next = node;
    else
        head = node;

    tail =node ;



}

//插地删除图片，释放指针
void RecycleBin::remove(std::shared_ptr<ImageNode> node)
{
    //空指针检查
    if(!node)
        return;

    //删除
    //节点前驱非空
    if (node->prev)
        node->prev->next = node->next;
    //节点前驱为空
    else head = node->next;

    //节点后继非空
    if (node->next)
        node->next->prev = node->prev;
    //节点后继为空
    else tail = node->prev;
}


//将RecycleBin链表的指针取出
 std::shared_ptr<ImageNode>RecycleBin:: restore(std::shared_ptr<ImageNode> node)
{
     //空指针检查
     if(!node)
         return nullptr;

     //节点的前驱不为空
     if(node->prev)
         node->prev->next = node->next;
     //为空
     else
         head = node->next;

     //节点的后继不为空
     if(node->next)
         node->next->prev = node->prev;
     //为空
    else
         tail = node->prev;

    //将摘除的node的前驱和后继都设置为空，防止被删掉
    node->prev = node->next = nullptr;

    return node;

}


//获取写下一张图片节点
std::shared_ptr<ImageNode>RecycleBin:: BinnextImage(std::shared_ptr<ImageNode> node)
{
    return node ? node->next : nullptr;
}


//获取上一张图片节点
std::shared_ptr<ImageNode> RecycleBin::BinprevImage(std::shared_ptr<ImageNode> node)
{
    return node ? node->prev : nullptr;
}

void RecycleBin::printRecycle()
{

}


std::shared_ptr<ImageNode>RecycleBin:: getHead(){return head;}

