// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

#if defined(__PS3__) && defined(__PS3_SONY_SDK__)

#include <sys/memory.h>
#include <sys/ppu_thread.h>
#include <sys/sys_time.h>
#include "PS3/SDK/PS3SDK.h"

int PS3SDKReserveAddress(size_t size, size_t alignment, uint32_t *address) {
	sys_addr_t nativeAddress = 0;
	int result = sys_mmapper_allocate_address(size, SYS_MEMORY_PAGE_SIZE_64K, alignment, &nativeAddress);
	if (result == 0) {
		*address = (uint32_t)nativeAddress;
	}
	return result;
}
int PS3SDKReleaseAddress(uint32_t address) { return sys_mmapper_free_address(address); }
int PS3SDKAllocateShared(size_t size, uint32_t *id) {
	return sys_mmapper_allocate_memory(size, SYS_MEMORY_PAGE_SIZE_64K, id);
}
int PS3SDKFreeShared(uint32_t id) { return sys_mmapper_free_memory(id); }
int PS3SDKMapShared(uint32_t address, uint32_t id) {
	return sys_mmapper_map_memory(address, id, SYS_MEMORY_PROT_READ_WRITE);
}
int PS3SDKUnmapShared(uint32_t address, uint32_t *id) { return sys_mmapper_unmap_memory(address, id); }
int PS3SDKAllocatePages(size_t size, uint32_t *address) {
	sys_addr_t nativeAddress = 0;
	int result = sys_memory_allocate(size, SYS_MEMORY_PAGE_SIZE_64K, &nativeAddress);
	if (result == 0) {
		*address = (uint32_t)nativeAddress;
	}
	return result;
}
int PS3SDKFreePages(uint32_t address) { return sys_memory_free(address); }
uint64_t PS3SDKSystemTime(void) { return sys_time_get_system_time(); }
void PS3SDKYield(void) { sys_ppu_thread_yield(); }
uint64_t PS3SDKCurrentThreadId(void) {
	sys_ppu_thread_t id;
	return sys_ppu_thread_get_id(&id) == 0 ? id : 0;
}
void PS3SDKSetCurrentThreadName(const char *name) {
	sys_ppu_thread_rename(PS3SDKCurrentThreadId(), name);
}

#endif
