// ConsoleCommunicationTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS  

#include <iostream>
#include <cstdio>
#include <Windows.h>
#include "shared.h"
#include <thread>
#include <mutex>
#include <condition_variable>

typedef struct _OVL_WRAPPER {
    OVERLAPPED  Overlapped;
    //CUSTOM_KEYBOARD_INPUT        ReturnedSequence;
} OVL_WRAPPER, * POVL_WRAPPER;

//moodycamel::BlockingConcurrentQueue<CUSTOM_KEYBOARD_INPUT> q;

DWORD WINAPI CompletionPortThread(LPVOID PortHandle);

/*void openAsyncIo(HANDLE *handle) {
    DWORD code;

    if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
        std::cout << "Invalid handle" << std::endl;
        return;
    }
    
    PCUSTOM_KEYBOARD_INPUT rawInput = (PCUSTOM_KEYBOARD_INPUT)malloc(2 * sizeof(CUSTOM_KEYBOARD_INPUT));
    DWORD keysNumber = 0;
    //DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE), nullptr, 0, &wrap->OutputBuffer, wrap->OutputBufferSize, nullptr, &wrap->Overlapped);
    DeviceIoControl(*handle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE), NULL, 0, rawInput, (DWORD)2 * sizeof(CUSTOM_KEYBOARD_INPUT), &keysNumber, NULL);

    keysNumber /= sizeof(CUSTOM_KEYBOARD_INPUT);

    code = GetLastError();
    if (code != ERROR_IO_PENDING && code != ERROR_SUCCESS) {
        printf("DeviceIoControl failed with error 0x%lx\n", code);
        free(rawInput);
        return;
    }

    //q.enqueue_bulk(rawInput, keysNumber);
    for (unsigned int i = 0; i < keysNumber; i++) {
        rawInput[i].MakeCode = 30;
    }
    if (!DeviceIoControl(*handle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_SENDKEYPRESS), rawInput, (DWORD) keysNumber * sizeof(CUSTOM_KEYBOARD_INPUT), NULL, 0, NULL, NULL)) {
        code = GetLastError();

        if (code != ERROR_IO_PENDING) {
            printf("Fail to send key press with error 0x%lx\n", code);
        }
    }

    free(rawInput);

}*/

