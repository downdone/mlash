"""Check tag/version, package allowlist, PE provenance and ZIP identity for CI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import zipfile
from package_payload import load_plan, verify_payload


def version_tag(source, requested=""):
    match = re.search(r"project\(Mlash VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES", source)
    if not match:
        raise ValueError("CMake version missing")
    version = match[1]
    if requested and requested != "v"+version:
        raise ValueError("release tag must match current CMake version")
    return version


def verify(root, build, expected):
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = json.loads((root/"package-manifest.json").read_text(encoding="utf-8-sig"))
    count = verify_payload(root, load_plan("scripts/package-files.json"))
    if manifest["version"] != expected or manifest["release_qualified"]:
        raise ValueError("CI only prepares explicitly unqualified prereleases")
    for name in ("Mlash.exe", "MlashTunService.exe"):
        if sha(root/name) != sha(build/"bin"/name):
            raise ValueError("packaged EXE differs from build")
    lock = json.loads(Path("dependencies.lock.json").read_text(encoding="utf-8"))["mihomo-release"]
    if manifest["core_sha256"] != lock["executable_sha256"] or sha(root/"cores/mihomo.exe") != lock["executable_sha256"]:
        raise ValueError("package core differs from pinned core")
    archive = Path(str(root)+".zip")
    with zipfile.ZipFile(archive) as payload:
        names = {p for p in payload.namelist() if not p.endswith("/")}
        if names != {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()}:
            raise ValueError("ZIP differs from staged payload")
        for name in names:
            if payload.read(name) != (root/name).read_bytes():
                raise ValueError("ZIP bytes differ from staged payload")
    if sha(archive) != Path(str(archive)+".sha256").read_text(encoding="utf-8-sig").split()[0]:
        raise ValueError("ZIP SHA differs from sidecar")
    return {"passed": True, "files": count, "executable_sha256": manifest["executable_sha256"], "core_sha256": manifest["core_sha256"], "runtime_fingerprint": manifest["runtime_fingerprint"], "zip_sha256": sha(archive)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path)
    parser.add_argument("--build", type=Path, default=Path("build/windows-ci"))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    version = version_tag(Path("CMakeLists.txt").read_text(encoding="utf-8"), os.environ.get("RELEASE_TAG", ""))
    if args.package:
        result = verify(args.package, args.build, version)
        if args.output:
            args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
        print(json.dumps(result))
    else:
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
                output.write("version="+version+"\n")
        print(version)
