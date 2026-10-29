# PSL1GHT libretro core

The PSL1GHT build uses the port's RSX hardware renderer. PPSSPP decodes PSP
command lists and submits geometry to RSX; the frontend presents the exported
GPU texture and owns display buffers, menus, scaling and presentation.

The core requests an RSX context with `RETRO_ENVIRONMENT_SET_HW_RENDER` and
obtains a versioned interface through `RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE`
in `context_reset`. `PS3/libretro/libretro_rsx.h` is the same public extension
header as RetroArch's `libretro-common/include/libretro_rsx.h`. It uses context
ID 11 and render-interface ID 6, with a single version 1 interface including
`retire_memory`. No PPSSPP symbols, include paths or compile definitions are
needed by the frontend video driver.

A frontend needs RSX backend support for hardware rendering. An unmodified
frontend refuses the request and the core falls back to software rendering.
Reading the PSL1GHT global context directly would bypass thread, GPU-state and
memory ownership; copying each frame back to the CPU would add transfer cost.

`PPSSPP_LIBRETRO_RSX=ON` is the default for PSL1GHT. Set it to `OFF` for a
software-only core, or choose Renderer = None in the core options.

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

The frontend selects its main-memory reservation with `RSX_HOST_SIZE`. The core
borrows only the unused tail, after the 1 MiB command buffer and its margin. Its 8 MiB vertex
ring resides in RSX local memory. It allocates no second main-memory command
region and does not register its own display buffers. Both users submit on the
main thread for RSX sessions; software cores retain threaded-video support.
Shader, vertex and render-state caches are invalidated at each handoff.

`context_destroy` releases the renderer before the frontend destroys the
interface. Reset and unload finish pending drawing before freeing GPU resources.
Savestate save writes RSX framebuffers back before serializing PSP memory;
loading recreates the renderer. The serialized format is unchanged.

The frontend helper copies working sources, including local additions, and
applies the VFS overlay. FFmpeg builds also need the five FFmpeg static libraries
in the frontend link. RetroArch's normal user-directory selection and fallback
to its installation directory remain in effect.

`samples/gfx/rsx_alloc` in RetroArch tests the generic hardware interface,
command-memory boundaries, thread rejection, GPU image submission, invalid
ranges and software-driver regressions on Linux and Windows. It has no PPSSPP
dependency.

Only PSL1GHT selects these sources. Other platforms retain their graphics
contexts.
