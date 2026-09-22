# SPDX-License-Identifier: AGPL-3.0-only
from pathlib import Path
import shutil
import sys
source, target = map(Path, sys.argv[1:])
count = 0
for folder in (source/'libs').glob('**/include/boost'):
    shutil.copytree(folder, target/'boost', dirs_exist_ok=True)
    count += 1
for name in ['config/user.hpp', 'serialization/vector.hpp', 'thread/thread.hpp']:
    if not (target/'boost'/name).is_file():
        raise RuntimeError('Missing Boost header after repair: '+name)
print(f'Restored physical Boost headers from {count} modules.')
