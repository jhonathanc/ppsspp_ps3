// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

#pragma once

// Pthread-embedded supplies its own POSIX types. Newlib's native pthread_t is
// an integer, while PTE uses a pointer and a reuse generation.

#include <sys/timespec.h>
#include <pthread.h>
