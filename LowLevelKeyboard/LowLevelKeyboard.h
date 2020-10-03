#pragma once

#include <Windows.h>
#include "shared.h"

#ifndef EXPORT
#define EXPORT _declspec(dllexport)
#endif

DWORD WINAPI Llk(HMODULE hModule);

// extern void (*Llk_KeyPressCallback)(PCUSTOM_KEYBOARD_INPUT input, DWORD size);
extern void (*Llk_KeyPressCallback)(CUSTOM_KEYBOARD_INPUT input);

typedef struct _OVL_WRAPPER {
	OVERLAPPED  Overlapped;
	CUSTOM_KEYBOARD_INPUT        ReturnedSequence;
} OVL_WRAPPER, * POVL_WRAPPER;

DWORD WINAPI Llk_Internal_CompletionPortThread(LPVOID PortHandle);
DWORD Llk_Internal_startConnectionHandle(DWORD dwThreadId, HANDLE* handle);
DWORD Llk_Internal_openQueueIoctl();

#ifdef __cplusplus
extern "C" {
#endif

	// EXPORT void Llk_RegisterKeyPressCallback(DWORD maxSize, void(*cb)(PCUSTOM_KEYBOARD_INPUT input, DWORD size));
	EXPORT void Llk_RegisterKeyPressCallback(void(*cb)(CUSTOM_KEYBOARD_INPUT input));
	EXPORT void Llk_SendKeyPress(PCUSTOM_KEYBOARD_INPUT input, DWORD size);

#ifdef __cplusplus
}
#endif