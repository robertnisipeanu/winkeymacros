#include "Driver.h"

DWORD nextUserDeviceID = 1;
PDEVICE_EXTENSION _keyboards = NULL;

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;
	WDFDRIVER WdfDriver;

	KdPrint(("[KB] Loading driver...\n"));

	// Initialize driver config to control the attributes that
	// are global to the driver. Note that framework by default
	// provides a driver unload routine. If you create any resources
	// in the DriverEntry and want to be cleaned in driver unload,
	// you can override that by manually setting the EvtDriverUnload in the
	// config structure. In general xxx_CONFIG_INIT macros are provided to
	// initialize most commonly used members

	WDF_DRIVER_CONFIG_INIT(&config, KeyboardFilter_EvtDeviceAdd);

	// Create a framework driver object to represent our driver
	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &WdfDriver);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] WdfDriverCreate failed with status 0x%x\n", status));
		return status;
	}

	KdPrint(("[KB] DriverObject created\n"));

	// Create a virtual device that will be used for IOCTL communication
	// with the app
	status = UserCommunication_RegisterControlDevice(WdfDriver);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] RegisterControlDevice failed with status 0x%x\n", status));
		return status;
	}

	return status;
}

/*++
Routine Description:

	EvtDeviceAdd is called by the framework in response to AddDevice
	call from the PnP manager. Here you can query the device properties
	using WdfFdoInitWdmGetPhysicalDevice/IoGetDeviceProperty and based
	on that, decide to create a filter device object and attach to the
	function stack.

	If you are not interested in filtering this particular instance of the
	device, you can just return STATUS_SUCCESS without creating a framework
	device.

Arguments:

	Driver - Handle to a framework driver object created in DriverEntry

	DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

	NTSTATUS

--*/
NTSTATUS KeyboardFilter_EvtDeviceAdd(IN WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit) {
	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	NTSTATUS status;

	KdPrint(("[KB] FilterEvtDeviceAdd\n"));

	// Tell the framework that you are filter driver. Framework
	// takes care of inherting all the device flags & characteristics
	// from the lower device you are attaching to
	WdfFdoInitSetFilter(DeviceInit);

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_KEYBOARD);

	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);
	deviceAttributes.EvtCleanupCallback = KeyboardFilter_OnKeyboardDisconnect;

	// Create a framework device object. This call will in turn create
	// a WDM deviceobject, attach to the lower stack and set the
	// appropriate flags and attributes.
	WDFDEVICE hDevice;
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &hDevice);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] WdfDeviceCreate failed with status code 0x%x\n", status));
		return status;
	}

	PDEVICE_EXTENSION filterExt;
	filterExt = GetContextFromDevice(hDevice);
	filterExt->WdfDevice = hDevice;

	// Configure the default queue to be Parallel. Do not use sequential queue
	// if this driver is going to be filtering PS2 ports because it can lead to
	// deadlock. The PS2 port driver sends a request to the top of the stack when it
	// receives an ioctl request and waits for it to be completed. If you use a
	// sequential queue, this request will be stuck in the queue because of the
	// outstanding ioctl request sent earlier to the port driver.
	WDF_IO_QUEUE_CONFIG ioQueueConfig;
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);

	// Framework by default creates a non-power managed queues for
	// filter drivers
	ioQueueConfig.EvtIoInternalDeviceControl = KeyboardFilter_EvtIoInternalDeviceControl;
	

	status = WdfIoQueueCreate(hDevice, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] WdfIoQueueCreate failed with code 0x%x\n", status));
		return status;
	}

	// Initialize the MacroManager class for this device
	filterExt->MacroManager = new MacroManager();

	return status;
}

VOID KeyboardFilter_EvtIoInternalDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t OutputBufferLength, IN size_t InputBufferLength, IN ULONG IoControlCode) {
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);
	PAGED_CODE();

	WDFDEVICE hDevice;
	PDEVICE_EXTENSION devExt;
	NTSTATUS status = STATUS_SUCCESS;
	PCONNECT_DATA connectData = NULL;
	size_t length;

	BOOLEAN forwardWithCompletionRoutine = FALSE;
	WDFCONTEXT completionContext = WDF_NO_CONTEXT;
	BOOLEAN ret = TRUE;
	WDFMEMORY outputMemory;
	WDF_REQUEST_SEND_OPTIONS options;

	hDevice = WdfIoQueueGetDevice(Queue);
	devExt = GetContextFromDevice(hDevice);

	switch (IoControlCode) {

	case IOCTL_INTERNAL_KEYBOARD_CONNECT:
	{
		// Only allow one connection
		if (devExt->UpperConnectData.ClassService != NULL) {
			status = STATUS_SHARING_VIOLATION;
			break;
		}

		// Get the input buffer from the request
		// (Parameters.DeviceIoControl.Type3InputBuffer)
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(CONNECT_DATA), (PVOID*)&connectData, &length);
		if (!NT_SUCCESS(status)) {
			KdPrint(("[KB] WdfRequestRetrieveInputBuffer failed with error code 0x%x\n", status));
			break;
		}

		NT_ASSERT(length == InputBufferLength);

		devExt->UpperConnectData = *connectData;

		// Asign a DeviceID
		devExt->DeviceID = nextUserDeviceID++;

		// Hook into the report chain. Everytime a keyboard packet is reported
		// to the system, KeyboardFilter_ServiceCallback will be called

		connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);

