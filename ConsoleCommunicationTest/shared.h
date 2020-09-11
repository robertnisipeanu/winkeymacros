#pragma once

#define FILE_DEVICE_INVERTED 0xCF54
#define IOCTL_KEYBOARDFILTER_KEYPRESSED_QUEUE CTL_CODE(FILE_DEVICE_INVERTED, 2049, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARDFILTER_SENDKEYPRESS CTL_CODE(FILE_DEVICE_INVERTED, 2050, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARDFILTER_GETKEYBOARDS CTL_CODE(FILE_DEVICE_INVERTED, 2051, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// type definition for the object that holds a key press input
//
typedef struct _CUSTOM_KEYBOARD_INPUT {
    //
    // Unit number.  E.g., for \Device\KeyboardPort0 the unit is '0',
    // for \Device\KeyboardPort1 the unit is '1', and so on.
    //

    USHORT UnitId;

    DWORD DeviceID;

    //
    // The "make" scan code (key depression).
    //

    USHORT MakeCode;

    //
    // The flags field indicates a "break" (key release) and other
    // miscellaneous scan code information defined below.
    //

    USHORT Flags;

    USHORT Reserved;

    //
    // Device-specific additional information for the event.
    //

    ULONG ExtraInformation;
} CUSTOM_KEYBOARD_INPUT, * PCUSTOM_KEYBOARD_INPUT;

//
// type definition for the object that holds info about a keyboard
//
typedef struct _CUSTOM_KEYBOARD_INFO {

    //
    // Specifies the unit number of a keyboard device.
    // It's the N in '\Device\KeyboardPortN'
    //
    USHORT UnitId;

    //
    // The DeviceID is allocated by our driver (auto-incremented with each device created(keyboard plugged)).
    // It's supposed to be an unique identifier of a keyboard between the library and the driver so we won't
    // have to pass strings (eg. HID) between them for every input. The app can cache the HID associated
    // with this ID
    //
    DWORD DeviceID;

    //
    // Maybe replace this 
    //
    WCHAR HID[256];

    //
    // Specifies the scan code mode
    //
    USHORT KeyboardMode;

    //
    // Specifies the number of function keys that a keyboard supports
    //
    USHORT NumberOfFunctionKeys;

    //
    // Specifies the number of LED indicators that a keyboard supports.
    //
    USHORT NumberOfIndicators;

    //
    // Specifies the number of keys that a keyboard supports
    //
    USHORT NumberOfKeysTotal;

} CUSTOM_KEYBOARD_INFO, * PCUSTOM_KEYBOARD_INFO;