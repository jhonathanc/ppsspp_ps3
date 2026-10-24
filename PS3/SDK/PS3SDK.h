// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int PS3SDKReserveAddress(size_t size, size_t alignment, uint32_t *address);
int PS3SDKReleaseAddress(uint32_t address);
int PS3SDKAllocateShared(size_t size, uint32_t *id);
int PS3SDKFreeShared(uint32_t id);
int PS3SDKMapShared(uint32_t address, uint32_t id);
int PS3SDKUnmapShared(uint32_t address, uint32_t *id);
int PS3SDKAllocatePages(size_t size, uint32_t *address);
int PS3SDKFreePages(uint32_t address);
uint64_t PS3SDKSystemTime(void);
void PS3SDKYield(void);
uint64_t PS3SDKCurrentThreadId(void);
void PS3SDKSetCurrentThreadName(const char *name);

#ifdef __cplusplus
}
#endif
