/********************************************************************************/
/*  */
/********************************************************************************/
/*
 * USB "응용레이어" 통신 파트 (libusb API 에 한층 더 씌워서, 사용 편의성을 높인다)
 *
 * 크게는 2개 파트로 나눈다:
 * 	1. USB device 와의 통신 (usbcomm)
 * 	2. USB device 의 hot plug 감지 (usb monitor)
 *
 */
#ifndef USBCOMM_H
#define USBCOMM_H

#include <QObject>
#include <QThread>
#include <QList>
#include <QMultiMap>
#include "libusb-1.0/include/libusb.h"

/********************************************************************************/
/* 파트1. USB device 와의 통신 (usbcomm) Class */
/********************************************************************************/
/*
 * USB 통신 main 클래스
 *
 * 이 클래스는 USB 장치와의 통신 데이터 전송을 주로 구현한다.
 *
 * 이 클래스 내부에서는 필요에 따라 libusb의 메서드 인터페이스를 내부적으로 캡슐화하고,
 * 현재 open된 장치 핸들 목록과 선언된 인터페이스 목록을 가지고 있는다.
 *
 * 따라서
 * 장치 핸들 및 Interface와 관련된 작업은
 * 가능한 이 클래스의 메서드를 사용하여 처리하되, 외부에서 libusb 오리지널 인터페이스를 직접 사용하지 않는다.
 * (이 클래스의 내부 유지 관리 목록이 무효화되어, Exception 상황이  발생하지 않도록 하기 위함)
*/
class UsbComm : public QObject
{
    Q_OBJECT
public:
    explicit UsbComm(QObject *parent = 0);
    ~UsbComm();

    /********************************************************************************/
    /*  */
    /********************************************************************************/
    void findUsbDevices();

    /********************************************************************************/
    /* Device 초기화 부분 */
    /********************************************************************************/
    /* 지정 device 열기 (여러개 있을 수 있다) */
    bool openUsbDevice(QMultiMap<quint16,quint16> &vpidMap);

    /* 지정 device 닫기 */
    void closeUsbDevice(libusb_device_handle *deviceHandle);
    /* 모든 device 닫기 */
    void closeAllUsbDevice();

    /* usb device 의 현재 config 활성화  */
    bool setUsbConfig(libusb_device_handle *deviceHandle,int bConfigurationValue=1);
    /* usb device 의 interface 선언  */
    bool claimUsbInterface(libusb_device_handle *deviceHandle,int interfaceNumber);
    /* usb device 의 interface 선언 취소 */
    void releaseUsbInterface(libusb_device_handle *deviceHandle,int interfaceNumber);
    /* usb interface 의 backup config 활성화  */
    bool setUsbInterfaceAltSetting(libusb_device_handle *deviceHandle,int interfaceNumber,int bAlternateSetting);
    bool resetUsbDevice(libusb_device_handle *deviceHandle);

    /********************************************************************************/
    /* 데이터 전송 부분 */
    /********************************************************************************/
    /*  */
    int bulkTransfer(libusb_device_handle *deviceHandle,quint8 endpoint, quint8 *data, int length, quint32 timeout);

    /********************************************************************************/
    /* USB Device 정보 쿼리 */
    /********************************************************************************/
    /* 현재 open된 디바이스 수량 취득 */
    int getOpenedDeviceCount(){return deviceHandleList.size();}

    /*该类中所有方法的函参(libusb_device_handle *deviceHandle)必须通过以下getDeviceHandleFrom*方法获取*/
    libusb_device_handle *getDeviceHandleFromIndex(int index);//通过索引获取打开的设备句柄
    libusb_device_handle *getDeviceHandleFromVpidAndPort(quint16 vid,quint16 pid,qint16 port);//通过vpid和端口号获取打开的设备句柄

private:
    /* usb device 정보 출력 */
    void printDevInfo(libusb_device *usbDevice);

    /* libusb의 하나의 "회화세션", libusb_init() 생성자함수가 신규 */
    libusb_context *context;
    /* open된 usb device handle list */
    QList<libusb_device_handle *> deviceHandleList;

    /* handle 과 해당interface list들의 map */
    QMap<libusb_device_handle *, QList<int> > handleClaimedInterfacesMap;
};





/********************************************************************************/
/* 파트2. USB hot plug 감지 Class */
/********************************************************************************/
/*
 * 이 클래스는
 * 전역 obj 를 정의(긴 생명주기)하여, usb device 에 대해서 hot plug 감지를 하는데 사용하면 된다.
 */
class UsbEventHandler;

class UsbMonitor : public QObject
{
    Q_OBJECT
public:
    UsbMonitor(QObject *parent = 0);
    ~UsbMonitor();

    /* hot plug 감시서비스 등록하기 */
    bool registerHotplugMonitorService(int deviceClass=LIBUSB_HOTPLUG_MATCH_ANY,
                                int vendorId=LIBUSB_HOTPLUG_MATCH_ANY,
                                int productId=LIBUSB_HOTPLUG_MATCH_ANY);
    /* hot plug 감시서비스 등록해제 하기 */
    void deregisterHotplugMonitorService();

signals:
    /* hot plug signal */
    void deviceHotplugSig(bool isAttached);

private:
    /* hot plug callback 함수 */
    static int LIBUSB_CALL hotplugCallback(libusb_context *ctx,libusb_device *device,
                                           libusb_hotplug_event event,void *user_data);

    /* libusb의 하나의 "회화세션", libusb_init() 생성자 함수가 신규 */
    libusb_context *context;
    //热插拔回调句柄
    libusb_hotplug_callback_handle hotplugHandle;
    //热插拔事件处理对象
    UsbEventHandler *hotplugEventHandler;

};

/********************************************************************************/
/* USB event 처리 Class */
/********************************************************************************/
/*
 * 이 클래스는 QThread를 상속하고 run() 메서드를 재정의하여
 * 서브스레드에서 보류 중인 이벤트(주로 USB 장치의 핫 플러그 이벤트)를 처리하고, 핫 플러그 콜백 함수가 트리거되게 한다.
 *
 * 현재 이 클래스는 주로 UsbMonitor의 핫 플러그 감지 인터페이스와 함께 사용되며,
 * 관련 처리는 인터페이스 내에서 이미 캡슐화되어 있으므로 다른 사용법은 생각하지 않아도 된다.
 */
class UsbEventHandler : public QThread
{
    Q_OBJECT
public:
    UsbEventHandler(libusb_context *context, QObject *parent = 0);

    /* Thread 종료 제어 flag 설정 */
    void setStopped(bool stopped){this->stopped = stopped;}

protected:
    virtual void run();

private:
    /* libusb의 하나의 "회화세션", libusb_init() 생성자 함수가 신규 */
    libusb_context *context;
    /* Thread 종료 제어 flag */
    volatile bool stopped;
};

#endif // USBCOMM_H
