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
//   +---------------+--------------------+ (E2)   |   x:198, z:-160
//   | DRIFT (E3)    | PLAC FIZYKI (E4)  | x:150..|   rozmiar 320x320
//   | x:-190..-30   | x:10..140         |   195  |   x: 198..518
//   | z:-190..-60   | z:-190..-60       | z:-60..|   z: -160..160
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
constexpr float kOffroadSize = 320.0f;
constexpr float kOffroadCellSize = 1.25f;
constexpr int kOffroadGridPoints = 257; // 256 cells * 1.25 m = 320 m
constexpr float kOffroadOverlap = 2.0f; // how far the chunk reaches under the plate
constexpr float kOffroadOriginX = kPlateHalfExtent - kOffroadOverlap; // 198.0
constexpr float kOffroadOriginZ = -kOffroadSize * 0.5f;			   // -160.0
constexpr float kOffroadGlobalMinHeight = -12.0f;
constexpr float kOffroadGlobalMaxHeight = 14.0f;
constexpr uint32_t kOffroadDefaultSeed = 1337u;

// Noise layers (local x measured from the seam at local x=0 == world x=198).
constexpr float kMacroWavelength = 90.0f;
constexpr float kMacroAmplitude = 8.0f;
constexpr float kMesoWavelength = 16.0f;
constexpr float kMesoAmplitude = 1.2f;
constexpr float kMicroWavelength = 2.8f;
constexpr float kMicroAmplitude = 0.22f;
constexpr float kFlatnessMaskWavelength = 140.0f;
constexpr float kDifficultyGradientDistance = 60.0f; // 0 at the seam, 1 by this far in
constexpr float kSeamOverlapRunLocalX = 4.0f * kOffroadCellSize; // ~5 m ramp-in
constexpr float kSeamOverlapDepth = -0.12f;					   // height at local x=0

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
	{ "Offroad - gleboko", 460.0f, 0.0f },
};
constexpr int kWorldAnchorCount = sizeof( kWorldAnchors ) / sizeof( kWorldAnchors[0] );

} // namespace JozzWorldLayout
