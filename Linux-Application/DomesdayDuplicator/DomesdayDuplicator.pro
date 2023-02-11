#-------------------------------------------------
#
# Project created by QtCreator 2018-09-10T10:51:04
#
#-------------------------------------------------

QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += serialport

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

CONFIG += c++11

# Include the libUSB library
unix {
    INCLUDEPATH += "/usr/include/libusb-1.0"
    LIBS += -L"/usr/lib" -lusb-1.0
}
win32 {
    LIBS += -L"$$PWD/libusb-1.0.0/" -llibusb-1.0 -lAdvAPI32
    INCLUDEPATH += "$$PWD/libusb-1.0.0"
    DEFINES += NOMINMAX QUSB_LIBRARY
}

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    configuration.cpp \
    usbdevice.cpp \
    aboutdialog.cpp \
    configurationdialog.cpp \
    usbcapture.cpp \
    playerremotedialog.cpp \
    playercommunication.cpp \
    playercontrol.cpp \
    automaticcapturedialog.cpp \
    advancednamingdialog.cpp \
    amplitudemeasurement.cpp \
    qcustomplot.cpp

HEADERS += \
        mainwindow.h \
    configuration.h \
    usbdevice.h \
    aboutdialog.h \
    configurationdialog.h \
    usbcapture.h \
    playerremotedialog.h \
    playercommunication.h \
    playercontrol.h \
    automaticcapturedialog.h \
    advancednamingdialog.h \
    amplitudemeasurement.h \
    qcustomplot.h

FORMS += \
        mainwindow.ui \
    aboutdialog.ui \
    configurationdialog.ui \
    playerremotedialog.ui \
    automaticcapturedialog.ui \
    advancednamingdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /usr/local/bin/
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

win32 {
    RC_FILE = resources.rc
}
