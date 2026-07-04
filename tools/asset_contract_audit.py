#!/usr/bin/env python3
"""
Asset contract audit helper for Jozz Vehicle Box3D Native.

This is not a runtime importer. It validates that current .asset.json sidecars
are at least internally consistent with their referenced glTF files.

Usage:
  python tools/asset_contract_audit.py
  python tools/asset_contract_audit.py --contracts-dir assets/contracts --out-dir assets/reports
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


@dataclass
class Issue:
    severity: str
    message: str


@dataclass
class BindingCheck:
    path: str
    name: str
    node_index_hint: int | None
    node_path_hint: str
    role: str
    role_category: str
    matches: list[int]


@dataclass
class ContractReport:
    file: str
    asset_id: str
    asset_type: str
    source: str
    bindings: list[BindingCheck] = field(default_factory=list)
    duplicate_node_names: dict[str, int] = field(default_factory=dict)
    issues: list[Issue] = field(default_factory=list)

    @property
    def status(self) -> str:
        if any(issue.severity == "ERROR" for issue in self.issues):
            return "ERROR"
        if any(issue.severity == "WARN" for issue in self.issues):
            return "WARN"
        return "OK"


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def get_path(data: Any, dotted: str) -> Any:
    current = data
    for part in dotted.split("."):
        if not isinstance(current, dict) or part not in current:
            return None
        current = current[part]
    return current


@dataclass
class SemanticBinding:
    path: str
    name: str
    node_index_hint: int | None = None
    node_path_hint: str = ""
    role: str = ""
    role_category: str = ""


def iter_semantic_bindings(value: Any, prefix: str) -> Iterable[SemanticBinding]:
    if isinstance(value, str):
        yield SemanticBinding(prefix, value)
    elif isinstance(value, list):
        for index, item in enumerate(value):
            yield from iter_semantic_bindings(item, f"{prefix}[{index}]")
    elif isinstance(value, dict):
        if "nameHint" in value:
            node_index = value.get("nodeIndexHint")
            yield SemanticBinding(
                path=prefix,
                name=str(value.get("nameHint", "")),
                node_index_hint=node_index if isinstance(node_index, int) else None,
                node_path_hint=str(value.get("nodePathHint", "")),
                role=str(value.get("role", "")),
                role_category=str(value.get("roleCategory", "")),
            )
            return

        for key, item in value.items():
            next_prefix = f"{prefix}.{key}" if prefix else key
            yield from iter_semantic_bindings(item, next_prefix)


REQUIRED_SEMANTIC_PATHS: dict[str, list[str]] = {
    "wheel": [
        "semantics.sockets.wheelMount",
        "semantics.axes.wheelSpin",
        "semantics.markers.tireRadiusOuter",
        "semantics.markers.tireWidthLeft",
        "semantics.markers.tireWidthRight",
    ],
    "suspension_corner_visual": [
        "semantics.sockets.chassisMount",
        "semantics.sockets.wheelCenter",
        "semantics.sockets.damperUpperR",
        "semantics.sockets.damperUpperL",
        "semantics.sockets.damperLowerR",
        "semantics.sockets.damperLowerL",
        "semantics.sockets.cardanDrive",
        "semantics.sockets.cardanHub",
        "semantics.axes.suspensionTravel",
    ],
    "damper_visual": [
        "semantics.visualParts.upper",
        "semantics.visualParts.stretch",
        "semantics.visualParts.lower",
    ],
    "cardan_shaft_visual": [
        "semantics.visualParts.firstPart",
        "semantics.visualParts.midPart",
        "semantics.visualParts.lastPart",
    ],
}


def audit_contract(contract_path: Path) -> ContractReport:
    contract = load_json(contract_path)
    asset_id = str(contract.get("assetId", ""))
    asset_type = str(contract.get("assetType", ""))
    source_rel = str(contract.get("source", {}).get("gltf", ""))
    report = ContractReport(
        file=contract_path.name,
        asset_id=asset_id,
        asset_type=asset_type,
        source=source_rel,
    )

    if not asset_id:
        report.issues.append(Issue("ERROR", "Missing assetId."))
    if not asset_type:
        report.issues.append(Issue("ERROR", "Missing assetType."))

    contract_version = contract.get("source", {}).get("contractVersion")
    if contract_version != 2:
        report.issues.append(Issue("ERROR", f"Expected source.contractVersion=2, got {contract_version!r}."))

    meters_per_unit = contract.get("units", {}).get("metersPerBlockbenchUnit")
    if not isinstance(meters_per_unit, (int, float)) or meters_per_unit <= 0:
        report.issues.append(Issue("ERROR", "Missing or invalid units.metersPerBlockbenchUnit."))

    if not source_rel:
        report.issues.append(Issue("ERROR", "Missing source.gltf."))
        return report

    source_path = (contract_path.parent / source_rel).resolve()
    if not source_path.exists():
        report.issues.append(Issue("ERROR", f"Referenced source glTF does not exist: {source_rel}"))
        return report

    try:
        gltf = load_json(source_path)
    except Exception as exc:  # pragma: no cover - defensive CLI path
        report.issues.append(Issue("ERROR", f"Could not parse glTF: {exc}"))
        return report

    nodes = gltf.get("nodes", [])
    node_names = [str(node.get("name", "")) for node in nodes]
    name_counts = Counter(name for name in node_names if name)
    report.duplicate_node_names = {name: count for name, count in name_counts.items() if count > 1}

    if report.duplicate_node_names:
        dupes = ", ".join(f"{name} x{count}" for name, count in report.duplicate_node_names.items())
        report.issues.append(Issue("WARN", f"Duplicate node names exist in source glTF: {dupes}. Name-only binding is unsafe."))

    required_paths = REQUIRED_SEMANTIC_PATHS.get(asset_type, [])
    if not required_paths:
        report.issues.append(Issue("WARN", f"No required semantic schema is defined for assetType={asset_type!r}."))

    for required_path in required_paths:
        value = get_path(contract, required_path)
        if value in (None, "") or value == [] or value == {}:
            report.issues.append(Issue("ERROR", f"Missing required semantic path: {required_path}"))

    semantics = contract.get("semantics", {})
    for binding in iter_semantic_bindings(semantics, "semantics"):
        binding_path = binding.path
        name = binding.name
        matches = [index for index, node_name in enumerate(node_names) if node_name == name]
        report.bindings.append(
            BindingCheck(binding_path, name, binding.node_index_hint, binding.node_path_hint, binding.role, binding.role_category, matches)
        )

        if not name:
            report.issues.append(Issue("ERROR", f"Binding {binding_path} is missing nameHint."))
        elif len(matches) == 0:
            report.issues.append(Issue("ERROR", f"Binding {binding_path} -> {name!r} does not resolve to any node."))
        elif len(matches) > 1:
            report.issues.append(Issue("WARN", f"Binding {binding_path} -> {name!r} resolves to multiple nodes {matches}."))

        if binding.node_index_hint is None:
            report.issues.append(Issue("ERROR", f"Binding {binding_path} -> {name!r} is missing nodeIndexHint."))
        elif binding.node_index_hint < 0 or binding.node_index_hint >= len(node_names):
            report.issues.append(Issue("ERROR", f"Binding {binding_path} nodeIndexHint {binding.node_index_hint} is out of range."))
        elif node_names[binding.node_index_hint] != name:
            report.issues.append(
                Issue(
                    "ERROR",
                    f"Binding {binding_path} nodeIndexHint {binding.node_index_hint} points to {node_names[binding.node_index_hint]!r}, expected {name!r}.",
                )
            )

        if not binding.node_path_hint:
            report.issues.append(Issue("WARN", f"Binding {binding_path} -> {name!r} is missing nodePathHint."))
        if not binding.role:
            report.issues.append(Issue("WARN", f"Binding {binding_path} -> {name!r} is missing role."))
        if not binding.role_category:
            report.issues.append(Issue("WARN", f"Binding {binding_path} -> {name!r} is missing roleCategory."))

    return report


def write_markdown(reports: list[ContractReport], path: Path) -> None:
    lines = [
        "# Asset Contract Audit Report",
        "",
        "| Contract | Asset type | Source | Status | Issues | Bindings |",
        "|---|---|---|---|---:|---:|",
    ]

    for report in reports:
        lines.append(
            f"| `{report.file}` | `{report.asset_type}` | `{report.source}` | **{report.status}** | {len(report.issues)} | {len(report.bindings)} |"
        )

    for report in reports:
        lines += ["", f"## `{report.file}`", ""]
        lines.append(f"Asset ID: `{report.asset_id}`  ")
        lines.append(f"Asset type: `{report.asset_type}`  ")
        lines.append(f"Source: `{report.source}`  ")
        lines.append(f"Status: **{report.status}**")

        lines += ["", "### Issues", ""]
        if not report.issues:
            lines.append("No issues detected.")
        else:
            for issue in report.issues:
                lines.append(f"- **{issue.severity}**: {issue.message}")

        lines += ["", "### Bindings", ""]
        if not report.bindings:
            lines.append("No semantic name bindings found.")
        else:
            lines += ["| Path | Role | Category | Name | Node index hint | Matching node indices |", "|---|---|---|---|---:|---|"]
            for binding in report.bindings:
                matches = ", ".join(str(index) for index in binding.matches) or "none"
                node_index_hint = "" if binding.node_index_hint is None else str(binding.node_index_hint)
                lines.append(
                    f"| `{binding.path}` | `{binding.role}` | `{binding.role_category}` | `{binding.name}` | {node_index_hint} | `{matches}` |"
                )

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def report_to_json(report: ContractReport) -> dict[str, Any]:
    return {
        "file": report.file,
        "assetId": report.asset_id,
        "assetType": report.asset_type,
        "source": report.source,
        "status": report.status,
        "duplicateNodeNames": report.duplicate_node_names,
        "issues": [{"severity": issue.severity, "message": issue.message} for issue in report.issues],
        "bindings": [
            {
                "path": binding.path,
                "role": binding.role,
                "roleCategory": binding.role_category,
                "nameHint": binding.name,
                "nodeIndexHint": binding.node_index_hint,
                "nodePathHint": binding.node_path_hint,
                "matches": binding.matches,
            }
            for binding in report.bindings
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contracts-dir", default="assets/contracts")
    parser.add_argument("--out-dir", default="assets/reports")
    args = parser.parse_args()

    contracts_dir = Path(args.contracts_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    reports = [audit_contract(path) for path in sorted(contracts_dir.glob("*.asset.json"))]

    json_path = out_dir / "asset_contract_audit_latest.json"
    md_path = out_dir / "asset_contract_audit_latest.md"
    json_path.write_text(json.dumps([report_to_json(report) for report in reports], indent=2), encoding="utf-8")
    write_markdown(reports, md_path)

    for report in reports:
        print(f"{report.file}: {report.status} ({len(report.issues)} issues, {len(report.bindings)} bindings)")
    print(f"Wrote {json_path}")
    print(f"Wrote {md_path}")

    return 1 if any(report.status == "ERROR" for report in reports) else 0


if __name__ == "__main__":
    raise SystemExit(main())
