#!/usr/bin/env python3
# Copyright (c) 2012- PPSSPP Project.
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Order PSL1GHT's allocating constructor after newlib's malloc lock setup."""
import pathlib
import sys

path = pathlib.Path(sys.argv[1]) / 'ppu/librt/globfile.c'
with path.open(newline='') as stream:
	text = stream.read()
old = 'static void __glob_file_init(void)  __attribute__((constructor(105)));'
new = 'static void __glob_file_init(void)  __attribute__((constructor(106)));'
if new not in text:
	if text.count(old) != 1:
		raise RuntimeError('Unexpected PSL1GHT constructor declaration')
	text = text.replace(old, new)
	with path.open('w', newline='') as stream:
		stream.write(text)
print('Prepared PSL1GHT: initialize the working directory after newlib malloc locks')
