#include "print.h"
#include "ui_print.h"
#include "printworkflow.h"

#include <QDir>
#include <QFileDialog>
#include <QPushButton>
#include "inkjet.h"
#include "printrecord.h"

Print::Print(QWidget *parent)
    : QWidget(parent), ui(new Ui::Print), m_inkjet(nullptr), m_printRecord(nullptr)
{
    ui->setupUi(this);

    connect(ui->BackBtn, &QPushButton::clicked,
            this, &Print::backRequested);
    connect(ui->PrintBtn, &QPushButton::clicked, this, &Print::onPrintBtnClicked);
}

Print::~Print()
{
    delete ui;
}

void Print::setInkjet(Inkjet *inkjet)
{
    m_inkjet = inkjet;
}

void Print::setPrintRecord(PrintRecord *printRecord)
{
    m_printRecord = printRecord;
}

void Print::onSelectFileClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "选择文件",
        QDir::currentPath(),
        "图片文件 (*.pdf *.bmp);;所有文件 (*.*)");

    if (!files.isEmpty())
        m_selectedFiles = files;
}

void Print::onPrintBtnClicked()
{
    PrintWorkflow::startInteractivePrint(this, m_inkjet, m_printRecord);
}
