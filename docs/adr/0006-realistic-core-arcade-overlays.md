# ADR 0006 — Realistic core, opt-in [ARCADE] overlays

Status: Accepted (Jozz, 2026-07-09)
Date: 2026-07-09

## Decision

The game's core is **maximally realistic and physical**: every default
behavior must emerge from the mechanical construction (geometry, springs,
friction, torque vs grip), never from a script or a corrective controller.

Non-realistic overlays (e.g. `rackCenteringHertz` return-to-center assist,
`uprightAssist` anti-rollover helper) ARE allowed for fun — but:

1. **Default OFF.** They are enabled consciously by the player/Jozz, never
   silently, never as a fix for a physics problem.
2. **Explicitly labeled** in the UI with the `[ARCADE]` prefix on the control
   label, plus a HelpMarker note that this is a mechanic outside the physical
   model ("mechanika poza modelem fizycznym").
3. A validator probe should pin the OFF state to realistic behavior wherever
   practical (e.g. `RunP4CenteringAssistProbe` asserts that with the assist
   at 0 a kicked wheel stays deflected at standstill — correct rest physics).

## Rationale

This rule existed implicitly since M7 (removal of the scripted self-align
"drift assist" as a documented negative result), but was never written down —
which is exactly why `rackCenteringHertz` became contested when it appeared:
there was no recorded line separating "realism core" from "optional arcade".
Writing the rule down is the vaccine against the next such dispute.

## Consequences

- Every future assist/helper goes through the same three requirements
  (default off, `[ARCADE]` label + HelpMarker, OFF-state probe if practical).
- A realism problem (e.g. steering return feel) must be fixed in the physical
  model, not masked by enabling an overlay by default.
- Existing overlays as of this ADR: `rackCenteringHertz` (steering
  return-to-center), `uprightAssist` (anti-rollover parallel joint). Both
  default off, both labeled `[ARCADE]` in the M6 lab UI.
- Presets MAY ship with an overlay enabled only if the preset's name/comment
  makes it obvious (e.g. a hypothetical "arcade" preset) — none do today.
