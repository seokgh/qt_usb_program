#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_list_usb_devices_clicked()
{
    qDebug() << Q_FUNC_INFO;

}


void MainWindow::on_pushButton_reset_usb_device_clicked()
{
    qDebug() << Q_FUNC_INFO;

}


void MainWindow::on_pushButton_hot_plug_monitor_clicked()
{
    qDebug() << Q_FUNC_INFO;

}


void MainWindow::on_pushButton_read_usb_device_clicked()
{
    qDebug() << Q_FUNC_INFO;

}


void MainWindow::on_pushButton_write_usb_device_clicked()
{
    qDebug() << Q_FUNC_INFO;

}

