/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.5.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout;
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout;
    QPushButton *pushButton_list_usb_devices;
    QPushButton *pushButton_reset_usb_device;
    QPushButton *pushButton_hot_plug_monitor;
    QListView *listView;
    QVBoxLayout *verticalLayout_2;
    QPushButton *pushButton_read_usb_device;
    QTextBrowser *textBrowser;
    QVBoxLayout *verticalLayout_3;
    QPlainTextEdit *plainTextEdit;
    QPushButton *pushButton_write_usb_device;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(835, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        gridLayout = new QGridLayout(centralwidget);
        gridLayout->setObjectName("gridLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        pushButton_list_usb_devices = new QPushButton(centralwidget);
        pushButton_list_usb_devices->setObjectName("pushButton_list_usb_devices");

        verticalLayout->addWidget(pushButton_list_usb_devices);

        pushButton_reset_usb_device = new QPushButton(centralwidget);
        pushButton_reset_usb_device->setObjectName("pushButton_reset_usb_device");

        verticalLayout->addWidget(pushButton_reset_usb_device);

        pushButton_hot_plug_monitor = new QPushButton(centralwidget);
        pushButton_hot_plug_monitor->setObjectName("pushButton_hot_plug_monitor");

        verticalLayout->addWidget(pushButton_hot_plug_monitor);

        listView = new QListView(centralwidget);
        listView->setObjectName("listView");

        verticalLayout->addWidget(listView);


        horizontalLayout->addLayout(verticalLayout);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName("verticalLayout_2");
        pushButton_read_usb_device = new QPushButton(centralwidget);
        pushButton_read_usb_device->setObjectName("pushButton_read_usb_device");

        verticalLayout_2->addWidget(pushButton_read_usb_device);

        textBrowser = new QTextBrowser(centralwidget);
        textBrowser->setObjectName("textBrowser");

        verticalLayout_2->addWidget(textBrowser);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName("verticalLayout_3");
        plainTextEdit = new QPlainTextEdit(centralwidget);
        plainTextEdit->setObjectName("plainTextEdit");

        verticalLayout_3->addWidget(plainTextEdit);

        pushButton_write_usb_device = new QPushButton(centralwidget);
        pushButton_write_usb_device->setObjectName("pushButton_write_usb_device");

        verticalLayout_3->addWidget(pushButton_write_usb_device);


        verticalLayout_2->addLayout(verticalLayout_3);


        horizontalLayout->addLayout(verticalLayout_2);


        gridLayout->addLayout(horizontalLayout, 0, 0, 1, 1);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 835, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        pushButton_list_usb_devices->setText(QCoreApplication::translate("MainWindow", "list usb devices", nullptr));
        pushButton_reset_usb_device->setText(QCoreApplication::translate("MainWindow", "reset usb device", nullptr));
        pushButton_hot_plug_monitor->setText(QCoreApplication::translate("MainWindow", "hotplug monitor", nullptr));
        pushButton_read_usb_device->setText(QCoreApplication::translate("MainWindow", "read usb device", nullptr));
        pushButton_write_usb_device->setText(QCoreApplication::translate("MainWindow", "write usb device", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
