#pragma once

#include <windows.h>

#ifdef MACROLIB_EXPORT
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma region shared_structs.h

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

#pragma endregion

    enum MACROLIB_STATUS {
        MACROLIB_STATUS_SUCCESS,
        MACROLIB_STATUS_CONNECTION_FAIL,
        MACROLIB_STATUS_GENERAL_ERROR,
    };


    /// <summary>
    /// Initialize the library, you need to call this before any other method that communicates with the driver or that method would return MACROLIB_STATUS_CONNECTION_FAIL
    /// </summary>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_initialize();

    /// <summary>
    /// Deinitialize the library. Library will perform a cleanup, after calling this method you need to call initialize method again if you want to call any other method.
    /// </summary>
    /// <returns></returns>
    void DLLEXPORT macrolib_deinitialize();

    /// <summary>
    /// Gets the number of keyboards
    /// </summary>
    /// <param name="outLength">
    /// Pointer to a size_t var that's gonna get the number of keyboards
    /// </param>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_get_keyboardslen(size_t* outLength);
    /// <summary>
    /// Gets info about connected keyboards
    /// </summary>
    /// <param name="outKeyboards">
    /// An array of CUSTOM_KEYBOARD_INFO (pointer to the first element) where the info is gonna be stored
    /// </param>
    /// <param name="outKeyboardsLength">
    /// Pointer to a size_t var that's gonna get the number of populated CUSTOM_KEYBOARD_INFO
    /// </param>
    /// <param name="keyboardsMaxLength">
    /// Max size of outKeyboards array
    /// </param>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_get_keyboards(PCUSTOM_KEYBOARD_INFO outKeyboards, size_t keyboardsMaxLength, size_t* outKeyboardsLength);

    /// <summary>
    /// Add a macro
    /// </summary>
    /// <param name="macroKey">
    /// Macro key (replaced key)
    /// </param>
    /// <param name="replacingKeys">
    /// An array of INPUT_KEYBOARD_KEY which are the replacing keys.
    /// </param>
    /// <param name="replacingKeysLength">
    /// The length of the replacingKeys array
    /// </param>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_add_macro(INPUT_KEYBOARD_MACRO macroKey, PINPUT_KEYBOARD_KEY replacingKeys, size_t replacingKeysLength);

    /// <summary>
    /// Check if there is a macro on a key
    /// </summary>
    /// <param name="macroKey">
    /// Macro key that will be checked
    /// </param>
    /// <param name="outResult">
    /// Pointer to a BOOLEAN var that will get the result
    /// </param>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_is_macro(INPUT_KEYBOARD_MACRO macroKey, BOOLEAN* outResult);

    /// <summary>
    /// Delete a macro
    /// </summary>
    /// <param name="macroKey">
    /// Macro key that should be deleted
    /// </param>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_delete_macro(INPUT_KEYBOARD_MACRO macroKey);

    /// <summary>
    /// Get the number of INPUT_KEYBOARD_KEYs (replacing keys) from a macro
    /// </summary>
    /// <param name="macro">
    /// Macro which to get the size of
    /// </param>
    /// <param name="outLength">
    /// Pointer to a size_t var that will get the result
    /// </param>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_get_macro_length(INPUT_KEYBOARD_MACRO macro, size_t* outLength);

    /// <summary>
    /// Get the replacing keys of a macro
    /// </summary>
    /// <param name="macroKey">
    /// Macro which to get replacing keys of
    /// </param>
    /// <param name="outKeys">
    /// An array of INPUT_KEYBOARD_KEYs that will get the result
    /// </param>
    /// <param name="outKeysLength">
    /// Pointer to a size_t var that's gonna get the number of replacing keys
    /// </param>
    /// <param name="keysMaxLength">
    /// Maximum length of outKeys
    /// </param>
    /// <returns></returns>
    MACROLIB_STATUS DLLEXPORT macrolib_get_macro(INPUT_KEYBOARD_MACRO macroKey, PINPUT_KEYBOARD_KEY outKeys, size_t keysMaxLength, size_t* outKeysLength);


    MACROLIB_STATUS DLLEXPORT macrolib_identify_key();

#ifdef __cplusplus
}
#endif