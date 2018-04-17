#-------------------------------------------------
#
# Project created by QtCreator 2017-11-22T13:07:18
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets serialport

TARGET = DomesdayDuplicator
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Include the libUSB library
INCLUDEPATH += "/usr/local/include/libusb-1.0"
LIBS += -L"/usr/local/lib" -lusb-1.0

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    QtUsb/qbaseusb.cpp \
    QtUsb/qlibusb.cpp \
    QtUsb/qusbmanager.cpp \
    usbdevice.cpp \
    QtUsb/qusbbulktransfer.cpp \
    playercontroldialog.cpp \
    serialportselectdialog.cpp \
    aboutdialog.cpp \
    lvdpcontrol.cpp

HEADERS += \
        mainwindow.h \
    QtUsb/qbaseusb.h \
    QtUsb/qlibusb.h \
    QtUsb/qusb.h \
    QtUsb/qusb_compat.h \
    QtUsb/qusb_global.h \
    QtUsb/qusb_types.h \
    QtUsb/qusbmanager.h \
    usbdevice.h \
    QtUsb/qusbbulktransfer.h \
    playercontroldialog.h \
    serialportselectdialog.h \
    aboutdialog.h \
    lvdpcontrol.h

FORMS += \
        mainwindow.ui \
    playercontroldialog.ui \
    serialportselectdialog.ui \
    aboutdialog.ui

RESOURCES += \
    images.qrc
