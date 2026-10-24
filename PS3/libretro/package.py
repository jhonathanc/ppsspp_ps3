#!/usr/bin/env python3
# Copyright (c) 2012- PPSSPP Project.
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Merge the built PS3 core archives for RetroArch's static core link."""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ar', required=True)
parser.add_argument('--directory', type=pathlib.Path, required=True)
parser.add_argument('--output', type=pathlib.Path, required=True)
args = parser.parse_args()
libraries = sorted(args.directory.resolve().glob('*.a'))
required = {'ppsspp_libretro.a', 'libCore.a', 'libCommon.a'}
if not required.issubset({path.name for path in libraries}):
	raise RuntimeError('Build the ppsspp_libretro target first')
output = args.output.resolve()
libraries = [path for path in libraries if path != output]
output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ps3-core-', dir=output.parent) as folder:
	work = pathlib.Path(folder)
	commands = ['CREATE core.a']
	for index, library in enumerate(libraries):
		name = 'input%d.a' % index
		(work / name).symlink_to(library)
		commands.append('ADDLIB ' + name)
	commands.extend(['SAVE', 'END'])
	subprocess.run([args.ar, '-M'], input='\n'.join(commands) + '\n', text=True, cwd=work, check=True)
	(work / 'core.a').replace(output)
print('PS3 libretro core archive: %s (%d libraries)' % (output, len(libraries)))
