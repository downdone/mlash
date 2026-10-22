"""Compare embedded PE ICON resources with the source ICO, without Shell icon caching.

Usage: python scripts/verify-exe-icon.py path/to/Mlash.exe [--report path.json] [--preview path.png]
Only optional preview rendering requires Pillow. This is a developer verification tool.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def verify(executable, source):
    pe, ico = executable.read_bytes(), source.read_bytes()
    pe_offset = struct.unpack_from("<I", pe, 0x3c)[0]
    if pe[:2] != b"MZ" or pe[pe_offset:pe_offset+4] != b"PE\0\0":
        raise ValueError("Not a PE executable")
    section_count, optional_size = struct.unpack_from("<H12xH", pe, pe_offset+6)
    optional = pe_offset+24
    magic = struct.unpack_from("<H", pe, optional)[0]
    if magic not in (0x10b, 0x20b):
        raise ValueError("Unsupported PE optional header")
    resource_rva = struct.unpack_from("<I", pe, optional+(112 if magic == 0x20b else 96)+16)[0]
    sections = [struct.unpack_from("<8sIIII", pe, optional+optional_size+40*i) for i in range(section_count)]

    def offset(rva):
        for _, virtual_size, virtual_rva, raw_size, raw_offset in sections:
            if virtual_rva <= rva < virtual_rva+max(virtual_size, raw_size):
                return raw_offset+rva-virtual_rva
        raise ValueError("Resource RVA has no section")

    base = offset(resource_rva)

    def entries(relative):
        directory = base+relative
        named, numbered = struct.unpack_from("<HH", pe, directory+12)
        return dict(struct.unpack_from("<II", pe, directory+16+8*i) for i in range(named+numbered))

    def leaves(resource_type):
        result = {}
        for identifier, name_directory in entries(entries(0)[resource_type] & 0x7fffffff).items():
            for language, data_entry in entries(name_directory & 0x7fffffff).items():
                rva, size = struct.unpack_from("<II", pe, base+data_entry)
                begin = offset(rva)
                data = pe[begin:begin+size]
                if len(data) != size:
                    raise ValueError("Truncated resource")
                result[(identifier, language)] = data
        return result

    if struct.unpack_from("<HH", ico)[0:2] != (0, 1):
        raise ValueError("Not an ICO file")
    count = struct.unpack_from("<H", ico, 4)[0]
    expected = {}
    for index in range(count):
        width, height, _, _, planes, depth, size, begin = struct.unpack_from("<BBBBHHII", ico, 6+16*index)
        data = ico[begin:begin+size]
        if len(data) != size:
            raise ValueError("Truncated ICO")
        # ICO may omit planes (0); windres normalizes this to the bitmap's single plane.
        expected[(width or 256, height or 256, planes or 1, depth)] = data
    icons, groups = leaves(3), leaves(14)
    if not groups:
        raise ValueError("No embedded icon group")
    evidence = []
    for (identifier, language), group in groups.items():
        if struct.unpack_from("<HHH", group) != (0, 1, count):
            raise ValueError("Embedded icon group differs from source")
        seen = set()
        for index in range(count):
            width, height, _, _, planes, depth, size, icon_id = struct.unpack_from("<BBBBHHIH", group, 6+14*index)
            key = (width or 256, height or 256, planes, depth)
            actual = icons[(icon_id, language)]
            if len(actual) != size or actual != expected[key] or key in seen:
                raise ValueError(f"Embedded {key[0]}px icon differs from source")
            seen.add(key)
            evidence.append({"group": identifier, "language": language, "size": key[0], "sha256": hashlib.sha256(actual).hexdigest()})
        if seen != expected.keys():
            raise ValueError("Missing icon dimensions")
    return {"executable": str(executable.resolve()), "source": str(source.resolve()), "matched": True, "icons": evidence}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parent.parent / "resources/mlash.ico")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    report = verify(args.executable, args.source)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if args.preview:
        from PIL import Image
        # Resource bytes were compared directly; preview the identical source payload.
        with Image.open(args.source) as icon:
            icon.ico.getimage((256, 256)).save(args.preview)
    print(f"Embedded ICON matches source at {len(report['icons'])} dimensions: " + ", ".join(str(icon["size"]) for icon in report["icons"]))
