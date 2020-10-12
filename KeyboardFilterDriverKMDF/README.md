
# FAQ 

## What is KeyboardFilterDriverKMDF?
KeyboardFilterDriverKMDF is a Windows 10 driver designed to allow Windows to take advantage of multiple keyboards. It allows apps to put macros on a key of a specific device (just like it would be a macro key from a keyboard of any brand (like Logitech, Razer and so on)).
This driver can replace any key of any keyboard with a sequence of other keys.

## Why was the driver created?
I needed to use my second keyboard as a macro keyboard (replacing all the keys on that keyboard) and I couldn't find any [reliable, easy to use and cheap solution] (let's call them "the 3 bullet points").
I got the inspiration to create the driver after watching TaranVH's videos about this subject, in which he also struggled to find a good solution, but neither of them were achieving all the 3 bullet points.

## How the driver works?
It takes a replacement KEY with a KEYBOARD as input along with a sequence of keys that are gonna replace that key/keyboard combination and then stores them in a hashtable. Whenever a key is pressed, it checks the hashtable for any macro entry, if there is one, then instead of executing the original key it executes the replacing keys.

## Why do all this logic inside the driver and not just pass the key pressses to the app and the app takes care of the logic?
This is exactly what interception is doing (this logic is inside the app) and this is the first approach I also took. The problem with this approach is that introduces two issues:
- a small issue would be added latency, as the driver has to communicate with the app for every key press, while this is supposed to be fast and not noticeable, in times where the CPU is at a big usage, the app that runs in user-mode can get less priority from windows and less CPU time, which would result in added latency;
- a bigger issue would be security related, because a driver is running in kernel mode, it doesn't knows anything about secure desktops (two examples of a secure desktop are the login page of windows and the prompt to input your administrator password) and it would pass every key press to the app. A malicious app could use that data to keylog everything, even what you pressed while in a SECURE DESKTOP.

## How can I contribute?
If you want to add a new feature, solve a bug, fix a typo or anything else, please open an issue first describing what you need and how do you want to add it, how it would help others using the driver, what security issues it implies (if it implies) and what are the alternatives to achieve this to get around thos issues. If everything is fine and we all agree the feature is needed, then you can open a PR with the changes.

# Dev Documentation

If you want to develop something on top of this driver, then what you're interested on starts here. I will try to provide documentation for every feature the driver has and how to use them.
If something is missing from here, please feel free to add a PR modifying this README.md or a issue specifying what is missing.

If you do not know how communication with a driver works and what is a IOCTL, then please use the LowLevelKeyboard library that implements this communication and is supposed to be easier to use (the driver still needs to be installed for the library to work, the library just acts as an API for the communication so you do not have to do it yourself). 

All of the structures and IOCTLs mentioned here can be found in `shared.h` which you should include in your project if you do not want to use the LowLevelKeyboard library.

## How long is a macro stored?
Each time a keyboard gets plugged in, the driver allocates an auto-incrementing `DWORD` DeviceID to it. The driver uses the DeviceID to store the macros for the keyboard. 
If a keyboard is being reconnected (disconnected and then connected again), it will reallocate a new DeviceID to it, so every macro stored in the driver would be lost. 
Because the DeviceID is a variable, it's gonna reset at every system boot (first value is 1). 
There is no way that two keyboards gets the same DeviceID in a single system uptime period, once a keyboard is disconnected, that DeviceID won't be allocated to any other keyboards connected after (not even to the same keyboard if it's reconnected). 
To make the macros persistent, you need to store the macros in your own app and identify devices(and add the macros to them) based on other non-changing(or rarely changing) properties of the keyboard(use your imagination, it can be anything, like the device HID, vendor id, product id and so on).

## IOCTLs

An app can communicate with the driver using IOCTLs. I will explain bellow what every IOCTL does and what parameters it needs. This documentation doesn't shows you how to call an IOCTL in your app, but what every IOCTL does.
If you want to see an example of how to call the IOCTLs, then look at the LowLevelKeyboard library, or even better, use the library cause it was created so you do not have to implement the communication yourself.

If a variable is referenced with 'Size' in name, then it means the size in bytes, if it is referenced with 'Length' in name, then it means the number of objects.
e.g.: A method takes two parameters: `PINPUT_KEYBOARD_KEY keys, size_t keysSize`, where keys is an array of 3 elements, then `keysSize = 3 * sizeof(INPUT_KEYBOARD_KEY)`. Another method takes two parameters: `PINPUT_KEYBOARD_KEY keys, size_t keysLength`, where keys is an array of 3 elements, then `keysLength = 3`.

Common return status codes (you can get them from any of the below listed IOCTLs):
- `STATUS_INVALID_BUFFER_SIZE` -> Input buffer or output buffer are too small
- Any status returned by `WdfRequestRetrieveInputBuffer`, `WdfRequestRetrieveOutputBuffer`, `WdfMemoryCopyToBuffer`

### IOCTL_KEYBOARDFILTER_GETKEYBOARDSLENGTH
Description: Gets the number of keyboards the driver is currently attached to (should be equivalent with the number of connected keyboards)

