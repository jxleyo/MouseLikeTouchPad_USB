﻿//由ReverSoft的hidusb2项目修改而成

#include "MouseLikeTouchPad_USB.h"

#if DBG 

#define KdPrintData(_x_) KdPrintDataFun _x_

#else 

#define KdPrintData(_x_)

#endif // DBG wudfwdm



double round(double r) { return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5); }

static __forceinline float absf(float x)
{
    if (x < 0)return -x;
    return x;
}

static __forceinline double absd(double x)
{
    if (x < 0)return -x;
    return x;
}

ULONG order = 0;
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
    KdPrint(("HumInternalIoctl runtimes_IOCTL_IOCTL,%x\n",  runtimes_IOCTL_IOCTL));

    UrbLen = sizeof(URB);
    AcquiredLock = FALSE;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    NeedCompleteIrp = TRUE;

    KdPrint(("HumInternalIoctl pDevObj=%wZ", pDevObj->DriverObject->DriverName));

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
        KdPrint(("IOCTL_HID_GET_STRING,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetStringDescriptor(pDevObj, pIrp);

        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_FEATURE: 
    {//新增代码
        KdPrint(("IOCTL_HID_GET_FEATURE,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetSetReport(pDevObj, pIrp, &NeedCompleteIrp);

        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_SET_FEATURE:
    {
        KdPrint(("IOCTL_HID_SET_FEATURE,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetSetReport(pDevObj, pIrp, &NeedCompleteIrp);

        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
    {
        KdPrint(("IOCTL_HID_GET_DEVICE_DESCRIPTOR,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetHidDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
    {
        KdPrint(("IOCTL_HID_GET_REPORT_DESCRIPTOR,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetReportDescriptor(pDevObj, pIrp, &NeedCompleteIrp);

        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_ACTIVATE_DEVICE:
    {
        KdPrint(("IOCTL_HID_ACTIVATE_DEVICE,%x\n",  runtimes_IOCTL_IOCTL));
        KdPrint(("IOCTL_HID_DEACTIVATE_DEVICE,%x\n",  runtimes_IOCTL_IOCTL));
        status = STATUS_SUCCESS;
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_DEACTIVATE_DEVICE:
    {
        KdPrint(("IOCTL_HID_DEACTIVATE_DEVICE,%x\n",  runtimes_IOCTL_IOCTL));
        status = STATUS_SUCCESS;
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
    {
        KdPrint(("IOCTL_HID_GET_DEVICE_ATTRIBUTES,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetDeviceAttributes(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
    {
        KdPrint(("IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST,%x\n",  runtimes_IOCTL_IOCTL));
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
    {
        status = HumGetSetReport(pDevObj, pIrp, &NeedCompleteIrp);
        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_HID_GET_INPUT_REPORT:
    {
        status = HumGetSetReport(pDevObj, pIrp, &NeedCompleteIrp);
        goto ExitCheckNeedCompleteIrp;
    }
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:
    {
        KdPrint(("IOCTL_GET_PHYSICAL_DESCRIPTOR,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetPhysicalDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_INDEXED_STRING:
    {
        KdPrint(("IOCTL_HID_GET_INDEXED_STRING,%x\n",  runtimes_IOCTL_IOCTL));
        status = HumGetStringDescriptor(pDevObj, pIrp);
        goto ExitCompleteIrp;
    }
    case IOCTL_HID_GET_MS_GENRE_DESCRIPTOR:
    {
        KdPrint(("IOCTL_HID_GET_MS_GENRE_DESCRIPTOR,%x\n",  runtimes_IOCTL_IOCTL));
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
        pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
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
        KdPrint(("HumInternalIoctl IOCTL_HID_READ_REPORT,%x\n", runtimes_IOCTL_HID_READ_REPORT));

        pUserBuffer = pIrp->UserBuffer;
        OutBuffLen = pStack->Parameters.DeviceIoControl.OutputBufferLength;
        if (OutBuffLen == 0 || pUserBuffer == NULL)
        {
            status = STATUS_INVALID_PARAMETER;
            KdPrint(("HumInternalIoctl IOCTL_HID_READ_REPORT STATUS_INVALID_PARAMETER,%x\n",  runtimes_IOCTL_IOCTL));

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

            pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
            if (pUrb == NULL)
            {
                status = STATUS_INSUFFICIENT_RESOURCES;
                KdPrint(("HumInternalIoctl IOCTL_HID_READ_REPORT ExAllocatePoolWithTag err,%x\n",  runtimes_IOCTL_IOCTL));
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
    if (runtimes_ioReadCompletion==2) {
        KdPrint(("HumReadCompletion start,%x\n", runtimes_IOCTL_HID_READ_REPORT));
    }
    status = STATUS_SUCCESS;
    pUrb = (PURB)pContext;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    if (NT_SUCCESS(pIrp->IoStatus.Status) == TRUE)
    {
        pIrp->IoStatus.Information = pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        PBYTE buff= pUrb->UrbBulkOrInterruptTransfer.TransferBuffer;
        ULONG len= pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        if (len >= sizeof(PPTP_REPORT)) {
            //struct mouse_report_t mReport;
            //mReport.report_id = FAKE_REPORTID_MOUSE;

            //mReport.button = 0;
            //mReport.dx = 1;
            //mReport.dy = 1;
            //mReport.h_wheel = 0;
            //mReport.v_wheel = 0;
            //RtlZeroBytes(buff, len);
            //RtlCopyBytes(buff, &mReport, sizeof(mReport));


            //PPTP_REPORT prp = (PPTP_REPORT)buff;
            //prp->ReportID = FAKE_REPORTID_MULTITOUCH;
            MouseLikeTouchPad_parse(pMiniDevExt, buff, &len);

        }
        
    }
    else if (pIrp->IoStatus.Status == STATUS_CANCELLED)
    {

    }
    else if (pIrp->IoStatus.Status == STATUS_DEVICE_NOT_CONNECTED)
    {

    }
    else
    {
        KdPrint(("HumReadCompletion err,%x\n",  status));
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
        pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
        if (pUrb == NULL){
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
    pUrb = (URB *)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
    if (pUrb == NULL){
        status = STATUS_NO_MEMORY;
    }
    else
    {
        memset(pUrb, 0, UrbLen);
        if (*pDescBuffer == NULL)
        {
            *pDescBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED,*pDescBuffLen, HID_USB_TAG);
            BufferAllocated = TRUE;
            if (*pDescBuffer == NULL){
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

        if (NT_SUCCESS(status) == FALSE){
        }
        else
        {
            if (USBD_SUCCESS(pUrb->UrbHeader.Status) == TRUE){
                status = STATUS_SUCCESS;
                *pDescBuffLen = pUrb->UrbControlTransfer.TransferBufferLength;
                ExFreePool(pUrb);
                goto Exit;
            }

            status = STATUS_UNSUCCESSFUL;
        }
        if (BufferAllocated == TRUE){
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
    if (pIrp == NULL){
        status = STATUS_INSUFFICIENT_RESOURCES;
    }
    else{
        status = IoSetCompletionRoutineEx(pDevObj, pIrp, HumCallUsbComplete, &Event, TRUE, TRUE, TRUE);
        if (NT_SUCCESS(status) == FALSE){
            IofCompleteRequest(pIrp, IO_NO_INCREMENT);
        }
        else{
            IoGetNextIrpStackLocation(pIrp)->Parameters.Others.Argument1 = pUrb;
            status = IofCallDriver(pDevExt->NextDeviceObject, pIrp);
            
            if (status == STATUS_PENDING){
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
            if (status == STATUS_PENDING){
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


    KdPrint(("HumInitDevice start"));
    runtimes_IOCTL_HID_READ_REPORT = 0;
    runtimes_IOCTL_IOCTL = 0;
    runtimes_ioReadCompletion = 0;
    

    pConfigDescr = NULL;
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    KdPrint(("HumInitDevice pDevObj=%wZ", pDevObj->DriverObject->DriverName));

    status = HumGetDeviceDescriptor(pDevObj, pMiniDevExt);
    if (NT_SUCCESS(status) == FALSE)
    {
        KdPrint(("HumInitDevice HumGetDeviceDescriptor err,%x\n",  status));
    }
    else
    {
        status = HumGetConfigDescriptor(pDevObj, &pConfigDescr, &DescLen);
        if (NT_SUCCESS(status) == FALSE){
        }
        else
        {
            status = HumGetHidInfo(pDevObj, (UINT_PTR)pConfigDescr, DescLen);
            if (NT_SUCCESS(status) == FALSE)
            {
            }
            else
            {
                status = HumSelectConfiguration(pDevObj, pConfigDescr);
                if (NT_SUCCESS(status))
                {
                    HumSetIdle(pDevObj);
                }
            }

            ExFreePool(pConfigDescr);
        }
    }


    pMiniDevExt->SensitivityChanged = FALSE;
    pMiniDevExt->ButtonDown = FALSE;

    pMiniDevExt->MouseSensitivity_Index = 1;//默认初始值为MouseSensitivityTable存储表的序号1项
    pMiniDevExt->MouseSensitivity_Value = MouseSensitivityTable[pMiniDevExt->MouseSensitivity_Index];//默认初始值为1.0

    //读取鼠标灵敏度设置
    ULONG ms_idx;
    status = GetRegisterMouseSensitivity(pMiniDevExt, &ms_idx);
    if (!NT_SUCCESS(status))
    {
        if (status == STATUS_OBJECT_NAME_NOT_FOUND)//     ((NTSTATUS)0xC0000034L)
        {
            KdPrint(("OnPrepareHardware GetRegisterMouseSensitivity STATUS_OBJECT_NAME_NOT_FOUND,%x\n", status));
            status = SetRegisterMouseSensitivity(pMiniDevExt, pMiniDevExt->MouseSensitivity_Index);//初始默认设置
            if (!NT_SUCCESS(status)) {
                KdPrint(("OnPrepareHardware SetRegisterMouseSensitivity err,%x\n", status));
            }
        }
        else
        {
            KdPrint(("OnPrepareHardware GetRegisterMouseSensitivity err,%x\n", status));
        }
    }
    else {
        if (ms_idx > 2) {//如果读取的数值错误
            ms_idx = pMiniDevExt->MouseSensitivity_Index;//恢复初始默认值
        }
        pMiniDevExt->MouseSensitivity_Index = (UCHAR)ms_idx;
        pMiniDevExt->MouseSensitivity_Value = MouseSensitivityTable[pMiniDevExt->MouseSensitivity_Index];
        KdPrint(("OnPrepareHardware GetRegisterMouseSensitivity MouseSensitivity_Index=,%x\n", pMiniDevExt->MouseSensitivity_Index));
    }

    MouseLikeTouchPad_parse_init(pMiniDevExt);

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
        //新增
        #if DBG 
                PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc = (PUSB_CONFIGURATION_DESCRIPTOR)pConfigDescr;
                KdPrint(("pConfigDesc->iConfiguration=,%x\n", pConfigDesc->iConfiguration));
                KdPrint(("pConfigDesc->wTotalLength=,%x\n", pConfigDesc->wTotalLength));

        #endif // DBG wudfwdm

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
                pBuff = (USBD_INTERFACE_INFORMATION *)ExAllocatePool2(POOL_FLAG_NON_PAGED,pInterfaceInfo->Length, HID_USB_TAG);
                pMiniDevExt->pInterfaceInfo = pBuff;
                if (pBuff)
                {     
                    KdPrint(("pMiniDevExt->pInterfaceInfo->Pipes->EndpointAddress=,%x\n", pMiniDevExt->pInterfaceInfo->Pipes->EndpointAddress));
                    KdPrint(("pMiniDevExt->pInterfaceInfo->Pipes->MaximumPacketSize=,%x\n", pMiniDevExt->pInterfaceInfo->Pipes->MaximumPacketSize));
                    KdPrint(("pMiniDevExt->pInterfaceInfo->Pipes->PipeType=,%x\n", pMiniDevExt->pInterfaceInfo->Pipes ->PipeType));
                    KdPrint(("pMiniDevExt->pInterfaceInfo->Pipes->Interval=,%x\n", pMiniDevExt->pInterfaceInfo->Pipes->Interval));
                    KdPrint(("pMiniDevExt->pInterfaceInfo->Pipes->PipeFlags=,%x\n", pMiniDevExt->pInterfaceInfo->Pipes ->PipeFlags));
                    pMiniDevExt->pInterfaceInfo->Pipes ->Interval = 3;//设置usb回报率
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
        KdPrint(("pMiniDevExt->pDevDesc->bcdDevice=,%x\n", pMiniDevExt->pDevDesc->bcdDevice));
        KdPrint(("pMiniDevExt->pDevDesc->bcdUSB=,%x\n", pMiniDevExt->pDevDesc->bcdUSB));
        KdPrint(("pMiniDevExt->pDevDesc->bDeviceClass=,%x\n", pMiniDevExt->pDevDesc->bDeviceClass));
        KdPrint(("pMiniDevExt->pDevDesc->bDeviceSubClass=,%x\n", pMiniDevExt->pDevDesc->bDeviceSubClass));
        KdPrint(("pMiniDevExt->pDevDesc->bLength=,%x\n", pMiniDevExt->pDevDesc->bLength));
        KdPrint(("pMiniDevExt->pDevDesc->bMaxPacketSize0=,%x\n", pMiniDevExt->pDevDesc->bMaxPacketSize0));
        KdPrint(("pMiniDevExt->pDevDesc->bNumConfigurations=,%x\n", pMiniDevExt->pDevDesc->bNumConfigurations));
        KdPrint(("pMiniDevExt->pDevDesc->idProduct=,%x\n", pMiniDevExt->pDevDesc->idProduct));
        KdPrint(("pMiniDevExt->pDevDesc->idVendor=,%x\n", pMiniDevExt->pDevDesc->idVendor));
        KdPrint(("pMiniDevExt->pDevDesc->iManufacturer=,%x\n", pMiniDevExt->pDevDesc->iManufacturer));
        KdPrint(("pMiniDevExt->pDevDesc->iSerialNumber=,%x\n", pMiniDevExt->pDevDesc->iSerialNumber));
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
    KdPrint(("HumAddDevice order=%x\n", order++));
    KdPrint(("HumAddDevice pDrvObj pDevObj=%wZ", pDrvObj->DeviceObject->DriverObject->DriverName));
    KdPrint(("HumAddDevice pFdo pDevObj=%wZ", pFdo->DriverObject->DriverName));

    pDevExt = (PHID_DEVICE_EXTENSION)pFdo->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    pMiniDevExt->Flags = 0;
    pMiniDevExt->PendingRequestsCount = 0;
    KeInitializeEvent(&pMiniDevExt->Event, NotificationEvent, FALSE);
    pMiniDevExt->pWorkItem = NULL;
    pMiniDevExt->PnpState = 0;
    pMiniDevExt->pFdo = pFdo;

    pMiniDevExt->PDO = pDevExt->PhysicalDeviceObject;
    KdPrint(("HumAddDevice PDO pDevObj=%wZ", pMiniDevExt->PDO->DriverObject->DriverName));

    IoInitializeRemoveLockEx(&pMiniDevExt->RemoveLock, HID_USB_TAG, 2, 0, sizeof(pMiniDevExt->RemoveLock));

    runtimes_IOCTL_IOCTL = 0;
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

    size_t ReportSize;


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
        pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
        if (pUrb == NULL)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        else
        {
            memset(pUrb, 0, UrbLen);
            Value = pTransferPacket->reportId + Offset;
            if (pTransferPacket->reportId == FAKE_REPORTID_DEVICE_CAPS) {
                ReportSize = sizeof(PTP_DEVICE_CAPS_FEATURE_REPORT);
                if (pTransferPacket->reportBufferLen < ReportSize) {
                    status = STATUS_INVALID_BUFFER_SIZE;
                    KdPrint(("PtpGetFeatures REPORTID_DEVICE_CAPS STATUS_INVALID_BUFFER_SIZE,%x\n", pTransferPacket->reportId));
                    goto Exit;
                }

                PPTP_DEVICE_CAPS_FEATURE_REPORT capsReport = (PPTP_DEVICE_CAPS_FEATURE_REPORT)pTransferPacket->reportBuffer;

                capsReport->MaximumContactPoints = PTP_MAX_CONTACT_POINTS;// pDevContext->CONTACT_COUNT_MAXIMUM;// PTP_MAX_CONTACT_POINTS;
                capsReport->ButtonType = PTP_BUTTON_TYPE_CLICK_PAD;// pDevContext->PAD_TYPE;// PTP_BUTTON_TYPE_CLICK_PAD;
                capsReport->ReportID = FAKE_REPORTID_DEVICE_CAPS;// pDevContext->REPORTID_DEVICE_CAPS;//FAKE_REPORTID_DEVICE_CAPS
                KdPrint(("PtpGetFeatures pHidPacket->reportId REPORTID_DEVICE_CAPS,%x\n", pTransferPacket->reportId));
                KdPrint(("PtpGetFeatures REPORTID_DEVICE_CAPS MaximumContactPoints,%x\n", capsReport->MaximumContactPoints));
                KdPrint(("PtpGetFeatures REPORTID_DEVICE_CAPS REPORTID_DEVICE_CAPS ButtonType,%x\n", capsReport->ButtonType));

                pIrp->IoStatus.Information = ReportSize;
                pIrp->IoStatus.Status = STATUS_SUCCESS;
                return STATUS_SUCCESS;
            }
            if (pTransferPacket->reportId == FAKE_REPORTID_PTPHQA) {
                ReportSize = sizeof(PTP_DEVICE_HQA_CERTIFICATION_REPORT);
                if (pTransferPacket->reportBufferLen < ReportSize)
                {
                    status = STATUS_INVALID_BUFFER_SIZE;
                    KdPrint(("PtpGetFeatures REPORTID_PTPHQA STATUS_INVALID_BUFFER_SIZE,%x\n", pTransferPacket->reportId));
                    goto Exit;
                }

                PPTP_DEVICE_HQA_CERTIFICATION_REPORT certReport = (PPTP_DEVICE_HQA_CERTIFICATION_REPORT)pTransferPacket->reportBuffer;

                *certReport->CertificationBlob = DEFAULT_PTP_HQA_BLOB;
                certReport->ReportID = FAKE_REPORTID_PTPHQA;//FAKE_REPORTID_PTPHQA//pDevContext->REPORTID_PTPHQA

                KdPrint(("PtpGetFeatures pHidPacket->reportId REPORTID_PTPHQA,%x\n", pTransferPacket->reportId));

                pIrp->IoStatus.Information = ReportSize;
                pIrp->IoStatus.Status = STATUS_SUCCESS;
                return STATUS_SUCCESS;
            }
            if (pTransferPacket->reportId == FAKE_REPORTID_INPUTMODE) {
                pTransferPacket->reportId = pMiniDevExt->desc_settings.REPORTID_INPUT_MODE;
                KdPrint(("SetFeatureInputModeValue=,%x\n", Value));
                //reportBuffer[0]=pTransferPacket->reportId
                //pTransferPacket->reportBuffer[1] = 0x03;//reportBuffer[1]=SetFeatureInputModeValue
            }
            if (pTransferPacket->reportId == FAKE_REPORTID_FUNCTION_SWITCH) {
                pTransferPacket->reportId = pMiniDevExt->desc_settings.REPORTID_FUNCTION_SWITCH;
                KdPrint(("SetFeatureFunswitchValue=,%x\n", Value));
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
                KdPrint(("HumIncrementPendingRequestCount STATUS_NO_SUCH_DEVICE,%x\n", status));
            }
            else
            {
                status = IoCallDriver(pDevExt->NextDeviceObject, pIrp);
                *pNeedToCompleteIrp = FALSE;
                KdPrint(("IoCallDriver ok,%x\n", status));
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
    KdPrint(("HumQueueResetWorkItem,%x\n",  runtimes_IOCTL_IOCTL));

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
    pContext = (WRKITM_RESET_CONTEXT *)ExAllocatePool2(POOL_FLAG_NON_PAGED,sizeof(WRKITM_RESET_CONTEXT), HID_USB_TAG);
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
    pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
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
    KdPrint(("HumResetWorkItem,%x\n",  runtimes_IOCTL_IOCTL));

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
        if (IoControlCode != IOCTL_HID_GET_INDEXED_STRING) //0xB01E2
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
    pStrDesc = (PUSB_STRING_DESCRIPTOR)ExAllocatePool2(POOL_FLAG_NON_PAGED,DescBuffLen, HID_USB_TAG);
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
    KdPrint(("HumPnP order=%x\n", order++));

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

    pReportDesc = NULL;
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    ReportDescLen = pMiniDevExt->HidDesc.DescriptorList[0].wReportLength + 0x40;
    if (BooleanFlagOn(pMiniDevExt->Flags, DEXT_NO_HID_DESC) == FALSE)
    {
        KdPrint(("HumGetReportDescriptor URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE,%x\n", ReportDescLen));
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
        KdPrint(("HumGetReportDescriptor URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT,%x\n", ReportDescLen));
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
                KdPrint(("HumGetReportDescriptor HumQueueResetWorkItem STATUS_PENDING,%x\n",  status));
                return STATUS_PENDING;
            }

            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
        else
        {
            UCHAR EndptAddr;

            EndptAddr = pPipeInfo->EndpointAddress;
            EndptAddr &= USB_ENDPOINT_DIRECTION_MASK;

            KdPrint(("HumGetReportDescriptor USB_ENDPOINT_DIRECTION_MASK,%x\n", pPipeInfo->EndpointAddress));

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
        KdPrint(("HumGetDescriptorRequest err,%x\n",  status));
        if (status == STATUS_DEVICE_NOT_CONNECTED)
        {
            KdPrint(("HumGetReportDescriptor STATUS_DEVICE_NOT_CONNECTED,%x\n",  status));
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
                KdPrint(("HumGetReportDescriptor HumQueueResetWorkItem STATUS_PENDING22,%x\n",  status));
                return STATUS_PENDING;
            }

            KdPrint(("HumGetReportDescriptor HumQueueResetWorkItem,%x\n",  status));
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


    USHORT sz = sizeof(ParallelMode_PtpReportDescriptor);
    if (sz > pStack->Parameters.DeviceIoControl.OutputBufferLength) {
        pStack->Parameters.DeviceIoControl.OutputBufferLength = sz;
    }
    RtlCopyMemory(pIrp->UserBuffer, &ParallelMode_PtpReportDescriptor, sz);
    pIrp->IoStatus.Information = sz;
    //RtlCopyMemory(pIrp->UserBuffer, pReportDesc, OutBuffLen);
    //pIrp->IoStatus.Information = OutBuffLen;

    KdPrint(("HumGetReportDescriptor pNeedToCompleteIrp=,%x\n", *pNeedToCompleteIrp));
    KdPrintData(("HumGetReportDescriptor pReportDesc", pReportDesc, (ULONG)OutBuffLen));

    pMiniDevExt->ReportDescriptorLength = (USHORT)OutBuffLen;
    pMiniDevExt->pReportDesciptorData = (PBYTE)ExAllocatePool2(POOL_FLAG_NON_PAGED,pMiniDevExt->ReportDescriptorLength, HID_USB_TAG);
    if (pMiniDevExt->pReportDesciptorData) {
        RtlCopyMemory(pMiniDevExt->pReportDesciptorData, pReportDesc, pMiniDevExt->ReportDescriptorLength);
        AnalyzeHidReportDescriptor(pMiniDevExt);
    } 


    ExFreePool(pReportDesc);

    KdPrint(("HumGetReportDescriptor end,%x\n",  status));
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

    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;

    pHidDesc = &pMiniDevExt->HidDesc;
    DescLen = pMiniDevExt->HidDesc.bLength;
    if (DescLen == 0)
    {
        pIrp->IoStatus.Information = 0;
        KdPrint(("HumGetHidDescriptor STATUS_INVALID_PARAMETER"));
        return STATUS_INVALID_PARAMETER;
    }

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    OutBuffLen = pStack->Parameters.DeviceIoControl.OutputBufferLength;
    if (OutBuffLen > DescLen)
    {
        OutBuffLen = DescLen;
    }

    memcpy(pIrp->UserBuffer, &DefaultHidDescriptor, OutBuffLen);
    pIrp->IoStatus.Information = OutBuffLen;
    /*memcpy(pIrp->UserBuffer, pHidDesc, OutBuffLen);
    pIrp->IoStatus.Information = OutBuffLen;*/

    KdPrintData(("HumGetHidDescriptor pHidDesc=", (PUCHAR)pHidDesc, OutBuffLen));
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

    KdPrint(("HumAbortPendingRequests,%x\n",  runtimes_IOCTL_IOCTL));
    pDevExt = (PHID_DEVICE_EXTENSION)pDevObj->DeviceExtension;
    pMiniDevExt = (PHID_MINI_DEV_EXTENSION)pDevExt->MiniDeviceExtension;
    UrbLen = sizeof(struct _URB_PIPE_REQUEST);
    pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
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
    KdPrint(("HumRemoveDevice,%x\n",  runtimes_IOCTL_IOCTL));
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
    pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
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
    pUrb = (PURB)ExAllocatePool2(POOL_FLAG_NON_PAGED,UrbLen, HID_USB_TAG);
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


    OutBuffLen = pIrp->Tail.Overlay.CurrentStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    pPhysDesc = MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, MdlMappingNoExecute | NormalPagePriority);
    if (OutBuffLen != 0 && pPhysDesc != NULL)
    {
        status = HumGetDescriptorRequest(pDevObj, URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE, 35, &pPhysDesc, &OutBuffLen, 0, 0, 0);
        if (NT_SUCCESS(status) ){
            KdPrintData(("HumGetPhysicalDescriptor HumGetDescriptorRequest=",pPhysDesc,OutBuffLen));
        }
    }
    else
    {
        status = STATUS_INVALID_USER_BUFFER;
        KdPrint(("HumGetPhysicalDescriptor MmGetSystemAddressForMdlSafe failed,%x\n", OutBuffLen));
    }
    return status;
}




NTSTATUS
AnalyzeHidReportDescriptor(PHID_MINI_DEV_EXTENSION pDevContext)
{
    NTSTATUS status = STATUS_SUCCESS;
    PBYTE descriptor = pDevContext->pReportDesciptorData;
    if (!descriptor) {
        KdPrint(("AnalyzeHidReportDescriptor pReportDesciptorData err,%x\n",  status));
        KdPrint(("AnalyzeHidReportDescriptor pReportDesciptorData err 0x%x", status));
        return STATUS_UNSUCCESSFUL;
    }

    USHORT descriptorLen = pDevContext->ReportDescriptorLength;
    PTP_PARSER* tp = &pDevContext->tp_settings;
    HIDDESC_SETTING* dst = &pDevContext->desc_settings;

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
                //KdPrint(("AnalyzeHidReportDescriptor inConfigTlc"));
            }
            else if (depth == 1 && usagePage == HID_USAGE_PAGE_DIGITIZER && lastUsage == HID_USAGE_DIGITIZER_TOUCH_PAD) {
                inTouchTlc = TRUE;
                lastCollection = HID_USAGE_DIGITIZER_TOUCH_PAD;
                //KdPrint(("AnalyzeHidReportDescriptor inTouchTlc"));
            }
            else if (depth == 1 && usagePage == HID_USAGE_PAGE_GENERIC && lastUsage == HID_USAGE_GENERIC_MOUSE) {
                inMouseTlc = TRUE;
                lastCollection = HID_USAGE_GENERIC_MOUSE;
                //KdPrint(("AnalyzeHidReportDescriptor inMouseTlc"));
            }
        }
        else if (type == HID_TYPE_END_COLLECTION) {
            depth--;

            //下面3个Tlc状态更新是有必要的，可以防止后续相关集合Tlc错误判定发生
            if (depth == 0 && inConfigTlc) {
                inConfigTlc = FALSE;
                //KdPrint(("AnalyzeHidReportDescriptor inConfigTlc end"));
            }
            else if (depth == 0 && inTouchTlc) {
                inTouchTlc = FALSE;
                //KdPrint(("AnalyzeHidReportDescriptor inTouchTlc end"));
            }
            else if (depth == 0 && inMouseTlc) {
                inMouseTlc = FALSE;
                //KdPrint(("AnalyzeHidReportDescriptor inMouseTlc end"));
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
            dst->REPORTID_MULTITOUCH_COLLECTION = reportId;
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_MULTITOUCH_COLLECTION=,%x\n", dst->REPORTID_MULTITOUCH_COLLECTION));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_MULTITOUCH_COLLECTION= 0x%x", dst->REPORTID_MULTITOUCH_COLLECTION));

            //这里计算单个报告数据包的手指数量用来后续判断报告模式及bHybrid_ReportingMode的赋值
            dst->DeviceDescriptorFingerCount++;
            KdPrint(("AnalyzeHidReportDescriptor DeviceDescriptorFingerCount=,%x\n", dst->DeviceDescriptorFingerCount));
            KdPrint(("AnalyzeHidReportDescriptor DeviceDescriptorFingerCount= 0x%x", dst->DeviceDescriptorFingerCount));
        }
        else if (inMouseTlc && depth == 2 && lastCollection == HID_USAGE_GENERIC_MOUSE && lastUsage == HID_USAGE_GENERIC_POINTER) {
            //下层的Mouse集合report本驱动并不会读取，只是作为输出到上层类驱动的Report使用
            dst->REPORTID_MOUSE_COLLECTION = reportId;
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_MOUSE_COLLECTION=,%x\n", dst->REPORTID_MOUSE_COLLECTION));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_MOUSE_COLLECTION= 0x%x", dst->REPORTID_MOUSE_COLLECTION));
        }
        else if (inConfigTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_INPUT_MODE) {
            dst->REPORTSIZE_INPUT_MODE = (reportSize + 7) / 8;//报告数据总长度
            dst->REPORTID_INPUT_MODE = reportId;
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_INPUT_MODE=,%x\n", dst->REPORTID_INPUT_MODE));
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_INPUT_MODE=,%x\n", dst->REPORTSIZE_INPUT_MODE));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_INPUT_MODE= 0x%x", dst->REPORTID_INPUT_MODE));
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_INPUT_MODE= 0x%x", dst->REPORTSIZE_INPUT_MODE));
            continue;
        }
        else if (inConfigTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_SURFACE_SWITCH || lastUsage == HID_USAGE_BUTTON_SWITCH) {
            //默认标准规范为HID_USAGE_SURFACE_SWITCH与HID_USAGE_BUTTON_SWITCH各1bit组合低位成1个字节HID_USAGE_FUNCTION_SWITCH报告
            dst->REPORTSIZE_FUNCTION_SWITCH = (reportSize + 7) / 8;//报告数据总长度
            dst->REPORTID_FUNCTION_SWITCH = reportId;
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_FUNCTION_SWITCH=,%x\n", dst->REPORTID_FUNCTION_SWITCH));
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_FUNCTION_SWITCH=,%x\n", dst->REPORTSIZE_FUNCTION_SWITCH));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_FUNCTION_SWITCH= 0x%x", dst->REPORTID_FUNCTION_SWITCH));
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_FUNCTION_SWITCH= 0x%x", dst->REPORTSIZE_FUNCTION_SWITCH));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_CONTACT_COUNT_MAXIMUM || lastUsage == HID_USAGE_PAD_TYPE) {
            //默认标准规范为HID_USAGE_CONTACT_COUNT_MAXIMUM与HID_USAGE_PAD_TYPE各4bit组合低位成1个字节HID_USAGE_DEVICE_CAPS报告
            dst->REPORTSIZE_DEVICE_CAPS = (reportSize + 7) / 8;//报告数据总长度
            dst->REPORTID_DEVICE_CAPS = reportId;
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_DEVICE_CAPS=,%x\n", dst->REPORTSIZE_DEVICE_CAPS));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_DEVICE_CAPS=,%x\n", dst->REPORTID_DEVICE_CAPS));
            KdPrint(("AnalyzeHidReportDescriptor REPORTSIZE_DEVICE_CAPS= 0x%x", dst->REPORTSIZE_DEVICE_CAPS));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_DEVICE_CAPS= 0x%x", dst->REPORTID_DEVICE_CAPS));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_FEATURE && lastUsage == HID_USAGE_PAGE_VENDOR_DEFINED_DEVICE_CERTIFICATION) {
            dst->REPORTSIZE_PTPHQA = 256;
            dst->REPORTID_PTPHQA = reportId;
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_PTPHQA=,%x\n", dst->REPORTID_PTPHQA));
            KdPrint(("AnalyzeHidReportDescriptor REPORTID_PTPHQA= 0x%x", dst->REPORTID_PTPHQA));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_INPUT && lastUsage == HID_USAGE_X) {
            tp->physicalMax_X = physicalMax;
            tp->logicalMax_X = logicalMax;
            tp->unitExp = UnitExponent_Table[unitExp];
            tp->unit = unit;
            KdPrint(("AnalyzeHidReportDescriptor physicalMax_X=,%x\n", tp->physicalMax_X));
            KdPrint(("AnalyzeHidReportDescriptor logicalMax_X=,%x\n", tp->logicalMax_X));
            KdPrint(("AnalyzeHidReportDescriptor unitExp=,%x\n", tp->unitExp));
            KdPrint(("AnalyzeHidReportDescriptor unit=,%x\n", tp->unit));
            KdPrint(("AnalyzeHidReportDescriptor physicalMax_X= 0x%x", tp->physicalMax_X));
            KdPrint(("AnalyzeHidReportDescriptor logicalMax_X= 0x%x", tp->logicalMax_X));
            continue;
        }
        else if (inTouchTlc && type == HID_TYPE_INPUT && lastUsage == HID_USAGE_Y) {
            tp->physicalMax_Y = physicalMax;
            tp->logicalMax_Y = logicalMax;
            tp->unitExp = UnitExponent_Table[unitExp];
            tp->unit = unit;
            KdPrint(("AnalyzeHidReportDescriptor physicalMax_Y=,%x\n", tp->physicalMax_Y));
            KdPrint(("AnalyzeHidReportDescriptor logicalMax_Y=,%x\n", tp->logicalMax_Y));
            KdPrint(("AnalyzeHidReportDescriptor unitExp=,%x\n", tp->unitExp));
            KdPrint(("AnalyzeHidReportDescriptor unit=,%x\n", tp->unit));
            KdPrint(("AnalyzeHidReportDescriptor physicalMax_Y= 0x%x", tp->physicalMax_Y));
            KdPrint(("AnalyzeHidReportDescriptor logicalMax_Y= 0x%x", tp->logicalMax_Y));
            continue;
        }
    }

    //判断触摸板报告模式
    if (dst->DeviceDescriptorFingerCount < 5) {//5个手指数据以下
        dst->bHybrid_ReportingMode = TRUE;//混合报告模式确认
        KdPrint(("AnalyzeHidReportDescriptor bHybrid_ReportingMode=,%x\n", dst->bHybrid_ReportingMode));
        KdPrint(("AnalyzeHidReportDescriptor bHybrid_ReportingMode= 0x%x", dst->bHybrid_ReportingMode));
        return STATUS_UNSUCCESSFUL;
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
        KdPrint(("AnalyzeHidReportDescriptor physical_Width_mm err"));
        KdPrint(("AnalyzeHidReportDescriptor physical_Width_mm err"));
        return STATUS_UNSUCCESSFUL;
    }
    KdPrint(("AnalyzeHidReportDescriptor physical_Width_mm=,%x\n", (ULONG)tp->physical_Width_mm));

    if (!tp->physical_Height_mm) {
        KdPrint(("AnalyzeHidReportDescriptor physical_Height_mm err"));
        KdPrint(("AnalyzeHidReportDescriptor physical_Height_mm err"));
        return STATUS_UNSUCCESSFUL;
    }
    KdPrint(("AnalyzeHidReportDescriptor physical_Height_mm=,%x\n", (ULONG)tp->physical_Height_mm));

    tp->TouchPad_DPMM_x = (float)(tp->logicalMax_X / tp->physical_Width_mm);//单位为dot/mm
    tp->TouchPad_DPMM_y = (float)(tp->logicalMax_Y / tp->physical_Height_mm);//单位为dot/mm
    KdPrint(("AnalyzeHidReportDescriptor TouchPad_DPMM_x=,%x\n", (ULONG)tp->TouchPad_DPMM_x));
    KdPrint(("AnalyzeHidReportDescriptor TouchPad_DPMM_y=,%x\n", (ULONG)tp->TouchPad_DPMM_y));
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

    KdPrint(("AnalyzeHidReportDescriptor end"));
    KdPrint(("AnalyzeHidReportDescriptor end,%x\n",  status));
    return status;
}



VOID MouseLikeTouchPad_parse(PHID_MINI_DEV_EXTENSION pDevContext, PBYTE pReportBuffer, PULONG pReportLength)
{
    NTSTATUS status = STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(status);

    PTP_PARSER* tp = &pDevContext->tp_settings;

    //计算报告频率和时间间隔
    KeQueryTickCount(&tp->current_Ticktime);


    //保存当前手指坐标
    tp->currentFinger = *((PTP_REPORT*)pReportBuffer);
    UCHAR currentFinger_Count = tp->currentFinger.ContactCount;//当前触摸点数量
    UCHAR lastFinger_Count = tp->lastFinger.ContactCount; //上次触摸点数量

    tp->currentFinger.ReportID = FAKE_REPORTID_MULTITOUCH;
    RtlCopyMemory(pReportBuffer, &tp->currentFinger, sizeof(PTP_REPORT));//需要赋值给pReportBuffer


    UCHAR MAX_CONTACT_FINGER = PTP_MAX_CONTACT_POINTS;
    BOOLEAN allFingerDetached = TRUE;
    for (UCHAR i = 0; i < MAX_CONTACT_FINGER; i++) {//所有TipSwitch为0时判定为手指全部离开，因为最后一个点离开时ContactCount和Confidence始终为1不会置0。
        if (tp->currentFinger.Contacts[i].TipSwitch) {
            allFingerDetached = FALSE;
            currentFinger_Count = tp->currentFinger.ContactCount;//重新定义当前触摸点数量
            break;
        }
    }
    if (allFingerDetached) {
        currentFinger_Count = 0;
    }


    //初始化鼠标事件
    struct mouse_report_t mReport;
    mReport.report_id = FAKE_REPORTID_MOUSE;
    //mReport.report_id = pDevContext->desc_settings.REPORTID_MOUSE_COLLECTION;

    mReport.button = 0;
    mReport.dx = 0;
    mReport.dy = 0;
    mReport.h_wheel = 0;
    mReport.v_wheel = 0;

    BOOLEAN bMouse_LButton_Status = 0; //定义临时鼠标左键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
    BOOLEAN bMouse_MButton_Status = 0; //定义临时鼠标中键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
    BOOLEAN bMouse_RButton_Status = 0; //定义临时鼠标右键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
    BOOLEAN bMouse_BButton_Status = 0; //定义临时鼠标Back后退键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
    BOOLEAN bMouse_FButton_Status = 0; //定义临时鼠标Forward前进键状态，0为释放，1为按下，每次都需要重置确保后面逻辑

    //初始化当前触摸点索引号，跟踪后未再赋值的表示不存在了
    tp->nMouse_Pointer_CurrentIndex = -1;
    tp->nMouse_LButton_CurrentIndex = -1;
    tp->nMouse_RButton_CurrentIndex = -1;
    tp->nMouse_MButton_CurrentIndex = -1;
    tp->nMouse_Wheel_CurrentIndex = -1;


   
    //所有手指触摸点的索引号跟踪
    for (char i = 0; i < MAX_CONTACT_FINGER; i++) {//必须搜索全部点的Contacts数据因为可能有效的点数据排列顺序不在靠前位置
        KdPrint(("currentfinger Contact[%x].Confidence = %x\n", i,tp->currentFinger.Contacts[i].Confidence));
        KdPrint(("currentfinger Contact[%x].ContactID = %x\n", i, tp->currentFinger.Contacts[i].ContactID));
        KdPrint(("currentfinger Contact[%x].TipSwitch = %x\n", i, tp->currentFinger.Contacts[i].TipSwitch));
        KdPrint(("currentfinger Contact[%x].X = %x\n", i, tp->currentFinger.Contacts[i].X));
        KdPrint(("currentfinger Contact[%x].Y = %x\n", i, tp->currentFinger.Contacts[i].Y));

        if (!tp->currentFinger.Contacts[i].Confidence || !tp->currentFinger.Contacts[i].TipSwitch) {
            //必须判断Confidence和TipSwitch，已经释放的点数据很大可能依然存在ContactID和XY信息
            continue;

        }

        if (tp->nMouse_Pointer_LastIndex != -1) {

            if (tp->lastFinger.Contacts[tp->nMouse_Pointer_LastIndex].ContactID == tp->currentFinger.Contacts[i].ContactID) {
                tp->nMouse_Pointer_CurrentIndex = i;//找到指针
                continue;//查找其他功能
            }
        }

        if (tp->nMouse_Wheel_LastIndex != -1) {
            if (tp->lastFinger.Contacts[tp->nMouse_Wheel_LastIndex].ContactID == tp->currentFinger.Contacts[i].ContactID) {
                tp->nMouse_Wheel_CurrentIndex = i;//找到滚轮辅助键
                continue;//查找其他功能
            }
        }

        if (tp->nMouse_LButton_LastIndex != -1) {
            if (tp->lastFinger.Contacts[tp->nMouse_LButton_LastIndex].ContactID == tp->currentFinger.Contacts[i].ContactID) {
                bMouse_LButton_Status = 1; //找到左键，
                tp->nMouse_LButton_CurrentIndex = i;//赋值左键触摸点新索引号
                continue;//查找其他功能
            }
        }

        if (tp->nMouse_RButton_LastIndex != -1) {
            if (tp->lastFinger.Contacts[tp->nMouse_RButton_LastIndex].ContactID == tp->currentFinger.Contacts[i].ContactID) {
                bMouse_RButton_Status = 1; //找到右键，
                tp->nMouse_RButton_CurrentIndex = i;//赋值右键触摸点新索引号
                continue;//查找其他功能
            }
        }

        if (tp->nMouse_MButton_LastIndex != -1) {
            if (tp->lastFinger.Contacts[tp->nMouse_MButton_LastIndex].ContactID == tp->currentFinger.Contacts[i].ContactID) {
                bMouse_MButton_Status = 1; //找到中键，
                tp->nMouse_MButton_CurrentIndex = i;//赋值中键触摸点新索引号
                continue;//查找其他功能
            }
        }
    }

    KdPrint(("currentfinger ContactCount = %x\n", tp->currentFinger.ContactCount));
    KdPrint(("currentfinger IsButtonClicked = %x\n", tp->currentFinger.IsButtonClicked));
    KdPrint(("currentfinger ReportID = %x\n", tp->currentFinger.ReportID));
    KdPrint(("currentfinger ScanTime = %x\n", tp->currentFinger.ScanTime));


    KdPrint(("nMouse_Pointer_CurrentIndex = %x\n", tp->nMouse_Pointer_CurrentIndex));
    KdPrint(("nMouse_Pointer_LastIndex = %x\n", tp->nMouse_Pointer_LastIndex));
    if (tp->nMouse_Pointer_LastIndex != -1) {
        KdPrint(("lastFinger tp->nMouse_Pointer_LastIndex Contact[%x].ContactID = %x\n", tp->nMouse_Pointer_LastIndex, tp->lastFinger.Contacts[tp->nMouse_Pointer_LastIndex].ContactID));
    }
  

    KdPrint(("MouseLikeTouchPad_parse traced currentFinger_Count=,%x\n", currentFinger_Count));


    if (currentFinger_Count == 5) {//5指轻触触控板
        pDevContext->ButtonDown= TRUE;     
        pDevContext->SensitivityChanged = TRUE;
    }
    else {
        pDevContext->ButtonDown = FALSE;
    }
    
    //if (!pDevContext->ButtonDown && pDevContext->SensitivityChanged) {
    //    pDevContext->SensitivityChanged = FALSE;
    //    SetNextSensitivity(pDevContext);
    //}


    //开始鼠标事件逻辑判定
    //注意多手指非同时快速接触触摸板时触摸板报告可能存在一帧中同时新增多个触摸点的情况所以不能用当前只有一个触摸点作为定义指针的判断条件
    if (tp->nMouse_Pointer_LastIndex == -1 && currentFinger_Count > 0) {//鼠标指针、左键、右键、中键都未定义,
        //指针触摸点压力、接触面长宽比阈值特征区分判定手掌打字误触和正常操作,压力越小接触面长宽比阈值越大、长度阈值越小
        for (UCHAR i = 0; i < currentFinger_Count; i++) {
            if (tp->currentFinger.Contacts[i].Confidence && tp->currentFinger.Contacts[i].TipSwitch) {//不需要tp->currentFinger.Contacts[i].ContactID == 0条件 
                tp->nMouse_Pointer_CurrentIndex = i;  //首个触摸点作为指针
                tp->MousePointer_DefineTime = tp->current_Ticktime;//定义当前指针起始时间
                break;
            }
        }     
    }
    else if (tp->nMouse_Pointer_CurrentIndex == -1 && tp->nMouse_Pointer_LastIndex != -1) {//指针消失
        tp->bMouse_Wheel_Mode = FALSE;//结束滚轮模式
        tp->bMouse_Wheel_Mode_JudgeEnable = TRUE;//开启滚轮判别

        tp->bGestureCompleted = TRUE;//手势模式结束,但tp->bPtpReportCollection不要重置待其他代码来处理

        tp->nMouse_Pointer_CurrentIndex = -1;
        tp->nMouse_LButton_CurrentIndex = -1;
        tp->nMouse_RButton_CurrentIndex = -1;
        tp->nMouse_MButton_CurrentIndex = -1;
        tp->nMouse_Wheel_CurrentIndex = -1;
    }
    else if (tp->nMouse_Pointer_CurrentIndex != -1 && !tp->bMouse_Wheel_Mode) {  //指针已定义的非滚轮事件处理
        //查找指针左侧或者右侧是否有手指作为滚轮模式或者按键模式，当指针左侧/右侧的手指按下时间与指针手指定义时间间隔小于设定阈值时判定为鼠标滚轮否则为鼠标按键，这一规则能有效区别按键与滚轮操作,但鼠标按键和滚轮不能一起使用
        //按键定义后会跟踪坐标所以左键和中键不能滑动食指互相切换需要抬起食指后进行改变，左键/中键/右键按下的情况下不能转变为滚轮模式，
        LARGE_INTEGER MouseButton_Interval;
        MouseButton_Interval.QuadPart = (tp->current_Ticktime.QuadPart - tp->MousePointer_DefineTime.QuadPart) * tp->tick_Count / 10000;//单位ms毫秒
        float Mouse_Button_Interval = (float)MouseButton_Interval.LowPart;//指针左右侧的手指按下时间与指针定义起始时间的间隔ms

        if (currentFinger_Count > 1) {//触摸点数量超过1才需要判断按键操作
            for (char i = 0; i < currentFinger_Count; i++) {
                if (i == tp->nMouse_Pointer_CurrentIndex || i == tp->nMouse_LButton_CurrentIndex || i == tp->nMouse_RButton_CurrentIndex || i == tp->nMouse_MButton_CurrentIndex || i == tp->nMouse_Wheel_CurrentIndex) {//i为正值所以无需检查索引号是否为-1
                    continue;  // 已经定义的跳过
                }
                float dx = (float)(tp->currentFinger.Contacts[i].X - tp->currentFinger.Contacts[tp->nMouse_Pointer_CurrentIndex].X);
                float dy = (float)(tp->currentFinger.Contacts[i].Y - tp->currentFinger.Contacts[tp->nMouse_Pointer_CurrentIndex].Y);
                float distance = (float)(sqrt((double)dx * (double)dx + (double)dy * (double)dy));//触摸点与指针的距离

                BOOLEAN isWheel = FALSE;//滚轮模式成立条件初始化重置，注意bWheelDisabled与bMouse_Wheel_Mode_JudgeEnable的作用不同，不能混淆
                if (!pDevContext->bWheelDisabled) {//滚轮功能开启时
                    // 指针左右侧有手指按下并且与指针手指起始定义时间间隔小于阈值，指针被定义后区分滚轮操作只需判断一次直到指针消失，后续按键操作判断不会被时间阈值约束使得响应速度不受影响
                    isWheel = tp->bMouse_Wheel_Mode_JudgeEnable && absf(distance) > tp->FingerMinDistance && absf(distance) < tp->FingerMaxDistance && Mouse_Button_Interval < ButtonPointer_Interval_MSEC;
                }

                if (isWheel) {//滚轮模式条件成立
                    tp->bMouse_Wheel_Mode = TRUE;  //开启滚轮模式
                    tp->bMouse_Wheel_Mode_JudgeEnable = FALSE;//关闭滚轮判别

                    tp->bGestureCompleted = FALSE; //手势操作结束标志,但tp->bPtpReportCollection不要重置待其他代码来处理

                    tp->nMouse_Wheel_CurrentIndex = i;//滚轮辅助参考手指索引值
                    //手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
                    tp->JitterFixStartTime = tp->current_Ticktime;//抖动修正开始计时
                    tp->Scroll_TotalDistanceX = 0;//累计滚动位移量重置
                    tp->Scroll_TotalDistanceY = 0;//累计滚动位移量重置


                    tp->nMouse_LButton_CurrentIndex = -1;
                    tp->nMouse_RButton_CurrentIndex = -1;
                    tp->nMouse_MButton_CurrentIndex = -1;
                    break;
                }
                else {//前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，
                    if (tp->nMouse_MButton_CurrentIndex == -1 && absf(distance) > tp->FingerMinDistance && absf(distance) < tp->FingerClosedThresholdDistance && dx < 0) {//指针左侧有并拢的手指按下
                        bMouse_MButton_Status = 1; //找到中键
                        tp->nMouse_MButton_CurrentIndex = i;//赋值中键触摸点新索引号
                        continue;  //继续找其他按键，食指已经被中键占用所以原则上左键已经不可用
                    }
                    else if (tp->nMouse_LButton_CurrentIndex == -1 && absf(distance) > tp->FingerClosedThresholdDistance && absf(distance) < tp->FingerMaxDistance && dx < 0) {//指针左侧有分开的手指按下
                        bMouse_LButton_Status = 1; //找到左键
                        tp->nMouse_LButton_CurrentIndex = i;//赋值左键触摸点新索引号
                        continue;  //继续找其他按键
                    }
                    else if (tp->nMouse_RButton_CurrentIndex == -1 && absf(distance) > tp->FingerMinDistance && absf(distance) < tp->FingerMaxDistance && dx > 0) {//指针右侧有手指按下
                        bMouse_RButton_Status = 1; //找到右键
                        tp->nMouse_RButton_CurrentIndex = i;//赋值右键触摸点新索引号
                        continue;  //继续找其他按键
                    }
                }

            }
        }

        KdPrint(("currentFinger_Count =%x\n", currentFinger_Count));
        KdPrint(("lastFinger_Count =%x\n", lastFinger_Count));

        //鼠标指针位移设置
        if (currentFinger_Count != lastFinger_Count) {//手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
            tp->JitterFixStartTime = tp->current_Ticktime;//抖动修正开始计时
        }
        else {
            LARGE_INTEGER FixTimer;
            FixTimer.QuadPart = (tp->current_Ticktime.QuadPart - tp->JitterFixStartTime.QuadPart) * tp->tick_Count / 10000;//单位ms毫秒
            float JitterFixTimer = (float)FixTimer.LowPart;//当前抖动时间计时

            float STABLE_INTERVAL;
            if (tp->nMouse_MButton_CurrentIndex != -1) {//中键状态下手指并拢的抖动修正值区别处理
                STABLE_INTERVAL = STABLE_INTERVAL_FingerClosed_MSEC;
            }
            else {
                STABLE_INTERVAL = STABLE_INTERVAL_FingerSeparated_MSEC;
            }

            SHORT diffX = tp->currentFinger.Contacts[tp->nMouse_Pointer_CurrentIndex].X - tp->lastFinger.Contacts[tp->nMouse_Pointer_LastIndex].X;
            SHORT diffY = tp->currentFinger.Contacts[tp->nMouse_Pointer_CurrentIndex].Y - tp->lastFinger.Contacts[tp->nMouse_Pointer_LastIndex].Y;

            CHAR px = (CHAR)(diffX / tp->thumb_Scale);
            CHAR py = (CHAR)(diffY / tp->thumb_Scale);

            if (JitterFixTimer < STABLE_INTERVAL) {//触摸点稳定前修正
                if (tp->nMouse_LButton_CurrentIndex != -1 || tp->nMouse_RButton_CurrentIndex != -1 || tp->nMouse_MButton_CurrentIndex != -1) {//有按键时修正，单指针时不需要使得指针更精确
                    if (abs((int)px) <= Jitter_Offset) {//指针轻微抖动修正
                        px = 0;
                    }
                    if (abs((int)py) <= Jitter_Offset) {//指针轻微抖动修正
                        py = 0;
                    }
                }
            }

            double xx = round(px / tp->PointerSensitivity_x);
            double yy = round(py / tp->PointerSensitivity_y);
            KdPrint(("xx =%x\n", (int)px));
            KdPrint(("yy =%x\n", (int)yy));

            mReport.dx = (CHAR)xx;
            mReport.dy = (CHAR)yy;
            KdPrint(("mReport.dx =%x\n", mReport.dx));
            KdPrint(("mReport.dy =%x\n", mReport.dy));

            if (absd(xx) > 0.5 && absd(xx) < 1) {//慢速精细移动指针修正
                if (xx > 0) {
                    mReport.dx = 1;
                }
                else {
                    mReport.dx = -1;
                }
                
            }
            if (absd(yy) > 0.5 && absd(yy) < 1) {//慢速精细移动指针修正
                if (xx > 0) {
                    mReport.dy = 1;
                }
                else {
                    mReport.dy = -1;
                }
            }
        }
    }
    else if (tp->nMouse_Pointer_CurrentIndex != -1 && tp->bMouse_Wheel_Mode) {//滚轮操作模式，触摸板双指滑动、三指四指手势也归为此模式下的特例设置一个手势状态开关供后续判断使用
            tp->bPtpReportCollection = TRUE;//发送PTP触摸板集合报告，后续再做进一步判断
    }
    else {
        //其他组合无效
    }


    if (tp->bPtpReportCollection) {//触摸板集合，手势模式判断
        if (!tp->bMouse_Wheel_Mode) {//以指针手指释放为滚轮模式结束标志，下一帧bPtpReportCollection会设置FALSE所以只会发送一次构造的手势结束报告
            tp->bPtpReportCollection = FALSE;//PTP触摸板集合报告模式结束
            tp->bGestureCompleted = TRUE;//结束手势操作，该数据和bMouse_Wheel_Mode区分开了，因为bGestureCompleted可能会比bMouse_Wheel_Mode提前结束
            KdPrint(("MouseLikeTouchPad_parse bPtpReportCollection bGestureCompleted0,%x\n",  status));

            //构造全部手指释放的临时数据包,TipSwitch域归零，windows手势操作结束时需要手指离开的点xy坐标数据
            PTP_REPORT CompletedGestureReport;
            RtlCopyMemory(&CompletedGestureReport, &tp->currentFinger, sizeof(PTP_REPORT));
            for (int i = 0; i < currentFinger_Count; i++) {
                CompletedGestureReport.Contacts[i].TipSwitch = 0;
            }

            //发送ptp报告
            RtlZeroMemory(pReportBuffer, *pReportLength);
            RtlCopyMemory(pReportBuffer, &CompletedGestureReport, sizeof(PTP_REPORT));


        }
        else if (tp->bMouse_Wheel_Mode && currentFinger_Count == 1 && !tp->bGestureCompleted) {//滚轮模式未结束并且剩下指针手指留在触摸板上,需要配合bGestureCompleted标志判断使得构造的手势结束报告只发送一次
            tp->bPtpReportCollection = FALSE;//PTP触摸板集合报告模式结束
            tp->bGestureCompleted = TRUE;//提前结束手势操作，该数据和bMouse_Wheel_Mode区分开了，因为bGestureCompleted可能会比bMouse_Wheel_Mode提前结束
            KdPrint(("MouseLikeTouchPad_parse bPtpReportCollection bGestureCompleted1,%x\n",  status));

            //构造指针手指释放的临时数据包,TipSwitch域归零，windows手势操作结束时需要手指离开的点xy坐标数据
            PTP_REPORT CompletedGestureReport2;
            RtlCopyMemory(&CompletedGestureReport2, &tp->currentFinger, sizeof(PTP_REPORT));
            CompletedGestureReport2.Contacts[0].TipSwitch = 0;

            //发送ptp报告
            RtlZeroMemory(pReportBuffer, *pReportLength);
            RtlCopyMemory(pReportBuffer, &CompletedGestureReport2, sizeof(PTP_REPORT));
        }

        if (!tp->bGestureCompleted) {//手势未结束，正常发送报告
            KdPrint(("MouseLikeTouchPad_parse bPtpReportCollection bGestureCompleted2,%x\n",  status));
            //发送ptp报告
            //不改变pReportBuffer数据
            KdPrint(("MouseLikeTouchPad_parse SendPtpMultiTouchReport ok,%x\n",  status));
        }
    }
    else {//发送MouseCollection
        mReport.button = bMouse_LButton_Status + (bMouse_RButton_Status << 1) + (bMouse_MButton_Status << 2) + (bMouse_BButton_Status << 3) + (bMouse_FButton_Status << 4);  //左中右后退前进键状态合成
        //发送鼠标报告
        RtlZeroMemory(pReportBuffer, *pReportLength);
        *pReportLength = sizeof(mReport);
        RtlCopyMemory(pReportBuffer, &mReport, sizeof(mReport));
        KdPrint(("MouseLikeTouchPad_parse SendMouseReport ok,%x\n", status));
        
    }


    //保存下一轮所有触摸点的初始坐标及功能定义索引号
    tp->lastFinger = tp->currentFinger;

    lastFinger_Count = currentFinger_Count;
    tp->nMouse_Pointer_LastIndex = tp->nMouse_Pointer_CurrentIndex;
    tp->nMouse_LButton_LastIndex = tp->nMouse_LButton_CurrentIndex;
    tp->nMouse_RButton_LastIndex = tp->nMouse_RButton_CurrentIndex;
    tp->nMouse_MButton_LastIndex = tp->nMouse_MButton_CurrentIndex;
    tp->nMouse_Wheel_LastIndex = tp->nMouse_Wheel_CurrentIndex;

}





VOID MouseLikeTouchPad_parse_init(PHID_MINI_DEV_EXTENSION pDevContext)
{
    PTP_PARSER* tp = &pDevContext->tp_settings;

    tp->nMouse_Pointer_CurrentIndex = -1; //定义当前鼠标指针触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_LButton_CurrentIndex = -1; //定义当前鼠标左键触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_RButton_CurrentIndex = -1; //定义当前鼠标右键触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_MButton_CurrentIndex = -1; //定义当前鼠标中键触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_Wheel_CurrentIndex = -1; //定义当前鼠标滚轮辅助参考手指触摸点坐标的数据索引号，-1为未定义

    tp->nMouse_Pointer_LastIndex = -1; //定义上次鼠标指针触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_LButton_LastIndex = -1; //定义上次鼠标左键触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_RButton_LastIndex = -1; //定义上次鼠标右键触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_MButton_LastIndex = -1; //定义上次鼠标中键触摸点坐标的数据索引号，-1为未定义
    tp->nMouse_Wheel_LastIndex = -1; //定义上次鼠标滚轮辅助参考手指触摸点坐标的数据索引号，-1为未定义

    pDevContext->bWheelDisabled = FALSE;//默认初始值为开启滚轮操作功能


    tp->bMouse_Wheel_Mode = FALSE;
    tp->bMouse_Wheel_Mode_JudgeEnable = TRUE;//开启滚轮判别

    tp->bGestureCompleted = FALSE; //手势操作结束标志
    tp->bPtpReportCollection = FALSE;//默认鼠标集合

    RtlZeroMemory(&tp->lastFinger, sizeof(PTP_REPORT));
    RtlZeroMemory(&tp->currentFinger, sizeof(PTP_REPORT));

    tp->Scroll_TotalDistanceX = 0;
    tp->Scroll_TotalDistanceY = 0;

    tp->tick_Count = KeQueryTimeIncrement();


}



VOID KdPrintDataFun(CHAR* pChars, PUCHAR DataBuffer, ULONG DataSize)
{
    DbgPrint(pChars);
    for (UINT32 i = 0; i < DataSize; i++) {
        DbgPrint("% x,", DataBuffer[i]);
    }
    DbgPrint("\n");
}


NTSTATUS SetRegisterMouseSensitivity(PHID_MINI_DEV_EXTENSION pMiniDevExt, ULONG ms_idx)//保存设置到注册表
{
    NTSTATUS status = STATUS_SUCCESS;
    //
    HANDLE devInstRegKey;
    status = IoOpenDeviceRegistryKey(pMiniDevExt->PDO->NextDevice,
                                     PLUGPLAY_REGKEY_DEVICE,//PLUGPLAY_REGKEY_DEVICE//PLUGPLAY_REGKEY_DRIVER
                                     STANDARD_RIGHTS_ALL,
                                     &devInstRegKey);
    if (!NT_SUCCESS(status)) {
        KdPrint(("IoOpenDeviceRegistryKey failed, % x\n", status));
        return STATUS_UNSUCCESSFUL;
    }

    UNICODE_STRING  ValueNameString;
    RtlInitUnicodeString(&ValueNameString, L"MouseSensitivity_Index");

    ULONG Value = ms_idx;

    status = ZwSetValueKey(devInstRegKey, &ValueNameString, 0, REG_DWORD, &Value, sizeof(ULONG));

    if (!NT_SUCCESS(status)) {
        KdPrint(("SetRegisterMouseSensitivity ZwSetValueKey failed,%x\n", status));
    }

    ZwClose(devInstRegKey);
    KdPrint(("SetRegisterMouseSensitivity ok,%x\n", status));
    return status;
}



NTSTATUS GetRegisterMouseSensitivity(PHID_MINI_DEV_EXTENSION pMiniDevExt, ULONG* ms_idx)//从注册表读取设置
{

    NTSTATUS status = STATUS_SUCCESS;
    ULONG Value = 1;//默认值

    //
    HANDLE devInstRegKey;
    status = IoOpenDeviceRegistryKey(pMiniDevExt->PDO->NextDevice,
                                     PLUGPLAY_REGKEY_DEVICE,//PLUGPLAY_REGKEY_DEVICE//PLUGPLAY_REGKEY_DRIVER
                                     STANDARD_RIGHTS_ALL,
                                     &devInstRegKey);
    if (!NT_SUCCESS(status)) {
        KdPrint(("GetRegisterMouseSensitivity IoOpenDeviceRegistryKey failed, % x\n", status));
        return STATUS_UNSUCCESSFUL;
    }

    UNICODE_STRING  ValueNameString;
    RtlInitUnicodeString(&ValueNameString, L"MouseSensitivity_Index");

    ULONG length;			// 接受长度的变量

    ZwQueryValueKey(devInstRegKey, &ValueNameString, KeyValuePartialInformation, NULL, 0, &length);		// 首先查询实际只是要求出实际需要的字节数

    PKEY_VALUE_PARTIAL_INFORMATION keyInfo =(PKEY_VALUE_PARTIAL_INFORMATION)(ExAllocatePoolWithTag(PagedPool, length, HID_USB_TAG));
    if (keyInfo == NULL)
    {
        KdPrint(("GetRegisterMouseSensitivity ExAllocatePoolWithTag failed,%x\n", status));
        ZwClose(devInstRegKey);
        return STATUS_UNSUCCESSFUL;
    }

    // 再次查询获取信息
    status = ZwQueryValueKey(devInstRegKey, &ValueNameString, KeyValuePartialInformation, keyInfo, length, &length);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("GetRegisterMouseSensitivity ZwQueryValueKey failed,%x\n", status));
        goto Exit;
    }

    Value = *((ULONG*)(keyInfo->Data));
    if (Value>2) {
        KdPrint(("GetRegisterMouseSensitivity ZwQueryValueKey errValue,%x\n", status));
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    *ms_idx = Value;


Exit:
    ExFreePoolWithTag(keyInfo, HID_USB_TAG);		// 释放动态内存

    ZwClose(devInstRegKey);			// 关闭句柄
    KdPrint(("GetRegisterMouseSensitivity end,%x\n", status));
    return status;

}


void SetNextSensitivity(PHID_MINI_DEV_EXTENSION pDevContext)
{
    UCHAR ms_idx = pDevContext->MouseSensitivity_Index;// MouseSensitivity_Normal;//MouseSensitivity_Slow//MouseSensitivity_FAST

    ms_idx++;
    if (ms_idx == 3) {//灵敏度循环设置
        ms_idx = 0;
    }

    //保存注册表灵敏度设置数值
    NTSTATUS status = SetRegisterMouseSensitivity(pDevContext, ms_idx);//MouseSensitivityTable存储表的序号值
    if (!NT_SUCCESS(status))
    {
        KdPrint(("SetNextSensitivity SetRegisterMouseSensitivity err,%x\n", status));
        return;
    }

    pDevContext->MouseSensitivity_Index = ms_idx;
    pDevContext->MouseSensitivity_Value = MouseSensitivityTable[ms_idx];
    KdPrint(("SetNextSensitivity pDevContext->MouseSensitivity_Index,%x\n", pDevContext->MouseSensitivity_Index));

    KdPrint(("SetNextSensitivity ok,%x\n", status));
}
