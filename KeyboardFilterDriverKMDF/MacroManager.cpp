#include "MacroManager.h"

PKB_MACRO_HASHVALUE _macros = NULL; // HashTable

INPUT_KEYBOARD_MACRO getMacroKeyZeroFilled(INPUT_KEYBOARD_MACRO macroKey) {
	INPUT_KEYBOARD_MACRO zeroFilledMacro;

	RtlZeroMemory(&zeroFilledMacro, sizeof(INPUT_KEYBOARD_MACRO));

	zeroFilledMacro.DeviceID = macroKey.DeviceID;
	zeroFilledMacro.ReplacedKeyScanCode = macroKey.ReplacedKeyScanCode;

	return zeroFilledMacro;
}

/// <summary>
/// Gets the macro for the specified macro key
/// </summary>
/// <param name="macroKey">
/// The macro key (keyboard/key combination) to check for
/// </param>
/// <returns>
/// A pointer to the KB_MACRO_HASHVALUE macro if there is one or NULL if no macro was found
/// </returns>
PKB_MACRO_HASHVALUE MacroManager::getMacro(INPUT_KEYBOARD_MACRO macroKey) {
	PKB_MACRO_HASHVALUE result;

	INPUT_KEYBOARD_MACRO key = getMacroKeyZeroFilled(macroKey);

#pragma warning(disable:4127)
	HASH_FIND(hh, _macros, &key, sizeof(INPUT_KEYBOARD_MACRO), result);
#pragma warning(default:4127)

	return result;
}

/// <summary>
/// Check if there is a macro on that keyboard/key combination
/// </summary>
/// <param name="macroKey">
/// The macro key (keyboard/key combination) to check for
/// </param>
/// <returns></returns>
BOOLEAN MacroManager::isMacro(INPUT_KEYBOARD_MACRO macroKey) {
	PKB_MACRO_HASHVALUE result = MacroManager::getMacro(macroKey);
	return result != NULL;
}

/// <summary>
/// Adds a macro to the macros hashtable
/// </summary>
/// <param name="macro">
/// A structure containing the DeviceID and the ScanCode of the key that would be replaced
/// </param>
/// <param name="keys">
/// A pointer to an array (the first element of it) of keys that would replace the macro key.
/// This method makes a copy of it, you can clear it from memory after calling the method.
/// </param>
/// <param name="keysSize">
/// The size of the keys array
/// </param>
BOOLEAN MacroManager::addMacro(INPUT_KEYBOARD_MACRO macroKey, PINPUT_KEYBOARD_KEY keys, size_t keysSize) {

	PKB_MACRO_HASHVALUE macro = MacroManager::getMacro(macroKey);

	//
	// If macro already exists, delete it
	//
	if (macro != NULL) {
		KdPrint(("[KB] Macro already exists, deleting it [%u, %u]\n", macroKey.DeviceID, macroKey.ReplacedKeyScanCode));
		MacroManager::deleteMacro(macroKey);
		macro = NULL;
	}

	// Allocate memory to store the replacing keys sequence
	PINPUT_KEYBOARD_KEY newKeysBuffer = (PINPUT_KEYBOARD_KEY) ExAllocatePoolWithTag(NonPagedPool, keysSize, KBFLTR_MM_TAG);
	
	if (newKeysBuffer == NULL) {
		KdPrint(("[KB] Failed to allocate memory for macro keys\n"));
		return FALSE;
	}

	// Allocate memory for the macro
	macro = (PKB_MACRO_HASHVALUE)ExAllocatePoolWithTag(NonPagedPool, keysSize, KBFLTR_MM_TAG);

	if (macro == NULL) {
		KdPrint(("[KB] Failed to allocate memory for macro\n"));
		return FALSE;
	}

	macro->OriginalKey = getMacroKeyZeroFilled(macroKey);;
	macro->NewKeys = newKeysBuffer;
	macro->NewKeysLength = keysSize / sizeof(INPUT_KEYBOARD_KEY);
	
	HASH_ADD(hh, _macros, OriginalKey, sizeof(INPUT_KEYBOARD_MACRO), macro);

	return TRUE;

}

void MacroManager::deleteMacro(INPUT_KEYBOARD_MACRO macroKey) {

	macroKey = getMacroKeyZeroFilled(macroKey);

	// If macro doesn't exist, then we don't need to delete it
	if (!MacroManager::isMacro(macroKey)) return;

	// Get the macro pointer
	PKB_MACRO_HASHVALUE macro = MacroManager::getMacro(macroKey);

	// Remove the macro from the hashtable
	HASH_DELETE(hh, _macros, macro);

	// Cleanup the macro's replacing keys memory
	ExFreePoolWithTag(macro->NewKeys, KBFLTR_MM_TAG);

	// Cleanup the macro memory
	ExFreePoolWithTag(macro, KBFLTR_MM_TAG);

}

