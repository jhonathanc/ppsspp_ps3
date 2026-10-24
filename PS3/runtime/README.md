# Native PS3 C++ runtime

The ps3aqua toolchain ships GCC 7.5.0 with a single-thread libstdc++. Full PPSSPP
requires threads, condition variables, exception guards and atomic shared
ownership. Rebuild the **complete** library with the pthread-embedded backend;
do not combine thread-enabled headers with the SDK's old libstdc++.a.

Restore the matching GNU GCC 7.2.0 or 7.5.0 release source (including libgcc and configure
support files). The installed pthread-embedded headers and library are also
required. Before building the runtime, apply the SDK and PTE fixes:

```sh
python3 ppsspp/PS3/runtime/prepare-sdk.py PSL1GHT
make -C PSL1GHT/ppu/librt install
python3 ppsspp/PS3/runtime/prepare-pthread.py pthread-emb-ps3
```

Rebuild and install pthread-embedded from that patched tree into the runtime
prefix before building libstdc++. `prepare-sdk.py` moves PSL1GHT's allocating
working-directory constructor after newlib 4.4's malloc lock constructor.
`prepare-pthread.py` uses LV2 elapsed time, gives zero timeouts nonblocking
semantics, and blocks idle polling threads briefly so they do not starve their
creator. Both scripts validate their source patterns and are idempotent.

To rebuild libstdc++:

```sh
export PS3DEV="$PWD/native-sdk/ps3dev"
export PSL1GHT="$PS3DEV"
python3 ppsspp/PS3/runtime/prepare.py gcc-7.5.0
bash ppsspp/PS3/runtime/build.sh gcc-7.5.0 native-runtime/build native-runtime/install
export PPSSPP_PS3_RUNTIME="$PWD/native-runtime/install"
cmake -S ppsspp -B build-ps3 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=PS3/psl1ght.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-ps3 --target Core
```

`prepare.py` must run once on a fresh source tree. It adapts the PTE handle
representation, replaces non-constexpr pointer sentinel initializers with real
initialization calls, and supplies C++17 character length where needed (GCC 7.2). The include wrappers
restore C linkage for the SDK pthread header and expose actual newlib C99
functions in strict C++ modes. For newlib 4.4, the overlay also suppresses its native pthread type definitions
so that PTE supplies the matching handle and synchronization types. Weak pthread
references are disabled so static
linking pulls in the real implementations.

`probe.cpp` checks mutex contention, shared ownership across threads, call_once,
condition signaling and timeouts, and C++17 string support. It also parks six
workers before releasing and joining them, covering the idle-thread startup
case. The native executable passes Cell ELF checks and ran successfully on the
PS3-DEX through ProDG: all six idle workers joined, the contention count was
8,000, call_once ran once, and the final result was PASS.

See ../INTEGRATION.md for validation of the current official-base integration.
Builds and recorded console results from the original snapshot do not establish
that this integration starts or plays games on a PS3.

GCC/libstdc++ sources retain their GPLv3 and GCC Runtime Library Exception
licenses; see the original release's COPYING3 and COPYING.RUNTIME.
