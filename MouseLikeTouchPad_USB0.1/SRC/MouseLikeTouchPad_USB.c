#include <ntifs.h>
#include <hidport.h>
#include <Usb.h>
#include <usbioctl.h>
#include <Usbdlib.h>



#define HID_USB_TAG     'UdiH'
#define HID_RESET_TAG   'tesR'
#define HID_REMLOCK_TAG 'WRUH'

#define debug_on 1
ULONG runtimes_IOCTL_HID_READ_REPORT;
ULONG runtimes_IOCTL_IOCTL;
ULONG runtimes_ioReadCompletion;

const ULONG DEXT_NO_HID_DESC = 0x1;

typedef
struct _HID_MINI_DEV_EXTENSION
{
    ULONG PnpState;
    PUSB_DEVICE_DESCRIPTOR pDevDesc;
    PUSBD_INTERFACE_INFORMATION pInterfaceInfo;
    USBD_CONFIGURATION_HANDLE UsbdConfigurationHandle;
    LONG PendingRequestsCount;
    KEVENT Event;
    ULONG Flags;
    PIO_WORKITEM pWorkItem;
    HID_DESCRIPTOR HidDesc;
    //3 bytes align
    PDEVICE_OBJECT pFdo;
    IO_REMOVE_LOCK RemoveLock;
} HID_MINI_DEV_EXTENSION, *PHID_MINI_DEV_EXTENSION;

typedef
struct _WRKITM_RESET_CONTEXT
{
    ULONG Tag;
    PIO_WORKITEM pWorkItem;
    PDEVICE_OBJECT pDeviceObject;
    PIRP pIrp;
} WRKITM_RESET_CONTEXT, *PWRKITM_RESET_CONTEXT;
VOID RegDebug(WCHAR* strValueName, PVOID dataValue, ULONG datasizeValue);//RegDebug(L"Run debug here",pBuffer,pBufferSize);//RegDebug(L"Run debug here",NULL,0x12345678);


