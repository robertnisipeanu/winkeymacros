#include "Driver.h"

WDFDEVICE ControlDevice = NULL;

NTSTATUS UserCommunication_RegisterControlDevice(WDFDRIVER WdfDriver) {
	KdPrint(("[KB] Creating a device control object..."));

	NTSTATUS status = STATUS_SUCCESS;

	PWDFDEVICE_INIT controlDeviceInit = NULL;
	WDF_OBJECT_ATTRIBUTES controlDeviceAttributes;
	WDF_IO_QUEUE_CONFIG controlDeviceQueueConfig;
	PINVERTED_DEVICE_CONTEXT devContext;

	controlDeviceInit = WdfControlDeviceInitAllocate(WdfDriver, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
	if (controlDeviceInit == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		KdPrint(("[KB] WdfControlDeviceInitAllocate failed\n"));
		return status;
	}

	WdfDeviceInitSetIoType(controlDeviceInit, WdfDeviceIoBuffered);

	DECLARE_CONST_UNICODE_STRING(controlDeviceName, L"\\Device\\KeyboardFilter");
	DECLARE_CONST_UNICODE_STRING(userControlDeviceName, L"\\Global??\\KeyboardFilter");

	status = WdfDeviceInitAssignName(controlDeviceInit, &controlDeviceName);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(controlDeviceInit);
		KdPrint(("[KB] WdfDeviceInitAssignName failed with status 0x%x\n", status));
		return status;
	}

	WdfControlDeviceInitSetShutdownNotification(controlDeviceInit, UserCommunication_EvtWdfDeviceShutdownNotification, WdfDeviceShutdown);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&controlDeviceAttributes, INVERTED_DEVICE_CONTEXT);

	//
	// Create our WdfDevice
	//
	status = WdfDeviceCreate(&controlDeviceInit, &controlDeviceAttributes, &ControlDevice);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(controlDeviceInit);
		KdPrint(("[KB] WdfDeviceCreate failed with status 0x%x\n", status));
		controlDeviceInit = NULL;
		return status;
	}

	//
	// Get our device storage area and initialize the sequence number 
	//
	devContext = InvertedGetContextFromDevice(ControlDevice);

	//
	// Create a symlink for the device so the usermode app can access it
	//
	status = WdfDeviceCreateSymbolicLink(ControlDevice, &userControlDeviceName);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ControlDevice);
		KdPrint(("[KB] WdfDeviceCreateSymbolicLink failed with status 0x%x\n", status));
		return status;
	}

	//
	// Set callback for IOCTLs and disable device power management
	//
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&controlDeviceQueueConfig, WdfIoQueueDispatchParallel);
	controlDeviceQueueConfig.EvtIoDeviceControl = UserCommunication_EvtIoDeviceControl;
	controlDeviceQueueConfig.PowerManaged = WdfFalse;

	//
	// Create an I/O Queue
	//
	status = WdfIoQueueCreate(ControlDevice, &controlDeviceQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ControlDevice);
		KdPrint(("[KB] WdfIoQueueCreate failed with status 0x%x\n", status));
		return status;
	}

	//
	// Create an I/O Queue for notification purposes
	// and set it as manual dispatching to hold the Requests
	//
	WDF_IO_QUEUE_CONFIG_INIT(&controlDeviceQueueConfig, WdfIoQueueDispatchManual);
	status = WdfIoQueueCreate(ControlDevice, &controlDeviceQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &devContext->NotificationQueue);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ControlDevice);
		KdPrint(("[KB] WdfIoQueueCreate[notification] failed with status 0x%x\n", status));
		return status;
	}

	WdfControlFinishInitializing(ControlDevice);

	KdPrint(("[KB] Created a device control object\n"));

	return status;
}

VOID UserCommunication_EvtWdfDeviceShutdownNotification(WDFDEVICE Device)
{
	UNREFERENCED_PARAMETER(Device);
	KdPrint(("[KB] EvtWdfDeviceShutdownNotification"));
}

VOID UserCommunication_EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode) {

	PINVERTED_DEVICE_CONTEXT devContext;
	NTSTATUS status = STATUS_INVALID_PARAMETER;
	ULONG_PTR info = 0;

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	devContext = InvertedGetContextFromDevice(WdfIoQueueGetDevice(Queue));


	KdPrint(("[KB] ControlDevice_EvtIoDeviceControl with ICC 0x%u\n", IoControlCode));
	switch (IoControlCode) {
		case IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE: {

			//
			// We return an 32-bit value with each completion notification
			// Be sure the user's data buffer is at least long enough for that.
			// 
			if (OutputBufferLength < sizeof(LONG)) {
				//
				// Not enough space? Complete the request with
				// STATUS_INVALID_PARAMETER (as set previously)
				break;
			}

			//
			// If request forward to holding queue, break and complete it
			//
			status = WdfRequestForwardToIoQueue(Request, devContext->NotificationQueue);
			if (!NT_SUCCESS(status)) {
				break;
			}

			KdPrint(("[KB] Added IOCTL to waiting queue\n"));

			//
			// We exit the function so the queue doesn't get completed
			//
			return;

		}

	}

	WdfRequestCompleteWithInformation(Request, status, info);
}