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
/* 생성자 함수，libusb에 대해 초기화작업을 실시한다. */
/********************************************************************************/
UsbComm::UsbComm(QObject *parent)
    : QObject(parent)
{
    context = NULL;

    /* libusb 초기화 */
    int err = libusb_init(&context);
    if (err != LIBUSB_SUCCESS) {
        qDebug()<<"libusb_init error:"<<libusb_error_name(err);
    }

    /* log level 설정 */
    libusb_set_debug(context, LIBUSB_LOG_LEVEL_WARNING); //old ver
    //libusb_set_option(context,LIBUSB_OPTION_LOG_LEVEL,LIBUSB_LOG_LEVEL_WARNING);	//new ver
}

/********************************************************************************/
/* 소멸자 함수，libusb에 대해 De-init작업을 실시한다. */
/********************************************************************************/
UsbComm::~UsbComm()
{
    closeAllUsbDevice();
    libusb_exit(context);
}

/*
 * 현재 접속된 USB device 를 탐색하여, device 정보를 출력
 */
void UsbComm::findUsbDevices()
{
    libusb_device **devs;

    /* get device list */
    ssize_t count = libusb_get_device_list(context,&devs);

    for (int i=0;i<count;i++)
        printDevInfo(devs[i]);

    /* */
    libusb_free_device_list(devs,1);
}

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
bool UsbComm::openUsbDevice(QMultiMap<quint16, quint16> &vpidMap)
{
    if (vpidMap.isEmpty()) {
        qDebug()<<"vpidMap is empty";
        return false;
    }

    /* 먼저 모든 이미 열린 device 를 닫는다 */
    closeAllUsbDevice();

    libusb_device **devs;

    /* get usb devices list */
    ssize_t count = libusb_get_device_list(context,&devs);
    if (count < 0) {
        qDebug()<<"libusb_get_device_list is error";
        return false;
    }

    for (int i=0; i < count; i++) {
        /* device */
        libusb_device_descriptor deviceDesc;

        int err = libusb_get_device_descriptor(devs[i], &deviceDesc);
        if (err != LIBUSB_SUCCESS) {
            qDebug()<<"libusb_get_device_descriptor error:"<<libusb_error_name(err);
            continue;
        }

        /* vid, pid 가 매칭되는 장비를 찾는다 */
        if (vpidMap.uniqueKeys().contains(deviceDesc.idVendor) &&
            vpidMap.values(deviceDesc.idVendor).contains(deviceDesc.idProduct)) {
            libusb_device_handle *deviceHandle = NULL;
            int err = libusb_open(devs[i], &deviceHandle);
            if (err != LIBUSB_SUCCESS) {
                qDebug()<<"libusb_open error:"<<libusb_error_name(err);
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
/* 지정 usb device 닫기 */
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
/* 모든 device 닫기 */
/********************************************************************************/
void UsbComm::closeAllUsbDevice()
{
    for (int i=0; i < deviceHandleList.size(); i++)
        closeUsbDevice(deviceHandleList.at(i));
}

/*
 *usb device 의 현재 config 를 활성화한다 (config가 1개인 usb device 는 default로 활성화되므로 호출안해도 된다)
 *@param:   deviceHandle: device handle
 *@param:   bConfigurationValue: config num
 *@return:  bool:true=OK  false=NG
 */
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
    int err = libusb_set_configuration(deviceHandle,bConfigurationValue);
    if (err != LIBUSB_SUCCESS) {
        qDebug()<<"libusb_set_configuration error:"<<libusb_error_name(err);
        return false;
    }
    return true;
}

/*
 *@brief:   声明usb设备接口
 * 在操作I/O或其他端点的时候必须先声明接口，接口声明用于告知底层操作系统你的程序想要取得此接口的所有权。
 *@param:   deviceHandle:设备句柄
 *@param:   interfaceNumber:接口号
 *@return:   bool:true=成功  false=失败
 */
bool UsbComm::claimUsbInterface(libusb_device_handle *deviceHandle, int interfaceNumber)
{
    if(!deviceHandleList.contains(deviceHandle))
    {
        return false;
    }
    //确保指定接口的内核驱动程序未激活，否则将无法声明该接口
    if(libusb_kernel_driver_active(deviceHandle, interfaceNumber) == 1)
    {
        qDebug()<<"Kernel driver active for interface"<<interfaceNumber;
        //卸载指定接口的内核驱动
        int err = libusb_detach_kernel_driver(deviceHandle,interfaceNumber);
        if(err != LIBUSB_SUCCESS)
        {
            qDebug()<<"libusb_detach_kernel_driver error:"<<libusb_error_name(err);
            return false;
        }
    }
    //声明接口(该接口是一个单纯的逻辑操作,不会通过总线发送任何请求)
    int err = libusb_claim_interface(deviceHandle, interfaceNumber);
    if(err != LIBUSB_SUCCESS)
    {
        qDebug()<<"libusb_claim_interface error:"<<libusb_error_name(err);
        return false;
    }

    //将成功声明的接口号添加到列表，方便在退出时释放所有声明的接口
    if(handleClaimedInterfacesMap.contains(deviceHandle))
    {
        QList<int> claimedInterfaceList = handleClaimedInterfacesMap.value(deviceHandle);
        if(!claimedInterfaceList.contains(interfaceNumber))
        {
            claimedInterfaceList.append(interfaceNumber);
            handleClaimedInterfacesMap.insert(deviceHandle,claimedInterfaceList);
        }
    }
    else
    {
        QList<int> claimedInterfaceList;
        claimedInterfaceList.append(interfaceNumber);
        handleClaimedInterfacesMap.insert(deviceHandle,claimedInterfaceList);
    }

    return true;
}
/*
 *@brief:   释放usb设备声明的接口
 *@param:   deviceHandle:设备句柄
 *@param:   interfaceNumber:要释放的接口号，-1表示释放当前所有生命过的接口
 */
void UsbComm::releaseUsbInterface(libusb_device_handle *deviceHandle,int interfaceNumber)
{
    //设备句柄不存在
    if(!deviceHandleList.contains(deviceHandle))
    {
        return;
    }
    //设备当前未声明任何接口
    if(!handleClaimedInterfacesMap.contains(deviceHandle))
    {
        return;
    }

    QList<int> claimedInterfaceList = handleClaimedInterfacesMap.value(deviceHandle);
    if(interfaceNumber != -1)
    {
        if(claimedInterfaceList.contains(interfaceNumber))
        {
            libusb_release_interface(deviceHandle,interfaceNumber);
            claimedInterfaceList.removeAll(interfaceNumber);
            handleClaimedInterfacesMap.insert(deviceHandle,claimedInterfaceList);
        }
    }
    else
    {
        for(int i=0;i<claimedInterfaceList.size();i++)
        {
            libusb_release_interface(deviceHandle,claimedInterfaceList.at(i));
        }
        handleClaimedInterfacesMap.remove(deviceHandle);
    }
}
/*
 *@brief:   激活usb设备接口备用设置(通常对于只有一个备用设置的接口，默认已经激活，无需调用)
 * 该函数调用之前需要先声明接口。
 *@param:   deviceHandle:设备句柄
 *@param:   interfaceNumber:接口号
 *@param:   bAlternateSetting:备用设置
 *@return:   bool:true=成功  false=失败
 */
bool UsbComm::setUsbInterfaceAltSetting(libusb_device_handle *deviceHandle, int interfaceNumber, int bAlternateSetting)
{
    //设备句柄不存在
    if(!deviceHandleList.contains(deviceHandle))
    {
        return false;
    }
    //设备接口未声明
    if(!handleClaimedInterfacesMap.contains(deviceHandle) ||
        !handleClaimedInterfacesMap.value(deviceHandle).contains(interfaceNumber))
    {
        return false;
    }
    //激活接口的备用设置(该函数是阻塞的)
    int err = libusb_set_interface_alt_setting(deviceHandle, interfaceNumber,bAlternateSetting);
    if(err != LIBUSB_SUCCESS)
    {
        qDebug()<<"libusb_set_interface_alt_setting error:"<<libusb_error_name(err);
        return false;
    }

    return true;
}
/*
 *@brief:   重置usb设备
 * 重新初始化设备，重置完成后，系统将尝试恢复之前的配置和备用设置。
 * 如果该函数返回false，则表明重置可能失败，外部需要重新调用查询方法获取设备句柄，因为有可能句柄已经被关闭了
 * 需要重新打开设备遍历寻找。
 *@param:   deviceHandle:设备句柄
 *@return:   bool:true=成功  false=失败
 */
bool UsbComm::resetUsbDevice(libusb_device_handle *deviceHandle)
{
    //设备句柄不存在
    if(!deviceHandleList.contains(deviceHandle))
    {
        return false;
    }
    //重置设备
    int err = libusb_reset_device(deviceHandle);
    if(err != LIBUSB_SUCCESS)
    {
        qDebug()<<"libusb_reset_device error:"<<libusb_error_name(err);
        if(err == LIBUSB_ERROR_NOT_FOUND)//句柄已经无效
        {
            libusb_close(deviceHandle);
            deviceHandleList.removeAll(deviceHandle);
        }
        return false;
    }

    return true;
}
/*
 *@brief:   (批量(块)传输)
 *@param:   deviceHandle:设备句柄
 *@param:   endpoint:端点,bit0:3表示端点地址，bit4:6为保留位，bit7表示方向(1=In: device-to-host  0=Out:host-to-device)
 *@param:   data:输入/输出数据buffer指针，内存空间要在外部申请好
 *@param:   length:写，data长度；读，data可接收的最大长度
 *@param:   timeout:超时时间，单位ms， 0 无限制
 *@return:   int:真实传输的字节数  小于0表示出错
 */
int UsbComm::bulkTransfer(libusb_device_handle *deviceHandle, quint8 endpoint,
                          quint8 *data, int length, quint32 timeout)
{
    if(!deviceHandleList.contains(deviceHandle))
    {
        return -100;
    }
    int actual_length=0;
    //该函数是阻塞的，只有数据传输完成或者超时才会返回
    int err = libusb_bulk_transfer(deviceHandle,endpoint,
                                   data,length,&actual_length,timeout);
    if(err == LIBUSB_SUCCESS || err == LIBUSB_ERROR_TIMEOUT)
    {
        return actual_length;
    }
    else
    {
        if(err == LIBUSB_ERROR_PIPE)
        {
            libusb_clear_halt(deviceHandle,endpoint);
        }
        qDebug()<<"libusb_bulk_transfer error:"<<libusb_error_name(err);
        return err;
    }
}
/*
 *@brief:   通过索引获取打开的设备句柄
 *@param:   index:索引号
 *@return:   libusb_device_handle:设备句柄
 */
libusb_device_handle *UsbComm::getDeviceHandleFromIndex(int index)
{
    if(index >=0 && index < deviceHandleList.size())
    {
        return deviceHandleList.at(index);
    }
    return NULL;
}
/*
 *@brief:   通过vpid和端口号获取打开的设备句柄
 *@param:   vid:厂商id
 *@param:   pid:产品id
 *@param:   port:端口号(通常是与硬件接口绑定的，可通过dmesg查看)，-1表示不匹配端口
 *@return:   libusb_device_handle:设备句柄
 */
libusb_device_handle *UsbComm::getDeviceHandleFromVpidAndPort(quint16 vid, quint16 pid, qint16 port)
{
    for(int i=0;i<deviceHandleList.size();i++)
    {
        libusb_device *dev = libusb_get_device(deviceHandleList.at(i));
        libusb_device_descriptor deviceDesc;
        int err = libusb_get_device_descriptor(dev, &deviceDesc);
        if(err != LIBUSB_SUCCESS)
        {
            continue;
        }
        //查找匹配的设备
        if(deviceDesc.idVendor == vid && deviceDesc.idProduct == pid)
        {
            //不匹配端口
            if(port == -1)
            {
                return deviceHandleList.at(i);
            }
            else
            {
                if(libusb_get_port_number(dev) == port)
                {
                    return deviceHandleList.at(i);
                }
            }
        }
    }
    return NULL;
}
/*
 *@brief:   打印USB设备详细信息
 *@param:   usbDevice:对应一个usb设备
 */
void UsbComm::printDevInfo(libusb_device *usbDevice)
{
    /*设备描述符层级:
     *设备(device)->配置(configuration)->接口(interface)->备用设置(altsetting)->端点(endpoint)*/

    /*设备(device)*/
    libusb_device_descriptor deviceDesc;
    int err = libusb_get_device_descriptor(usbDevice, &deviceDesc);
    if(err != LIBUSB_SUCCESS)
    {
        qDebug()<<"libusb_get_device_descriptor error:"<<libusb_error_name(err);
        return;
    }
    //    if(!(deviceDesc.idVendor == 0x04b4 && deviceDesc.idProduct == 0x00f1))
    //    {
    //        return;
    //    }
    qDebug()<<"***************************************";
    qDebug()<<"Bus: "<<(int)libusb_get_bus_number(usbDevice);//设备所在总线
    qDebug()<<"Device Address: "<<(int)libusb_get_device_address(usbDevice);//设备在总线上的地址
    qDebug()<<"Device Port: "<<(int)libusb_get_port_number(usbDevice);//设备端口号
    qDebug()<<"Device Speed: "<<(int)libusb_get_device_speed(usbDevice);//设备连接速度，详见enum libusb_speed{}
    qDebug()<<"Device Class: "<<QString("0x%1").arg((int)deviceDesc.bDeviceClass,2,16,QChar('0'));//设备类
    qDebug()<<"VendorID: "<<QString("0x%1").arg((int)deviceDesc.idVendor,4,16,QChar('0'));//设备厂商id
    qDebug()<<"ProductID: "<<QString("0x%1").arg((int)deviceDesc.idProduct,4,16,QChar('0'));//设备产品id
    qDebug()<<"Number of configurations: "<<(int)deviceDesc.bNumConfigurations;//设备的配置数
    /*配置(configuration)*/
    for(int i=0;i<(int)deviceDesc.bNumConfigurations;i++)
    {
        qDebug()<<"Configuration index:"<<i;
        libusb_config_descriptor *configDesc;
        libusb_get_config_descriptor(usbDevice,i,&configDesc);
        qDebug()<<"Configuration Value: "<<(int)configDesc->bConfigurationValue;
        qDebug()<<"Number of interfaces: "<<(int)configDesc->bNumInterfaces;
        /*接口(interface)*/
        for(int j=0; j<(int)configDesc->bNumInterfaces;j++)
        {
            qDebug()<<"\tInterface index:"<<j;
            const libusb_interface *usbInterface;
            usbInterface = &configDesc->interface[j];
            qDebug()<<"\tNumber of alternate settings: "<<usbInterface->num_altsetting;
            /*备用设置(altsetting)*/
            for(int k=0; k<usbInterface->num_altsetting; k++)
            {
                qDebug()<<"\t\tAltsetting index:"<<k;
                const libusb_interface_descriptor *interfaceDesc;
                interfaceDesc = &usbInterface->altsetting[k];
                qDebug()<<"\t\tInterface Class: "<<
                    QString("0x%1").arg((int)interfaceDesc->bInterfaceClass,2,16,QChar('0'));//接口类
                qDebug()<<"\t\tInterface Number: "<<(int)interfaceDesc->bInterfaceNumber;
                qDebug()<<"\t\tAlternate settings: "<<(int)interfaceDesc->bAlternateSetting;
                qDebug()<<"\t\tNumber of endpoints: "<<(int)interfaceDesc->bNumEndpoints;
                /*端点(endpoint)*/
                for(int m=0; m<(int)interfaceDesc->bNumEndpoints; m++)
                {
                    qDebug()<<"\t\t\tEndpoint index:"<<m;
                    const libusb_endpoint_descriptor *endpointDesc;
                    endpointDesc = &interfaceDesc->endpoint[m];
                    qDebug()<<"\t\t\tEP address: "<<
                        QString("0x%1").arg((int)endpointDesc->bEndpointAddress,2,16,QChar('0'));
                    qDebug()<<"\t\t\tEP transfer type:"<<
                        (endpointDesc->bmAttributes&0x03);//端点传输类型，详见enum libusb_transfer_type{}
                }
            }
        }
        libusb_free_config_descriptor(configDesc);//释放配置描述符空间
    }
    qDebug()<<"***************************************";
}




/********************************************************************************/
/* Part2: UsbMonitor */
/********************************************************************************/
/*
 * 생성자
 */
UsbMonitor::UsbMonitor(QObject *parent) :QObject(parent)
{
    /* 맴버변수 초기화 */
    context = NULL;
    hotplugHandle = -1;
    hotplugEventHandler = NULL;

    /* libusb 초기화 */
    int err = libusb_init(&context);
    if (err != LIBUSB_SUCCESS) {
        qDebug()<<"libusb_init error:"<<libusb_error_name(err);
    }

    /* log level 설정 */
    libusb_set_debug(context, LIBUSB_LOG_LEVEL_WARNING);	//old
    //libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL,LIBUSB_LOG_LEVEL_WARNING);	//new
}

/*
 * 소멸자
 */
UsbMonitor::~UsbMonitor()
{
    /* hot plug 서비스 등록 해제 */
    deregisterHotplugMonitorService();

    /* libusb 종료 */
    libusb_exit(context);
}


/*
 *@brief:   注册热插拔监测服务
 *@param:   deviceClass:监测的设备类
 *@param:   vendorId:监测的设备厂商id
 *@param:   productId:监测的设备产品id
 *@return:   bool:true=注册成功  false=注册失败
 */
bool UsbMonitor::registerHotplugMonitorService(int deviceClass, int vendorId, int productId)
{
    //先判断当前平台的libusb库是否支持热插拔监测
    if(!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
    {
        qDebug()<<"hotplug capabilites are not supported on this platform";
        return false;
    }
    //注册热插拔的回调函数
    int err = libusb_hotplug_register_callback(
        context, (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED|LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
        LIBUSB_HOTPLUG_NO_FLAGS, vendorId,productId, deviceClass,hotplugCallback,(void *)this, &hotplugHandle);
    if(err != LIBUSB_SUCCESS)
    {
        qDebug()<<"libusb_hotplug_register_callback error:"<<libusb_error_name(err);
        return false;
    }
    //回调函数需要经过轮询事件处理才可以被触发执行
    if(hotplugEventHandler == NULL)
    {
        hotplugEventHandler = new UsbEventHandler(context,this);
    }
    hotplugEventHandler->setStopped(false);
    hotplugEventHandler->start();

    return true;
}

/*
 *@brief:   注销热插拔监测服务
 */
void UsbMonitor::deregisterHotplugMonitorService()
{
    if(hotplugHandle != -1)//注销回调
    {
        libusb_hotplug_deregister_callback(context,hotplugHandle);
        hotplugHandle = -1;
    }
    if(hotplugEventHandler != NULL)//停止热插拔事件处理线程
    {
        hotplugEventHandler->setStopped(true);
        hotplugEventHandler->wait();//等待线程结束
    }
}

/*
 *@brief:   热插拔回调函数(在UsbEventHandler子线程中执行)
 * 注1:必须是静态成员函数或全局函数，否则会因为隐含的this指针导致注册回调语句编译不通过，
 * 也因此回调函数无法直接使用实例对象，但可以通过函参user_data访问实例对象的方法与数据。
 * 注2:该函数内使用user_data发射实例对象的信号，因为信号依附于子线程发射，而槽一般在主线
 * 程，connect默认采用队列连接，确保了该函数只做最小处理，绝不拖泥带水。
 *@param:   ctx:表示libusb的一个会话
 *@param:   device:热插拔的设备
 *@param:   event:热插拔的事件
 *@param:   user_data:用户数据指针，在调用libusb_hotplug_register_callback()传递，
 *                  我们这里传的是this指针,实现在静态成员函数访问实例对象的方法与数据。
 *@return:   int:当返回值为1时，则会撤销注册(deregistered)
 */
int UsbMonitor::hotplugCallback(libusb_context *ctx, libusb_device *device,
                                libusb_hotplug_event event, void *user_data)
{
    Q_UNUSED(ctx)
    Q_UNUSED(device)

    /* 강제로 hot plug 감시하는 object 로 캐스팅  */
    UsbMonitor *tmpUsbMonitor = (UsbMonitor*)user_data;

    /* usb 삽입 */
    if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        emit tmpUsbMonitor->deviceHotplugSig(true);
    } else {
        /* usb 제거 */
        emit tmpUsbMonitor->deviceHotplugSig(false);
    }

    return 0;
}

/*
 *@brief:	생성자함수
 *@param:	context: libusb 회화 세션
 *@parent:	parent:
 */
UsbEventHandler::UsbEventHandler(libusb_context *context, QObject *parent)
    :QThread(parent)
{
    this->context = context;
    this->stopped = false;
}

/*
 * Sub thread
 */
void UsbEventHandler::run()
{
    /* timeout: 100 ms */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    while(!this->stopped && context != NULL) {
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
