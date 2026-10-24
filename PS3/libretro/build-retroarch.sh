#!/usr/bin/env bash
set -euo pipefail
: "${PS3DEV:?Set PS3DEV to the ps3aqua SDK root}"
: "${PSL1GHT:?Set PSL1GHT to the SDK root}"
: "${PPSSPP_PS3_RUNTIME:?Set PPSSPP_PS3_RUNTIME to the threaded runtime prefix}"
repo=$(cd "$(dirname "$0")/../.." && pwd)
retroarch=$(realpath "${1:?RetroArch source directory}")
core_build=$(realpath "${2:?PPSSPP PS3 libretro CMake build directory}")
output=$(realpath -m "${3:?Separate RetroArch build directory}")
if [[ $output == "$retroarch" || $output == "$repo" ]]; then
	echo 'Use a separate build directory.' >&2
	exit 1
fi
cmake --build "$core_build" --target ps3_libretro_archive --parallel "${JOBS:-6}"
mkdir -p "$output/source" "$output/obj"
# Copy current tracked sources, including local edits, without old build artifacts.
git -C "$retroarch" ls-files -z | tar -C "$retroarch" --null -T - -cf - | tar -C "$output/source" -xf -
python3 "$repo/PS3/prepare-libretro-vfs.py" \
	"$output/source/libretro-common/vfs/vfs_implementation.c" \
	"$output/source/libretro-common/vfs/vfs_implementation.c"
python3 "$repo/PS3/libretro/prepare-retroarch.py" "$output/source"
touch "$output/source/griffin/griffin.c"
make -C "$output/source" -f "$repo/PS3/libretro/RetroArch.mk" \
	-j"${JOBS:-6}" OUT="$output" PPSSPP_ROOT="$repo" \
	CORE_LIBRARY="$core_build/ppsspp_libretro_psl1ght.a" \
	ELF_TARGET="$output/ppsspp_libretro_ps3.elf" "$output/ppsspp_libretro_ps3.elf"
"$PS3DEV/bin/sprxlinker" "$output/ppsspp_libretro_ps3.elf"
python3 "$repo/PS3/cpu-test/verify-elf.py" "$output/ppsspp_libretro_ps3.elf"
echo "RetroArch + PPSSPP ELF: $output/ppsspp_libretro_ps3.elf"
