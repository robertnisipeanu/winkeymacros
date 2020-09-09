// ConsoleCommunicationTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS  

#include <iostream>
#include <cstdio>
#include <Windows.h>
#include "shared.h"

typedef struct _OVL_WRAPPER {
    OVERLAPPED  Overlapped;
    LONG        ReturnedSequence;
} OVL_WRAPPER, * POVL_WRAPPER;

HANDLE driverHandle;

DWORD WINAPI CompletionPortThread(LPVOID PortHandle);

int main()
{
    HANDLE completionPortHandle;
    HANDLE hThread;
    DWORD dwThreadId = 0;
    DWORD code;
    DWORD Function = 0;
    POVL_WRAPPER wrap;

    driverHandle = CreateFile(LR"(\\.\KeyboardFilter)", GENERIC_READ|GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

    if (driverHandle == INVALID_HANDLE_VALUE) {
        code = GetLastError();

        std::cout << "CreateFile failed with error" << code << std::endl;
        return code;
    }

    completionPortHandle = CreateIoCompletionPort(driverHandle, nullptr, 0, 0);
    if (completionPortHandle == nullptr) {
        code = GetLastError();
        std::cout << "CreateIoCompletionPort failed with error 0x" << code << std::endl;
        return code;
    }

    hThread = CreateThread(nullptr, 0, CompletionPortThread, completionPortHandle, 0, &dwThreadId);

    if (hThread == nullptr) {
        code = GetLastError();

        std::cout << "CreateThread failed with error 0x" << code << std::endl;
        return code;
    }

    while (TRUE) {

        printf("\n\t1. Asynchronously send IOCTL_OSR_INVERT_NOTIFICATION\n");
        printf("\t2. Cause an event notification\n");
        printf("\n\t0. Exit\n");
        printf("\n\tSelection: ");
        scanf("%lx", &Function);

        switch (Function) {


        case 1:
        {
            wrap = static_cast<POVL_WRAPPER>(malloc(sizeof(OVL_WRAPPER)));
            memset(wrap, 0, sizeof(OVL_WRAPPER));

            //
            // Test the IOCTL interface
            //
            DeviceIoControl(driverHandle,
                static_cast<DWORD>(IOCTL_OSR_INVERT_NOTIFICATION),
                nullptr,                      // Ptr to InBuffer
                0,                            // Length of InBuffer
                &wrap->ReturnedSequence,      // Ptr to OutBuffer
                sizeof(LONG),                 // Length of OutBuffer
                nullptr,                      // BytesReturned
                &wrap->Overlapped);           // Ptr to Overlapped structure

            code = GetLastError();

            if (code != ERROR_IO_PENDING) {

                printf("DeviceIoControl failed with error 0x%lx\n", code);

                return(code);

            }

            break;
        }

        case 2:
        {
            wrap = static_cast<POVL_WRAPPER>(malloc(sizeof(OVL_WRAPPER)));
            memset(wrap, 0, sizeof(OVL_WRAPPER));

            if (!DeviceIoControl(driverHandle,
                static_cast<DWORD>(IOCTL_OSR_INVERT_SIMULATE_EVENT_OCCURRED),
                nullptr,                         // Ptr to InBuffer
                0,                            // Length of InBuffer
                nullptr,                         // Ptr to OutBuffer
                0,                            // Length of OutBuffer
                nullptr,                         // BytesReturned
                &wrap->Overlapped))          // Ptr to Overlapped structure
            {

                code = GetLastError();

                if (code != ERROR_IO_PENDING) {

                    printf("DeviceIoControl failed with error 0x%lx\n", code);

                    return(code);

                }
            }
            break;

        }

        case 0:
        {
            //
            // zero is get out!
            //
            return(0);
        }

        default:
        {
            //
            // Just re-prompt for anything else
            //
            break;
        }

        }
    }
}

DWORD WINAPI CompletionPortThread(LPVOID PortHandle) {
    DWORD byteCount = 0;
    ULONG_PTR compKey = 0;
    OVERLAPPED* overlapped = nullptr;
    POVL_WRAPPER wrap;
    DWORD code;

    while (TRUE) {
        overlapped = nullptr;

        BOOL worked = GetQueuedCompletionStatus(PortHandle, &byteCount, &compKey, &overlapped, INFINITE);

        //
        // If it's our notification ioctl that's jut been
        // completed, don't do anything special
        //
        if (byteCount == 0) {
            continue;
        }

        if (overlapped == nullptr) {

            // An unrecoverable error occurred in the completion port
            // Wait for the next notification
            continue;
        }

        //
        // Because the wrapper structure STARTS with the OVERLAPPED structure,
        // the pointers are the same. It would be nicer 
        // to use CONTAINING_RECORD here, however you do that in user-mode
        //
        wrap = reinterpret_cast<POVL_WRAPPER>(overlapped);

        code = GetLastError();

        std::cout << ">>> Notification received. Sequence = " << wrap->ReturnedSequence << std::endl;
    }
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
