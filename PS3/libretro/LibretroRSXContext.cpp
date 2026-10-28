// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include "PS3/libretro/LibretroRSXContext.h"
#include "PS3/rsx/RSXCore.h"
#include "PS3/rsx/RSXRenderer.h"
#include "PS3/rsx/TexSpu.h"
#include "PS3/frontend/Frontend.h"
#include "Common/FrontendTrace.h"
#include "Common/Log.h"
#include "GPU/GPUCommon.h"

namespace PS3Frontend {
void Trace(const char *format, ...) {
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	INFO_LOG(Log::G3D, "%s", text);
}
}

bool LibretroRSXContext::InitAPI(void *, std::string *, std::string *) {
	hwRender_.context_type = RETRO_HW_CONTEXT_RSX;
	hwRender_.version_major = 1;
	hwRender_.context_reset = []() {
		if (Libretro::ctx)
			static_cast<LibretroRSXContext *>(Libretro::ctx)->ContextReset();
	};
	hwRender_.context_destroy = []() {
		if (Libretro::ctx)
			static_cast<LibretroRSXContext *>(Libretro::ctx)->ContextDestroy();
	};
	return Libretro::environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hwRender_);
}

void LibretroRSXContext::ContextReset() {
	const retro_hw_render_interface *base = nullptr;
	interface_ = nullptr;
	if (Libretro::environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, &base) && base &&
		base->interface_type == RETRO_HW_RENDER_INTERFACE_RSX &&
		base->interface_version == RETRO_HW_RENDER_INTERFACE_RSX_VERSION) {
		const auto *rsx = reinterpret_cast<const retro_hw_render_interface_rsx *>(base);
		if (rsx->context && rsx->io_address && rsx->set_image)
			interface_ = rsx;
	}
	tried_ = false;
}

void LibretroRSXContext::ContextDestroy() {
	ShutdownCore();
	interface_ = nullptr;
}

void LibretroRSXContext::SetRenderTarget() {
	if (!gpu || !interface_)
		return;
	if (!renderer_ && !tried_) {
		tried_ = true;
		if (!RSX::AttachContext((_gcmCtxData *)interface_->context, interface_->io_address,
			interface_->io_size, interface_->io_used))
			return;
		RSXRendererOptions options;
		options.ownDisplay = false;
		options.scale = std::clamp(g_Config.iInternalResolution, 1, 3);
		options.syncEveryFrame = false;
		options.tiled = false;
		options.tiledDepth = false;
		TexSpu::SetEnabled(false);
		renderer_ = RSXRendererCreate(options);
		if (renderer_) {
			Rasterizer::g_hardwareBackend = RSXRendererBackend(renderer_);
			INFO_LOG(Log::G3D, "PPSSPP RSX hardware renderer active, %dx, libretro RSX interface v1", options.scale);
		} else {
			ERROR_LOG(Log::G3D, "PPSSPP RSX initialization failed; using software rendering");
			RSX::ReleaseContext();
		}
	}
	if (renderer_)
		RSXRendererBeginFrame(renderer_);
}

void LibretroRSXContext::SwapBuffers() {
	if (renderer_) {
		RSXRendererFrame frame{};
		if (RSXRendererGetFrame(renderer_, &frame)) {
			retro_rsx_image output{frame.offset, frame.pitch, frame.width, frame.height};
			if (g_Config.bDisplayCropTo16x9) {
				output.offset += output.pitch * (output.height / NATIVEHEIGHT);
				output.height -= 2 * (output.height / NATIVEHEIGHT);
			}
			if (interface_->set_image(interface_->handle, &output)) {
				video_cb(RETRO_HW_FRAME_BUFFER_VALID, output.width, output.height, 0);
				return;
			}
		}
		interface_->set_image(interface_->handle, nullptr);
	}
	LibretroSoftwareContext::SwapBuffers();
}

void LibretroRSXContext::PrepareState(bool loading) {
	if (!renderer_)
		return;
	gpu->FlushPendingDrawing();
	RSXRendererWriteBackAll(renderer_);
	if (loading)
		ShutdownCore();
}

void LibretroRSXContext::ShutdownCore() {
	if (interface_)
		interface_->set_image(interface_->handle, nullptr);
	if (renderer_) {
		if (gpu)
			gpu->FlushPendingDrawing();
		RSXRendererWriteBackAll(renderer_);
		INFO_LOG(Log::G3D, "PPSSPP RSX final: %s", RSXRendererStats(renderer_).c_str());
		Rasterizer::g_hardwareBackend = nullptr;
		RSXRendererDestroy(renderer_);
		renderer_ = nullptr;
		TexSpu::Shutdown();
		RSX::ReleaseContext();
	}
	tried_ = false;
}

void LibretroRSXContext::ShutdownAPI() {
	ShutdownCore();
	LibretroSoftwareContext::ShutdownAPI();
}
