// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

// Single source of truth for the map layout (docs/MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md
// and PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md §5). Etap 2-6 read these constants
// instead of re-deriving zone rectangles; nothing here allocates or touches
// box3d state.
//
//   z+ (polnoc)
//   +-----------------------------------+--------+  <- z=190
//   |  TOR WYSCIGOWY (E3)               | POLIGO-|
//   |  x:-190..140, z:60..190           | NY     |   OFFROAD (E1)
//   +-----------------------------------+ ZAWIE- |   heightfield chunk
//   |        bufor / centrum @ (0,0)    | SZEN   |   origin (rog siatki):
//   +---------------+--------------------+ (E2)   |   x:198, z:-200
//   | DRIFT (E3)    | PLAC FIZYKI (E4)  | x:150..|   rozmiar 400x400
//   | x:-190..-30   | x:10..140         |   195  |   x: 198..598
//   | z:-190..-60   | z:-190..-60       | z:-60..|   z: -200..200
//   +---------------+--------------------+   60  +--------+  <- z=-190
//                                 styk x=200 (zakladka 2 m POD plyta)

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

// --- Skan (wyspa, fundament v3, 2026-07-24) --------------------------------
// A photogrammetry scan imported as a SEPARATE island NORTH of the plate,
// reached by teleport only, its footprint edges left as cliffs (decisions
// D4/D5). The body origin is computed at LOAD time from the scan's own bounds
// (jozz_vehicle_scan_import) so any scan size clears the existing world: the
// island's near (south) edge is pinned at kScanSouthEdgeZ - well north of the
// plate's north edge (z=+200) - and its lowest point sits at kScanGroundY.
// Nothing here allocates box3d state; only the teleport target reads it.
constexpr float kScanSouthEdgeZ = 320.0f; // world z of the island's south edge
constexpr float kScanGroundY = 0.0f;      // world y of the island's lowest point

// --- Poligon zawieszen (Etap 2, docs/MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md) -
// A 45x120 m strip sitting on the plate (east side, west of the offroad
// seam) holding 6 progression lanes. Cars enter from the west edge (local
// x=0, world x=kPoligonOriginX) and drive east; lanes are stacked along Z,
// one per ~20 m band. Single source of truth so the obstacle-kit course
// builder and any future UI/label code never re-derive these numbers.
constexpr float kPoligonOriginX = 150.0f;			// world x where a lane's stations start
constexpr float kPoligonLaneLength = 45.0f;		// world x: 150..195
constexpr float kPoligonOriginZ = -60.0f;			// world z: -60..60
constexpr float kPoligonExtentZ = 120.0f;
constexpr int kLaneCount = 6;
constexpr float kLaneBandWidth = kPoligonExtentZ / (float)kLaneCount; // 20 m per lane band
constexpr float kPoligonStationSpacing = 8.0f;		// nominal gap between stations along a lane (E2 doc §4)
constexpr float kPoligonStationMargin = 5.0f;		// first station's offset from kPoligonOriginX

// Difficulty tiers drive both the station label color (P8) and, loosely, how
// harsh each lane's obstacle parameters are. Kept as a plain enum (not a
// box3d color) so this header stays free of engine color types; the course
// builder maps tier -> b3HexColor.
enum JozzLaneDifficulty
{
	kLaneDifficultyEasy = 0,
	kLaneDifficultyMedium = 1,
	kLaneDifficultyHard = 2,
};

struct JozzLaneSpec
{
	const char* name;
	float zCenter;
	JozzLaneDifficulty difficulty;
};

// L1..L6, south to north (increasing Z), per the E2 doc's progression table.
constexpr JozzLaneSpec kLanes[kLaneCount] = {
	{ "L1 Komfort", kPoligonOriginZ + kLaneBandWidth * 0.5f, kLaneDifficultyEasy },
	{ "L2 Szuter", kPoligonOriginZ + kLaneBandWidth * 1.5f, kLaneDifficultyEasy },
	{ "L3 Rajd", kPoligonOriginZ + kLaneBandWidth * 2.5f, kLaneDifficultyMedium },
	{ "L4 Kamien", kPoligonOriginZ + kLaneBandWidth * 3.5f, kLaneDifficultyMedium },
	{ "L5 Ekstrema", kPoligonOriginZ + kLaneBandWidth * 4.5f, kLaneDifficultyHard },
	{ "L6 Skocznie", kPoligonOriginZ + kLaneBandWidth * 5.5f, kLaneDifficultyHard },
};

// --- Plac fizyki skraj (E2 doc pkt. 2.4: propy odsuniete z osi jazdy) ------
// The M5 test-course prop scatter used to sit on the driving axis around
// (0,0); Etap 2 moves it to the near edge of the future Etap 4 "plac fizyki"
// zone (x:10..140, z:-190..-60 per the master plan §5) so the centre stays
// clear and E4 can still claim the rest of that rectangle later.
// The scatter's own spread is about +-24 m in both axes (see kPropSpecs in
// jozz_vehicle_m5_test_course.cpp); origins are offset in from the plac-
// fizyki rectangle's x:10..140, z:-190..-60 so every prop lands inside it,
// biased toward its near (north, small-|z|) edge rather than its far end.
constexpr float kPropZoneOriginX = 40.0f;
constexpr float kPropZoneOriginZ = -95.0f;

// --- Teleporty minimalne (P1, pelny rejestr dopiero w Etapie 6) ------------
struct JozzWorldAnchor
{
	const char* name;
	float x;
	float z;
};

constexpr JozzWorldAnchor kWorldAnchors[] = {
	{ "Start", 0.0f, 0.0f },
	{ "Offroad - wjazd", 240.0f, 0.0f },
	{ "Offroad - gora", kOffroadOriginX + kMountainCenterLocal - 70.0f, kOffroadOriginZ + kMountainCenterLocal },
	{ "Offroad - gleboko", 560.0f, 0.0f },
	{ "Poligon zawieszen", kPoligonOriginX - 5.0f, 0.0f },
};
constexpr int kWorldAnchorCount = sizeof( kWorldAnchors ) / sizeof( kWorldAnchors[0] );

// --- Fragmenty mapy (per-fragment spawn system, 2026-07-24) -----------------
// The world is three driveable fragments the vehicle can respawn on, each with
// its own spawn point (session + persistent tiers, see jozz_vehicle_m6_rig_lab).
// This classifier answers "which fragment is (x,z) on" from the SAME rectangles
// the segments are built from, so the UI/checkpoint logic never re-derives them.
// Pure geometry (plain floats, no box3d types) so the headless validator links
// and tests it directly. Enum values double as array indices (0..2).
enum JozzMapFragment
{
	FragmentPlate = 0,	 // central test plate + poligon (west of the offroad seam)
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
