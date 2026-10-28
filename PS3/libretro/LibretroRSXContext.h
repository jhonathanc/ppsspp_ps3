// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
#pragma once
#include "libretro/LibretroGraphicsContext.h"
#include "PS3/libretro/libretro_rsx.h"

class RSXRenderer;

class LibretroRSXContext : public LibretroSoftwareContext {
public:
	~LibretroRSXContext() override { ShutdownAPI(); }
	bool InitAPI(void *, std::string *, std::string *) override;
	void SetRenderTarget() override;
	void SwapBuffers() override;
	void PrepareState(bool loading) override;
	void ShutdownCore() override;
	void ShutdownAPI() override;
	void ContextReset();
	void ContextDestroy();
	const char *Ident() override { return "RSX"; }

private:
	RSXRenderer *renderer_ = nullptr;
	const retro_hw_render_interface_rsx *interface_ = nullptr;
	retro_hw_render_callback hwRender_{};
	bool tried_ = false;
};
