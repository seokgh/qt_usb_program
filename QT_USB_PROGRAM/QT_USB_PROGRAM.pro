################################################################################
#
# Linux PC:
# 	$ sudo apt install libusb-1.0-0-dev
#
# Mac OS:
#
#
# Windows PC:
#
#
################################################################################
QT       += core gui core5compat

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += 3rdparty/

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        usbcomm.cpp
        usbcomm.cpp

HEADERS += \
        mainwindow.h \
        usbcomm.h
        usbcomm.h

FORMS += \
    mainwindow.ui

################################################################################
#
################################################################################
LIBS += -L./3rdparty/libusb-1.0/lib -lusb-1.0


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
