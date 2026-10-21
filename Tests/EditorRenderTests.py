#!/usr/bin/env python3
"""Render tier: the processor suite renders the editor screenshot; check what it wrote.

The suite writes the PNG through CERES_EDITOR_SNAPSHOT, with a tEXt chunk
"Ceres-Editor-Source" holding a hash of Source/PluginEditor.h and .cpp (see
CMakeLists.txt). This check requires a complete, tagged PNG whose pixels are
not one flat colour and, while the editor source is the one the committed
screenshot was rendered from (the same tag), the committed pixel size. When
the editor source has changed, its size may too: the size is then reported,
not compared, and the main build commits the new screenshot. (Comparing the
size unconditionally, as this check did before, made a size change
impossible to land: the main build only commits a screenshot after this
check passes.) Pixels are not compared with the committed image: font
rasterisation differs between platforms.
"""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
COMMITTED = ROOT / "Docs/screenshots/ceres.png"


TAG = b"Ceres-Editor-Source"


def text_chunks(path: Path) -> dict:
    data, chunks, position = path.read_bytes(), {}, 8
    while position + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        if kind == b"tEXt":
            keyword, _, text = data[position + 8:position + 8 + length].partition(b"\0")
            chunks[keyword] = text.decode("latin-1")
        position += length + 12
    return chunks


def read_png(path: Path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[-12:] != b"\0\0\0\0IEND\xaeB`\x82":
        raise AssertionError(f"{path} is not a complete PNG")
    width, height, depth, colour = struct.unpack(">IIBB", data[16:26])
    position, idat = 8, b""
    while position < len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        if kind == b"IDAT":
            idat += data[position + 8:position + 8 + length]
        position += length + 12
    return width, height, depth, colour, zlib.decompress(idat)


def distinct_rows(width, height, depth, colour, raw) -> int:
    # Unfilter scanlines (PNG filter types 0-4) and count distinct rows.
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
    stride = width * channels * depth // 8
    pixel = max(1, channels * depth // 8)
    previous, rows = bytearray(stride), set()
    for row in range(height):
        start = row * (stride + 1)
        kind, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            left = line[i - pixel] if i >= pixel else 0
            up, corner = previous[i], previous[i - pixel] if i >= pixel else 0
            if kind == 1: line[i] = (line[i] + left) & 255
            elif kind == 2: line[i] = (line[i] + up) & 255
            elif kind == 3: line[i] = (line[i] + (left + up) // 2) & 255
            elif kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        rows.add(bytes(line))
        previous = line
    return len(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processor-tests", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ceres-render-tier-") as folder:
        snapshot = Path(folder, "ceres.png")
        environment = {**os.environ, "CERES_EDITOR_SNAPSHOT": str(snapshot)}
        subprocess.run([str(args.processor_tests)], env=environment, check=True,
                       stdout=subprocess.DEVNULL, timeout=600)
        width, height, *image = read_png(snapshot)
        source = text_chunks(snapshot).get(TAG)
        if not source or source == "unknown":
            raise AssertionError("the snapshot does not name the editor source it was rendered from")
        # Until the first main build commits the screenshot there is nothing
        # to compare the size with; that build's render is the first one.
        if COMMITTED.exists():
            expected = read_png(COMMITTED)[:2]
            if text_chunks(COMMITTED).get(TAG) == source:
                if (width, height) != expected:
                    raise AssertionError(f"rendered {width}x{height}, the committed screenshot is "
                                         f"{expected[0]}x{expected[1]} from the same editor source")
            elif (width, height) != expected:
                print(f"Ceres: the editor source changed since the committed screenshot; it now renders "
                      f"{width}x{height} (committed {expected[0]}x{expected[1]})")
        rows = distinct_rows(width, height, *image)
        if rows < height // 4:
            raise AssertionError(f"only {rows} distinct rows in {height}: the render looks blank")
    print(f"Ceres: editor rendered at {width}x{height} with {rows} distinct rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
