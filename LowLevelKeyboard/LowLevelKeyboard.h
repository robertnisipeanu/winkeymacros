#pragma once

#pragma comment(lib, "hid.lib")
#include <stdint.h>
#include <Windows.h>
#include <hidsdi.h>
#include <hidusage.h>
#include <string>
#include <vector>
#include <fstream>
#include <deque>
#include <wchar.h>

#ifndef EXPORT
#define EXPORT _declspec(dllexport)
#endif

extern bool (*LlkPress_Callback)(struct KeyboardEventArgs args);

struct KeyboardStruct {
	HANDLE Handle;
	char DeviceName[1024];
};

struct KeyboardEventArgs {
	uint32_t vkCode;
	uint32_t scanCode;
	struct KeyboardStruct source;
};

struct DecisionRecord
{
	USHORT virtualKeyCode;
	BOOL decision;

	DecisionRecord(USHORT _virtualKeyCode, BOOL _decision) : virtualKeyCode(_virtualKeyCode), decision(_decision) {}
};


DWORD WINAPI Llk(HMODULE hModule);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyProc(int nCode, WPARAM wParam, LPARAM lParam);

void RefreshDevices();


#ifdef __cplusplus
extern "C" {
#endif

	EXPORT void LlkPress_Callback_Register(bool(*cb)(struct KeyboardEventArgs args));
	EXPORT bool LlkPress_GetKeyboardStruct(HANDLE handle, struct KeyboardStruct *kbS);

#ifdef __cplusplus
}
#endif