NTSTATUS HumGetStringDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetHidDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetReportDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp, PBOOLEAN pNeedToCompleteIrp);
NTSTATUS HumGetDeviceAttributes(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetPhysicalDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetMsGenreDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
PUSBD_PIPE_INFORMATION GetInterruptInputPipeForDevice(PHID_MINI_DEV_EXTENSION pMiniDevExt);
LONG HumDecrementPendingRequestCount(PHID_MINI_DEV_EXTENSION pMiniDevExt);
NTSTATUS HumReadCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
NTSTATUS HumPowerCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
VOID HumSetIdleWorker(PDEVICE_OBJECT DeviceObject, PVOID Context);
NTSTATUS HumSetIdle(PDEVICE_OBJECT pDevObj);
NTSTATUS HumCallUSB(PDEVICE_OBJECT pDevObj, PURB pUrb);
NTSTATUS HumGetDescriptorRequest(PDEVICE_OBJECT pDevObj, USHORT Function, CHAR DescriptorType, PVOID *pDescBuffer, PULONG pDescBuffLen, int Unused, CHAR Index, SHORT LangId);
NTSTATUS HumGetDeviceDescriptor(PDEVICE_OBJECT pDevObj, PHID_MINI_DEV_EXTENSION pMiniDevExt);
NTSTATUS HumGetConfigDescriptor(PDEVICE_OBJECT pDevObj, PUSB_CONFIGURATION_DESCRIPTOR *ppConfigDesc, PULONG pConfigDescLen);
NTSTATUS HumGetHidInfo(PDEVICE_OBJECT pDevObj, UINT_PTR pConfigDescr, UINT_PTR TransferrBufferLen);
NTSTATUS HumSelectConfiguration(PDEVICE_OBJECT pDevObj, PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc);
NTSTATUS HumParseHidInterface(HID_MINI_DEV_EXTENSION *pMiniDevExt, PUSB_INTERFACE_DESCRIPTOR pInterface, LONG TotalLength, PUSB_INTERFACE_DESCRIPTOR *ppHidDesc);
NTSTATUS HumCreateClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumPnP(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetSetReport(PDEVICE_OBJECT pDevObj, PIRP pIrp, PBOOLEAN pNeedToCompleteIrp);
NTSTATUS HumAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pFdo);
VOID HumUnload(PDRIVER_OBJECT pDrvObj);
NTSTATUS HumGetSetReportCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
VOID HumResetWorkItem(PDEVICE_OBJECT DeviceObject, PWRKITM_RESET_CONTEXT pContext);
NTSTATUS HumAbortPendingRequests(PDEVICE_OBJECT pDevObj);
NTSTATUS HumRemoveDevice(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumStopDevice(PDEVICE_OBJECT pDevObj);
NTSTATUS HumWriteCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
NTSTATUS HumIncrementPendingRequestCount(PHID_MINI_DEV_EXTENSION pMiniDevExt);
NTSTATUS HumQueueResetWorkItem(PDEVICE_OBJECT pDevObj, PIRP pIrp);

LARGE_INTEGER UrbTimeout = { 0xFD050F80, 0xFFFFFFFF };

NTSTATUS HumInternalIoctl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS                status;
    ULONG                   IoControlCode;
    BOOLEAN                 NeedCompleteIrp;
    PIO_STACK_LOCATION      pStack;
    PIO_STACK_LOCATION      pNextStack;
    PHID_DEVICE_EXTENSION   pDevExt;
    PVOID                   pUserBuffer;
    PURB                    pUrb;
    PUSBD_INTERFACE_INFORMATION pInterfaceInfo;
    ULONG                   Index;
    ULONG                   NumOfPipes;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    BOOLEAN                 AcquiredLock;
    PHID_XFER_PACKET        pTransferPacket;
    ULONG                   OutBuffLen;
    PUSBD_PIPE_INFORMATION  pPipeInfo;
    USHORT                  Value;
    USHORT                  UrbLen;

    runtimes_IOCTL_IOCTL++;
    //RegDebug(L"HumInternalIoctl runtimes_IOCTL_IOCTL", NULL, runtimes_IOCTL_IOCTL);

    UrbLen = sizeof(URB);
    AcquiredLock = FALSE;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    NeedCompleteIrp = TRUE;

    status = IoAcquireRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, __FILE__, __LINE__, sizeof(pMiniDevExt->RemoveLock));
    if (NT_SUCCESS(status) == FALSE)
    {
        goto ExitNoLock;
    }

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    AcquiredLock = TRUE;
    IoControlCode = pStack->Parameters.DeviceIoControl.IoControlCode;


    switch (IoControlCode)
    {
    case IOCTL_HID_GET_STRING:
    {
        status = HumGetStringDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_FEATURE: 
    //{//新增代码
    //    RegDebug(L"IOCTL_HID_GET_FEATURE", NULL, runtimes_IOCTL_IOCTL);
    //    status = HumGetSetReport(pDevObj, pIrp, &NeedCompleteIrp);
    //    goto ExitCheckNeedCompleteIrp;
    //}
    case IOCTL_HID_SET_FEATURE:
    {
        RegDebug(L"IOCTL_HID_SET_FEATURE", NULL, runtimes_IOCTL_IOCTL);
        status = HumGetSetReport(pDevObj, pIrp, &NeedCompleteIrp);
        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
    {
        status = HumGetHidDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
    {
        status = HumGetReportDescriptor(pDevObj, pIrp, &NeedCompleteIrp);
        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_ACTIVATE_DEVICE:
    case IOCTL_HID_DEACTIVATE_DEVICE:
    {
        status = STATUS_SUCCESS;
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
    {
        status = HumGetDeviceAttributes(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
    {
        if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(USB_IDLE_CALLBACK_INFO))
        {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else
        {
            IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));
            pNextStack = IoGetNextIrpStackLocation(pIrp);
            NeedCompleteIrp = FALSE;
            pNextStack->MajorFunction = pStack->MajorFunction;
            pNextStack->Parameters.DeviceIoControl.InputBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;
            pNextStack->Parameters.DeviceIoControl.Type3InputBuffer = pStack->Parameters.DeviceIoControl.Type3InputBuffer;
            pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION;
            pNextStack->DeviceObject = pDevExt->NextDeviceObject;
            status = IoCallDriver(pDevExt->NextDeviceObject, pIrp);
            if (NT_SUCCESS(status) == TRUE)
            {
                goto Exit;
            }
        }

        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_SET_OUTPUT_REPORT:
    case IOCTL_HID_GET_INPUT_REPORT:
    {
        status = HumGetSetReport(pDevObj, pIrp, &NeedCompleteIrp);
        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:
    {
        status = HumGetPhysicalDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_INDEXED_STRING:
    {
        status = HumGetStringDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_MS_GENRE_DESCRIPTOR:
    {
        status = HumGetMsGenreDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_WRITE_REPORT:
    {
        pTransferPacket = (PHID_XFER_PACKET)pIrp->UserBuffer;
        if (pTransferPacket == NULL || pTransferPacket->reportBuffer == NULL || pTransferPacket->reportBufferLen == 0)
        {
            status = STATUS_DATA_ERROR;
            NeedCompleteIrp = TRUE;

            goto ExitCheckNeedCompleteIrp;
        }
        pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
        if (pUrb == NULL)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            NeedCompleteIrp = TRUE;

            goto ExitCheckNeedCompleteIrp;
        }
        memset(pUrb, 0, UrbLen);
        if (BooleanFlagOn(pMiniDevExt->Flags, DEXT_NO_HID_DESC))
        {
            pPipeInfo = GetInterruptInputPipeForDevice(pMiniDevExt);
            if (pPipeInfo == NULL)
            {
                status = STATUS_DATA_ERROR;
                ExFreePool(pUrb);
                NeedCompleteIrp = TRUE;

                goto ExitCheckNeedCompleteIrp;
            }
            pUrb->UrbControlVendorClassRequest.Index = pPipeInfo->EndpointAddress & ~USB_ENDPOINT_DIRECTION_MASK;
            pUrb->UrbHeader.Length = sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
            pUrb->UrbHeader.Function = URB_FUNCTION_CLASS_ENDPOINT;
            pUrb->UrbControlVendorClassRequest.RequestTypeReservedBits = 0x22;
            pUrb->UrbControlVendorClassRequest.Request = 0x9;
            Value = pTransferPacket->reportId + 0x200;
        }
        else
        {
            NumOfPipes = pMiniDevExt->pInterfaceInfo->NumberOfPipes;
            if (NumOfPipes == 0)
            {
                pUrb->UrbHeader.Length = sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
                pUrb->UrbHeader.Function = URB_FUNCTION_CLASS_INTERFACE;
                pInterfaceInfo = pMiniDevExt->pInterfaceInfo;
                pUrb->UrbControlVendorClassRequest.Index = pInterfaceInfo->InterfaceNumber;
                pUrb->UrbControlVendorClassRequest.RequestTypeReservedBits = 0x22;
                pUrb->UrbControlVendorClassRequest.Request = 0x9;
                Value = pTransferPacket->reportId + 0x200;
            }
            else
            {
                Index = 0;
                pPipeInfo = pMiniDevExt->pInterfaceInfo->Pipes;
                for (;;)
                {
                    if ((USB_ENDPOINT_DIRECTION_OUT(pPipeInfo->EndpointAddress)) && (pPipeInfo->PipeType == UsbdPipeTypeInterrupt))
                    {
                        pUrb->UrbHeader.Length = sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER);
                        pUrb->UrbHeader.Function = URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
                        pUrb->UrbBulkOrInterruptTransfer.PipeHandle = pPipeInfo->PipeHandle;
                        pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = pTransferPacket->reportBufferLen;
                        pUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL = 0;
                        pUrb->UrbBulkOrInterruptTransfer.TransferBuffer = pTransferPacket->reportBuffer;
                        pUrb->UrbBulkOrInterruptTransfer.TransferFlags = USBD_SHORT_TRANSFER_OK;
                        pUrb->UrbBulkOrInterruptTransfer.UrbLink = 0;
                        goto CheckPnpStateAndCallDriver1;
                    }
                    ++Index;
                    pPipeInfo++;
                    if (Index >= NumOfPipes)
                    {
                        break;
                    }
                }

                pUrb->UrbHeader.Length = sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
                pUrb->UrbHeader.Function = URB_FUNCTION_CLASS_INTERFACE;
                pInterfaceInfo = pMiniDevExt->pInterfaceInfo;
                pUrb->UrbControlVendorClassRequest.Index = pInterfaceInfo->InterfaceNumber;
                pUrb->UrbControlVendorClassRequest.RequestTypeReservedBits = 0x22;
                pUrb->UrbControlVendorClassRequest.Request = 0x9;
                Value = pTransferPacket->reportId + 0x200;
            }
        }

        pUrb->UrbControlVendorClassRequest.Value = Value;
        pUrb->UrbControlVendorClassRequest.TransferFlags = 0;
        pUrb->UrbControlVendorClassRequest.TransferBuffer = pTransferPacket->reportBuffer;
        pUrb->UrbControlVendorClassRequest.TransferBufferLength = pTransferPacket->reportBufferLen;

    CheckPnpStateAndCallDriver1:
        IoSetCompletionRoutine(pIrp, HumWriteCompletion, pUrb, TRUE, TRUE, TRUE);
        pNextStack = IoGetNextIrpStackLocation(pIrp);
        pNextStack->Parameters.Others.Argument1 = pUrb;
        pNextStack->MajorFunction = pStack->MajorFunction;
        pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
        pNextStack->DeviceObject = pDevExt->NextDeviceObject;
        InterlockedIncrement(&pMiniDevExt->PendingRequestsCount);
        if (pMiniDevExt->PnpState == 2 || pMiniDevExt->PnpState == 1)
        {
            status = IoCallDriver(pDevExt->NextDeviceObject, pIrp);
            NeedCompleteIrp = FALSE;

            goto ExitCheckNeedCompleteIrp;
        }
        HumDecrementPendingRequestCount(pMiniDevExt);
        ExFreePool(pUrb);
        status = STATUS_NO_SUCH_DEVICE;
        NeedCompleteIrp = TRUE;

        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_READ_REPORT:
    {
        runtimes_IOCTL_HID_READ_REPORT++;
        //RegDebug(L"HumInternalIoctl IOCTL_HID_READ_REPORT", NULL, runtimes_IOCTL_HID_READ_REPORT);

        pUserBuffer = pIrp->UserBuffer;
        OutBuffLen = pStack->Parameters.DeviceIoControl.OutputBufferLength;
        if (OutBuffLen == 0 || pUserBuffer == NULL)
        {
            status = STATUS_INVALID_PARAMETER;
            //RegDebug(L"HumInternalIoctl IOCTL_HID_READ_REPORT STATUS_INVALID_PARAMETER", NULL, runtimes_IOCTL_IOCTL);

        }
        else
        {

            pInterfaceInfo = pMiniDevExt->pInterfaceInfo;
            Index = 0;
            NumOfPipes = pInterfaceInfo->NumberOfPipes;
            if (NumOfPipes == 0)
            {
                status = STATUS_DEVICE_CONFIGURATION_ERROR;
                NeedCompleteIrp = TRUE;

                goto ExitCheckNeedCompleteIrp;
            }
            pPipeInfo = pMiniDevExt->pInterfaceInfo->Pipes;
            while (USB_ENDPOINT_DIRECTION_OUT(pPipeInfo->EndpointAddress) || (pPipeInfo->PipeType != UsbdPipeTypeInterrupt))
            {
                ++Index;
                pPipeInfo++;
                if (Index >= NumOfPipes)
                {
                    status = STATUS_DEVICE_CONFIGURATION_ERROR;
                    NeedCompleteIrp = TRUE;

                    goto ExitCheckNeedCompleteIrp;
                }
            }

            pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
            if (pUrb == NULL)
            {
                status = STATUS_INSUFFICIENT_RESOURCES;
                //RegDebug(L"HumInternalIoctl IOCTL_HID_READ_REPORT ExAllocatePoolWithTag err", NULL, runtimes_IOCTL_IOCTL);
            }
            else
            {
                memset(pUrb, 0, UrbLen);
                pUrb->UrbHeader.Length = sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER);
                pUrb->UrbHeader.Function = URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
                pUrb->UrbBulkOrInterruptTransfer.PipeHandle = pPipeInfo->PipeHandle;
                pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = OutBuffLen;
                pUrb->UrbBulkOrInterruptTransfer.TransferBuffer = pUserBuffer;
                pUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL = 0;
                pUrb->UrbBulkOrInterruptTransfer.TransferFlags = USBD_SHORT_TRANSFER_OK | USBD_TRANSFER_DIRECTION_IN;
                pUrb->UrbBulkOrInterruptTransfer.UrbLink = 0;

                

                IoSetCompletionRoutine(pIrp, HumReadCompletion, pUrb, TRUE, TRUE, TRUE);

                pNextStack = IoGetNextIrpStackLocation(pIrp);
                pNextStack->Parameters.Others.Argument1 = pUrb;
                pNextStack->MajorFunction = pStack->MajorFunction;
                pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
                pNextStack->DeviceObject = pDevExt->NextDeviceObject;

                InterlockedIncrement(&pMiniDevExt->PendingRequestsCount);
                if (pMiniDevExt->PnpState == 2 || pMiniDevExt->PnpState == 1)
                {
                    status = IoCallDriver(pDevExt->NextDeviceObject, pIrp);
                    NeedCompleteIrp = FALSE;

                    if (NT_SUCCESS(status) == TRUE)
                    {
                        goto Exit;
                    }
                    goto ExitCheckNeedCompleteIrp;
                }
                HumDecrementPendingRequestCount(pMiniDevExt);
                ExFreePool(pUrb);
                status = STATUS_NO_SUCH_DEVICE;
            }
        }
        NeedCompleteIrp = TRUE;
        goto ExitCheckNeedCompleteIrp;
    }
    default:
    {
        status = pIrp->IoStatus.Status;
        goto ExitCompleteIrp;
    }
    }

ExitNoLock:

    pIrp->IoStatus.Status = status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return status;

ExitCheckNeedCompleteIrp:
    if (NeedCompleteIrp == FALSE)
    {
        return status;
    }
ExitCompleteIrp:
    pIrp->IoStatus.Status = status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    if (AcquiredLock)
    {
        IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));
    }
Exit:

    return status;
}

NTSTATUS HumPower(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS                status;
    PHID_DEVICE_EXTENSION   pDevExt;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    status = IoAcquireRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, __FILE__, __LINE__, sizeof(pMiniDevExt->RemoveLock));
    if (NT_SUCCESS(status) == FALSE)
    {
        PoStartNextPowerIrp(pIrp);
        pIrp->IoStatus.Status = status;
        pIrp->IoStatus.Information = 0;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    }
    else
    {
        IoCopyCurrentIrpStackLocationToNext(pIrp);
        IoSetCompletionRoutine(pIrp, HumPowerCompletion, NULL, TRUE, TRUE, TRUE);
        status = PoCallDriver(pDevExt->NextDeviceObject, pIrp);
    }

    return status;
}

NTSTATUS HumReadCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext)
{
    NTSTATUS                status;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PURB                    pUrb;
    PHID_DEVICE_EXTENSION   pDevExt;

    runtimes_ioReadCompletion++;
    //if (runtimes_ioReadCompletion==2) {
    //    RegDebug(L"HumReadCompletion start", NULL, runtimes_IOCTL_HID_READ_REPORT);
    //}
    status = STATUS_SUCCESS;
    pUrb = (PURB)pContext;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    if (NT_SUCCESS(pIrp->IoStatus.Status) == TRUE)
    {
        pIrp->IoStatus.Information = pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        //size_t sz= pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        //PUCHAR pBuff = (PUCHAR)pUrb->UrbBulkOrInterruptTransfer.TransferBuffer;
        //if (sz) {
        //    KdPrint(("datalen= 0x%x\n", (INT)sz));
        //    KdPrint(("data= 0x%x,0x%x,0x%x,0x%x,0x%x,\n", pBuff[0], pBuff[1], pBuff[2], pBuff[3], pBuff[4]));
        //}

    }
    else if (pIrp->IoStatus.Status == STATUS_CANCELLED)
    {

    }
    else if (pIrp->IoStatus.Status == STATUS_DEVICE_NOT_CONNECTED)
    {

    }
    else
    {
        /*
        WPP_RECORDER_SF_qq(
        *(_DWORD *)&WPP_GLOBAL_Control->StackSize,
        3,
        2u,
        24,
        "^jJt??|0Q\v+︵?,
        (char)pIrp,
        pIrp->IoStatus.Status);
        */
        status = HumQueueResetWorkItem(pDevObj, pIrp);
    }
    ExFreePool(pUrb);
    if (InterlockedDecrement(&pMiniDevExt->PendingRequestsCount) < 0)
    {
        KeSetEvent(&pMiniDevExt->Event, IO_NO_INCREMENT, FALSE);
    }
    IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));

    return status;
}

NTSTATUS HumWriteCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext)
{
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PURB                    pUrb;
    PHID_DEVICE_EXTENSION   pDevExt;

    pUrb = (PURB)pContext;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    if (NT_SUCCESS(pIrp->IoStatus.Status) == TRUE)
    {
        pIrp->IoStatus.Information = pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
    }
    ExFreePool(pUrb);
    if (pIrp->PendingReturned)
    {
        IoMarkIrpPending(pIrp);
    }
    if (_InterlockedDecrement(&pMiniDevExt->PendingRequestsCount) < 0)
    {
        KeSetEvent(&pMiniDevExt->Event, IO_NO_INCREMENT, FALSE);
    }
    IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));

    return STATUS_SUCCESS;
}

