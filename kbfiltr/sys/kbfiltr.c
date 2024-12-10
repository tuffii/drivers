#include "kbfiltr.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, KbFilter_EvtDeviceAdd)
#pragma alloc_text (PAGE, KbFilter_EvtIoInternalDeviceControl)
#endif

ULONG InstanceNo = 0;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG               config;
    NTSTATUS                        status;

    DebugPrint(("Keyboard Filter Driver Sample - Driver Framework Edition.\n"));
    DebugPrint(("Built %s %s\n", __DATE__, __TIME__));

    WDF_DRIVER_CONFIG_INIT(
        &config,
        KbFilter_EvtDeviceAdd
    );

    status = WdfDriverCreate(DriverObject,
                            RegistryPath,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &config,
                            WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfDriverCreate failed with status 0x%x\n", status));
    }

    return status;
}

NTSTATUS
KbFilter_EvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    NTSTATUS                status;
    WDFDEVICE               hDevice;
    WDFQUEUE                hQueue;
    PDEVICE_EXTENSION       filterExt;
    WDF_IO_QUEUE_CONFIG     ioQueueConfig;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    DebugPrint(("Enter FilterEvtDeviceAdd \n"));

    WdfFdoInitSetFilter(DeviceInit);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_KEYBOARD);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &hDevice);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("WdfDeviceCreate failed with status code 0x%x\n", status));
        return status;
    }

    filterExt = FilterGetData(hDevice);
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    ioQueueConfig.EvtIoInternalDeviceControl = KbFilter_EvtIoInternalDeviceControl;

    status = WdfIoQueueCreate(hDevice,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            WDF_NO_HANDLE
                            );
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig,
                             WdfIoQueueDispatchParallel);

    ioQueueConfig.EvtIoDeviceControl = KbFilter_EvtIoDeviceControlFromRawPdo;

    status = WdfIoQueueCreate(hDevice,
                            &ioQueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &hQueue
                            );
    if (!NT_SUCCESS(status)) {
        DebugPrint( ("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    filterExt->rawPdoQueue = hQueue;
    status = KbFiltr_CreateRawPdo(hDevice, ++InstanceNo);

    return status;
}

VOID
KbFilter_EvtIoDeviceControlFromRawPdo(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE hDevice;
    WDFMEMORY outputMemory;
    PDEVICE_EXTENSION devExt;
    size_t bytesTransferred = 0;

    PVOID inputBuffer = NULL;
    size_t bufferLength = 0;
    size_t newMappingsCount = 0;
    size_t i = 0;
    KEY_MAPPING* dest = NULL;

    UNREFERENCED_PARAMETER(InputBufferLength);

    DebugPrint(("Enteredd KbFilter_EvtIoInternalDeviceControl\n"));

    hDevice = WdfIoQueueGetDevice(Queue);
    if (hDevice == NULL) {
        DebugPrint(("hDevice null\n"));
    }
    DebugPrint(("WdfIoQueueGetDevice returned device: %p\n", hDevice));

    devExt = PdoGetData(hDevice);
    if (devExt == NULL) {
        DebugPrint(("devExt null\n"));
    }

    switch (IoControlCode) {

    case IOCTL_KBFILTR_CLEAR_KEY_MAPPING:
        DebugPrint(("Clear data from user request\n"));
        GlobalKeyMappingStorage.KeyMappingsCount = 0;
        DebugPrint(("Key mappings cleared\n"));
        break;

    case IOCTL_KBFILTR_SET_KEY_MAPPING:
    DebugPrint(("Get data from user request\n"));
    status = WdfRequestRetrieveInputBuffer(Request, sizeof(KEY_MAPPING), &inputBuffer, &bufferLength);
    if (!NT_SUCCESS(status)) {
        DebugPrint(("Failed to retrieve input buffer: 0x%x\n", status));
        break;
    }

    DebugPrint(("check bufferLength %% sizeof(KEY_MAPPING) != 0\n"));
    if (bufferLength % sizeof(KEY_MAPPING) != 0) {
        DebugPrint(("Invalid input buffer size: %zu\n", bufferLength));
        status = STATUS_INVALID_BUFFER_SIZE;
        break;
    }

    newMappingsCount = bufferLength / sizeof(KEY_MAPPING);
    DebugPrint(("newMappingsCount = %zu\n", newMappingsCount));

    if (GlobalKeyMappingStorage.KeyMappingsCount + newMappingsCount > MAX_KEY_MAPPINGS) {
        DebugPrint(("Too many mappings provided. Current: %zu, New: %zu, Max: %zu\n",
                    GlobalKeyMappingStorage.KeyMappingsCount,
                    newMappingsCount,
                    MAX_KEY_MAPPINGS));
        status = STATUS_INSUFFICIENT_RESOURCES;
        break;
    }

    DebugPrint(("Adding new mappings to storage\n"));
    dest = GlobalKeyMappingStorage.KeyMappings + GlobalKeyMappingStorage.KeyMappingsCount;
    RtlCopyMemory(dest, inputBuffer, bufferLength);
    GlobalKeyMappingStorage.KeyMappingsCount += newMappingsCount;

    for (i = 0; i < GlobalKeyMappingStorage.KeyMappingsCount; i++) {
        DebugPrint(("Mapping [%zu]: OriginalKey=0x%02X, MappedKey=0x%02X\n",
                    i,
                    GlobalKeyMappingStorage.KeyMappings[i].OriginalKey,
                    GlobalKeyMappingStorage.KeyMappings[i].MappedKey));
    }
    break;






    case IOCTL_KBFILTR_WRITE_MESSAGE:
        DebugPrint(("Entered case IOCTL_KBFILTR_WRITE_MESSAGE\n"));
        // Получаем данные из пользовательской программы
        status = WdfRequestRetrieveInputBuffer(Request, 1, &inputBuffer, &bufferLength);
        if (!NT_SUCCESS(status)) {
            DebugPrint(("Failed to retrieve input buffer: 0x%x\n", status));
            break;
        }
        DebugPrint(("Received message: %.*s\n", (int)bufferLength, (char*)inputBuffer));
        break;

    case IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES:
        DebugPrint(("Entered case IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES\n"));
        if (OutputBufferLength < sizeof(KEYBOARD_ATTRIBUTES)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
        
        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfRequestRetrieveOutputMemory failed %x\n", status));
            break;
        }
        
        status = WdfMemoryCopyFromBuffer(outputMemory,
                                    0,
                                    &devExt->KeyboardAttributes,
                                    sizeof(KEYBOARD_ATTRIBUTES));

        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfMemoryCopyFromBuffer failed %x\n", status));
            break;
        }

        bytesTransferred = sizeof(KEYBOARD_ATTRIBUTES);
        break;

    default:
        DebugPrint(("Entered case DEFAULT\n"));
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }
    
    WdfRequestCompleteWithInformation(Request, status, bytesTransferred);

    DebugPrint(("returned KbFilter_EvtIoInternalDeviceControl\n"));

    return;
}


VOID
KbFilter_EvtIoInternalDeviceControl(
    IN WDFQUEUE      Queue,
    IN WDFREQUEST    Request,
    IN size_t        OutputBufferLength,
    IN size_t        InputBufferLength,
    IN ULONG         IoControlCode
    )
{
    PDEVICE_EXTENSION               devExt;
    PINTERNAL_I8042_HOOK_KEYBOARD   hookKeyboard = NULL;
    PCONNECT_DATA                   connectData = NULL;
    NTSTATUS                        status = STATUS_SUCCESS;
    size_t                          length;
    WDFDEVICE                       hDevice;
    BOOLEAN                         forwardWithCompletionRoutine = FALSE;
    BOOLEAN                         ret = TRUE;
    WDFCONTEXT                      completionContext = WDF_NO_CONTEXT;
    WDF_REQUEST_SEND_OPTIONS        options;
    WDFMEMORY                       outputMemory;
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    PAGED_CODE();

    DebugPrint(("Entered KbFilter_EvtIoInternalDeviceControl\n"));

    hDevice = WdfIoQueueGetDevice(Queue);
    devExt = FilterGetData(hDevice);

    switch (IoControlCode) {
    case IOCTL_INTERNAL_KEYBOARD_CONNECT:
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }
        status = WdfRequestRetrieveInputBuffer(Request,
                                    sizeof(CONNECT_DATA),
                                    &connectData,
                                    &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }

        ASSERT(length == InputBufferLength);

        devExt->UpperConnectData = *connectData;

        connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);

#pragma warning(disable:4152)

        connectData->ClassService = KbFilter_ServiceCallback;

#pragma warning(default:4152)

        break;

    case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:

        status = STATUS_NOT_IMPLEMENTED;
        break;

    case IOCTL_INTERNAL_I8042_HOOK_KEYBOARD:

        DebugPrint(("hook keyboard received!\n"));

        status = WdfRequestRetrieveInputBuffer(Request,
                            sizeof(INTERNAL_I8042_HOOK_KEYBOARD),
                            &hookKeyboard,
                            &length);
        if(!NT_SUCCESS(status)){
            DebugPrint(("WdfRequestRetrieveInputBuffer failed %x\n", status));
            break;
        }

        ASSERT(length == InputBufferLength);

        devExt->UpperContext = hookKeyboard->Context;

        hookKeyboard->Context = (PVOID) devExt;

        if (hookKeyboard->InitializationRoutine) {
            devExt->UpperInitializationRoutine =
                hookKeyboard->InitializationRoutine;
        }
        hookKeyboard->InitializationRoutine =
            (PI8042_KEYBOARD_INITIALIZATION_ROUTINE)
            KbFilter_InitializationRoutine;

        if (hookKeyboard->IsrRoutine) {
            devExt->UpperIsrHook = hookKeyboard->IsrRoutine;
        }
        hookKeyboard->IsrRoutine = (PI8042_KEYBOARD_ISR) KbFilter_IsrHook;

        devExt->IsrWritePort = hookKeyboard->IsrWritePort;
        devExt->QueueKeyboardPacket = hookKeyboard->QueueKeyboardPacket;
        devExt->CallContext = hookKeyboard->CallContext;

        status = STATUS_SUCCESS;
        break;


    case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
        forwardWithCompletionRoutine = TRUE;
        completionContext = devExt;
        break;
        
    case IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION:
    case IOCTL_KEYBOARD_QUERY_INDICATORS:
    case IOCTL_KEYBOARD_SET_INDICATORS:
    case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
    case IOCTL_KEYBOARD_SET_TYPEMATIC:
        break;
    }

    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return;
    }

    if (forwardWithCompletionRoutine) {
        
        status = WdfRequestRetrieveOutputMemory(Request, &outputMemory); 

        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfRequestRetrieveOutputMemory failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
            return;
        }

        status = WdfIoTargetFormatRequestForInternalIoctl(WdfDeviceGetIoTarget(hDevice),
                                                         Request,
                                                         IoControlCode,
                                                         NULL,
                                                         NULL,
                                                         outputMemory,
                                                         NULL);

        if (!NT_SUCCESS(status)) {
            DebugPrint(("WdfIoTargetFormatRequestForInternalIoctl failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
            return;
        }
    
        WdfRequestSetCompletionRoutine(Request,
                                    KbFilterRequestCompletionRoutine,
                                    completionContext);

        ret = WdfRequestSend(Request,
                             WdfDeviceGetIoTarget(hDevice),
                             WDF_NO_SEND_OPTIONS);

        if (ret == FALSE) {
            status = WdfRequestGetStatus (Request);
            DebugPrint( ("WdfRequestSend failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
        }

    }
    else
    {
        WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                      WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

        ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), &options);

        if (ret == FALSE) {
            status = WdfRequestGetStatus (Request);
            DebugPrint(("WdfRequestSend failed: 0x%x\n", status));
            WdfRequestComplete(Request, status);
        }
        
    }

    return;
}

