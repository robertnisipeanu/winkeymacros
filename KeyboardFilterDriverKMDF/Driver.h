#pragma once

#include <ntddk.h>
#include <ntverp.h>
#pragma warning(disable:4201)

#include "ntddk.h"
#include "kbdmou.h"
#include <ntddkbd.h>

#pragma warning(default:4201)

#include <wdf.h>

#define NTRSTRSAFE_LIB
#include <ntstrsafe.h>

#include <initguid.h>
#include <devguid.h>
#include "shared.h"

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

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _INVERTED_DEVICE_CONTEXT {
	WDFQUEUE NotificationQueue;
} INVERTED_DEVICE_CONTEXT, *PINVERTED_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, FilterGetData)
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(INVERTED_DEVICE_CONTEXT, InvertedGetContextFromDevice)


DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD KeyboardFilter_EvtDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KeyboardFilter_EvtIoInternalDeviceControl;
EVT_WDF_REQUEST_COMPLETION_ROUTINE KeyboardFilterRequestCompletionRoutine;

VOID KeyboardFilter_ServiceCallback(IN PDEVICE_OBJECT DeviceObject, IN PKEYBOARD_INPUT_DATA InputDataStart,
	IN PKEYBOARD_INPUT_DATA InputDataEnd, IN OUT PULONG InputDataConsumed);

NTSTATUS UserCommunication_RegisterControlDevice(WDFDRIVER WdfDriver);

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL UserCommunication_EvtIoDeviceControl;
EVT_WDF_DEVICE_SHUTDOWN_NOTIFICATION UserCommunication_EvtWdfDeviceShutdownNotification;

WDFDEVICE KBFLTR_GetDeviceByCustomID(WDFDRIVER WdfDriver, DWORD DeviceID);