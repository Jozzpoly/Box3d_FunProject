// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// Mapa Etap 2 (docs/MAPA_INDEX_PL.md): a parametric
// library of static drivable obstacles used to assemble the suspension-test
// lane poligon (and, later, Etap 3's track curbs/berms). Every Add* function
// builds exactly ONE static body named "kit_<type>" and tags every shape it
// creates with terrainCategoryBits (R5 - everything drivable needs the
// terrain category or the M6 split wheel envelope's rolling sphere cannot
// see it).
//
// Two anchor conventions, documented per function below:
//   ENTRY anchor  - `position` is the low/entry edge at local ground level
//                   (y measured from that point); local +X (after yaw) is
//                   the direction of travel INTO the obstacle. Used by
//                   ramps, steps and stairs, where the car approaches from
//                   one specific side.
//   CENTER anchor - `position` is the footprint's ground-level center; the
//                   obstacle is roughly symmetric about it. Used by
//                   rhythmic/field obstacles the car drives straight through.
//
// yawDegrees rotates the whole obstacle around +Y using the same convention
// as vehicle steering (README_FOR_AGENTS §4): forward=+X, up=+Y, right=+Z; a
// positive yaw turns the local +X axis toward -Z (left). All Etap 2 lane
// stations use yaw=0 (lanes run straight along +X); a future track stage
// should re-verify non-zero yaw with a render before trusting it blindly.
//
// Roundedness spectrum (E2 doc §3): capsules give the only genuinely round
// static primitive (lips, whoops, logs, speed bumps); everything sharp is a
// b3MakeTransformedBoxHull box. No engine changes either way.
//
// customColorHex (E2 doc §2.5/P8): optional b3HexColor tint forwarded
// straight into every shape's shapeDef.baseMaterial.customColor. 0 (default)
// means "no override" - the renderer's normal per-material color. The lane
// poligon uses this to paint each station green/yellow/red by difficulty.

#include "box3d/box3d.h"

// ---- Skocznie / rampy (ENTRY anchor) --------------------------------------

