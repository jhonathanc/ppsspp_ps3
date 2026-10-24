#!/usr/bin/env python3
# Copyright (c) 2012- PPSSPP Project.
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Generate a PSL1GHT VFS overlay without editing libretro-common."""
import pathlib
import sys

source, output = map(pathlib.Path, sys.argv[1:])
with source.open(newline='') as stream:
	text = stream.read()
ending = '\r\n' if '\r\n' in text else '\n'
text = text.replace('\r\n', '\n')
stat = text.index('sysFSStat stat_buf;')
start = text.rfind('#elif ', 0, stat)
end = text.index('#elif ', stat)
old = '#elif defined(__PSL1GHT__) || defined(__PS3__)\n'
block = text[start:end]
if not block.startswith(old):
	if not block.startswith('#elif defined(__PSL1GHT__)\n') or 'stat_buf.size' not in block:
		raise RuntimeError('Unexpected libretro-common PS3 stat implementation')
else:
	body = block[len(old):]
	psl = body.replace('stat_buf.st_size', 'stat_buf.size')
	psl = psl.replace('stat_buf.st_mode', 'stat_buf.mode').replace('stat_buf.st_mtime', 'stat_buf.mtime')
	if psl == body:
		raise RuntimeError('Expected Sony fields in the shared PS3 stat implementation')
	text = text[:start] + '#elif defined(__PSL1GHT__)\n' + psl + '#elif defined(__PS3__)\n' + body + text[end:]
output.parent.mkdir(parents=True, exist_ok=True)
with output.open('w', newline='') as stream:
	stream.write(text.replace('\n', ending))
print('Prepared libretro-common VFS: use the PSL1GHT stat layout')
