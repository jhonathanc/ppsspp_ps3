// Copyright (c) 2012- PPSSPP Project.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "ppsspp_config.h"
#if PPSSPP_PLATFORM(PS3)
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <map>
#include <mutex>
#include "PS3/SDK/PS3SDK.h"
#include "Common/MemoryUtil.h"
#include "Common/Log.h"

int GetMemoryProtectPageSize() { return 64 * 1024; }
bool PlatformIsWXExclusive() { return true; }

namespace {
constexpr size_t PAGE_SIZE = 64 * 1024;
constexpr size_t RESERVE_ALIGNMENT = 256 * 1024 * 1024;
struct Mapping {
	size_t size;
	uint32_t id;
	bool mapped = false;
};
struct Reservation {
	size_t size;
	std::map<uintptr_t, Mapping> mappings;
};
std::map<uintptr_t, Reservation> reservations;
std::mutex reservationMutex;
}

void *ReserveMemoryPages(size_t size) {
	if (!size || size > SIZE_MAX - (RESERVE_ALIGNMENT - 1)) {
		return nullptr;
	}
	const size_t rounded = (size + RESERVE_ALIGNMENT - 1) & ~(RESERVE_ALIGNMENT - 1);
	uint32_t address;
	int result = PS3SDKReserveAddress(rounded, RESERVE_ALIGNMENT, &address);
	if (result) {
		ERROR_LOG(Log::MemMap, "PS3 reserve memory pages failed: %08x", result);
		return nullptr;
	}
	std::lock_guard<std::mutex> guard(reservationMutex);
	reservations.emplace(address, Reservation{size, {}});
	return reinterpret_cast<void *>(static_cast<uintptr_t>(address));
}

bool CommitMemoryPages(void *ptr, size_t size) {
	if (!ptr || !size) {
		return false;
	}
	std::lock_guard<std::mutex> guard(reservationMutex);
	uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
	auto reservation = reservations.upper_bound(address);
	if (reservation == reservations.begin()) {
		return false;
	}
	--reservation;
	size_t offset = address - reservation->first;
	if (offset >= reservation->second.size || size > reservation->second.size - offset) {
		return false;
	}
	const uintptr_t end = (address + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	address &= ~(PAGE_SIZE - 1);
	auto &mappings = reservation->second.mappings;
	while (address < end) {
		auto next = mappings.upper_bound(address);
		auto current = next;
		if (current != mappings.begin()) {
			--current;
		}
		if (current == mappings.end() || current->first > address || address >= current->first + current->second.size) {
			size_t bytes = (next == mappings.end() ? end : std::min(end, next->first)) - address;
			uint32_t id;
			int result = PS3SDKAllocateShared(bytes, &id);
			if (result) {
				ERROR_LOG(Log::MemMap, "PS3 commit allocation failed: %08x", result);
				return false;
			}
			// Keep failed mappings for retry or cleanup, including failed frees.
			current = mappings.emplace(address, Mapping{bytes, id}).first;
		}
		Mapping &mapping = current->second;
		if (!mapping.mapped) {
			int result = PS3SDKMapShared(static_cast<uint32_t>(current->first), mapping.id);
			if (result) {
				ERROR_LOG(Log::MemMap, "PS3 commit mapping failed: %08x", result);
				return false;
			}
			mapping.mapped = true;
			memset(reinterpret_cast<void *>(current->first), 0, mapping.size);
		}
		address = current->first + mapping.size;
	}
	return true;
}

void *AllocateExecutableMemory(size_t size) {
	ERROR_LOG(Log::MemMap, "PS3 executable memory is unsupported; select the IR interpreter");
	return nullptr;
}
void FreeExecutableMemory(void *ptr, size_t size) {
	if (ptr)
		ERROR_LOG(Log::MemMap, "Unexpected PS3 executable allocation");
}

void *AllocateMemoryPages(size_t size, uint32_t flags) {
	constexpr size_t PAGE_SIZE = 64 * 1024;
	if (!size || size > SIZE_MAX - (PAGE_SIZE - 1) || flags != (MEM_PROT_READ | MEM_PROT_WRITE))
		return nullptr;
	uint32_t address;
	int result = PS3SDKAllocatePages((size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1), &address);
	if (result) {
		ERROR_LOG(Log::MemMap, "PS3 allocate memory pages failed: %08x", result);
		return nullptr;
	}
	return reinterpret_cast<void *>(static_cast<uintptr_t>(address));
}

bool ProtectMemoryPages(const void *ptr, size_t size, uint32_t flags) {
	// These adapters have no page-protection transition API. Never report
	// success for a permission change that has not actually been performed.
	return false;
}

void FreeMemoryPages(void *ptr, size_t size) {
	if (!ptr)
		return;
	std::lock_guard<std::mutex> guard(reservationMutex);
	uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
	auto reservation = reservations.find(address);
	if (reservation != reservations.end()) {
		auto &mappings = reservation->second.mappings;
		for (auto i = mappings.begin(); i != mappings.end();) {
			Mapping &mapping = i->second;
			uint32_t id;
			if (mapping.mapped && PS3SDKUnmapShared(static_cast<uint32_t>(i->first), &id)) {
				ERROR_LOG(Log::MemMap, "PS3 unmap committed pages failed");
				++i;
				continue;
			}
			mapping.mapped = false;
			if (PS3SDKFreeShared(mapping.id)) {
				ERROR_LOG(Log::MemMap, "PS3 free committed pages failed");
				++i;
				continue;
			}
			i = mappings.erase(i);
		}
		if (mappings.empty() && !PS3SDKReleaseAddress(static_cast<uint32_t>(address))) {
			reservations.erase(reservation);
		}
		return;
	}
	int result = PS3SDKFreePages(static_cast<uint32_t>(address));
	if (result)
		ERROR_LOG(Log::MemMap, "PS3 free memory pages failed: %08x", result);
}

void *AllocateAlignedMemory(size_t size, size_t alignment) {
	if (!size || alignment < sizeof(void *) || (alignment & (alignment - 1)))
		return nullptr;
	return memalign(alignment, size);
}
void FreeAlignedMemory(void *ptr) { free(ptr); }
#endif
