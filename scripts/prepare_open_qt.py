"""Install only pinned public Qt SDK archives into build/_deps; never alter an installed SDK."""
import hashlib
import json
import subprocess
import urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/"build/_deps/qt-open"
BASE="https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6102/qt6_6102/qt.qt6.6102.win64_mingw/"
PREFIX="6.10.2-0-202601261212"
SUFFIX="-Windows-Windows_11_24H2-Mingw-Windows-Windows_11_24H2-X86_64.7z"
PACKAGES={"qtbase":"511139353ebf960d16e8140eaae0c5424977152e7bc51d040d0279ca8d96a389",
          "qttools":"e88fccac1cb1addaff9d1d0f318dab05eea2118f7cabfcc8e1c36e7b07dc199a"}


def download(url,path,expected):
    if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()==expected: return
    temporary=path.with_suffix(path.suffix+".download")
    with urllib.request.urlopen(url,timeout=60) as response,temporary.open("wb") as output:
        while block:=response.read(1024*1024): output.write(block)
    if hashlib.sha256(temporary.read_bytes()).hexdigest()!=expected:
        raise RuntimeError("official SHA256 mismatch: "+path.name)
    temporary.replace(path)


def main():
    DEST.mkdir(parents=True,exist_ok=True); records=[]
    extractor=Path("C:/Program Files/7-Zip/7z.exe")
    if not extractor.is_file(): raise RuntimeError("7-Zip is required for SDK extraction")
    for component,expected in PACKAGES.items():
        name=PREFIX+component+SUFFIX; archive=DEST/name; url=BASE+name
        print("Downloading/validating public Qt "+component,flush=True); download(url,archive,expected)
        listing=subprocess.check_output([str(extractor),"l","-slt",str(archive)],text=True,encoding="utf-8",errors="replace")
        entries=[line[7:] for line in listing.splitlines() if line.startswith("Path = ")][1:]
        for entry in entries:
            parts=entry.replace("\\","/").split("/")
            if entry.startswith(("/","\\")) or ".." in parts or ":" in entry:
                raise RuntimeError("unsafe SDK archive path")
        subprocess.run([str(extractor),"x","-y","-o"+str(DEST),str(archive)],check=True,stdout=subprocess.DEVNULL)
        records.append({"component":component,"url":url,"sha256":expected})
    kit=DEST
    if not (kit/"bin/moc.exe").exists(): raise RuntimeError("Qt kit layout changed")
    # Qt paths are relocatable; explicit local prefix prevents old installer paths.
    (kit/"bin/qt.conf").write_text("[Paths]\nPrefix=..\n",encoding="utf-8")
    (DEST/"provenance.json").write_text(json.dumps({"schema":1,"version":"6.10.2","origin":"official-public-sdk","kit":str(kit),"archives":records},indent=2),encoding="utf-8")
    print("Public Qt kit: "+str(kit),flush=True)


if __name__=="__main__": main()
