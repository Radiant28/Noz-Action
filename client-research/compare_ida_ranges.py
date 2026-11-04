"""Compare saved IDA code ranges with a disk PE32 file; never modify the EXE."""

import hashlib
import json
import struct
import sys
from pathlib import Path


def pe_layout(data):
    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError("Truncated PE header")
        return struct.unpack_from(fmt, data, offset)

    if data[:2] != b"MZ":
        raise ValueError("Missing MZ signature")
    pe, = unpack("<I", 0x3C)
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Missing PE signature")
    machine, count = unpack("<HH", pe + 4)
    optional_size, = unpack("<H", pe + 20)
    optional = pe + 24
    magic, = unpack("<H", optional)
    if machine != 0x14C or magic != 0x10B or optional_size < 96:
        raise ValueError("Expected x86 PE32")
    image_base, = unpack("<I", optional + 28)
    sections = []
    for index in range(count):
        offset = optional + optional_size + index * 40
        name, virtual_size, rva, raw_size, raw_offset = unpack("<8sIIII", offset)
        if raw_offset + raw_size > len(data):
            raise ValueError("Section extends beyond file")
        sections.append({
            "name": name.rstrip(b"\0").decode("ascii", errors="replace"),
            "rva": rva, "virtual_size": virtual_size,
            "raw_offset": raw_offset, "raw_size": raw_size,
        })
    return image_base, sections


def file_offset(rva, size, sections):
    if size <= 0:
        raise ValueError("Range must be nonempty")
    for section in sections:
        delta = rva - section["rva"]
        if 0 <= delta and delta + size <= section["raw_size"]:
            return section["raw_offset"] + delta
    raise ValueError("Range is not wholly backed by a file section")


def compare(exe, evidence):
    data = exe.read_bytes()
    base, sections = pe_layout(data)
    ida_base = int(evidence["metadata"]["base"], 16)
    results = []
    for entry in evidence["ranges"]:
        expected = bytes.fromhex(entry["bytes"])
        size = int(entry["size"], 16)
        if len(expected) != size:
            raise ValueError("Evidence size mismatch")
        offset = file_offset(int(entry["address"], 16) - ida_base, size, sections)
        actual = data[offset:offset + size]
        differences = [index for index, pair in enumerate(zip(expected, actual))
                       if pair[0] != pair[1]]
        results.append({
            "name": entry["name"], "ida_va": entry["address"],
            "local_va": hex(base + int(entry["address"], 16) - ida_base),
            "file_offset": hex(offset), "size": size,
            "match": actual == expected, "different_bytes": len(differences),
            "first_difference": hex(differences[0]) if differences else None,
        })
    return {
        "exe": str(exe.resolve()), "md5": hashlib.md5(data).hexdigest(),
        "sha256": hashlib.sha256(data).hexdigest(), "image_base": hex(base),
        "sections": sections, "ranges_checked": len(results),
        "ranges_matching": sum(entry["match"] for entry in results),
        "scope": "Only saved code ranges compared; callees, data and runtime unverified.",
        "results": results,
    }


def self_test():
    data = bytearray(0x300)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", data, 0x84, 0x14C, 1)
    struct.pack_into("<H", data, 0x94, 0xE0)
    struct.pack_into("<H", data, 0x98, 0x10B)
    struct.pack_into("<I", data, 0xB4, 0x400000)
    struct.pack_into("<8sIIII", data, 0x178, b".text", 0x200, 0x1000, 0x100, 0x200)
    base, sections = pe_layout(data)
    assert base == 0x400000
    assert file_offset(0x1010, 4, sections) == 0x210
    for rva, size in [(0x0FFF, 1), (0x10FF, 2), (0x1100, 1), (0x1000, 0)]:
        try:
            file_offset(rva, size, sections)
        except ValueError:
            continue
        raise AssertionError("Accepted an invalid or virtual-only range")
    for invalid in [b"", data[:0x180], b"NO" + data[2:]]:
        try:
            pe_layout(invalid)
        except ValueError:
            continue
        raise AssertionError("Accepted a malformed PE")
    print("Self-test passed")


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        self_test()
    else:
        root = Path(__file__).resolve().parent
        exe = Path(sys.argv[1]) if len(sys.argv) > 1 else root.parent / "WoW.exe"
        evidence = json.loads((root / "ida-code-ranges.json").read_text(encoding="utf-8"))
        print(json.dumps(compare(exe, evidence), indent=2))