Input:
- NO INPUT

Output:
- output buffer needs to be `sizeof(size_t)`, the driver returns a size_t variable that is equal with the number of keyboards it is currently attached to


Return status:
- `STATUS_SUCCESS` -> Returned the number of keyboards successfully

### IOCTL_KEYBOARDFILTER_GETKEYBOARDS
Description: Gets a list of `CUSTOM_KEYBOARD_INFO` (keyboards information) of currently connected keyboards.

Input:
- NO INPUT

Output:
- output buffer needs to be at least(and a multiplier of) `sizeof(CUSTOM_KEYBOARD_INFO)`. It will populate as much of the buffer as you give, so if there are 3 keyboards and your buffer size is `2 * sizeof(CUSTOM_KEYBOARD_INFO)`, it will populate the buffer with 2 keyboards information and return STATUS_SUCCESS. If you want to get info about all the keyboards, then firstly call IOCTL_KEYBOARDFILTER_GETKEYBOARDSLENGTH to get how many keyboards info you can get.


Return status:
- `STATUS_SUCCESS` -> Returned X keyboards information (where X depends on the buffer size)

### IOCTL_KEYBOARDFILTER_ADDMACRO
Description: Adds a macro for a keyboard - key combination.

Input:
- input buffer is composed of:
	- an INPUT_KEYBOARD_MACRO structure that stores the device id and the scancode of the key on which to add the macro
	- with an offset of `sizeof(INPUT_KEYBOARD_MACRO)` (so immediatly after the INPUT_KEYBOARD_MACRO structure) it needs to be an array of INPUT_KEYBOARD_KEYs that represent the replacing keys for the macro
	- If there is nothing after the INPUT_KEYBOARD_MACRO the driver is gonna assume that you want to disable the key from INPUT_KEYBOARD_MACRO, so that key would become useless until the macro is replaced/deleted or system reboots

Output:
- NO OUTPUT


Return status:
- `STATUS_SUCCESS` -> Macro was added successfully

### IOCTL_KEYBOARDFILTER_ISMACRO
Description: Checks if there is a macro on a keyboard - key combination.

Input:
- input buffer needs to be an INPUT_KEYBOARD_MACRO structure that stores the device id and the scancode of the key used to check if a macro is on

Output:
- output buffer needs to be `sizeof(BOOLEAN)`, it returns TRUE if there is a macro on the keyboard/key combination specified in the input, FALSE otherwise


Return status:
- `STATUS_SUCCESS` -> If a keyboard was found, no matter if there is a macro or not on that key
- `STATUS_NOT_FOUND` -> Keyboard was not found
- `STATUS_GENERIC_COMMAND_FAILED` -> Failed to allocate memory for the macro in hashtable

### IOCTL_KEYBOARDFILTER_DELETEMACRO
Description: Deletes a macro from a keyboard - key combination.

Input:
- input buffer needs to be an INPUT_KEYBOARD_MACRO structure that stores the device id and the scancode of the key of which the macro should be removed

Output:
- NO OUTPUT


Return status:
- `STATUS_SUCCESS` -> Macro was deleted (returned even if there was no macro stored)
- `STATUS_NOT_FOUND` -> Keyboard was not found

### IOCTL_KEYBOARDFILTER_GETMACRO
Description: Gets replacing key sequence of a macro.

Input:
- input buffer needs to be an INPUT_KEYBOARD_MACRO structure that stores the device id and the scancode of the key you want to get the macro of

Output:
- output buffer needs to be `macroLength * sizeof(INPUT_BUFFER_KEY)` (first call `IOCTL_KEYBOARDFILTER_GETMACROLENGTH` to get the macroLength), it returns an array of INPUT_KEYBOARD_KEY which is the replacing keys sequence of the macro


Return status:
- `STATUS_SUCCESS` -> Replacing keys sequence successfully returned
- `STATUS_NOT_FOUND` -> Keyboard was not found
- `STATUS_NO_MATCH` -> No macro found on that INPUT_KEYBOARD_MACRO

### IOCTL_KEYBOARDFILTER_GETMACROLENGTH
Description: Gets how many keys are stored in the replacing key sequence of a macro.

Input:
- input buffer needs to be an INPUT_KEYBOARD_MACRO structure that stores the device id and the scancode of the key of which you want to know the macro length;

Output:
- output buffer needs to be `sizeof(size_t)`, it returns the number of INPUT_KEYBOARD_KEY (replacing key sequence) that are stored for that macro


Return status:
- `STATUS_SUCCESS` -> A macro was found
- `STATUS_NOT_FOUND` -> Keyboard was not found
- `STATUS_NO_MATCH` -> No macro found on that INPUT_KEYBOARD_MACRO

### IOCTL_KEYBOARDFILTER_IDENTIFYKEY
Description: Gets the next key press from any keyboard (not affected by macros). The Request will be added to a Queue and will be completed later, when a key press happens. The keypress event will be cancelled for security reasons (to keep keyloggers away, user would see the key is not happening if he is another app and realize something is wrong).

Input:
- NO INPUT

Output:
- --

Return status:
- --