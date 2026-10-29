# PS3 integration on the current official base

The PS3 changes from origin commit 4c357a2 are adapted onto official commit
42bb9b134b. The original commit is a full source snapshot following an empty
parent, so applying it directly replaces the tree and loses the submodules.
This integration keeps the official submodule entries and the current CPU/GPU
interfaces, including JitBlockTable, AddressSpace and the block shadow.

## PS3 SDK

The PS3 port uses PSL1GHT. `PS3/psl1ght.cmake` sets `PS3=ON` and
`PS3_PSL1GHT=ON`, defining `__PS3__` and `__PSL1GHT__` for the core.

Memory reservations, page commits, clocks and thread identification use
`PS3/SDK/PS3SDK.h`, implemented by `PS3/SDK/PSL1GHT.c`. The C adapter
keeps SDK headers separate from the C++17 core.

The RSX transform and lighting code, GE worker and SAS kernel bridge live under PS3/, with
conditional includes in the shared sources. Default desktop builds use the
official SAS mixer and do not build PPC, RSX or SPU sources. The generic endian
fixes remain shared because PSP data is always little endian.

## Core without the standalone frontend

In WSL, the updated SDK is installed at `~/.local/ps3aqua-sdk`, with the
threaded C++ runtime at `~/.local/ps3aqua-runtime`. The user's .bashrc and .profile
load their environment for new sessions. For the current shell:

```sh
. "$HOME/.local/ps3aqua-runtime/env.sh"
cmake -S . -B build-ps3 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=PS3/psl1ght.cmake \
  -DCMAKE_BUILD_TYPE=Release -DPS3_FRONTEND=OFF
cmake --build build-ps3 --target Core --parallel 6
```

The runtime contains matching GCC 7.5.0 headers, libstdc++.a and libpthread.a.
The updated PPU toolchain uses newlib 4.4.0; SPU also uses GCC 7.5.0. Sources and
build logs are under `~/.local/share/ps3aqua/`. The original SDK at
`/usr/local/ps3dev` is preserved. Installed source revisions:

- [ps3aqua/ps3toolchain](https://github.com/ps3aqua/ps3toolchain):
  `961fddac01337f18da08f4471d558a7a5b0d9af2`.
- [ps3aqua/PSL1GHT](https://github.com/ps3aqua/PSL1GHT):
  `af9d3d964c8faa69abce4961a269f9582e05a33f`.

The pthread dependency comes from
[bucanero/pthread-emb-ps3](https://github.com/bucanero/pthread-emb-ps3), commit
643171135e8f315e2a1c0bf469de0788f167a055.

PS3_FRONTEND defaults to OFF. With LIBRETRO=ON, the PS3 build produces a static
core that can be linked to RetroArch using PS3/libretro/build-retroarch.sh.
The integration uses PPSSPP software rendering and RetroArch's RSX video driver.
The imported standalone frontend remains optional. See PS3/libretro/README.md.

The PPC JIT memory provider no longer depends on the standalone GUI. A frontend
may register logging/thread diagnostic hooks and an optional persistent path for
the direct code-write probe. Without that path, the JIT uses the PS3MAPI route,
falling back to the IR interpreter when it is unavailable.

The current postoffice socket backend follows the official submodule's callback
and partial-I/O contract. PSL1GHT supports its IPv4 TCP path; ENet and IPv6 requests
fail explicitly. A small generated overlay reduces the session pool without
editing the submodule.

The updated SDK supplies `netGetSockInfo`, verified by compiling and linking a
native Cell executable. The older receive-queue fallback using `MSG_PEEK` and
its feature detection have been removed. The select/FD_SET adapter remains:
SDK socket handles contain `SOCKET_FD_MASK`, while newlib FD_SET does not remove
it and uses a different mask layout. The sendmsg adapter also remains because
the SDK declares POSIX sendmsg without implementing its wrapper. Both adapters
are compiled only for `__PSL1GHT__`.

## Validation and remaining risks

The installed runtime probe compiles and links a native Cell ELF using
std::thread, mutex, condition_variable, call_once, shared_ptr and C++17 strings.
The Cell OSABI, import table and function-descriptor checks pass. The runtime
probe also passed on the PS3-DEX through ProDG, including six parked workers,
8,000 contended updates, shared ownership, call_once and timed waits.

Hardware startup exposed two dependency bugs, now covered by reproducible
patch scripts in PS3/runtime/: PSL1GHT allocated its working directory before
newlib initialized malloc locks, and PTE used incorrect timeout/polling behavior.
The patched SDK and PTE libraries are installed in the new WSL prefixes.

The current official-base headless executable starts on the PS3-DEX. The
IR interpreter and software GPU passed cpu/cpu_alu, cpu/vfpu/prefix_load,
gpu/commands/blocktransfer, gpu/transfer/mirrors and gpu/depth/precision.

RetroArch was built from its current workspace files, including the user's
uncommitted reversal of the per-user directory feature. The integrated SELF
started Final Fantasy Type-0 from the console HDD using the PPC JIT and reached
the initial autosave notice. A screenshot was captured after 120 frontend
frames (two emulated seconds), followed by a clean shutdown. Gameplay, audio
and movie playback have not been validated. The tested SELF has FFmpeg disabled.

The PS3 build grows the IR cache on demand and uses smaller software-GPU queues.
Hardware transform buffers unused by the PS3 software frontend are omitted.
RetroArch's RSX host reservation is 8 MB in this build; the command buffer is 1 MB.

PS3/tests/run-host.sh exercises memory aliases, incremental page commits,
allocation/map/cleanup failures, monotonic time, DNS, sockets, cancellation,
positioned reads and ZIP randomness against host models. The PPC compiler and
optional RSX/SAS code also receive host syntax checks. Neither check establishes
PS3 runtime correctness.

Linux validation passed 92 C++ tests, all 473 standard pspautotests plus the
separate no-memory-stick test, and all twelve PS3 adapter groups. A QEMU
big-endian PowerPC run passed 851 checks of the production IR interpreter.

Regression-sensitive areas are endian conversions, CPU cache invalidation and
block linking, software GPU/RSX arithmetic, asynchronous GE ordering and SAS/SPU
mixing. Wider hardware validation is still required. The HLE function tables keep their
original order; the GPUCommon savestate section remains version 8. Cross-endian
savestates and game compatibility have not been established.

Windows validation requires the Visual Studio C++ workload; the installed VS2022
currently lacks Microsoft.Cpp.Default.props.
