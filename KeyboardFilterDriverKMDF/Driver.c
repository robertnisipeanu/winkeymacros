#include "Driver.h"

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;
	WDFDRIVER WdfDriver;

	KdPrint(("[KB] Loading driver...\n"));

	//
	// Initialize driver config to control the attributes that
	// are global to the driver. Note that framework by default
	// provides a driver unload routine. If you create any resources
	// in the DriverEntry and want to be cleaned in driver unload,
	// you can override that by manually setting the EvtDriverUnload in the
	// config structure. In general xxx_CONFIG_INIT macros are provided to
	// initialize most commonly used members
	//

	WDF_DRIVER_CONFIG_INIT(&config, KeyboardFilter_EvtDeviceAdd);

	//
	// Create a framework driver object to represent our driver
	//
	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &WdfDriver);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] WdfDriverCreate failed with status 0x%x\n", status));
		return status;
	}

	KdPrint(("[KB] DriverObject created\n"));

	status = UserCommunication_RegisterControlDevice(WdfDriver);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] RegisterControlDevice failed with status 0x%x\n", status));
		return status;
	}

	//status = WdfDeviceCreate();

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

	//
	// Tell the framework that you are filter driver. Framework
	// takes care of inherting all the device flags & characteristics
	// from the lower device you are attaching to
	//
	WdfFdoInitSetFilter(DeviceInit);

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_KEYBOARD);

	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);

	//
	// Create a framework device object. This call will in turn create
	// a WDM deviceobject, attach to the lower stack and set the
	// appropriate flags and attributes.
	//
	WDFDEVICE hDevice;
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &hDevice);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] WdfDeviceCreate failed with status code 0x%x\n", status));
		return status;
	}

	PDEVICE_EXTENSION filterExt;
	filterExt = FilterGetData(hDevice);

	//
	// Configure the default queue to be Parallel. Do not use sequential queue
	// if this driver is going to be filtering PS2 ports because it can lead to
	// deadlock. The PS2 port driver sends a request to the top of the stack when it
	// receives an ioctl request and waits for it to be completed. If you use a
	// sequential queue, this request will be stuck in the queue because of the
	// outstanding ioctl request sent earlier to the port driver.
	//
	WDF_IO_QUEUE_CONFIG ioQueueConfig;
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);

	//
	// Framework by default creates a non-power managed queues for
	// filte drivers
	//
	ioQueueConfig.EvtIoInternalDeviceControl = KeyboardFilter_EvtIoInternalDeviceControl;
	//ioQueueConfig.EvtIoRead = KeyboardFilter_EvtIoRead;

	status = WdfIoQueueCreate(hDevice, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] WdfIoQueueCreate failed with code 0x%x\n", status));
		return status;
	}

	//

	return status;
}

VOID KeyboardFilter_EvtIoInternalDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t OutputBufferLength, IN size_t InputBufferLength, IN ULONG IoControlCode) {
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);
	PAGED_CODE();

	KdPrint(("[KB] EvtIoInternalDeviceControl"));

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
	devExt = FilterGetData(hDevice);

	switch (IoControlCode) {

	case IOCTL_INTERNAL_KEYBOARD_CONNECT:
		//
		// Only allow one connection
		//
		if (devExt->UpperConnectData.ClassService != NULL) {
			status = STATUS_SHARING_VIOLATION;
			break;
		}

		//
		// Get the input buffer from the request
		// (Parameters.DeviceIoControl.Type3InputBuffer)
		//
		status = WdfRequestRetrieveInputBuffer(Request, sizeof(CONNECT_DATA), &connectData, &length);
		if (!NT_SUCCESS(status)) {
			KdPrint(("[KB] WdfRequestRetrieveInputBuffer failed with error code 0x%x\n", status));
			break;
		}

		NT_ASSERT(length == InputBufferLength);

		devExt->UpperConnectData = *connectData;
		
		//
		// Hook into the report chain. Everytime a keyboard packet is reported
		// to the system, KeyboardFilter_ServiceCallback will be called
		//

		connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);

#pragma warning(disable:4152) // nonstandard extension, function/data pointer conversion

		connectData->ClassService = KeyboardFilter_ServiceCallback;

