#!/usr/bin/env python3
"""Generate small original low-poly actor models and PNG atlases for PSP.

The generated art is original, deliberately low-poly, and kept small for the
PSP. This tool runs on the build host; the game itself loads OBJ/PNG files.
"""
from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ATLAS_SIZE = 128
CELL = 32
MARGIN = 3


class ObjBuilder:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.uvs: list[tuple[float, float]] = []
        self.faces: list[str] = []

    def add_face(self, corners: list[tuple[float, float, float]],
                 uv_cell: tuple[int, int]) -> None:
        col, row = uv_cell
        u0 = (col * CELL + MARGIN) / ATLAS_SIZE
        u1 = ((col + 1) * CELL - MARGIN) / ATLAS_SIZE
        # OBJ V is bottom-up; the PSP loader flips V on import.
        v_top = 1.0 - (row * CELL + MARGIN) / ATLAS_SIZE
        v_bottom = 1.0 - ((row + 1) * CELL - MARGIN) / ATLAS_SIZE
        uv_corners = [(u0, v_bottom), (u1, v_bottom),
                      (u1, v_top), (u0, v_top)]
        start = len(self.vertices) + 1
        for p in corners:
            self.vertices.append(p)
        for uv in uv_corners:
            self.uvs.append(uv)
        ids = [f"{start + i}/{start + i}" for i in range(4)]
        self.faces.append("f " + " ".join(ids))

    def box(self, center: tuple[float, float, float],
            size: tuple[float, float, float],
            cells: tuple[int, int] | list[tuple[int, int]]) -> None:
        cx, cy, cz = center
        sx, sy, sz = (v / 2.0 for v in size)
        p = {
            "lbf": (cx-sx, cy-sy, cz+sz), "rbf": (cx+sx, cy-sy, cz+sz),
            "rtf": (cx+sx, cy+sy, cz+sz), "ltf": (cx-sx, cy+sy, cz+sz),
            "lbb": (cx-sx, cy-sy, cz-sz), "rbb": (cx+sx, cy-sy, cz-sz),
            "rtb": (cx+sx, cy+sy, cz-sz), "ltb": (cx-sx, cy+sy, cz-sz),
        }
        sides = [
            [p["lbf"], p["rbf"], p["rtf"], p["ltf"]],
            [p["rbb"], p["lbb"], p["ltb"], p["rtb"]],
            [p["lbb"], p["lbf"], p["ltf"], p["ltb"]],
            [p["rbf"], p["rbb"], p["rtb"], p["rtf"]],
            [p["ltf"], p["rtf"], p["rtb"], p["ltb"]],
            [p["lbb"], p["rbb"], p["rbf"], p["lbf"]],
        ]
        face_cells = cells if isinstance(cells, list) else [cells] * 6
        for side, cell in zip(sides, face_cells):
            self.add_face(side, cell)

    def octa_head(self, center: tuple[float, float, float],
                  size: tuple[float, float, float],
                  uv_cell: tuple[int, int]) -> None:
        cx, cy, cz = center
        sx, sy, sz = size
        top = (cx, cy + sy, cz)
        bottom = (cx, cy - sy, cz)
        ring = [(cx-sx, cy, cz), (cx, cy, cz+sz),
                (cx+sx, cy, cz), (cx, cy, cz-sz)]
        for i in range(4):
            j = (i + 1) % 4
            self.add_face([top, ring[i], ring[j], ring[j]], uv_cell)
            self.add_face([bottom, ring[j], ring[i], ring[i]], uv_cell)

    def write(self, path: Path) -> None:
        lines = ["# Original low-poly character generated for Survival Protocol"]
        lines.extend(f"v {x:.5f} {y:.5f} {z:.5f}" for x, y, z in self.vertices)
        lines.extend(f"vt {u:.6f} {v:.6f}" for u, v in self.uvs)
        lines.extend(self.faces)
        path.write_text("\n".join(lines) + "\n", encoding="ascii")


