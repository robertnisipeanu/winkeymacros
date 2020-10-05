#include "MacroManager.h"

MacroManager::MacroManager() {
	this->_macros = NULL;
}

MacroManager::~MacroManager() {
	this->clearAllMacros();
}

/// <summary>
/// Clears all the macros the current MacroManager instance has
/// </summary>
void MacroManager::clearAllMacros() {
	PKB_MACRO_HASHVALUE s, tmp;

	HASH_ITER(hh, this->_macros, s, tmp) {
		this->deleteMacroInternal(s);
	}
}

/// <summary>
/// Gets the macro for the specified key
/// </summary>
/// <param name="scanCode">
/// ScanCode of the key that contains the macro
/// </param>
/// <returns>
/// A pointer to the KB_MACRO_HASHVALUE macro if there is one orNULL if no macro was found
/// </returns>
PKB_MACRO_HASHVALUE MacroManager::getMacro(USHORT scanCode) {
	PKB_MACRO_HASHVALUE result;

#pragma warning(disable:4127)
	HASH_FIND(hh, this->_macros, &scanCode, sizeof(USHORT), result);
#pragma warning(default:4127)

	return result;
}

/// <summary>
/// Check if there is a macro on the specified key
/// </summary>
/// <param name="scanCode">
/// ScanCode of the key to check if there is a macro on
/// </param>
/// <returns>
/// TRUE if there is a macro on that key, otherwise FALSE
/// </returns>
BOOLEAN MacroManager::isMacro(USHORT scanCode) {
	PKB_MACRO_HASHVALUE result = this->getMacro(scanCode);
	return result != NULL;
}

/// <summary>
/// Adds a macro to the macros hashtable
/// </summary>
/// <param name="scanCode">
/// ScanCode of the key that would be replaced
/// </param>
/// <param name="keys">
/// A pointer to an array (the first element of it) of keys that would replace the macro key.
/// This method makes a copy of it, you can clear it from memory after calling the method.
/// </param>
/// <param name="keysSize">
/// The size of the keys array
/// </param>
/// <returns>
/// TRUE if macro allocations is successfull, otherwise FALSE
/// </returns>
BOOLEAN MacroManager::addMacro(USHORT scanCode, PINPUT_KEYBOARD_KEY keys, size_t keysSize) {

	//
	// If macro already exists, delete it
	//
	if (this->isMacro(scanCode)) {
		KdPrint(("[KB] Macro already exists, deleting it [%u]\n", scanCode));
		this->deleteMacro(scanCode);
	}

	// Allocate memory to store the replacing keys sequence
	PINPUT_KEYBOARD_KEY newKeysBuffer = (PINPUT_KEYBOARD_KEY)ExAllocatePoolWithTag(NonPagedPool, keysSize, KBFLTR_MM_TAG);

	if (newKeysBuffer == NULL) {
		KdPrint(("[KB] Failed to allocate memory for macro keys\n"));
		return FALSE;
	}

	// Copy keys into allocated memory
	RtlCopyMemory(newKeysBuffer, keys, keysSize);

	// Allocate memory for the macro
	PKB_MACRO_HASHVALUE macro = (PKB_MACRO_HASHVALUE)ExAllocatePoolWithTag(NonPagedPool, sizeof(KB_MACRO_HASHVALUE), KBFLTR_MM_TAG);

	if (macro == NULL) {
		ExFreePoolWithTag(newKeysBuffer, KBFLTR_MM_TAG);
		KdPrint(("[KB] Failed to allocate memory for macro\n"));
		return FALSE;
	}

	// Populate macro object with the data
	macro->OriginalKeyScanCode = scanCode;
	macro->NewKeys = newKeysBuffer;
	macro->NewKeysLength = keysSize / sizeof(INPUT_KEYBOARD_KEY);

	// Add macro object to macros hashtable
	HASH_ADD(hh, this->_macros, OriginalKeyScanCode, sizeof(USHORT), macro);

	return TRUE;
}

/// <summary>
/// Deletes a macro from the hashtable
/// </summary>
/// <param name="scanCode">
/// The scancode of the key that contains the macro
/// </param>
void MacroManager::deleteMacro(USHORT scanCode) {
	// If macro doesn't exist, then we don't need to delete it
	if (!this->isMacro(scanCode)) return;

	// Get the macro pointer
	PKB_MACRO_HASHVALUE macro = MacroManager::getMacro(scanCode);

	// Delete the macro from the hashtable
	this->deleteMacroInternal(macro);
}

/// <summary>
/// Deletes macro from hashtable and clears it's memory
/// </summary>
/// <param name="macro">
/// Pointer to the macro that should be deleted
/// </param>
void MacroManager::deleteMacroInternal(PKB_MACRO_HASHVALUE macro) {
	// Remove the macor from the hashtable
	HASH_DELETE(hh, this->_macros, macro);

	// Cleanup the macro's replacing keys memory
	ExFreePoolWithTag(macro->NewKeys, KBFLTR_MM_TAG);

	// Cleanup the macro memory
	ExFreePoolWithTag(macro, KBFLTR_MM_TAG);
}