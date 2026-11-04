"""Generate the 8x8 white reticle atlas and a dark-background PNG preview."""

import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CELL = 64
SIZE = CELL * 8
SAMPLES = 4
STYLES = ("Crosshair", "Plus", "Dot", "Circle", "Circle and dot",
          "Square", "Diamond", "Corners")


def contains(style, thickness, x, y):
    """Coordinates and stroke widths use a 32-unit cell, rendered at 2x."""
    x, y = abs(x), abs(y)
    half = thickness / 2
    if style == 0:
        return (5 <= x <= 12 and y <= half) or (5 <= y <= 12 and x <= half)
    if style == 1:
        return (x <= 12 and y <= half) or (y <= 12 and x <= half)
    if style == 2:
        return x * x + y * y <= half * half
    if style in (3, 4):
        radius_squared = x * x + y * y
        ring = (13 - thickness) ** 2 <= radius_squared <= 13 ** 2
        return ring or (style == 4 and radius_squared <= half * half)
    if style == 5:
        return max(x, y) <= 12 and max(x, y) >= 12 - thickness
    if style == 6:
        return 13 - thickness * math.sqrt(2) <= x + y <= 13
    return (max(x, y) <= 12 and
            ((x >= 12 - thickness and y >= 5) or
             (y >= 12 - thickness and x >= 5)))


def render_cell(style, thickness):
    alpha = bytearray(CELL * CELL)
    offsets = [(i + 0.5) / SAMPLES for i in range(SAMPLES)]
    for py in range(CELL):
        for px in range(CELL):
            covered = sum(contains(style, thickness,
                                   (px + dx) / 2 - 16,
                                   (py + dy) / 2 - 16)
                          for dy in offsets for dx in offsets)
            alpha[py * CELL + px] = round(255 * covered / SAMPLES ** 2)
    return alpha


def validate(cells):
    assert len(cells) == 64 and len({bytes(cell) for cell in cells}) == 64
    for style in range(8):
        coverage = [sum(cells[style * 8 + column]) for column in range(8)]
        assert all(a < b for a, b in zip(coverage, coverage[1:])), STYLES[style]
        for thickness in range(1, 9):
            cell = cells[style * 8 + thickness - 1]
            assert len(cell) == CELL * CELL and max(cell) >= 191
            if style in (2, 3, 4, 6):
                assert any(0 < value < 255 for value in cell), "Missing antialiasing"
            assert all(cell[y * CELL + x] == 0
                       for y in range(CELL) for x in range(CELL)
                       if min(x, y, CELL - 1 - x, CELL - 1 - y) < 4), "Texture bleed"
            center = [cell[y * CELL + x] for y in (31, 32) for x in (31, 32)]
            assert all(bool(value) == (style in (1, 2, 4)) for value in center)
            assert contains(style, thickness, 0, 0) == (style in (1, 2, 4))
            if style in (3, 4):
                assert contains(style, thickness, 12.75, 0)
                assert not contains(style, thickness, 4.5, 0), "Circle lost its gap"
                assert cell[32 * CELL + 40] == 0, "Rendered circle lost its gap"
            if style in (0, 7):
                assert not contains(style, thickness, 4.5, 0), "Center gap closed"
            if style == 5:
                assert not contains(style, thickness, 3.5, 3.5)
            if style == 6:
                assert not contains(style, thickness, 0.5, 0.5)


def png_chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data +
            struct.pack(">I", zlib.crc32(kind + data)))


def main():
    cells = [render_cell(style, thickness)
             for style in range(8) for thickness in range(1, 9)]
    validate(cells)
    bgra = bytearray()
    preview = bytearray()
    for y in range(SIZE):
        preview.append(0)  # PNG row filter: none.
        for x in range(SIZE):
            alpha = cells[(y // CELL) * 8 + x // CELL][(y % CELL) * CELL + x % CELL]
            bgra.extend((255, 255, 255, alpha))
            background = 52 if x % CELL == 0 or y % CELL == 0 else 24
            value = background + round((255 - background) * alpha / 255)
            preview.extend((value, value, value))

    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0,
                         0, 0, SIZE, SIZE, 32, 0x28)
    atlas = ROOT / "addon" / "NozdorAction" / "Reticles.tga"
    atlas.write_bytes(header + bgra)
    stored = atlas.read_bytes()
    assert len(stored) == 18 + SIZE * SIZE * 4
    assert struct.unpack("<BBBHHBHHHHBB", stored[:18]) == (
        0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 32, 0x28)
    assert stored[18:] == bgra
    assert all(value == 255 for channel in range(3) for value in stored[18 + channel::4])

    png = (b"\x89PNG\r\n\x1a\n" +
           png_chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)) +
           png_chunk(b"IDAT", zlib.compress(preview)) + png_chunk(b"IEND", b""))
    output = ROOT / "build" / "reticles-preview.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(png)
    print(f"Validated 64 distinct antialiased cells; wrote {atlas} ({len(stored)} bytes)")
    print(f"Rows: {', '.join(STYLES)}; columns: thickness 1..8")
    print(f"Preview: {output}")


if __name__ == "__main__":
    main()
