#!/usr/bin/env python3
"""PNG -> uncompressed RGBA .rttex, padded onto a fixed canvas (default 64x64).

usage: python tools/png_to_rttex.py art.png resources/custom_assets/Name.rttex [--canvas 64]
       python tools/png_to_rttex.py --pad old.rttex new.rttex [--canvas 64]

RTTEX needs power-of-two sizes, so e.g. a 48x16 sprite must live on a 64x64 (or 64x16)
canvas. Art is placed at the top-left; the rest is transparent.
"""
import struct, sys
from PIL import Image

HDR = b"RTTXTR\0\0"

def read_rttex(path):
    d = open(path, "rb").read()
    h, w = struct.unpack("<2I", d[8:16])
    im = Image.frombytes("RGBA", (w, h), d[len(d) - w * h * 4:])
    return im.transpose(Image.FLIP_TOP_BOTTOM)  # stored bottom-up

def write_rttex(im, path):
    w, h = im.size
    assert w & (w - 1) == 0 and h & (h - 1) == 0, "canvas must be power of two"
    px = im.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).tobytes()
    hdr = HDR + struct.pack("<2I", h, w) + struct.pack("<I", 5121) + struct.pack("<2I", h, w)
    hdr += struct.pack("<2I", 1, 1)                  # same flag words as the existing files
    hdr = hdr.ljust(100, b"\0") + struct.pack("<3I", h, w, w * h * 4)  # mip-level entry
    hdr = hdr.ljust(124, b"\0")
    open(path, "wb").write(hdr + px)

def main():
    a = sys.argv[1:]
    canvas = 64
    if "--canvas" in a:
        i = a.index("--canvas"); canvas = int(a[i + 1]); del a[i:i + 2]
    pad = a[0] == "--pad"
    if pad: a = a[1:]
    src, dst = a
    im = read_rttex(src) if pad else Image.open(src).convert("RGBA")
    if im.width > canvas or im.height > canvas:
        sys.exit(f"art {im.size} is bigger than canvas {canvas}x{canvas}")
    out = Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))
    out.paste(im, (0, 0))
    write_rttex(out, dst)
    print(f"wrote {dst}: art {im.size} on {canvas}x{canvas} canvas")

if __name__ == "__main__":
    main()
