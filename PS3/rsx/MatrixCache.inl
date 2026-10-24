// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

static void GECombineMatricesBits(uint32_t out[16], const uint32_t a[16], const uint32_t b[16]) {
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			GERowTerm terms[4];
			for (int k = 0; k < 4; ++k) {
				terms[k] = GEProductBits(a[r * 4 + k], b[k * 4 + c]);
			}
			out[r * 4 + c] = GERowSumBits(terms, 4);
		}
	}
}

// The last few products, by their inputs (build 155): a draw's transform state is recomputed for
// a lighting or other change with the same matrices, and the GE's exact products are slow.
static void GECombineMatricesBitsMemo(uint32_t out[16], const uint32_t a[16], const uint32_t b[16]) {
	struct Entry {
		uint32_t a[16], b[16], out[16];
		bool valid;
	};
	static Entry cache[8];
	static int next = 0;
	for (Entry &e : cache) {
		if (e.valid && memcmp(e.a, a, sizeof(e.a)) == 0 && memcmp(e.b, b, sizeof(e.b)) == 0) {
			memcpy(out, e.out, sizeof(e.out));
#if PPSSPP_ARCH(POWERPC)
			PS3Profile::xfMemoHits++;
#endif
			return;
		}
	}
	GECombineMatricesBits(out, a, b);
	Entry &e = cache[next];
	next = (next + 1) & 7;
	memcpy(e.a, a, sizeof(e.a));
	memcpy(e.b, b, sizeof(e.b));
	memcpy(e.out, out, sizeof(e.out));
	e.valid = true;
#if PPSSPP_ARCH(POWERPC)
	PS3Profile::xfMemoMisses++;
#endif
}

// A 4x3 matrix (as in gstate) as a 4x4 one in float bits, read straight from memory.
static void Matrix4x3To4x4Bits(uint32_t out[16], const float *m4x3) {
	uint32_t m[12];
	memcpy(m, m4x3, sizeof(m));
	out[0] = m[0]; out[1] = m[1]; out[2] = m[2]; out[3] = 0;
	out[4] = m[3]; out[5] = m[4]; out[6] = m[5]; out[7] = 0;
	out[8] = m[6]; out[9] = m[7]; out[10] = m[8]; out[11] = 0;
	out[12] = m[9]; out[13] = m[10]; out[14] = m[11]; out[15] = 0x3F800000;
}

