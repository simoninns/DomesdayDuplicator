#ifndef DOMESDAYDUPLICATOR_H
#define DOMESDAYDUPLICATOR_H

#include <QMainWindow>
#include <QDebug>
#include <QLabel>

#include "usbdevice.h"

class QLabel;

namespace Ui {
class domesdayDuplicator;
}

class domesdayDuplicator : public QMainWindow
{
    Q_OBJECT

public:
    explicit domesdayDuplicator(QWidget *parent = 0);
    ~domesdayDuplicator();

private slots:
    void on_transferButton_clicked();

private:
    Ui::domesdayDuplicator *ui;

    usbDevice *domDupDevice;
    QLabel *status; // Status label for the status bar
    bool transferInProgressFlag;
};

#endif // DOMESDAYDUPLICATOR_H
