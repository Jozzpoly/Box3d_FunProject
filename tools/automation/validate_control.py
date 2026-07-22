#!/usr/bin/env python3
"""Validate the immutable automation control contract and work queue."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from common import AutomationContractError, load_control, load_work_items, machine_result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    args = parser.parse_args()
    repo = args.repo.resolve()
    try:
        control = load_control(repo / ".automation/CONTROL.yaml")
        items = load_work_items(repo / control["work_items"]["path"])
        result = machine_result(
            "CONTROL_VALID",
            mode=control["mode"],
            cadence_hours=control["cadence_hours"],
            work_item_count=len(items),
            implementation_enabled=control["mode"] == "IMPLEMENT_SAFE",
        )
        print(json.dumps(result, sort_keys=True))
        return 0
    except AutomationContractError as exc:
        print(json.dumps(machine_result("POLICY_CONFLICT", error=str(exc)), sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
