#!/usr/bin/env python3
"""UI token 检查（原则 11：样式只能来自设计层）。

扫描 src/ui 与 src/app 下的 C++ 源，报告三类「绕过 UiTheme 的硬编码」：

  1. 字面色值      QColor("#rrggbb") / QColor(Qt::white) / QColor(0x...) / "#rrggbb" 字符串
  2. 代码设定字号  setPointSize / setPixelSize / font-size: <n>px|pt 的内联 QSS
  3. 内联样式表    setStyleSheet(...)（应改为 setProperty + QSS 状态选择器）

豁免：设计特化层（src/ui/design 整个目录）、主题与图标实现文件、tests/。
白名单只登记存在的文件；文件被删除时必须同步删除条目，否则 --self-test 会失败。

用法：
  python scripts/check_ui_tokens.py            # 打印命中并返回码
  python scripts/check_ui_tokens.py --files a.cpp b.cpp
  python scripts/check_ui_tokens.py --self-test
返回码：0 无命中；1 有命中或自检失败。
"""
import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = [ROOT / "src" / "ui", ROOT / "src" / "app"]
# 设计特化层整体豁免：token 的唯一定义处。
EXEMPT_DIRS = (ROOT / "src" / "ui" / "design",)
# 语义取值与 Qt 外观适配器：这些文件就是把 token 变成 Qt 值的地方。
EXEMPT_FILES = (
    "UiTheme.h", "UiTheme.cpp",
    "UiIcons.h", "UiIcons.cpp",
    "WidgetStyle.h", "WidgetStyle.cpp",
    "NodePresentation.h", "NodePresentation.cpp",
    "ProxyTableModel.h", "ProxyTableModel.cpp",
)
# 二维码必须黑白高反差，不随主题：功能性例外。
EXEMPT_LINES = {
    "MainWindowProfiles.cpp": re.compile(r"qrcodegen|fill\(Qt::white\)|Qt::black"),
}

LITERAL_COLOR = re.compile(
    r'(?:QColor\s*\(\s*(?:QStringLiteral\s*\(\s*)?"#[0-9a-fA-F]{3,8}"\s*\))'
    r'|(?:QColor\s*\(\s*Qt::(?:white|black|red|green|blue|gray|darkGray|lightGray)\s*\))'
    r'|(?:QColor\s*\(\s*0x[0-9a-fA-F]+\s*\))'
)
LITERAL_HEX = re.compile(r'"(#[0-9a-fA-F]{6}|#[0-9a-fA-F]{8})"')
FONT_SIZE = re.compile(r"set(?:Point|Pixel)Size\s*\(\s*([A-Za-z_][A-Za-z_0-9]*|\d+)\s*\)|font-size:\s*(\d+(?:px|pt)|@?[A-Za-z]+)")
FOCUS_PADDING = re.compile(r"padding[^;]*;.*border[^;]*@Focus|@Focus[^;]*;.*padding")
INLINE_STYLESHEET = re.compile(r"setStyleSheet\s*\(")

PATTERNS = (
    ("literal-color", LITERAL_COLOR),
    ("literal-hex", LITERAL_HEX),
    ("font-size", FONT_SIZE),
    ("focus-padding", FOCUS_PADDING),
    ("inline-stylesheet", INLINE_STYLESHEET),
)


def exempt(path: Path, line: str) -> bool:
    if path.name in EXEMPT_FILES:
        return True
    if any(directory in path.parents for directory in EXEMPT_DIRS):
        return True
    if "tests" in path.parts:
        return True
    for name, pattern in EXEMPT_LINES.items():
        if path.name == name and pattern.search(line):
            return True
    return False


def default_paths() -> list[Path]:
    paths: list[Path] = []
    for directory in SCAN_DIRS:
        if directory.is_dir():
            paths.extend(sorted(p for p in directory.rglob("*") if p.suffix in {".cpp", ".h"}))
    return paths


def scan(paths: list[Path]) -> list[tuple[Path, int, str, str]]:
    hits = []
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for number, line in enumerate(text.splitlines(), start=1):
            if exempt(path, line):
                continue
            for label, pattern in PATTERNS:
                match = pattern.search(line)
                if match:
                    hits.append((path, number, label, match.group(0)))
    return hits


def self_test() -> int:
    """验证白名单条目全部存在，且扫描能命中、能豁免。"""
    missing = [name for name in EXEMPT_FILES if not any((d / name).is_file() for d in SCAN_DIRS)]
    if missing:
        print("self-test: 白名单包含不存在的文件：" + ", ".join(missing), file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        sample = Path(tmp) / "Sample.cpp"
        sample.write_text('auto c = QColor("#ff0000");\n', encoding="utf-8")
        if not scan([sample]):
            print("self-test: 未命中字面色值", file=sys.stderr)
            return 1
        exempt_sample = Path(tmp) / "UiTheme.cpp"
        exempt_sample.write_text('auto c = QColor("#ff0000");\n', encoding="utf-8")
        if scan([exempt_sample]):
            print("self-test: 白名单文件未被豁免", file=sys.stderr)
            return 1
    print("UI tokens self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--files", nargs="*", help="以显式文件列表代替默认扫描范围")
    parser.add_argument("--self-test", action="store_true", help="检查白名单有效性与扫描能力")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    paths = [Path(name) for name in args.files] if args.files else default_paths()
    hits = scan(paths)
    for path, number, label, text in hits:
        try:
            shown = path.relative_to(ROOT).as_posix()
        except ValueError:
            shown = str(path)
        print(f"{shown}:{number}: {label}: {text}")
    print(f"{len(hits)} hits")
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())