NTSTATUS HumPnpCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext)
{
    UNREFERENCED_PARAMETER(pDevObj);
    UNREFERENCED_PARAMETER(pIrp);

    KeSetEvent((PKEVENT)pContext, EVENT_INCREMENT /* 1 */, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS HumPowerCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext)
{
    NTSTATUS                status;
    PIO_STACK_LOCATION      pStack;
    PIO_WORKITEM            pWorkItem;
    PHID_DEVICE_EXTENSION   pDevExt;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;

    UNREFERENCED_PARAMETER(pContext);

    status = pIrp->IoStatus.Status;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    if (pIrp->PendingReturned)
    {
        IoMarkIrpPending(pIrp);
    }

    if (NT_SUCCESS(status) == TRUE)
    {
        pStack = pIrp->Tail.Overlay.CurrentStackLocation;
        if (pStack->MinorFunction == IRP_MN_SET_POWER
            && pStack->Parameters.Power.Type == DevicePowerState
            && pStack->Parameters.Power.State.DeviceState == PowerDeviceD0
            && pMiniDevExt->PnpState == 2)
        {
            if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
            {
                pWorkItem = IoAllocateWorkItem(pDevObj);
                if (pWorkItem)
                {
                    IoQueueWorkItem(pWorkItem, HumSetIdleWorker, 0, (PVOID)pWorkItem);
                }
                else
                {
                }
            }
            else
            {
                HumSetIdle(pDevObj);
            }
        }
    }
    IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));

    return STATUS_SUCCESS;
}

NTSTATUS HumSetIdle(PDEVICE_OBJECT pDevObj)
{
    NTSTATUS                status;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PHID_DEVICE_EXTENSION   pDevExt;
    PURB                    pUrb;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    if (pMiniDevExt == NULL)
    {
        status = STATUS_NOT_FOUND;
    }
    else
    {
        USHORT UrbLen;

        UrbLen = sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
        pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
        if (pUrb == NULL)
        {
            //v2 = WPP_GLOBAL_Control;
            status = STATUS_NO_MEMORY;
        }
        else
        {
            memset(pUrb, 0, UrbLen);
            if (BooleanFlagOn(pMiniDevExt->Flags, DEXT_NO_HID_DESC))
            {
                pUrb->UrbHeader.Length = UrbLen;
                pUrb->UrbHeader.Function = URB_FUNCTION_CLASS_ENDPOINT;
                pUrb->UrbControlVendorClassRequest.Value = 0;
                pUrb->UrbControlVendorClassRequest.Index = 0;
                pUrb->UrbControlVendorClassRequest.RequestTypeReservedBits = 0x22;
                pUrb->UrbControlVendorClassRequest.Request = 0xA;
                pUrb->UrbControlVendorClassRequest.TransferFlags = 0;
                pUrb->UrbControlVendorClassRequest.TransferBuffer = 0;
                pUrb->UrbControlVendorClassRequest.TransferBufferLength = 0;
            }
            else
            {
                pUrb->UrbHeader.Length = UrbLen;
                pUrb->UrbHeader.Function = URB_FUNCTION_CLASS_INTERFACE;
                pUrb->UrbControlVendorClassRequest.Index = pMiniDevExt->pInterfaceInfo->InterfaceNumber;
                pUrb->UrbControlVendorClassRequest.RequestTypeReservedBits = 0x22;
                pUrb->UrbControlVendorClassRequest.Request = 0xA;
                pUrb->UrbControlVendorClassRequest.TransferFlags = 0;
                pUrb->UrbControlVendorClassRequest.TransferBuffer = 0;
                pUrb->UrbControlVendorClassRequest.TransferBufferLength = 0;
            }
            status = HumCallUSB(pDevObj, pUrb);
            ExFreePool(pUrb);

            //v2 = WPP_GLOBAL_Control;
        }
    }

    return status;
}

NTSTATUS HumGetDescriptorRequest(PDEVICE_OBJECT pDevObj, USHORT Function, CHAR DescriptorType, PVOID *pDescBuffer, PULONG pDescBuffLen, int Unused, CHAR Index, SHORT LangId)
{
    NTSTATUS status;
    PURB     pUrb;
    USHORT   UrbLen;
    BOOLEAN  BufferAllocated;

    UNREFERENCED_PARAMETER(Unused);
    BufferAllocated = FALSE;

    UrbLen = sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);
    pUrb = (URB *)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
    if (pUrb == NULL)
    {
        status = STATUS_NO_MEMORY;
    }
    else
    {
        memset(pUrb, 0, UrbLen);
        if (*pDescBuffer == NULL)
        {
            *pDescBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, *pDescBuffLen, HID_USB_TAG);
            BufferAllocated = TRUE;
            if (*pDescBuffer == NULL)
            {
                status = STATUS_NO_MEMORY;

                ExFreePool(pUrb);

                goto Exit;
            }
        }
        memset(*pDescBuffer, 0, *pDescBuffLen);
        pUrb->UrbHeader.Function = Function;
        pUrb->UrbHeader.Length = UrbLen;
        pUrb->UrbControlDescriptorRequest.TransferBufferLength = *pDescBuffLen;
        pUrb->UrbControlDescriptorRequest.TransferBufferMDL = 0;
        pUrb->UrbControlDescriptorRequest.TransferBuffer = *pDescBuffer;
        pUrb->UrbControlDescriptorRequest.DescriptorType = DescriptorType;
        pUrb->UrbControlDescriptorRequest.Index = Index;
        pUrb->UrbControlDescriptorRequest.LanguageId = LangId;
        pUrb->UrbControlDescriptorRequest.UrbLink = 0;

        status = HumCallUSB(pDevObj, pUrb);

        if (NT_SUCCESS(status) == FALSE)
        {

        }
        else
        {
            if (USBD_SUCCESS(pUrb->UrbHeader.Status) == TRUE)
            {
                status = STATUS_SUCCESS;
                *pDescBuffLen = pUrb->UrbControlTransfer.TransferBufferLength;
                ExFreePool(pUrb);
                goto Exit;
            }

            status = STATUS_UNSUCCESSFUL;
        }
        if (BufferAllocated == TRUE)
        {
            ExFreePool(*pDescBuffer);
            *pDescBuffLen = 0;
        }
        *pDescBuffLen = 0;
        ExFreePool(pUrb);
    }

Exit:

    return status;
}

NTSTATUS HumCallUsbComplete(PDEVICE_OBJECT pDeviceObject, PIRP pIrp, PVOID Context)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    UNREFERENCED_PARAMETER(pIrp);

    KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS HumCallUSB(PDEVICE_OBJECT pDevObj, PURB pUrb)
{
    NTSTATUS              status;
    PIRP                  pIrp;
    IO_STATUS_BLOCK       IoStatusBlock;
    PHID_DEVICE_EXTENSION pDevExt;
    KEVENT                Event;


    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    pIrp = IoBuildDeviceIoControlRequest(
        IOCTL_INTERNAL_USB_SUBMIT_URB,
        pDevExt->NextDeviceObject,
        NULL,
        0,
        NULL,
        0,
        TRUE,
        &Event,
        &IoStatusBlock);
    if (pIrp == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        status = IoSetCompletionRoutineEx(pDevObj, pIrp, HumCallUsbComplete, &Event, TRUE, TRUE, TRUE);
        if (NT_SUCCESS(status) == FALSE)
        {
            IofCompleteRequest(pIrp, IO_NO_INCREMENT);
        }
        else
        {
            IoGetNextIrpStackLocation(pIrp)->Parameters.Others.Argument1 = pUrb;
            status = IofCallDriver(pDevExt->NextDeviceObject, pIrp);

            if (status == STATUS_PENDING)
            {
                NTSTATUS StatusWait;
                StatusWait = KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, &UrbTimeout);
                if (StatusWait == STATUS_TIMEOUT)
                {
                    IoCancelIrp(pIrp);

                    StatusWait = KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
                    IoStatusBlock.Status = STATUS_IO_TIMEOUT;
                }

            }
            IofCompleteRequest(pIrp, IO_NO_INCREMENT);
            if (status == STATUS_PENDING)
            {
                status = IoStatusBlock.Status;
            }
        }
    }

    return status;
}

NTSTATUS HumInitDevice(PDEVICE_OBJECT pDevObj)
{
    NTSTATUS                      status;
    PUSB_CONFIGURATION_DESCRIPTOR pConfigDescr;
    ULONG                         DescLen;
    PHID_MINI_DEV_EXTENSION       pMiniDevExt;
    PHID_DEVICE_EXTENSION         pDevExt;


    RegDebug(L"HumInitDevice start", NULL, 1);
    runtimes_IOCTL_HID_READ_REPORT = 0;
    runtimes_IOCTL_IOCTL = 0;
    runtimes_ioReadCompletion = 0;

    pConfigDescr = NULL;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    status = HumGetDeviceDescriptor(pDevObj, pMiniDevExt);
    if (NT_SUCCESS(status) == FALSE)
    {
        RegDebug(L"HumInitDevice HumGetDeviceDescriptor err", NULL, status);
    }
    else
    {
        status = HumGetConfigDescriptor(pDevObj, &pConfigDescr, &DescLen);
        if (NT_SUCCESS(status) == FALSE)
        {
        }
        else
        {
            status = HumGetHidInfo(pDevObj, (UINT_PTR)pConfigDescr, DescLen);
            if (NT_SUCCESS(status) == FALSE)
            {
                /*
                v7 = "PNP_HIDInfo";
                */
            }
            else
            {
                status = HumSelectConfiguration(pDevObj, pConfigDescr);
                if (NT_SUCCESS(status))
                {
                    HumSetIdle(pDevObj);
                }
                else
                {
                    /*
                    v7 = "PNP_Configuration";
                    TransferrBufferLen = status;
                    v22 = 0;
                    v23 = 4;
                    pMiniDevExt = (HID_MINI_DEV_EXTENSION *)pMiniDevExt->pSelf;
                    v21 = &TransferrBufferLen;
                    v24 = 0;
                    v25 = &pMiniDevExt;
                    v26 = 0;
                    v27 = 4;
                    v28 = 0;
                    _TlgCreateSz((const CHAR **)&v29, v7);
                    _TlgWrite(v8, (unsigned __int8 *)&unk_1521F, v11, v12, 5, &v20);
                    ExFreePool(pConfigDescr);
                    return status;
                    */
                }
            }

            ExFreePool(pConfigDescr);
        }
    }
    return status;
}

