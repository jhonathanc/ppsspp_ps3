// Copyright (c) 2012- PPSSPP Project.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include <map>
#include <sys/mman.h>
#include <sys/memory.h>
#include <lv2/memory.h>
#include <unistd.h>
#include "Common/MemoryUtil.h"
static std::map<sys_mem_addr_t, size_t> pageAllocations;
static bool failPageAllocate;
int sysMemoryAllocate(size_t size, uint64_t flags, sys_mem_addr_t *address) {
	assert(flags == SYS_MEMORY_PAGE_SIZE_64K && size % 65536 == 0);
	if (failPageAllocate) return -1;
	void *p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	assert(p != MAP_FAILED);
	*address = reinterpret_cast<uintptr_t>(p);
	pageAllocations[*address] = size;
	return 0;
}
int sysMemoryFree(sys_mem_addr_t address) {
	assert(munmap(reinterpret_cast<void *>(static_cast<uintptr_t>(address)), pageAllocations.at(address)) == 0);
	pageAllocations.erase(address);
	return 0;
}
namespace {
struct Allocation { int fd; size_t size; };
std::map<sys_mem_id_t, Allocation> allocations;
std::map<sys_mem_addr_t, sys_mem_id_t> mappings;
sys_mem_id_t nextId = 1;
void *reservation = nullptr;
size_t reservationSize = 0;
bool failReserve = false, failAllocate = false, failMap = false;
bool failUnmap = false, failFree = false, failRelease = false;
}

int sysMMapperAllocateAddress(size_t size, uint64_t flags, size_t alignment, sys_mem_addr_t *address) {
	assert(flags == SYS_MEMORY_PAGE_SIZE_64K && alignment == size);
	if (failReserve) return -1;
	reservation = mmap(reinterpret_cast<void *>(0x30000000), size, PROT_NONE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
	assert(reservation != MAP_FAILED);
	reservationSize = size;
	*address = reinterpret_cast<uintptr_t>(reservation);
	return 0;
}
int sysMMapperFreeAddress(sys_mem_addr_t address) {
	if (failRelease) return -1;
	assert(mappings.empty() && allocations.empty());
	assert(address == reinterpret_cast<uintptr_t>(reservation));
	assert(munmap(reservation, reservationSize) == 0);
	reservation = nullptr;
	return 0;
}
int sysMMapperAllocateMemory(size_t size, uint64_t flags, sys_mem_id_t *id) {
	assert(flags == SYS_MEMORY_PAGE_SIZE_64K && size % 65536 == 0);
	if (failAllocate) return -1;
	int fd = memfd_create("psp-memory-test", 0);
	assert(fd >= 0 && ftruncate(fd, size) == 0);
	*id = nextId++;
	allocations[*id] = {fd, size};
	return 0;
}
int sysMMapperFreeMemory(sys_mem_id_t id) {
	if (failFree) return -1;
	for (const auto &view : mappings) assert(view.second != id);
	assert(close(allocations.at(id).fd) == 0);
	allocations.erase(id);
	return 0;
}
int sysMMapperMapMemory(sys_mem_addr_t address, sys_mem_id_t id, uint64_t flags) {
	assert(flags == SYS_MEMORY_PROT_READ_WRITE);
	if (failMap) return -1;
	if (mappings.count(address)) return -1;
	const auto &allocation = allocations.at(id);
	void *p = mmap(reinterpret_cast<void *>(static_cast<uintptr_t>(address)), allocation.size,
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, allocation.fd, 0);
	assert(reinterpret_cast<uintptr_t>(p) == address);
	mappings[address] = id;
	return 0;
}
int sysMMapperUnmapMemory(sys_mem_addr_t address, sys_mem_id_t *id) {
	if (failUnmap) return -1;
	*id = mappings.at(address);
	size_t size = allocations.at(*id).size;
	void *p = mmap(reinterpret_cast<void *>(static_cast<uintptr_t>(address)), size,
		PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	assert(reinterpret_cast<uintptr_t>(p) == address);
	mappings.erase(address);
	return 0;
}

int main() {
	assert(GetMemoryProtectPageSize() == 65536);
	assert(!AllocateMemoryPages(0, MEM_PROT_READ | MEM_PROT_WRITE));
	assert(!AllocateMemoryPages(SIZE_MAX, MEM_PROT_READ | MEM_PROT_WRITE));
	assert(!AllocateMemoryPages(65536, MEM_PROT_READ | MEM_PROT_EXEC));
	assert(!AllocateExecutableMemory(65536));
	failPageAllocate = true;
	assert(!AllocateMemoryPages(1, MEM_PROT_READ | MEM_PROT_WRITE));
	failPageAllocate = false;
	auto *p = static_cast<unsigned char *>(AllocateMemoryPages(65537, MEM_PROT_READ | MEM_PROT_WRITE));
	assert(p && pageAllocations.begin()->second == 131072);
	p[0] = 12; p[65536] = 34;
	assert(!ProtectMemoryPages(p, 1, MEM_PROT_READ));
	assert(!ProtectMemoryPages(p, 1, MEM_PROT_READ | MEM_PROT_EXEC));
	FreeMemoryPages(p, 65537);
	FreeMemoryPages(nullptr, 0);
	assert(pageAllocations.empty());
	assert(!AllocateAlignedMemory(100, 3));
	assert(!AllocateAlignedMemory(0, 64));
	void *aligned = AllocateAlignedMemory(100, 64);
	assert(aligned && reinterpret_cast<uintptr_t>(aligned) % 64 == 0);
	FreeAlignedMemory(aligned);
	void *reserved = ReserveMemoryPages(256 * 1024 * 1024);
	assert(reserved && allocations.empty());
	auto *code = static_cast<unsigned char *>(reserved) + 0x08000000;
	failAllocate = true;
	assert(!CommitMemoryPages(code, 65536));
	failAllocate = false;
	failMap = true;
	assert(!CommitMemoryPages(code, 65536));
	assert(allocations.size() == 1);
	failMap = false;
	assert(CommitMemoryPages(code, 65536));
	assert(code[0] == 0 && code[65535] == 0);
	code[0] = 17;
	assert(CommitMemoryPages(code + 1, 131071));
	assert(code[0] == 17 && code[65536] == 0 && allocations.size() == 2);
	assert(!CommitMemoryPages(reserved, SIZE_MAX));
	assert(!CommitMemoryPages(code, 0));
	failUnmap = true;
	FreeMemoryPages(reserved, 0);
	assert(mappings.size() == 2);
	failUnmap = false;
	failFree = true;
	FreeMemoryPages(reserved, 0);
	assert(mappings.empty() && allocations.size() == 2);
	failFree = false;
	failRelease = true;
	FreeMemoryPages(reserved, 0);
	assert(allocations.empty() && reservation);
	failRelease = false;
	FreeMemoryPages(reserved, 0);
	assert(!reservation);
	puts("PS3 page adapter: rounding, permissions, failure, writes and free PASS (host syscall model)");
}
