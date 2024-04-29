/********************************************************************************/
/* USB "응용레이어" 통신 Part (libusb API 에 한층 더 씌워서, 사용 편의성을 높인다) */
/********************************************************************************/
#include "usbcomm.h"
#include <QDebug>
#include <QCoreApplication>

/********************************************************************************/
/* Part1: UsbComm */
/********************************************************************************/

/********************************************************************************/
/*
 *@brief: 생성자 함수，libusb에 대해 초기화 작업을 실시한다.
 *@param:
 *@return:
 */
/********************************************************************************/
UsbComm::UsbComm(QObject *parent): QObject(parent)
{
    context = NULL;

    /* libusb 초기화 */
    int err = libusb_init(&context);
    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_init error:" << libusb_error_name(err);
    }

    /* log level 설정 */
    libusb_set_debug(context, LIBUSB_LOG_LEVEL_WARNING);	//old ver
    //libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);	//new ver
}

/********************************************************************************/
/*
 *@brief: 소멸자 함수，libusb에 대해 De-init작업을 실시한다.
 *@param:
 *@return:
 */
/********************************************************************************/
UsbComm::~UsbComm()
{
    closeAllUsbDevice();
    libusb_exit(context);
}

/********************************************************************************/
/*
 *@brief: 현재 접속된 모든 USB device 를 탐색하여, device 정보를 출력
 *@param:
 *@return:
 */
/********************************************************************************/
void UsbComm::findUsbDevices()
{
    libusb_device **devs;

    /* get device list */
    ssize_t count = libusb_get_device_list(context, &devs);

    for (int i = 0; i < count; i++)
        printDevInfo(devs[i]);

    /* */
    libusb_free_device_list(devs, 1);
}

/********************************************************************************/
/*
 * 지정한 usb device를 open한다
 *
 * NOTE:
 * 1. 이 함수는 all usb device list를 스캔하여, 지정한 장치를 open하여, 그 핸들러를 맴버변수 deviceHandleList 에 집어넣는다.
 * 	실은 libusb_open_device_with_vid_pid() 라는 함수로도 지정한 vid/pid 로 usb장비를 열수 있다.
 * 	함수 내에서도 libusb_get_device_list() 로 모든 device 를 취득하여 device descript를 스캔하여, vid/pid로 비교를 진행한다.
 * 	비교해서 매칭되는 게 있으면 libusb_open() 로 장비를 열고, 핸들러를 반환하고, device list 를 free한다.
 *
 * 	하지만
 * 	libusb-1.0 의 설명을 보면, 이 함수에 제한이 있다:
 * 	여러개의 device가 같은 id를 사용하면, 첫번째 것 핸들러만 반환한다고 한다... 따라서, 정식 사용에는 패기?
 *
 *
 * 2. usb device를 열려면 권한이 필요하다.
 * 	일반 user가 열때는 “LIBUSB_ERROR_ACCESS” Error가 반환될 가능성이 높다.
 * 	이를 해결하려면, udev 규칙 파일에 해당 usb device 에게 r/w 권한(MODE="0666")을 추가해야 한다.
 *
 * @param:   vpidMap: <vid, pid> table
 * @return:  true=OK  false=NG
 */
/********************************************************************************/
bool UsbComm::openUsbDevice(QMultiMap<quint16, quint16> &vpidMap)
{
    if (vpidMap.isEmpty()) {
        qDebug() << "vpidMap is empty";
        return false;
    }

    /* 먼저 모든 이미 열린 device 를 닫는다 */
    closeAllUsbDevice();

    libusb_device **devs;

    /* get usb devices list */
    ssize_t count = libusb_get_device_list(context, &devs);
    if (count < 0) {
        qDebug() << "libusb_get_device_list is error";
        return false;
    }
    for (int i = 0; i < count; i++) {
        /* device */
        libusb_device_descriptor deviceDesc;

        int err = libusb_get_device_descriptor(devs[i], &deviceDesc);
        if (err != LIBUSB_SUCCESS) {
            qDebug() << "libusb_get_device_descriptor error:" << libusb_error_name(err);
            continue;
        }

        /* vid, pid 가 매칭되는 device를 찾는다 */
        if (vpidMap.uniqueKeys().contains(deviceDesc.idVendor) && vpidMap.values(deviceDesc.idVendor).contains(deviceDesc.idProduct)) {
            libusb_device_handle *deviceHandle = NULL;
            int err = libusb_open(devs[i], &deviceHandle);
            if (err != LIBUSB_SUCCESS) {
                qDebug() << "libusb_open error:" << libusb_error_name(err);
            } else {
                deviceHandleList.append(deviceHandle);
            }
        }
    }

    /* free device list */
    libusb_free_device_list(devs, 1);

    return (bool)deviceHandleList.size();
}