NTSTATUS HumGetHidInfo(PDEVICE_OBJECT pDevObj, UINT_PTR pConfigDescr, UINT_PTR TransferrBufferLen)
{
    NTSTATUS                  status;
    PHID_DEVICE_EXTENSION     pDevExt;
    PHID_MINI_DEV_EXTENSION   pMiniDevExt;
    PUSB_INTERFACE_DESCRIPTOR pInterfaceDesc;
    PUSB_INTERFACE_DESCRIPTOR pHidDesc;
    BOOLEAN                   IsHIDClass;

    status = STATUS_SUCCESS;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    RtlZeroMemory(&pMiniDevExt->HidDesc, sizeof(pMiniDevExt->HidDesc));
    pInterfaceDesc = (PUSB_INTERFACE_DESCRIPTOR)USBD_ParseConfigurationDescriptorEx(
        (PUSB_CONFIGURATION_DESCRIPTOR)pConfigDescr,
        (PVOID)pConfigDescr,
        -1,
        -1,
        USB_DEVICE_CLASS_HUMAN_INTERFACE,
        -1,
        -1);
    if (pInterfaceDesc == NULL)
    {
    }
    else
    {
        IsHIDClass = pInterfaceDesc->bInterfaceClass == USB_DEVICE_CLASS_HUMAN_INTERFACE;
        pHidDesc = NULL;
        if (IsHIDClass)
        {
            HumParseHidInterface(
                pMiniDevExt,
                pInterfaceDesc,
                (LONG)(TransferrBufferLen + pConfigDescr - (UINT_PTR)pInterfaceDesc),
                &pHidDesc);
            if (pHidDesc != NULL)
            {
                RtlCopyMemory(&pMiniDevExt->HidDesc, pHidDesc, sizeof(pMiniDevExt->HidDesc));
                goto Exit;
            }
        }

    }

    status = STATUS_UNSUCCESSFUL;
Exit:

    return status;
}

NTSTATUS HumSelectConfiguration(PDEVICE_OBJECT pDevObj, PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc)
{
    NTSTATUS                    status;
    PURB                        pUrb;
    PUSBD_INTERFACE_INFORMATION pInterfaceInfo;
    PUSBD_INTERFACE_INFORMATION pBuff;
    USBD_INTERFACE_LIST_ENTRY   InterfaceList[2];
    PHID_MINI_DEV_EXTENSION     pMiniDevExt;
    PHID_DEVICE_EXTENSION       pDevExt;

    pInterfaceInfo = NULL;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    InterfaceList[0].InterfaceDescriptor = USBD_ParseConfigurationDescriptorEx(
        pConfigDesc,
        pConfigDesc,
        -1,
        -1,
        USB_DEVICE_CLASS_HUMAN_INTERFACE,
        -1,
        -1);
    InterfaceList[1].InterfaceDescriptor = NULL;
    if (InterfaceList[0].InterfaceDescriptor == NULL)
    {
        status = STATUS_INVALID_PARAMETER;
    }
    else
    {
        pUrb = (URB *)USBD_CreateConfigurationRequestEx(pConfigDesc, &InterfaceList[0]);
        if (pUrb == NULL)
        {
            status = STATUS_NO_MEMORY;
        }
        else
        {
            status = HumCallUSB(pDevObj, pUrb);
            if (NT_SUCCESS(status) != TRUE)
            {
                pMiniDevExt->UsbdConfigurationHandle = 0;
            }
            else
            {
                pInterfaceInfo = &pUrb->UrbSelectConfiguration.Interface;
                pMiniDevExt->UsbdConfigurationHandle = pUrb->UrbSelectConfiguration.ConfigurationHandle;
            }

            if (NT_SUCCESS(status))
            {
                pBuff = (USBD_INTERFACE_INFORMATION *)ExAllocatePoolWithTag(NonPagedPoolNx, pInterfaceInfo->Length, HID_USB_TAG);
                pMiniDevExt->pInterfaceInfo = pBuff;
                if (pBuff)
                {
                    memcpy(pBuff, pInterfaceInfo, pInterfaceInfo->Length);
                }
                else
                {
                    status = STATUS_NO_MEMORY;
                }
            }

            ExFreePool(pUrb);
        }
    }

    return status;
}

NTSTATUS HumParseHidInterface(HID_MINI_DEV_EXTENSION *pMiniDevExt, PUSB_INTERFACE_DESCRIPTOR pInterface, LONG TotalLength, PUSB_INTERFACE_DESCRIPTOR *ppHidDesc)
{
    NTSTATUS                  status;
    ULONG                     EndptIndex;
    LONG                      Remain;
    PUSB_INTERFACE_DESCRIPTOR pDesc;

    *ppHidDesc = NULL;
    pDesc = pInterface;
    Remain = TotalLength;
    EndptIndex = 0;

    if (Remain < 9)
    {
        goto Exit;
    }

    if (pDesc->bLength < 9)
    {
        goto Exit;
    }

    Remain -= pDesc->bLength;
    if (Remain < 2)
    {
    }

    pMiniDevExt->Flags &= ~DEXT_NO_HID_DESC;
    pDesc = (PUSB_INTERFACE_DESCRIPTOR)((UINT_PTR)pDesc + pDesc->bLength);
    if (pDesc->bLength < 2)
    {
        goto Exit;
    }

    if (pDesc->bDescriptorType != 0x21)
    {
        pMiniDevExt->Flags |= DEXT_NO_HID_DESC;
    }
    else
    {
        if (pDesc->bLength != 9)
        {
            goto Exit;
        }
        *ppHidDesc = pDesc;
        Remain -= pDesc->bLength;
        if (Remain < 0)
        {
            goto Exit;
        }
        pDesc = (PUSB_INTERFACE_DESCRIPTOR)((UINT_PTR)pDesc + pDesc->bLength);
    }

    if (pInterface->bNumEndpoints != 0)
    {
        for (;;)
        {
            if (Remain < 2)
            {
                goto Exit;
            }
            if (pDesc->bDescriptorType == 5)
            {
                if (pDesc->bLength != 7)
                {
                    goto Exit;
                }
                EndptIndex++;
            }
            else
            {
                if (pDesc->bDescriptorType == 4)
                {
                    goto Exit;
                }
            }

            if (pDesc->bLength == 0)
            {
                goto Exit;
            }
            Remain -= pDesc->bLength;
            if (Remain < 0)
            {
                goto Exit;
            }
            pDesc = (PUSB_INTERFACE_DESCRIPTOR)((UINT_PTR)pDesc + pDesc->bLength);
            if (EndptIndex == pInterface->bNumEndpoints)
            {
                break;
            }
        }
    }

    if (BooleanFlagOn(pMiniDevExt->Flags, DEXT_NO_HID_DESC) == FALSE)
    {
        if (*ppHidDesc == NULL)
        {
            goto ExitFail;
        }
        else
        {
            //Log desc
        }
    }
    else
    {
        if (Remain < 2)
        {
            goto Exit;
        }
        if (pDesc->bDescriptorType == 0x21)
        {
            *ppHidDesc = pDesc;
        }
        else
        {
            if (pDesc->bLength != 9)
            {

            }
            else
            {
                *ppHidDesc = pDesc;
            }
        }

        if (Remain >= pDesc->bLength)
        {
            if (*ppHidDesc == NULL)
            {
                goto ExitFail;
            }
            else
            {
                //Log desc
            }
        }
        else
        {
            if (Remain < 9)
            {
                *ppHidDesc = NULL;
            }
            else
            {
            }
        }
    }

Exit:
    if (*ppHidDesc == NULL)
    {
    ExitFail:

        status = STATUS_UNSUCCESSFUL;
    }
    else
    {
        if (pInterface->bNumEndpoints == EndptIndex)
        {
            status = STATUS_SUCCESS;
        }
        else
        {
            status = STATUS_UNSUCCESSFUL;
        }
    }

    return status;
}

NTSTATUS HumGetConfigDescriptor(PDEVICE_OBJECT pDevObj, PUSB_CONFIGURATION_DESCRIPTOR *ppConfigDesc, PULONG pConfigDescLen)
{
    NTSTATUS                      status;
    ULONG                         TotalLength;
    ULONG                         ConfigDescLen;
    PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc;

    pConfigDesc = NULL;
    ConfigDescLen = sizeof(*pConfigDesc);
    status = HumGetDescriptorRequest(
        pDevObj,
        URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE,
        USB_CONFIGURATION_DESCRIPTOR_TYPE,
        &pConfigDesc,
        &ConfigDescLen,
        0,
        0,
        0);
    if (NT_SUCCESS(status) == FALSE)
    {
    }
    else
    {
        if (ConfigDescLen < 9)
        {
            return STATUS_DEVICE_DATA_ERROR;
        }
        TotalLength = pConfigDesc->wTotalLength;
        ExFreePool(pConfigDesc);
        if (TotalLength == 0)
        {
            return STATUS_DEVICE_DATA_ERROR;
        }
        pConfigDesc = NULL;
        ConfigDescLen = TotalLength;
        status = HumGetDescriptorRequest(
            pDevObj,
            URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE,
            USB_CONFIGURATION_DESCRIPTOR_TYPE,
            &pConfigDesc,
            &ConfigDescLen,
            0,
            0,
            0);

        if (NT_SUCCESS(status) == FALSE)
        {
        }
        else
        {
            if (!ConfigDescLen || ConfigDescLen < 9)
            {
                if (pConfigDesc)
                {
                    ExFreePool(pConfigDesc);
                }

                return STATUS_DEVICE_DATA_ERROR;
            }
            if (pConfigDesc->wTotalLength > TotalLength)
            {
                pConfigDesc->wTotalLength = (USHORT)TotalLength;
            }

            if (pConfigDesc->bLength < 9)
            {
                pConfigDesc->bLength = 9;
            }
        }
    }

    *ppConfigDesc = pConfigDesc;
    *pConfigDescLen = ConfigDescLen;

    return status;
}

