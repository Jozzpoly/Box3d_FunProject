#!/usr/bin/env python3
"""Strict shared contracts for the recurring-agent control plane.

CONTROL.yaml intentionally uses the JSON-compatible subset of YAML 1.2. This
keeps the file readable while avoiding implicit YAML types, aliases and parser
version drift. No third-party package is required.
"""
from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import fnmatch
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from typing import Any, Iterable, Mapping, Sequence

MODES = ("DISABLED", "READ_ONLY", "PLAN_ONLY", "IMPLEMENT_SAFE")
MODE_RANK = {value: index for index, value in enumerate(MODES)}
STATUSES = (
    "PROPOSED",
    "AGENT_READY",
    "ACTIVE",
    "BLOCKED",
    "OWNER_NEEDED",
    "VISUAL_REVIEW",
    "DONE",
    "REJECTED",
)
RISK_CLASSES = ("A0", "A1", "A2", "A3", "A4")
NO_OP_RESULTS = (
    "NO_MATERIAL_CHANGE",
    "NO_SAFE_WORK",
    "ACTIVE_AGENT_DETECTED",
    "LOCK_UNAVAILABLE",
    "LOCK_UNCERTAIN",
    "OWNER_GATE",
    "VISUAL_GATE",
    "PRIVATE_DATA_REQUIRED",
    "CI_PENDING",
    "BASE_MOVED",
    "TASK_TOO_LARGE",
    "POLICY_CONFLICT",
)
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
RUN_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{5,80}$")
WORK_ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{2,80}$")
CONTROL_START = "<!-- automation-control-json:start -->"
CONTROL_END = "<!-- automation-control-json:end -->"


class AutomationContractError(ValueError):
    """Raised when a control-plane document is not exact and safe."""


@dataclass(frozen=True)
class CommandResult:
    returncode: int
    stdout: str
    stderr: str


