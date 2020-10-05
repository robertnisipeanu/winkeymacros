#include "operators.h"

void* operator new(size_t size) {
	auto p = ExAllocatePoolWithTag(NonPagedPool, size, KBFLTR_CLASS_TAG);
	if (p == NULL) {
		KdPrint(("Failed to allocate memory in new operator\n"));
	}
	return p;
}

void operator delete(void* p, unsigned __int64) {
	if (p == NULL) return;
	ExFreePoolWithTag(p, KBFLTR_CLASS_TAG);
}