NTSTATUS HumGetDeviceDescriptor(PDEVICE_OBJECT pDevObj, PHID_MINI_DEV_EXTENSION pMiniDevExt)
{
    NTSTATUS status;
    ULONG    DeviceDescLen;

    DeviceDescLen = sizeof(USB_DEVICE_DESCRIPTOR);

    status = HumGetDescriptorRequest(
        pDevObj,
        URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE,
        USB_DEVICE_DESCRIPTOR_TYPE,
        &pMiniDevExt->pDevDesc,
        &DeviceDescLen,
        0,
        0,
        0);
    if (NT_SUCCESS(status) == FALSE)
    {

    }
    else
    {
    }

    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    NTSTATUS                    status;
    HID_MINIDRIVER_REGISTRATION HidReg;
    PDRIVER_EXTENSION           pDrvExt;

    pDrvExt = pDrvObj->DriverExtension;
    pDrvObj->MajorFunction[IRP_MJ_CREATE] = (PDRIVER_DISPATCH)HumCreateClose;
    pDrvObj->MajorFunction[IRP_MJ_CLOSE] = (PDRIVER_DISPATCH)HumCreateClose;//原代码错误修正
    pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = (PDRIVER_DISPATCH)HumInternalIoctl;
    pDrvObj->MajorFunction[IRP_MJ_PNP] = HumPnP;
    pDrvObj->MajorFunction[IRP_MJ_POWER] = HumPower;
    pDrvObj->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = (PDRIVER_DISPATCH)HumSystemControl;
    pDrvExt->AddDevice = (PDRIVER_ADD_DEVICE)HumAddDevice;
    pDrvObj->DriverUnload = (PDRIVER_UNLOAD)HumUnload;

    HidReg.Revision = HID_REVISION;
    HidReg.DriverObject = pDrvObj;
    HidReg.RegistryPath = pRegPath;
    //HidReg.DeviceExtensionSize = 84;//0x54, sizeof()
    HidReg.DeviceExtensionSize = sizeof(HID_MINI_DEV_EXTENSION);
    HidReg.DevicesArePolled = FALSE;
    status = HidRegisterMinidriver(&HidReg);


    //KeInitializeSpinLock(&resetWorkItemsListSpinLock);
    if (NT_SUCCESS(status) == FALSE)
    {
    }
    return status;
}

NTSTATUS HumAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pFdo)
{
    PHID_DEVICE_EXTENSION   pDevExt;
    HID_MINI_DEV_EXTENSION *pMiniDevExt;

    UNREFERENCED_PARAMETER(pDrvObj);

    pDevExt = (PHID_DEVICE_EXTENSION)pFdo->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    pMiniDevExt->Flags = 0;
    pMiniDevExt->PendingRequestsCount = 0;
    KeInitializeEvent(&pMiniDevExt->Event, NotificationEvent, FALSE);
    pMiniDevExt->pWorkItem = NULL;
    pMiniDevExt->PnpState = 0;
    pMiniDevExt->pFdo = pFdo;
    IoInitializeRemoveLockEx(&pMiniDevExt->RemoveLock, HID_USB_TAG, 2, 0, sizeof(pMiniDevExt->RemoveLock));

    return STATUS_SUCCESS;
}

NTSTATUS HumCreateClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(pDevObj);
    status = STATUS_SUCCESS;

    if (pIrp->Tail.Overlay.CurrentStackLocation->MajorFunction == IRP_MJ_CREATE)
    {
    }
    else
    {
        if (pIrp->Tail.Overlay.CurrentStackLocation->MajorFunction != IRP_MJ_CLOSE)
        {

            status = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

    }


    pIrp->IoStatus.Information = 0;
Exit:
    pIrp->IoStatus.Status = status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return status;
}

VOID HumUnload(PDRIVER_OBJECT pDrvObj)
{
    UNREFERENCED_PARAMETER(pDrvObj);
}

VOID HumSetIdleWorker(PDEVICE_OBJECT DeviceObject, PVOID Context)
{
    HumSetIdle(DeviceObject);
    IoFreeWorkItem((PIO_WORKITEM)Context);
}

PUSBD_PIPE_INFORMATION GetInterruptInputPipeForDevice(PHID_MINI_DEV_EXTENSION pMiniDevExt)
{
    ULONG                       Index;
    ULONG                       NumOfPipes;
    PUSBD_PIPE_INFORMATION      pPipeInfo;
    PUSBD_INTERFACE_INFORMATION pInterfaceInfo;

    pInterfaceInfo = pMiniDevExt->pInterfaceInfo;
    Index = 0;
    NumOfPipes = pInterfaceInfo->NumberOfPipes;
    if (NumOfPipes == 0)
    {
        return NULL;
    }
    pPipeInfo = &pInterfaceInfo->Pipes[0];
    while ((USBD_PIPE_DIRECTION_IN(pPipeInfo) == FALSE) || (pPipeInfo->PipeType != UsbdPipeTypeInterrupt))
    {
        ++Index;
        ++pPipeInfo;
        if (Index >= NumOfPipes)
        {
            return NULL;
        }
    }
    return &pInterfaceInfo->Pipes[Index];
}

LONG HumDecrementPendingRequestCount(PHID_MINI_DEV_EXTENSION pMiniDevExt)
{
    LONG result;

    result = _InterlockedDecrement(&pMiniDevExt->PendingRequestsCount);
    if (result < 0)
    {
        result = KeSetEvent(&pMiniDevExt->Event, IO_NO_INCREMENT, FALSE);
    }
    return result;
}

NTSTATUS HumGetPortStatus(PDEVICE_OBJECT pDevObj, PULONG pOut)
{
    NTSTATUS              status;
    PIRP                  pIrp;
    KEVENT                Event;
    IO_STATUS_BLOCK       IoStatus;
    PHID_DEVICE_EXTENSION pDevExt;
    PIO_STACK_LOCATION    pStack;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    *pOut = 0;
    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    pIrp = IoBuildDeviceIoControlRequest(
        IOCTL_INTERNAL_USB_GET_PORT_STATUS,
        pDevExt->NextDeviceObject,
        NULL,
        0,
        NULL,
        0,
        TRUE,
        &Event,
        &IoStatus);
    if (pIrp == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    pStack = IoGetNextIrpStackLocation(pIrp);
    pStack->Parameters.Others.Argument1 = pOut;
    status = IoCallDriver(pDevExt->NextDeviceObject, pIrp);
    if (status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
        status = IoStatus.Status;
    }
    return status;
}

NTSTATUS HumGetSetReport(PDEVICE_OBJECT pDevObj, PIRP pIrp, PBOOLEAN pNeedToCompleteIrp)
{
    NTSTATUS                status;
    USHORT                  UrbLen;
    LONG                    TransferFlags;
    PVOID                   pUserBuffer;
    PURB                    pUrb;
    USHORT                  Value;
    SHORT                   Index;
    PIO_STACK_LOCATION      pStack;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    SHORT                   Offset;
    CHAR                    Request;
    PHID_DEVICE_EXTENSION   pDevExt;
    PHID_XFER_PACKET        pTransferPacket;
    PIO_STACK_LOCATION      pNextStack;

    TransferFlags = Offset = Request = 0; //Make compiler happy//错误修正，原本代码位置错误，更换到目前位置才是正确的

    status = STATUS_SUCCESS;
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    switch (pStack->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_HID_SET_FEATURE:
        TransferFlags = 0;
        Request = 9;
        Offset = 0x300;
        break;
    case IOCTL_HID_GET_FEATURE:
        Offset = 0x300;
        TransferFlags = 1;
        Request = 1;
        break;
    case IOCTL_HID_SET_OUTPUT_REPORT:
        TransferFlags = 0;
        Request = 9;
        Offset = 0x200;
        break;
    case IOCTL_HID_GET_INPUT_REPORT:
        Offset = 0x100;
        TransferFlags = 1;
        Request = 1;
        break;
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (NT_SUCCESS(status) == FALSE)
    {
        goto Exit;
    }


    pUserBuffer = pIrp->UserBuffer;
    pTransferPacket = (PHID_XFER_PACKET)pUserBuffer;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    if (pTransferPacket == NULL || pTransferPacket->reportBuffer == NULL || pTransferPacket->reportBufferLen == 0)
    {
        status = STATUS_DATA_ERROR;
    }
    else
    {
        UrbLen = sizeof(URB);
        pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
        if (pUrb == NULL)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        else
        {
            memset(pUrb, 0, UrbLen);
            Value = pTransferPacket->reportId + Offset;
            if (pTransferPacket->reportId == 0x8) {
                RegDebug(L"GetFeatureValue=", NULL, Value);
            }
            if (pTransferPacket->reportId == 0xe) {
                RegDebug(L"GetFeaturePTPcertValue=", NULL, Value);
            }
            if (pTransferPacket->reportId == 0xb) {
                RegDebug(L"SetFeatureInputModeValue=", NULL, Value);
                pTransferPacket->reportBuffer[0] = 0x0;
            }
            if (pTransferPacket->reportId == 0xc) {
                RegDebug(L"SetFeatureFunswitchValue=", NULL, Value);
            }
            if (BooleanFlagOn(pMiniDevExt->Flags, DEXT_NO_HID_DESC))
            {
                pUrb->UrbHeader.Length = sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
                pUrb->UrbHeader.Function = URB_FUNCTION_CLASS_ENDPOINT;
                Index = 1;
            }
            else
            {
                pUrb->UrbHeader.Length = sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST);
                pUrb->UrbHeader.Function = URB_FUNCTION_CLASS_INTERFACE;
                Index = pMiniDevExt->pInterfaceInfo->InterfaceNumber;
            }
            pUrb->UrbControlVendorClassRequest.Index = Index;
            pUrb->UrbControlVendorClassRequest.TransferFlags = TransferFlags;
            pUrb->UrbControlVendorClassRequest.RequestTypeReservedBits = 0x22;
            pUrb->UrbControlVendorClassRequest.Request = Request;
            pUrb->UrbControlVendorClassRequest.Value = Value;
            pUrb->UrbControlVendorClassRequest.TransferBuffer = pTransferPacket->reportBuffer;
            pUrb->UrbControlVendorClassRequest.TransferBufferLength = pTransferPacket->reportBufferLen;

            IoSetCompletionRoutine(pIrp, HumGetSetReportCompletion, pUrb, TRUE, TRUE, TRUE);

            pNextStack = IoGetNextIrpStackLocation(pIrp);
            pNextStack->Parameters.Others.Argument1 = pUrb;
            pNextStack->MajorFunction = pStack->MajorFunction;
            pNextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
            pNextStack->DeviceObject = pDevExt->NextDeviceObject;

            if (NT_SUCCESS(HumIncrementPendingRequestCount(pMiniDevExt)) == FALSE)
            {
                ExFreePool(pUrb);
                status = STATUS_NO_SUCH_DEVICE;
            }
            else
            {
                status = IoCallDriver(pDevExt->NextDeviceObject, pIrp);
                *pNeedToCompleteIrp = FALSE;
            }
        }
    }

Exit:
    return status;
}

NTSTATUS HumGetSetReportCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext)
{
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PHID_DEVICE_EXTENSION   pDevExt;
    PURB                    pUrb;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    pUrb = (PURB)pContext;

    if (NT_SUCCESS(pIrp->IoStatus.Status) == TRUE)
    {
        pIrp->IoStatus.Information = pUrb->UrbControlTransfer.TransferBufferLength;
    }

    ExFreePool(pUrb);
    if (pIrp->PendingReturned)
    {
        IoMarkIrpPending(pIrp);
    }
    HumDecrementPendingRequestCount(pMiniDevExt);
    IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));

    return STATUS_SUCCESS;
}

