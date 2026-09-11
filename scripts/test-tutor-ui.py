#!/usr/bin/env python3
"""Compatibility entry point for the complete guided Equations tutor UI suite.

The former compact/detail suite is superseded by complete guided/summary page
replays, retaining its negative/repeated-root, locale, cache, and HOME coverage.
Existing --bin and --out invocations continue to work.
"""
import importlib.util
from pathlib import Path

source = Path(__file__).with_name("test-tutor-teaching-ui.py")
spec = importlib.util.spec_from_file_location("tutor_teaching_ui", source)
suite = importlib.util.module_from_spec(spec)
spec.loader.exec_module(suite)

if __name__ == "__main__":
    suite.main()
