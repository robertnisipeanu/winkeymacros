#include "LowLevelKeyboard.h"

HMODULE _hInstance;
void (*_Llk_KeyPressCallback)(PCUSTOM_KEYBOARD_INPUT input, DWORD size);
DWORD _maxSize;

DWORD WINAPI Llk(HMODULE hModule) {
	_hInstance = hModule;
}

void Llk_RegisterKeyPressCallback(DWORD maxSize, void(*cb)(PCUSTOM_KEYBOARD_INPUT input, DWORD size)) {
	_Llk_KeyPressCallback = cb;
	_maxSize = maxSize;
}

DWORD Llk_Internal_startConnectionHandle(DWORD dwThreadId, HANDLE* handle) {
	DWORD code;
	*handle = CreateFile(LR"(\\.KeyboardFilter)", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (*handle == INVALID_HANDLE_VALUE) {
		code = GetLastError();
		return code;
	}
}

DWORD Llk_Internal_openQueueIoctl(HANDLE *handle) {
	if (*handle == NULL || *handle == INVALID_HANDLE_VALUE) {
		return ERROR_HANDLE_NO_LONGER_VALID;
	}

	POVL_WRAPPER wrap = static_cast<POVL_WRAPPER>(malloc(sizeof(OVL_WRAPPER)));
	memset(wrap, 0, sizeof(OVL_WRAPPER));

	DeviceIoControl(*handle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE), NULL, 0, &wrap->ReturnedSequence, sizeof(CUSTOM_KEYBOARD_INPUT), NULL, &wrap->Overlapped);
}