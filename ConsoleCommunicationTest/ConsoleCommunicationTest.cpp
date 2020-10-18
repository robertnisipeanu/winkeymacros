// ConsoleCommunicationTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS  

#include <iostream>
#include <cstdio>
#include "MacroLibrary.h"


int main() {

    std::cout << "Initializing macro library..." << std::endl;
    MACROLIB_STATUS status = macrolib_initialize();

    if (status != MACROLIB_STATUS_SUCCESS) {
        std::cout << "Library initialization failed, closing program" << std::endl;
        return 1;
    }

    size_t numOfKeyb = 0;
    status = macrolib_get_keyboardslen(&numOfKeyb);

    if (status != MACROLIB_STATUS_SUCCESS) {
        std::cout << "Failed to get number of keyboards, closing program" << std::endl;
        return 1;
    }


    std::cout << "Number of keybords: " << numOfKeyb << std::endl;

    PCUSTOM_KEYBOARD_INFO keybInfos = reinterpret_cast<PCUSTOM_KEYBOARD_INFO>(malloc(numOfKeyb * sizeof(CUSTOM_KEYBOARD_INFO)));

    size_t actualNumOfKeyb = 0;
    status = macrolib_get_keyboards(keybInfos, numOfKeyb, &actualNumOfKeyb);

    if (status != MACROLIB_STATUS_SUCCESS) {
        std::cout << "Failed to get keyboards info, closing program" << std::endl;
    }

    std::cout << "Actual number of keyboards: " << actualNumOfKeyb << std::endl;

    for (int i = 0; i < actualNumOfKeyb; i++) {
        std::cout << "DeviceID: " << (keybInfos + i)->DeviceID << "; HID: ";
        std::wcout << (keybInfos + i)->HID << std::endl;
    }

    free(keybInfos);


    while (TRUE) {
        int number;
        std::cout << std::endl << std::endl
            << "What do you want to do?" << std::endl
            << "1. Add macro on key 'A' keyboard '1'" << std::endl
            << "2. Is macro on key 'A' keyboard '1'" << std::endl
            << "3. Get macro on key 'A' keyboard '1'" << std::endl
            << "4. Delete macro on key 'A' keyboard '1'" << std::endl
            << std::endl << "Your input: ";
        std::cin >> number;

        switch (number) {
        case 1: {
            INPUT_KEYBOARD_MACRO macro;
            INPUT_KEYBOARD_KEY replacingKeys[8];

            macro.DeviceID = 1;
            macro.ReplacedKeyScanCode = 0x1e;

            replacingKeys[0].MakeCode = 0x14; replacingKeys[0].Flags = RI_KEY_MAKE;
            replacingKeys[1].MakeCode = 0x14; replacingKeys[1].Flags = RI_KEY_BREAK;
            replacingKeys[2].MakeCode = 0x12; replacingKeys[2].Flags = RI_KEY_MAKE;
            replacingKeys[3].MakeCode = 0x12; replacingKeys[3].Flags = RI_KEY_BREAK;
            replacingKeys[4].MakeCode = 0x1f; replacingKeys[4].Flags = RI_KEY_MAKE;
            replacingKeys[5].MakeCode = 0x1f; replacingKeys[5].Flags = RI_KEY_BREAK;
            replacingKeys[6].MakeCode = 0x14; replacingKeys[6].Flags = RI_KEY_MAKE;
            replacingKeys[7].MakeCode = 0x14; replacingKeys[7].Flags = RI_KEY_BREAK;

            MACROLIB_STATUS addStatus = macrolib_add_macro(macro, replacingKeys, 8);
            if (addStatus == MACROLIB_STATUS_SUCCESS) {
                std::cout << "Macro added successfully" << std::endl;
            }
            else {
                std::cout << "Error adding macro: " << addStatus << std::endl;
            }

            break;
        }
        case 2: {
            INPUT_KEYBOARD_MACRO macro;

            macro.DeviceID = 1;
            macro.ReplacedKeyScanCode = 0x1e;

            BOOLEAN result;
            MACROLIB_STATUS isStatus = macrolib_is_macro(macro, &result);
            if (isStatus == MACROLIB_STATUS_SUCCESS) {
                std::cout << (result ? "There is a macro on key 'A' device 1" : "There is no macro on key 'A' device 1") << std::endl;
            }
            else {
                std::cout << "Error while check if macro: " << isStatus << std::endl;
            }

            break;
        }
        case 3: {

            INPUT_KEYBOARD_MACRO macro;

            macro.DeviceID = 1;
            macro.ReplacedKeyScanCode = 0x1e;

            size_t macroLength = 0;

            MACROLIB_STATUS macLenStatus = macrolib_get_macro_length(macro, &macroLength);
            if (macLenStatus != MACROLIB_STATUS_SUCCESS) {
                std::cout << "Error while getting macro length: " << macLenStatus << std::endl;
                break;
            }

            std::cout << "Macro length: " << macroLength << std::endl;

            PINPUT_KEYBOARD_KEY keys = reinterpret_cast<PINPUT_KEYBOARD_KEY>(malloc(macroLength * sizeof(INPUT_KEYBOARD_KEY)));

            size_t macroActualLength = 0;
            MACROLIB_STATUS macStatus = macrolib_get_macro(macro, keys, macroLength, &macroActualLength);
            if (macStatus == MACROLIB_STATUS_SUCCESS) {
                std::cout << "Macro on key 'A' device 1 (length: " << macroActualLength << "): " << std::endl;

                for (int i = 0; i < macroActualLength; i++) {
                    std::cout << keys[i].MakeCode << "(" << (keys[i].Flags == RI_KEY_MAKE ? "KEY_DOWN" : (keys[i].Flags == RI_KEY_BREAK ? "KEY_UP" : "UNKNOWN")) << ") ";
                }
                std::cout << std::endl;
            }
            else {
                std::cout << "Failed to get macro: " << macStatus << std::endl;
            }

            free(keys);

            break;
        }
        case 4: {
            INPUT_KEYBOARD_MACRO macro;

            macro.DeviceID = 1;
            macro.ReplacedKeyScanCode = 0x1e;

            MACROLIB_STATUS deleteStatus = macrolib_delete_macro(macro);
            if (deleteStatus == MACROLIB_STATUS_SUCCESS) {
                std::cout << "Macro deleted successfully" << std::endl;
            }
            else {
                std::cout << "Error while deleting macro: " << deleteStatus << std::endl;
            }

            break;
        }
        default:
            break;
        }
    }

    return 0;
}


