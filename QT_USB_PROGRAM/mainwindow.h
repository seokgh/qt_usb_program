#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <usbcomm.h>
#include <QStringListModel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /* */
    UsbComm		m_usbComm;

public slots:
    void slotGetDevInfoFromLibusb(QString vid, QString pid);

private slots:
    void on_pushButton_list_usb_devices_clicked();

    void on_pushButton_reset_usb_device_clicked();

    void on_pushButton_hot_plug_monitor_clicked();

    void on_pushButton_read_usb_device_clicked();

    void on_pushButton_write_usb_device_clicked();

private:
    Ui::MainWindow *ui;

    QStringListModel	m_model_of_vid_pid_list;
    QStringList			m_dataList_of_vid_pid_list;
};
#endif // MAINWINDOW_H
