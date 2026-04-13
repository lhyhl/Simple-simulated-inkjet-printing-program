#ifndef PRINT_H
#define PRINT_H

#include <QStringList>
#include <QWidget>

namespace Ui
{
    class Print;
}

class Inkjet;
class PrintRecord;

class Print : public QWidget
{
    Q_OBJECT

public:
    explicit Print(QWidget *parent = nullptr);
    ~Print();

    void setInkjet(Inkjet *inkjet);
    void setPrintRecord(PrintRecord *printRecord);

signals:
    void backRequested();

private slots:
    void onSelectFileClicked();
    void onPrintBtnClicked();

private:
    Ui::Print *ui;
    Inkjet *m_inkjet;
    PrintRecord *m_printRecord;
    QStringList m_selectedFiles;
};

#endif // PRINT_H
