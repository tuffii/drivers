/*--

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.


Module Name:

    RawPdo.c

Abstract: This module have the code enumerate a raw PDO for every device
          the filter attaches to so that it can provide a direct
          sideband communication with the usermode application.

          The toaster filter driver sample demonstrates an alternation
          approach where you can create one control-device for all the
          instances of the filter device.

Environment:

    Kernel mode only.

--*/

#include "kbfiltr.h"
#include "public.h"

VOID
KbFilter_EvtIoDeviceControlForRawPdo(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
/*++

Routine Description:

    This routine is the dispatch routine for device control requests.

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
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE parent = WdfIoQueueGetDevice(Queue);
    PRPDO_DEVICE_DATA pdoData;
    WDF_REQUEST_FORWARD_OPTIONS forwardOptions;
        
    pdoData = PdoGetData(parent);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    DebugPrint(("Entered KbFilter_EvtIoDeviceControlForRawPdo\n"));

    //
    // Process the ioctl and complete it when you are done.
    // Since the queue is configured for serial dispatch, you will
    // not receive another ioctl request until you complete this one.
    //

    switch (IoControlCode) {
    case IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES:
        WDF_REQUEST_FORWARD_OPTIONS_INIT(&forwardOptions);
        status = WdfRequestForwardToParentDeviceIoQueue(Request, pdoData->ParentQueue, &forwardOptions);
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(Request, status);
        }
        break;
    default:
        WdfRequestComplete(Request, status);
        break;
    }

    return;
}

#define MAX_ID_LEN 128

NTSTATUS
KbFiltr_CreateRawPdo(
    WDFDEVICE       Device,
    ULONG           InstanceNo
    )
{
    NTSTATUS                    status;
    PWDFDEVICE_INIT             pDeviceInit = NULL;
    PRPDO_DEVICE_DATA           pdoData = NULL;
    WDFDEVICE                   hChild = NULL;
    WDF_OBJECT_ATTRIBUTES       pdoAttributes;
    WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
    WDF_IO_QUEUE_CONFIG         ioQueueConfig;
    WDFQUEUE                    queue;
    WDF_DEVICE_STATE            deviceState;
    PDEVICE_EXTENSION           devExt;
    DECLARE_CONST_UNICODE_STRING(deviceId, KBFILTR_DEVICE_ID);
    DECLARE_CONST_UNICODE_STRING(hardwareId, KBFILTR_DEVICE_ID);
    DECLARE_CONST_UNICODE_STRING(deviceLocation, L"Keyboard Filter\0");
    DECLARE_UNICODE_STRING_SIZE(buffer, MAX_ID_LEN);

    DebugPrint(("Entered KbFiltr_CreateRawPdo\n"));

    pDeviceInit = WdfPdoInitAllocate(Device);

    if (pDeviceInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    // Assign Raw Device
    status = WdfPdoInitAssignRawDevice(pDeviceInit, &GUID_DEVCLASS_KEYBOARD);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    // Set Security Descriptor
    status = WdfDeviceInitAssignSDDLString(pDeviceInit, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    // Assign Device ID
    status = WdfPdoInitAssignDeviceID(pDeviceInit, &deviceId);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    // Assign Hardware ID (optional for compatibility)
    if (!RtlIsNtDdiVersionAvailable(NTDDI_WINXP)) {
        status = WdfPdoInitAddHardwareID(pDeviceInit, &hardwareId);
        if (!NT_SUCCESS(status)) {
            goto Cleanup;
        }
    }

    // Assign Instance ID
    status = RtlUnicodeStringPrintf(&buffer, L"%02d", InstanceNo);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    status = WdfPdoInitAssignInstanceID(pDeviceInit, &buffer);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    // Add Device Text
    status = RtlUnicodeStringPrintf(&buffer, L"Keyboard_Filter_%02d", InstanceNo);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    status = WdfPdoInitAddDeviceText(pDeviceInit, &buffer, &deviceLocation, 0x409);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    // Set Default Locale
    WdfPdoInitSetDefaultLocale(pDeviceInit, 0x409);

    // Allow Forwarding Requests to Parent
    WdfPdoInitAllowForwardingRequestToParent(pDeviceInit);

    // Create PDO
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&pdoAttributes, RPDO_DEVICE_DATA);
    status = WdfDeviceCreate(&pDeviceInit, &pdoAttributes, &hChild);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    pdoData = PdoGetData(hChild);
    pdoData->InstanceNo = InstanceNo;
    devExt = FilterGetData(Device);
    pdoData->ParentQueue = devExt->rawPdoQueue;

    // Configure Queue for PDO
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchSequential);
    ioQueueConfig.EvtIoDeviceControl = KbFilter_EvtIoDeviceControlFromRawPdo;

    status = WdfIoQueueCreate(hChild, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfIoQueueCreate failed 0x%x\n", status));
        goto Cleanup;
    }

    // Set PNP Capabilities
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.Removable = WdfTrue;
    pnpCaps.SurpriseRemovalOK = WdfTrue;
    pnpCaps.NoDisplayInUI = WdfTrue;
    pnpCaps.Address = InstanceNo;
    pnpCaps.UINumber = InstanceNo;

    WdfDeviceSetPnpCapabilities(hChild, &pnpCaps);

    // Set Device State
    WDF_DEVICE_STATE_INIT(&deviceState);
    deviceState.DontDisplayInUI = WdfTrue;
    WdfDeviceSetDeviceState(hChild, &deviceState);

    // Create Device Interface for PDO
    status = WdfDeviceCreateDeviceInterface(hChild, &GUID_DEVINTERFACE_KBFILTER, NULL);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfDeviceCreateDeviceInterface failed 0x%x\n", status));
        goto Cleanup;
    }

    // Add PDO to Parent FDO
    status = WdfFdoAddStaticChild(Device, hChild);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    return STATUS_SUCCESS;

Cleanup:
    DebugPrint(("KbFiltr_CreateRawPdo failed 0x%x\n", status));
    if (pDeviceInit != NULL) {
        WdfDeviceInitFree(pDeviceInit);
    }
    if (hChild) {
        WdfObjectDelete(hChild);
    }
    return status;
}


