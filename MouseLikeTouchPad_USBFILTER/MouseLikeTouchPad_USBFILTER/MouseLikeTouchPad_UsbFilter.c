#include "MouseLikeTouchPad_UsbFilter.h"
#include<math.h>
//extern "C" int _fltused = 0;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, FilterEvtDeviceAdd)
#endif


VOID RegDebug(WCHAR* strValueName, PVOID dataValue, ULONG datasizeValue)//RegDebug(L"Run debug here",pBuffer,pBufferSize);//RegDebug(L"Run debug here",NULL,0x12345678);
{
    //初始化注册表项
    UNICODE_STRING stringKey;
    RtlInitUnicodeString(&stringKey, L"\\Registry\\Machine\\Software\\RegDebug");

    //初始化OBJECT_ATTRIBUTES结构
    OBJECT_ATTRIBUTES  ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes, &stringKey, OBJ_CASE_INSENSITIVE, NULL, NULL);

    //创建注册表项
    HANDLE hKey;
    ULONG Des;
    NTSTATUS status = ZwCreateKey(&hKey, KEY_ALL_ACCESS, &ObjectAttributes, 0, NULL, REG_OPTION_NON_VOLATILE, &Des);
    if (NT_SUCCESS(status))
    {
        if (Des == REG_CREATED_NEW_KEY)
        {
            KdPrint(("新建注册表项！\n"));
        }
        else
        {
            KdPrint(("要创建的注册表项已经存在！\n"));
        }
    }
    else {
        return;
    }

    //初始化valueName
    UNICODE_STRING valueName;
    RtlInitUnicodeString(&valueName, strValueName);

    if (dataValue == NULL) {
        //设置REG_DWORD键值
        status = ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &datasizeValue, 4);
        if (!NT_SUCCESS(status))
        {
            KdPrint(("设置REG_DWORD键值失败！\n"));
        }
    }
    else {
        //设置REG_BINARY键值
        status = ZwSetValueKey(hKey, &valueName, 0, REG_BINARY, dataValue, datasizeValue);
        if (!NT_SUCCESS(status))
        {
            KdPrint(("设置REG_BINARY键值失败！\n"));
        }
    }
    ZwFlushKey(hKey);
    //关闭注册表句柄
    ZwClose(hKey);
}


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG   config;
    NTSTATUS            status;
    WDFDRIVER           hDriver;

    //
    // Initiialize driver config to control the attributes that
    // are global to the driver. Note that framework by default
    // provides a driver unload routine. If you create any resources
    // in the DriverEntry and want to be cleaned in driver unload,
    // you can override that by manually setting the EvtDriverUnload in the
    // config structure. In general xxx_CONFIG_INIT macros are provided to
    // initialize most commonly used members.
    //

    WDF_DRIVER_CONFIG_INIT(
        &config,
        FilterEvtDeviceAdd
    );

    //
    // Create a framework driver object to represent our driver.
    //
    status = WdfDriverCreate(DriverObject,
                            RegistryPath,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &config,
                            &hDriver);
    if (!NT_SUCCESS(status)) {
        KdPrint( ("WdfDriverCreate failed with status 0x%x\n", status));
    }

    return status;
}


NTSTATUS
FilterEvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    NTSTATUS                status;
    WDFDEVICE               device;
    WDF_IO_QUEUE_CONFIG     ioQueueConfig;

    PAGED_CODE ();

    UNREFERENCED_PARAMETER(Driver);

    //
    // Tell the framework that you are filter driver. Framework
    // takes care of inherting all the device flags & characterstics
    // from the lower device you are attaching to.
    //
    WdfFdoInitSetFilter(DeviceInit);
    
    //
    // Specify the size of device extension where we track per device
    // context.
    //

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, FILTER_EXTENSION);

    //
    // Create a framework device object.This call will inturn create
    // a WDM deviceobject, attach to the lower stack and set the
    // appropriate flags and attributes.
    //
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint( ("WdfDeviceCreate failed with status code 0x%x\n", status));
        return status;
    }
    
    //
    // Configure the default queue to be Parallel.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    ioQueueConfig.EvtIoInternalDeviceControl = FilterEvtIoDeviceControl;

    status = WdfIoQueueCreate(device,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            WDF_NO_HANDLE // pointer to default queue
                            );
    if (!NT_SUCCESS(status)) {
        KdPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }
    
    runtimes_ioctl = 0;
    runtimes_ioread = 0;
    runtimes_FilterRequestCompletionRoutine = 0;
    nCommonDescOffset = 0;
    bDispatchDescComplete = FALSE;
    pUsbDeviceDescriptor = NULL;
    pConfigDesc = NULL;
    pHidDesc = NULL;

    pReportDesciptorData = NULL;
    ReportDescriptorLength = 0;
    pCertReport = NULL;
    pUsbStrData = NULL;

    RtlZeroMemory(&tp_settings, sizeof(PTP_PARSER));

    return status;
}

VOID
FilterEvtIoDeviceControl(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

   VOID

--*/
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);
    WDFDEVICE                       device;

    device = WdfIoQueueGetDevice(Queue);
    //if (runtimes_ioControl == 1) {