NTSTATUS HumIncrementPendingRequestCount(PHID_MINI_DEV_EXTENSION pMiniDevExt)
{
    InterlockedIncrement(&pMiniDevExt->PendingRequestsCount);
    if (pMiniDevExt->PnpState == 2 || pMiniDevExt->PnpState == 1)
    {
        return 0;
    }
    HumDecrementPendingRequestCount(pMiniDevExt);
    return STATUS_NO_SUCH_DEVICE;
}

NTSTATUS HumQueueResetWorkItem(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS                status;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PWRKITM_RESET_CONTEXT   pContext;
    PIO_WORKITEM            pWorkItem;
    PHID_DEVICE_EXTENSION   pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    if (NT_SUCCESS(HumIncrementPendingRequestCount(pMiniDevExt)) == FALSE)
    {
        return 0;
    }
    pContext = (WRKITM_RESET_CONTEXT *)ExAllocatePoolWithTag(NonPagedPoolNx, sizeof(WRKITM_RESET_CONTEXT), HID_USB_TAG);
    if (pContext)
    {
        pWorkItem = IoAllocateWorkItem(pMiniDevExt->pFdo);
        pContext->pWorkItem = pWorkItem;
        if (pWorkItem)
        {
            if (InterlockedCompareExchangePointer(
                &pMiniDevExt->pWorkItem,
                &pContext->pWorkItem,
                0) == 0)
            {
                pContext->pDeviceObject = pDevObj;
                pContext->Tag = HID_RESET_TAG;
                pContext->pIrp = pIrp;
                pIrp->Tail.Overlay.CurrentStackLocation->Control |= 1u;
                IoAcquireRemoveLockEx(&pMiniDevExt->RemoveLock, (PVOID)HID_REMLOCK_TAG, __FILE__, __LINE__, sizeof(pMiniDevExt->RemoveLock));
                IoQueueWorkItem(pContext->pWorkItem, HumResetWorkItem, DelayedWorkQueue, pContext);
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
            IoFreeWorkItem(pContext->pWorkItem);
        }
        ExFreePool(pContext);
        HumDecrementPendingRequestCount(pMiniDevExt);
        status = 0;
    }
    else
    {
        HumDecrementPendingRequestCount(pMiniDevExt);
        status = 0;
    }
    return status;
}

NTSTATUS HumResetInterruptPipe(PDEVICE_OBJECT pDevObj)
{
    NTSTATUS                status;
    USHORT                  UrbLen;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PURB                    pUrb;
    PUSBD_PIPE_INFORMATION  pPipeInfo;
    PHID_DEVICE_EXTENSION   pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    UrbLen = sizeof(struct _URB_PIPE_REQUEST);
    pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
    if (pUrb == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pUrb->UrbHeader.Length = UrbLen;
    pUrb->UrbHeader.Function = URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL;
    pPipeInfo = GetInterruptInputPipeForDevice(pMiniDevExt);
    if (pPipeInfo != NULL)
    {
        pUrb->UrbPipeRequest.PipeHandle = pPipeInfo->PipeHandle;
        status = HumCallUSB(pDevObj, pUrb);
        ExFreePool(pUrb);
    }
    else
    {
        ExFreePool(pUrb);
        status = STATUS_INVALID_DEVICE_REQUEST;
    }
    return status;
}

NTSTATUS HumResetParentPort(PDEVICE_OBJECT pDevObj)
{
    NTSTATUS              status;
    PIRP                  pIrp;
    KEVENT                Event;
    IO_STATUS_BLOCK       IoStatus;
    PHID_DEVICE_EXTENSION pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    pIrp = IoBuildDeviceIoControlRequest(
        IOCTL_INTERNAL_USB_RESET_PORT,
        pDevExt->NextDeviceObject,
        NULL,
        0,
        NULL,
        0,
        TRUE,
        &Event,
        &IoStatus);
    if (pIrp == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    status = IoCallDriver(pDevExt->NextDeviceObject, pIrp);
    if (status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
        status = IoStatus.Status;
    }

    return status;
}

VOID HumResetWorkItem(PDEVICE_OBJECT pDevObj, PWRKITM_RESET_CONTEXT pContext)
{
    NTSTATUS                status;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    ULONG                   PortStatus;
    PHID_DEVICE_EXTENSION   pDevExt;

    UNREFERENCED_PARAMETER(pDevObj);

    PortStatus = 0;
    pDevExt = (PHID_DEVICE_EXTENSION)pContext->pDeviceObject->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    if (NT_SUCCESS(HumIncrementPendingRequestCount(pMiniDevExt) == TRUE))
    {
        status = HumGetPortStatus(pContext->pDeviceObject, &PortStatus);
        if (NT_SUCCESS(status) == FALSE)
        {
            HumDecrementPendingRequestCount(pMiniDevExt);
            goto Exit;
        }
        if ((PortStatus & USBD_PORT_CONNECTED) == 0)
        {
            HumDecrementPendingRequestCount(pMiniDevExt);
        }
        else
        {
            status = HumAbortPendingRequests(pContext->pDeviceObject);
            if (NT_SUCCESS(status) == FALSE)
            {
            }
            else
            {
                status = HumResetParentPort(pContext->pDeviceObject);
                if (status == STATUS_DEVICE_DATA_ERROR)
                {
                    pMiniDevExt->PnpState = 6;
                    IoInvalidateDeviceState(pDevExt->PhysicalDeviceObject);

                    HumDecrementPendingRequestCount(pMiniDevExt);
                    goto Exit;
                }
                if (NT_SUCCESS(status))
                {
                }
                else
                {
                }
            }
            if (NT_SUCCESS(status))
            {
                status = HumResetInterruptPipe(pContext->pDeviceObject);
            }
            HumDecrementPendingRequestCount(pMiniDevExt);
        }
    }

Exit:
    InterlockedExchangePointer(&pMiniDevExt->pWorkItem, NULL);

    IoCompleteRequest(pContext->pIrp, IO_NO_INCREMENT);
    IoFreeWorkItem(pContext->pWorkItem);
    ExFreePool(pContext);
    HumDecrementPendingRequestCount(pMiniDevExt);
    IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, (PVOID)HID_REMLOCK_TAG, sizeof(pMiniDevExt->RemoveLock));
}

NTSTATUS HumGetStringDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS                status;
    PIO_STACK_LOCATION      pStack;
    ULONG                   OutBuffLen;
    ULONG_PTR               Type3InputBuffer;
    PUSB_DEVICE_DESCRIPTOR  pDevDesc;
    ULONG                   DescBuffLen;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PVOID                   pUserBuffer;
    CHAR                    Index;
    ULONG                   IoControlCode;
    USHORT                  LangId;
    USHORT                  GetStrCtlCode;
    BOOLEAN                 Mapped;
    PUSB_STRING_DESCRIPTOR  pStrDesc;
    PHID_DEVICE_EXTENSION   pDevExt;

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    Mapped = FALSE;
    IoControlCode = pStack->Parameters.DeviceIoControl.IoControlCode;
    if (IoControlCode == IOCTL_HID_GET_STRING)
    {
        pUserBuffer = pIrp->UserBuffer;
    }
    else
    {
        if (IoControlCode != 0xB01E2) //XXX Undoc?
        {
            pUserBuffer = NULL;
        }
        else
        {
            if (pIrp->MdlAddress == NULL)
            {
                pUserBuffer = NULL;
            }
            else
            {
                pUserBuffer = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, MdlMappingNoExecute | NormalPagePriority);
                Mapped = TRUE;
            }
        }
    }

    OutBuffLen = pStack->Parameters.DeviceIoControl.OutputBufferLength;
    if (pUserBuffer == NULL || OutBuffLen < 2)
    {
        return STATUS_INVALID_USER_BUFFER;
    }

    Type3InputBuffer = (ULONG_PTR)pStack->Parameters.DeviceIoControl.Type3InputBuffer;
    LangId = (USHORT)(Type3InputBuffer >> 0x10);
    GetStrCtlCode = Type3InputBuffer & 0xFFFF;
    Index = (CHAR)GetStrCtlCode;
    if (Mapped == FALSE)
    {
        switch (GetStrCtlCode)
        {
        case HID_STRING_ID_IPRODUCT:
            pDevDesc = pMiniDevExt->pDevDesc;
            Index = pDevDesc->iProduct;
            if (pDevDesc->iProduct == 0 || pDevDesc->iProduct == -1)
            {
                return STATUS_INVALID_PARAMETER;
            }
            break;
        case HID_STRING_ID_IMANUFACTURER:
            pDevDesc = pMiniDevExt->pDevDesc;
            Index = pDevDesc->iManufacturer;
            if (pDevDesc->iManufacturer == 0 || pDevDesc->iManufacturer == -1)
            {
                return STATUS_INVALID_PARAMETER;
            }
            break;
        case HID_STRING_ID_ISERIALNUMBER:
            pDevDesc = pMiniDevExt->pDevDesc;
            Index = pDevDesc->iSerialNumber;
            if (pDevDesc->iSerialNumber == 0 || pDevDesc->iSerialNumber == -1)
            {
                return STATUS_INVALID_PARAMETER;
            }
            break;
        default:
            return STATUS_INVALID_PARAMETER;
        }
    }

    DescBuffLen = OutBuffLen + 2;
    pStrDesc = (PUSB_STRING_DESCRIPTOR)ExAllocatePoolWithTag(NonPagedPoolNx, DescBuffLen, HID_USB_TAG);
    if (pStrDesc == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = HumGetDescriptorRequest(pDevObj, URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE, USB_STRING_DESCRIPTOR_TYPE, (PVOID *)&pStrDesc, &DescBuffLen, 0, Index, LangId);
    if (NT_SUCCESS(status) == TRUE)
    {
        ULONG Length;

        Length = pStrDesc->bLength;
        Length -= 2;
        if (Length > DescBuffLen)
        {
            Length = DescBuffLen;
        }
        Length &= 0xFFFFFFFE;
        if (Length >= OutBuffLen - 2)
        {
            status = STATUS_INVALID_BUFFER_SIZE;
        }
        else
        {
            PWCHAR p;
            RtlCopyMemory(pUserBuffer, &pStrDesc->bString, Length);

            p = (PWCHAR)((PCHAR)pUserBuffer + Length);
            *p = UNICODE_NULL;
            Length += 2;
            pIrp->IoStatus.Information = Length;
        }
    }

    ExFreePool(pStrDesc);
    return status;
}

NTSTATUS HumPnP(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS                    status;
    PHID_MINI_DEV_EXTENSION     pMiniDevExt;
    ULONG                       PrevPnpState;
    UCHAR                       MinorFunction;
    PUSBD_INTERFACE_INFORMATION pInterfaceInfo;
    PIO_STACK_LOCATION          pStack;
    KEVENT                      Event;
    PHID_DEVICE_EXTENSION       pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    status = IoAcquireRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, __FILE__, __LINE__, sizeof(pMiniDevExt->RemoveLock));
    if (NT_SUCCESS(status) == FALSE)
    {
        pIrp->IoStatus.Status = status;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return status;
    }
    else
    {
        switch (pStack->MinorFunction)
        {
        case IRP_MN_START_DEVICE:
            PrevPnpState = pMiniDevExt->PnpState;
            pMiniDevExt->PnpState = 1;
            KeResetEvent(&pMiniDevExt->Event);
            if (PrevPnpState == 3 || PrevPnpState == 4 || PrevPnpState == 5)
            {
                HumIncrementPendingRequestCount(pMiniDevExt);
            }
            pMiniDevExt->pInterfaceInfo = 0;
            break;
        case IRP_MN_REMOVE_DEVICE:
            return HumRemoveDevice(pDevObj, pIrp);
        case IRP_MN_STOP_DEVICE:
            if (pMiniDevExt->PnpState != 2)
            {
                break;
            }
            status = HumStopDevice(pDevObj);
            if (NT_SUCCESS(status) == FALSE)
            {
                pIrp->IoStatus.Status = status;
                IoCompleteRequest(pIrp, IO_NO_INCREMENT);
                IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));
                return status;
            }
            break;
        case IRP_MN_QUERY_PNP_DEVICE_STATE:
            if (pMiniDevExt->PnpState == 6)
            {
                pIrp->IoStatus.Information |= PNP_DEVICE_FAILED;
            }
            break;
        default:
            break;
        }

        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        IoCopyCurrentIrpStackLocationToNext(pIrp);
        status = IoSetCompletionRoutineEx(pDevObj, pIrp, HumPnpCompletion, &Event, TRUE, TRUE, TRUE);
        if (NT_SUCCESS(status) == FALSE)
        {
            goto ExitUnlock;
        }
        if (IoCallDriver(pDevExt->NextDeviceObject, pIrp) == STATUS_PENDING)
        {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        }
        status = pIrp->IoStatus.Status;
        MinorFunction = pStack->MinorFunction;
        switch (MinorFunction)
        {
        case IRP_MN_STOP_DEVICE:
        {
            pInterfaceInfo = pMiniDevExt->pInterfaceInfo;
            pMiniDevExt->PnpState = 4;
            if (pInterfaceInfo)
            {
                ExFreePool(pInterfaceInfo);
                pMiniDevExt->pInterfaceInfo = NULL;
            }
            if (pMiniDevExt->pDevDesc)
            {
                ExFreePool(pMiniDevExt->pDevDesc);
                pMiniDevExt->pDevDesc = 0;
            }
            break;
        }
        case IRP_MN_QUERY_CAPABILITIES:
        {
            if (NT_SUCCESS(status) == TRUE)
            {
                pStack->Parameters.DeviceCapabilities.Capabilities->SurpriseRemovalOK = 1;
            }
            break;
        }
        case IRP_MN_START_DEVICE:
        {
            if (NT_SUCCESS(status) == FALSE)
            {
                pMiniDevExt->PnpState = 6;
            }
            else
            {
                pMiniDevExt->PnpState = 2;
                status = HumInitDevice(pDevObj);
                if (NT_SUCCESS(status) == FALSE)
                {
                    pMiniDevExt->PnpState = 6;
                }
            }
            break;
        }
        }
    ExitUnlock:
        pIrp->IoStatus.Status = status;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));
        return status;
    }
}

