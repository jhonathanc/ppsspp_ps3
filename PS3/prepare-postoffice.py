#!/usr/bin/env python3
"""Generate a PS3-only overlay without modifying the pinned upstream submodule."""
import pathlib
import shutil
import sys

src, dst = map(pathlib.Path, sys.argv[1:])
dst.mkdir(parents=True, exist_ok=True)
for path in src.glob('*.h'):
    shutil.copy2(path, dst / path.name)
shutil.copy2(src.parent / 'aemu_postoffice_packets.h', dst.parent / 'aemu_postoffice_packets.h')

def replace(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Pinned postoffice source changed: ' + old[:60])
    return text.replace(old, new)

shutil.copy2(src / 'postoffice.c', dst / 'postoffice.c')
# 4 sessions of each kind like the PSP build, not 32: 32 take 2 MB of the PS3's scarce main memory.
text = (src / 'postoffice_mem_stdc.c').read_text()
text = replace(text, '#define SESSIONS_PER_TYPE 32', '#define SESSIONS_PER_TYPE 4')
(dst / 'postoffice_mem_stdc.c').write_text(text)
