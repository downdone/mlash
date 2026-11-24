#!/usr/bin/env python3
"""必要性限制就地声明检查（原则 13）。

规则：
  1. REQUIRED 中的文件必须含 `// CONTRACT` 块，且至少有 owner、invariant、tests、spec 四个字段。
  2. 任何含 `// CONTRACT` 的文件都要通过同样的字段检查（可选文件不得写出半截契约）。
  3. `tests:` 中引用的每个名称必须是对应的 CTest 用例名（add_test NAME ...），否则失败。
  4. 头内不得重复机检事实：出现「允许依赖 / 禁止 include」一类措辞即失败，应改为引用检查名。

用法：
  python scripts/check_contract_headers.py
  python scripts/check_contract_headers.py --self-test
返回码：0 通过；1 有违例。
"""
from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = (
    "src/app/Async.h",
    "src/app/SessionGate.h",
    "src/core/TunSession.h",
    "src/storage/Storage.h",
    "src/domain/ConnectionColumns.h",
    "src/ui/ContainerIds.h",
)
REQUIRED_FIELDS = ("owner", "invariant", "forbidden", "lifecycle", "tests", "spec")
OPTIONAL_FIELDS: tuple[str, ...] = ()
LAYERS = {"app", "core", "profile", "platform", "storage", "diagnostics", "domain", "ui", "common"}
PLACEHOLDERS = {"", "x", "todo", "tbd", "n/a", "na", "-"}
MIN_VALUE = 6
MAX_BLOCK_LINES = 8
FIELD = re.compile(r"^//\s*(owner|invariant|forbidden|lifecycle|tests|spec)\s*:\s*(.+)$")
CONTRACT = re.compile(r"^//\s*CONTRACT\b", re.MULTILINE)
RESTATED = re.compile(r"(允许依赖|禁止 ?include|allowed dependencies|forbidden includes)", re.IGNORECASE)


def ctest_names() -> set[str]:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    return set(re.findall(r"add_test\(\s*NAME\s+(\w+)", cmake))


def parse_contract(text: str) -> tuple[dict[str, str], int, list[str]]:
    """返回（字段字典，块行数，问题列表）。首行 `// CONTRACT owner=x layer=y`，其后为 `// 字段: 值`。"""
    fields: dict[str, str] = {}
    problems: list[str] = []
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if CONTRACT.match(line):
            start = index
            break
    if start is None:
        return fields, 0, ["缺少 // CONTRACT 块"]
    header = lines[start]
    for name in ("owner", "layer"):
        match = re.search(rf"\b{name}\s*=\s*(\S+)", header)
        if match:
            fields[name] = match.group(1).strip()
    block_lines = 1
    for line in lines[start + 1:]:
        stripped = line.strip()
        if not stripped:
            break
        match = FIELD.match(stripped)
        if match:
            fields[match.group(1)] = match.group(2).strip()
            block_lines += 1
            continue
        break
    for name in REQUIRED_FIELDS:
        value = fields.get(name, "")
        if not value:
            problems.append(f"缺少字段 {name}")
            continue
        if name in {"owner", "layer"}:
            if value.strip().lower() in PLACEHOLDERS:
                problems.append(f"字段 {name} 是占位值")
            continue
        if len(value) < MIN_VALUE or value.strip().lower() in PLACEHOLDERS:
            problems.append(f"字段 {name} 是占位值，必须写明实际约束")
    if any(RESTATED.search(value) for value in fields.values()):
        problems.append("头内重复了机检事实（依赖方向或 include 规则），改为引用检查名")
    if block_lines > MAX_BLOCK_LINES:
        problems.append(f"契约块 {block_lines} 行，超过 {MAX_BLOCK_LINES} 行上限")
    return fields, block_lines, problems


def check_file(path: Path, known_ctests: set[str]) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    fields, _, problems = parse_contract(text)
    if fields.get("tests"):
        for name in (item.strip() for item in fields["tests"].split(",")):
            if name and name not in known_ctests:
                problems.append(f"tests 引用了不存在的 CTest 用例：{name}")
    if fields.get("spec"):
        target = ROOT / fields["spec"]
        if not target.is_file():
            problems.append(f"spec 指向的文档不存在：{fields['spec']}")
    layer = fields.get("layer", "")
    if layer:
        if layer not in LAYERS:
            problems.append(f"layer={layer} 不是已知层")
        else:
            parts = path.relative_to(ROOT).parts
            if len(parts) > 1 and parts[1] != layer:
                problems.append(f"layer={layer} 与所在目录 {parts[1]}/ 不一致")
    return problems


def candidates() -> list[Path]:
    files = sorted(
        p for p in (ROOT / "src").rglob("*") if p.suffix in {".h", ".cpp", ".go"}
    )
    required = [ROOT / name for name in REQUIRED]
    optional = [p for p in files if CONTRACT.search(p.read_text(encoding="utf-8", errors="replace"))]
    return sorted({*required, *optional})


def self_test() -> int:
    good = (
        "// CONTRACT owner=demo layer=domain\n"
        "// invariant: 值对象不可变，跨线程只读\n"
        "// forbidden: 不得在此引入文件或网络访问\n"
        "// lifecycle: 仅作为纯值在栈上传递\n"
        "// tests: domain_contract_tests\n"
        "// spec: docs/engineering.md\n"
    )
    fields, _, problems = parse_contract(good)
    if problems or not fields:
        print(f"self-test: 合法块被误判：{problems}", file=sys.stderr)
        return 1
    missing = "// CONTRACT owner=demo layer=domain\n// invariant: 值对象不可变\n"
    if not parse_contract(missing)[2]:
        print("self-test: 缺字段未被发现", file=sys.stderr)
        return 1
    placeholder = (
        "// CONTRACT owner=demo layer=domain\n"
        "// invariant: x\n// forbidden: x\n// lifecycle: x\n// tests: x\n// spec: x\n"
    )
    if not parse_contract(placeholder)[2]:
        print("self-test: 占位值未被发现", file=sys.stderr)
        return 1
    restated = (
        "// CONTRACT owner=demo layer=domain\n"
        "// invariant: 允许依赖 core 模块\n"
        "// forbidden: 不得引入 I/O\n// lifecycle: 纯值传递\n"
        "// tests: domain_contract_tests\n// spec: docs/engineering.md\n"
    )
    if not parse_contract(restated)[2]:
        print("self-test: 重复机检事实未被发现", file=sys.stderr)
        return 1
    print("Contract headers self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()

    known = ctest_names()
    problems: list[str] = []
    for path in candidates():
        for problem in check_file(path, known):
            problems.append(f"{path.relative_to(ROOT).as_posix()}: {problem}")
    for problem in problems:
        print(problem, file=sys.stderr)
    total = len(candidates())
    print(f"Contract headers: {total} files checked, {len(problems)} findings")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())