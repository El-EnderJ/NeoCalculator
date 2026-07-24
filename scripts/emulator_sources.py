#!/usr/bin/env python3
"""Emit the exact [env:emulator_pc] project source allowlist for CMake."""

from __future__ import annotations

import argparse
import pathlib
import re


def emulator_sources(repo: pathlib.Path) -> list[pathlib.Path]:
    lines = (repo / "platformio.ini").read_text(encoding="utf-8").splitlines()
    in_environment = False
    in_filter = False
    patterns: list[str] = []
    for raw in lines:
        stripped = raw.strip()
        if stripped.startswith("["):
            in_environment = stripped == "[env:emulator_pc]"
            in_filter = False
            continue
        if not in_environment:
            continue
        if stripped == "build_src_filter =":
            in_filter = True
            continue
        if in_filter and stripped and not stripped.startswith(("+<", ";")):
            break
        match = re.fullmatch(r"\+<([^>]+)>", stripped)
        if in_filter and match:
            patterns.append(match.group(1))

    if not patterns:
        raise SystemExit("emulator_pc build_src_filter was not found")

    result: set[pathlib.Path] = set()
    source_root = repo / "src"
    for pattern in patterns:
        for path in source_root.glob(pattern):
            if path.is_file() and path.suffix in {".c", ".cc", ".cpp", ".cxx"}:
                result.add(path.resolve())
    return sorted(result)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    parser.add_argument("--cmake", type=pathlib.Path, required=True)
    args = parser.parse_args()
    sources = emulator_sources(args.repo.resolve())
    args.cmake.parent.mkdir(parents=True, exist_ok=True)
    body = "set(NUMOS_EMULATOR_SOURCES\n"
    body += "".join(f'  "{path.as_posix()}"\n' for path in sources)
    body += ")\n"
    args.cmake.write_text(body, encoding="utf-8")
    print(f"emulator_web: {len(sources)} project translation units")


if __name__ == "__main__":
    main()
