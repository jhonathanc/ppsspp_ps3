OUT ?= /tmp/ppsspp-retroarch
OBJDIR ?= $(OUT)/obj

override OBJ = $(OUT)/obj/modern_opaque.vpo.o $(OUT)/obj/modern_opaque.fpo.o
override OBJ += $(OUT)/obj/modern_alpha_blend.vpo.o $(OUT)/obj/modern_alpha_blend.fpo.o
override OBJ += $(OUT)/obj/libco.o $(OBJDIR)/griffin.o
ifneq ($(wildcard frontend/drivers/platform_ps3_userinfo.S),)
override OBJ += $(OUT)/obj/userinfo.o
endif

include Makefile.psl1ght

CFLAGS += -I$(PPSSPP_ROOT)/PS3/runtime/include -I$(PPSSPP_PS3_RUNTIME)/include
CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS += -L$(PPSSPP_PS3_RUNTIME)/lib -Wl,--gc-sections
LDFLAGS += -Wl,--wrap=rsxSetTransferScaleSurface
LIBDIRS += -L$(PSL1GHT)/ppu/lib
LIBS := -Wl,--start-group $(CORE_LIBRARY) $(filter-out -lretro_psl1ght,$(LIBS)) -lstdc++ -lpthread -Wl,--end-group

$(ELF_TARGET): $(CORE_LIBRARY) $(PSL1GHT)/ppu/lib/librt.a $(PPSSPP_PS3_RUNTIME)/lib/libpthread.a

$(OBJDIR)/griffin.o: griffin/griffin.c $(PPSSPP_ROOT)/PS3/libretro/RetroArch.mk
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/obj/libco.o: libretro-common/libco/ps3.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/obj/userinfo.o: frontend/drivers/platform_ps3_userinfo.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/obj/%.vpo: gfx/drivers/rsx_shaders/%.vcg
	$(CGCOMP) -v $< $@

$(OUT)/obj/%.fpo: gfx/drivers/rsx_shaders/%.fcg
	$(CGCOMP) -f $< $@

$(OUT)/obj/%.vpo.o: $(OUT)/obj/%.vpo
	bin2s -a 64 $< | $(AS) -o $@

$(OUT)/obj/%.fpo.o: $(OUT)/obj/%.fpo
	bin2s -a 64 $< | $(AS) -o $@
