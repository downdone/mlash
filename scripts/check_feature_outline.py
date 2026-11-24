#!/usr/bin/env python3
"""功能大纲与界面容器双向映射检查（原则 12，优化项 G）。

规则：
  1. `docs/requirements-traceability.md` 的「主容器映射」表每行必须给出一个容器标识。
  2. 容器标识必须来自 `src/ui/ContainerIds.h` 的登记值，或显式写 `infra`（非界面能力）。
  3. 每个登记容器要么被至少一个功能引用，要么列入 RESERVED 并写明原因。两侧都不占的容器视为孤儿。
  4. RESERVED 中的容器一旦被功能引用，必须从 RESERVED 删除。

用法：
  python scripts/check_feature_outline.py
  python scripts/check_feature_outline.py --self-test
返回码：0 通过；1 有违例。
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "src" / "ui" / "ContainerIds.h"
VIEW_IDS = ROOT / "src" / "ui" / "ViewIds.h"
OUTLINE = ROOT / "docs" / "requirements-traceability.md"

CONSTANT = re.compile(r'inline\s+constexpr\s+char\s+(\w+)\[\]\s*=\s*"([^"]+)"\s*;')
ROW = re.compile(r"^\|\s*(F-[A-Z0-9-]+)\s*\|\s*`?([a-z:_-]+)`?\s*\|", re.MULTILINE)
FEATURE_ROW = re.compile(r"^\|\s*(F-[A-Z0-9-]+)\s*\|", re.MULTILINE)
VIEW = re.compile(r"^inline\s+constexpr\s+int\s+(\w+)\s*=\s*(-?\d+)\s*;", re.MULTILINE)
DATA_TAB = re.compile(r"enum\s+class\s+DataTab\s*\{([^}]*)\}")

# 已登记但尚未被功能引用的容器。每条必须说明去向。
# G1 已把 U40 的 32 条任务并入主容器映射，对应条目在合并时逐条删除。
# 剩余两类：结构性父容器（由下属页签/任务承载），以及 U40 清单未收录的隧道工作区。
RESERVED: dict[str, str] = {
    "view:home": "结构性父容器：首页视图由 proxy:run 与 proxy:members 承载，功能引用这两个工作区页",
    "view:overview": "U40 提案把总览并入连接页，35 条任务无总览条目；当前总览只做跨链路只读摘要，去留待决",
    "view:data": "结构性父容器：数据视图由流量、连接、测试、日志四个页签承载",
    "view:tunnel": "结构性父容器：隧道视图由 tunnel:run 与 tunnel:members 承载",
    "tunnel:run": "U40 清单未收录隧道运行页，双内核设计另行维护，待新增编号",
    "tunnel:members": "U40 清单未收录 Peer 列表，双内核设计另行维护，待新增编号",
}


def registry_values(text: str) -> dict[str, str]:
    """返回（标识 → 常量名）。"""
    return {value: name for name, value in CONSTANT.findall(text)}


def outline_rows(text: str) -> list[tuple[str, str]]:
    section = text.split("## 主容器映射", 1)
    if len(section) < 2:
        return []
    return [(code, container.strip("`")) for code, container in ROW.findall(section[1])]


def declared_features(text: str) -> list[str]:
    """需求主表里的全部功能编号（位于「主容器映射」之前）。"""
    head = text.split("## 主容器映射", 1)[0]
    return FEATURE_ROW.findall(head)


def view_ids(text: str) -> tuple[list[str], list[str]]:
    """返回（视图常量名，数据页签成员名）。"""
    names = [name for name, _ in VIEW.findall(text) if name not in {"DataTabCount", "Count"}]
    tabs: list[str] = []
    match = DATA_TAB.search(text)
    if match:
        for item in match.group(1).split(","):
            member = item.split("=", 1)[0].strip()
            if member:
                tabs.append(member)
    return names, tabs


def check(registry_text: str, outline_text: str, view_text: str, reserved: dict[str, str] | None = None) -> list[str]:
    reserved = RESERVED if reserved is None else reserved
    problems: list[str] = []
    values = registry_values(registry_text)
    if not values:
        return ["ContainerIds.h 未解析到任何登记标识"]
    rows = outline_rows(outline_text)
    if not rows:
        return ["需求映射缺少「主容器映射」表或表为空"]

    # 双向：主表每个功能恰好一行映射，映射不得出现主表之外的功能。
    declared = declared_features(outline_text)
    if not declared:
        return ["需求主表未解析到任何功能编号"]
    mapped = [code for code, _ in rows]
    for code in declared:
        if code not in mapped:
            problems.append(f"{code}: 需求主表有该功能，但「主容器映射」缺行")
        elif mapped.count(code) > 1:
            problems.append(f"{code}: 主容器映射重复")
    for code in mapped:
        if code not in declared:
            problems.append(f"{code}: 映射出现在主表之外，请先登记功能")

    # 注册表与代码绑定：视图常量与数据页签必须各自有对应标识。
    views, tabs = view_ids(view_text)
    if not views:
        problems.append("ViewIds.h 未解析到视图常量")
    for name in views:
        expected = f"view:{name.lower()}"
        if expected not in values:
            problems.append(f"{expected}: ViewIds.h 有 {name} 视图，但注册表缺对应标识")
    for value in values:
        if value.startswith("view:") and f"view:{value[5:]}" in {f"view:{n.lower()}" for n in views}:
            continue
        if value.startswith("view:") and views:
            problems.append(f"{value}: 注册表声明了该视图，但 ViewIds.h 中没有对应常量")
    for name in tabs:
        expected = f"data:{name.lower()}"
        if expected not in values:
            problems.append(f"{expected}: ViewIds.h 有 {name} 页签，但注册表缺对应标识")

    for code, container in rows:
        if container not in values and container != "infra":
            problems.append(f"{code}: 未登记的容器标识 {container}")
        if container in reserved:
            problems.append(f"{code}: 引用了仍标记为预留的容器 {container}，请从 RESERVED 删除登记")

    referenced = {container for _, container in rows}
    for value in sorted(values):
        if value == "infra" or value in referenced:
            continue
        if value not in reserved:
            problems.append(f"{value}: 孤儿容器，既无功能引用也未登记原因")

    for value, reason in sorted(reserved.items()):
        if value not in values:
            problems.append(f"{value}: RESERVED 中的容器未在 ContainerIds.h 登记")
        if value in referenced:
            problems.append(f"{value}: 已被功能引用，必须从 RESERVED 删除")
        if len(reason.strip()) < 8:
            problems.append(f"{value}: RESERVED 理由过于空泛（需写明 U40 编号或去向）")
    return problems


def self_test() -> int:
    registry = 'inline constexpr char A[] = "view:a";\ninline constexpr char I[] = "infra";\n'
    views = "inline constexpr int A = 0;\ninline constexpr int Count = 1;\n"
    good = "| F-1 | x |\n\n## 主容器映射\n\n| 功能 | 主容器 |\n|---|---|\n| F-1 | `view:a` |\n"
    if check(registry, good, views, reserved={}):
        print(f"self-test: 合法映射被误判：{check(registry, good, views, reserved={})}", file=sys.stderr)
        return 1
    unknown = "| F-1 | x |\n\n## 主容器映射\n\n| 功能 | 主容器 |\n|---|---|\n| F-1 | `view:missing` |\n"
    if not check(registry, unknown, views, reserved={}):
        print("self-test: 未拦下未登记容器", file=sys.stderr)
        return 1
    # 主表有功能但映射缺行 —— 这是上一版漏掉的方向。
    missing_row = "| F-1 | x |\n| F-2 | y |\n\n## 主容器映射\n\n| 功能 | 主容器 |\n|---|---|\n| F-1 | `view:a` |\n"
    if not check(registry, missing_row, views, reserved={}):
        print("self-test: 未发现缺失映射行", file=sys.stderr)
        return 1
    # 注册表与 ViewIds 不一致。
    if not check(registry, good, "inline constexpr int B = 0;\ninline constexpr int Count = 1;\n", reserved={}):
        print("self-test: 未发现注册表与 ViewIds 不一致", file=sys.stderr)
        return 1
    print("Feature outline self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()

    problems = check(
        REGISTRY.read_text(encoding="utf-8", errors="replace"),
        OUTLINE.read_text(encoding="utf-8", errors="replace"),
        VIEW_IDS.read_text(encoding="utf-8", errors="replace"),
    )
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"Feature outline: {'OK' if not problems else f'{len(problems)} problems'}")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())