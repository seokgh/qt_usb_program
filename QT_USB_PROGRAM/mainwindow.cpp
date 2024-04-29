#include "mainwindow.h"
#include "ui_mainwindow.h"

/********************************************************************************/
/* */
/********************************************************************************/
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /*  */
    ui->listView_vid_pid_list->setModel(&m_model_of_vid_pid_list);

    /*  */
    connect(&m_usbComm, SIGNAL(sigPutDevInfo2MainUI(QString, QString)), this, SLOT(slotGetDevInfoFromLibusb(QString, QString)));
}

/********************************************************************************/
/* */
/********************************************************************************/
MainWindow::~MainWindow()
{
    delete ui;
}

/********************************************************************************/
/* List all USB devices */
/********************************************************************************/
void MainWindow::on_pushButton_list_usb_devices_clicked()
{
    qDebug() << Q_FUNC_INFO;

    /* USB vid/pid list 지우기 */
    m_dataList_of_vid_pid_list.clear();

    /*  */
    m_usbComm.findUsbDevices();
}


/********************************************************************************/
/* */
/********************************************************************************/
void MainWindow::on_pushButton_reset_usb_device_clicked()
{
    qDebug() << Q_FUNC_INFO;

}


/********************************************************************************/
/* */
/********************************************************************************/
void MainWindow::on_pushButton_hot_plug_monitor_clicked()
{
    qDebug() << Q_FUNC_INFO;

}


/********************************************************************************/
/* */
/********************************************************************************/
void MainWindow::on_pushButton_read_usb_device_clicked()
{
    qDebug() << Q_FUNC_INFO;

}


/********************************************************************************/
/* */
/********************************************************************************/
void MainWindow::on_pushButton_write_usb_device_clicked()
{
    qDebug() << Q_FUNC_INFO;

}

/********************************************************************************/
/* */
/********************************************************************************/
void MainWindow::slotGetDevInfoFromLibusb(QString vid, QString pid)
{
    qDebug() << Q_FUNC_INFO << "VID:" << vid << "PID:" << pid;

    /* 누적 추가한다 */
    m_dataList_of_vid_pid_list << vid + ", " + pid;

    m_model_of_vid_pid_list.setStringList(m_dataList_of_vid_pid_list);
}
