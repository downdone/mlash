"""Prepare pinned public inputs on a clean Windows runner; no user/system state."""
import json
import os
from pathlib import Path
import subprocess
from prepare_open_qt import ROOT, DEST, download, main as prepare_qt


def main():
    lock = json.loads((ROOT/"dependencies.lock.json").read_text(encoding="utf-8"))
    prepare_qt()
    tools = ROOT/"build/_deps/mingw"
    tools.mkdir(parents=True, exist_ok=True)
    compiler = lock["compiler"]
    archive = tools/"mingw.7z"
    download(compiler["archive_url"], archive, compiler["archive_sha256"])
    extractor = "C:/Program Files/7-Zip/7z.exe"
    listing = subprocess.check_output([extractor, "l", "-slt", str(archive)], text=True, encoding="utf-8")
    for entry in [line[7:] for line in listing.splitlines() if line.startswith("Path = ")][1:]:
        if entry.startswith(("/", "\\")) or ".." in entry.replace("\\", "/").split("/") or ":" in entry:
            raise ValueError("unsafe compiler archive path")
    subprocess.run([extractor, "x", "-y", "-o"+str(tools), str(archive)], check=True, stdout=subprocess.DEVNULL)
    compilers = sorted(tools.rglob("bin/g++.exe"), key=lambda p: len(p.parts))
    if not compilers:
        raise ValueError("compiler archive has no g++")
    mingw = compilers[0].parent.parent
    if subprocess.check_output([str(mingw/"bin/g++.exe"), "-dumpfullversion"], text=True).strip() != "13.1.0":
        raise ValueError("unexpected compiler version")
    core = lock["mihomo-release"]
    download(core["asset_url"], ROOT/"cores/mihomo.exe", core["executable_sha256"])
    subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT/"scripts/bootstrap.ps1")], check=True)
    source = ROOT/"build/_deps/mihomo-source"
    source.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "init", str(source)], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["git", "-C", str(source), "fetch", "--depth=1", core["repository"], core["source_commit"]], check=True)
    subprocess.run(["git", "-C", str(source), "checkout", "--detach", "FETCH_HEAD"], check=True)
    if subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip() != core["source_commit"]:
        raise ValueError("core source commit differs from release lock")
    if os.environ.get("GITHUB_ENV"):
        with open(os.environ["GITHUB_ENV"], "a", encoding="utf-8") as output:
            output.write(f"MLASH_MINGW={mingw.as_posix()}\nQT_PLUGIN_PATH={(DEST/'plugins').as_posix()}\n")
    print("Pinned public Qt/MinGW/core and corresponding core commit prepared", flush=True)


if __name__ == "__main__":
    main()
