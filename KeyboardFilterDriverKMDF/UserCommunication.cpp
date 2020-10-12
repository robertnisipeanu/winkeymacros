#include "UserCommunication.h"

WDFDEVICE ControlDevice = NULL;

NTSTATUS UserCommunication_RegisterControlDevice(WDFDRIVER WdfDriver) {
	KdPrint(("[KB] Creating a device control object..."));

	NTSTATUS status = STATUS_SUCCESS;

	PWDFDEVICE_INIT controlDeviceInit = NULL;
	WDF_OBJECT_ATTRIBUTES controlDeviceAttributes;
	PINVERTED_DEVICE_CONTEXT devContext;

	// Used to create the virtual device
	controlDeviceInit = WdfControlDeviceInitAllocate(WdfDriver, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
	if (controlDeviceInit == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		KdPrint(("[KB] WdfControlDeviceInitAllocate failed\n"));
		return status;
	}

	WdfDeviceInitSetIoType(controlDeviceInit, WdfDeviceIoBuffered);

	// Set the name of our object and the global name(for symlink) that is gonna
	// be used by our app to communicate
	DECLARE_CONST_UNICODE_STRING(controlDeviceName, L"\\Device\\KeyboardFilter");
	DECLARE_CONST_UNICODE_STRING(userControlDeviceName, L"\\Global??\\KeyboardFilter");

	status = WdfDeviceInitAssignName(controlDeviceInit, &controlDeviceName);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(controlDeviceInit);
		KdPrint(("[KB] WdfDeviceInitAssignName failed with status 0x%x\n", status));
		return status;
	}

	// Not yet sure if needed (we don't do any cleanup on ControlDevice shutdown yet)
	WdfControlDeviceInitSetShutdownNotification(controlDeviceInit, UserCommunication_EvtWdfDeviceShutdownNotification, WdfDeviceShutdown);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&controlDeviceAttributes, INVERTED_DEVICE_CONTEXT);

	// Create our WdfDevice
	status = WdfDeviceCreate(&controlDeviceInit, &controlDeviceAttributes, &ControlDevice);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(controlDeviceInit);
		KdPrint(("[KB] WdfDeviceCreate failed with status 0x%x\n", status));
		controlDeviceInit = NULL;
		return status;
	}

	// Get our device storage area and initialize the sequence number 
	devContext = InvertedGetContextFromDevice(ControlDevice);

	// Create a symlink for the device so the usermode app can access it
	status = WdfDeviceCreateSymbolicLink(ControlDevice, &userControlDeviceName);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ControlDevice);
		KdPrint(("[KB] WdfDeviceCreateSymbolicLink failed with status 0x%x\n", status));
		return status;
	}

	// Set callback for IOCTLs and disable device power management
	WDF_IO_QUEUE_CONFIG controlDeviceDefaultQueueConfig;
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&controlDeviceDefaultQueueConfig, WdfIoQueueDispatchParallel);
	controlDeviceDefaultQueueConfig.EvtIoDeviceControl = UserCommunication_EvtIoDeviceControl;
	controlDeviceDefaultQueueConfig.PowerManaged = WdfFalse;

	// Create the default I/O Queue used for communication
	status = WdfIoQueueCreate(ControlDevice, &controlDeviceDefaultQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ControlDevice);
		KdPrint(("[KB] WdfIoQueueCreate failed with status 0x%x\n", status));
		return status;
	}

	// Create an I/O Queue for notification purposes
	// and set it as manual dispatching to hold the Requests
	WDF_IO_QUEUE_CONFIG controlDeviceNotificationQueueConfig;
	WDF_IO_QUEUE_CONFIG_INIT(&controlDeviceNotificationQueueConfig, WdfIoQueueDispatchManual);
	status = WdfIoQueueCreate(ControlDevice, &controlDeviceNotificationQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &devContext->NotificationQueue);
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

