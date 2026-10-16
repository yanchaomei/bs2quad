import csv
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent
rows = list(csv.DictReader((root / 'status.csv').open()))
assert len(rows) == 10
expected = {'no_step': None, 'controller': (2916, 136), 'init': (2916, 136)}
for row in rows:
    path = root / (row['mode'] + '-' + row['count'] + '.log')
    log = path.read_text()
    match = re.search(r'SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)', log)
    actual = tuple(map(int, match.groups())) if match else None
    target = expected.get(row['mode'], (2964, 138))
    assert actual == target, (row, actual, target)
    assert row['exit_code'] == ('0' if row['mode'] == 'no_step' else '1')
    if int(row['count']) > 0:
        assert 'PROBE completed writes=' + row['count'] in log
    print(row['mode'], row['count'], 'exit=' + row['exit_code'], actual)
assert (root / 'build.log').read_text() == ''
for name in ['probe.cpp', 'build.sh', 'run-controls.sh', 'collect-environment.sh', 'README.md', 'environment.txt', 'sdk-sha256.txt', 'pinned-source-excerpts.txt', 'probe.step']:
    assert (root / name).is_file(), name
print('PASS: 10 controls, exact leak counts, successful STEP completions, clean build, metadata and source receipts present')
