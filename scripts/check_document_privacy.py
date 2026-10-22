"""Reject private workstation identifiers in first-party distributable documentation.

Reports paths and rule names only: never echo a matched private value.
Generic product data paths and required third-party attribution are allowed.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
import zipfile

RULES = {
    'absolute-workstation-path': re.compile(r'(?<![\w/])(?:[A-Za-z]:[\\/]|/(?:Users|home)/)[^\s<>`]+'),
    'user-profile-path': re.compile(r'(?i)(?:[\\/]Users[\\/])(?!Public\b|Default\b|<)[^\s\\/]+'),
    'clipboard-attachment': re.compile(r'codex-clipboard-[a-f0-9-]+', re.I),
    'personal-email': re.compile(r'\b[\w.+-]+@(?:gmail|outlook|hotmail|qq|163|126)\.com\b', re.I),
}


def is_document(name):
    p = Path(name)
    return (p.suffix.lower() in {'.md', '.txt', '.rst'} or
            p.name in {'dependencies.lock.json', 'manifest.json', 'CMakePresets.json'}) and not (
                name.startswith('licenses/') and p.name != 'README.md') and p.name != 'LICENSE'


def inspect(name, data):
    try:
        content = data.decode('utf-8-sig')
    except UnicodeDecodeError:
        return []
    return [(name, rule) for rule, pattern in RULES.items() if pattern.search(content)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, action='append', default=[])
    args = parser.parse_args()
    failures = []
    count = 0
    if args.archive:
        for archive in args.archive:
            with zipfile.ZipFile(archive) as source:
                for name in source.namelist():
                    if is_document(name):
                        count += 1
                        failures.extend(inspect(name, source.read(name)))
    else:
        root = Path(__file__).resolve().parents[1]
        names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')
        for name in names:
            if name and is_document(name) and (root/name).is_file():
                count += 1
                failures.extend(inspect(name, (root/name).read_bytes()))
    for name, rule in failures:
        print(f'{name}: {rule}', file=sys.stderr)
    print(f'Document privacy: {count} inspected, {len(failures)} findings')
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
