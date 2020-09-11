// ConsoleCommunicationTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS  

#include <iostream>
#include <cstdio>
#include <Windows.h>
#include "shared.h"
#include <thread>


void openAsyncIo(HANDLE *handle) {
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
    }

    if (!DeviceIoControl(*handle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_SENDKEYPRESS), rawInput, (DWORD) keysNumber * sizeof(CUSTOM_KEYBOARD_INPUT), NULL, 0, NULL, NULL)) {
        code = GetLastError();

        if (code != ERROR_IO_PENDING) {
            printf("Fail to send key press with error 0x%lx\n", code);
        }
    }

    free(rawInput);

}

void startConnectionHandle(DWORD* dwThreadId, HANDLE* handle) {
    DWORD code;
    *handle = CreateFile(LR"(\\.\KeyboardFilter)", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (*handle == INVALID_HANDLE_VALUE) {
        code = GetLastError();

        std::cout << "CreateFile failed with error" << code << std::endl;
        return;
    }
}

void startThread() {
    DWORD dwThreadId = GetCurrentThreadId();

    HANDLE handle;
    startConnectionHandle(&dwThreadId, &handle);
    while (TRUE) {
        openAsyncIo(&handle);
    }
}

int main()
{
    DWORD Function = 0;


    std::cout << "Listening for keys..." << std::endl;

    for (int i = 0; i < 5; i++) {
        std::thread* th = new std::thread(startThread);
    }

    std::cout << "Created threads" << std::endl;

    while (TRUE) {
        scanf("%lx", &Function);
        //openAsyncIo();
    }

}