//    RegDebug(L"IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST IoControlCode", NULL, IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST);//0xb002b
//    RegDebug(L"IOCTL_HID_GET_DEVICE_DESCRIPTOR IoControlCode", NULL, IOCTL_HID_GET_DEVICE_DESCRIPTOR);//0xb0003
//    RegDebug(L"IOCTL_HID_GET_REPORT_DESCRIPTOR IoControlCode", NULL, IOCTL_HID_GET_REPORT_DESCRIPTOR);//0xb0007
//    RegDebug(L"IOCTL_HID_GET_DEVICE_ATTRIBUTES IoControlCode", NULL, IOCTL_HID_GET_DEVICE_ATTRIBUTES);//0xb0027
//    RegDebug(L"IOCTL_HID_READ_REPORT IoControlCode", NULL, IOCTL_HID_READ_REPORT);//0xb000b
//    RegDebug(L"IOCTL_HID_WRITE_REPORT IoControlCode", NULL, IOCTL_HID_WRITE_REPORT);//0xb000f
//    RegDebug(L"IOCTL_HID_GET_STRING IoControlCode", NULL, IOCTL_HID_GET_STRING);//0xb0013
//    RegDebug(L"IOCTL_HID_GET_FEATURE IoControlCode", NULL, IOCTL_HID_GET_FEATURE);//0xb0192
//    RegDebug(L"IOCTL_HID_SET_FEATURE IoControlCode", NULL, IOCTL_HID_SET_FEATURE);//0xb0191
//    RegDebug(L"IOCTL_HID_GET_INPUT_REPORT IoControlCode", NULL, IOCTL_HID_GET_INPUT_REPORT);//0xb01a2
//    RegDebug(L"IOCTL_HID_SET_OUTPUT_REPORT IoControlCode", NULL, IOCTL_HID_SET_OUTPUT_REPORT);//0xb0195
//    RegDebug(L"IOCTL_HID_DEVICERESET_NOTIFICATION IoControlCode", NULL, IOCTL_HID_DEVICERESET_NOTIFICATION);//0xb0233
//}

    runtimes_ioctl++;
 
    //
    // Set up post processing of the IOCTL IRP.
    //

    PURB pUrb;
    PIRP pIrp;
    PIO_STACK_LOCATION pStack;

    pIrp = WdfRequestWdmGetIrp(Request);
    pStack = IoGetCurrentIrpStackLocation(pIrp);

    if (IOCTL_INTERNAL_USB_SUBMIT_URB == IoControlCode) {
        pUrb = (PURB)pStack->Parameters.Others.Argument1;
        if (pUrb != NULL) {
            switch (pUrb->UrbHeader.Function) {
            case URB_FUNCTION_SELECT_CONFIGURATION:
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_SELECT_CONFIGURATION", NULL, runtimes_ioctl);
                FilterForwardRequestWithCompletionRoutine(Request, WdfDeviceGetIoTarget(device));
                return;
                break;

            case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE", NULL, runtimes_ioctl);
                FilterForwardRequestWithCompletionRoutine(Request, WdfDeviceGetIoTarget(device));
                return;
                break;

            case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
                //RegDebug(L"FilterEvtIoDeviceControl URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE", NULL, runtimes_ioctl);
                FilterForwardRequestWithCompletionRoutine(Request, WdfDeviceGetIoTarget(device));
                return;
                break;

            case URB_FUNCTION_CLASS_INTERFACE:
                //RegDebug(L"FilterEvtIoDeviceControl URB_FUNCTION_CLASS_INTERFACE", NULL, runtimes_ioctl);
                FilterForwardRequestWithCompletionRoutine(Request, WdfDeviceGetIoTarget(device));
                return;
                break;

            case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
                runtimes_ioread++;
                //RegDebug(L"URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER runtimes_ioread", NULL, runtimes_ioread);

                //PUCHAR pBuff = (PUCHAR)pUrb->UrbBulkOrInterruptTransfer.TransferBuffer;
                //UINT16 bufLen = (UINT16)pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;

                FilterForwardRequestWithCompletionRoutine(Request, WdfDeviceGetIoTarget(device));
                return;
                break;

            case URB_FUNCTION_ABORT_PIPE:
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_ABORT_PIPE", NULL, runtimes_ioctl);
                //FilterForwardRequestWithCompletionRoutine(Request, WdfDeviceGetIoTarget(device));
                //return;
                break;    
            default:
                    //RegDebug(L"pUrb->UrbHeader.Function unknown", NULL, pUrb->UrbHeader.Function);
                break;
            }
        }

    }
    

    ////测试代码
    //switch (IoControlCode)
    //{
    //    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:RegDebug(L"IOCTL_HID_GET_DEVICE_DESCRIPTOR", NULL, IoControlCode);break;
    //    case IOCTL_HID_GET_REPORT_DESCRIPTOR:RegDebug(L"IOCTL_HID_GET_REPORT_DESCRIPTOR", NULL, IoControlCode); break;
    //    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:RegDebug(L"IOCTL_HID_GET_DEVICE_ATTRIBUTES", NULL, IoControlCode); break;

    //    case IOCTL_HID_SET_FEATURE:
    //        RegDebug(L"IOCTL_HID_SET_FEATURE", NULL, IoControlCode);
    //        break;
    //    case IOCTL_HID_GET_FEATURE:
    //        RegDebug(L"IOCTL_HID_GET_FEATURE", NULL, IoControlCode);
    //        break;
    //    case IOCTL_HID_READ_REPORT:
    //        RegDebug(L"IOCTL_HID_READ_REPORT", NULL, IoControlCode);
    //        break;
    //    case IOCTL_HID_GET_STRING:
    //        RegDebug(L"IOCTL_HID_GET_STRING", NULL, IoControlCode);
    //        break;

    //    case IOCTL_INTERNAL_USB_SUBMIT_URB://IOCTL_BTHHFP_GET_DEVICEOBJECT/IOCTL_INTERNAL_USB_SUBMIT_URB(0x00220003)

    //    default://RegDebug(L"STATUS_NOT_SUPPORTED", NULL, FUNCTION_FROM_CTL_CODE(IoControlCode));
    //        break;
    //}


    Filter_DispatchPassThrough(Request, WdfDeviceGetIoTarget(device));

}

VOID
FilterForwardRequestWithCompletionRoutine(
    IN WDFREQUEST Request,
    IN WDFIOTARGET Target
    )
