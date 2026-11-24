"""架构边界检查（原则 2）。

检查项：
  1. 目录依赖矩阵：不在允许集合内的跨目录 include 必须失败；已登记偏差只减不增。
  2. domain 不依赖任何上层目录。
  3. 迁移期废弃头不得复现。
  4. 应用层不得绕过适配器直接发原始核心请求。
  5. UI 头不得包含控制器实现头。
  6. 核心能力消费者不得依赖具体适配器。
  7. 调色板字面值只能出现在设计特化层。
  8. 引擎集成测试不得依赖 UI。

用法：
  python tests/test_architecture_boundaries.py            # 检查
  python tests/test_architecture_boundaries.py --self-test # 自检（负向用例）
"""
from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from architecture_matrix import (  # noqa: E402
    KNOWN_VIOLATIONS,
    PUBLIC_DIRS,
    scan,
)

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
SOURCE_SUFFIXES = {".cpp", ".h"}


def sources(tree: Path):
    return sorted(p for p in tree.rglob("*") if p.suffix in SOURCE_SUFFIXES and p.is_file())


def check_matrix() -> list[str]:
    problems: list[str] = []
    _, violations = scan(ROOT)
    current = set(violations)
    known = set(KNOWN_VIOLATIONS)
    for entry in sorted(current - known):
        problems.append(f"新增越层依赖（未登记）：{entry}")
    for entry in sorted(known - current):
        problems.append(f"已消除的越层依赖仍留在白名单，请删除：{entry}")
    return problems


def check_domain() -> list[str]:
    problems = []
    for source in sources(ROOT / "src" / "domain"):
        for name in INCLUDE.findall(source.read_text(encoding="utf-8-sig")):
            if name.split("/", 1)[0] in PUBLIC_DIRS - {"domain"}:
                problems.append(f"{source.relative_to(ROOT).as_posix()}: domain 依赖上层 {name}")
    return problems


def check_obsolete() -> list[str]:
    obsolete = {
        "app/Types.h",
        "app/ConnectionColumns.h",
        "ui/MainWindowParts.h",
        "MainWindowParts.h",
        "WindowUiBasics.h",
        "WindowUiIncludes.h",
    }
    problems = []
    for tree in (ROOT / "src", ROOT / "tests"):
        for source in sources(tree):
            for name in INCLUDE.findall(source.read_text(encoding="utf-8-sig")):
                if name in obsolete:
                    problems.append(f"{source.relative_to(ROOT).as_posix()}: 废弃契约头 {name}")
    return problems


def check_raw_core_request() -> list[str]:
    problems = []
    for source in sorted((ROOT / "src" / "app").glob("*.cpp")):
        if re.search(r"\bapi_(?:\.|->)request\s*\(", source.read_text(encoding="utf-8")):
            problems.append(f"{source.relative_to(ROOT).as_posix()}: 绕过适配器直接发原始请求")
    return problems


def check_ui_headers() -> list[str]:
    problems = []
    for source in sorted((ROOT / "src" / "ui").glob("*.h")):
        if "app/AppController.h" in INCLUDE.findall(source.read_text(encoding="utf-8-sig")):
            problems.append(f"{source.relative_to(ROOT).as_posix()}: UI 头包含控制器实现")
    return problems


def check_capability_consumers() -> list[str]:
    paths = (
        "src/core/CoreManager.h",
        "src/app/HomeMetrics.h",
        "src/app/ConnectivityProbe.h",
        "src/app/HomeNodeProbe.h",
        "src/app/WebsiteTests.h",
    )
    problems = []
    for name in paths:
        path = ROOT / name
        if not path.is_file():
            continue
        imports = INCLUDE.findall(path.read_text(encoding="utf-8-sig"))
        if any(item.endswith("MihomoClient.h") for item in imports):
            problems.append(f"{name}: 能力消费者依赖具体适配器")
    return problems


def check_palette() -> list[str]:
    theme = (ROOT / "src" / "ui" / "UiTheme.cpp").read_text(encoding="utf-8")
    if "void applyAppearance(" not in theme or "void install()" not in theme:
        return ["src/ui/UiTheme.cpp: 结构变化，调色板检查需要更新"]
    body = theme.split("void applyAppearance(", 1)[1].split("void install()", 1)[0]
    if re.search(r'"#[0-9a-fA-F]{6}"', body):
        return ["src/ui/UiTheme.cpp: 调色板字面值出现在设计特化层之外"]
    return []


def check_engine_test() -> list[str]:
    path = ROOT / "tests" / "EngineTests.cpp"
    if not path.is_file():
        return []
    imports = INCLUDE.findall(path.read_text(encoding="utf-8-sig"))
    if any(item.startswith("ui/") for item in imports):
        return ["tests/EngineTests.cpp: 引擎集成目标依赖 UI"]
    return []


CHECKS = (
    ("目录矩阵", check_matrix),
    ("domain 依赖方向", check_domain),
    ("废弃契约头", check_obsolete),
    ("原始核心请求", check_raw_core_request),
    ("UI 头包含控制器", check_ui_headers),
    ("能力消费者", check_capability_consumers),
    ("调色板字面值", check_palette),
    ("引擎测试依赖", check_engine_test),
)


def run_checks() -> list[str]:
    problems: list[str] = []
    for _, check in CHECKS:
        problems.extend(check())
    return problems


def self_test() -> int:
    """构造最小目录树，验证矩阵能拦住越层、放过允许边。"""
    with tempfile.TemporaryDirectory() as tmp:
        tree = Path(tmp)
        (tree / "src" / "core").mkdir(parents=True)
        (tree / "src" / "ui").mkdir(parents=True)
        (tree / "src" / "core" / "Allowed.cpp").write_text(
            '#include "domain/Types.h"\n', encoding="utf-8"
        )
        (tree / "src" / "core" / "Forbidden.cpp").write_text(
            '#include "ui/MainWindow.h"\n', encoding="utf-8"
        )
        # 相对路径与尖括号写法必须同样被拦下，否则矩阵形同虚设。
        (tree / "src" / "core" / "Relative.cpp").write_text(
            '#include "../ui/MainWindow.h"\n', encoding="utf-8"
        )
        (tree / "src" / "core" / "Angle.cpp").write_text(
            '#include <ui/MainWindow.h>\n', encoding="utf-8"
        )
        _, violations = scan(tree)
        for name in ("Forbidden.cpp", "Relative.cpp", "Angle.cpp"):
            if not any(name in item for item in violations):
                print(f"self-test: 未拦住 core -> ui 越层（{name}）", file=sys.stderr)
                return 1
        if any("Allowed.cpp" in item for item in violations):
            print("self-test: 误报 core -> domain 允许边", file=sys.stderr)
            return 1
    print("Architecture boundaries self-test: OK")
    return 0


def main() -> int:
    if "--self-test" in sys.argv[1:]:
        return self_test()
    problems = run_checks()
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"Architecture boundaries: {'OK' if not problems else f'{len(problems)} problems'}")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())