// A single sharp wedge: a tilted box, its low edge buried at `entry`, rising
// over `length` meters to `angleDegrees`.
void AddWedgeRamp( b3WorldId worldId, b3Pos entry, float yawDegrees, float length, float width, float angleDegrees,
					uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// Same wedge as AddWedgeRamp, plus a capsule running along the take-off edge
// (radius `lipRadius`) so the leading edge is rounded instead of a hard
// corner - a softer, more forgiving launch.
void AddKicker( b3WorldId worldId, b3Pos entry, float yawDegrees, float length, float width, float angleDegrees,
				 float lipRadius, uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// Three-box safe jump: approach ramp up to `height`, a flat table
// (`tableLength` long), then a landing ramp back down to ground over
// `landingLength`. The approach/landing angles are derived from height and
// their respective run lengths, not passed explicitly.
void AddTabletop( b3WorldId worldId, b3Pos entry, float yawDegrees, float approachLength, float tableLength,
				   float landingLength, float height, float width, uint64_t terrainCategoryBits,
				   uint32_t customColorHex = 0 );

// Two ramps facing each other across an empty gap: a hard energy/airtime
// test, no forgiving table in between. Both ramps share `rampHeight` and
// `rampAngleDegrees`; their run length is derived (height / sin(angle)).
void AddGapJump( b3WorldId worldId, b3Pos entry, float yawDegrees, float rampHeight, float rampAngleDegrees,
				  float gapLength, float width, uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// ---- Uskoki (ENTRY anchor) -------------------------------------------------

// A raised platform: a sheer vertical face at `entry`, height `height`,
// extending `depth` meters in local +X. Drive up onto it head-on.
void AddStepUp( b3WorldId worldId, b3Pos entry, float yawDegrees, float width, float height, float depth,
				 uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// The same raised-platform geometry as AddStepUp; use it where the vehicle
// arrives ALREADY elevated (e.g. after a preceding ramp) and drops off the
// far edge (`entry.x + depth`) back down to the ambient ground - the plate
// underneath is already there, so no separate landing shape is needed.
void AddStepDown( b3WorldId worldId, b3Pos entry, float yawDegrees, float width, float height, float depth,
				   uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// ---- Rytmiczne (CENTER anchor) --------------------------------------------

enum JozzBumperPattern
{
	kJozzBumperFullWidth = 0,
	kJozzBumperAlternatingSides = 1,
	kJozzBumperWave = 2,
};

// A dense, deterministic bumper bank. `centerY` is explicit in world metres
// relative to the ground plane: it is intentionally not inferred from the
// radius, because low comfort bumpers and raised logs need different profiles.
// Alternating patterns use shorter left/right capsules so the car sees a
// deliberate rhythm rather than one accidental wall.
void AddBumperBank( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float spacing, float radius,
					float width, float centerY, float sideOffset, JozzBumperPattern pattern, uint64_t terrainCategoryBits,
					uint32_t customColorHex = 0 );

// A row of `count` capsule moguls spanning `width`, spaced `spacing` apart
// along local X, each partly embedded so it reads as a molded dip-and-rise
// rather than a loose log. Classic rhythmic whoops section.
void AddWhoops( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float spacing, float radius, float width,
				 uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// A single embedded capsule ridge across the lane - a traffic-calming bump.
void AddSpeedBump( b3WorldId worldId, b3Pos center, float yawDegrees, float radius, float width,
					uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// `count` thin sharp slats across the lane, spaced `spacing` apart, each
// `height` tall - the harsh, high-frequency contrast to AddWhoops' rounded
// moguls.
void AddWashboard( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float spacing, float height,
					float width, uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// ---- Teren trudny (CENTER anchor) -----------------------------------------

// A field of randomly placed, randomly rotated and partly buried boxes
// ("rocks") over a `lengthX` x `widthZ` footprint. `density` is rocks per
// square meter; size is drawn from [minSize, maxSize]. Deterministic for a
// given seed (re-rolls exactly like the offroad terrain's "Przebuduj teren").
void AddRockGarden( b3WorldId worldId, b3Pos center, float yawDegrees, float lengthX, float widthZ, float density,
						 float minSize, float maxSize, uint64_t terrainCategoryBits, uint32_t seed,
						 uint32_t customColorHex = 0 );

// Clustered rock island for high-density engine testing. One static body owns
// many deterministic hull shapes; clusters and an exclusion radius keep the
// result readable and avoid the old single sparse scatter.
void AddRockIsland( b3WorldId worldId, b3Pos center, float yawDegrees, float lengthX, float widthZ, int clusterCount,
					int rocksPerCluster, float clusterRadius, float minSize, float maxSize, uint64_t terrainCategoryBits,
					uint32_t seed, uint32_t customColorHex = 0 );

// Two parallel V-groove trenches, one per wheel track (`trackWidth` apart),
// `depth` deep, `length` long - drive IN a rut and climb back OUT of it.
void AddRuts( b3WorldId worldId, b3Pos center, float yawDegrees, float length, float depth, float trackWidth,
			   uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// A flat plate tilted sideways (roll about local X) by `tiltDegrees`,
// `length` x `width`, embedded so its downhill edge touches ground -
// a lateral-grip/roll test.
void AddOffCamber( b3WorldId worldId, b3Pos center, float yawDegrees, float length, float width, float tiltDegrees,
					uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// A banked arc of radius `archRadius`: `segmentCount` tangent box segments
// sweeping `archDegrees` symmetrically about `center`, each banked by
// `bankAngleDegrees`. `center` is the MIDPOINT of the arc's own path (drive
// straight through it, same as any other CENTER-anchor obstacle) - not the
// circle's geometric pivot, which sits `archRadius` away in local +Z.
void AddBerm( b3WorldId worldId, b3Pos center, float yawDegrees, float archRadius, float archDegrees, float width,
			   float bankAngleDegrees, int segmentCount, uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// A staircase of `stepCount` solid steps, each `stepHeight` down and
// `stepDepth` deep, descending from `entry` (the top landing, at local
// ground level) in local +X.
void AddStairs( b3WorldId worldId, b3Pos entry, float yawDegrees, int stepCount, float stepHeight, float stepDepth,
				  float width, uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// `count` capsule logs spanning `width`, spaced `spacing` apart along local
// X - larger radius and shallower embed than AddWhoops so they read as
// distinct round obstacles sitting mostly above the ground, not molded dips.
void AddLogs( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float radius, float spacing, float width,
			   uint64_t terrainCategoryBits, uint32_t customColorHex = 0 );

// Two independent low-profile humps, one per wheel path, offset from each
// other by `offsetLR` along local X so the two sides are never at the same
// height at the same time. Each path has a complete ascent and descent; the
// function must never leave a raised ramp ending in a hard drop.
void AddArticulationRamps( b3WorldId worldId, b3Pos center, float yawDegrees, float length, float width,
							 float angleDegrees, float offsetLR, uint64_t terrainCategoryBits,
							 uint32_t customColorHex = 0 );
