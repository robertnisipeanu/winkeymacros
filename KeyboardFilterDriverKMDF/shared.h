#pragma once

#define FILE_DEVICE_INVERTED 0xCF54
#define IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE CTL_CODE(FILE_DEVICE_INVERTED, 2049, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARDFILTER_SENDKEYPRESS CTL_CODE(FILE_DEVICE_INVERTED, 2050, METHOD_BUFFERED, FILE_ANY_ACCESS)