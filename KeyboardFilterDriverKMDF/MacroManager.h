#pragma once
#include "Driver.h"

#define KBFLTR_MM_TAG 'Kmm'

typedef struct _KB_MACRO_HASHVALUE {
	
	//
	// Original key that the macro is gonna replace
	//
	USHORT OriginalKeyScanCode;

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
} KB_MACRO_HASHVALUE, *PKB_MACRO_HASHVALUE;

class MacroManager {
public:
	MacroManager();
	~MacroManager();
	BOOLEAN isMacro(USHORT scanCode);
	PKB_MACRO_HASHVALUE getMacro(USHORT scanCode);
	BOOLEAN addMacro(USHORT scanCode, PINPUT_KEYBOARD_KEY keys, size_t keysSize);
	void deleteMacro(USHORT scanCode);
	void clearAllMacros();
private:
	void deleteMacroInternal(PKB_MACRO_HASHVALUE macro);
	PKB_MACRO_HASHVALUE _macros;
};