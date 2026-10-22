"""Validate repository Markdown file links and navigation coverage, without network access."""
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote

ROOT=Path(__file__).resolve().parents[1]


def targets(text):
    text=re.sub(r'(?ms)^\s*(```|~~~).*?^\s*\1\s*$', '', text)
    text=re.sub(r'`[^`\n]+`','',text)
    inline=re.findall(r'\]\(\s*(<[^>]+>|[^\s()]+)(?:\s+"[^"]*")?\s*\)',text)
    references=re.findall(r'(?m)^\[[^\n\]]+\]:\s*(\S+)',text)
    return [t.strip('<>') for t in inline+references]


def resolve(source,target,root=ROOT):
    if not target or target.startswith('#') or re.match(r'[A-Za-z][\w+.-]*:',target):
        return None
    path=unquote(target.split('#',1)[0].split('?',1)[0])
    if not path:return None
    return (root/path.lstrip('/') if path.startswith('/') else source.parent/path).resolve()


def check(root,names):
    root=root.resolve()
    markdown={root/name for name in names if name.endswith('.md') and (root/name).is_file()}
    graph={};errors=[]
    for source in sorted(markdown):
        neighbors=[]
        for target in targets(source.read_text(encoding='utf-8-sig')):
            dest=resolve(source,target,root)
            if dest is None:continue
            if not dest.is_relative_to(root) or not dest.exists():
                errors.append((source.relative_to(root).as_posix(),'broken file link: '+target))
            elif dest in markdown:neighbors.append(dest)
        graph[source]=neighbors
    reached=set();pending=[root/'README.md',root/'docs/README.md']
    while pending:
        path=pending.pop()
        if path in reached:continue
        reached.add(path);pending.extend(graph.get(path,[]))
    for source in sorted(markdown):
        if source.is_relative_to(root/'docs') and source not in reached:
            errors.append((source.relative_to(root).as_posix(),'not reachable from documentation navigation'))
    return len(markdown),errors


def main():
    names=subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode().split('\0')
    count,errors=check(ROOT,names)
    for name,error in errors:print(f'{name}: {error}',file=sys.stderr)
    print(f'Document links/navigation: {count} Markdown files, {len(errors)} findings')
    return int(bool(errors))


if __name__=='__main__':raise SystemExit(main())
