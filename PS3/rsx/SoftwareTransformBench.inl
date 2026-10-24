// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#if !PPSSPP_PLATFORM(PS3)
// Test builds: PPSSPP_NULL_BACKEND=1 installs a hardware backend that takes every draw and does
// nothing (=2: behind the GE worker thread), to time the emulation-side cost of a GE dump replay.
namespace {
class NullBackend : public Rasterizer::HardwareBackend {
public:
	bool SupportsTL() const override { return true; }
	bool DrawTL(const Rasterizer::RasterizerState &st, const Rasterizer::TLParams &tp, const Rasterizer::TLVertex *verts, int, const uint16_t *indices, int nidx, int cull) override {
		// Checksum of what would be drawn (for comparing merged and unmerged jobs).
		for (int i = 0; i < nidx; ++i) {
			const Rasterizer::TLVertex &v = verts[indices[i]];
			sum_ += v.x * 1.1 + v.y * 1.3 + v.z * 1.7 + v.u * 0.7 + v.v * 0.9 + v.c[0] + tp.mvp[0] + cull;
		}
		{
			// And of the state each index is drawn with (as the backend sees it).
			alignas(16) Rasterizer::RasterizerState n;
			memcpy((void *)&n, (const void *)&st, sizeof(n));
			Rasterizer::NormalizeForHardware(n);
			n.cpuEpoch = 0;
			n.texptr[0] = nullptr;  // Host address: differs between runs.
			uint32_t h = 2166136261u;
			const uint8_t *b = (const uint8_t *)&n;
			for (size_t i = 0; i < sizeof(n); ++i)
				h = (h ^ b[i]) * 16777619u;
			stateSum_ += (double)(h % 9973) * nidx;
		}
		idx_ += nidx;
		tlCalls_++;
		// PPSSPP_FETCHDUMP=dir: each texture as the RSX renderer's slow path decodes it (Sampler fetch).
		static const char *fetchDir = getenv("PPSSPP_FETCHDUMP");
		if (fetchDir && st.enableTextures && st.texptr[0]) {
			static std::set<uint32_t> seen;
			if (seen.insert(st.texaddr[0]).second) {
				Sampler::FetchFunc fetch = Sampler::GetFetchFunc(st.samplerID);
				const int w = st.samplerID.cached.sizes[0].w, h = st.samplerID.cached.sizes[0].h;
				char path[512];
				snprintf(path, sizeof(path), "%s/fetch_%08x_fmt%d.ppm", fetchDir, st.texaddr[0], (int)st.samplerID.TexFmt());
				FILE *f = fopen(path, "wb");
				if (f && fetch) {
					fprintf(f, "P6 %d %d 255\n", w, h);
					int alpha0 = 0;
					for (int y = 0; y < h; ++y)
						for (int x = 0; x < w; ++x) {
							const uint32_t rgba = Math3D::Vec4<int>(fetch(x, y, st.texptr[0], st.texbufw[0], 0, st.samplerID)).ToRGBA();
							const uint8_t px[3] = { (uint8_t)rgba, (uint8_t)(rgba >> 8), (uint8_t)(rgba >> 16) };
							if ((rgba >> 24) < 0x21) alpha0++;
							fwrite(px, 1, 3, f);
						}
					fprintf(stderr, "FETCH %08x fmt %d %dx%d: %d texels with alpha <= 0x20\n", st.texaddr[0], (int)st.samplerID.TexFmt(), w, h, alpha0);
					snprintf(path, sizeof(path), "%s/fetch_%08x_fmt%d_alpha.pgm", fetchDir, st.texaddr[0], (int)st.samplerID.TexFmt());
					if (FILE *fa = fopen(path, "wb")) {
						fprintf(fa, "P5 %d %d 255\n", w, h);
						for (int y = 0; y < h; ++y)
							for (int x = 0; x < w; ++x) {
								const uint32_t rgba = Math3D::Vec4<int>(fetch(x, y, st.texptr[0], st.texbufw[0], 0, st.samplerID)).ToRGBA();
								fputc((int)(rgba >> 24), fa);
							}
						fclose(fa);
					}
				}
				if (f) fclose(f);
			}
		}
		// Same test as the RSX renderer's ApplyState memo, to see how often it would hit.
		const uint8_t *clut = st.samplerID.cached.clut;
		Rasterizer::RasterizerState n;
		memcpy((void *)&n, &st, sizeof(n));
		Rasterizer::NormalizeForHardware(n);
		const uint8_t *a = (const uint8_t *)&n, *b = (const uint8_t *)&last_;
		bool same = memcmp(a, b, sizeof(n)) == 0 && (!clut || (hasClut_ && memcmp(clut, lastClut_, 1024) == 0));
		if (same) hits_++; else {
			misses_++;
			if (getenv("PPSSPP_MEMO_DIFF")) {
				for (size_t i = 0; i < sizeof(n); ++i) if (a[i] != b[i]) { fprintf(stderr, "MEMO: miss, first differing byte %d\n", (int)i); break; }
			}
		}
		memcpy((void *)&last_, &n, sizeof(n));
		hasClut_ = clut != nullptr;
		if (clut) memcpy(lastClut_, clut, 1024);
		if (((hits_ + misses_) & 4095) == 0) fprintf(stderr, "MEMO: %d hits %d misses\n", hits_, misses_);
		if (getenv("PPSSPP_TLSUM")) fprintf(stderr, "TLSUM: %d calls %lld indices sum %.6e state %.0f\n", tlCalls_, (long long)idx_, sum_, stateSum_);
		return true;
	}
	Rasterizer::RasterizerState last_{};
	uint8_t lastClut_[1024];
	bool hasClut_ = false;
	int hits_ = 0, misses_ = 0;
	double sum_ = 0.0;
	double stateSum_ = 0.0;
	long long idx_ = 0;
	int tlCalls_ = 0;
	bool DrawTriangles(const Rasterizer::RasterizerState &, const Rasterizer::FastVertex *, int, bool) override { return true; }
	bool DrawIndexed(const Rasterizer::RasterizerState &, const Rasterizer::FastVertex *, int, const uint16_t *, int, bool) override { return true; }
	void Triangle(const Rasterizer::RasterizerState &, const VertexData &, const VertexData &, const VertexData &) override {}
	void Rect(const Rasterizer::RasterizerState &st, const VertexData &v0, const VertexData &v1, bool clear) override {
		static const bool log = getenv("PPSSPP_RECTLOG") != nullptr;
		if (log)
			fprintf(stderr, "RECT #%u z %d tex %08x %s xy %.1f,%.1f - %.1f,%.1f st %.2f,%.2f - %.2f,%.2f clear %d\n", Rasterizer::g_drawSerial, (int)v1.screenpos.z, st.enableTextures ? st.texaddr[0] : 0, st.throughMode ? "thru" : "3d",
				v0.screenpos.x / 16.0f, v0.screenpos.y / 16.0f, v1.screenpos.x / 16.0f, v1.screenpos.y / 16.0f, v0.texturecoords.s(), v0.texturecoords.t(), v1.texturecoords.s(), v1.texturecoords.t(), clear ? 1 : 0);
	}
	void Line(const Rasterizer::RasterizerState &, const VertexData &, const VertexData &) override {}
	void Point(const Rasterizer::RasterizerState &, const VertexData &) override {}
	void Flush(const char *) override {}
	bool HasPendingWrite(uint32_t, uint32_t, uint32_t, uint32_t) override { return false; }
};
}

#ifdef PPSSPP_RSX_HOSTSIM
Rasterizer::HardwareBackend *HostSimRSXCreate(bool worker);
#endif

void InstallBenchBackendFromEnv() {
#ifdef PPSSPP_RSX_HOSTSIM
	if (const char *sim = getenv("PPSSPP_RSX_SIM")) {
		Rasterizer::g_hardwareBackend = HostSimRSXCreate(atoi(sim) == 2);
		return;
	}
#endif
	const char *v = getenv("PPSSPP_NULL_BACKEND");
	if (!v)
		return;
	static NullBackend nullBackend;
	fprintf(stderr, "BENCH: sizeof RasterizerState %d, GEState %d, VertexData %d, TLJob %d, TLParams %d\n", (int)sizeof(Rasterizer::RasterizerState), (int)sizeof(GEState), (int)sizeof(VertexData), (int)sizeof(TLJob), (int)sizeof(Rasterizer::TLParams));
	Rasterizer::g_hardwareBackend = atoi(v) == 2 ? Rasterizer::StartGEWorker(&nullBackend) : &nullBackend;
}
#endif
