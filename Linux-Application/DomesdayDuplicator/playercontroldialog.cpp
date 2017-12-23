#include "playercontroldialog.h"
#include "ui_playercontroldialog.h"

playerControlDialog::playerControlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::playerControlDialog)
{
    ui->setupUi(this);
}

playerControlDialog::~playerControlDialog()
{
    delete ui;
}