VOID UserCommunication_EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferSize, size_t InputBufferSize, ULONG IoControlCode) {

	PINVERTED_DEVICE_CONTEXT devContext;
	NTSTATUS status = STATUS_INVALID_PARAMETER;
	ULONG_PTR info = 0;

	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(InputBufferSize);

	devContext = InvertedGetContextFromDevice(WdfIoQueueGetDevice(Queue));


	// KdPrint(("[KB] ControlDevice_EvtIoDeviceControl with ICC 0x%u\n", IoControlCode));
	switch (IoControlCode) {

		case IOCTL_KEYBOARDFILTER_GETKEYBOARDSLENGTH: {

			// Make sure the output buffer can store our parameter
			if (OutputBufferSize < sizeof(size_t)) {
				KdPrint(("[KB] OutputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			size_t* bufferPointer;

			// Retrieve output buffer
			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(size_t), reinterpret_cast<PVOID*>(&bufferPointer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve output buffer\n"));
				break;
			}

			// Assign the number of devices to the output buffer
			*bufferPointer = KBFLTR_GetNumberOfKeyboards();

			// Specify the size of the output buffer
			info = sizeof(size_t);

			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_GETKEYBOARDS: {			
			// Output buffer should be able to store at least one CUSTOM_KEYBOARD_INFO
			if (OutputBufferSize < sizeof(CUSTOM_KEYBOARD_INFO)) {
				KdPrint(("[KB] OutputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve output buffer
			PCUSTOM_KEYBOARD_INFO bufferPointer;

			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(CUSTOM_KEYBOARD_INFO), reinterpret_cast<PVOID*>(&bufferPointer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve output buffer\n"));
				break;
			}
			size_t kbInfoLength = OutputBufferSize / sizeof(CUSTOM_KEYBOARD_INFO);

			// Retrieve keyboards info in the buffer and finish IOCTL
			size_t gotKbInfoLength;

			KBFLTR_GetKeyboardsInfo(bufferPointer, kbInfoLength, &gotKbInfoLength);

			info = gotKbInfoLength * sizeof(CUSTOM_KEYBOARD_INFO);

			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_ADDMACRO: {

			// The KEYBOARD_MACRO object is required as input for this IOCTL
			if (InputBufferSize < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			WDFMEMORY inputBufferMemory;

			INPUT_KEYBOARD_MACRO keyboard;

			// Retrieve the memory in which the input buffer is stored
			status = WdfRequestRetrieveInputMemory(Request, &inputBufferMemory);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Copy the INPUT_KEYBOARD_MACRO object from the input buffer memory
			status = WdfMemoryCopyToBuffer(inputBufferMemory, 0, &keyboard, sizeof(INPUT_KEYBOARD_MACRO));
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Calculate the number of INPUT_KEYBOARD_KEY objects that remains in the input buffer after removing the INPUT_KEYBOARD_MACRO obj
			size_t numOfKeys = (InputBufferSize - sizeof(INPUT_KEYBOARD_MACRO)) / sizeof(INPUT_KEYBOARD_KEY);

			// If we got 0 KEYBOARD_MACRO_KEYs, then delete the macro for the key
			if (numOfKeys == 0) {
				// TODO: Got 0 keys, so delete the macro
				status = STATUS_SUCCESS;
				break;
			}

			// Allocate memory for the INPUT_KEYBOARD_KEY array
			PINPUT_KEYBOARD_KEY keyboardKeys = reinterpret_cast<PINPUT_KEYBOARD_KEY>(ExAllocatePoolWithTag(NonPagedPool, numOfKeys * sizeof(PINPUT_KEYBOARD_KEY), KBFLTR_USR_TAG));
			if (keyboardKeys == NULL) {
				KdPrint(("[KB] Failed to allocate memory for keyboardKeys array\n"));
				break;
			}

			// Retrieve the KEYBOARD_MACRO_KEY array from the input buffer
			status = WdfMemoryCopyToBuffer(inputBufferMemory, sizeof(INPUT_KEYBOARD_MACRO), keyboardKeys, numOfKeys * sizeof(INPUT_KEYBOARD_KEY));
			if (!NT_SUCCESS(status)) {
				ExFreePoolWithTag(keyboardKeys, KBFLTR_USR_TAG);
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Get the device on which the macro should be added
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID(keyboard.DeviceID);

			// If no device was found, return an error code
			if (device == NULL) {
				ExFreePoolWithTag(keyboardKeys, KBFLTR_USR_TAG);
				status = STATUS_NOT_FOUND;
				KdPrint(("[KB] No keyboard found with DeviceID %u [ScanCode: %u]\n", keyboard.DeviceID, keyboard.ReplacedKeyScanCode));
				break;
			}

			// Get the MacroManager class instance associated with found device
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = (MacroManager*)devExt->MacroManager;

			// Add the macro
			BOOLEAN result = macroManager->addMacro(keyboard.ReplacedKeyScanCode, keyboardKeys, numOfKeys * sizeof(INPUT_KEYBOARD_KEY));
			if (!result) {
				status = STATUS_GENERIC_COMMAND_FAILED;
				KdPrint(("[KB] Failed to add macro\n"));
				break;
			}

			// Free memory for the INPUT_KEYBOARD_KEY array
			ExFreePoolWithTag(keyboardKeys, KBFLTR_USR_TAG);
			
			status = STATUS_SUCCESS;

			break;
		}
		case IOCTL_KEYBOARDFILTER_ISMACRO: {

			// INPUT_KEYBOARD_MACRO is required
			if (InputBufferSize < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve input buffer
			PINPUT_KEYBOARD_MACRO inputBuffer;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(INPUT_KEYBOARD_MACRO), reinterpret_cast<PVOID*>(&inputBuffer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Make sure the output buffer can store our parameter
			if (OutputBufferSize < sizeof(BOOLEAN)) {
				KdPrint(("[KB] OutputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			BOOLEAN* bufferPointer;

			// Retrieve output buffer
			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(BOOLEAN), reinterpret_cast<PVOID*>(&bufferPointer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve output buffer\n"));
				break;
			}

			// Get the device from the DeviceID
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID(inputBuffer->DeviceID);
			if (device == NULL) {
				KdPrint(("[KB] Device not found\n"));
				status = STATUS_NOT_FOUND;
				break;
			}

			// Get the device macro manager
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = reinterpret_cast<MacroManager*>(devExt->MacroManager);

			// Check if there is a macro on the specified key and return
			*bufferPointer = macroManager->isMacro(inputBuffer->ReplacedKeyScanCode);
			info = sizeof(BOOLEAN);

			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_DELETEMACRO: {

			// INPUT_KEYBOARD_MACRO is required
			if (InputBufferSize < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve input buffer
			PINPUT_KEYBOARD_MACRO inputBuffer;
			
			status = WdfRequestRetrieveInputBuffer(Request, sizeof(INPUT_KEYBOARD_MACRO), reinterpret_cast<PVOID*>(&inputBuffer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Get the device from the DeviceID
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID(inputBuffer->DeviceID);
			if (device == NULL) {
				KdPrint(("[KB] Device not found\n"));
				status = STATUS_NOT_FOUND;
				break;
			}

			// Get the device macro manager
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = reinterpret_cast<MacroManager*>(devExt->MacroManager);

			// Delete the macro for that key
			macroManager->deleteMacro(inputBuffer->ReplacedKeyScanCode);

			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_GETMACRO: {

			// INPUT_KEYBOARD_MACRO is required
			if (InputBufferSize < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve input buffer
			PINPUT_KEYBOARD_MACRO inputBuffer;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(INPUT_KEYBOARD_MACRO), reinterpret_cast<PVOID*>(&inputBuffer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Get the device from the DeviceID
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID(inputBuffer->DeviceID);
			if (device == NULL) {
				KdPrint(("[KB] Device not found\n"));
				status = STATUS_NOT_FOUND;
				break;
			}

			// Get the device macro manager
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = reinterpret_cast<MacroManager*>(devExt->MacroManager);

			// If there is no macro for that key, return STATUS_NO_MATCH
			if (!macroManager->isMacro(inputBuffer->ReplacedKeyScanCode)) {
				status = STATUS_NO_MATCH;
				break;
			}

			// Get the macro
			PKB_MACRO_HASHVALUE macro = macroManager->getMacro(inputBuffer->ReplacedKeyScanCode);

			size_t neededBufferSize = macro->NewKeysLength * sizeof(INPUT_KEYBOARD_KEY);

			// If output buffer is not big enough, return
			if (OutputBufferSize < neededBufferSize) {
				KdPrint(("[KB] OutputBufferSize is too small\n"));
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			// Retrieve output buffer
			PINPUT_KEYBOARD_KEY bufferPointer;
			status = WdfRequestRetrieveOutputBuffer(Request, neededBufferSize, reinterpret_cast<PVOID*>(&bufferPointer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve output buffer\n"));
				break;
			}

			// Copy replacing keys into output buffer
			RtlCopyMemory(bufferPointer, macro->NewKeys, neededBufferSize);
			info = neededBufferSize;

			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_GETMACROLENGTH: {

			// INPUT_KEYBOARD_MACRO is required
			if (InputBufferSize < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve input buffer
			PINPUT_KEYBOARD_MACRO inputBuffer;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(INPUT_KEYBOARD_MACRO), reinterpret_cast<PVOID*>(&inputBuffer), NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Make sure the output buffer can store our parameter
			if (OutputBufferSize < sizeof(size_t)) {
				KdPrint(("[KB] OutputBufferSize is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			size_t* bufferPointer;

			// Retrieve output buffer
			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(size_t), reinterpret_cast<PVOID*>(&bufferPointer), NULL);
			if (!NT_SUCCESS(status)) {
				status = STATUS_SUCCESS;
				break;
			}

			// Get the device from the DeviceID
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID(inputBuffer->DeviceID);
			if (device == NULL) {
				KdPrint(("[KB] Device not found\n"));
				status = STATUS_NOT_FOUND;
				break;
			}

			// Get the device macro manager
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = reinterpret_cast<MacroManager*>(devExt->MacroManager);

			// If there is no macro for that key, return 0
			// otherwise return how many INPUT_KEYBOARD_KEY objects are stored for that macro
			if (!macroManager->isMacro(inputBuffer->ReplacedKeyScanCode)) {
				KdPrint(("[KB] Macro not found"));
				status = STATUS_NO_MATCH;
				break;
			}

			PKB_MACRO_HASHVALUE macro = macroManager->getMacro(inputBuffer->ReplacedKeyScanCode);
			*bufferPointer = macro->NewKeysLength;
			
			info = sizeof(size_t);
			status = STATUS_SUCCESS;

			break;
		}
		case IOCTL_KEYBOARDFILTER_IDENTIFYKEY: {

			//
			// We return a KEYPRESS_IDENTIFY object, so make sure the output buffer is big enough to store it
			//
			if (OutputBufferSize < sizeof(KEYPRESS_IDENTIFY)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			// If there are already KBLFTR_MAX_QUEUE_LENGTH Requests in the Queue, deny this one
			ULONG queueRequests, driverRequests;
			WdfIoQueueGetState(devContext->NotificationQueue, &queueRequests, &driverRequests);

			// TODO
			KdPrint(("queueRequests: %u, driverRequests: %u", queueRequests, driverRequests));

			// Forward request to our queue
			status = WdfRequestForwardToIoQueue(Request, devContext->NotificationQueue);
			if (!NT_SUCCESS(status)) {
				break;
			}

			// Exit the function so the queue doesn't get completed
			return;
		}

	}

	WdfRequestCompleteWithInformation(Request, status, info);
}