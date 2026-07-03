# ADR 0002 — Orientation policy

Status: Accepted  
Date: 2026-07-03

## Decision

Do not block the project on final Blockbench model orientation.

Jozz will adjust model orientation later, while testing visual output in the native game. Until then, sidecar asset contracts may contain temporary orientation correction metadata.

## Rationale

Trying to fix orientation blindly in Blockbench before seeing models in-game wastes time and creates confusion. The project can move forward if the importer makes corrections explicit and centralized.

## Consequences

- Runtime/game-space still needs one convention.
- Asset-local orientation may differ temporarily.
- Physics code must not contain random per-model rotation hacks.
- Importer/asset contract owns temporary corrections.
- Final authoring orientation cleanup becomes a later visual-pipeline task.
