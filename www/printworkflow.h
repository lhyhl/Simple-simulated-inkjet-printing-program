#ifndef PRINTWORKFLOW_H
#define PRINTWORKFLOW_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <memory>

#include <QTemporaryDir>

#include "inkjet.h"
#include "printrecord.h"

class QWidget;

class PrintPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    PrintPreviewDialog(const QStringList &bmpPaths,
                       const QStringList &recordFileNames,
                       const QStringList &recordOriginalFormats,
                       QWidget *parent,
                       Inkjet *inkjet,
                       PrintRecord *printRecord,
                       PrintMode printMode,
                       const QString &printModeText,
                       std::unique_ptr<QTemporaryDir> tempDir);

    ~PrintPreviewDialog();

    void setCurrentIndex(int index);
    void updateDisplay();

private slots:
    void onPreviousClicked();
    void onNextClicked();
    void onPrintClicked();
    void onCloseClicked();

private:
    QStringList m_imagePaths;
    QStringList m_recordFileNames;
    QStringList m_recordOriginalFormats;
    int m_currentIndex;

    QLabel *m_imageLabel;
    QLabel *m_pageLabel;
    QPushButton *m_previousBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_printBtn;
    QPushButton *m_closeBtn;

    Inkjet *m_inkjet;
    PrintRecord *m_printRecord;

    PrintMode m_printMode;
    QString m_printModeText;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    bool m_imagesSaved; // 标记是否已将图片保存到相册
};

namespace PrintWorkflow
{
void startInteractivePrint(QWidget *parent, Inkjet *inkjet, PrintRecord *record);
void startPreviewFromPaths(QWidget *parent,
                           Inkjet *inkjet,
                           PrintRecord *record,
                           const QStringList &sourcePaths,
                           PrintMode mode,
                           const QString &printModeText);
}

#endif // PRINTWORKFLOW_H
