// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// Fundament terenu (Etap 1 of docs/MAPA_INDEX_PL.md): the
// 3x3-tile flat plate plus the offroad FBM heightfield chunk that dips under
// its east edge. Pure content/course building, shared by both the M5 and M6
// labs; kept out of jozz_vehicle_validation (this is scenery for interactive
// testing, same boundary as jozz_vehicle_m5_test_course - see its header).
//
// terrainCategoryBits tags every drivable surface here (plate tiles and the
// offroad heightfield) so the M6 split wheel envelope's rolling sphere can
// see them (JOZZ_M6_TERRAIN_CATEGORY, see jozz_vehicle_m6_suspension_rig.h).
// The M5 lab passes the engine default (1) and gets the old no-split
// behavior, matching CreateJozzVehicleM5TestCourse's existing convention.

#include "jozz_vehicle_world_layout.h"

#include "box3d/box3d.h"

struct JozzVehicleWorldGround
{
	b3BodyId plateBodyId = b3_nullBodyId;
	b3ShapeId plateTileShapeIds[JozzWorldLayout::kPlateTileCount * JozzWorldLayout::kPlateTileCount] = {};
	b3BodyId offroadBodyId = b3_nullBodyId;
	b3ShapeId offroadShapeId = b3_nullShapeId;
	b3HeightFieldData* offroadField = nullptr;
	uint32_t seed = JozzWorldLayout::kOffroadDefaultSeed;
};

// Builds the whole ground: one static "world_plate" body carrying 9 offset
// box tiles (top y=0, zero gaps - see jozz_vehicle_world_layout.h), plus one
// static "world_offroad" body carrying the FBM heightfield chunk. Only the
// CENTER tile is registered with SetGroundShape (R10 from the roadmap: the
// debug renderer's procedural grid texture is keyed on a single shape id, not
// a list); the other 8 tiles get a neutral flat color via SetShapeMaterial so
// the seam between them is invisible.
JozzVehicleWorldGround CreateJozzWorldGround( b3WorldId worldId, uint64_t terrainCategoryBits,
											   uint32_t seed = JozzWorldLayout::kOffroadDefaultSeed );

// Frees the offroad height field data (box3d only holds a reference, per
// b3CreateHeightFieldShape's contract - same pattern as the old test-course
// wave patch). Bodies are freed by the world teardown like the rest of the
// sample.
void DestroyJozzWorldGround( JozzVehicleWorldGround* ground );

// Destroys and rebuilds ONLY the offroad chunk with a new seed; the plate is
// left untouched. Used by the "Przebuduj teren" button.
void RegenerateJozzWorldOffroad( JozzVehicleWorldGround* ground, uint64_t terrainCategoryBits, uint32_t seed );

// Height of the drivable surface at world (x, z): 0.0 on the plate, the same
// deterministic FBM formula used to build the grid on the offroad chunk
// (evaluated continuously, not snapped to grid points - exact, not
// interpolated). Used for teleport spawn height and, later, for spawner drop
// height (Etap 5).
float SampleJozzWorldGroundHeight( const JozzVehicleWorldGround& ground, float worldX, float worldZ );
