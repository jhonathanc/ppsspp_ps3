#!/usr/bin/env python3
# Copyright (c) 2012- PPSSPP Project.
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Fix PSL1GHT timeout handling in the pinned pthread-embedded source."""
import pathlib
import sys

path = pathlib.Path(sys.argv[1]) / 'platform/psl1ght/psl1ght_osal.c'
with path.open(newline='') as stream:
	text = stream.read()
ending = '\r\n' if '\r\n' in text else '\n'
text = text.replace('\r\n', '\n')

def replace(old, new, count=1):
	global text
	if new in text:
		return
	if text.count(old) != count:
		raise RuntimeError('Unexpected pthread-embedded source: ' + old)
	text = text.replace(old, new)

# clock() is not a reliable elapsed-time clock in PSL1GHT. LV2 provides one
# directly, with the microsecond units required by the semaphore polling loop.
replace('clock_t start_time;', 'u64 start_time;')
replace('start_time = clock();', 'start_time = sysGetSystemTime();')
replace('// clock() is in microseconds, timeout as passed in was in milliseconds',
	'// LV2 time is in microseconds; the requested timeout is in milliseconds.')
replace('((clock() - start_time) > timeout)', '((sysGetSystemTime() - start_time) >= timeout)')
replace('timeout = *pTimeout * 1000;', 'timeout = (u64)*pTimeout * 1000;')
replace('timeoutUsecs = *pTimeoutMsecs * 1000;', 'timeoutUsecs = (u64)*pTimeoutMsecs * 1000;')
replace('u64 timeoutUsecs = timeoutMsecs*1000;', 'u64 timeoutUsecs = (u64)timeoutMsecs * 1000;')
replace('s32 status = sysMutexLock(handle, timeoutUsecs);',
	's32 status = timeoutMsecs == 0 ? sysMutexTryLock(handle) : sysMutexLock(handle, timeoutUsecs);')

# PTE uses a non-null zero timeout for polling (including timed-wait cleanup).
# LV2's blocking sysSemWait interprets zero as an infinite wait instead.
replace('result = sysSemWait(handle, timeoutUsecs);', '''if (pTimeoutMsecs != NULL && *pTimeoutMsecs == 0)
    result = sysSemTryWait(handle);
  else
    result = sysSemWait(handle, timeoutUsecs);''')
replace('else if (result == 0x8001000b)', '''else if (result == 0x8001000b
           || (pTimeoutMsecs != NULL && *pTimeoutMsecs == 0 && result == 0x8001000a))''')

# A runnable poller can starve its creator while the PPU is fully occupied.
# Block briefly so idle condition-variable workers release the processor.
replace('sysThreadYield ();', 'sysUsleep (1000);', count=2)

with path.open('w', newline='') as stream:
	stream.write(text.replace('\n', ending))
print('Prepared pthread-embedded: LV2 monotonic time and nonblocking zero timeouts')
