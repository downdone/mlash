"""目录依赖矩阵（唯一来源）。

被 tests/test_architecture_boundaries.py 与 scripts/measure-architecture.py 共同引用，
避免两处各自维护一份允许列表。改动此处必须同步 docs/engineering.md 的依赖说明。

矩阵是"策略"，不是现状描述：现状偏差登记在 KNOWN_VIOLATIONS，只减不增。
"""
from __future__ import annotations

import os
import re
from pathlib import Path

# 目录 → 允许依赖的目录。
# diagnostics 是横切服务（有界日志与进程度量），各层可向其写入，但它不得反向依赖 app/ui；
# storage 提供加密与原子写原语，位于 platform 之上、core/profile 之下；界面可依赖全部下层。
ALLOWED: dict[str, set[str]] = {
    "domain": set(),
    "common": {"domain"},
    "platform": {"domain", "common", "diagnostics"},
    "storage": {"domain", "platform", "common", "diagnostics"},
    "core": {"domain", "platform", "storage", "common", "diagnostics"},
    "profile": {"domain", "platform", "storage", "common", "diagnostics"},
    "diagnostics": {"domain", "platform", "common"},
    "app": {"domain", "platform", "storage", "core", "profile", "diagnostics", "common"},
    "ui": {"domain", "platform", "storage", "core", "profile", "diagnostics", "app", "common"},
}
PUBLIC_DIRS = set(ALLOWED)

# 声明 Q_OBJECT 但无同名实现的纯接口头（附理由，按原则 9 不属于缺模块）。
INTERFACE_HEADERS: dict[str, str] = {
    "src/platform/GlobalHotkeys.h": "纯虚能力接口，由平台适配器实现",
}

# 存量越层边。每条必须写明理由与消除它的优化项；条目只能减少不能新增。
# 消除后必须从本表删除，否则检查会报"已消除但未登记"。
KNOWN_VIOLATIONS: dict[str, str] = {
    "src/core/CoreUpdate.h -> profile/ProfileStore.h": "暂留：PreparedProfile 下沉 domain 后消除",
    "src/core/OfficialCoreSource.h -> profile/SubscriptionDownloader.h": "暂留：下载策略下沉后消除",
    "src/core/TunPolicy.cpp -> profile/ConfigBuilder.h": "暂留：TunPolicy 迁入 domain 后消除",
    "src/core/TunSession.cpp -> app/Async.h": "由公共层搬迁消除（Async 迁出 app）",
    "src/core/WireGuardSession.cpp -> app/Async.h": "由公共层搬迁消除（Async 迁出 app）",
    "src/profile/SubscriptionDownloader.h -> core/MihomoClient.h": "暂留：需先抽出下载传输契约",
    "src/profile/WebDavClient.cpp -> app/Async.h": "由公共层搬迁消除（Async 迁出 app）",
    "src/platform/ProxyCoordinator.cpp -> storage/Storage.h": "暂留：原子写原语需下沉 common",
    "src/platform/windows/TunNative.cpp -> core/TunPolicy.h": "暂留：TunPolicy 迁入 domain 后消除",
    "src/platform/windows/TunNative.cpp -> storage/Storage.h": "暂留：原子写原语需下沉 common",
    "src/platform/windows/TunServiceMain.cpp -> core/TunApiGate.h": "暂留：服务端需复用 API 网关契约",
    "src/platform/windows/TunServiceMain.cpp -> core/TunPolicy.h": "暂留：TunPolicy 迁入 domain 后消除",
    "src/platform/windows/TunServiceMain.cpp -> storage/Storage.h": "暂留：原子写原语需下沉 common",
    "src/platform/windows/WireGuardService.cpp -> profile/WireGuardConfig.h": "暂留：WireGuard 配置解析需下沉 domain",
    "src/platform/windows/WireGuardService.cpp -> storage/Storage.h": "暂留：原子写原语需下沉 common",
    "src/diagnostics/QualificationRun.cpp -> app/Async.h": "由公共层搬迁消除（Async 迁出 app）",
    "src/diagnostics/QualificationRun.cpp -> storage/Storage.h": "暂留：资格报告写入改为注入回调后消除",
    "src/diagnostics/QualificationRun.h -> app/AppController.h": "暂缓：资格运行编排归 app，待 E 完成后重评",
    "src/diagnostics/QualificationRun.h -> ui/GuiHost.h": "暂缓：同上，待 E 完成后重评",
    "src/diagnostics/QualificationRun.h -> ui/TrayController.h": "暂缓：同上，待 E 完成后重评",
}

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
SOURCE_SUFFIXES = {".cpp", ".h", ".go"}


def source_files(root: Path) -> list[Path]:
    return sorted(
        p for p in (root / "src").rglob("*") if p.suffix in SOURCE_SUFFIXES and p.is_file()
    )


def resolve_include(source: Path, target: str, root: Path) -> Path | None:
    """按编译器顺序解析 include：先相对所在目录，再相对 src/。

    必须归一化 `../` 与 `<>` 形式，否则 `#include "../ui/X.h"` 一类写法能绕过目录矩阵。
    """
    candidates = [source.parent / target, root / "src" / target]
    for candidate in candidates:
        try:
            normalized = Path(os.path.normpath(candidate))
        except (OSError, ValueError):
            continue
        if normalized.is_file():
            return normalized
    return None


def module_of_path(path: Path, root: Path) -> str | None:
    try:
        parts = path.resolve().relative_to(root.resolve()).parts
    except (ValueError, OSError):
        return None
    if len(parts) < 3 or parts[0] != "src":
        return None
    return parts[1] if parts[1] in PUBLIC_DIRS else None


def scan(root: Path) -> tuple[int, list[str]]:
    """返回（跨目录 include 边数，越层边清单）。"""
    edges = 0
    violations: list[str] = []
    for path in source_files(root):
        source = module_of_path(path, root)
        if source is None:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for target in INCLUDE.findall(text):
            resolved = resolve_include(path, target, root)
            if resolved is not None:
                head = module_of_path(resolved, root)
            else:
                # 目标文件不存在时退回首段启发式：测试树或未来头文件也要能被判层。
                first = target.split("/", 1)[0]
                if "../" in target or target.startswith("."):
                    head = module_of_path(Path(os.path.normpath(path.parent / target)), root)
                else:
                    head = first if first in PUBLIC_DIRS else None
            if head is None or head == source:
                continue
            edges += 1
            if head not in ALLOWED[source]:
                violations.append(f"{path.relative_to(root).as_posix()} -> {target}")
    return edges, sorted(violations)


def scan_headers_without_impl(root: Path) -> list[str]:
    """声明 Q_OBJECT 但没有同名 .cpp 的头文件（纯接口白名单除外）。"""
    missing: list[str] = []
    for path in source_files(root):
        if path.suffix != ".h":
            continue
        if "Q_OBJECT" not in path.read_text(encoding="utf-8", errors="replace"):
            continue
        relative = path.relative_to(root).as_posix()
        if relative in INTERFACE_HEADERS:
            continue
        if not (path.parent / (path.stem + ".cpp")).is_file():
            missing.append(relative)
    return sorted(missing)