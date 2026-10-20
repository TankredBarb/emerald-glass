#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Validate the researched upstream shaders; does not load a KWin plugin."""
import hashlib
import json
from pathlib import Path
import subprocess


root = Path(__file__).resolve().parent
source = root / 'vendor/kwin-effects-glass/src/generated'
output = root / 'build/shader-validation'
output.mkdir(parents=True, exist_ok=True)
include = (root / 'vendor/kwin-sdf-6.7.5.glsl').read_text()
records = []

for path in sorted(source.iterdir()):
    if path.suffix not in ('.vert', '.frag'):
        continue
    text = path.read_text().replace('#include "sdf.glsl"', include)
    target = output / path.name
    target.write_text(text)
    records.append({'shader': path.name,
                    'expanded_sha256': hashlib.sha256(text.encode()).hexdigest()})

checks = []
for vertex, fragment in [('vertex.vert', name) for name in
                         ('downsample.frag', 'upsample.frag', 'noise.frag')] + [
                         ('onscreen_rounded.vert', 'onscreen_rounded.frag')]:
    command = ['glslangValidator', '-l', str(output / vertex), str(output / fragment)]
    result = subprocess.run(command, text=True, capture_output=True)
    checks.append({'vertex': vertex, 'fragment': fragment,
                   'exit_code': result.returncode,
                   'output': result.stdout + result.stderr})

report = {'method': 'Offline GLSL compilation and stage linkage, not GPU rendering',
          'shaders': records, 'checks': checks,
          'passed': all(item['exit_code'] == 0 for item in checks)}
(root / 'shader-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'shaders': len(records), 'programs': len(checks),
                  'passed': report['passed']}))
raise SystemExit(0 if report['passed'] else 1)
