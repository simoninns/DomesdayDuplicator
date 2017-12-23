#ifndef SERIALPORTSELECTDIALOG_H
#define SERIALPORTSELECTDIALOG_H

#include <QDialog>
#include <QDebug>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

namespace Ui {
class serialPortSelectDialog;
}

class serialPortSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit serialPortSelectDialog(QWidget *parent = 0);
    ~serialPortSelectDialog();
    virtual void showEvent(QShowEvent *);

    struct Settings {
            QString name;
            QSerialPort::BaudRate baudRate;
            QSerialPort::DataBits dataBits;
            QSerialPort::Parity parity;
            QSerialPort::StopBits stopBits;
            QSerialPort::FlowControl flowControl;
            bool configured;
        };

    Settings settings() const;

signals:
    void serialPortChanged(void);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::serialPortSelectDialog *ui;

    Settings currentSettings;

    void fillPortsInfo();
    void updateSettings();

};

#endif // SERIALPORTSELECTDIALOG_H
