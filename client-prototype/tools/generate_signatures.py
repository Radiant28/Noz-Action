"""Generate code guards from this exact client; never write the executable."""

import hashlib
import json
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT.parent / "WoW.exe"
EXPECTED_SHA256 = "c33f54116161b0baf3d6f3b58276688336e96c50a80e921f10a8d5a6c38a4a60"


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def main():
    assert fnv(b"") == 2166136261 and fnv(b"hello") == 0x4F9F2CAB
    data = EXE.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise SystemExit("Unsupported WoW.exe SHA-256; do not regenerate blindly")
    pe = runpy.run_path(str(ROOT.parent / "client-research" / "compare_ida_ranges.py"))
    base, sections = pe["pe_layout"](data)
    if base != 0x400000:
        raise SystemExit("Unsupported image base")
    evidence = json.loads((ROOT / "abi-evidence.json").read_text(encoding="utf-8"))
    ranges = json.loads((ROOT.parent / "client-research" / "ida-code-ranges.json").read_text())["ranges"]
    for item in evidence["functions"]:
        ranges.append({"address": item["address"], "size": item["size"]})
    camera = json.loads((ROOT / "camera-shoulder-evidence.json").read_text(encoding="utf-8"))
    for item in camera["functions"]:
        ranges.append({"address": item["address"], "size": item["size"]})
    features = json.loads((ROOT / "feature-ranges.json").read_text(encoding="utf-8"))
    for item in features["functions"]:
        size = int(item["size"], 16)
        offset = pe["file_offset"](int(item["address"], 16) - base, size, sections)
        if data[offset:offset + size] != bytes.fromhex(item["bytes"]):
            raise SystemExit(f"Function differs from recorded bytes at {item['address']}")
        ranges.append({"address": item["address"], "size": item["size"]})
    rows = []
    for entry in ranges:
        address = int(entry["address"], 16)
        size = int(entry["size"], 16)
        offset = pe["file_offset"](address - base, size, sections)
        rows.append(f"    {{0x{address:X}u, {size}u, 0x{fnv(data[offset:offset + size]):08X}u}},")
    header = """// Generated from the recorded local EXE; FNV guards detect accidental changes, not hostile tampering.
#pragma once
#include <cstdint>
struct CodeGuard { std::uintptr_t address; unsigned size; std::uint32_t hash; };
inline constexpr CodeGuard kCodeGuards[] = {
""" + "\n".join(rows) + "\n};\n"
    (ROOT / "src" / "signatures.h").write_text(header, encoding="ascii")
    print(f"Generated {len(rows)} runtime code guards; client SHA-256 verified")


if __name__ == "__main__":
    main()