NTSTATUS HumGetReportDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp, PBOOLEAN pNeedToCompleteIrp)
{
    NTSTATUS                status;
    PIO_STACK_LOCATION      pStack;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    SIZE_T                  OutBuffLen;
    PVOID                   pReportDesc;
    ULONG                   ReportDescLen;
    PHID_DEVICE_EXTENSION   pDevExt;

    RegDebug(L"HumGetReportDescriptor start", NULL, 1);

    pReportDesc = NULL;
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    ReportDescLen = pMiniDevExt->HidDesc.DescriptorList[0].wReportLength + 0x40;
    if (BooleanFlagOn(pMiniDevExt->Flags, DEXT_NO_HID_DESC) == FALSE)
    {
        RegDebug(L"HumGetReportDescriptor DEXT_NO_HID_DESC", NULL, ReportDescLen);
        status = HumGetDescriptorRequest(
            pDevObj,
            URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE,
            pMiniDevExt->HidDesc.DescriptorList[0].bReportType,
            &pReportDesc,
            &ReportDescLen,
            0,
            0,
            pMiniDevExt->pInterfaceInfo->InterfaceNumber);
    }
    else
    {
        PUSBD_PIPE_INFORMATION pPipeInfo;

        pPipeInfo = GetInterruptInputPipeForDevice(pMiniDevExt);
        if (pPipeInfo == NULL)
        {
            pIrp->IoStatus.Status = STATUS_DEVICE_CONFIGURATION_ERROR;
            status = HumQueueResetWorkItem(pDevObj, pIrp);
            if (status == STATUS_MORE_PROCESSING_REQUIRED)
            {
                *pNeedToCompleteIrp = FALSE;
                IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));
                //RegDebug(L"HumGetReportDescriptor HumQueueResetWorkItem STATUS_PENDING", NULL, status);
                return STATUS_PENDING;
            }

            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
        else
        {
            UCHAR EndptAddr;

            EndptAddr = pPipeInfo->EndpointAddress;
            EndptAddr &= USB_ENDPOINT_DIRECTION_MASK;

            //RegDebug(L"HumGetReportDescriptor USB_ENDPOINT_DIRECTION_MASK", NULL, pPipeInfo->EndpointAddress);

            status = HumGetDescriptorRequest(
                pDevObj,
                URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT,
                pMiniDevExt->HidDesc.DescriptorList[0].bReportType,
                &pReportDesc,
                &ReportDescLen,
                0,
                0,
                EndptAddr);
        }
    }

    if (NT_SUCCESS(status) == FALSE)
    {
        if (status == STATUS_DEVICE_NOT_CONNECTED)
        {
            RegDebug(L"HumGetReportDescriptor STATUS_DEVICE_NOT_CONNECTED", NULL, status);
            return status;
        }
        else
        {
            NTSTATUS ResetStatus;

            pIrp->IoStatus.Status = status;
            ResetStatus = HumQueueResetWorkItem(pDevObj, pIrp);
            if (ResetStatus == STATUS_MORE_PROCESSING_REQUIRED)
            {
                *pNeedToCompleteIrp = FALSE;
                IoReleaseRemoveLockEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));
                RegDebug(L"HumGetReportDescriptor HumQueueResetWorkItem STATUS_PENDING22", NULL, status);
                return STATUS_PENDING;
            }

            RegDebug(L"HumGetReportDescriptor HumQueueResetWorkItem", NULL, status);
            return status;
        }
    }

    OutBuffLen = pStack->Parameters.DeviceIoControl.OutputBufferLength;
    if (OutBuffLen > pMiniDevExt->HidDesc.DescriptorList[0].wReportLength)
    {
        OutBuffLen = pMiniDevExt->HidDesc.DescriptorList[0].wReportLength;
    }
    if (OutBuffLen > ReportDescLen)
    {
        OutBuffLen = ReportDescLen;
    }

    RtlCopyMemory(pIrp->UserBuffer, pReportDesc, OutBuffLen);
    pIrp->IoStatus.Information = OutBuffLen;

    //RegDebug(L"HumGetReportDescriptor pNeedToCompleteIrp=", NULL, *pNeedToCompleteIrp);
    RegDebug(L"HumGetReportDescriptor pReportDesc", pReportDesc, (ULONG)OutBuffLen);
    ExFreePool(pReportDesc);

    RegDebug(L"HumGetReportDescriptor end", NULL, status);
    return status;
}