/********************************************************************************/
/*
 *@brief: 지정 usb device 닫기
 *@param:
 *@return:
 */
/********************************************************************************/
void UsbComm::closeUsbDevice(libusb_device_handle *deviceHandle)
{
    /* device 의 모든 interface 를 free한다 */
    releaseUsbInterface(deviceHandle, -1);

    /* open 된 device 를 닫는다 */
    if (deviceHandleList.contains(deviceHandle)) {
        libusb_close(deviceHandle);
        deviceHandleList.removeAll(deviceHandle);
    }
}

/********************************************************************************/
/*
 *@brief: 모든 device 닫기
 *@param:
 *@return:
 */
/********************************************************************************/
void UsbComm::closeAllUsbDevice()
{
    for (int i = 0; i < deviceHandleList.size(); i++)
        closeUsbDevice(deviceHandleList.at(i));
}

/********************************************************************************/
/*
 *usb device 의 현재 config 를 활성화한다 (config가 1개인 usb device 는 default로 활성화되므로 호출안해도 된다)
 *@param:   deviceHandle: device handle
 *@param:   bConfigurationValue: config num
 *@return:  bool:true=OK  false=NG
 */
/********************************************************************************/
bool UsbComm::setUsbConfig(libusb_device_handle *deviceHandle, int bConfigurationValue)
{
    if (!deviceHandleList.contains(deviceHandle)) {
        return false;
    }

    /* 지정 config 를 활성화한다
     * (1개 USB Device 에는 config 가 여러개 있을 수 있지만, 동일시각에 1개만 활성화 할 수 있다)
     * libusb_get_configuration() 로 현재 활성화된 config index? (default: 1)를 가져올 수 있다.
     * 만약 지정한 config 가 현재와 같으면 "가벼운"동작(USB device 상태 재설정?)으로 끝난다.
     */
    int err = libusb_set_configuration(deviceHandle, bConfigurationValue);
    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_set_configuration error:" << libusb_error_name(err);
        return false;
    }

    return true;
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
bool UsbComm::claimUsbInterface(libusb_device_handle *deviceHandle, int interfaceNumber)
{
    if (!deviceHandleList.contains(deviceHandle)) {
        return false;
    }

    if (libusb_kernel_driver_active(deviceHandle, interfaceNumber) == 1) {
        qDebug() << "Kernel driver active for interface" << interfaceNumber;
        int err = libusb_detach_kernel_driver(deviceHandle, interfaceNumber);
        if (err != LIBUSB_SUCCESS) {
            qDebug() << "libusb_detach_kernel_driver error:" << libusb_error_name(err);
            return false;
        }
    }

    int err = libusb_claim_interface(deviceHandle, interfaceNumber);
    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_claim_interface error:" <<libusb_error_name(err);
        return false;
    }

    if (handleClaimedInterfacesMap.contains(deviceHandle)) {
        QList<int> claimedInterfaceList = handleClaimedInterfacesMap.value(deviceHandle);
        if (!claimedInterfaceList.contains(interfaceNumber)) {
            claimedInterfaceList.append(interfaceNumber);
            handleClaimedInterfacesMap.insert(deviceHandle, claimedInterfaceList);
        }
    } else {
        QList<int> claimedInterfaceList;
        claimedInterfaceList.append(interfaceNumber);
        handleClaimedInterfacesMap.insert(deviceHandle, claimedInterfaceList);
    }

    return true;
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
void UsbComm::releaseUsbInterface(libusb_device_handle *deviceHandle,int interfaceNumber)
{
    if (!deviceHandleList.contains(deviceHandle))
        return;

    if (!handleClaimedInterfacesMap.contains(deviceHandle))
        return;

    QList<int> claimedInterfaceList = handleClaimedInterfacesMap.value(deviceHandle);

    if (interfaceNumber != -1) {
        if (claimedInterfaceList.contains(interfaceNumber)) {
            libusb_release_interface(deviceHandle, interfaceNumber);
            claimedInterfaceList.removeAll(interfaceNumber);
            handleClaimedInterfacesMap.insert(deviceHandle, claimedInterfaceList);
        }
    } else {
        for (int i = 0; i < claimedInterfaceList.size(); i++)
            libusb_release_interface(deviceHandle, claimedInterfaceList.at(i));

        handleClaimedInterfacesMap.remove(deviceHandle);
    }
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
bool UsbComm::setUsbInterfaceAltSetting(libusb_device_handle *deviceHandle, int interfaceNumber, int bAlternateSetting)
{
    if (!deviceHandleList.contains(deviceHandle))
        return false;

    if (!handleClaimedInterfacesMap.contains(deviceHandle) || !handleClaimedInterfacesMap.value(deviceHandle).contains(interfaceNumber))
        return false;

    int err = libusb_set_interface_alt_setting(deviceHandle, interfaceNumber, bAlternateSetting);
    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_set_interface_alt_setting error:" << libusb_error_name(err);
        return false;
    }

    return true;
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
bool UsbComm::resetUsbDevice(libusb_device_handle *deviceHandle)
{
    if (!deviceHandleList.contains(deviceHandle)) {
        return false;
    }

    int err = libusb_reset_device(deviceHandle);
    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_reset_device error:" << libusb_error_name(err);
        if (err == LIBUSB_ERROR_NOT_FOUND) {
            libusb_close(deviceHandle);
            deviceHandleList.removeAll(deviceHandle);
        }
        return false;
    }
    return true;
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
int UsbComm::bulkTransfer(libusb_device_handle *deviceHandle, quint8 endpoint, quint8 *data, int length, quint32 timeout)
{
    if (!deviceHandleList.contains(deviceHandle)) {
        return -100;
    }

    int actual_length = 0;

    /* blocking전송. 전송이 끝나거나 타임아웃될 경우에만 return한다 */
    int err = libusb_bulk_transfer(deviceHandle, endpoint, data, length, &actual_length, timeout);
    if (err == LIBUSB_SUCCESS || err == LIBUSB_ERROR_TIMEOUT) {
        return actual_length;
    } else {
        if (err == LIBUSB_ERROR_PIPE) {
            libusb_clear_halt(deviceHandle, endpoint);
        }
        qDebug() << "libusb_bulk_transfer error:" << libusb_error_name(err);
        return err;
    }
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
libusb_device_handle *UsbComm::getDeviceHandleFromIndex(int index)
{
    if (index >= 0 && index < deviceHandleList.size())
        return deviceHandleList.at(index);

    return NULL;
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
libusb_device_handle *UsbComm::getDeviceHandleFromVpidAndPort(quint16 vid, quint16 pid, qint16 port)
{
    for (int i = 0; i < deviceHandleList.size(); i++) {
        libusb_device *dev = libusb_get_device(deviceHandleList.at(i));
        libusb_device_descriptor deviceDesc;

        int err = libusb_get_device_descriptor(dev, &deviceDesc);
        if (err != LIBUSB_SUCCESS) {
            continue;
        }

        /* 매칭되는 디바이스 찾기 */
        if (deviceDesc.idVendor == vid && deviceDesc.idProduct == pid) {
            if (port == -1) {
                return deviceHandleList.at(i);
            } else {
                if (libusb_get_port_number(dev) == port) {
                    return deviceHandleList.at(i);
                }
            }
        }
    }
    return NULL;
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
void UsbComm::printDevInfo(libusb_device *usbDevice)
{
    /* usb device descriptor 레이어:
     *		device
     *			->configuration
     *				->interface
     *					->altsetting
     *						->endpoint
     */

    /* device */
    libusb_device_descriptor deviceDesc;

    int err = libusb_get_device_descriptor(usbDevice, &deviceDesc);
    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_get_device_descriptor error:" << libusb_error_name(err);
        return;
    }

    // if (!(deviceDesc.idVendor == 0x04b4 && deviceDesc.idProduct == 0x00f1)) {
    //        return;
    // }

    QString vid, pid;

    qDebug() << "********************************************************************************";
    qDebug() << "Bus: "<<(int)libusb_get_bus_number(usbDevice);						/* 현재 bus */
    qDebug() << "Device Address: " <<(int)libusb_get_device_address(usbDevice);				/* bus 에서의 주소 */
    qDebug() << "Device Port: " <<(int)libusb_get_port_number(usbDevice);					/* device end point number */
    qDebug() << "Device Speed: " <<(int)libusb_get_device_speed(usbDevice);					/* device 속도, 자세하게는 enum libusb_speed {} */
    qDebug() << "Device Class: " <<QString("0x%1").arg((int)deviceDesc.bDeviceClass, 2, 16, QChar('0'));	/* device class */

    vid = QString("0x%1").arg((int)deviceDesc.idVendor, 4, 16, QChar('0'));		/* VID */
    pid = QString("0x%1").arg((int)deviceDesc.idProduct, 4, 16, QChar('0'));	/* PID */

    qDebug() << "VendorID = " << vid;		/* VID */
    qDebug() << "ProductID = " << pid;		/* PID */

    /********************************************************************************/
    /* Main Window 로 vid/pid 를 signal 로 보낸다 */
    /********************************************************************************/
    emit sigPutDevInfo2MainUI(vid, pid);

    qDebug() << "Number of configurations: " <<(int)deviceDesc.bNumConfigurations;				/* configuration 개수 */

    /* configuration */
    for (int i = 0; i < (int)deviceDesc.bNumConfigurations; i++) {
        qDebug() << "Configuration index:" << i;

        libusb_config_descriptor *configDesc;
        libusb_get_config_descriptor(usbDevice, i, &configDesc);

        qDebug() << "Configuration Value: " << (int)configDesc->bConfigurationValue;
        qDebug() << "Number of interfaces: " << (int)configDesc->bNumInterfaces;

        /* interface */
        for (int j = 0; j < (int)configDesc->bNumInterfaces; j++) {
            qDebug()<<"\tInterface index:"<<j;
            const libusb_interface *usbInterface;
            usbInterface = &configDesc->interface[j];
            qDebug()<<"\tNumber of alternate settings: "<<usbInterface->num_altsetting;

            /* alt setting */
            for (int k = 0; k < usbInterface->num_altsetting; k++) {
                qDebug() << "\t\tAltsetting index:" << k;
                const libusb_interface_descriptor *interfaceDesc;
                interfaceDesc = &usbInterface->altsetting[k];
                qDebug() << "\t\tInterface Class: " << QString("0x%1").arg((int)interfaceDesc->bInterfaceClass, 2, 16, QChar('0'));	/* interface class */
                qDebug() << "\t\tInterface Number: " << (int)interfaceDesc->bInterfaceNumber;
                qDebug() << "\t\tAlternate settings: " << (int)interfaceDesc->bAlternateSetting;
                qDebug() << "\t\tNumber of endpoints: " << (int)interfaceDesc->bNumEndpoints;

                /* endpoint */
                for (int m = 0; m < (int)interfaceDesc->bNumEndpoints; m++) {
                    qDebug() << "\t\t\tEndpoint index:"<<m;
                    const libusb_endpoint_descriptor *endpointDesc;
                    endpointDesc = &interfaceDesc->endpoint[m];
                    qDebug() << "\t\t\tEP address: " << QString("0x%1").arg((int)endpointDesc->bEndpointAddress, 2, 16, QChar('0'));

                    /* device 의 transfer type, 자세히는 enum libusb_transfer_type { } */
                    qDebug() << "\t\t\tEP transfer type:" << (endpointDesc->bmAttributes&0x03);
                }
            }
        }

        libusb_free_config_descriptor(configDesc);
    }
    qDebug() << "********************************************************************************";
}




/********************************************************************************/
/* Part2: UsbMonitor */
/********************************************************************************/

/********************************************************************************/
/*
 *@brief: 생성자
 *@param:
 *@return:
 */
/********************************************************************************/
UsbMonitor::UsbMonitor(QObject *parent) :QObject(parent)
{
    /* 맴버변수 초기화 */
    context = NULL;
    hotplugHandle = -1;
    hotplugEventHandler = NULL;

    /* libusb 초기화 */
    int err = libusb_init(&context);
    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_init error:" << libusb_error_name(err);
    }

    /* log level 설정 */
    libusb_set_debug(context, LIBUSB_LOG_LEVEL_WARNING);	//old
    //libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL,LIBUSB_LOG_LEVEL_WARNING);	//new
}

/********************************************************************************/
/* 소멸자 */
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
UsbMonitor::~UsbMonitor()
{
    /* hot plug 서비스 등록 해제 */
    deregisterHotplugMonitorService();

    /* libusb 종료 */
    libusb_exit(context);
}


/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
bool UsbMonitor::registerHotplugMonitorService(int deviceClass, int vendorId, int productId)
{
    /* 현재 사용하고 있는 libusb 가 hot plug 감지를 지원하는지 체크 */
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        qDebug()<<"hotplug capabilites are not supported on this platform";
        return false;
    }

    /* hot plug callback 함수 등록 */
    int err = libusb_hotplug_register_callback(context,
                                               (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
                                               LIBUSB_HOTPLUG_NO_FLAGS,
                                               vendorId,
                                               productId,
                                               deviceClass,
                                               hotplugCallback,
                                               (void *)this,
                                               &hotplugHandle);

    if (err != LIBUSB_SUCCESS) {
        qDebug() << "libusb_hotplug_register_callback error:" << libusb_error_name(err);
        return false;
    }

    /* callback 함수는 event polling 후, 트리거 될 것이다 */
    if (hotplugEventHandler == NULL) {
        hotplugEventHandler = new UsbEventHandler(context,this);
    }

    hotplugEventHandler->setStopped(false);
    hotplugEventHandler->start();

    return true;
}

/********************************************************************************/
/*
 *@brief:
 *@param:
 *@return:
 */
/********************************************************************************/
void UsbMonitor::deregisterHotplugMonitorService()
{
    /*  */
    if (hotplugHandle != -1) {
        libusb_hotplug_deregister_callback(context, hotplugHandle);
        hotplugHandle = -1;
    }

    /* hot plug 이벤트 처리 쓰레드를 종료시킨다*/
    if (hotplugEventHandler != NULL) {
        hotplugEventHandler->setStopped(true);
        /* 쓰레드 종료를 기다린다 */
        hotplugEventHandler->wait();
    }
}

/********************************************************************************/
/*
 * @brief: 핫 플러그 콜백 함수(UsbEventHandler 하위 스레드에서 실행)
 *
 * NOTE 1: 반드시 정적 멤버 함수 또는 전역 함수여야 한다.
 * 			그렇지 않으면 this 포인터로 인해 콜백 등록 문이 컴파일되지 않는다.
 * 			따라서 콜백 함수는 인스턴스 객체를 직접 사용할 수 없지만, 매개 변수 user_data를 통해 인스턴스 객체의 메서드와 데이터에 액세스할 수 있다.
 *
 * NOTE 2: 이 함수 내에서 user_data를 사용하여 인스턴스 객체의 신호를 발생시킨다.
 * 			신호는 하위 스레드에서 발생하므로 연결된 슬롯은 주로 main 스레드에 있다.
 * 			connect는 기본적으로 큐 연결을 사용하므로 이 함수가 최소한의 처리만 수행하고 불필요한 작업을 하지 않도록 보장한다.
 *
 *@return: int: 반환 값이 1 이면 등록 취소(deregistered)
 */
/********************************************************************************/
int UsbMonitor::hotplugCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
    Q_UNUSED(ctx)
    Q_UNUSED(device)

    /* 강제로 hot plug 감시하는 object 로 캐스팅  */
    UsbMonitor *tmpUsbMonitor = (UsbMonitor*)user_data;

    /* usb 삽입 */
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        emit tmpUsbMonitor->deviceHotplugSig(true);
    } else {
        /* usb 제거 */
        emit tmpUsbMonitor->deviceHotplugSig(false);
    }

    return 0;
}