void startConnectionHandle(DWORD dwThreadId, HANDLE* handle) {
    DWORD code;
    *handle = CreateFile(LR"(\\.\KeyboardFilter)", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (*handle == INVALID_HANDLE_VALUE) {
        code = GetLastError();

        std::cout << "CreateFile failed with error" << code << std::endl;
        return;
    }
}


HANDLE driverHandle;

/*DWORD openIoctl() {

    if (driverHandle == NULL || driverHandle == INVALID_HANDLE_VALUE) {
        std::cout << "Invalid handle" << std::endl;
        return ERROR_HANDLE_NO_LONGER_VALID;
    }

    POVL_WRAPPER wrap = static_cast<POVL_WRAPPER>(malloc(sizeof(OVL_WRAPPER)));
    memset(wrap, 0, sizeof(OVL_WRAPPER));

    DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE), NULL, 0, &wrap->ReturnedSequence, sizeof(CUSTOM_KEYBOARD_INPUT), NULL, &wrap->Overlapped);
    DWORD code = GetLastError();

    if (code != ERROR_IO_PENDING) {

        printf("DeviceIoControl failed with error 0x%lx\n", code);

        return(code);

    }
}*/

int main()
{
    HANDLE completionPortHandle;
    DWORD code;
    DWORD dwThreadId = 0;
    HANDLE hThread;
    DWORD Function = 0;

    std::condition_variable cv;

    startConnectionHandle(dwThreadId, &driverHandle);
    if (driverHandle == INVALID_HANDLE_VALUE) {

        code = GetLastError();

        printf("CreateFile failed with error 0x%lx\n",
            code);

        return(code);
    }

    completionPortHandle = CreateIoCompletionPort(driverHandle, NULL, 0, 0);
    if (completionPortHandle == nullptr) {

        code = GetLastError();

        printf("CreateIoCompletionPort failed with error 0x%lx\n", code);

        return(code);

    }

    hThread = CreateThread(NULL,               // Default thread security descriptor
        0,                     // Default stack size
        CompletionPortThread,  // Start routine
        completionPortHandle,  // Start routine parameter
        0,                     // Run immediately
        &dwThreadId);          // Thread ID

    if (hThread == nullptr) {
        code = GetLastError();

        printf("CreateThread failed with error 0x%lx\n", code);

        return(code);
    }

    std::cout << "Started to listen for keys, main thread id: " << GetCurrentThreadId() << std::endl;

    INPUT_KEYBOARD_MACRO keyboard, keyboard2;
    memset(&keyboard, 0, sizeof(INPUT_KEYBOARD_MACRO));
    memset(&keyboard2, 0, sizeof(INPUT_KEYBOARD_MACRO));
    keyboard.DeviceID = 1;
    keyboard.ReplacedKeyScanCode = 0x1e;

    INPUT_KEYBOARD_KEY replacingKeys[8];
    replacingKeys[0].MakeCode = 0x14; replacingKeys[0].Flags = RI_KEY_MAKE;
    replacingKeys[1].MakeCode = 0x14; replacingKeys[1].Flags = RI_KEY_BREAK;
    replacingKeys[2].MakeCode = 0x12; replacingKeys[2].Flags = RI_KEY_MAKE;
    replacingKeys[3].MakeCode = 0x12; replacingKeys[3].Flags = RI_KEY_BREAK;
    replacingKeys[4].MakeCode = 0x1f; replacingKeys[4].Flags = RI_KEY_MAKE;
    replacingKeys[5].MakeCode = 0x1f; replacingKeys[5].Flags = RI_KEY_BREAK;
    replacingKeys[6].MakeCode = 0x14; replacingKeys[6].Flags = RI_KEY_MAKE;
    replacingKeys[7].MakeCode = 0x14; replacingKeys[7].Flags = RI_KEY_BREAK;

    size_t bufferSize = sizeof(INPUT_KEYBOARD_MACRO) + (8 * sizeof(INPUT_KEYBOARD_KEY));

    unsigned char* bytePointer = (unsigned char*)malloc(bufferSize);
    memcpy(bytePointer, &keyboard, sizeof(INPUT_KEYBOARD_MACRO));
    memcpy(bytePointer + sizeof(INPUT_KEYBOARD_MACRO), replacingKeys, 8 * sizeof(INPUT_KEYBOARD_KEY));

    memcpy(&keyboard2, bytePointer, sizeof(INPUT_KEYBOARD_MACRO));

    std::cout << keyboard2.DeviceID << " " << keyboard2.ReplacedKeyScanCode << std::endl;

    DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_ADDMACRO), bytePointer, bufferSize, NULL, 0, NULL, NULL);

    free(bytePointer);

    while (TRUE) {
        int number;
        scanf("%d", &number);
    }

}

DWORD WINAPI CompletionPortThread(LPVOID PortHandle) {
    DWORD byteCount = 0;
    ULONG_PTR compKey = 0;
    OVERLAPPED* overlapped = NULL;
    DWORD code;
    POVL_WRAPPER wrap;

    while (TRUE) {
        overlapped = NULL;
        BOOL worked = GetQueuedCompletionStatus(PortHandle, &byteCount, &compKey, &overlapped, INFINITE);

        if (byteCount == 0) {
            continue;
        }

        if (overlapped == NULL) {
            continue;
        }

        /*wrap = reinterpret_cast<POVL_WRAPPER>(overlapped);
        code = GetLastError();

        wrap->ReturnedSequence.MakeCode = 30;
        if (!DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_SENDKEYPRESS), &wrap->ReturnedSequence, (DWORD)1 * sizeof(CUSTOM_KEYBOARD_INPUT), NULL, 0, NULL, NULL)) {
            code = GetLastError();

            if (code != ERROR_IO_PENDING) {
                printf("Fail to send key press with error 0x%lx\n", code);
            }
        }

        openIoctl();*/
    }
}