def make_atlas(path: Path, palette: list[tuple[int, int, int]]) -> None:
    raw = bytearray()
    for y in range(ATLAS_SIZE):
        raw.append(0)  # PNG filter: None
        for x in range(ATLAS_SIZE):
            col, row = x // CELL, y // CELL
            base = palette[row * 4 + col]
            local_x, local_y = x % CELL, y % CELL
            # Low-contrast woven/painted variation that remains readable at
            # PSP resolution and does not depend on external textures.
            band = 7 if ((local_y // 5) % 2 == 0) else -5
            grain = ((local_x * 13 + local_y * 7 + col * 3) % 7) - 3
            edge = local_x < 2 or local_y < 2 or local_x > 29 or local_y > 29
            shade = -17 if edge else band + grain
            raw.extend(max(0, min(255, c + shade)) for c in base)
            raw.append(255)
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", ATLAS_SIZE, ATLAS_SIZE, 8, 6, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(chunk(b"IEND", b""))
    path.write_bytes(png)


def build_player() -> ObjBuilder:
    o = ObjBuilder()
    dark, coat, pants, boots = (0, 0), (1, 0), (2, 0), (3, 0)
    skin, pack, trim, glove = (0, 1), (1, 1), (2, 1), (3, 1)
    o.box((0, 1.05, 0), (0.66, 0.78, 0.38), [coat, coat, dark, dark, trim, dark])
    o.box((0, 0.54, 0), (0.48, 0.34, 0.34), pants)
    o.box((-0.16, 0.25, 0), (0.23, 0.48, 0.25), pants)
    o.box((0.16, 0.25, 0), (0.23, 0.48, 0.25), pants)
    o.box((-0.16, 0.045, 0.07), (0.27, 0.12, 0.38), boots)
    o.box((0.16, 0.045, 0.07), (0.27, 0.12, 0.38), boots)
    o.box((-0.45, 1.03, 0), (0.21, 0.67, 0.23), coat)
    o.box((0.45, 1.03, 0), (0.21, 0.67, 0.23), coat)
    o.box((-0.45, 0.67, 0.03), (0.18, 0.17, 0.19), glove)
    o.box((0.45, 0.67, 0.03), (0.18, 0.17, 0.19), glove)
    o.box((0, 1.48, 0), (0.18, 0.16, 0.18), skin)
    o.octa_head((0, 1.72, 0), (0.23, 0.29, 0.21), skin)
    o.box((0, 1.05, -0.25), (0.38, 0.55, 0.16), pack)
    o.box((0, 1.18, 0.205), (0.38, 0.12, 0.025), trim)
    return o


def build_sentinel() -> ObjBuilder:
    o = ObjBuilder()
    coat, dark, pants, boots = (0, 0), (1, 0), (2, 0), (3, 0)
    mask, trim, glove, leather = (0, 1), (1, 1), (2, 1), (3, 1)
    o.box((0, 1.08, 0), (0.72, 0.88, 0.42), [coat, coat, dark, dark, trim, dark])
    o.box((0, 0.56, 0), (0.54, 0.34, 0.37), pants)
    o.box((-0.18, 0.25, 0), (0.25, 0.48, 0.27), pants)
    o.box((0.18, 0.25, 0), (0.25, 0.48, 0.27), pants)
    o.box((-0.18, 0.045, 0.06), (0.29, 0.13, 0.39), boots)
    o.box((0.18, 0.045, 0.06), (0.29, 0.13, 0.39), boots)
    o.box((-0.48, 1.06, 0), (0.23, 0.72, 0.25), coat)
    o.box((0.48, 1.06, 0), (0.23, 0.72, 0.25), coat)
    o.box((-0.48, 0.67, 0.03), (0.20, 0.18, 0.20), glove)
    o.box((0.48, 0.67, 0.03), (0.20, 0.18, 0.20), glove)
    o.box((0, 1.49, 0), (0.22, 0.15, 0.20), leather)
    o.octa_head((0, 1.74, 0), (0.25, 0.31, 0.23), mask)
    o.box((0, 1.05, -0.27), (0.4, 0.58, 0.16), dark)
    o.box((0, 1.25, 0.22), (0.32, 0.16, 0.03), trim)
    return o


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    player_palette = [
        (35, 53, 69), (67, 83, 91), (66, 70, 74), (38, 43, 47),
        (171, 133, 104), (42, 48, 53), (150, 112, 76), (30, 35, 39),
        (70, 85, 91), (90, 98, 101), (110, 87, 65), (54, 58, 61),
        (25, 33, 39), (49, 57, 62), (130, 104, 77), (22, 26, 30),
    ]
    sentinel_palette = [
        (57, 71, 60), (43, 52, 47), (52, 58, 52), (34, 38, 35),
        (81, 89, 78), (65, 71, 62), (100, 85, 65), (33, 38, 34),
        (91, 99, 86), (73, 81, 70), (92, 75, 56), (46, 48, 43),
        (26, 34, 30), (45, 52, 45), (121, 103, 76), (22, 27, 23),
    ]
    build_player().write(ASSETS / "player.obj")
    build_sentinel().write(ASSETS / "enemy.obj")
    make_atlas(ASSETS / "player.png", player_palette)
    make_atlas(ASSETS / "enemy.png", sentinel_palette)
    print("Generated original low-poly player/enemy OBJ models and 128x128 PNG atlases.")


if __name__ == "__main__":
    main()