#pragma warning(disable:4152) // nonstandard extension, function/data pointer conversion

		connectData->ClassService = KeyboardFilter_ServiceCallback;

#pragma warning(default:4152)

		// Add device extension to our global keyboard manager
		HASH_ADD(hh, _keyboards, DeviceID, sizeof(DWORD), devExt);

		break;
	}
	case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:
	{
		KdPrint(("[KB] IOCTL_INTERNAL_KEYBOARD_DISCONNECT called\n"));
		status = STATUS_NOT_IMPLEMENTED;
		break;
	}
	case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
	{
		forwardWithCompletionRoutine = TRUE;
		completionContext = devExt;
		break;
	}
	// Might want to capture these in the future. For now, then pass them down
	// the stack. These queries must be successful for the RIT to communicate
	// with the keyboard.
	case IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION:
	case IOCTL_KEYBOARD_QUERY_INDICATORS:
	case IOCTL_KEYBOARD_SET_INDICATORS:
	case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
	case IOCTL_KEYBOARD_SET_TYPEMATIC:
		break;
	}

	if (!NT_SUCCESS(status)) {
		WdfRequestComplete(Request, status);
		return;
	}

	// Forward the request down. WdfDeviceGetIoTarget returns
	// the default target, which represents the device attached
	// to us below in the stack

	if (forwardWithCompletionRoutine) {
		// Format the request with the output memory so the completion routine
		// can access the return data in order to cache it into the context area
		status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);

		if (!NT_SUCCESS(status)) {
			KdPrint(("[KB] WdfRequestRetrieveOutputMemory failed with error code: 0x%x\n", status));
			WdfRequestComplete(Request, status);
			return;
		}

		status = WdfIoTargetFormatRequestForInternalIoctl(WdfDeviceGetIoTarget(hDevice), Request, IoControlCode, NULL, NULL, outputMemory, NULL);

		if (!NT_SUCCESS(status)) {
			KdPrint(("[KB] WdfIoTargetFormatRequestForInternalIoctl failed: 0x%x\n", status));
			WdfRequestComplete(Request, status);
			return;
		}

		// Set our completion routine with a context area
		// that we will save the output data into
		WdfRequestSetCompletionRoutine(Request, KeyboardFilterRequestCompletionRoutine, completionContext);

		ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), WDF_NO_SEND_OPTIONS);

		if (ret == FALSE) {
			status = WdfRequestGetStatus(Request);
			KdPrint(("[KB] WdfRequestSend failed: 0x%x\n", status));
			WdfRequestComplete(Request, status);
		}
	}
	else {
		// We are not interested in post processing the IRP so
		// fire and forget.
		WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

		ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), &options);

		if (ret == FALSE) {
			status = WdfRequestGetStatus(Request);
			KdPrint(("[KB] WdfRequestSend failed: 0x%x\n", status));
			WdfRequestComplete(Request, status);
		}
	}

	return;
}


VOID KeyboardFilter_OnKeyboardDisconnect (_In_ WDFOBJECT Object) {

	WDFDEVICE device = (WDFDEVICE)Object;
	PDEVICE_EXTENSION devExt = GetContextFromDevice(device);

	// Clear the connection parameters in the device extension.
	devExt->UpperConnectData.ClassDeviceObject = NULL;
	devExt->UpperConnectData.ClassService = NULL;

	// Remove device extension from keyboards hash table
	PDEVICE_EXTENSION devExtHashSearch;
	DWORD devExtHashID = devExt->DeviceID;
#pragma warning(disable:4127)
	HASH_FIND(hh, _keyboards, &devExtHashID, sizeof(DWORD), devExtHashSearch);
#pragma warning(default:4127)
	if (devExtHashSearch != NULL) {
		HASH_DEL(_keyboards, devExtHashSearch);
		KdPrint(("[KB] Deleted device extension from keyboards hash table\n"));
	}

	// Delete the MacroManager
	delete ((MacroManager*) (devExt->MacroManager));

}




