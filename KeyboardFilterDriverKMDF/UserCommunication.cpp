#include "UserCommunication.h"

WDFDEVICE ControlDevice = NULL;

NTSTATUS UserCommunication_RegisterControlDevice(WDFDRIVER WdfDriver) {
	KdPrint(("[KB] Creating a device control object..."));

	NTSTATUS status = STATUS_SUCCESS;

	PWDFDEVICE_INIT controlDeviceInit = NULL;
	WDF_OBJECT_ATTRIBUTES controlDeviceAttributes;
	WDF_IO_QUEUE_CONFIG controlDeviceQueueConfig;
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
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&controlDeviceQueueConfig, WdfIoQueueDispatchParallel);
	controlDeviceQueueConfig.EvtIoDeviceControl = UserCommunication_EvtIoDeviceControl;
	controlDeviceQueueConfig.PowerManaged = WdfFalse;

	// Create an I/O Queue
	status = WdfIoQueueCreate(ControlDevice, &controlDeviceQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ControlDevice);
		KdPrint(("[KB] WdfIoQueueCreate failed with status 0x%x\n", status));
		return status;
	}

	// Create an I/O Queue for notification purposes
	// and set it as manual dispatching to hold the Requests
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


	// KdPrint(("[KB] ControlDevice_EvtIoDeviceControl with ICC 0x%u\n", IoControlCode));
	switch (IoControlCode) {

// How old version worked:
// - when a key press happens, cancel the key press and send the key press to an user mode app
// - the user mode app would send back what keys to be pressed (the same key it got or a modified version if it's a macro)
// How new version works:
// - the app tells the driver that it should replace X key with [Y, Z, ...] keys and the driver stores that information into a hashtable
// - when a keypress happens, the driver checks the hashtable if there is any macro for that key
// Because the old version had to communicate with the app for every key press, there could be a delay between the physical key press and the driver gets the keypress back from the usermode app.
// If the usermode app would freeze or there is too much cpu load and the app has lower priority than the process that is using the CPU, then some keypresses can even take seconds to be processed.
// Another reason why the behaviour needed to be changed it's because this is a kernel driver, it gets EVERY key press, even if that happens inside a secure desktop (secure desktop = UAC prompt/winlogon page)
// and a bad app could use that to get the user password
// TLDR: OLD_VERSION was slower and less secure
#ifdef KEYBOARDFILTER_IS_OLD_VERSION
		case IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE: {

			//
			// We return an 32-bit value with each completion notification
			// Be sure the user's data buffer is at least long enough for that.
			// 
			if (OutputBufferLength < sizeof(CUSTOM_KEYBOARD_INPUT)) {
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

			// KdPrint(("[KB] Added IOCTL to waiting queue\n"));

			//
			// We exit the function so the queue doesn't get completed
			//
			return;

		}
		//
		// Called by the app to make the driver send keys
		//
		case IOCTL_KEYBOARDFILTER_SENDKEYPRESS: {

			//
			// If the input buffer length is less than a possible key press
			// exit
			//
			if (InputBufferLength < sizeof(CUSTOM_KEYBOARD_INPUT)) {
				KdPrint(("[KB] InputBufferLength is too small\n"));
				break;
			}

			// Get number of keys pressed
			size_t numOfKeys = InputBufferLength / sizeof(CUSTOM_KEYBOARD_INPUT);
			
			PCUSTOM_KEYBOARD_INPUT keyInput;

			//
			// Retrieve input buffer
			//
			NTSTATUS memoryAllocStatus = WdfRequestRetrieveInputBuffer(Request, sizeof(CUSTOM_KEYBOARD_INPUT), (PVOID *) &keyInput, NULL);
			if (!NT_SUCCESS(memoryAllocStatus)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			//
			// Get the device (keyboard) that should execute
			// the key pressses
			// If DeviceID is 0 then get the first available keyboard
			//
			WDFDEVICE deviceToPressOn = KBFLTR_GetDeviceByCustomID(WdfDeviceGetDriver(WdfObjectContextGetObject(devContext)), keyInput[0].DeviceID);

			if (deviceToPressOn) {

				//
				// The device on which the key should execute was found,
				// so proceed with processing the key press event
				// like it would come from the keyboard itself
				//
				ULONG consumed = 0;
				PDEVICE_EXTENSION deviceToPressOnExt = GetContextFromDevice(deviceToPressOn);
				PKEYBOARD_INPUT_DATA nativeKeys;
				WDFMEMORY nativeKeysMemory;

				memoryAllocStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPool, 0, numOfKeys * sizeof(KEYBOARD_INPUT_DATA), &nativeKeysMemory, &nativeKeys);
				if (!NT_SUCCESS(memoryAllocStatus)) {
					KdPrint(("[KB] Failed to allocate memory for native inputs\n"));
					break;
				}

				for (int i = 0; i < numOfKeys; i++) {
					nativeKeys[i].UnitId = keyInput[i].UnitId;
					nativeKeys[i].Reserved = keyInput[i].Reserved;
					nativeKeys[i].ExtraInformation = keyInput[i].ExtraInformation;
					nativeKeys[i].Flags = keyInput[i].Flags;
					nativeKeys[i].MakeCode = keyInput[i].MakeCode;
				}

				(*(PSERVICE_CALLBACK_ROUTINE)deviceToPressOnExt->UpperConnectData.ClassService)(deviceToPressOnExt->UpperConnectData.ClassDeviceObject, nativeKeys, &nativeKeys[numOfKeys], &consumed);

				WdfObjectDelete(nativeKeysMemory);
			}

			status = STATUS_SUCCESS;
			info = 0;

			break;
		}
#endif
		case IOCTL_KEYBOARDFILTER_GETKEYBOARDSLENGTH: {

			// Make sure the output buffer can store our parameter
			if (OutputBufferLength < sizeof(size_t)) {
				KdPrint(("[KB] OutputBufferLength is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			size_t* bufferPointer;

			// Retrieve output buffer
			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(size_t), (PVOID*)&bufferPointer, NULL);
			if (!NT_SUCCESS(status)) {
				status = STATUS_NOT_COMMITTED;
				info = 0;
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
			// TODO
			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_ADDMACRO: {

			// The KEYBOARD_MACRO object is required as input for this IOCTL
			if (InputBufferLength < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferLength is too small\n"));
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
			size_t numOfKeys = (InputBufferLength - sizeof(INPUT_KEYBOARD_MACRO)) / sizeof(INPUT_KEYBOARD_KEY);

			// If we got 0 KEYBOARD_MACRO_KEYs, then delete the macro for the key
			if (numOfKeys == 0) {
				// TODO: Got 0 keys, so delete the macro
				status = STATUS_SUCCESS;
				break;
			}

			// Allocate memory for the INPUT_KEYBOARD_KEY array
			PINPUT_KEYBOARD_KEY keyboardKeys = (PINPUT_KEYBOARD_KEY) ExAllocatePoolWithTag(NonPagedPool, numOfKeys * sizeof(PINPUT_KEYBOARD_KEY), KBFLTR_USR_TAG);
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
			if (InputBufferLength < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferLength is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve input buffer
			PINPUT_KEYBOARD_MACRO *inputBuffer;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(INPUT_KEYBOARD_MACRO), (PVOID*) &inputBuffer, NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Make sure the output buffer can store our parameter
			if (OutputBufferLength < sizeof(BOOLEAN)) {
				KdPrint(("[KB] OutputBufferLength is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			BOOLEAN* bufferPointer;

			// Retrieve output buffer
			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(BOOLEAN), (PVOID*)&bufferPointer, NULL);
			if (!NT_SUCCESS(status)) {
				status = STATUS_SUCCESS;
				break;
			}

			// Get the device from the DeviceID
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID((*inputBuffer)->DeviceID);
			if (device == NULL) {
				KdPrint(("[KB] Device not found\n"));
				status = STATUS_NOT_FOUND;
				break;
			}

			// Get the device macro manager
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = (MacroManager*) devExt->MacroManager;

			// Check if there is a macro on the specified key and return
			*bufferPointer = macroManager->isMacro((*inputBuffer)->ReplacedKeyScanCode);
			info = sizeof(BOOLEAN);

			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_DELETEMACRO: {

			// INPUT_KEYBOARD_MACRO is required
			if (InputBufferLength < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferLength is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve input buffer
			PINPUT_KEYBOARD_MACRO* inputBuffer;
			
			status = WdfRequestRetrieveInputBuffer(Request, sizeof(INPUT_KEYBOARD_MACRO), (PVOID*)&inputBuffer, NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Get the device from the DeviceID
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID((*inputBuffer)->DeviceID);
			if (device == NULL) {
				KdPrint(("[KB] Device not found\n"));
				status = STATUS_NOT_FOUND;
				break;
			}

			// Get the device macro manager
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = (MacroManager*)devExt->MacroManager;

			// Delete the macro for that key
			macroManager->deleteMacro((*inputBuffer)->ReplacedKeyScanCode);

			status = STATUS_SUCCESS;
			break;
		}
		case IOCTL_KEYBOARDFILTER_GETMACRO: {
			status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		case IOCTL_KEYBOARDFILTER_GETMACROLENGTH: {

			// INPUT_KEYBOARD_MACRO is required
			if (InputBufferLength < sizeof(INPUT_KEYBOARD_MACRO)) {
				KdPrint(("[KB] InputBufferLength is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			// Retrieve input buffer
			PINPUT_KEYBOARD_MACRO* inputBuffer;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(INPUT_KEYBOARD_MACRO), (PVOID*)&inputBuffer, NULL);
			if (!NT_SUCCESS(status)) {
				KdPrint(("[KB] Failed to retrieve input buffer\n"));
				break;
			}

			// Make sure the output buffer can store our parameter
			if (OutputBufferLength < sizeof(size_t)) {
				KdPrint(("[KB] OutputBufferLength is too small\n"));
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			size_t* bufferPointer;

			// Retrieve output buffer
			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(size_t), (PVOID*)&bufferPointer, NULL);
			if (!NT_SUCCESS(status)) {
				status = STATUS_SUCCESS;
				break;
			}

			// Get the device from the DeviceID
			WDFDEVICE device = KBFLTR_GetDeviceByCustomID((*inputBuffer)->DeviceID);
			if (device == NULL) {
				KdPrint(("[KB] Device not found\n"));
				status = STATUS_NOT_FOUND;
				break;
			}

			// Get the device macro manager
			PDEVICE_EXTENSION devExt = GetContextFromDevice(device);
			MacroManager* macroManager = (MacroManager*)devExt->MacroManager;

			// If there is no macro for that key, return 0
			// otherwise return how many INPUT_KEYBOARD_KEY objects are stored for that macro
			if (!macroManager->isMacro((*inputBuffer)->ReplacedKeyScanCode)) {
				*bufferPointer = 0;
			}
			else {
				PKB_MACRO_HASHVALUE macro = macroManager->getMacro((*inputBuffer)->ReplacedKeyScanCode);
				*bufferPointer = macro->NewKeysLength;
			}
			
			info = sizeof(size_t);
			status = STATUS_SUCCESS;

			break;
		}

	}

	WdfRequestCompleteWithInformation(Request, status, info);
}