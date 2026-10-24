// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

#if defined(__PS3__) && defined(__PSL1GHT__)

#include <lv2/memory.h>
#include <sys/memory.h>
#include <sys/systime.h>
#include <sys/thread.h>
#include "PS3/SDK/PS3SDK.h"

int PS3SDKReserveAddress(size_t size, size_t alignment, uint32_t *address) {
	return sysMMapperAllocateAddress(size, SYS_MEMORY_PAGE_SIZE_64K, alignment, address);
}
int PS3SDKReleaseAddress(uint32_t address) { return sysMMapperFreeAddress(address); }
int PS3SDKAllocateShared(size_t size, uint32_t *id) {
	return sysMMapperAllocateMemory(size, SYS_MEMORY_PAGE_SIZE_64K, id);
}
int PS3SDKFreeShared(uint32_t id) { return sysMMapperFreeMemory(id); }
int PS3SDKMapShared(uint32_t address, uint32_t id) {
	return sysMMapperMapMemory(address, id, SYS_MEMORY_PROT_READ_WRITE);
}
int PS3SDKUnmapShared(uint32_t address, uint32_t *id) { return sysMMapperUnmapMemory(address, id); }
int PS3SDKAllocatePages(size_t size, uint32_t *address) {
	return sysMemoryAllocate(size, SYS_MEMORY_PAGE_SIZE_64K, address);
}
int PS3SDKFreePages(uint32_t address) { return sysMemoryFree(address); }
uint64_t PS3SDKSystemTime(void) { return sysGetSystemTime(); }
void PS3SDKYield(void) { sysThreadYield(); }
uint64_t PS3SDKCurrentThreadId(void) {
	sys_ppu_thread_t id;
	return sysThreadGetId(&id) == 0 ? id : 0;
}
void PS3SDKSetCurrentThreadName(const char *name) {
	sysThreadRename(PS3SDKCurrentThreadId(), name);
}

#endif
