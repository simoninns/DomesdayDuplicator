#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
class ProgressDialog;
}

class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = nullptr);
    ~ProgressDialog();

    void setPercentage(qint32 percentage);
    void setText(QString message);

private:
    Ui::ProgressDialog *ui;
};

#endif // PROGRESSDIALOG_H