NTSTATUS
KbFilter_InitializationRoutine(
    IN PVOID                           InitializationContext,
    IN PVOID                           SynchFuncContext,
    IN PI8042_SYNCH_READ_PORT          ReadPort,
    IN PI8042_SYNCH_WRITE_PORT         WritePort,
    OUT PBOOLEAN                       TurnTranslationOn
    )
{
    PDEVICE_EXTENSION  devExt;
    NTSTATUS            status = STATUS_SUCCESS;

    devExt = (PDEVICE_EXTENSION)InitializationContext;

    if (devExt->UpperInitializationRoutine) {
        status = (*devExt->UpperInitializationRoutine) (
                        devExt->UpperContext,
                        SynchFuncContext,
                        ReadPort,
                        WritePort,
                        TurnTranslationOn
                        );

        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    *TurnTranslationOn = TRUE;
    return status;
}

BOOLEAN
KbFilter_IsrHook(
    PVOID                  IsrContext,
    PKEYBOARD_INPUT_DATA   CurrentInput,
    POUTPUT_PACKET         CurrentOutput,
    UCHAR                  StatusByte,
    PUCHAR                 DataByte,
    PBOOLEAN               ContinueProcessing,
    PKEYBOARD_SCAN_STATE   ScanState
    )
{
    PDEVICE_EXTENSION devExt;
    BOOLEAN           retVal = TRUE;

    devExt = (PDEVICE_EXTENSION)IsrContext;

    if (devExt->UpperIsrHook) {
        retVal = (*devExt->UpperIsrHook) (
                        devExt->UpperContext,
                        CurrentInput,
                        CurrentOutput,
                        StatusByte,
                        DataByte,
                        ContinueProcessing,
                        ScanState
                        );

        if (!retVal || !(*ContinueProcessing)) {
            return retVal;
        }
    }

    *ContinueProcessing = TRUE;
    return retVal;
}

VOID
KbFilter_ServiceCallback(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
{
    PDEVICE_EXTENSION devExt;
    PKEYBOARD_INPUT_DATA currentInputData;
    size_t i = 0;

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    currentInputData = InputDataStart;

    // Обходим каждый ввод
    while (currentInputData < InputDataEnd) {
        KdPrint(("kbfiltr: KeyCode: 0x%04X, Flags: 0x%04X\n",
                 currentInputData->MakeCode, currentInputData->Flags));

        // Проверяем, нужно ли заменить текущий код клавиши
        for (i = 0; i < GlobalKeyMappingStorage.KeyMappingsCount; i++) {
            if (currentInputData->MakeCode == GlobalKeyMappingStorage.KeyMappings[i].OriginalKey) {
                KdPrint(("kbfiltr: Replacing KeyCode 0x%04X with 0x%04X\n",
                         currentInputData->MakeCode,
                         GlobalKeyMappingStorage.KeyMappings[i].MappedKey));

                currentInputData->MakeCode = GlobalKeyMappingStorage.KeyMappings[i].MappedKey;
                break;
            }
        }

        currentInputData++;
    }

    // Передаём данные о вводе клавиатуры дальше
    (*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
    );
}

VOID
KbFilterRequestCompletionRoutine(
    WDFREQUEST                  Request,
    WDFIOTARGET                 Target,
    PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    WDFCONTEXT                  Context
   )
{
    WDFMEMORY buffer = CompletionParams->Parameters.Ioctl.Output.Buffer;

    UNREFERENCED_PARAMETER(Target);
    if (CompletionParams->Type == WdfRequestTypeDeviceControlInternal &&
        NT_SUCCESS(CompletionParams->IoStatus.Status) && 
        CompletionParams->Parameters.Ioctl.IoControlCode == IOCTL_KEYBOARD_QUERY_ATTRIBUTES) {

        if( CompletionParams->Parameters.Ioctl.Output.Length >= sizeof(KEYBOARD_ATTRIBUTES)) {
            WdfMemoryCopyToBuffer(buffer,
                                  CompletionParams->Parameters.Ioctl.Output.Offset,
                                  &((PDEVICE_EXTENSION)Context)->KeyboardAttributes,
                                  sizeof(KEYBOARD_ATTRIBUTES)
                                  );
        }
    }

    WdfRequestComplete(Request, CompletionParams->IoStatus.Status);

    return;
}


