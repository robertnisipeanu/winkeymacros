#pragma once
#include "Driver.h"

NTSTATUS UserCommunication_RegisterControlDevice(WDFDRIVER WdfDriver);

extern "C" {
	EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL UserCommunication_EvtIoDeviceControl;
	EVT_WDF_DEVICE_SHUTDOWN_NOTIFICATION UserCommunication_EvtWdfDeviceShutdownNotification;
}