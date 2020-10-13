#pragma once

#ifndef _KERNEL_MODE
#include <windows.h>
#endif

/// <summary>
/// Second to last objects passed as arguments from ADDMACRO IOCTL.
/// It represents the keys that replaces the macro
/// </summary>
typedef struct _INPUT_KEYBOARD_KEY {

    USHORT MakeCode;

    USHORT Flags;

} INPUT_KEYBOARD_KEY, * PINPUT_KEYBOARD_KEY;


/// <summary>
/// The replaced key of the macro.
/// </summary>
typedef struct _INPUT_KEYBOARD_MACRO {

    DWORD DeviceID;

    USHORT ReplacedKeyScanCode;

} INPUT_KEYBOARD_MACRO, * PINPUT_KEYBOARD_MACRO;

/// <summary>
/// A key identifier (used as output from IOCTL_KEYBOARDFILTER_IDENTIFYKEY)
/// </summary>
typedef struct _KEYPRESS_IDENTIFY {

    DWORD DeviceID;

    INPUT_KEYBOARD_KEY Key;

} KEYPRESS_IDENTIFY, * PKEYPRESS_IDENTIFY;

/// <summary>
/// type definition for the object that holds info about a keyboard
/// </summary>
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