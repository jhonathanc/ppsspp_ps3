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

// Copies voices to and from the self-contained records of SasMixKernel.h (mixed on an SPU on the PS3).
struct SasKernelBridge {
	// False if the voice needs SasInstance::MixVoice (ATRAC3, or memory the kernel can't vouch for).
	static bool Export(const SasVoice &voice, SasKVoice &k) {
		memset(&k, 0, sizeof(k));
		switch (voice.type) {
		case VOICETYPE_VAG: {
			const VagDecoder &vag = voice.vag;
			if (!vag.end_) {
				const u32 bytes = (u32)vag.numBlocks_ * 16;
				if (vag.numBlocks_ < 0 || !Memory::IsValidRange(vag.read_, bytes) || !Memory::IsValidRange(vag.data_, bytes) || vag.read_ < vag.data_)
					return false;
				if (vag.curSample < 0 || vag.curSample > 28 || (vag.loopAtNextBlock_ && vag.loopStartBlock_ < -1))
					return false;
				k.dataEa = (uint64_t)(uintptr_t)Memory::GetPointerUnchecked(vag.data_);
				k.dataBytes = bytes;
			}
			k.type = SASK_TYPE_VAG;
			k.vagCurSample = vag.curSample;
			k.vagReadOff = (int32_t)(vag.read_ - vag.data_);
			k.vagCurBlock = vag.curBlock_;
			k.vagLoopStartBlock = vag.loopStartBlock_;
			k.vagNumBlocks = vag.numBlocks_;
			k.vagS1 = vag.s_1;
			k.vagS2 = vag.s_2;
			memcpy(k.vagSamples, vag.samples, sizeof(k.vagSamples));
			k.vagLoopEnabled = vag.loopEnabled_;
			k.vagLoopAtNextBlock = vag.loopAtNextBlock_;
			k.vagEnd = vag.end_;
			break;
		}
		case VOICETYPE_PCM:
			if (voice.pcmSize <= 0 || voice.pcmIndex < 0 || voice.pcmIndex > voice.pcmSize)
				return false;
			if (voice.loop && (voice.pcmLoopPos < 0 || voice.pcmLoopPos >= voice.pcmSize))
				return false;
			if (!Memory::IsValidRange(voice.pcmAddr, (u32)voice.pcmSize * 2))
				return false;
			k.type = SASK_TYPE_PCM;
			k.dataEa = (uint64_t)(uintptr_t)Memory::GetPointerUnchecked(voice.pcmAddr);
			k.dataBytes = (u32)voice.pcmSize * 2;
			k.pcmSize = voice.pcmSize;
			k.pcmIndex = voice.pcmIndex;
			k.pcmLoopPos = voice.pcmLoopPos;
			break;
		case VOICETYPE_ATRAC3:
			return false;
		default:
			k.type = 0;  // Silence, but the envelope runs.
			break;
		}
		k.pitch = voice.pitch;
		k.sampleFrac = voice.sampleFrac;
		k.volumeLeft = voice.volumeLeft;
		k.volumeRight = voice.volumeRight;
		k.effectLeft = voice.effectLeft;
		k.effectRight = voice.effectRight;
		k.resampleHist[0] = voice.resampleHist[0];
		k.resampleHist[1] = voice.resampleHist[1];
		k.loop = voice.loop;
		k.on = voice.on;
		k.playing = voice.playing;
		const ADSREnvelope &e = voice.envelope;
		k.height = e.height_;
		k.envState = (int32_t)e.state_;
		k.attackRate = e.attackRate;
		k.decayRate = e.decayRate;
		k.sustainRate = e.sustainRate;
		k.sustainLevel = e.sustainLevel;
		k.releaseRate = e.releaseRate;
		k.attackType = e.attackType;
		k.decayType = e.decayType;
		k.sustainType = e.sustainType;
		k.releaseType = e.releaseType;
		return true;
	}

	static void Import(const SasKVoice &k, SasVoice &voice) {
		if (voice.type == VOICETYPE_VAG) {
			VagDecoder &vag = voice.vag;
			vag.curSample = k.vagCurSample;
			vag.read_ = vag.data_ + (u32)k.vagReadOff;
			vag.curBlock_ = k.vagCurBlock;
			vag.loopStartBlock_ = k.vagLoopStartBlock;
			vag.s_1 = k.vagS1;
			vag.s_2 = k.vagS2;
			memcpy(vag.samples, k.vagSamples, sizeof(vag.samples));
			vag.loopAtNextBlock_ = k.vagLoopAtNextBlock != 0;
			vag.end_ = k.vagEnd != 0;
		} else if (voice.type == VOICETYPE_PCM) {
			voice.pcmIndex = k.pcmIndex;
		}
		voice.sampleFrac = k.sampleFrac;
		voice.resampleHist[0] = k.resampleHist[0];
		voice.resampleHist[1] = k.resampleHist[1];
		voice.on = k.on != 0;
		voice.playing = k.playing != 0;
		voice.envelope.height_ = k.height;
		voice.envelope.state_ = (ADSREnvelope::ADSRState)k.envState;
	}
};

