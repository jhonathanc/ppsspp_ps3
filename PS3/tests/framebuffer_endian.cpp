// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

#include <cassert>
#include <cstdio>
#include <vector>
#include "GPU/Software/SoftGpu.h"

int main() {
	std::vector<u8> memory(2 * 1024 * 1024);
	FormatBuffer color;
	color.data = memory.data();
	color.Set16(0, 0, 16, 0x1234);
	assert(memory[0] == 0x34 && memory[1] == 0x12);
	memory[0] = 0xCD;
	memory[1] = 0xAB;
	assert(color.Get16(0, 0, 16) == 0xABCD);
	color.Set32(1, 0, 16, 0x12345678);
	assert(memory[4] == 0x78 && memory[5] == 0x56 && memory[6] == 0x34 && memory[7] == 0x12);
	assert(color.Get32(1, 0, 16) == 0x12345678);

	DepthBuffer depth;
	depth.vram = memory.data();
	for (const auto &layouts : DEPTH_LAYOUTS) {
		for (const auto &layout : layouts) {
			depth.layout = layout;
			for (int x = 0; x < 32; ++x) {
				depth.Set16(x, 1, 64, 0x1234);
				u32 offset = layout.Stored((64 + x) * 2);
				assert(memory[offset] == 0x34 && memory[offset + 1] == 0x12);
				memory[offset] = 0xCD;
				memory[offset + 1] = 0xAB;
				assert(depth.Get16(x, 1, 64) == 0xABCD);
			}
		}
	}
	puts("Framebuffer byte order: color formats and ten depth layouts PASS");
}
