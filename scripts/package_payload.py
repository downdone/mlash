"""Strict portable payload contract; no dependency discovery or user data reads."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil


def relative(value):
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise ValueError("invalid payload path")
    path = PurePosixPath(value)
    if path.is_absolute() or any(p in ("", ".", "..") for p in value.split("/")):
        raise ValueError("unsafe payload path")
    return value


def load_plan(path):
    plan = json.loads(Path(path).read_text(encoding="utf-8"))
    if plan.get("schema") != 1:
        raise ValueError("unsupported payload schema")
    targets = list(plan["deployed_files"]) + list(plan["generated_files"])
    for asset in plan["assets"]:
        relative(asset["source"])
        targets.append(asset["target"])
    targets = [relative(p) for p in targets]
    if len({p.casefold() for p in targets}) != len(targets):
        raise ValueError("duplicate payload target")
    if set(plan["generated_files"]) != {"qt.conf", "cores/manifest.json", "package-manifest.json"}:
        raise ValueError("unexpected generated payload")
    return plan


def files_under(root):
    files = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError("symbolic links are not portable payload files")
        if path.is_file():
            files.add(path.relative_to(root).as_posix())
    return files


def exact(actual, expected):
    if actual != expected:
        raise ValueError(f"payload differs: missing={sorted(expected-actual)}, extra={sorted(actual-expected)}")


def stage_payload(repo, stage, plan):
    repo, stage = Path(repo).resolve(), Path(stage).resolve()
    exact(files_under(stage), set(plan["deployed_files"]))
    core = json.loads((repo/"cores/manifest.json").read_text(encoding="utf-8-sig"))
    # Never export local filesystem paths or arbitrary additions from a development manifest.
    public_core = {key: core[key] for key in ("version", "origin", "source_commit", "executable_sha256", "reported_version")}
    public_core["upstream_repository"] = "https://github.com/MetaCubeX/mihomo"
    for asset in plan["assets"]:
        source = (repo/asset["source"]).resolve()
        if not source.is_relative_to(repo) or not source.is_file():
            raise ValueError("payload source is missing or outside repository")
        destination = stage/asset["target"]
        destination.parent.mkdir(parents=True, exist_ok=True)
        if asset.get("template"):
            destination.write_text(source.read_text(encoding="utf-8").replace("@CORE_VERSION@", core["version"]), encoding="utf-8")
        else:
            shutil.copyfile(source, destination)
    (stage/"qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\n", encoding="utf-8")
    (stage/"cores/manifest.json").write_text(json.dumps(public_core, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    verify_payload(stage, plan, with_manifest=False)


def verify_payload(stage, plan, with_manifest=True):
    stage = Path(stage)
    expected = set(plan["deployed_files"]) | set(plan["generated_files"]) | {a["target"] for a in plan["assets"]}
    if not with_manifest:
        expected.remove("package-manifest.json")
    exact(files_under(stage), expected)
    if with_manifest:
        manifest = json.loads((stage/"package-manifest.json").read_text(encoding="utf-8-sig"))
        records = manifest["files"]
        paths = [relative(f["path"]) for f in records]
        if len(set(paths)) != len(paths):
            raise ValueError("duplicate integrity entry")
        exact(set(paths), expected-{"package-manifest.json"})
        for record in records:
            data = (stage/record["path"]).read_bytes()
            if len(data) != record["bytes"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
                raise ValueError("payload integrity mismatch: "+record["path"])
    return len(expected)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("stage", "verify"))
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--repo", type=Path)
    args = parser.parse_args()
    try:
        plan = load_plan(args.plan)
        if args.action == "stage":
            if args.repo is None:
                raise ValueError("staging requires repository")
            stage_payload(args.repo, args.root, plan)
        else:
            print(f"Portable allowlist and integrity verified: {verify_payload(args.root, plan)} files")
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f"Payload rejected: {error}\n")