/*++

Routine Description:

	Called when there are keyboard packets to report to the Win32 subsystem.
	You can do anything you like to the packets.  For instance:

	o Drop a packet altogether
	o Mutate the contents of a packet
	o Insert packets into the stream

Arguments:

	DeviceObject - Context passed during the connect IOCTL

	InputDataStart - First packet to be reported

	InputDataEnd - One past the last packet to be reported.  Total number of
				   packets is equal to InputDataEnd - InputDataStart

	InputDataConsumed - Set to the total number of packets consumed by the RIT
						(via the function pointer we replaced in the connect
						IOCTL)

Return Value:

	Status is returned.

--*/
VOID KeyboardFilter_ServiceCallback(IN PDEVICE_OBJECT DeviceObject, IN PKEYBOARD_INPUT_DATA InputDataStart, IN PKEYBOARD_INPUT_DATA InputDataEnd, IN OUT PULONG InputDataConsumed) {

	WDFDEVICE WdfDevice;
	WdfDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);

	PDEVICE_EXTENSION devExt;
	devExt = GetContextFromDevice(WdfDevice);
	MacroManager* macroManager = (MacroManager*)devExt->MacroManager;

#ifdef KEYBOARDFILTER_IS_OLD_VERSION

	PINVERTED_DEVICE_CONTEXT InvertedDeviceContext;
	NTSTATUS QueueReturnStatus;
	ULONG_PTR info;
	WDFREQUEST notifyRequest;
	PVOID  bufferPointer;

	// ControlDevice not yet initialized, process keyboard input normally
	if (ControlDevice == NULL) {
		KdPrint(("[KB] No ControlDevice found, passing packets normally\n"));
		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);
		return;
	}
	InvertedDeviceContext = InvertedGetContextFromDevice(ControlDevice);

	NTSTATUS queueAvailable = WdfIoQueueRetrieveNextRequest(InvertedDeviceContext->NotificationQueue, &notifyRequest);

	// No queue available, process keyboard input normally
	if (!NT_SUCCESS(queueAvailable)) {
		KdPrint(("[KB] No queue available, passing packets normally"));
		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);
		return;
	}

	// Get a pointer to the output buffer that was passed-in with the user
	// notification IOCTL. We'll use this to return additional info about
	// the event.
	size_t bufferSize;
	queueAvailable = WdfRequestRetrieveOutputBuffer(notifyRequest, sizeof(CUSTOM_KEYBOARD_INPUT), (PVOID*)&bufferPointer, &bufferSize);
	if (!NT_SUCCESS(queueAvailable)) {
		// Should never get here
		QueueReturnStatus = STATUS_SUCCESS;
		info = 0;

		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);

		WdfRequestCompleteWithInformation(notifyRequest, QueueReturnStatus, info);
		KdPrint(("[KB] WdfRequestRetrieveOutputBuffer fail with status 0x%x\n", queueAvailable));
		return;
	}

	KEYBOARD_INPUT_DATA * pCur;
	PCUSTOM_KEYBOARD_INPUT dataToReturn;
	WDFMEMORY dataToReturnMemory;

	// Create local buffer to init the data that will be sent
	// to the app running in user mode.
	// Process keys normally if it fails
	NTSTATUS memoryAllocStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPool, 0, bufferSize, &dataToReturnMemory, &dataToReturn);
	if (!NT_SUCCESS(memoryAllocStatus)) {
		KdPrint(("[KB] Fail to allocate memory for local buffer"));
		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);
		return;
	}

	// Loop through key presses to construct an array of CUSTOM_KEYBOARD_INPUT
	// which will be then sent to the app through IOCTL
	int i;
	for (pCur = InputDataStart, i = 0; pCur < InputDataEnd; pCur++, i++) {

		// If there is no space left in the buffer, then process the rest of the input normally
		// TODO: Loop through IOCTL queue until all data is sent
		if (i >= bufferSize / sizeof(CUSTOM_KEYBOARD_INPUT)) {
			KdPrint(("[KB] Buffer size overrun\n"));
			ULONG consumed = 0;
			(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, pCur, InputDataEnd, &consumed);
			break;
		}

		dataToReturn[i].UnitId = pCur->UnitId;
		dataToReturn[i].DeviceID = devExt->DeviceID; // Unique identifier for the keyboard
		dataToReturn[i].Reserved = pCur->Reserved;
		dataToReturn[i].ExtraInformation = pCur->ExtraInformation;
		dataToReturn[i].Flags = pCur->Flags;
		dataToReturn[i].MakeCode = pCur->MakeCode;
	}

	// Copy local buffer into output buffer
	RtlCopyMemory(bufferPointer, dataToReturn, bufferSize);

	// Delete local buffer
	WdfObjectDelete(dataToReturnMemory);

	// Return data
	QueueReturnStatus = STATUS_SUCCESS;
	info = i * sizeof(CUSTOM_KEYBOARD_INPUT);
	WdfRequestCompleteWithInformation(notifyRequest, QueueReturnStatus, info);

	*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);

	// Notify END
