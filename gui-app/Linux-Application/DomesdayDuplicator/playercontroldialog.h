#ifndef PLAYERCONTROLDIALOG_H
#define PLAYERCONTROLDIALOG_H

#include <QDialog>

namespace Ui {
class playerControlDialog;
}

class playerControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit playerControlDialog(QWidget *parent = 0);
    ~playerControlDialog();

private:
    Ui::playerControlDialog *ui;
};

#endif // PLAYERCONTROLDIALOG_H
