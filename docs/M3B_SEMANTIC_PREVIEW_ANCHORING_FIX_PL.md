# M3B Semantic Preview Anchoring Fix

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: analysis before/with anchoring fix

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

M3B semantic preview should use separate anchors:

```text
wheel preview origin       = near actual primitive wheel body center
suspension preview origin  = near live chassis/root mount height
```

Consequences:

- wheel preview may move with the physical wheel/body;
- suspension preview follows live root/chassis movement;
- suspension preview does not move merely because `Rest drop` changes;
- neither preview drives physics.

## 5. Important distinction

This still does not solve final import transform.

The preview remains a schematic. It is not the final Blockbench-to-game mapping. The point is to make the semantic ownership clearer before real rendering/import work begins.

## 6. Validation after fix

After build, check:

1. `M3B semantic preview` is visible.
2. Wheel preview appears near/following the wheel body.
3. Suspension travel preview appears near/following the chassis/root side.
4. Changing `Rest drop` and pressing Apply should not simply drag the whole suspension schematic with the wheel rest center.
5. Live root movement may move the suspension schematic because live root moves the chassis/root.
6. Toggling preview still does not affect physics.
7. No glTF mesh is rendered.

## 7. Final judgement

Jozz's critique caught an important mental-model bug before it became a deeper rig/import bug.

This is exactly why M3B.1 exists before mesh rendering: debug semantics must be understandable before pretty visuals are attached.