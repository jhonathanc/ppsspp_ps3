#pragma once
#include <cstdint>
using sys_ppu_thread_t = uint64_t;
int sysThreadYield();
int sysThreadGetId(sys_ppu_thread_t *);
int sysThreadRename(sys_ppu_thread_t, const char *);
