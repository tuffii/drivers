#ifndef _PUBLIC_H
#define _PUBLIC_H
#include <initguid.h>

extern "C" const GUID GUID_DEVINTERFACE_KBFILTER;

typedef struct _KEY_MAPPING {
    UCHAR OriginalKey;
    UCHAR MappedKey;
} KEY_MAPPING, * PKEY_MAPPING;

#define IOCTL_KBFILTR_GET_KEYBOARD_ATTRIBUTES CTL_CODE( FILE_DEVICE_KEYBOARD,   \
                                                        IOCTL_INDEX,    \
                                                        METHOD_BUFFERED,    \
                                                        FILE_READ_DATA)

#define IOCTL_KBFILTR_WRITE_MESSAGE CTL_CODE( FILE_DEVICE_KEYBOARD,   \
                                              IOCTL_INDEX + 1,       \
                                              METHOD_BUFFERED,       \
                                              FILE_WRITE_DATA)

#define IOCTL_KBFILTR_SET_KEY_MAPPING CTL_CODE( FILE_DEVICE_KEYBOARD,   \
                                                 IOCTL_INDEX + 2,       \
                                                 METHOD_BUFFERED,       \
                                                 FILE_WRITE_DATA)

#define IOCTL_KBFILTR_CLEAR_KEY_MAPPING CTL_CODE( FILE_DEVICE_KEYBOARD,   \
                                                 IOCTL_INDEX + 3,       \
                                                 METHOD_BUFFERED,       \
                                                 FILE_WRITE_DATA)

DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
    0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);

#endif

