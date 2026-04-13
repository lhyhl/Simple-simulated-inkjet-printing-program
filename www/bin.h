#ifndef BIN_H
#define BIN_H

#include <QWidget>
#include <QListWidget>
#include <QMenu>
#include <QAction>
#include "photoalbum.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Bin; }
QT_END_NAMESPACE

class Bin : public QWidget
{
    Q_OBJECT

public:
    explicit Bin(QWidget *parent = nullptr);
    ~Bin();

    void loadFromRecycleDir();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onCustomContextMenuRequested(const QPoint &pos);
    void onImageDoubleClicked(QListWidgetItem *item);

private:
    Ui::Bin *ui;

    RecycleBin m_recycleBin;
    Album m_restoredAlbum;

    QString m_recycleDir;
    QString m_targetImageDir;

    void PermanentDelete(QListWidgetItem *item);
    std::shared_ptr<ImageNode>BinfindNodeByPath(const QString &path);
    void RestoreImage(QListWidgetItem *item);
    void showImageViewer(std::shared_ptr<ImageNode> currentNode);

    bool m_isInitialized;  // 标记是否已初始化
signals:
    void BinbackRequested();
    void imageRestored(const QString &filePath);  // 还原图片信号
};

#endif // BIN_H