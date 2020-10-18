/**
* File name: Driver.h
* Project name: Multi-Keyboard macros
* Author: Robert Gabriel Nisipeanu
* Author's Github Profile: https://github.com/robertnisipeanu
*/

#pragma once

#include <ntddk.h>
#include <ntverp.h>
#pragma warning(disable:4201)

#include "ntddk.h"
#include "kbdmou.h"
#include <ntddkbd.h>

#pragma warning(default:4201)

#include <wdf.h>

// USB libs
#include <usbdi.h>
#include <usbdlib.h>
#include <wdfusb.h>

#define NTRSTRSAFE_LIB
#include <ntstrsafe.h>

#include <initguid.h>
#include <devguid.h>
#include "UserCommunication.h"
#include "shared_ioctls.h"
#include "shared_structs.h"
#include "uthash.h"
#include "operators.h"
#include "MacroManager.h"

#define KBFLTR_KP_TAG 'Keyp'

extern DWORD nextUserDeviceID;
extern WDFDEVICE ControlDevice;

typedef struct _DEVICE_EXTENSION
{
	WDFDEVICE WdfDevice;

	//
	// Device identifier
	//
	DWORD DeviceID;

	//
	// The real connect data that this driver reports to
	//
	CONNECT_DATA UpperConnectData;

	//
	// Cached Keyboard Attributes
	//
	KEYBOARD_ATTRIBUTES KeyboardAttributes;

	//
	// Pointer to the device specific macro manager
	//
	PVOID MacroManager; // MacroManager* MacroManager

	//
	// Handle to WDFUSBDEVICE if it's an USB Device, otherwise NULL
	//
	WDFUSBDEVICE UsbDevice;

	UT_hash_handle hh;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _INVERTED_DEVICE_CONTEXT {

	WDFQUEUE IdentifyKeyQueue; // Used to identify key presses (without being affected by macros) by a user-mode app

	WDFQUEUE KeyboardEventQueue; // Used to send keyboard events (like keyboard connect/disconnect) to a user-mode app - NOT YET IMPLEMENTED

} INVERTED_DEVICE_CONTEXT, *PINVERTED_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, GetContextFromDevice)
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(INVERTED_DEVICE_CONTEXT, InvertedGetContextFromDevice)

VOID KeyboardFilter_ServiceCallback(IN PDEVICE_OBJECT DeviceObject, IN PKEYBOARD_INPUT_DATA InputDataStart,
	IN PKEYBOARD_INPUT_DATA InputDataEnd, IN OUT PULONG InputDataConsumed);

BOOLEAN KBFLTR_ProcessKeyPressIdentifyMode(WDFDEVICE controlDevice, DWORD DeviceID, PKEYBOARD_INPUT_DATA kbInput);

WDFDEVICE KBFLTR_GetDeviceByCustomID(DWORD DeviceID);
size_t KBFLTR_GetNumberOfKeyboards();
void KBFLTR_GetKeyboardsInfo(PCUSTOM_KEYBOARD_INFO buffer, size_t maxKeyboards, size_t* keyboardsNumberOutput);

extern "C" {
	DRIVER_INITIALIZE DriverEntry;
	EVT_WDF_DRIVER_DEVICE_ADD KeyboardFilter_EvtDeviceAdd;
	EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KeyboardFilter_EvtIoInternalDeviceControl;
	EVT_WDF_REQUEST_COMPLETION_ROUTINE KeyboardFilterRequestCompletionRoutine;
	EVT_WDF_OBJECT_CONTEXT_CLEANUP KeyboardFilter_OnKeyboardDisconnect;
}