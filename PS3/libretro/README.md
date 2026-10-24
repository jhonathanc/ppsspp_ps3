# PSL1GHT libretro core

Load the threaded runtime environment, then configure a separate build:

```sh
. "$HOME/.local/ps3aqua-runtime/env.sh"
cmake -S . -B build-ps3-libretro -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=PS3/psl1ght.cmake \
  -DCMAKE_BUILD_TYPE=Release -DLIBRETRO=ON \
  -DHEADLESS=OFF -DPS3_FRONTEND=OFF
cmake --build build-ps3-libretro --target ps3_libretro_archive --parallel 6
bash PS3/libretro/build-retroarch.sh /path/to/RetroArch \
  build-ps3-libretro build-retroarch-ppsspp
```

The first target combines the frontend, Core, GPU and built dependency archives
into `ppsspp_libretro_psl1ght.a`. The script copies the current tracked RetroArch
files, including changes without a commit, to a separate build directory. It
generates the VFS and RSX memory overlays there and links
`ppsspp_libretro_ps3.elf`. The original RetroArch workspace is preserved.
`cgcomp` needs its Cg shared library on the host's library search path.

The integration uses PPSSPP's software GPU and RetroArch's RSX video driver.
OpenGL and Vulkan contexts are excluded from this PS3 core. The RSX host-memory
reservation is 8 MB, with a 1 MB command buffer. Data paths follow the RetroArch
sources supplied to the script. The current tested reversal uses
`/dev_hdd0/game/SSNE10001/USRDIR`; no user-ID override is applied.

For the ProDG test, the ELF was converted to a SELF with the installed Sony
`make_fself.exe`. The tested file is
`PS3/build/libretro/ppsspp_libretro_ps3_reverted.self`. Its source build uses
PSL1GHT; the Sony tool supplies the SELF container.

On PS3-DEX, this SELF loaded Final Fantasy Type-0 from the HDD and displayed
the initial autosave notice using the PPC JIT. The run ended cleanly after
120 frontend frames (two emulated seconds). The capture and log are
`PS3/build/libretro/type0-reverted.png` and `type0-reverted.log`.
Gameplay, audio and movies have not been validated. FFmpeg is disabled in
this tested SELF. The log also records a missing `ppge_atlas.zim` asset and
an invalid-size `sceKernelReferThreadStatus` call; these need investigation
before claiming game compatibility.

See [../INTEGRATION.md](../INTEGRATION.md) for SDK selection, validation on other
platforms and remaining regression risks. The Sony C adapter is available, but
the installed Sony GCC 4.1.1 cannot build the C++17 core.
