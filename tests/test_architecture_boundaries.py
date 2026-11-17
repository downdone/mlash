"""Guard the dependency direction established by ADR 0055."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)


def main() -> int:
    problems = []
    for source in (ROOT / "src/domain").rglob("*"):
        if source.suffix not in {".h", ".cpp"}:
            continue
        for name in INCLUDE.findall(source.read_text(encoding="utf-8")):
            if name.startswith(("app/", "core/", "platform/", "profile/", "storage/", "ui/")):
                problems.append(f"{source.relative_to(ROOT)}: domain imports {name}")

    # These migration shims were removed once every consumer used its owning
    # contract. Keep both filenames out of source and tests permanently.
    obsolete = {"app/Types.h", "app/ConnectionColumns.h", "ui/MainWindowParts.h",
                "MainWindowParts.h", "WindowUiBasics.h", "WindowUiIncludes.h"}
    for tree in (ROOT / "src", ROOT / "tests"):
        for source in tree.rglob("*"):
            if source.suffix not in {".h", ".cpp"}:
                continue
            for name in INCLUDE.findall(source.read_text(encoding="utf-8")):
                if name in obsolete:
                    problems.append(f"{source.relative_to(ROOT)}: obsolete contract include {name}")

    forbidden = {
        "src/profile/ProfileStore.cpp": {"app/WebsiteTests.h", "core/TunPolicy.h", "platform/GlobalHotkeys.h"},
        "src/profile/ConfigBuilder.cpp": {"core/TunPolicy.h"},
    }
    for path, names in forbidden.items():
        imports = set(INCLUDE.findall((ROOT / path).read_text(encoding="utf-8")))
        for name in sorted(imports & names):
            problems.append(f"{path}: reverse dependency {name}")

    for source in (ROOT / "src/app").glob("*.cpp"):
        if re.search(r"\bapi_(?:\.|->)request\s*\(", source.read_text(encoding="utf-8")):
            problems.append(f"{source.relative_to(ROOT)}: raw core request outside adapter")

    for source in (ROOT / "src/ui").glob("*.h"):
        if "app/AppController.h" in INCLUDE.findall(source.read_text(encoding="utf-8")):
            problems.append(f"{source.relative_to(ROOT)}: UI header imports controller implementation")

    for path in ("src/core/CoreManager.h", "src/app/HomeMetrics.h", "src/app/ConnectivityProbe.h",
                 "src/app/HomeNodeProbe.h", "src/app/WebsiteTests.h"):
        imports = INCLUDE.findall((ROOT / path).read_text(encoding="utf-8"))
        if any(name.endswith("MihomoClient.h") for name in imports):
            problems.append(f"{path}: concrete core adapter in capability consumer")

    theme = (ROOT / "src/ui/UiTheme.cpp").read_text(encoding="utf-8")
    appearance = theme.split("void applyAppearance(", 1)[1].split("void install()", 1)[0]
    if re.search(r'"#[0-9a-fA-F]{6}"', appearance):
        problems.append("src/ui/UiTheme.cpp: palette value outside design specialization")

    engine_test_imports = INCLUDE.findall((ROOT / "tests/EngineTests.cpp").read_text(encoding="utf-8"))
    if any(name.startswith("ui/") for name in engine_test_imports):
        problems.append("tests/EngineTests.cpp: UI dependency in engine integration target")

    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1
    print("Architecture boundaries: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