static SasKVoice g_kernelVoices[PSP_SAS_VOICES_MAX] __attribute__((aligned(128)));

#if defined(PPSSPP_NO_GL) && !PPSSPP_PLATFORM(PS3)
// Test build (PPSSPP_SAS_FUZZ=n): mixes n random voices with MixVoice and with the kernel and
// compares every grain. Uses guest memory at 0x08400000 (the volatile area) for sample data.
static void SasKernelFuzz(SasInstance *sas, int scenarios) {
	uint32_t rng = 12345;
	auto rnd = [&](uint32_t n) { rng = rng * 1103515245u + 12345u; return (uint32_t)((rng >> 8) % n); };
	const u32 base = 0x08400000;
	int bad = 0;
	long grains = 0;
	const int savedGrain = sas->GetGrainSize();
	for (int sc = 0; sc < scenarios; ++sc) {
		const int grain = 64 * (1 + rnd(16));
		sas->SetGrainSize(grain);
		// Sample data: VAG blocks or PCM.
		const bool pcm = rnd(3) == 0;
		const int blocks = 1 + rnd(200);
		const u32 addr = base + 16 * rnd(1000);
		u8 *mem = Memory::GetPointerWriteUnchecked(addr);
		for (int b = 0; b < blocks; ++b) {
			u8 *blk = mem + b * 16;
			for (int i = 0; i < 16; ++i) blk[i] = (u8)rnd(256);
			blk[0] = (u8)((rnd(16) << 4) | rnd(16));
			const uint32_t fl = rnd(40);
			blk[1] = fl == 0 ? 6 : fl == 1 ? 3 : fl == 2 ? 7 : (u8)rnd(3) == 0 ? (u8)rnd(8) : 0;
		}
		SasVoice a, b;
		for (SasVoice *v : { &a, &b }) {
			uint32_t save = rng;
			v->type = pcm ? VOICETYPE_PCM : (rnd(10) == 0 ? VOICETYPE_NOISE : VOICETYPE_VAG);
			v->vagAddr = addr; v->vagSize = blocks * 16;
			v->pcmAddr = addr; v->pcmSize = blocks * 8; v->pcmIndex = 0;
			v->loop = rnd(2) != 0;
			v->pcmLoopPos = rnd(blocks * 8);
			v->pitch = rnd(4) == 0 ? 0x1000 : (int)rnd(0x4001);
			v->volumeLeft = (int)rnd(0x2001) - 0x1000; v->volumeRight = (int)rnd(0x2001) - 0x1000;
			v->effectLeft = (int)rnd(0x2001) - 0x1000; v->effectRight = (int)rnd(0x2001) - 0x1000;
			if (rnd(2))
				v->envelope.SetSimpleEnvelope(rnd(0x10000), rnd(0x10000));
			v->envelope.SetEnvelope(15, rnd(6), rnd(6), rnd(6), rnd(6));
			const int rates[] = { 0, 1, 0x100, 0x10000, 0x1000000, 0x7FFFFFFF, (int)rnd(0x80000000u) };
			v->envelope.SetRate(15, rates[rnd(7)], rates[rnd(7)], rates[rnd(7)], rates[rnd(7)]);
			v->envelope.SetSustainLevel(rnd(0x40000001));
			v->KeyOn();
			if (v == &b) {
				// Same draws as for a.
			} else {
				rng = save;
			}
		}
		for (int g = 0; g < 40 && (a.playing || b.playing); ++g) {
			if (rnd(15) == 0) { a.KeyOff(); b.KeyOff(); }
			std::vector<int32_t> om(grain * 2), os(grain * 2), km(grain * 2), ks(grain * 2);
			memset(sas->mixBuffer, 0, grain * 8);
			memset(sas->sendBuffer, 0, grain * 8);
			if (a.playing) sas->MixVoice(a);
			memcpy(om.data(), sas->mixBuffer, grain * 8);
			memcpy(os.data(), sas->sendBuffer, grain * 8);
			memset(sas->mixBuffer, 0, grain * 8);
			memset(sas->sendBuffer, 0, grain * 8);
			static SasKVoice k __attribute__((aligned(128)));
			if (b.playing && SasKernelBridge::Export(b, k)) {
				SasKernelMix(&k, 1, grain, km.data(), ks.data());
				SasKernelBridge::Import(k, b);
			}
			++grains;
			static SasKVoice ea __attribute__((aligned(128))), eb __attribute__((aligned(128)));
			SasKernelBridge::Export(a, ea);
			SasKernelBridge::Export(b, eb);
			if (om != km || os != ks || memcmp(&ea, &eb, sizeof(ea)) != 0 || a.playing != b.playing) {
				if (bad++ < 10)
					printf("SAS FUZZ: scenario %d grain %d type %d pitch %x differs (mix %d send %d state %d)\n", sc, g, (int)a.type, a.pitch,
						om != km, os != ks, memcmp(&ea, &eb, sizeof(ea)) != 0);
				break;
			}
		}
	}
	printf("SAS FUZZ: %d scenarios, %ld grains, %d differ\n", scenarios, grains, bad);
	sas->SetGrainSize(savedGrain);
}
#endif

