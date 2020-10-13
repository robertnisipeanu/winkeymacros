#define MACROLIB_EXPORT

#include "MacroLibrary.h"
#include "shared_ioctls.h"

HANDLE driverHandle = INVALID_HANDLE_VALUE;

MACROLIB_STATUS macrolib_initialize() {
	driverHandle = CreateFile(LR"(\\.\KeyboardFilter)", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	return MACROLIB_STATUS_SUCCESS;
}

void macrolib_deinitialize() {
	if (driverHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(driverHandle);
	}

	driverHandle = INVALID_HANDLE_VALUE;
}

MACROLIB_STATUS macrolib_get_keyboardslen(size_t* outLength) {
	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_GETKEYBOARDSLENGTH), NULL, 0, outLength, sizeof(size_t), NULL, NULL);

	return MACROLIB_STATUS_SUCCESS;
}

MACROLIB_STATUS macrolib_get_keyboards(PCUSTOM_KEYBOARD_INFO outKeyboards, size_t* outKeyboardsLength, size_t keyboardsMaxLength) {
	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	DWORD outKeyboardsSize = 0;
	DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_GETKEYBOARDS), NULL, 0, outKeyboards, (DWORD) keyboardsMaxLength * sizeof(CUSTOM_KEYBOARD_INFO), &outKeyboardsSize, NULL);
	*outKeyboardsLength = outKeyboardsSize / sizeof(CUSTOM_KEYBOARD_INFO);

	return MACROLIB_STATUS_SUCCESS;
}


MACROLIB_STATUS macrolib_add_macro(INPUT_KEYBOARD_MACRO macroKey, PINPUT_KEYBOARD_KEY replacingKeys, size_t replacingKeysLength) {
	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	size_t bufferSize = sizeof(INPUT_KEYBOARD_MACRO) + (replacingKeysLength * sizeof(INPUT_KEYBOARD_KEY));
	unsigned char* bytePointer = reinterpret_cast<unsigned char*>(malloc(bufferSize));
	memcpy(bytePointer, &macroKey, sizeof(INPUT_KEYBOARD_MACRO));
	memcpy(bytePointer + sizeof(INPUT_KEYBOARD_MACRO), replacingKeys, replacingKeysLength * sizeof(INPUT_KEYBOARD_KEY));

	DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_ADDMACRO), bytePointer, (DWORD) bufferSize, NULL, 0, NULL, NULL);

	free(bytePointer);

	return MACROLIB_STATUS_SUCCESS;

}

MACROLIB_STATUS macrolib_is_macro(INPUT_KEYBOARD_MACRO macroKey, BOOLEAN* outResult) {
	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_ISMACRO), &macroKey, sizeof(INPUT_KEYBOARD_MACRO), outResult, sizeof(BOOLEAN), NULL, NULL);

	return MACROLIB_STATUS_SUCCESS;
}

MACROLIB_STATUS macrolib_delete_macro(INPUT_KEYBOARD_MACRO macroKey) {
	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_DELETEMACRO), &macroKey, sizeof(INPUT_KEYBOARD_MACRO), NULL, 0, NULL, NULL);

	return MACROLIB_STATUS_SUCCESS;
}

MACROLIB_STATUS macrolib_get_macro_length(size_t* outLength) {
	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_GETMACROLENGTH), NULL, 0, outLength, sizeof(size_t), NULL, NULL);

	return MACROLIB_STATUS_SUCCESS;
}

MACROLIB_STATUS macrolib_get_macro(PINPUT_KEYBOARD_KEY outKeys, size_t* outKeysLength, size_t keysMaxLength) {
	if (driverHandle == INVALID_HANDLE_VALUE) {
		return MACROLIB_STATUS_CONNECTION_FAIL;
	}

	DWORD outMacroSize = 0;
	DeviceIoControl(driverHandle, static_cast<DWORD>(IOCTL_KEYBOARDFILTER_GETMACRO), NULL, 0, outKeys, (DWORD) keysMaxLength * sizeof(INPUT_KEYBOARD_KEY), &outMacroSize, NULL);
	*outKeysLength = outMacroSize / sizeof(INPUT_KEYBOARD_KEY);

	return MACROLIB_STATUS_SUCCESS;
}