/*++
Routine Description:

    This routine forwards the request to a lower driver with
    a completion so that when the request is completed by the
    lower driver, it can regain control of the request and look
    at the result.

--*/
{
    BOOLEAN ret;
    NTSTATUS status;

    // The following funciton essentially copies the content of
    // current stack location of the underlying IRP to the next one.
    //
    WdfRequestFormatRequestUsingCurrentType(Request);

    WdfRequestSetCompletionRoutine(Request,
                                FilterRequestCompletionRoutine,
                                WDF_NO_CONTEXT);
    ret = WdfRequestSend(Request,
                         Target,
                         WDF_NO_SEND_OPTIONS);

    if (ret == FALSE) {
        status = WdfRequestGetStatus (Request);
        KdPrint( ("WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }

    return;
}

VOID
FilterRequestCompletionRoutine(
    IN WDFREQUEST                  Request,
    IN WDFIOTARGET                 Target,
    PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    IN WDFCONTEXT                  Context
   )
/*++

Routine Description:

    Completion Routine.  Adjust the HID report data for the device.

Arguments:

    Target - Target handle
    Request - Request handle
    Params - request completion params
    Context - Driver supplied context


Return Value:

    VOID

--*/
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    runtimes_FilterRequestCompletionRoutine++;
    //KdPrint(("runtimes_FilterRequestCompletionRoutine 0x%x\n", runtimes_FilterRequestCompletionRoutine));
    //RegDebug(L"FilterRequestCompletionRoutine runtimes_FilterRequestCompletionRoutine", NULL, runtimes_FilterRequestCompletionRoutine);

    PURB pUrb;
    PIRP pIrp;
    PIO_STACK_LOCATION pStack;
    PUCHAR pBuff;
    UINT16 bufLen;

    pIrp = WdfRequestWdmGetIrp(Request);
    pStack = IoGetCurrentIrpStackLocation(pIrp);

    //测试代码
            //RegDebug(L"CompletionParams->IoStatus.Status", NULL, (ULONG)CompletionParams->IoStatus.Status);
            //RegDebug(L"CompletionParams->IoStatus.Information", NULL, (ULONG)CompletionParams->IoStatus.Information);//测试恒定为0
            //RegDebug(L"CompletionParams->Parameters.Ioctl.Output.Buffer", CompletionParams->Parameters.Ioctl.Output.Buffer, (ULONG)CompletionParams->Parameters.Ioctl.Output.Length);//测试恒定为0
            //RegDebug(L"CompletionParams->Parameters.Read.Buffer", CompletionParams->Parameters.Read.Buffer, (ULONG)CompletionParams->Parameters.Read.Length);//测试恒定为0

    
    //switch (pStack->Parameters.DeviceIoControl.IoControlCode){
    //    case IOCTL_INTERNAL_USB_SUBMIT_URB://IOCTL_BTHHFP_GET_DEVICEOBJECT/IOCTL_INTERNAL_USB_SUBMIT_URB(0x00220003)
    //        //RegDebug(L"FilterRequestCompletionRoutine IOCTL_INTERNAL_USB_SUBMIT_URB", NULL, pStack->Parameters.DeviceIoControl.IoControlCode);
    //        break;
    //    default:
    //        RegDebug(L"FilterRequestCompletionRoutine STATUS_NOT_SUPPORTED", NULL, pStack->Parameters.DeviceIoControl.IoControlCode);
    //        break;
    //}

    NTSTATUS status = WdfRequestGetStatus(Request);
    if (NT_SUCCESS(status)) { // success
        //KdPrint(("WdfRequestGetStatus ok 0x%x\n", runtimes_FilterRequestCompletionRoutine));
        ////
        LONG retlen = (LONG)WdfRequestGetInformation(Request);
        //UCHAR* data = (UCHAR*)WdfMemoryGetBuffer(CompletionParams->Parameters.Ioctl.Output.Buffer, NULL);

        //RegDebug(L"WdfRequestGetStatus", NULL, status);//测试会执行
        if (retlen) {
            //RegDebug(L"retlen", NULL, retlen);//测试恒定为0
            KdPrint(("retlen= 0x%x\n", retlen));
        }
        
       // RegDebug(L"CompletionParams->Parameters.Ioctl.Output.Buffer", CompletionParams->Parameters.Ioctl.Output.Buffer, (ULONG)CompletionParams->Parameters.Ioctl.Output.Length);
        //RegDebug(L"CompletionParams->Parameters.Read.Buffer", CompletionParams->Parameters.Read.Buffer, (ULONG)CompletionParams->Parameters.Read.Length);
    }
    

    pUrb = (PURB)pStack->Parameters.Others.Argument1;

    if (pUrb != NULL) {
        //RegDebug(L"pUrb->UrbHeader.Function", NULL, pUrb->UrbHeader.Function);
        //UINT16 CtlLen = (UINT16)pUrb->UrbControlTransfer.TransferBufferLength;//必须强制为UINT6位宽否则会出现巨大数值
        //RegDebug(L"IOCTL_INTERNAL_USB_SUBMIT_URB UrbControlTransfer len=", NULL, CtlLen);
        //KdPrint(("UrbControlTransfer CtlLen= 0x%x\n", CtlLen));
        //if (CtlLen) {
        //    UCHAR* CtlBuffer = (UCHAR*)pUrb->UrbControlTransfer.TransferBuffer;
        //    KdPrint(("IOCTL_INTERNAL_USB_SUBMIT_URB  UrbControlTransfer buff= 0x%x\n", CtlBuffer));
        //    //RegDebug(L"IOCTL_INTERNAL_USB_SUBMIT_URB UrbControlTransfer buff=", CtlBuffer, CtlLen);
        //}

    //拦截到设备返回的描述表，
        UCHAR* DescriptorBuffer = (UCHAR*)pUrb->UrbControlDescriptorRequest.TransferBuffer;
        UINT16 DescriptorLen = (UINT16)pUrb->UrbControlDescriptorRequest.TransferBufferLength;//必须强制为UINT6位宽否则会出现巨大数值
        //RegDebug(L"URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE Descriptor=", DescriptorBuffer, DescriptorLen);
        //KdPrint(("UrbControlDescriptorRequest DescriptorLen= 0x%x\n", DescriptorLen));

        if (DescriptorLen) {
            if (DescriptorLen == 0x12) {//sizeof（USB_DEVICE_DESCRIPTOR）=0x12 //固定长度
                pUsbDeviceDescriptor = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPoolNx, 0x12, HIDUSB_POOL_TAG);
                if (!pUsbDeviceDescriptor) {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    //RegDebug(L"pUsbDeviceDescriptor ExAllocatePoolWithTag failed", NULL, status);
                    return;
                }
                
                RtlCopyMemory(pUsbDeviceDescriptor, DescriptorBuffer, 0x12);//复制数据永久保存才能后面调用
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->iManufacturer= 0x%x", pUsbDeviceDescriptor->iManufacturer));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->idVendor= 0x%x", pUsbDeviceDescriptor->idVendor));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->iProduct= 0x%x", pUsbDeviceDescriptor->iProduct));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->iSerialNumber= 0x%x", pUsbDeviceDescriptor->iSerialNumber));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->bDescriptorType= 0x%x", pUsbDeviceDescriptor->bDescriptorType));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->bDeviceClass= 0x%x", pUsbDeviceDescriptor->bDeviceClass));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->bDeviceSubClass= 0x%x", pUsbDeviceDescriptor->bDeviceSubClass));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->bcdUSB version= 0x%x", pUsbDeviceDescriptor->bcdUSB));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->bLength= 0x%x", pUsbDeviceDescriptor->bLength));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->bMaxPacketSize0= 0x%x", pUsbDeviceDescriptor->bMaxPacketSize0));
                //KdPrint(("DescriptorBuffer pUsbDeviceDescriptor->bNumConfigurations= 0x%x", pUsbDeviceDescriptor->bNumConfigurations));
            }
            else if (DescriptorLen == 0x9) {//sizeof(USB_CONFIGURATION_DESCRIPTOR)=0x9 //固定长度
                pConfigDesc = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPoolNx, 0x9, HIDUSB_POOL_TAG);
                if (!pConfigDesc) {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    //RegDebug(L"pConfigDesc ExAllocatePoolWithTag failed", NULL, status);
                    return;
                }

                RtlCopyMemory(pConfigDesc, DescriptorBuffer, 0x9);//复制数据永久保存才能后面调用
                //KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->bLength= 0x%x", pConfigDesc->bLength));
                //KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->bDescriptorType= 0x%x", pConfigDesc->bDescriptorType));
                //KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->bConfigurationValue= 0x%x", pConfigDesc->bConfigurationValue));
                //KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->bmAttributes= 0x%x", pConfigDesc->bmAttributes));
                KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->wTotalLength= 0x%x", pConfigDesc->wTotalLength));
                //KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->bNumInterfaces= 0x%x", pConfigDesc->bNumInterfaces));
                //KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->iConfiguration= 0x%x", pConfigDesc->iConfiguration));
                //KdPrint(("PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc->MaxPower= 0x%x", pConfigDesc->MaxPower));

            }
            else if (pConfigDesc) {////如果已经获取配置描述符，这里作为一个整体数据块
                if (DescriptorLen == pConfigDesc->wTotalLength) {//configDesc->wTotalLength = 0x22//动态识别长度
                    //USB_CONFIGURATION_DESCRIPTOR + USB_INTERFACE_DESCRIPTOR + HID_DESCRIPTOR + USB_ENDPOINT_DESCRIPTOR x bNumInterfaces 

                    PUSB_COMMON_DESCRIPTOR commonDesc = (PUSB_COMMON_DESCRIPTOR)DescriptorBuffer;// //USB_COMMON_DESCRIPTOR动态识别各种Desc
                    nCommonDescOffset = 0;//D0 重置commonDescOffset
                    while (nCommonDescOffset < pConfigDesc->wTotalLength) {//循环识别多个desc //USB_COMMON_DESCRIPTOR动态识别各种Desc
                        commonDesc = (PUSB_COMMON_DESCRIPTOR)(DescriptorBuffer + nCommonDescOffset);//需要再次赋值
                        switch (commonDesc->bDescriptorType) {
                            case  USB_CONFIGURATION_DESCRIPTOR_TYPE: {
                                    PUSB_CONFIGURATION_DESCRIPTOR configDesc2 = (PUSB_CONFIGURATION_DESCRIPTOR)(DescriptorBuffer + nCommonDescOffset);
                                    nCommonDescOffset += 9;//sizeof(USB_CONFIGURATION_DESCRIPTOR) == 9
                                    UNREFERENCED_PARAMETER(configDesc2);
                                    KdPrint(("configDesc2->bLength= 0x%x", configDesc2->bLength));
                                    KdPrint(("configDesc2->bDescriptorType= 0x%x", configDesc2->bDescriptorType));
                                    break;
                            }

                            case USB_INTERFACE_DESCRIPTOR_TYPE: {
                                    ////临时打印用，永久需要复制数据才行
                                    PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc = (PUSB_INTERFACE_DESCRIPTOR)(DescriptorBuffer + nCommonDescOffset);
                                    nCommonDescOffset += 9;////sizeof(USB_INTERFACE_DESCRIPTOR) == 9
                                    UNREFERENCED_PARAMETER(pInterfaceDesc);
                                    KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->bLength= 0x%x", pInterfaceDesc->bLength));
                                    //KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->bDescriptorType= 0x%x", pInterfaceDesc->bDescriptorType));
                                    //KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->bInterfaceClass= 0x%x", pInterfaceDesc->bInterfaceClass));
                                    //KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->bInterfaceNumber= 0x%x", pInterfaceDesc->bInterfaceNumber));
                                    //KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->bInterfaceProtocol= 0x%x", pInterfaceDesc->bInterfaceProtocol));
                                    //KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->bInterfaceSubClass= 0x%x", pInterfaceDesc->bInterfaceSubClass));
                                    //KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->bNumEndpoints= 0x%x", pInterfaceDesc->bNumEndpoints));
                                    //KdPrint(("PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc->iInterface= 0x%x", pInterfaceDesc->iInterface));
                                    break;
                            }

                            case HID_HID_DESCRIPTOR_TYPE: {
                                    size_t sz = commonDesc->bLength;// sizeof(HID_DESCRIPTOR)=动态长度
                                    pHidDesc = (PHID_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPoolNx, sz, HIDUSB_POOL_TAG);
                                    if (!pHidDesc) {
                                        status = STATUS_INSUFFICIENT_RESOURCES;
                                        //RegDebug(L"pHidDesc ExAllocatePoolWithTag failed", NULL, status);
                                        return;
                                    }

                                    RtlCopyMemory(pHidDesc, DescriptorBuffer + nCommonDescOffset, sz);//复制数据永久保存才能后面调用
                                    nCommonDescOffset += pHidDesc->bLength;//动态长度

                                    KdPrint(("HID_HID_DESCRIPTOR_TYPE pHidDesc->bLength= 0x%x", pHidDesc->bLength));
                                    KdPrint(("HID_HID_DESCRIPTOR_TYPE pHidDesc->bDescriptorType= 0x%x", pHidDesc->bDescriptorType));
                                    KdPrint(("HID_HID_DESCRIPTOR_TYPE pHidDesc->bcdHID= 0x%x", pHidDesc->bcdHID));
                                    KdPrint(("HID_HID_DESCRIPTOR_TYPE pHidDesc->bCountry= 0x%x", pHidDesc->bCountry));
                                    KdPrint(("HID_HID_DESCRIPTOR_TYPE pHidDesc->bNumDescriptors= 0x%x", pHidDesc->bNumDescriptors));
                                    KdPrint(("HID_HID_DESCRIPTOR_TYPE pHidDesc->DescriptorList[0].bReportType= 0x%x", pHidDesc->DescriptorList[0].bReportType));
                                    KdPrint(("HID_HID_DESCRIPTOR_TYPE pHidDesc->DescriptorList[0].wReportLength= 0x%x", pHidDesc->DescriptorList[0].wReportLength));

                                    break;
                            }

                            case USB_ENDPOINT_DESCRIPTOR_TYPE: {
                                    PUSB_ENDPOINT_DESCRIPTOR endpointDesc1 = (PUSB_ENDPOINT_DESCRIPTOR)(DescriptorBuffer + nCommonDescOffset);//
                                    nCommonDescOffset += 7;//sizeof(USB_ENDPOINT_DESCRIPTOR) == 7
                                    UNREFERENCED_PARAMETER(endpointDesc1);
                                    KdPrint(("PUSB_ENDPOINT_DESCRIPTOR endpointDesc1->bLength= 0x%x", endpointDesc1->bLength));
                                    KdPrint(("PUSB_ENDPOINT_DESCRIPTOR endpointDesc1->bDescriptorType= 0x%x", endpointDesc1->bDescriptorType));
                                    KdPrint(("PUSB_ENDPOINT_DESCRIPTOR endpointDesc1->bEndpointAddress= 0x%x", endpointDesc1->bEndpointAddress));
                                    KdPrint(("PUSB_ENDPOINT_DESCRIPTOR endpointDesc1->bInterval= 0x%x", endpointDesc1->bInterval));
                                    KdPrint(("PUSB_ENDPOINT_DESCRIPTOR endpointDesc1->bmAttributes= 0x%x", endpointDesc1->bmAttributes));
                                    KdPrint(("PUSB_ENDPOINT_DESCRIPTOR endpointDesc1->wMaxPacketSize= 0x%x", endpointDesc1->wMaxPacketSize));
                                    break;
                            }
                            default:
                                break;
                        }
                    }
                }
                else if (DescriptorLen == 0x101) {//Device certification status feature report
                    pCertReport = (PPTP_DEVICE_HQA_CERTIFICATION_REPORT)ExAllocatePoolWithTag(NonPagedPoolNx, 0x101, HIDUSB_POOL_TAG);
                    if (!pCertReport) {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                        //RegDebug(L"pCertReport ExAllocatePoolWithTag failed", NULL, status);
                        return;
                    }
                    RtlCopyMemory(pCertReport, DescriptorBuffer, 0x101);//复制数据永久保存才能后面调用

                    KdPrint(("pCertReport->ReportID= 0x%x", pCertReport->ReportID));
                    KdPrint(("pCertReport="));
                    int i;
                    for (i = 0; i < DescriptorLen / 32; i++) {
                        KdPrint(("0x%x,", DescriptorBuffer[i]));
                    }
                    KdPrint(("\n"));
                }
                else if (DescriptorLen == pHidDesc->DescriptorList[0].wReportLength) {//报告描述符数据//hidDesc->DescriptorList[0].wReportLength =0x1fe 动态识别长度
                    size_t szLen = pHidDesc->DescriptorList[0].wReportLength;
                    pReportDesciptorData = (PUCHAR)ExAllocatePoolWithTag(NonPagedPoolNx, szLen, HIDUSB_POOL_TAG);
                    if (!pReportDesciptorData) {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                        //RegDebug(L"pReportDesciptorData ExAllocatePoolWithTag failed", NULL, status);
                        return;
                    }
                    ReportDescriptorLength = (USHORT)szLen;
                    RtlCopyMemory(pReportDesciptorData, DescriptorBuffer, szLen);//复制数据永久保存才能后面调用

                    KdPrint(("pReportDesciptorData="));
                    int i;
                    for (i = 0; i < DescriptorLen; i++) {
                        KdPrint(("0x%x,", DescriptorBuffer[i]));
                    }
                    KdPrint(("\n"));

                    AnalyzeHidReportDescriptor();

                }
                else if (DescriptorLen == 0x2) {//USB_DEVICE_STATUS??//USB_INTERFACE_STATUS??//
                PPTP_DEVICE_FETURE_REPORT FeatureReport = (PPTP_DEVICE_FETURE_REPORT)DescriptorBuffer;

                UNREFERENCED_PARAMETER(FeatureReport);
                    KdPrint(("FeatureReport->ReportID= 0x%x", FeatureReport->ReportID));
                    KdPrint(("FeatureReport->FeatureValue= 0x%x", FeatureReport->FeatureValue));

                    bDispatchDescComplete = TRUE;
                }
                else if (DescriptorLen >=4 && DescriptorLen < 256) {//必须过滤掉异常超大数据请求否则会蓝屏
                    ///////USB_STRING_DESCRIPTOR字符////0x03 unicode string
                    PUSB_STRING_DESCRIPTOR pUsbStrDesc = (PUSB_STRING_DESCRIPTOR)DescriptorBuffer;
                    if (pUsbStrDesc->bDescriptorType == USB_STRING_DESCRIPTOR_TYPE) {
                        size_t pUsbStrLen =(UCHAR) ((ULONG)pUsbStrDesc->bLength + 2);//不能打印无结尾标记的pUsbStrDesc->bString否则越界蓝屏，需要添加0x0x0后缀
                        pUsbStrData = (PWCHAR)ExAllocatePoolWithTag(NonPagedPoolNx, pUsbStrLen, HIDUSB_POOL_TAG);
                        if (!pUsbStrData) {
                            status = STATUS_INSUFFICIENT_RESOURCES;
                            //RegDebug(L"pUsbStrData ExAllocatePoolWithTag failed", NULL, status);
                            return;
                        }
                        RtlZeroMemory(pUsbStrData, pUsbStrLen);//复制数据永久保存才能后面调用 
                        RtlCopyMemory(pUsbStrData, DescriptorBuffer + 2, pUsbStrDesc->bLength);//复制数据永久保存才能后面调用 

                        KdPrint(("USB_STRING_DESCRIPTOR pUsbStrDesc->bDescriptorType= 0x%x", pUsbStrDesc->bDescriptorType));
                        KdPrint(("USB_STRING_DESCRIPTOR pUsbStrDesc->bLength= 0x%x", pUsbStrDesc->bLength));//动态字符长度sizeof(USB_STRING_DESCRIPTOR) = 0x24 
                        KdPrint(("USB_STRING_DESCRIPTOR pUsbStrDesc->bString= %ws\n", pUsbStrData));//不能打印无结尾标记的pUsbStrDesc->bString否则越界蓝屏
                    }
                   
                }
            }          
        }
        

        //拦截到
        //UINT16 BulkLen = (UINT16)pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;//必须强制为UINT6位宽否则会出现巨大数值
        //KdPrint(("IOCTL_INTERNAL_USB_SUBMIT_URB UrbBulkOrInterruptTransfer BulkLen = 0x%x\n", BulkLen));

        switch (pUrb->UrbHeader.Function) {
            case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE: {
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE", NULL, runtimes_FilterRequestCompletionRoutine);
                //KdPrint(("URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE= 0x%x\n", runtimes_FilterRequestCompletionRoutine));
                break;
            }

            case URB_FUNCTION_SELECT_CONFIGURATION: {
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_SELECT_CONFIGURATION", NULL, runtimes_FilterRequestCompletionRoutine);
                //KdPrint(("URB_FUNCTION_SELECT_CONFIGURATION= 0x%x\n", runtimes_FilterRequestCompletionRoutine));
                break;
            }

            case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE: {
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE", NULL, runtimes_FilterRequestCompletionRoutine);
                //KdPrint(("URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE= 0x%x\n", runtimes_FilterRequestCompletionRoutine));
                break;
            }

            case URB_FUNCTION_CLASS_INTERFACE: {
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_CLASS_INTERFACE", NULL, runtimes_FilterRequestCompletionRoutine);
                //KdPrint(("URB_FUNCTION_CLASS_INTERFACE= 0x%x\n", runtimes_FilterRequestCompletionRoutine));
                break;
            }

            case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER: {
                //RegDebug(L"pUrb->UrbHeader.Function URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER", NULL, runtimes_FilterRequestCompletionRoutine);//会蓝屏
                //
                pBuff = (PUCHAR)pUrb->UrbBulkOrInterruptTransfer.TransferBuffer;
                bufLen = (UINT16)pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;//测试恒定为11////必须强制为UINT6位宽否则会出现巨大数值
                //KdPrint(("bufLen= 0x%x\n", bufLen));
                if (bufLen && pBuff) {//测试会执行    
                    KdPrint(("pBuff= 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,\n", pBuff[0], pBuff[1], pBuff[2], pBuff[3], pBuff[4],\
                            pBuff[5], pBuff[6], pBuff[7], pBuff[8], pBuff[9], pBuff[10]));
           /*         pBuff[0] = 1;
                    pBuff[1] = 0;
                    pBuff[2] = 1;
                    pBuff[3] = 1;
                    pBuff[4] = 0;
                    pBuff[5] = 0;
                    pBuff[6] = 0;
                    pBuff[7] = 0;
                    pBuff[8] = 0;
                    pBuff[9] = 0;
                    pBuff[10] = 0;*/

                }
                break;
            }
            default:
                break;
        }
    }

    WdfRequestComplete(Request, CompletionParams->IoStatus.Status);

}


