#!/usr/bin/env python3
"""Generate a newline-delimited raym3 icon manifest from source usage.

The scanner is intentionally conservative: it looks for string literals passed
to common icon-bearing APIs and struct fields. Apps can edit the generated file
afterward for dynamic icon names.
"""

from __future__ import annotations

import argparse
import pathlib
import re


PATTERNS = [
    re.compile(r'IconButton\(\s*"([a-z0-9_]+)"'),
    re.compile(r'Icon\(\s*"([a-z0-9_]+)"'),
    re.compile(r'\.(?:leadingIcon|trailingIcon|startIcon|endIcon|addTabIcon)\s*=\s*"([a-z0-9_]+)"'),
    re.compile(r'(?:leadingIcon|trailingIcon|startIcon|endIcon|addTabIcon)\s*=\s*"([a-z0-9_]+)"'),
    re.compile(r'IconComponent::Render\(\s*"([a-z0-9_]+)"'),
]


def iter_source_files(root: pathlib.Path):
    for path in root.rglob("*"):
        if path.parts and any(part in {".git", "build", "cmake-build-debug"} for part in path.parts):
            continue
        if path.suffix in {".cpp", ".cc", ".cxx", ".h", ".hpp", ".md"}:
            yield path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path("."))
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("resources/icons/app-icons.txt"),
    )
    args = parser.parse_args()

    icons: set[str] = set()
    for path in iter_source_files(args.root):
        text = path.read_text(errors="ignore")
        for pattern in PATTERNS:
            icons.update(pattern.findall(text))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(sorted(icons)) + ("\n" if icons else ""))
    print(f"wrote {len(icons)} icons to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
