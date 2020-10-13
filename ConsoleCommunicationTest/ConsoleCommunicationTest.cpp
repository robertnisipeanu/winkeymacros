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
        std::cout << "Failed to number of keyboards, closing program" << std::endl;
        return 1;
    }

    std::cout << "Number of keybords: " << numOfKeyb << std::endl;


    while (TRUE) {
        int number;
        scanf("%d", &number);
    }

    return 0;
}


