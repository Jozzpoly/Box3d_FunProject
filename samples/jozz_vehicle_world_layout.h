// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

// Single source of truth for the map layout. Current entry point:
// docs/MAPA_INDEX_PL.md. Historical rationale: docs/archive/map_scan_2026-07/PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md §5.
// Etap 2-6 read these constants
// instead of re-deriving zone rectangles; nothing here allocates or touches
// box3d state.
//
//   z+ (polnoc)
//   +----------------------+----------------------+----------------------+
//   | NW: technical yard  | N: ramp yard         | NE: future connector |
//   +----------------------+----------------------+----------------------+
//   | W: articulation     | C: CENTRAL CAMPUS    | E: rock islands /     |
//   | and dense detail    | spawn + low tests    | offroad gate          |
//   +----------------------+----------------------+----------------------+
//   | SW: physics yard    | S: bumper/stress     | SE: landing/stress    |
//   |                     | approach             | yard                 |
//   +----------------------+----------------------+----------------------+
//                                 -> OFFROAD (E1) at x=+200

namespace JozzWorldLayout
{

// --- Plyta (3x3 kafle, top y=0) -------------------------------------------
constexpr float kPlateExtent = 400.0f;			   // total footprint, x and z
constexpr int kPlateTileCount = 3;					   // 3x3 grid
constexpr float kPlateTileSize = kPlateExtent / (float)kPlateTileCount; // 133.333...
constexpr float kPlateTileHalf = kPlateTileSize * 0.5f;			 // 66.666...
constexpr float kPlateHalfExtent = kPlateExtent * 0.5f;			 // 200.0, edge at x=+-200
constexpr float kPlateTopY = 0.0f;
constexpr float kPlateBodyY = -1.0f; // body origin; tile half-height is 1.0

// --- Offroad heightfield chunk (E1) ----------------------------------------
// Attached to the plate's east edge (x=+200) with a 2 m strip that dips UNDER
// the plate (the seam requirement from Jozz's feedback: no upward step).
// Etap 1 doszlifowanie (2026-07-12): resized 320->400 so plate and offroad
// are "two equal-size map tiles" per Jozz's screenshot annotation.
constexpr float kOffroadSize = 400.0f;
constexpr float kOffroadCellSize = 1.25f;
constexpr int kOffroadGridPoints = 321; // 320 cells * 1.25 m = 400 m
constexpr float kOffroadOverlap = 2.0f; // how far the chunk reaches under the plate
constexpr float kOffroadOriginX = kPlateHalfExtent - kOffroadOverlap; // 198.0
constexpr float kOffroadOriginZ = -kOffroadSize * 0.5f;			   // -200.0
constexpr float kOffroadGlobalMinHeight = -12.0f;
// Raised 14->22->28 (E1 final polish 2026-07-12, second pass after Jozz drove
// the mountain and asked for it taller + real ridges) for headroom above the
// taller summit and its arms.
constexpr float kOffroadGlobalMaxHeight = 28.0f;
constexpr uint32_t kOffroadDefaultSeed = 1337u;

// Noise layers (local x measured from the seam at local x=0 == world x=198).
// Macro is ridged (1-|noise|)^2, 2 octaves, sampled through a domain warp so
// ridgelines curve instead of following the noise lattice's axes (mountain
// character, Jozz's "closer to real mountains" ask). elevationShape (the
// [0,1] ridged blend, see world_terrain.cpp) doubles as the height signal
// that drives roughness below - no separate "flatness mask" layer needed.
constexpr float kMacroWavelength = 90.0f;
constexpr float kMacroAmplitude = 8.0f;
constexpr float kMacroOctave2WavelengthScale = 0.45f; // second ridged octave: finer detail on the same ridges
constexpr float kMacroOctave2Weight = 0.3f;		   // 0 = only octave 1, 1 = only octave 2
constexpr float kWarpWavelength = 60.0f;
constexpr float kWarpStrength = 22.0f;
constexpr float kMesoWavelength = 16.0f;
constexpr float kMesoAmplitude = 1.2f;
constexpr float kMicroWavelength = 2.8f;
constexpr float kMicroAmplitude = 0.22f;

// Roughness: how much of meso/micro amplitude actually shows up at a point.
// Driven by elevationShape (higher = rockier) but gated by its own noise
// layer so "high == rough" is a trend, not a mechanical rule (Jozz: "z
// odpowiednim szumem na wystepowanie tego szumu"). Floors keep valleys from
// going perfectly billiard-smooth; LerpLow/High let the noise both damp
// roughness on high ground (smooth outcrop) and boost it on lower ground
// (exposed rock patch) relative to the pure elevation signal.
constexpr float kRoughnessWavelength = 30.0f;
constexpr float kRoughnessMesoFloor = 0.2f;
constexpr float kRoughnessMicroFloor = 0.1f;
constexpr float kRoughnessNoiseLerpLow = 0.4f;
constexpr float kRoughnessNoiseLerpHigh = 1.3f;

constexpr float kDifficultyGradientDistance = 60.0f; // 0 at the seam, 1 by this far in
constexpr float kSeamOverlapRunLocalX = 4.0f * kOffroadCellSize; // ~5 m ramp-in
constexpr float kSeamOverlapDepth = -0.12f;					   // height at local x=0

// --- Central mountain (E1 final polish, 2026-07-12) ------------------------
// Jozz's ask: the terrain lacked a focal point - grow ONE natural mountain
// somewhere near the middle, summit ~1.5x higher than the standard ~8 m base
// peaks, with its own realistic multi-scale noise. Built from five stacked
// mechanisms (see world_terrain.cpp ComputeMountain): (1) seed-jittered center
// so "Przebuduj teren" re-rolls its position; (2) a smoothstep radial mass -
// the gradient that lifts the summit and blends into the base at the foot;
// (3) a domain warp on the distance field so the footprint is not a circle;
// (4) an angular spur modulation (radiating ridges/gullies, not a volcano
// cone); (5) a dedicated 4-octave ridged FBM for craggy summit relief that
// fades toward the base. The base terrain is suppressed under the mass so the
// mountain REPLACES the local rolling ground instead of doubling on top of it.
constexpr float kMountainCenterLocal = kOffroadSize * 0.5f; // 200 m nominal center
constexpr float kMountainJitter = 45.0f;				   // seed-driven offset from center
// Etap 1 second final polish (2026-07-12, Jozz drove the first pass): taller
// summit (12.5->17), wider base to keep the flank driveable at the new height
// (95->110), craggier relief (5.0->5.5).
constexpr float kMountainRadius = 110.0f;				   // base radius (footprint ~220 m across)
constexpr float kMountainPeakHeight = 17.0f;			   // additive summit mass
constexpr float kMountainWarpWavelength = 70.0f;		   // outline-warp lattice
constexpr float kMountainWarpStrength = 30.0f;			   // +-30 m footprint wander
constexpr float kMountainSpurRingRadius = 2.5f;			   // spur COUNT ~ 2*pi*r / wavelength
constexpr float kMountainSpurWavelength = 1.0f;
constexpr float kMountainSpurAmount = 0.22f;			   // +-22% radius wobble -> footprint outline only
constexpr float kMountainSurfaceWavelength = 34.0f;		   // coarsest summit-noise octave
constexpr int kMountainSurfaceOctaves = 4;				   // 34 / 17 / 8.5 / 4.25 m detail
constexpr float kMountainSurfaceAmp = 5.5f;				   // craggy relief amplitude at summit
constexpr float kMountainSurfaceBias = 0.35f;			   // ~mean of ridged FBM; centers crest/gully
constexpr float kMountainBaseSuppress = 0.55f;			   // base-terrain fade under the mass

// --- Mountain arms / "wezly gorskie" (second final polish, 2026-07-12) -----
// Jozz drove the first-pass mountain and asked for real ridges: the old
// kMountainSpur* above only wobbles the SUMMIT'S OWN footprint outline +-22%
// - it never grows relief of its own, so it can't read as "arms". This is a
// separate mechanism: a handful of dominant ridges radiating from the flank,
// living in a ring around the mountain's own base (still overlapping its
// lower slope on the inside, gone well past it on the outside), each one
// carrying additive sub-peaks along its own crest ("wezly gorskie powinny
// tworzyc na sobie mniejsze gory"). See ComputeMountain in world_terrain.cpp.
constexpr float kArmRingWavelength = 1.35f; // angular sample wavelength on a unit circle -> ~4-6 dominant lobes
constexpr float kArmRingRadius = 1.0f;		 // unit-circle sampling radius for the angular field
constexpr float kArmSharpness = 2.2f;		 // power curve on the ridged angular field: higher = narrower, more dominant spokes
constexpr float kArmInnerRadiusScale = 0.35f;	// arms start fading IN at this fraction of kMountainRadius (still on the flank)
constexpr float kArmMidRadiusScale = 0.55f;	// ...fully grown by here
constexpr float kArmOuterStartScale = 0.85f;	// aggressive falloff starts just past the mountain's own foot
constexpr float kArmOuterEndScale = 1.7f;		// ...and is fully gone by here
constexpr float kArmHeightScale = 0.5f;		// baseline arm ridge height, as a fraction of kMountainPeakHeight
constexpr float kArmSubPeakWavelength = 22.0f; // smaller peaks riding along each arm's crest
constexpr float kArmSubPeakAmp = 4.0f;			// additive bump amplitude at those sub-peak crests

// --- Edge fade (second final polish) ----------------------------------------
// A pechowy seed can otherwise grow the massif right up against the map's
// far/side boundaries and cut it off flat. The mountain+arms fade fully to 0
// in the last kEdgeFadeDistance meters before z=+-200 or the far edge x=598;
// the near edge (local x=0) is the seam and already has the difficulty
// gradient, so it needs no extra fade. Base rolling terrain only eases down
// to kEdgeFadeBaseFloor, not to zero - the boundary keeps modest texture.
constexpr float kEdgeFadeDistance = 35.0f;
constexpr float kEdgeFadeBaseFloor = 0.7f;

// --- Etap 2R + masterplan satellite yards -------------------------------
// The old x=150..195 six-lane strip is intentionally gone. C is the central
// campus; larger content gets explicit satellite yards on adjacent tiles so
// later Etap 3/4 work can grow without inventing a second layout system.
enum JozzWorldTileId
{
	kTileNW,
	kTileN,
	kTileNE,
	kTileW,
	kTileC,
	kTileE,
	kTileSW,
	kTileS,
	kTileSE,
};

struct JozzWorldYard
{
	const char* name;
	JozzWorldTileId tile;
	float minX;
	float maxX;
	float minZ;
	float maxZ;
};

constexpr float kCentralCampusUsableMin = -60.0f;
constexpr float kCentralCampusUsableMax = 60.0f;

// E3 skeleton envelope. The long loop is intentionally a composite N/NW/NE
// layout rather than a new central-campus rectangle. It leaves a generous
// clear margin around C and keeps the future physical split at tile seams
// explicit.
constexpr float kLongTrackMinX = -185.0f;
constexpr float kLongTrackMaxX = 185.0f;
constexpr float kLongTrackMinZ = 85.0f;
constexpr float kLongTrackMaxZ = 182.0f;

constexpr JozzWorldYard kMasterplanYards[] = {
	{ "Ramp Yard N", kTileN, -60.0f, 60.0f, 72.0f, 160.0f },
	{ "Technical Yard NW", kTileNW, -160.0f, -72.0f, 72.0f, 160.0f },
	{ "Track Gateway NE", kTileNE, 72.0f, 160.0f, 72.0f, 160.0f },
	{ "Offroad Gate E", kTileE, 72.0f, 160.0f, -60.0f, 60.0f },
	{ "Physics Yard SW", kTileSW, -160.0f, -72.0f, -160.0f, -72.0f },
	{ "Stress Landing Yard SE", kTileSE, 72.0f, 160.0f, -160.0f, -72.0f },
};
constexpr int kMasterplanYardCount = sizeof( kMasterplanYards ) / sizeof( kMasterplanYards[0] );

// --- Teleporty minimalne (P1, pelny rejestr dopiero w Etapie 6) ------------
struct JozzWorldAnchor
{
	const char* name;
	float x;
	float z;
};

constexpr JozzWorldAnchor kWorldAnchors[] = {
	{ "Start", 0.0f, 0.0f },
	{ "Centralny kampus", 0.0f, 0.0f },
	{ "Plac rampowy N", 0.0f, 110.0f },
	{ "Plac fizyki SW", -115.0f, -115.0f },
	{ "Plac ladowan SE", 115.0f, -115.0f },
	{ "Offroad - wjazd", 240.0f, 0.0f },
	{ "Offroad - gora", kOffroadOriginX + kMountainCenterLocal - 70.0f, kOffroadOriginZ + kMountainCenterLocal },
	{ "Offroad - gleboko", 560.0f, 0.0f },
	// Compatibility alias for existing teleport scripts; it now resolves to C.
	{ "Poligon zawieszen", 0.0f, 0.0f },
};
constexpr int kWorldAnchorCount = sizeof( kWorldAnchors ) / sizeof( kWorldAnchors[0] );

// --- Skan (wyspa): stałe pozycjonowania (import skanu, 2026-07-24) ----------
// A photogrammetry scan imported as a SEPARATE island NORTH of the plate. The
// body origin is computed at LOAD time from the scan's own bounds
// (jozz_vehicle_scan_import) so any scan size clears the existing world: the
// island's near (south) edge is pinned at kScanSouthEdgeZ - well north of the
// plate's north edge (z=+200) - and its lowest point sits at kScanGroundY.
// Nothing here allocates box3d state; only the teleport target reads it.
constexpr float kScanSouthEdgeZ = 320.0f; // world z of the island's south edge
constexpr float kScanGroundY = 0.0f;      // world y of the island's lowest point

// --- Klasyfikator fragmentu mapy (import skanu, 2026-07-24) -----------------
// Which driveable fragment a world (x,z) falls in: central plate/campus,
// procedural offroad chunk, or the imported photogrammetry island. Pure (no
// box3d types) so the validator links it; used by the per-fragment spawn UI.
enum JozzMapFragment
{
	FragmentPlate = 0,	 // central test plate + campus (west of the offroad seam)
	FragmentOffroad = 1, // procedural offroad chunk east of the plate (x >= seam)
	FragmentScan = 2,	 // imported photogrammetry island (north, teleport-only)
};

// scanLoaded + the island's world-AABB x/z extents let the scan take priority
// over the offroad rectangle it sits far north of; pass scanLoaded=false (and
// the bounds are ignored) when no island is live. The offroad seam is the same
// kOffroadOriginX the chunk is built at, so classification can't drift from it.
inline JozzMapFragment ClassifyJozzMapFragment( float x, float z, bool scanLoaded, float scanMinX, float scanMaxX,
												float scanMinZ, float scanMaxZ )
{
	if ( scanLoaded && x >= scanMinX && x <= scanMaxX && z >= scanMinZ && z <= scanMaxZ )
	{
		return FragmentScan;
	}
	if ( x >= kOffroadOriginX )
	{
		return FragmentOffroad;
	}
	return FragmentPlate;
}

} // namespace JozzWorldLayout
