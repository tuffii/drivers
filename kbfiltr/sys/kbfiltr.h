#ifndef KBFILTER_H
#define KBFILTER_H

#pragma warning(disable:4201)

#include "ntddk.h"
#include "kbdmou.h"
#include <ntddkbd.h>
#include <ntdd8042.h>

#pragma warning(default:4201)

#include <wdf.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include <initguid.h>
#include <devguid.h>

#include "public.h"

#define MAX_KEY_MAPPINGS 128

#define KBFILTER_POOL_TAG (ULONG) 'tlfK'

#if DBG

#define TRAP()                      DbgBreakPoint()

#define DebugPrint(_x_) DbgPrint _x_

#else

#define TRAP()

#define DebugPrint(_x_)

#endif

#define MIN(_A_,_B_) (((_A_) < (_B_)) ? (_A_) : (_B_))

typedef struct _KEY_MAPPING {
    UCHAR OriginalKey;
    UCHAR MappedKey;
} KEY_MAPPING, *PKEY_MAPPING;

#define MAX_KEY_MAPPINGS 256 // Максимальное количество маппингов

// Глобальная структура для хранения маппингов
static struct {
    KEY_MAPPING KeyMappings[MAX_KEY_MAPPINGS];
    size_t KeyMappingsCount;
} GlobalKeyMappingStorage = { 0 };

typedef struct _DEVICE_EXTENSION {
    WDFDEVICE WdfDevice;
    WDFQUEUE rawPdoQueue;
    LONG EnableCount;
    CONNECT_DATA UpperConnectData;
    PVOID UpperContext;
    PI8042_KEYBOARD_INITIALIZATION_ROUTINE UpperInitializationRoutine;
    PI8042_KEYBOARD_ISR UpperIsrHook;
    IN PI8042_ISR_WRITE_PORT IsrWritePort;
    IN PI8042_QUEUE_PACKET QueueKeyboardPacket;
    IN PVOID CallContext;
    KEYBOARD_ATTRIBUTES KeyboardAttributes;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION,
                                        FilterGetData)


typedef struct _WORKER_ITEM_CONTEXT {

    WDFREQUEST  Request;
    WDFIOTARGET IoTarget;

} WORKER_ITEM_CONTEXT, *PWORKER_ITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WORKER_ITEM_CONTEXT, GetWorkItemContext)

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD KbFilter_EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL KbFilter_EvtIoDeviceControlForRawPdo;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL KbFilter_EvtIoDeviceControlFromRawPdo;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KbFilter_EvtIoInternalDeviceControl;

NTSTATUS
KbFilter_InitializationRoutine(
    IN PVOID                           InitializationContext,
    IN PVOID                           SynchFuncContext,
    IN PI8042_SYNCH_READ_PORT          ReadPort,
    IN PI8042_SYNCH_WRITE_PORT         WritePort,
    OUT PBOOLEAN                       TurnTranslationOn
    );

BOOLEAN
KbFilter_IsrHook(
    PVOID                  IsrContext,
    PKEYBOARD_INPUT_DATA   CurrentInput,
    POUTPUT_PACKET         CurrentOutput,
    UCHAR                  StatusByte,
    PUCHAR                 DataByte,
    PBOOLEAN               ContinueProcessing,
    PKEYBOARD_SCAN_STATE   ScanState
    );

VOID
KbFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

EVT_WDF_REQUEST_COMPLETION_ROUTINE
KbFilterRequestCompletionRoutine;

//
// Used to identify kbfilter bus. This guid is used as the enumeration string
// for the device id.
DEFINE_GUID(GUID_BUS_KBFILTER,
0xa65c87f9, 0xbe02, 0x4ed9, 0x92, 0xec, 0x1, 0x2d, 0x41, 0x61, 0x69, 0xfa);
// {A65C87F9-BE02-4ed9-92EC-012D416169FA}

DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);
// {3FB7299D-6847-4490-B0C9-99E0986AB886}


#define  KBFILTR_DEVICE_ID L"{A65C87F9-BE02-4ed9-92EC-012D416169FA}\\KeyboardFilter\0"


typedef struct _RPDO_DEVICE_DATA
{

    ULONG InstanceNo;
    WDFQUEUE ParentQueue;

} RPDO_DEVICE_DATA, *PRPDO_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RPDO_DEVICE_DATA, PdoGetData)


NTSTATUS
KbFiltr_CreateRawPdo(
    WDFDEVICE       Device,
    ULONG           InstanceNo
);



#endif