/********************************************************************************/
/*
 *@brief:	생성자함수
 *@param:	context: libusb 회화 세션
 *@parent:	parent:
 */
/********************************************************************************/
UsbEventHandler::UsbEventHandler(libusb_context *context, QObject *parent) :QThread(parent)
{
    this->context = context;
    this->stopped = false;
}

/********************************************************************************/
/*
 *@brief: Sub thread
 *@param:
 *@return:
 */
/********************************************************************************/
void UsbEventHandler::run()
{
    /* timeout: 100 ms */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    while (!this->stopped && context != NULL) {
        //qDebug()<<"libusb_handle_events().......";

        /* pending중인 이벤트를 처리한다. blocking되지 않고 timeout되면 즉시 return한다.
         * 처음에는 libusb_handle_events()로 블로킹 방식으로 작업을 수행했지만, 블로킹으로 인해 Thread가 제대로 종료되지 않았다.
         * terminate()를 호출하여 강제로 종료한 후에 wait 작업을 실행하면 정지...
         *
         * 블로킹 작업이 kernel모드로 빠져서 user모드에서 thread를 강제로 종료할 수 없게 되는 것으로 의심.
         *
         * 참고:
         * 		pending중인 핫플러그 이벤트가 있으면 등록된 콜백 함수가 이 thread 내에서 호출된다.
         */
        libusb_handle_events_timeout_completed(context, &tv, NULL);
    }
}
