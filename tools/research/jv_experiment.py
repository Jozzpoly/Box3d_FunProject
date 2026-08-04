#!/usr/bin/env python3
"""Compatibility entry point; use tools/jv_lab.py from project workflows."""
from __future__ import annotations

from jv_research.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
