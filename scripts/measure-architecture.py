#!/usr/bin/env python3
"""结构度量（架构优化项 P0）。

只读扫描源码与构建脚本，输出可复现的结构指标，供优化项结算与门禁使用。
口径定义见 docs/design/architecture-optimization-program.md §3.1；脚本改动
必须同步该节，否则指标失去意义。

用法：
  python scripts/measure-architecture.py            # 打印指标表
  python scripts/measure-architecture.py --json     # 打印机器可读结果
  python scripts/measure-architecture.py --check    # 与 BASELINE 比较，漂移即失败
返回码：0 正常；1 --check 发现漂移。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SOURCE_SUFFIXES = {".cpp", ".h", ".go"}

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests"))

from architecture_matrix import scan, scan_headers_without_impl  # noqa: E402

INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
LARGE_FILE_LINES = 600

# --check 的基线。由首次运行结果填入；任何改动必须附验证报告中的前后两次输出。
# 基线绑定工作树快照（2026-11-24，未提交状态）；提交后需在目标提交上重算并更新。
BASELINE: dict[str, int] = {
    "source_files": 310,
    "source_lines": 26362,
    "controller_header_lines": 444,
    "controller_public_prototypes": 119,
    "controller_public_inline": 4,
    "controller_signals": 21,
    "controller_private_prototypes": 57,
    "controller_private_inline": 3,
    "controller_pending_members": 16,
    "controller_generation_members": 9,
    "qjsonobject_rows": 418,
    "qjsonobject_files": 136,
    "qjsonobject_occurrences": 452,
    "cross_module_edges": 299,
    "layer_violations": 20,
    "controller_header_dependents": 39,
    "ctest_cases": 79,
    "executables": 42,
    "test_executables": 38,
    "tests_linking_ui": 26,
    "qobject_headers_without_impl": 0,
    "files_over_600_lines": 3,
    "lease_pending_calls": 2,
    "controller_header_closure": 30,
    "controller_direct_includes": 19,
    "error_codes_unwired": 6,
}


def source_files() -> list[Path]:
    return sorted(p for p in SRC.rglob("*") if p.suffix in SOURCE_SUFFIXES and p.is_file())


def count_lines(path: Path) -> int:
    try:
        return len(path.read_text(encoding="utf-8", errors="replace").splitlines())
    except OSError:
        return 0


def controller_metrics() -> dict[str, int]:
    path = SRC / "app" / "AppController.h"
    if not path.is_file():
        return {}
    lines = path.read_text(encoding="utf-8").splitlines()
    markers = [i for i, line in enumerate(lines) if re.match(r"^(public|signals|private|protected):", line.strip())]
    section_of = {}
    for index, marker in enumerate(markers):
        end = markers[index + 1] if index + 1 < len(markers) else len(lines)
        section_of[lines[marker].strip().rstrip(":")] = lines[marker + 1:end]
    public = section_of.get("public", [])
    private = section_of.get("private", [])
    signals = section_of.get("signals", [])
    declared = re.compile(r"^\s{4}[\w:<>,&*\s]+\w+\(.*\);\s*$")
    inline = re.compile(r"^\s{4}[\w:<>,&*\s]+\w+\(.*\)\s*\{.*\}\s*$")
    member = re.compile(r"^\s{4}[A-Za-z_][\w:<>,&*\s]*\s+[a-zA-Z]\w*_\s*(=.*)?;")
    return {
        "controller_header_lines": len(lines),
        "controller_public_prototypes": sum(1 for line in public if declared.match(line)),
        "controller_public_inline": sum(1 for line in public if inline.match(line)),
        "controller_signals": sum(1 for line in signals if re.match(r"^\s{4}void \w+\(", line)),
        "controller_private_prototypes": sum(1 for line in private if declared.match(line)),
        "controller_private_inline": sum(1 for line in private if inline.match(line)),
        "controller_pending_members": sum(
            1 for line in private if member.match(line) and "Pending" in line
        ),
        "controller_generation_members": sum(
            1 for line in private if member.match(line) and re.search(r"(Generation|Epoch|Serial)_", line)
        ),
    }


def json_metrics(files: list[Path]) -> dict[str, int]:
    rows = 0
    touched = 0
    occurrences = 0
    for path in files:
        text = path.read_text(encoding="utf-8", errors="replace")
        hits = text.count("QJsonObject")
        if hits:
            touched += 1
            occurrences += hits
            rows += sum(1 for line in text.splitlines() if "QJsonObject" in line)
    return {"qjsonobject_rows": rows, "qjsonobject_files": touched, "qjsonobject_occurrences": occurrences}


def include_edges(files: list[Path]) -> tuple[int, list[str]]:
    # 矩阵扫描统一由 tests/architecture_matrix.py 提供，避免两处各维护一份允许列表。
    return scan(ROOT)


def header_closure(entry: str) -> int:
    """从入口头出发、仅计 src/ 下可达的项目头数量（含间接包含）。

    include 解析顺序与编译器一致：先相对于所在目录，再相对于 src/。
    只按 src/ 根解析会漏掉同目录 include（例如 AppController.h 的 "HomeMetrics.h"）。
    """
    seen: set[str] = set()
    stack = [entry]
    while stack:
        rel = stack.pop()
        if rel in seen:
            continue
        seen.add(rel)
        path = ROOT / rel
        if not path.is_file():
            continue
        for included in INCLUDE.findall(path.read_text(encoding="utf-8", errors="replace")):
            candidates = [path.parent / included, SRC / included]
            for candidate in candidates:
                if candidate.is_file():
                    stack.append(candidate.relative_to(ROOT).as_posix())
                    break
    return len(seen) - 1


def direct_project_includes(header: str) -> int:
    """入口头直接 include 的项目头数量（不含 Qt 与标准库）。"""
    path = ROOT / header
    if not path.is_file():
        return 0
    count = 0
    for included in INCLUDE.findall(path.read_text(encoding="utf-8", errors="replace")):
        if (path.parent / included).is_file() or (SRC / included).is_file():
            count += 1
    return count


# 已登记但当前没有任何生产者的码值。按原则 6 作为债务登记：接线后必须从本表删除，
# 否则检查失败（防止"登记了就算做了"）。
EXPECTED_UNWIRED_CODES = {
    "DOWNLOAD_FAILED",
    "BUSY",
    "PORT_CONFLICT",
    "READ_ONLY",
    "CLEANUP_REQUIRED",
    "VERSION_CONFLICT",
}


def error_wiring(files: list[Path]) -> list[str]:
    """返回没有生产者的已登记码值（在 ErrorCategory.cpp 之外不出现）。"""
    registry = SRC / "domain" / "ErrorCategory.cpp"
    codes = re.findall(r'\{"([A-Z0-9_]+)",\s*ErrorCategory::', registry.read_text(encoding="utf-8"))
    unwired = []
    for code in codes:
        token = f'"{code}"'
        producers = sum(
            1
            for path in files
            if path != registry and token in path.read_text(encoding="utf-8", errors="replace")
        )
        if producers == 0:
            unwired.append(code)
    return sorted(unwired)


def debt_markers(files: list[Path]) -> dict[str, int]:
    """租约闸门直连点：应集中在 SessionGate 构造与适配器声明处（目标 ≤ 2）。"""
    calls = sum(
        sum(1 for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if "leasePending()" in line)
        for path in files
    )
    return {"lease_pending_calls": calls}


def dependents(files: list[Path], header: str) -> int:
    return sum(
        1
        for path in files
        if path.suffix == ".cpp" and f'#include "{header}"' in path.read_text(encoding="utf-8", errors="replace")
    )


def wiring_metrics() -> dict[str, int]:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    tests = re.findall(r"add_test\(\s*NAME\s+(\w+)", cmake)
    executables = re.findall(r"^\s*add_executable\(\s*(\w+)", cmake, re.MULTILINE)
    ui_linked = re.findall(r"target_link_libraries\(\s*(\w+_tests)\b[^)]*mlash_ui", cmake, re.DOTALL)
    return {
        "ctest_cases": len(tests),
        "executables": len(executables),
        "test_executables": sum(1 for name in executables if name.endswith("_tests")),
        "tests_linking_ui": len(set(ui_linked)),
    }


def module_coverage(files: list[Path]) -> dict[str, int]:
    # 模块覆盖判定同样复用共享矩阵的口径（纯接口白名单在 architecture_matrix 内）。
    missing = scan_headers_without_impl(ROOT)
    large = [
        path.relative_to(ROOT).as_posix()
        for path in files
        if path.suffix in {".cpp", ".h"} and count_lines(path) > LARGE_FILE_LINES
    ]
    return {
        "qobject_headers_without_impl": len(missing),
        "files_over_600_lines": len(large),
        "module_coverage_missing": missing,
        "module_coverage_large": large,
    }


def collect() -> dict[str, int]:
    files = source_files()
    metrics: dict[str, int] = {
        "source_files": len(files),
        "source_lines": sum(count_lines(p) for p in files),
    }
    metrics.update(controller_metrics())
    metrics.update(json_metrics(files))
    edges, violations = include_edges(files)
    metrics["cross_module_edges"] = edges
    metrics["layer_violations"] = len(violations)
    metrics["controller_header_dependents"] = dependents(files, "app/AppController.h")
    metrics.update(wiring_metrics())
    metrics.update(debt_markers(files))
    metrics["controller_header_closure"] = header_closure("src/app/AppController.h")
    metrics["controller_direct_includes"] = direct_project_includes("src/app/AppController.h")
    metrics["error_codes_unwired"] = len(error_wiring(files))
    coverage = module_coverage(files)
    metrics["qobject_headers_without_impl"] = coverage["qobject_headers_without_impl"]
    metrics["files_over_600_lines"] = coverage["files_over_600_lines"]
    metrics["_layer_violations_detail"] = violations  # type: ignore[assignment]
    metrics["_module_coverage_missing"] = coverage["module_coverage_missing"]  # type: ignore[assignment]
    metrics["_large_files"] = coverage["module_coverage_large"]  # type: ignore[assignment]
    return metrics


def printable(metrics: dict[str, int]) -> list[tuple[str, str]]:
    rows = []
    for key, value in metrics.items():
        if key.startswith("_"):
            continue
        rows.append((key, f"{value:,}"))
    return rows


# 债务指标：只能下降，超过基线即失败。其余指标是快照，必须精确相等。
# 纳入债务的判据：该项增长必然意味着可观测复杂度上升，且不存在"合法增长"。
# 反例：controller_header_lines 与 cross_module_edges 会随正常接线波动（新增一个声明、
# 新增一条允许方向的 include），因此只作快照，真正的债务是 layer_violations 与公开命令面。
DEBT_METRICS = {
    "layer_violations",
    "tests_linking_ui",
    "qobject_headers_without_impl",
    "files_over_600_lines",
    "controller_header_dependents",
    "qjsonobject_rows",
    "qjsonobject_files",
    "controller_pending_members",
    "controller_generation_members",
    "controller_public_prototypes",
    "controller_private_prototypes",
    "lease_pending_calls",
    "controller_header_closure",
    "controller_direct_includes",
    "error_codes_unwired",
}


def compare(baseline: dict[str, int], actual: dict[str, int]) -> list[str]:
    """返回漂移描述；空列表表示与基线一致或债务指标下降。"""
    drift: list[str] = []
    for key in sorted(set(baseline) ^ set(actual)):
        side = "缺少指标" if key in baseline else "新增指标未登记基线"
        drift.append(f"{key}: {side}")
    for key, expected in baseline.items():
        found = actual.get(key)
        if found is None:
            continue
        if key in DEBT_METRICS:
            if found > expected:
                drift.append(f"{key}: 债务指标只能下降 baseline={expected} actual={found}")
        elif found != expected:
            drift.append(f"{key}: baseline={expected} actual={found}")
    return drift


def self_test() -> int:
    """验证比较器：快照必须相等、债务只许下降、键集合必须一致。"""
    sample = {"a": 1, "layer_violations": 5}
    if compare(sample, {"a": 1, "layer_violations": 5}):
        print("self-test: 一致值被误判为漂移", file=sys.stderr)
        return 1
    if compare(sample, {"a": 1, "layer_violations": 4}):
        print("self-test: 债务下降被误判为漂移", file=sys.stderr)
        return 1
    if not compare(sample, {"a": 1, "layer_violations": 6}):
        print("self-test: 债务上升未被发现", file=sys.stderr)
        return 1
    if not compare(sample, {"a": 2, "layer_violations": 5}):
        print("self-test: 快照变化未被发现", file=sys.stderr)
        return 1
    if not compare(sample, {"a": 1}):
        print("self-test: 键集合缺失未被发现", file=sys.stderr)
        return 1
    print("Architecture metrics self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="输出 JSON")
    parser.add_argument("--check", action="store_true", help="与基线比较，漂移即失败")
    parser.add_argument("--details", action="store_true", help="列出明细")
    parser.add_argument("--self-test", action="store_true", help="验证比较器")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    metrics = collect()
    numeric = {k: v for k, v in metrics.items() if isinstance(v, int)}

    if args.json:
        print(json.dumps(metrics, ensure_ascii=False, indent=2))
        return 0

    if args.check:
        if not BASELINE:
            print("measure-architecture: BASELINE 未设置，先运行 --json 后填入脚本", file=sys.stderr)
            return 1
        drift = compare(BASELINE, numeric)
        # 码值接线集合必须精确匹配：接线后不清债务表同样算漂移。
        unwired = set(error_wiring(source_files()))
        if unwired != EXPECTED_UNWIRED_CODES:
            for code in sorted(unwired - EXPECTED_UNWIRED_CODES):
                drift.append(f"{code}: 新出现无生产者的码值，请接线或登记")
            for code in sorted(EXPECTED_UNWIRED_CODES - unwired):
                drift.append(f"{code}: 已接线但仍登记在欠债表，请删除")
        for line in drift:
            print(line, file=sys.stderr)
        print(f"Architecture metrics: {len(drift)} drift")
        return 1 if drift else 0

    width = max(len(key) for key, _ in printable(metrics))
    for key, value in printable(metrics):
        print(f"{key.ljust(width)}  {value}")
    if args.details:
        for key in ("_layer_violations_detail", "_module_coverage_missing", "_large_files"):
            entries = metrics.get(key) or []
            print(f"\n{key.lstrip('_')} ({len(entries)}):")
            for entry in entries:
                print(f"  {entry}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())