def _duplicate_guard(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise AutomationContractError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json_document(path: Path) -> Any:
    path = Path(path)
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise AutomationContractError(f"cannot read {path.as_posix()}") from exc
    try:
        return json.loads(
            text,
            object_pairs_hook=_duplicate_guard,
            parse_constant=lambda value: (_ for _ in ()).throw(
                AutomationContractError(f"non-finite JSON number: {value}")
            ),
        )
    except (json.JSONDecodeError, UnicodeError) as exc:
        raise AutomationContractError(
            f"{path.as_posix()} must use strict JSON-compatible YAML"
        ) from exc


def canonical_json(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def _object(value: Any, name: str, keys: set[str]) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise AutomationContractError(f"{name} must be an object")
    actual = set(value)
    missing = sorted(keys - actual)
    unknown = sorted(actual - keys)
    if missing or unknown:
        raise AutomationContractError(
            f"{name} keys mismatch; missing={missing}, unknown={unknown}"
        )
    return value


def _list_of_strings(value: Any, name: str, *, nonempty: bool = False) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise AutomationContractError(f"{name} must be a string list")
    if nonempty and not value:
        raise AutomationContractError(f"{name} must not be empty")
    if len(value) != len(set(value)):
        raise AutomationContractError(f"{name} must not contain duplicates")
    return value


def _bool(value: Any, name: str) -> bool:
    if type(value) is not bool:
        raise AutomationContractError(f"{name} must be boolean")
    return value


def _int(value: Any, name: str, minimum: int, maximum: int) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise AutomationContractError(f"{name} must be integer {minimum}..{maximum}")
    return value


def _string(value: Any, name: str, *, nonempty: bool = True) -> str:
    if not isinstance(value, str) or (nonempty and not value.strip()):
        raise AutomationContractError(f"{name} must be a non-empty string")
    return value


def validate_control(document: Any) -> dict[str, Any]:
    top = _object(
        document,
        "CONTROL",
        {
            "schema_version",
            "mode",
            "cadence_hours",
            "control_issue",
            "authority",
            "execution",
            "branch",
            "work_items",
            "risk",
            "gates",
            "scope",
            "reporting",
            "no_op_results",
        },
    )
    if top["schema_version"] != 1:
        raise AutomationContractError("unsupported CONTROL schema_version")
    if top["mode"] not in MODES:
        raise AutomationContractError("CONTROL.mode is invalid")
    _int(top["cadence_hours"], "cadence_hours", 1, 168)

    issue = _object(
        top["control_issue"],
        "control_issue",
        {
            "title",
            "lease_minutes",
            "stale_lock_policy",
            "require_claim_reread",
            "claim_settle_seconds",
        },
    )
    _string(issue["title"], "control_issue.title")
    _int(issue["lease_minutes"], "control_issue.lease_minutes", 5, 240)
    if issue["stale_lock_policy"] != "OWNER_REVIEW_REQUIRED":
        raise AutomationContractError("stale locks must require owner review")
    _bool(issue["require_claim_reread"], "control_issue.require_claim_reread")
    if issue["require_claim_reread"] is not True:
        raise AutomationContractError("claim reread cannot be disabled")
    _int(issue["claim_settle_seconds"], "control_issue.claim_settle_seconds", 1, 60)

    authority = _object(
        top["authority"],
        "authority",
        {
            "root_memory",
            "global_agent_rules",
            "domain_state_pattern",
            "checkpoint_ledger",
            "technical_debt",
            "active_campaign_source",
            "precedence",
        },
    )
    for field in (
        "root_memory",
        "global_agent_rules",
        "domain_state_pattern",
        "checkpoint_ledger",
        "technical_debt",
    ):
        _string(authority[field], f"authority.{field}")
    if authority["active_campaign_source"] != "control_issue":
        raise AutomationContractError("active campaign must be owner-controlled in the control issue")
    expected_precedence = [
        "CONTROL_ISSUE",
        "AI_PROJECT_MEMORY",
        "DOMAIN_CURRENT_STATE",
        "ACTIVE_CAMPAIGN_PR",
        "README_FOR_AGENTS",
        "CHECKPOINT_LEDGER",
        "TECH_DEBT",
        "SUBSYSTEM_DOCS",
        "CODE_AND_TESTS",
    ]
    if authority["precedence"] != expected_precedence:
        raise AutomationContractError("authority.precedence must match the fixed hierarchy")

    execution = _object(
        top["execution"],
        "execution",
        {
            "max_tasks_per_run",
            "max_open_automation_prs",
            "require_exact_base_sha",
            "require_fresh_remote_check_before_write",
            "require_draft_pr",
            "allow_merge",
            "allow_force_push",
            "allow_rebase",
            "allow_retarget_existing_pr",
            "allow_close_other_prs",
            "allow_direct_push_to_active_branch",
            "allow_self_policy_modification",
            "commit_periodic_reports",
        },
    )
    if execution["max_tasks_per_run"] != 1 or execution["max_open_automation_prs"] != 1:
        raise AutomationContractError("execution cardinality must remain one")
    required_true = (
        "require_exact_base_sha",
        "require_fresh_remote_check_before_write",
        "require_draft_pr",
    )
    required_false = (
        "allow_merge",
        "allow_force_push",
        "allow_rebase",
        "allow_retarget_existing_pr",
        "allow_close_other_prs",
        "allow_direct_push_to_active_branch",
        "allow_self_policy_modification",
        "commit_periodic_reports",
    )
    for field in required_true:
        if _bool(execution[field], f"execution.{field}") is not True:
            raise AutomationContractError(f"execution.{field} cannot be disabled")
    for field in required_false:
        if _bool(execution[field], f"execution.{field}") is not False:
            raise AutomationContractError(f"execution.{field} cannot be enabled")

    branch = _object(
        top["branch"],
        "branch",
        {"prefix", "format", "owner_branches_forbidden", "protected_active_branches"},
    )
    if branch["prefix"] != "automation/":
        raise AutomationContractError("automation branch prefix is fixed")
    if branch["format"] != "automation/{work_item_id}/{run_id}":
        raise AutomationContractError("automation branch format is fixed")
    forbidden_branches = _list_of_strings(
        branch["owner_branches_forbidden"], "branch.owner_branches_forbidden", nonempty=True
    )
    if "main" not in forbidden_branches or "jozz-vehicle-sandbox-m0" not in forbidden_branches:
        raise AutomationContractError("owner protected branches are incomplete")
    if _bool(branch["protected_active_branches"], "branch.protected_active_branches") is not True:
        raise AutomationContractError("active branches must remain protected")

    items = _object(
        top["work_items"],
        "work_items",
        {
            "path",
            "schema_path",
            "selectable_status",
            "require_owner_readiness",
            "one_item_per_run",
        },
    )
    _string(items["path"], "work_items.path")
    _string(items["schema_path"], "work_items.schema_path")
    if items["selectable_status"] != "AGENT_READY":
        raise AutomationContractError("only AGENT_READY work may be implemented")
    if _bool(items["require_owner_readiness"], "work_items.require_owner_readiness") is not True:
        raise AutomationContractError("owner readiness cannot be disabled")
    if _bool(items["one_item_per_run"], "work_items.one_item_per_run") is not True:
        raise AutomationContractError("one-item rule cannot be disabled")

    risk = _object(
        top["risk"],
        "risk",
        {
            "allowed_in_read_only",
            "allowed_in_plan_only",
            "allowed_in_implement_safe",
            "never_autonomous",
        },
    )
    if risk["allowed_in_read_only"] != ["A0"] or risk["allowed_in_plan_only"] != ["A0"]:
        raise AutomationContractError("READ_ONLY and PLAN_ONLY may execute only A0")
    if risk["allowed_in_implement_safe"] != ["A0", "A1", "A2_AGENT_READY_ONLY"]:
        raise AutomationContractError("IMPLEMENT_SAFE risk contract changed")
    if risk["never_autonomous"] != ["A3", "A4"]:
        raise AutomationContractError("A3/A4 must remain never-autonomous")

    gates = _object(
        top["gates"],
        "gates",
        {
            "owner_decision",
            "visual_review",
            "private_data",
            "threshold_change",
            "accepted_behavior_change",
            "box3d_core_change",
            "policy_change",
            "lock_uncertain",
            "base_moved",
            "ci_pending",
        },
    )
    if any(value != "stop" for value in gates.values()):
        raise AutomationContractError("all hard gates must remain stop")

    scope = _object(
        top["scope"],
        "scope",
        {
            "glob_style",
            "always_forbidden_paths",
            "protected_control_paths",
            "threshold_sensitive_patterns",
            "maximum_default_files",
            "maximum_default_changed_lines",
        },
    )
    if scope["glob_style"] != "gitwildmatch":
        raise AutomationContractError("scope.glob_style must be gitwildmatch")
    always_forbidden = _list_of_strings(
        scope["always_forbidden_paths"], "scope.always_forbidden_paths", nonempty=True
    )
    if "src/**" not in always_forbidden or "include/**" not in always_forbidden:
        raise AutomationContractError("Box3D core paths must remain forbidden")
    _list_of_strings(scope["protected_control_paths"], "scope.protected_control_paths", nonempty=True)
    _list_of_strings(
        scope["threshold_sensitive_patterns"], "scope.threshold_sensitive_patterns", nonempty=True
    )
    _int(scope["maximum_default_files"], "scope.maximum_default_files", 1, 25)
    _int(scope["maximum_default_changed_lines"], "scope.maximum_default_changed_lines", 1, 2000)

    reporting = _object(
        top["reporting"],
        "reporting",
        {
            "use_control_issue",
            "update_project_docs_only_when_state_moves",
            "draft_pr_template",
            "local_report_path",
            "commit_periodic_reports",
        },
    )
    if _bool(reporting["use_control_issue"], "reporting.use_control_issue") is not True:
        raise AutomationContractError("control issue cannot be disabled")
    if _bool(
        reporting["update_project_docs_only_when_state_moves"],
        "reporting.update_project_docs_only_when_state_moves",
    ) is not True:
        raise AutomationContractError("state docs must only move with real state")
    _string(reporting["draft_pr_template"], "reporting.draft_pr_template")
    local_report = _string(reporting["local_report_path"], "reporting.local_report_path")
    if not local_report.startswith("build/"):
        raise AutomationContractError("periodic report must stay under ignored build/")
    if _bool(reporting["commit_periodic_reports"], "reporting.commit_periodic_reports") is not False:
        raise AutomationContractError("periodic reports must never be committed")

    no_ops = _list_of_strings(top["no_op_results"], "no_op_results", nonempty=True)
    if no_ops != list(NO_OP_RESULTS):
        raise AutomationContractError("no-op result vocabulary changed")
    return top


def load_control(path: Path) -> dict[str, Any]:
    return validate_control(load_json_document(path))


def validate_control_transition(old: Mapping[str, Any], new: Mapping[str, Any], actor: str) -> None:
    old_doc = validate_control(dict(old))
    new_doc = validate_control(dict(new))
    if actor == "AUTOMATION" and canonical_json(old_doc) != canonical_json(new_doc):
        raise AutomationContractError("automation cannot modify its own control policy")
    if MODE_RANK[new_doc["mode"]] > MODE_RANK[old_doc["mode"]] and actor != "OWNER":
        raise AutomationContractError("only the owner may raise autonomy mode")


def validate_work_item(item: Any) -> dict[str, Any]:
    fields = {
        "id",
        "title",
        "campaign",
        "status",
        "risk_class",
        "priority",
        "exact_base_sha",
        "base_resolution_rule",
        "allowed_paths",
        "forbidden_paths",
        "acceptance_criteria",
        "required_tests",
        "owner_gate",
        "visual_gate",
        "private_data_required",
        "dependencies",
        "conflicts",
        "maximum_scope",
        "readiness_authority",
        "owner_approved_by",
        "owner_approved_at",
    }
    value = _object(item, "work_item", fields)
    work_id = _string(value["id"], "work_item.id")
    if not WORK_ID_RE.fullmatch(work_id):
        raise AutomationContractError("work_item.id is invalid")
    for name in ("title", "campaign"):
        _string(value[name], f"work_item.{name}")
    if value["status"] not in STATUSES:
        raise AutomationContractError("work_item.status is invalid")
    if value["risk_class"] not in RISK_CLASSES:
        raise AutomationContractError("work_item.risk_class is invalid")
    _int(value["priority"], "work_item.priority", 0, 1000)
    exact = value["exact_base_sha"]
    rule = value["base_resolution_rule"]
    if exact is not None and (not isinstance(exact, str) or not SHA_RE.fullmatch(exact)):
        raise AutomationContractError("work_item.exact_base_sha must be null or a full SHA")
    if rule is not None and not isinstance(rule, str):
        raise AutomationContractError("work_item.base_resolution_rule must be null or string")
    if (exact is None) == (rule is None):
        raise AutomationContractError("work item needs exactly one base selector")
    for name in (
        "allowed_paths",
        "forbidden_paths",
        "acceptance_criteria",
        "required_tests",
        "dependencies",
        "conflicts",
    ):
        _list_of_strings(value[name], f"work_item.{name}", nonempty=name in {"allowed_paths", "acceptance_criteria", "required_tests"})
    for name in ("owner_gate", "visual_gate", "private_data_required"):
        _bool(value[name], f"work_item.{name}")
    maximum = _object(
        value["maximum_scope"],
        "work_item.maximum_scope",
        {"max_files", "max_changed_lines", "single_subsystem"},
    )
    _int(maximum["max_files"], "maximum_scope.max_files", 1, 25)
    _int(maximum["max_changed_lines"], "maximum_scope.max_changed_lines", 1, 2000)
    _bool(maximum["single_subsystem"], "maximum_scope.single_subsystem")
    if value["readiness_authority"] not in ("OWNER", "NONE"):
        raise AutomationContractError("work_item.readiness_authority is invalid")
    approved_by = value["owner_approved_by"]
    approved_at = value["owner_approved_at"]
    if approved_by is not None and not isinstance(approved_by, str):
        raise AutomationContractError("work_item.owner_approved_by must be null or string")
    if approved_at is not None and not isinstance(approved_at, str):
        raise AutomationContractError("work_item.owner_approved_at must be null or string")
    if value["status"] == "AGENT_READY":
        if value["risk_class"] != "A2":
            raise AutomationContractError("AGENT_READY must be risk A2")
        if value["readiness_authority"] != "OWNER":
            raise AutomationContractError("AGENT_READY must be owner-authorized")
        if not approved_by or approved_by.lower().startswith(("agent", "automation", "codex")):
            raise AutomationContractError("AGENT_READY needs a real owner identity")
        if not approved_at:
            raise AutomationContractError("AGENT_READY needs owner approval time")
    if value["risk_class"] in ("A3", "A4") and value["status"] == "AGENT_READY":
        raise AutomationContractError("A3/A4 can never be AGENT_READY")
    return value


def load_work_items(path: Path) -> list[dict[str, Any]]:
    document = load_json_document(path)
    root = _object(document, "WORK_ITEMS", {"schema_version", "items"})
    if root["schema_version"] != 1:
        raise AutomationContractError("unsupported WORK_ITEMS schema_version")
    if not isinstance(root["items"], list):
        raise AutomationContractError("WORK_ITEMS.items must be a list")
    items = [validate_work_item(item) for item in root["items"]]
    ids = [item["id"] for item in items]
    if len(ids) != len(set(ids)):
        raise AutomationContractError("duplicate work item id")
    return items


def normalize_repo_path(path: str) -> str:
    candidate = path.replace("\\", "/").strip("/")
    logical = PurePosixPath(candidate)
    if not candidate or logical.is_absolute() or ".." in logical.parts:
        raise AutomationContractError(f"unsafe repository path: {path}")
    return logical.as_posix()


def path_matches(path: str, patterns: Iterable[str]) -> bool:
    normalized = normalize_repo_path(path)
    for raw in patterns:
        pattern = normalize_repo_path(raw.replace("**", "__DOUBLESTAR__"))
        pattern = pattern.replace("__DOUBLESTAR__", "*")
        if fnmatch.fnmatchcase(normalized, pattern):
            return True
        if raw.endswith("/**") and normalized.startswith(raw[:-3].rstrip("/") + "/"):
            return True
    return False


def run_command(args: Sequence[str], cwd: Path, timeout: int = 120) -> CommandResult:
    try:
        completed = subprocess.run(
            list(args),
            cwd=str(cwd),
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return CommandResult(127, "", str(exc))
    return CommandResult(completed.returncode, completed.stdout.strip(), completed.stderr.strip())


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def parse_utc(value: str | None) -> datetime | None:
    if value is None:
        return None
    if not isinstance(value, str):
        raise AutomationContractError("timestamp must be null or string")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise AutomationContractError("timestamp is not ISO-8601") from exc
    if parsed.tzinfo is None:
        raise AutomationContractError("timestamp must include timezone")
    return parsed.astimezone(timezone.utc)


def validate_control_issue_state(value: Any) -> dict[str, Any]:
    fields = {
        "schema_version",
        "enabled",
        "mode",
        "active_campaign",
        "authoritative_branch",
        "authoritative_head",
        "current_owner_gate",
        "current_visual_gate",
        "current_private_gate",
        "active_run_id",
        "lease_started_at",
        "lease_expires_at",
        "active_branch",
        "active_pr",
        "last_completed_run",
        "last_result",
    }
    state = _object(value, "control_issue_state", fields)
    if state["schema_version"] != 1:
        raise AutomationContractError("unsupported control issue schema")
    _bool(state["enabled"], "control_issue_state.enabled")
    if state["mode"] not in MODES:
        raise AutomationContractError("control issue mode invalid")
    for name in (
        "active_campaign",
        "authoritative_branch",
        "current_owner_gate",
        "current_visual_gate",
        "current_private_gate",
        "last_result",
    ):
        _string(state[name], f"control_issue_state.{name}")
    if not SHA_RE.fullmatch(_string(state["authoritative_head"], "authoritative_head")):
        raise AutomationContractError("authoritative_head must be a full SHA")
    for name in ("active_run_id", "active_branch", "active_pr", "last_completed_run"):
        if state[name] is not None and not isinstance(state[name], str):
            raise AutomationContractError(f"control_issue_state.{name} must be null or string")
    if state["active_run_id"] is not None and not RUN_ID_RE.fullmatch(state["active_run_id"]):
        raise AutomationContractError("active_run_id invalid")
    started = parse_utc(state["lease_started_at"])
    expires = parse_utc(state["lease_expires_at"])
    lease_values = (state["active_run_id"], started, expires)
    if any(item is None for item in lease_values) and not all(item is None for item in lease_values):
        raise AutomationContractError("lease fields must be all null or all populated")
    if started and expires and expires <= started:
        raise AutomationContractError("lease expiration must be after start")
    return state


def extract_control_issue_state(body: str) -> dict[str, Any]:
    if not isinstance(body, str):
        raise AutomationContractError("control issue body must be text")
    if body.count(CONTROL_START) != 1 or body.count(CONTROL_END) != 1:
        raise AutomationContractError("control issue needs exactly one JSON control block")
    start = body.index(CONTROL_START) + len(CONTROL_START)
    end = body.index(CONTROL_END, start)
    block = body[start:end].strip()
    if block.startswith("```json") and block.endswith("```"):
        block = block[7:-3].strip()
    try:
        value = json.loads(block, object_pairs_hook=_duplicate_guard)
    except json.JSONDecodeError as exc:
        raise AutomationContractError("control issue JSON block is invalid") from exc
    return validate_control_issue_state(value)


def replace_control_issue_state(body: str, state: Mapping[str, Any]) -> str:
    validated = validate_control_issue_state(dict(state))
    start = body.index(CONTROL_START) + len(CONTROL_START)
    end = body.index(CONTROL_END, start)
    block = "\n```json\n" + json.dumps(validated, indent=2, sort_keys=True) + "\n```\n"
    return body[:start] + block + body[end:]


def machine_result(code: str, **details: Any) -> dict[str, Any]:
    return {"schema_version": 1, "result": code, **details}
