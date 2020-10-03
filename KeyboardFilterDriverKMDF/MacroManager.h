#pragma once
#include "Driver.h"

#define KBFLTR_MM_TAG 'Kmm'

/// <summary>
/// The object type of the hashtable elements.
/// </summary>
typedef struct _KB_MACRO_HASHVALUE {

	//
	// Original key that the macro is gonna replace
	//
	INPUT_KEYBOARD_MACRO OriginalKey;

	//
	// Array of keys that the macro will replace the original key with
	//
	PINPUT_KEYBOARD_KEY NewKeys;
	//
	// Number of elements in NewKeys
	//
	size_t NewKeysLength;

	//
	// Needed for UTHASH hashtable
	//
	UT_hash_handle hh;

} KB_MACRO_HASHVALUE, * PKB_MACRO_HASHVALUE;

namespace MacroManager {
	// extern PKB_MACRO_HASHVALUE _macros; // Macros hashtable

	PKB_MACRO_HASHVALUE getMacro(INPUT_KEYBOARD_MACRO macroKey);
	BOOLEAN isMacro(INPUT_KEYBOARD_MACRO macroKey);
	BOOLEAN addMacro(INPUT_KEYBOARD_MACRO macroKey, PINPUT_KEYBOARD_KEY keys, size_t keysSize);
	void deleteMacro(INPUT_KEYBOARD_MACRO macroKey);

}