NTSTATUS HumGetDeviceAttributes(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PHID_DEVICE_EXTENSION   pDevExt;
    PHID_DEVICE_ATTRIBUTES  pDevAttrs;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PIO_STACK_LOCATION      pStack;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    pDevAttrs = (PHID_DEVICE_ATTRIBUTES)pIrp->UserBuffer;
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    if (pStack->Parameters.DeviceIoControl.OutputBufferLength < 0x20)
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }
    pIrp->IoStatus.Information = 0x20;
    pDevAttrs->Size = 0x20;
    pDevAttrs->VendorID = pMiniDevExt->pDevDesc->idVendor;
    pDevAttrs->ProductID = pMiniDevExt->pDevDesc->idProduct;
    pDevAttrs->VersionNumber = pMiniDevExt->pDevDesc->bcdDevice;

    return STATUS_SUCCESS;
}

NTSTATUS HumGetHidDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PHID_DEVICE_EXTENSION   pDevExt;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PHID_DESCRIPTOR         pHidDesc;
    UCHAR                   DescLen;
    ULONG                   OutBuffLen;
    PIO_STACK_LOCATION      pStack;

    //RegDebug(L"HumGetHidDescriptor start", NULL, 1);

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    pHidDesc = &pMiniDevExt->HidDesc;
    DescLen = pMiniDevExt->HidDesc.bLength;
    if (DescLen == 0)
    {
        pIrp->IoStatus.Information = 0;
        //RegDebug(L"HumGetHidDescriptor STATUS_INVALID_PARAMETER", NULL, 1);
        return STATUS_INVALID_PARAMETER;
    }
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    OutBuffLen = pStack->Parameters.DeviceIoControl.OutputBufferLength;
    if (OutBuffLen > DescLen)
    {
        OutBuffLen = DescLen;
    }
    memcpy(pIrp->UserBuffer, pHidDesc, OutBuffLen);
    pIrp->IoStatus.Information = OutBuffLen;

    //RegDebug(L"HumGetHidDescriptor pHidDesc=", pHidDesc, OutBuffLen);
    return STATUS_SUCCESS;
}

NTSTATUS HumAbortPendingRequests(PDEVICE_OBJECT pDevObj)
{
    NTSTATUS                    status;
    USHORT                      UrbLen;
    PHID_MINI_DEV_EXTENSION     pMiniDevExt;
    PURB                        pUrb;
    PUSBD_INTERFACE_INFORMATION pInterfaceInfo;
    USBD_PIPE_HANDLE            PipeHandle;
    PHID_DEVICE_EXTENSION       pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    UrbLen = sizeof(struct _URB_PIPE_REQUEST);
    pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
    if (pUrb == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    pInterfaceInfo = pMiniDevExt->pInterfaceInfo;
    if (pInterfaceInfo == NULL || pInterfaceInfo->NumberOfPipes == 0)
    {
        status = STATUS_NO_SUCH_DEVICE;
    }
    else
    {
        PipeHandle = pInterfaceInfo->Pipes[0].PipeHandle;
        if (PipeHandle)
        {
            status = STATUS_NO_SUCH_DEVICE;
        }
        else
        {
            pUrb->UrbHeader.Length = UrbLen;
            pUrb->UrbHeader.Function = URB_FUNCTION_ABORT_PIPE;
            pUrb->UrbPipeRequest.PipeHandle = PipeHandle;
            status = HumCallUSB(pDevObj, pUrb);
            if (NT_SUCCESS(status) == FALSE)
            {
            }
        }
    }

    ExFreePool(pUrb);

    return status;
}

NTSTATUS HumRemoveDevice(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS                status;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    ULONG                   PrevPnpState;
    PHID_DEVICE_EXTENSION   pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    PrevPnpState = pMiniDevExt->PnpState;
    pMiniDevExt->PnpState = 5;
    if (PrevPnpState != 3 && PrevPnpState != 4)
    {
        HumDecrementPendingRequestCount(pMiniDevExt);
    }
    if (PrevPnpState == 2)
    {
        HumAbortPendingRequests(pDevObj);
    }
    KeWaitForSingleObject(&pMiniDevExt->Event, Executive, KernelMode, FALSE, NULL);

    IoCopyCurrentIrpStackLocationToNext(pIrp);
    pIrp->IoStatus.Status = 0;
    status = IofCallDriver(pDevExt->NextDeviceObject, pIrp);
    IoReleaseRemoveLockAndWaitEx(&pMiniDevExt->RemoveLock, pIrp, sizeof(pMiniDevExt->RemoveLock));

    if (pMiniDevExt->pInterfaceInfo)
    {
        ExFreePool(pMiniDevExt->pInterfaceInfo);
        pMiniDevExt->pInterfaceInfo = NULL;
    }
    if (pMiniDevExt->pDevDesc)
    {
        ExFreePool(pMiniDevExt->pDevDesc);
        pMiniDevExt->pDevDesc = NULL;
    }

    return status;
}

NTSTATUS HumStopDevice(PDEVICE_OBJECT pDevObj)
{
    NTSTATUS                status;
    USHORT                  UrbLen;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    PURB                    pUrb;
    PHID_DEVICE_EXTENSION   pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    pMiniDevExt->PnpState = 3;
    HumAbortPendingRequests(pDevObj);
    HumDecrementPendingRequestCount(pMiniDevExt);
    KeWaitForSingleObject(&pMiniDevExt->Event, Executive, KernelMode, FALSE, NULL);

    UrbLen = sizeof(struct _URB_SELECT_CONFIGURATION);
    pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
    if (pUrb == NULL)
    {
        status = STATUS_UNSUCCESSFUL;
    }
    else
    {
        pUrb->UrbHeader.Length = UrbLen;
        pUrb->UrbHeader.Function = URB_FUNCTION_SELECT_CONFIGURATION;
        pUrb->UrbSelectConfiguration.ConfigurationDescriptor = NULL;
        status = HumCallUSB(pDevObj, pUrb);
        ExFreePool(pUrb);
        if (NT_SUCCESS(status))
        {
            return status;
        }
    }

    pMiniDevExt->PnpState = 4;

    return status;
}

NTSTATUS HumSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PHID_DEVICE_EXTENSION pDevExt;

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    IoCopyCurrentIrpStackLocationToNext(pIrp);
    return IoCallDriver(pDevExt->NextDeviceObject, pIrp);
}

NTSTATUS HumGetMsGenreDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS                status;
    USHORT                  UrbLen;
    PIO_STACK_LOCATION      pStack;
    PVOID                   pMappedBuff;
    PURB                    pUrb;
    PHID_MINI_DEV_EXTENSION pMiniDevExt;
    ULONG                   OutBuffLen;
    PHID_DEVICE_EXTENSION   pDevExt;

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    OutBuffLen = pStack->Parameters.DeviceIoControl.OutputBufferLength;
    if (OutBuffLen == 0)
    {
        return STATUS_INVALID_USER_BUFFER;
    }

    pMappedBuff = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, MdlMappingNoExecute | NormalPagePriority);
    if (pMappedBuff == NULL)
    {
        return STATUS_INVALID_USER_BUFFER;
    }

    UrbLen = sizeof(struct _URB_OS_FEATURE_DESCRIPTOR_REQUEST);
    pUrb = (PURB)ExAllocatePoolWithTag(NonPagedPoolNx, UrbLen, HID_USB_TAG);
    if (pUrb == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(pUrb, 0, UrbLen);
    memset(pMappedBuff, 0, OutBuffLen);
    pUrb->UrbHeader.Length = UrbLen;
    pUrb->UrbHeader.Function = URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR;
    pUrb->UrbOSFeatureDescriptorRequest.TransferBufferMDL = NULL;
    pUrb->UrbOSFeatureDescriptorRequest.Recipient = 1;
    pUrb->UrbOSFeatureDescriptorRequest.TransferBufferLength = OutBuffLen;
    pUrb->UrbOSFeatureDescriptorRequest.TransferBuffer = pMappedBuff;
    pUrb->UrbOSFeatureDescriptorRequest.InterfaceNumber = pMiniDevExt->pInterfaceInfo->InterfaceNumber;
    pUrb->UrbOSFeatureDescriptorRequest.MS_FeatureDescriptorIndex = 1;
    pUrb->UrbOSFeatureDescriptorRequest.UrbLink = 0;

    status = HumCallUSB(pDevObj, pUrb);
    if (NT_SUCCESS(status) == FALSE)
    {
        ExFreePool(pUrb);
    }
    else if (USBD_SUCCESS(pUrb->UrbHeader.Status) == FALSE)
    {
        ExFreePool(pUrb);
        status = STATUS_UNSUCCESSFUL;
    }
    else
    {
        pIrp->IoStatus.Information = pUrb->UrbOSFeatureDescriptorRequest.TransferBufferLength;
        ExFreePool(pUrb);
        status = STATUS_SUCCESS;
    }

    return status;
}

NTSTATUS HumGetPhysicalDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS status;
    PVOID    pPhysDesc;
    ULONG    OutBuffLen;

    RegDebug(L"HumGetPhysicalDescriptor start", NULL, 1);

    OutBuffLen = pIrp->Tail.Overlay.CurrentStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    pPhysDesc = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, MdlMappingNoExecute | NormalPagePriority);
    if (OutBuffLen != 0 && pPhysDesc != NULL)
    {
        status = HumGetDescriptorRequest(pDevObj, URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE, 35, &pPhysDesc, &OutBuffLen, 0, 0, 0);
        if (NT_SUCCESS(status) ){
            RegDebug(L"HumGetPhysicalDescriptor HumGetDescriptorRequest=",pPhysDesc,OutBuffLen);
        }
    }
    else
    {
        status = STATUS_INVALID_USER_BUFFER;
        RegDebug(L"HumGetPhysicalDescriptor MmGetSystemAddressForMdlSafe failed", NULL, OutBuffLen);
    }
    return status;
}



VOID RegDebug(WCHAR* strValueName, PVOID dataValue, ULONG datasizeValue)//RegDebug(L"Run debug here",pBuffer,pBufferSize);//RegDebug(L"Run debug here",NULL,0x12345678);
{
    if (!debug_on) {//调试开关
        return;
    }

    //初始化注册表项
    UNICODE_STRING stringKey;
    RtlInitUnicodeString(&stringKey, L"\\Registry\\Machine\\Software\\RegDebug");

    //初始化OBJECT_ATTRIBUTES结构
    OBJECT_ATTRIBUTES  ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes, &stringKey, OBJ_CASE_INSENSITIVE, NULL, NULL);//OBJ_CASE_INSENSITIVE对大小写敏感

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

