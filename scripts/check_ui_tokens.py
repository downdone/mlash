#!/usr/bin/env python3
"""UI token 检查（界面分层设计 6.3 规则 7）。

扫描 src/ui 与 src/app 下的 C++ 源，报告三类「绕过 UiTheme 的硬编码」：

  1. 字面色值      QColor("#rrggbb") / QColor(Qt::white) / QColor(0x...) / "#rrggbb" 字符串
  2. 代码设定字号  setPointSize / setPixelSize / font-size: <n>px|pt 的内联 QSS
  3. padding 防跳  QSS 里以 padding 变化代替焦点环的写法

白名单（6.3 规则 7）：UiTheme.*、UiIcons.*、WidgetStyle.*、NodePresentation.*、
MainWindowParts.*、CompactComboBox.*、ProxyTableModel.*、tests/ 全部豁免；另有
MainWindowProfiles.cpp 的二维码黑白单行豁免。

用法：
  python scripts/check_ui_tokens.py            # 打印命中并返回码
  python scripts/check_ui_tokens.py --files a.cpp b.cpp
返回码：0 无命中；1 有命中。
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = [ROOT / "src" / "ui", ROOT / "src" / "app"]
EXEMPT_FILES = (
    "UiTheme.h", "UiTheme.cpp",
    "UiIcons.h", "UiIcons.cpp",
    "WidgetStyle.h", "WidgetStyle.cpp",
    "NodePresentation.h", "NodePresentation.cpp",
    "MainWindowParts.h",
    "CompactComboBox.h", "CompactComboBox.cpp",
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
# 每次刷新用 setStyleSheet 覆盖样式：应改为 setProperty + QSS 状态选择器。
INLINE_STYLESHEET = re.compile(r"setStyleSheet\s*\(")


def exempt(path: Path, line: str) -> bool:
    if path.name in EXEMPT_FILES:
        return True
    if "tests" in path.parts:
        return True
    for name, pattern in EXEMPT_LINES.items():
        if path.name == name and pattern.search(line):
            return True
    return False


def scan(paths):
    hits = []
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for number, line in enumerate(text.splitlines(), start=1):
            if exempt(path, line):
                continue
            for label, pattern in (
                ("literal-color", LITERAL_COLOR),
                ("literal-hex", LITERAL_HEX),
                ("font-size", FONT_SIZE),
                ("focus-padding", FOCUS_PADDING),
                ("inline-stylesheet", INLINE_STYLESHEET),
            ):
                match = pattern.search(line)
                if match:
                    hits.append((path, number, label, match.group(0)))
    return hits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--files", nargs="*", help="以显式文件列表代替默认扫描范围")
    args = parser.parse_args()
    if args.files:
        paths = [Path(name) for name in args.files]
    else:
        paths = sorted(p for d in SCAN_DIRS if d.exists() for p in d.glob("*.cpp")) + \
                sorted(p for d in SCAN_DIRS if d.exists() for p in d.glob("*.h"))
    hits = scan(paths)
    for path, number, label, text in hits:
        rel = path.relative_to(ROOT) if path.is_absolute() and ROOT in path.parents else path
        print(f"{rel}:{number}: {label}: {text}")
    print(f"\n{len(hits)} hits")
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
