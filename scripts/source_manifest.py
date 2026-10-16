#!/usr/bin/env python3
"""Hash executable sources and verification recipes, excluding generated docs."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--root', default=str(Path(__file__).resolve().parent.parent))
parser.add_argument('--output')
args = parser.parse_args()
root = Path(args.root).resolve()
paths = set()
for directory in ['src', 'include', 'tests', 'scripts']:
    for path in (root / directory).rglob('*'):
        if path.is_file() and path.suffix in ['.cpp', '.h', '.sh', '.py']:
            paths.add(path.relative_to(root).as_posix())
for filename in ['CMakeLists.txt', 'Dockerfile', '.dockerignore']:
    if (root / filename).is_file():
        paths.add(filename)
aggregate = hashlib.sha256()
files = []
for name in sorted(paths):
    data = (root / name).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    aggregate.update(name.encode('utf-8') + b'\0' + digest.encode('ascii') + b'\n')
    files.append({'path': name, 'bytes': len(data), 'sha256': digest})
result = {'schema': 1, 'scope': 'sources, tests, verification recipes; generated documents/evidence excluded',
          'aggregate_method': 'SHA256 of sorted path+NUL+hex_file_sha256+LF records',
          'aggregate_sha256': aggregate.hexdigest(), 'files': files}
text = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
if args.output:
    Path(args.output).write_text(text, encoding='utf-8')
else:
    print(text, end='')