#else

	PKEYBOARD_INPUT_DATA pCur;

	for (pCur = InputDataStart; pCur < InputDataEnd; pCur++) {
		PKB_MACRO_HASHVALUE macro = macroManager->getMacro(pCur->MakeCode);
		ULONG consumed = 0;

		// If there is no macro for this key, process the key normally
		if (macro == NULL) {
			(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, pCur, pCur + 1, &consumed);
			continue;
		}

		// If key is not KEY DOWN but there is a macro on this key, skip execution at all
		if (pCur->Flags != KEY_MAKE) {
			continue;
		}

		// Allocate memory for replacing keys
		PKEYBOARD_INPUT_DATA MacroData = (PKEYBOARD_INPUT_DATA) ExAllocatePoolWithTag(NonPagedPool, macro->NewKeysLength * sizeof(KEYBOARD_INPUT_DATA), KBFLTR_KP_TAG);

		// If memory allocation fails, execute the key press normally
		if (MacroData == NULL) {
			KdPrint(("[KB] Failed to allocate memory for macro keys\n"));
			(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, pCur, pCur + 1, &consumed);
			continue;
		}

		// Create KEYBOARD_INPUT_DATA objects that we can pass down the stack
		for (int i = 0; i < macro->NewKeysLength; i++) {
			MacroData[i].UnitId = pCur->UnitId;
			MacroData[i].Reserved = pCur->Reserved;
			MacroData[i].ExtraInformation = pCur->ExtraInformation;
			MacroData[i].MakeCode = macro->NewKeys[i].MakeCode;
			MacroData[i].Flags = macro->NewKeys[i].Flags;
		}

		// Pass the replacing keys down the stack
		(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, MacroData, MacroData + macro->NewKeysLength, &consumed);

		// Free the memory allocated for replacing keys
		ExFreePoolWithTag(MacroData, KBFLTR_KP_TAG);

	}

	*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);

	//(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);
#endif
}


/*++

Routine Description:

	Completion Routine

Arguments:

	Target - Target handle
	Request - Request handle
	Params - request completion params
	Context - Driver supplied context


Return Value:

	VOID

--*/
VOID KeyboardFilterRequestCompletionRoutine(WDFREQUEST Request, WDFIOTARGET Target, PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, WDFCONTEXT Context) {
	UNREFERENCED_PARAMETER(Target);

	WDFMEMORY buffer = CompletionParams->Parameters.Ioctl.Output.Buffer;
	NTSTATUS status = CompletionParams->IoStatus.Status;
	PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)Context;

	// Save the keyboard attributes in our context area
	// so that we can return them to the app later

	if (NT_SUCCESS(status) && CompletionParams->Type == WdfRequestTypeDeviceControlInternal && CompletionParams->Parameters.Ioctl.IoControlCode == IOCTL_KEYBOARD_QUERY_ATTRIBUTES) {
		if (CompletionParams->Parameters.Ioctl.Output.Length >= sizeof(KEYBOARD_ATTRIBUTES)) {

			KdPrint(("[KB] Cached KeyboardAttributes\n"));
			status = WdfMemoryCopyToBuffer(buffer, CompletionParams->Parameters.Ioctl.Output.Offset, &devExt->KeyboardAttributes, sizeof(KEYBOARD_ATTRIBUTES));
		}
	}

	WdfRequestComplete(Request, status);

	return;
}

/// <summary>
/// Get's a WdfDevice object from a driver-generated DeviceID
/// </summary>
/// <param name="DeviceID">
/// The ID of the device we need
/// </param>
/// <returns>
/// WDFDEVICE if a device was found using specified DeviceID, NULL otherwise
/// </returns>
WDFDEVICE KBFLTR_GetDeviceByCustomID(DWORD DeviceID) {

	// Search for the device in the hash
	PDEVICE_EXTENSION devExtHashSearch;
#pragma warning(disable:4127)
	HASH_FIND(hh, _keyboards, &DeviceID, sizeof(DWORD), devExtHashSearch);
#pragma warning(default:4127)

	// Return the device if found, else return NULL
	if (devExtHashSearch == NULL) return NULL;
	return devExtHashSearch->WdfDevice;
}

/// <summary>
/// Get number of devices(keyboards) that supports macros
/// </summary>
/// <returns></returns>
size_t KBFLTR_GetNumberOfKeyboards() {
	return HASH_COUNT(_keyboards);
}