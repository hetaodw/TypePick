# SPDX-License-Identifier: AGPL-3.0-only
"""Package tracked source only; never copy workspace secrets or unrelated files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--stage', type=Path, required=True)
p.add_argument('--sources', type=Path, required=True)
p.add_argument('--boost', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
revision = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
archive = root / 'dist' / 'TypePick-Source-0.1.0.zip'
manifest = {}

def walk_source(path):
    for current, dirs, files in os.walk(path):
        dirs[:] = [d for d in dirs if d != '.git']
        for name in files:
            if name != '.git' and not name.endswith('.part'):
                yield Path(current) / name

with zipfile.ZipFile(str(archive) + '.part', 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for repo, prefix in [(root, 'TypePick'), (root/'upstream/weasel', 'TypePick/upstream/weasel')]:
        tracked = subprocess.check_output(['git', '-C', str(repo), 'ls-files', '-z']).decode().split('\0')
        for name in tracked:
            f = repo/name
            if name and f.is_file() and f.name != '.env':
                z.write(f, f'{prefix}/{name}')
    for f in walk_source(a.sources):
        relative = f.relative_to(a.sources)
        z.write(f, 'dependencies/' + relative.as_posix())
        if f.name.lower().startswith(('license', 'copying', 'copyright', 'notice')) or f.name == 'lua.h':
            out = a.stage/'licenses'/'dependencies'/relative
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, out)
    # Include Weasel's exact Boost source archive; the build cache may lack it.
    boost_archive = a.boost.parent/'boost-1.84.0.7z'
    if not boost_archive.exists():
        import urllib.request
        urllib.request.urlretrieve('https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.7z', str(boost_archive)+'.part')
        Path(str(boost_archive)+'.part').replace(boost_archive)
    if hashlib.sha256(boost_archive.read_bytes()).hexdigest() != 'ce132f85fc706aa8b5c7e6e52a8338de33a688e92e7c8fada3713194b109232e':
        raise RuntimeError('Boost source archive checksum mismatch')
    z.write(boost_archive, 'dependencies/boost-1.84.0.7z')
    z.writestr('BUILD.txt', 'TypePick revision: '+revision+'\nSee TypePick/docs/windows-release.md.\nDependency source and original upstream build scripts are included.\n')
with zipfile.ZipFile(str(archive)+'.part') as z:
    if z.testzip():
        raise RuntimeError('Invalid source ZIP')
Path(str(archive)+'.part').replace(archive)
for f in walk_source(a.stage):
    manifest[f.relative_to(a.stage).as_posix()] = hashlib.sha256(f.read_bytes()).hexdigest()
(a.stage/'SOURCE.txt').write_text(
    'TypePick source revision: '+revision+'\n'
    'Source: https://github.com/hetaodw/TypePick/tree/'+revision+'\n'
    'Corresponding source archive: TypePick-Source-0.1.0.zip, distributed alongside this installer.\n'
    'TypePick changes: AGPL-3.0-only. Weasel: GPL-3.0. See licenses for other components.\n', encoding='utf-8')
(root/'dist'/'build-manifest.json').write_text(json.dumps({'revision': revision, 'files': manifest}, indent=2), encoding='utf-8')
print('Source ZIP verified; secret files and untracked workspace files excluded.')
