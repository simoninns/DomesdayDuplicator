#include "progressdialog.h"
#include "ui_progressdialog.h"

ProgressDialog::ProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

// Set the percentage value of the progress bar
void ProgressDialog::setPercentage(qint32 percentage)
{
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    ui->progressBar->setValue(percentage);
}

// Change the processing label information
void ProgressDialog::setText(QString message)
{
    ui->processingLabel->setText(message);
}

void ProgressDialog::closeEvent(QCloseEvent *event)
{
    qDebug() << "ProgressDialog::closeEvent(): User attempted to close progress dialogue - ignoring";
    event->ignore();
}
