#!/usr/bin/env python3
"""Validate optional OBJ/PNG assets against the current PSP loader's limits.

This script runs on the development machine/CI host. It is not part of the PSP
runtime and uses only Python's standard library.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
MAX_OBJ_LINE = 2048
MAX_FACE_VERTICES = 64
MAX_OBJ_VERTICES = 50000
MAX_TEXTURE_BYTES = 8 * 1024 * 1024
MAX_PNG_DIMENSION = 4096
OPTIONAL_ASSETS = (
    "player.obj",
    "enemy.obj",
    "player.png",
    "enemy.png",
)


class AssetError(Exception):
    pass


def check_obj(path: Path) -> list[str]:
    positions = 0
    texcoords = 0
    faces = 0
    errors: list[str] = []

    with path.open("r", encoding="utf-8", errors="strict") as stream:
        for line_number, line in enumerate(stream, 1):
            if len(line) > MAX_OBJ_LINE:
                errors.append(f"{path.name}:{line_number}: line exceeds C loader buffer")
                continue
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            fields = stripped.split()
            kind = fields[0]
            if kind == "v":
                if len(fields) < 4:
                    errors.append(f"{path.name}:{line_number}: malformed vertex")
                    continue
                try:
                    tuple(float(value) for value in fields[1:4])
                except ValueError:
                    errors.append(f"{path.name}:{line_number}: non-numeric vertex")
                    continue
                positions += 1
                if positions > MAX_OBJ_VERTICES:
                    errors.append(f"{path.name}:{line_number}: too many vertices for prototype budget")
                    break
            elif kind == "vt":
                if len(fields) < 3:
                    errors.append(f"{path.name}:{line_number}: malformed texture coordinate")
                    continue
                try:
                    tuple(float(value) for value in fields[1:3])
                except ValueError:
                    errors.append(f"{path.name}:{line_number}: non-numeric texture coordinate")
                    continue
                texcoords += 1
            elif kind == "f":
                refs = fields[1:]
                if len(refs) < 3 or len(refs) > MAX_FACE_VERTICES:
                    errors.append(f"{path.name}:{line_number}: face must contain 3..{MAX_FACE_VERTICES} vertices")
                    continue
                for ref in refs:
                    parts = ref.split("/")
                    if len(parts) < 2 or not parts[0] or not parts[1]:
                        errors.append(f"{path.name}:{line_number}: use v/vt face references")
                        break
                    try:
                        vi, ti = int(parts[0]), int(parts[1])
                    except ValueError:
                        errors.append(f"{path.name}:{line_number}: invalid face index")
                        break
                    resolved_vi = vi - 1 if vi > 0 else positions + vi
                    resolved_ti = ti - 1 if ti > 0 else texcoords + ti
                    if not (0 <= resolved_vi < positions and 0 <= resolved_ti < texcoords):
                        errors.append(f"{path.name}:{line_number}: face index is out of range")
                        break
                else:
                    faces += 1

    if positions == 0 or faces == 0:
        errors.append(f"{path.name}: model must contain vertices and faces")
    if not errors:
        print(f"OK {path.name}: {positions} positions, {texcoords} UVs, {faces} faces")
    return errors


def check_png(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        data = path.read_bytes()
    except OSError as exc:
        return [f"{path.name}: {exc}"]
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        return [f"{path.name}: invalid PNG signature or IHDR"]
    width, height = struct.unpack(">II", data[16:24])
    if width <= 0 or height <= 0 or width > MAX_PNG_DIMENSION or height > MAX_PNG_DIMENSION:
        errors.append(f"{path.name}: dimensions {width}x{height} exceed validation limits")
    if len(data) > MAX_TEXTURE_BYTES:
        errors.append(f"{path.name}: compressed file exceeds the C loader's 8 MiB limit")
    if not errors:
        print(f"OK {path.name}: {width}x{height}, {len(data)} bytes")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strict", action="store_true",
                        help="fail if any optional player/enemy asset is missing")
    args = parser.parse_args()
    errors: list[str] = []

    for name in OPTIONAL_ASSETS:
        path = ASSETS / name
        if not path.exists():
            message = f"MISSING optional asset: assets/{name}"
            if args.strict:
                errors.append(message)
            else:
                print(f"WARN {message}")
            continue
        if path.suffix.lower() == ".obj":
            errors.extend(check_obj(path))
        elif path.suffix.lower() == ".png":
            errors.extend(check_png(path))

    if errors:
        print("\n".join(f"ERROR {message}" for message in errors), file=sys.stderr)
        return 1
    print("Asset validation complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