#pragma warning(default:4152)

		break;

	case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:
		//
		// Clear the connection parameters in the device extension.
		//
		devExt->UpperConnectData.ClassDeviceObject = NULL;
		devExt->UpperConnectData.ClassService = NULL;

		//
		// Delete the created WdfDevice
		//
		WdfObjectDelete(devExt->Wdfdevice);

		break;

	case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
		forwardWithCompletionRoutine = TRUE;
		completionContext = devExt;
		break;

		//
		// Might want to capture these in the future. For now, then pass them down
		// the stack. These queries must be successful for the RIT to communicate
		// with the keyboard.
		//
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

	//
	// Forward the request down. WdfDeviceGetIoTarget returns
	// the default target, which represents the device attached
	// to us below in the stack
	//

	if (forwardWithCompletionRoutine) {
		//
		// Format the request with the output memory so the completion routine
		// can access the return data in order to cache it into the context area
		//
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

		//
		// Set our completion routine with a context area
		// that we will save the output data into
		//
		WdfRequestSetCompletionRoutine(Request, KeyboardFilterRequestCompletionRoutine, completionContext);

		ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), WDF_NO_SEND_OPTIONS);

		if (ret == FALSE) {
			status = WdfRequestGetStatus(Request);
			KdPrint(("[KB] WdfRequestSend failed: 0x%x\n", status));
			WdfRequestComplete(Request, status);
		}
	}
	else {
		//
		// We are not interested in post processing the IRP so
		// fire and forget.
		//
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

	PDEVICE_EXTENSION devExt;
	WDFDEVICE hDevice;

	PINVERTED_DEVICE_CONTEXT InvertedDeviceContext;
	//NTSTATUS QueueReturnStatus;
	//ULONG_PTR info;
	//LONG valueToReturn;
	//WDFREQUEST notifyRequest;
	//PULONG  bufferPointer;

	hDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
	devExt = FilterGetData(hDevice);

	// ControlDevice not yet initialized, process keyboard input normally
	if (ControlDevice == NULL) {
		KdPrint(("[KB] No ControlDevice found, passing packets normally\n"));
		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);
		return;
	}
	InvertedDeviceContext = InvertedGetContextFromDevice(ControlDevice);

	KdPrint(("[KB] Input from device: %u; %u\n", devExt->KeyboardAttributes.KeyboardIdentifier.Type, devExt->KeyboardAttributes.KeyboardIdentifier.Subtype));

	/*PKEYBOARD_INPUT_DATA pCur = InputDataStart;

	while (pCur < InputDataEnd) {
		ULONG consumed = 0;



		KdPrint(("[KB] Scan code %x\n", pCur->MakeCode));
		if (pCur->MakeCode == 0x1f) {
			KdPrint(("[KB] Replacing scancode\n"));
			pCur->MakeCode = 0x00;
		}

		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, pCur, pCur+1, &consumed);
		pCur++;
	}*/

	// Notify START

	/*NTSTATUS queueAvailable = WdfIoQueueRetrieveNextRequest(InvertedDeviceContext->NotificationQueue, &notifyRequest);

	// No queue available, process keyboard input normally
	if (!NT_SUCCESS(queueAvailable)) {
		KdPrint(("[KB] No queue available, passing packets normally"));
		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);
		return;
	}

	//
	// Get a pointer to the output buffer that was passed-in with the user
	// notification IOCTL. We'll use this to return additional info about
	// the event.
	//
	queueAvailable = WdfRequestRetrieveOutputBuffer(notifyRequest, sizeof(LONG), (PVOID*)&bufferPointer, nullptr);
	if (!NT_SUCCESS(queueAvailable)) {
		// Should never get here
		QueueReturnStatus = STATUS_SUCCESS;
		info = 0;

		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
		(*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);

		WdfRequestCompleteWithInformation(notifyRequest, QueueReturnStatus, info);
	}
	else {
		valueToReturn = 1;
		*bufferPointer = valueToReturn;

		QueueReturnStatus = STATUS_SUCCESS;
		info = sizeof(LONG);

		ULONG total = 0, temp = 0;
		KEYBOARD_INPUT_DATA *pCur;

		for (pCur = InputDataStart; pCur < InputDataEnd; pCur++) {
			total++;
			(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, pCur, pCur + 1, &temp);
			total += temp;
		}

		*InputDataConsumed = total;

		KdPrint(("[KB] Dropped %u packets\n", total));

		WdfRequestCompleteWithInformation(notifyRequest, QueueReturnStatus, info);
	}*/

	//ULONG total = 0;
	KEYBOARD_INPUT_DATA *pCur;
	ULONG total = 0;

	for (pCur = InputDataStart; pCur < InputDataEnd; pCur++) {
		ULONG consumed = 0;

		if (pCur->MakeCode == 0x1f) {
			consumed += 1;
			KdPrint(("[KB] S pressed, consumed currvalue: %u\n", consumed));
		}
		else {
			(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, pCur, pCur + 1, &consumed);
		}

		//ULONG temp = 0;
		total += consumed;

		//RtlCopyMemory(data, pCur, sizeof(KEYBOARD_INPUT_DATA));
		//total++;
		//(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, data, data + 1, &temp);
		//total += temp;
	}

	*InputDataConsumed = (ULONG) (InputDataEnd - InputDataStart);

	//*InputDataConsumed = total;

	//(*(PSERVICE_CALLBACK_ROUTINE)devExt->UpperConnectData.ClassService)(devExt->UpperConnectData.ClassDeviceObject, InputDataStart, InputDataEnd, InputDataConsumed);

	// Notify END
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

	//
	// Save the keyboard attributes in our context area
	// so that we can return them to the app later
	//

	if (NT_SUCCESS(status) && CompletionParams->Type == WdfRequestTypeDeviceControlInternal && CompletionParams->Parameters.Ioctl.IoControlCode == IOCTL_KEYBOARD_QUERY_ATTRIBUTES) {
		if (CompletionParams->Parameters.Ioctl.Output.Length >= sizeof(KEYBOARD_ATTRIBUTES)) {

			KdPrint(("[KB] Cached KeyboardAttributes"));
			status = WdfMemoryCopyToBuffer(buffer, CompletionParams->Parameters.Ioctl.Output.Offset, &((PDEVICE_EXTENSION)Context)->KeyboardAttributes, sizeof(KEYBOARD_ATTRIBUTES));
		}
	}

	WdfRequestComplete(Request, status);

	return;
}

