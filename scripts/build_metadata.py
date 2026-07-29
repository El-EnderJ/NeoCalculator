"""Inject reproducible, read-only build identity into NumOS firmware."""

from __future__ import annotations

import subprocess

Import("env")  # noqa: F821 - supplied by PlatformIO/SCons


def git_value(*args: str) -> str:
    try:
        return subprocess.check_output(
            ["git", *args], text=True, encoding="utf-8"
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


commit = git_value("rev-parse", "HEAD")
environment = env.subst("$PIOENV")

env.Append(
    CPPDEFINES=[
        ("NUMOS_BUILD_COMMIT", f'\\"{commit}\\"'),
        ("NUMOS_BUILD_ENVIRONMENT", f'\\"{environment}\\"'),
    ]
)
