#pragma once
#include "Driver.h"

#define KBFLTR_CLASS_TAG 'Kbcl'

void* operator new(size_t size);
void operator delete(void* p, unsigned __int64);