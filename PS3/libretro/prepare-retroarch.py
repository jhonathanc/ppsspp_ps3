#!/usr/bin/env python3
# Copyright (c) 2012- PPSSPP Project.
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Make the copied RetroArch RSX command-memory reservation configurable."""
import pathlib
import sys

path = pathlib.Path(sys.argv[1]) / 'gfx/drivers/rsx_gfx.c'
with path.open(newline='') as stream:
	text = stream.read()
ending = '\r\n' if '\r\n' in text else '\n'
text = text.replace('\r\n', '\n')
old = '#define HOST_SIZE\t(32*1024*1024)'
new = '''#ifndef PPSSPP_PS3_RSX_HOST_SIZE
#define PPSSPP_PS3_RSX_HOST_SIZE (32*1024*1024)
#endif
#define HOST_SIZE PPSSPP_PS3_RSX_HOST_SIZE'''
if new not in text:
	if text.count(old) != 1:
		raise RuntimeError('Unexpected RetroArch RSX host-memory declaration')
	text = text.replace(old, new)
	with path.open('w', newline='') as stream:
		stream.write(text.replace('\n', ending))
print('Prepared RetroArch: configurable RSX command-memory reservation')
