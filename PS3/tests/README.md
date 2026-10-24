# PS3 adapter checks

Run `bash PS3/tests/run-host.sh` from the repository. The test runner compiles
and runs the production PS3 memory adapters against a Linux syscall model.
Shared POSIX mappings exercise VRAM alias writes; injected failures check
cleanup retries, bounds, page rounding and unsupported permissions. This
model does not establish that LV2 permits the same mappings on real PS3.

The PS3 build must compile these files with the installed PSL1GHT headers and
the rebuilt pthread-enabled C++ runtime. Never ship the mock headers or their
implementations with the emulator. Native executable allocation is currently
unsupported in this model (the PowerPC JIT is tested with PS3/tools/ppc64-test).

Regression risks: insufficient physical memory, LV2 mapping/alias restrictions,
failed cleanup during shutdown, thread lifecycle, clock initialization order,
and filesystem errors. Console testing remains required before distributing
a game-loading PKG. Console results from the original snapshot do not validate
this integration on the current official base.

The postoffice socket model also checks connection cleanup, partial packets,
would-block, EOF, EINTR and bounded polling with cancellation. Native IPv6 is
explicitly unsupported. PS3 configuration copies the pinned postoffice client
and reduces its session pool in the build directory via PS3/prepare-postoffice.py;
the upstream submodule is left intact. The separate socket and ENet adapters
follow the current callback API. Overlay preparation fails if the expected
source patterns change.

Native executable link checks add sendmsg gathering (one real send/sendto per
message), positioned file reads with a mutex around seek/read, and LV2 ZIP
randomness. Eleven groups exercise API models; framebuffer_endian checks the production
color/depth buffer accessors and physical byte order. It also passes under
big-endian PowerPC QEMU. Secure randomness propagates
syscall failure; only the separate non-cryptographic ZIP API has a fallback.
The native headless ELF link and Cell import/function-descriptor checks are
build checks. For the current console results and their limits, see
PS3/INTEGRATION.md.
