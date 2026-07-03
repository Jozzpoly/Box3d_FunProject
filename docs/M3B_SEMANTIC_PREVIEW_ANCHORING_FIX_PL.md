# M3B Semantic Preview Anchoring Fix

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: anchoring fix implemented and manually validated by Jozz screenshots

## 1. Problem reported by Jozz

Jozz validated the first M3B semantic preview build and reported that it appears to run, but raised a critical concern:

```text
Rest drop moves the additional markers with it.
If they are suspension markers, they should be independent from the wheels.
```

This is correct.

## 2. Root cause

The first M3B.1 implementation anchored both preview schematics to:

```cpp
GetLiveRestWheelCenterY()
```

That meant:

```text
wheel semantic preview      follows rest wheel center
suspension semantic preview follows rest wheel center
```

For the wheel preview this is defensible as a first schematic, but for the suspension preview it is misleading.

## 3. Why it is wrong for suspension preview

The suspension asset semantics describe a suspension/corner authoring object, not the dynamic wheel body itself.

The travel axis and suspension sockets should be reasoned about relative to the chassis/root side of the corner, not relative to a value that is edited as wheel rest drop.

If changing `Rest drop` moves the entire suspension schematic, the UI suggests the wrong mental model:

```text
suspension semantic markers are owned by the wheel/rest center
```

The correct mental model is closer to:

```text
wheel semantic preview follows the wheel/body side
suspension semantic preview follows the chassis/root side
```

## 4. Corrected anchoring rule

M3B semantic preview now uses separate anchors:

```text
wheel preview origin       = near actual primitive wheel body center
suspension preview origin  = near live chassis/root mount height
```

Consequences:

- wheel preview may move with the physical wheel/body;
- suspension preview follows live root/chassis movement;
- suspension preview does not move merely because `Rest drop` changes;
- neither preview drives physics.

## 5. Jozz screenshot analysis after fix

### 5.1 Live-root validation pair

Jozz first sent two screenshots after the anchoring fix with adjusted settings to make the preview easier to see.

Observed difference:

```text
Screenshot A: live root offset around +1.18
Screenshot B: live root offset around -0.32
Rest drop shown as 2.00 in both views
```

Interpretation:

- the panel/HUD reports the corrected model: `wheel->body, suspension->chassis/root`;
- the wheel schematic remains associated with the wheel/body side;
- the suspension schematic changes height when live root changes, which is expected because it follows chassis/root;
- this screenshot pair supports live-root/chassis anchoring behavior.

### 5.2 Rest-drop-focused validation pair

Jozz then sent a clearer set of screenshots with the same live root value and different rest-drop/rig relationship.

Key observed comparison:

```text
Live root offset: about +0.81 in both compared screenshots
Rest drop: around 1.39 in one view, around 0.25 in another view
Wheel/rest center: changes strongly
Suspension schematic: remains visually tied to chassis/root instead of being dragged wholesale with the wheel/rest center
```

Interpretation:

- changing rest drop moves the wheel/rest relationship as expected;
- the wheel-side schematic follows the wheel/body side;
- the magenta suspension travel schematic remains chassis/root-owned;
- the original bug is fixed enough to continue.

Manual validation status:

```text
accepted by Jozz screenshots for M3B.1 anchoring fix
```

## 6. Important distinction

This still does not solve final import transform.

The preview remains a schematic. It is not the final Blockbench-to-game mapping. The point is to make the semantic ownership clearer before real rendering/import work begins.

## 7. Validation checklist kept for future regressions

Future agents should still check:

1. `M3B semantic preview` is visible.
2. Wheel preview appears near/following the wheel body.
3. Suspension travel preview appears near/following the chassis/root side.
4. Changing `Rest drop` and pressing Apply should not simply drag the whole suspension schematic with the wheel rest center.
5. Live root movement may move the suspension schematic because live root moves the chassis/root.
6. Toggling preview still does not affect physics.
7. No glTF mesh is rendered.

Focused rest-drop-only regression check:

```text
Set Live root offset to the same value in both tests.
Change only Rest drop.
Press Apply rig rebuild.
Expected: wheel/rest center and wheel schematic may change relationship, but the suspension schematic should not be dragged wholesale as a wheel-owned group.
```

## 8. Final judgement

Jozz's critique caught an important mental-model bug before it became a deeper rig/import bug.

This is exactly why M3B.1 exists before mesh rendering: debug semantics must be understandable before pretty visuals are attached.

With the anchoring fix validated, the next safe work is either semantic-preview polish/labels or metadata loading without mesh rendering. Full rig/import should still wait.