VOID
Filter_DispatchPassThrough(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target
)
{
    //RegDebug(L"Filter_DispatchPassThrough", NULL, 0);

    WDF_REQUEST_SEND_OPTIONS options;
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE device = WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request));


    PFILTER_EXTENSION pDevContext = FilterGetContext(device);

    UNREFERENCED_PARAMETER(pDevContext);
    //
    // We are not interested in post processing the IRP so 
    // fire and forget.

    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);//WDF_REQUEST_SEND_OPTION_TIMEOUT  //WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET

    //将I/O请求发送到下层设备前，做相应的数据处理//不设置完成例程时，下面这句可有可无
    //WdfRequestFormatRequestUsingCurrentType(Request);


    //将 I/O 请求发送到下层设备
    BOOLEAN ret = WdfRequestSend(Request, Target, &options);//WDF_NO_SEND_OPTIONS
    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        RegDebug(L"WdfRequestSend failed", NULL, status);
        WdfRequestComplete(Request, status);
    }

    //RegDebug(L"Filter_DispatchPassThrough WdfRequestSend ok", 0, status);

    return;
}



NTSTATUS
AnalyzeHidReportDescriptor(
    //PFILTER_EXTENSION pDevContext
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PBYTE descriptor = pReportDesciptorData;
    if (!descriptor) {
        //RegDebug(L"AnalyzeHidReportDescriptor pReportDesciptorData err", NULL, status);
        KdPrint(("AnalyzeHidReportDescriptor pReportDesciptorData err 0x%x", status));
        return STATUS_UNSUCCESSFUL;
    }

    USHORT descriptorLen = ReportDescriptorLength;
    PTP_PARSER* tp = &tp_settings;

    int depth = 0;
    BYTE usagePage = 0;
    BYTE reportId = 0;
    BYTE reportSize = 0;
    USHORT reportCount = 0;
    BYTE lastUsage = 0;
    BYTE lastCollection = 0;//改变量能够用于准确判定PTP、MOUSE集合输入报告的reportID
    BOOLEAN inConfigTlc = FALSE;
    BOOLEAN inTouchTlc = FALSE;
    BOOLEAN inMouseTlc = FALSE;
    USHORT logicalMax = 0;
    USHORT physicalMax = 0;
    UCHAR unitExp = 0;
    UCHAR unit = 0;

    for (size_t i = 0; i < descriptorLen;) {
        BYTE type = descriptor[i++];
        int size = type & 3;
        if (size == 3) {
            size++;
        }
        BYTE* value = &descriptor[i];
        i += size;

        if (type == HID_TYPE_BEGIN_COLLECTION) {
            depth++;
            if (depth == 1 && usagePage == HID_USAGE_PAGE_DIGITIZER && lastUsage == HID_USAGE_CONFIGURATION) {
                inConfigTlc = TRUE;
                lastCollection = HID_USAGE_CONFIGURATION;
                //RegDebug(L"AnalyzeHidReportDescriptor inConfigTlc", NULL, 0);
            }
            else if (depth == 1 && usagePage == HID_USAGE_PAGE_DIGITIZER && lastUsage == HID_USAGE_DIGITIZER_TOUCH_PAD) {
                inTouchTlc = TRUE;
                lastCollection = HID_USAGE_DIGITIZER_TOUCH_PAD;
                //RegDebug(L"AnalyzeHidReportDescriptor inTouchTlc", NULL, 0);
            }
            else if (depth == 1 && usagePage == HID_USAGE_PAGE_GENERIC && lastUsage == HID_USAGE_GENERIC_MOUSE) {
                inMouseTlc = TRUE;
                lastCollection = HID_USAGE_GENERIC_MOUSE;
                //RegDebug(L"AnalyzeHidReportDescriptor inMouseTlc", NULL, 0);
            }
        }
        else if (type == HID_TYPE_END_COLLECTION) {
            depth--;

            //下面3个Tlc状态更新是有必要的，可以防止后续相关集合Tlc错误判定发生
            if (depth == 0 && inConfigTlc) {
                inConfigTlc = FALSE;
                //RegDebug(L"AnalyzeHidReportDescriptor inConfigTlc end", NULL, 0);
            }
            else if (depth == 0 && inTouchTlc) {
                inTouchTlc = FALSE;
                //RegDebug(L"AnalyzeHidReportDescriptor inTouchTlc end", NULL, 0);
            }
            else if (depth == 0 && inMouseTlc) {
                inMouseTlc = FALSE;
                //RegDebug(L"AnalyzeHidReportDescriptor inMouseTlc end", NULL, 0);
            }

        }
        else if (type == HID_TYPE_USAGE_PAGE) {
            usagePage = *value;
        }
        else if (type == HID_TYPE_USAGE) {
            lastUsage = *value;
        }
        else if (type == HID_TYPE_REPORT_ID) {
            reportId = *value;
        }
        else if (type == HID_TYPE_REPORT_SIZE) {
            reportSize = *value;
        }
        else if (type == HID_TYPE_REPORT_COUNT) {
            reportCount = *value;
        }
        else if (type == HID_TYPE_REPORT_COUNT_2) {
            reportCount = *(PUSHORT)value;
        }
        else if (type == HID_TYPE_LOGICAL_MINIMUM) {
            logicalMax = *value;
        }
        else if (type == HID_TYPE_LOGICAL_MAXIMUM_2) {
            logicalMax = *(PUSHORT)value;
        }
        else if (type == HID_TYPE_PHYSICAL_MAXIMUM) {
            physicalMax = *value;
        }
        else if (type == HID_TYPE_PHYSICAL_MAXIMUM_2) {
            physicalMax = *(PUSHORT)value;
        }
        else if (type == HID_TYPE_UNIT_EXPONENT) {
            unitExp = *value;
        }
        else if (type == HID_TYPE_UNIT) {
            unit = *value;
        }
        else if (type == HID_TYPE_UNIT_2) {
            unit = *value;
        }

        else if (inTouchTlc && depth == 2 && lastCollection == HID_USAGE_DIGITIZER_TOUCH_PAD && lastUsage == HID_USAGE_DIGITIZER_FINGER) {//
            REPORTID_MULTITOUCH_COLLECTION = reportId;
            //RegDebug(L"AnalyzeHidReportDescriptor REPORTID_MULTITOUCH_COLLECTION=", NULL, REPORTID_MULTITOUCH_COLLECTION);
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_MULTITOUCH_COLLECTION= 0x%x", REPORTID_MULTITOUCH_COLLECTION));

            //这里计算单个报告数据包的手指数量用来后续判断报告模式及bHybrid_ReportingMode的赋值
            DeviceDescriptorFingerCount++;
            //RegDebug(L"AnalyzeHidReportDescriptor DeviceDescriptorFingerCount=", NULL, DeviceDescriptorFingerCount);
            KdPrint(("AnalyzeHidReportDescriptor DeviceDescriptorFingerCount= 0x%x", DeviceDescriptorFingerCount));
        }
        else if (inMouseTlc && depth == 2 && lastCollection == HID_USAGE_GENERIC_MOUSE && lastUsage == HID_USAGE_GENERIC_POINTER) {
            //下层的Mouse集合report本驱动并不会读取，只是作为输出到上层类驱动的Report使用
            REPORTID_MOUSE_COLLECTION = reportId;
            //RegDebug(L"AnalyzeHidReportDescriptor REPORTID_MOUSE_COLLECTION=", NULL, REPORTID_MOUSE_COLLECTION);
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_MOUSE_COLLECTION= 0x%x", REPORTID_MOUSE_COLLECTION));
        }
        else if (inConfigTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_INPUT_MODE) {
            REPORTSIZE_INPUT_MODE = (reportSize + 7) / 8;//报告数据总长度
            REPORTID_INPUT_MODE = reportId;
            // RegDebug(L"AnalyzeHidReportDescriptor REPORTID_INPUT_MODE=", NULL, REPORTID_INPUT_MODE);
             //RegDebug(L"AnalyzeHidReportDescriptor REPORTSIZE_INPUT_MODE=", NULL, REPORTSIZE_INPUT_MODE);
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_INPUT_MODE= 0x%x", REPORTID_INPUT_MODE));
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_INPUT_MODE= 0x%x", REPORTSIZE_INPUT_MODE));
            continue;
        }
        else if (inConfigTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_SURFACE_SWITCH || lastUsage == HID_USAGE_BUTTON_SWITCH) {
            //默认标准规范为HID_USAGE_SURFACE_SWITCH与HID_USAGE_BUTTON_SWITCH各1bit组合低位成1个字节HID_USAGE_FUNCTION_SWITCH报告
            REPORTSIZE_FUNCTION_SWITCH = (reportSize + 7) / 8;//报告数据总长度
            REPORTID_FUNCTION_SWITCH = reportId;
            //RegDebug(L"AnalyzeHidReportDescriptor REPORTID_FUNCTION_SWITCH=", NULL, REPORTID_FUNCTION_SWITCH);
            //RegDebug(L"AnalyzeHidReportDescriptor REPORTSIZE_FUNCTION_SWITCH=", NULL, REPORTSIZE_FUNCTION_SWITCH);
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_FUNCTION_SWITCH= 0x%x", REPORTID_FUNCTION_SWITCH));
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_FUNCTION_SWITCH= 0x%x", REPORTSIZE_FUNCTION_SWITCH));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_CONTACT_COUNT_MAXIMUM || lastUsage == HID_USAGE_PAD_TYPE) {
            //默认标准规范为HID_USAGE_CONTACT_COUNT_MAXIMUM与HID_USAGE_PAD_TYPE各4bit组合低位成1个字节HID_USAGE_DEVICE_CAPS报告
            REPORTSIZE_DEVICE_CAPS = (reportSize + 7) / 8;//报告数据总长度
            REPORTID_DEVICE_CAPS = reportId;
            //RegDebug(L"AnalyzeHidReportDescriptor REPORTSIZE_DEVICE_CAPS=", NULL, REPORTSIZE_DEVICE_CAPS);
            //RegDebug(L"AnalyzeHidReportDescriptor REPORTID_DEVICE_CAPS=", NULL, REPORTID_DEVICE_CAPS);
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_DEVICE_CAPS= 0x%x", REPORTSIZE_DEVICE_CAPS));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_DEVICE_CAPS= 0x%x", REPORTID_DEVICE_CAPS));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_PAGE_VENDOR_DEFINED_DEVICE_CERTIFICATION) {
            REPORTSIZE_PTPHQA = 256;
            REPORTID_PTPHQA = reportId;
            //RegDebug(L"AnalyzeHidReportDescriptor REPORTID_PTPHQA=", NULL, REPORTID_PTPHQA);
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_PTPHQA= 0x%x", REPORTID_PTPHQA));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_INPUT && lastUsage == HID_USAGE_X) {
            tp->physicalMax_X = physicalMax;
            tp->logicalMax_X = logicalMax;
            tp->unitExp = UnitExponent_Table[unitExp];
            tp->unit = unit;
            //RegDebug(L"AnalyzeHidReportDescriptor physicalMax_X=", NULL, tp->physicalMax_X);
            //RegDebug(L"AnalyzeHidReportDescriptor logicalMax_X=", NULL, tp->logicalMax_X);
            //RegDebug(L"AnalyzeHidReportDescriptor unitExp=", NULL, tp->unitExp);
            //RegDebug(L"AnalyzeHidReportDescriptor unit=", NULL, tp->unit);
            KdPrint(("AnalyzeHidReportDescriptor physicalMax_X= 0x%x", tp->physicalMax_X));
            KdPrint(("AnalyzeHidReportDescriptor logicalMax_X= 0x%x", tp->logicalMax_X));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_INPUT && lastUsage == HID_USAGE_Y) {
            tp->physicalMax_Y = physicalMax;
            tp->logicalMax_Y = logicalMax;
            tp->unitExp = UnitExponent_Table[unitExp];
            tp->unit = unit;
            //RegDebug(L"AnalyzeHidReportDescriptor physicalMax_Y=", NULL, tp->physicalMax_Y);
            //RegDebug(L"AnalyzeHidReportDescriptor logicalMax_Y=", NULL, tp->logicalMax_Y);
            //RegDebug(L"AnalyzeHidReportDescriptor unitExp=", NULL, tp->unitExp);
           // RegDebug(L"AnalyzeHidReportDescriptor unit=", NULL, tp->unit);
            KdPrint(("AnalyzeHidReportDescriptor physicalMax_Y= 0x%x", tp->physicalMax_Y));
            KdPrint(("AnalyzeHidReportDescriptor logicalMax_Y= 0x%x", tp->logicalMax_Y));
            continue;
        }
    }

    //判断触摸板报告模式
    if (DeviceDescriptorFingerCount < 5) {//5个手指数据以下
        bHybrid_ReportingMode = TRUE;//混合报告模式确认
        //RegDebug(L"AnalyzeHidReportDescriptor bHybrid_ReportingMode=", NULL, bHybrid_ReportingMode);
        KdPrint(("AnalyzeHidReportDescriptor bHybrid_ReportingMode= 0x%x", bHybrid_ReportingMode));
    }


    //计算保存触摸板尺寸分辨率等参数
    //转换为mm长度单位
    if (tp->unit == 0x11) {//cm长度单位
        tp->physical_Width_mm = tp->physicalMax_X * pow(10.0, tp->unitExp) * 10;
        tp->physical_Height_mm = tp->physicalMax_Y * pow(10.0, tp->unitExp) * 10;
    }
    else {//0x13为inch长度单位
        tp->physical_Width_mm = tp->physicalMax_X * pow(10.0, tp->unitExp) * 25.4;
        tp->physical_Height_mm = tp->physicalMax_Y * pow(10.0, tp->unitExp) * 25.4;
    }

    if (!tp->physical_Width_mm) {
        //RegDebug(L"AnalyzeHidReportDescriptor physical_Width_mm err", NULL, 0);
        KdPrint(("AnalyzeHidReportDescriptor physical_Width_mm err"));
        return STATUS_UNSUCCESSFUL;
    }
    if (!tp->physical_Height_mm) {
        //RegDebug(L"AnalyzeHidReportDescriptor physical_Height_mm err", NULL, 0);
        KdPrint(("AnalyzeHidReportDescriptor physical_Height_mm err"));
        return STATUS_UNSUCCESSFUL;
    }

    tp->TouchPad_DPMM_x = (float)(tp->logicalMax_X / tp->physical_Width_mm);//单位为dot/mm
    tp->TouchPad_DPMM_y = (float)(tp->logicalMax_Y / tp->physical_Height_mm);//单位为dot/mm
    //RegDebug(L"AnalyzeHidReportDescriptor TouchPad_DPMM_x=", NULL, (ULONG)tp->TouchPad_DPMM_x);
    //RegDebug(L"AnalyzeHidReportDescriptor TouchPad_DPMM_y=", NULL, (ULONG)tp->TouchPad_DPMM_y);
    KdPrint(("AnalyzeHidReportDescriptor TouchPad_DPMM_x= 0x%x", (ULONG)tp->TouchPad_DPMM_x));
    KdPrint(("AnalyzeHidReportDescriptor TouchPad_DPMM_y= 0x%x", (ULONG)tp->TouchPad_DPMM_y));

    //动态调整手指头大小常量
    tp->thumb_Width = 18;//手指头宽度,默认以中指18mm宽为基准
    tp->thumb_Scale = 1.0;//手指头尺寸缩放比例，
    tp->FingerMinDistance = 12 * tp->TouchPad_DPMM_x * tp->thumb_Scale;//定义有效的相邻手指最小距离
    tp->FingerClosedThresholdDistance = 16 * tp->TouchPad_DPMM_x * tp->thumb_Scale;//定义相邻手指合拢时的最小距离
    tp->FingerMaxDistance = tp->FingerMinDistance * 4;//定义有效的相邻手指最大距离(FingerMinDistance*4) 

    tp->PointerSensitivity_x = tp->TouchPad_DPMM_x / 25;
    tp->PointerSensitivity_y = tp->TouchPad_DPMM_y / 25;

    tp->StartY_TOP = (ULONG)(10 * tp->TouchPad_DPMM_y);////起点误触横线Y值为距离触摸板顶部10mm处的Y坐标
    ULONG halfwidth = (ULONG)(43.2 * tp->TouchPad_DPMM_x);//起点误触竖线X值为距离触摸板中心线左右侧43.2mm处的X坐标

    if (tp->logicalMax_X / 2 > halfwidth) {//触摸板宽度大于正常触摸起点区域宽度
        tp->StartX_LEFT = tp->logicalMax_X / 2 - halfwidth;
        tp->StartX_RIGHT = tp->logicalMax_X / 2 + halfwidth;
    }
    else {
        tp->StartX_LEFT = 0;
        tp->StartX_RIGHT = tp->logicalMax_X;
    }

    //RegDebug(L"AnalyzeHidReportDescriptor tp->StartTop_Y =", NULL, tp->StartY_TOP);
    //RegDebug(L"AnalyzeHidReportDescriptor tp->StartX_LEFT =", NULL, tp->StartX_LEFT);
    //RegDebug(L"AnalyzeHidReportDescriptor tp->StartX_RIGHT =", NULL, tp->StartX_RIGHT);

    KdPrint(("AnalyzeHidReportDescriptor end"));
    //RegDebug(L"AnalyzeHidReportDescriptor end", NULL, status);
    return status;
}
