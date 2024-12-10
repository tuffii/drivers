typedef struct IUnknown IUnknown;

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>

#include "public.h"

#pragma comment(lib, "SetupAPI.lib")

#define IOCTL_INDEX 0x800

#define MAX_KEY_MAPPINGS 128

int ParseHex(const char* input, USHORT* output) {
    return sscanf(input, "%hx", output);  // Парсинг шестнадцатеричного числа
}

void SendMessageToDriver(const char* devicePath, const char* message) {
    HANDLE file = CreateFileA(devicePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("Failed to open device: 0x%x\n", GetLastError());
        return;
    }

    DWORD bytesReturned;
    if (!DeviceIoControl(file,
        IOCTL_KBFILTR_WRITE_MESSAGE,
        (void*)message,
        (DWORD)strlen(message) + 1,
        NULL,
        0,
        &bytesReturned,
        NULL)) {
        printf("DeviceIoControl failed: 0x%x\n", GetLastError());
    }
    else {
        printf("Message sent successfully.\n");
    }

    CloseHandle(file);
}


void SendKeyMappingsToDriver(const char* devicePath, const KEY_MAPPING* mappings, size_t count) {

    HANDLE file = CreateFileA(devicePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("Failed to open device: 0x%x\n", GetLastError());
        return;
    }

    DWORD bytesReturned;
    if (!DeviceIoControl(file,
        IOCTL_KBFILTR_SET_KEY_MAPPING,
        (void*)mappings,
        (DWORD)(count * sizeof(KEY_MAPPING)),
        NULL,
        0,
        &bytesReturned,
        NULL)) {
        printf("DeviceIoControl failed: 0x%x\n", GetLastError());
    }
    else {
        printf("Key mappings sent successfully.\n");
    }

    CloseHandle(file);
}

void ClearKeyMappings(const char* devicePath) {
    HANDLE file = CreateFileA(devicePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("Failed to open device: 0x%x\n", GetLastError());
        return;
    }

    DWORD bytesReturned;
    if (!DeviceIoControl(file,
        IOCTL_KBFILTR_CLEAR_KEY_MAPPING,
        NULL, 0,
        NULL, 0,
        &bytesReturned,
        NULL)) {
        printf("Failed to send clear command: 0x%x\n", GetLastError());
    }
    else {
        printf("Key mappings cleared successfully.\n");
    }

    CloseHandle(file);
}

int main() {
    HDEVINFO hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_A deviceInterfaceDetailData = NULL;
    ULONG requiredLength = 0;

    KEY_MAPPING mappings[MAX_KEY_MAPPINGS];
    size_t count = 0;

    hardwareDeviceInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_KBFILTER,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (hardwareDeviceInfo == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed: 0x%x\n", GetLastError());
        return 1;
    }

    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    if (!SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, NULL, &GUID_DEVINTERFACE_KBFILTER, 0, &deviceInterfaceData)) {
        printf("SetupDiEnumDeviceInterfaces failed: 0x%x\n", GetLastError());
        SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
        return 1;
    }

    SetupDiGetDeviceInterfaceDetailA(hardwareDeviceInfo, &deviceInterfaceData, NULL, 0, &requiredLength, NULL);
    deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(requiredLength);
    if (!deviceInterfaceDetailData) {
        printf("Memory allocation failed.\n");
        SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
        return 1;
    }

    deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
    if (!SetupDiGetDeviceInterfaceDetailA(hardwareDeviceInfo, &deviceInterfaceData, deviceInterfaceDetailData, requiredLength, NULL, NULL)) {
        printf("SetupDiGetDeviceInterfaceDetail failed: 0x%x\n", GetLastError());
        free(deviceInterfaceDetailData);
        SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
        return 1;
    }

    SendMessageToDriver(deviceInterfaceDetailData->DevicePath, "USER PROGRAMM MESSAGE!!!");


    while (1) {
        printf("\nSelect an option:\n");
        printf("1. Send a key mapping\n");
        printf("2. Clear all key mappings\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting.\n");
            break;
        }

        switch (choice) {
        case 1: {
            char original[10], mapped[10];
            printf("Enter original key (e.g., 0x1E): ");
            if (scanf("%s", original) != 1) {
                printf("Invalid input.\n");
                break;
            }

            printf("Enter mapped key (e.g., 0x30): ");
            if (scanf("%s", mapped) != 1) {
                printf("Invalid input.\n");
                break;
            }

            USHORT originalKey, mappedKey;
            if (ParseHex(original, &originalKey) && ParseHex(mapped, &mappedKey)) {
                // Создаём временный массив для отправки
                KEY_MAPPING tempMapping = { (UCHAR)originalKey, (UCHAR)mappedKey };
                SendKeyMappingsToDriver(deviceInterfaceDetailData->DevicePath, &tempMapping, 1);
            }
            else {
                printf("Invalid input format. Please enter values in the format 0xXXXX.\n");
            }
            break;
        }

        case 2:
            ClearKeyMappings(deviceInterfaceDetailData->DevicePath);
            break;

        case 3:
            free(deviceInterfaceDetailData);
            SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
            return 0;

        default:
            printf("Invalid choice. Try again.\n");
        }
    }
}