bool g_sasMixOnPpu = false;

void SasInstance::MixVoices() {
#if PPSSPP_ARCH(POWERPC)
	const bool useKernel = !g_sasMixOnPpu;
#else
	const bool useKernel = false;
#endif
	const bool debug = SoundDebugOn();
	int playing = 0;
#if defined(PPSSPP_NO_GL) && !PPSSPP_PLATFORM(PS3)
	static bool fuzzed = false;
	if (!fuzzed && getenv("PPSSPP_SAS_FUZZ")) {
		fuzzed = true;
		SasKernelFuzz(this, atoi(getenv("PPSSPP_SAS_FUZZ")));
	}
#endif
	int kernelIndex[PSP_SAS_VOICES_MAX];
	int count = 0;
	for (int v = 0; v < PSP_SAS_VOICES_MAX; v++) {
		SasVoice &voice = voices[v];
		if (!voice.playing || voice.paused)
			continue;
		// Exactly the voices MixVoice would skip.
		if ((voice.type == VOICETYPE_VAG && !voice.vagAddr) || (voice.type == VOICETYPE_PCM && !voice.pcmAddr))
			continue;
		if (debug) {
			SoundDebugStats::Voice &d = g_soundDebug.voice[v];
			d.mixes++;
			d.type = (int)voice.type;
			d.volL = std::max(d.volL, voice.volumeLeft);
			d.volR = std::max(d.volR, voice.volumeRight);
			d.effL = std::max(d.effL, voice.effectLeft);
			d.effR = std::max(d.effR, voice.effectRight);
			d.env = std::max(d.env, voice.envelope.GetHeight());
			d.pitch = voice.pitch;
			playing++;
		}
		if (useKernel && SasKernelBridge::Export(voice, g_kernelVoices[count])) {
#if defined(PPSSPP_NO_GL) && !PPSSPP_PLATFORM(PS3)
			// Test build: mix with both and compare (PPSSPP_SAS_COMPARE=1).
			static const bool compare = getenv("PPSSPP_SAS_COMPARE") != nullptr;
			if (compare) {
				static SasKVoice k __attribute__((aligned(128)));
				k = g_kernelVoices[count];
				std::vector<int32_t> km(grainSize * 2), ks(grainSize * 2), om(grainSize * 2), os(grainSize * 2);
				SasKernelMix(&k, 1, grainSize, km.data(), ks.data());
				std::vector<int32_t> savedMix(mixBuffer, mixBuffer + grainSize * 2), savedSend(sendBuffer, sendBuffer + grainSize * 2);
				memset(mixBuffer, 0, grainSize * 8);
				memset(sendBuffer, 0, grainSize * 8);
				MixVoice(voice);
				memcpy(om.data(), mixBuffer, grainSize * 8);
				memcpy(os.data(), sendBuffer, grainSize * 8);
				for (int i = 0; i < grainSize * 2; ++i) {
					mixBuffer[i] = savedMix[i] + om[i];
					sendBuffer[i] = savedSend[i] + os[i];
				}
				static SasKVoice after __attribute__((aligned(128)));
				SasKernelBridge::Export(voice, after);
				static int reports = 0;
				const bool sameState = after.sampleFrac == k.sampleFrac && after.height == k.height && after.envState == k.envState &&
					after.vagReadOff == k.vagReadOff && after.vagCurSample == k.vagCurSample && after.vagCurBlock == k.vagCurBlock &&
					after.vagEnd == k.vagEnd && after.pcmIndex == k.pcmIndex && after.playing == k.playing && after.on == k.on &&
					after.resampleHist[0] == k.resampleHist[0] && after.resampleHist[1] == k.resampleHist[1] &&
					after.vagS1 == k.vagS1 && after.vagS2 == k.vagS2;
				static long compared = 0;
				++compared;
				if ((om != km || os != ks || !sameState) && reports++ < 20)
					printf("SAS COMPARE: voice %d type %d differs (mix %d, send %d, state %d) frac %u/%u height %lld/%lld state %d/%d read %d/%d\n", v, (int)voice.type,
						om != km, os != ks, !sameState, after.sampleFrac, k.sampleFrac, (long long)after.height, (long long)k.height, after.envState, k.envState, after.vagReadOff, k.vagReadOff);
				if ((compared & 1023) == 0 || compared == 1)
					printf("SAS COMPARE: %ld voice mixes compared, %d differ\n", compared, reports);
				continue;
			}
#endif
			kernelIndex[count++] = v;
		} else {
			MixVoice(voice);
		}
	}
	if (debug)
		g_soundDebug.sasVoicesMax = std::max(g_soundDebug.sasVoicesMax, playing);
	if (count == 0)
		return;
	SasKernelMix(g_kernelVoices, count, grainSize, mixBuffer, sendBuffer);
	for (int i = 0; i < count; ++i)
		SasKernelBridge::Import(g_kernelVoices[i], voices[kernelIndex[i]]);
}
