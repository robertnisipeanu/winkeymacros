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

NTSTATUS KeyboardFilter_EvtDevicePrepareHardware(WDFDEVICE hDevice, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated) {
	UNREFERENCED_PARAMETER(ResourcesRaw);
	UNREFERENCED_PARAMETER(ResourcesTranslated);

	KdPrint(("EvtDevicePrepareHardware"));
	PDEVICE_EXTENSION filterExt = GetContextFromDevice(hDevice);

	if (filterExt->UsbDevice != NULL) {
		return STATUS_SUCCESS;
	}

	NTSTATUS status;
	// Check if device is an USB device (so we can query for USB information used to identify the device more accurately, like Serial Number)
	WCHAR enumeratorName[32];
	ULONG enumeratorNameLength;
	status = WdfDeviceQueryProperty(hDevice, DevicePropertyEnumeratorName, sizeof(enumeratorName), enumeratorName, &enumeratorNameLength);
	if (!NT_SUCCESS(status)) {
		KdPrint(("[KB] WdfDeviceQueryProperty[EnumeratorName] failed with status code 0x%x\n", status));
		return status;
	}

	UNICODE_STRING enumeratorNameString, enumeratorNameUSB;

	RtlInitUnicodeString(&enumeratorNameUSB, L"HID");
	RtlInitUnicodeString(&enumeratorNameString, enumeratorName);

	if (RtlCompareUnicodeString(&enumeratorNameString, &enumeratorNameUSB, TRUE) == 0) {

		KdPrint(("[KB] Device is USB\n"));
		// Device is USB

		WDF_USB_DEVICE_CREATE_CONFIG UsbDeviceConfig;

		// Create WDFUSBDEVICE object
		WDF_USB_DEVICE_CREATE_CONFIG_INIT(&UsbDeviceConfig, USBD_CLIENT_CONTRACT_VERSION_602);

		NTSTATUS UsbDeviceCreateStatus = WdfUsbTargetDeviceCreateWithParameters(hDevice, &UsbDeviceConfig, WDF_NO_OBJECT_ATTRIBUTES, &filterExt->UsbDevice);
		if (!NT_SUCCESS(UsbDeviceCreateStatus)) {
			KdPrint(("WdfUsbTargetDeviceCreateWithParameters failed with status 0x%x\n", UsbDeviceCreateStatus));

			// If failed to create an USB Device, handle device as non-usb (PS/2)
			filterExt->UsbDevice = NULL;
		}

	}
	else {
		KdPrint(("[KB] Device is not USB\n"));
		filterExt->UsbDevice = NULL;
	}

	// URB urb;
	// USB_DEVICE_DESCRIPTOR deviceDescriptor;

	// UsbBuildGetDescriptorRequest(&urb, sizeof(urb), USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, &deviceDescriptor, NULL, sizeof(USB_DEVICE_DESCRIPTOR), NULL);

	return STATUS_SUCCESS;
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



	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = KeyboardFilter_EvtDevicePrepareHardware;
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

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

	filterExt->UsbDevice = NULL;

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

VOID KeyboardFilter_EvtIoInternalDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t OutputBufferSize, IN size_t InputBufferSize, IN ULONG IoControlCode) {
	UNREFERENCED_PARAMETER(OutputBufferSize);
	UNREFERENCED_PARAMETER(InputBufferSize);
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

		NT_ASSERT(length == InputBufferSize);

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

	WDFDEVICE WdfDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
	PDEVICE_EXTENSION devExt = GetContextFromDevice(WdfDevice);
	MacroManager* macroManager = (MacroManager*)devExt->MacroManager;

	// Loop through all input(key presses) we got (usually it is only one, but better to be safe)
	PKEYBOARD_INPUT_DATA pCur;

	for (pCur = InputDataStart; pCur < InputDataEnd; pCur++) {

		// Check if running in identify mode and cancel the key press if so
		BOOLEAN identifyMode = KBFLTR_ProcessKeyPressIdentifyMode(ControlDevice, devExt->DeviceID, pCur);
		if (identifyMode) {
			continue;
		}

		//
		// NORMAL MODE
		//

		// Check if there is a macro on the key
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
}

/// <summary>
/// Processes identify mode for a key press
/// </summary>
/// <param name="controlDevice">
/// The device that handles user communication
/// </param>
/// <returns>
/// TRUE -> Processed key as identify mode
/// FALSE -> Key not processed, running in normal mode
/// </returns>
BOOLEAN KBFLTR_ProcessKeyPressIdentifyMode(WDFDEVICE controlDevice, DWORD DeviceID, PKEYBOARD_INPUT_DATA kbInput) {

	// If ControlDevice not yet initialized, process keyboard input normally
	if (controlDevice == NULL) {
		return FALSE;
	}

	PINVERTED_DEVICE_CONTEXT devExt = InvertedGetContextFromDevice(controlDevice);
	WDFREQUEST notifyRequest;

	// If no queue available, process keyboard input normally
	NTSTATUS queueAvailable = WdfIoQueueRetrieveNextRequest(devExt->IdentifyKeyQueue, &notifyRequest);
	if (!NT_SUCCESS(queueAvailable)) {
		return FALSE;
	}

	NTSTATUS queueReturnStatus = STATUS_SUCCESS;
	ULONG_PTR info = 0;

	// Get a pointer to the output buffer that was passed in with the user
	// IDENTIFYKEY IOCTL
	PKEYPRESS_IDENTIFY bufferPointer;

	queueAvailable = WdfRequestRetrieveOutputBuffer(notifyRequest, sizeof(KEYPRESS_IDENTIFY), reinterpret_cast<PVOID*>(&bufferPointer), NULL);
	if (!NT_SUCCESS(queueAvailable)) {
		// Should never get here as we already check for buffer size when we process the IOCTL, but we check to be sure
		queueReturnStatus = STATUS_BUFFER_TOO_SMALL;

		WdfRequestCompleteWithInformation(notifyRequest, queueReturnStatus, info);

		return FALSE;
	}

	// Populate output buffer with data
	bufferPointer->DeviceID = DeviceID;
	bufferPointer->Key.MakeCode = kbInput->MakeCode;
	bufferPointer->Key.Flags = kbInput->Flags;

	// Return data and cancel key press
	info = sizeof(KEYPRESS_IDENTIFY);
	WdfRequestCompleteWithInformation(notifyRequest, queueReturnStatus, info);
	return TRUE;
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
			status = WdfMemoryCopyToBuffer(buffer, CompletionParams->Parameters.Ioctl.Output.Offset, &devExt->KeyboardAttributes, sizeof(KEYBOARD_ATTRIBUTES));
			KdPrint(("[KB] Cached KeyboardAttributes\n"));
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

/// <summary>
/// Get the list of devices(keyboards) that supports macros
/// </summary>
/// <param name="buffer">
/// OUT - Buffer where array of CUSTOM_KEYBOARD_INFO is gonna be stored
/// </param>
/// <param name="maxKeyboards">
/// Max number of keyboards that can be stored in the provided buffer
/// </param>
/// <param name="keyboardsNumberOutput">
/// OUT - Pointer to a size_t object which will get how many CUSTOM_KEYBOARD_INFO objects was copied into the buffer
/// </param>
void KBFLTR_GetKeyboardsInfo(PCUSTOM_KEYBOARD_INFO buffer, size_t maxKeyboards, size_t* keyboardsNumberOutput) {

	*keyboardsNumberOutput = 0;
	PDEVICE_EXTENSION s, tmp;

	HASH_ITER(hh, _keyboards, s, tmp) {

		// If there is no more space in the buffer, return
		if (*keyboardsNumberOutput >= maxKeyboards) {
			return;
		}

		// Create the current keyboard info item and populate with data
		CUSTOM_KEYBOARD_INFO sInfo;

		sInfo.DeviceID = s->DeviceID;
		sInfo.NumberOfFunctionKeys = s->KeyboardAttributes.NumberOfFunctionKeys;
		sInfo.NumberOfIndicators = s->KeyboardAttributes.NumberOfIndicators;
		sInfo.NumberOfKeysTotal = s->KeyboardAttributes.NumberOfKeysTotal;

		KdPrint(("\n\n"));
		// Get device hardware ID
		ULONG dataLen;
		WCHAR test[256];

		WdfDeviceQueryProperty(s->WdfDevice, DevicePropertyHardwareID, sizeof(sInfo.HID), sInfo.HID, &dataLen);
		KdPrint(("HardwareID[%u]: %ws\n", dataLen, sInfo.HID));

		WdfDeviceQueryProperty(s->WdfDevice, DevicePropertyDeviceDescription, sizeof(test), test, &dataLen);
		KdPrint(("DeviceDescription[%u]: %ws\n", dataLen, test));

		WdfDeviceQueryProperty(s->WdfDevice, DevicePropertyManufacturer, sizeof(test), test, &dataLen);
		KdPrint(("DevicePropertyManufacturer[%u]: %ws\n", dataLen, test));

		WdfDeviceQueryProperty(s->WdfDevice, DevicePropertyFriendlyName, sizeof(test), test, &dataLen);
		KdPrint(("DevicePropertyFriendlyName[%u]: %ws\n", dataLen, test));

		WdfDeviceQueryProperty(s->WdfDevice, DevicePropertyEnumeratorName, sizeof(test), test, &dataLen);
		KdPrint(("DevicePropertyEnumeratorName [%u]: %ws\n", dataLen, test));

		WdfDeviceQueryProperty(s->WdfDevice, DevicePropertyContainerID, sizeof(test), test, &dataLen);
		KdPrint(("DevicePropertyContainerID  [%u]: %ws\n", dataLen, test));

		// Device capabilities
		{
			WDFREQUEST Request;
			WDF_REQUEST_SEND_OPTIONS options;
			WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);


			WDFIOTARGET target = WdfDeviceGetIoTarget(s->WdfDevice);

			NTSTATUS capStatus;
			DEVICE_CAPABILITIES devCap;

			RtlZeroMemory(&devCap, sizeof(DEVICE_CAPABILITIES));
			devCap.Size = sizeof(DEVICE_CAPABILITIES);
			devCap.Version = 1;
			devCap.Address = (ULONG)-1;
			devCap.UINumber = (ULONG)-1;

			IO_STACK_LOCATION stack;

			RtlZeroMemory(&stack, sizeof(stack));

			stack.MajorFunction = IRP_MJ_PNP;
			stack.MinorFunction = IRP_MN_QUERY_CAPABILITIES;
			stack.Parameters.DeviceCapabilities.Capabilities = &devCap;

			capStatus = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES, target, &Request);
			if (NT_SUCCESS(capStatus)) {

				WDF_REQUEST_REUSE_PARAMS reuse;
				WDF_REQUEST_REUSE_PARAMS_INIT(&reuse, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);

				WdfRequestReuse(Request, &reuse);

				WdfRequestWdmFormatUsingStackLocation(Request, &stack);

				if (WdfRequestSend(Request, target, &options) == TRUE) {
					capStatus = WdfRequestGetStatus(Request);
					if (NT_SUCCESS(capStatus)) {
						KdPrint(("dev_cap: Address %u, Removable: %u, SurpriseRemoval: %u, Unique ID: %u \n", devCap.Address, devCap.Removable, devCap.SurpriseRemovalOK, devCap.UniqueID));
						KdPrint(("dev_cap: Address %u, Removable: %ws, SurpriseRemoval: %ws, Unique ID: %ws \n", devCap.Address, (devCap.Removable == TRUE ? L"TRUE" : L"FALSE"), (devCap.SurpriseRemovalOK == TRUE ? L"TRUE" : L"FALSE"), (devCap.UniqueID == TRUE ? L"TRUE" : L"FALSE")));
					}
					else {
						KdPrint(("DEVICE_CAPABILITIES failed with error code 0x%x\n", capStatus));
					}
				}
				else {
					KdPrint(("DEVICE_CAPABILITIES WdfRequestSend fail\n"));
				}
			}

			if (Request != NULL) {
				WdfObjectDelete(Request);
			}
		}

		// Instance id
		{
			WDFREQUEST Request;
			WDF_REQUEST_SEND_OPTIONS options;
			WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

			WDFIOTARGET target = WdfDeviceGetIoTarget(s->WdfDevice);

			NTSTATUS instStatus;

			IO_STACK_LOCATION stack;
			RtlZeroMemory(&stack, sizeof(stack));

			stack.MajorFunction = IRP_MJ_PNP;
			stack.MinorFunction = IRP_MN_QUERY_ID;
			stack.Parameters.QueryId.IdType = BusQueryInstanceID;

			instStatus = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES, target, &Request);
			if (NT_SUCCESS(instStatus)) {

				WDF_REQUEST_REUSE_PARAMS reuse;
				WDF_REQUEST_REUSE_PARAMS_INIT(&reuse, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_NOT_SUPPORTED);

				WdfRequestReuse(Request, &reuse);

				WdfRequestWdmFormatUsingStackLocation(Request, &stack);

				if (WdfRequestSend(Request, target, &options) == TRUE) {
					instStatus = WdfRequestGetStatus(Request);
					if (NT_SUCCESS(instStatus)) {
						WDF_REQUEST_COMPLETION_PARAMS completionParams;
						WDF_REQUEST_COMPLETION_PARAMS_INIT(&completionParams);
						WdfRequestGetCompletionParams(Request, &completionParams);

						PWCHAR stringResult = reinterpret_cast<PWCHAR>(completionParams.IoStatus.Information);

						KdPrint(("Instance get status: 0x%x, Instance id: %ws\n", completionParams.IoStatus.Status, stringResult));
					}
					else {
						KdPrint(("instanceid failed with status 0x%x\n", instStatus));
					}
				}
				else {
					KdPrint(("INstanceID WdfRequestSend failed\n"));
				}

			}
		}

		//NTSTATUS testStatus = WdfUsbTargetDeviceQueryString(s->WdfDevice, )
		
		// Copy the current keyboard info item into buffer
		RtlCopyMemory(buffer + *keyboardsNumberOutput, &sInfo, sizeof(CUSTOM_KEYBOARD_INFO));

		// Increase the number of copied items
		*keyboardsNumberOutput += 1;

	}

}