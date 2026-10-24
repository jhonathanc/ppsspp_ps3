// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

#include <stddef.h>
#include "enet.h"

// The upstream ENet backend requires IPv6 socket APIs absent from PSL1GHT.
// TCP remains available; requesting ENet must fail explicitly.
int init_enet(void) { return -1; }
void deinit_enet(void) {}
int pump_enet(void) { return 1; }
void *connect_enet(const struct aemu_postoffice_sock_addr *addr4,
	const struct aemu_postoffice_sock6_addr *addr6, int channels,
	bool (*abort_connection)(void *), void *abort_arg) {
	return NULL;
}
void close_enet(void *handle) {}
int recv_enet(void *handle, char *buf, int size, int channel) { return -1; }
int peek_enet(void *handle, char *buf, int size, int channel) { return -1; }
int send_enet(void *handle, const char *buf, int size, int channel, bool reliable) { return -1; }
bool is_disconnected_enet(void *handle) { return true; }
