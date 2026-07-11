// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M5 test playground: ramps, a washboard lane, and a scatter of dynamic
// props. The rough-terrain heightfield patch that used to live here was
// replaced by the offroad chunk in jozz_vehicle_world_terrain (Mapa Etap 1,
// docs/MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md) - it stuck up above the flat
// ground instead of tucking under it, which is exactly the seam defect that
// track fixes. Pure content/course building, kept out of
// jozz_vehicle_m5_drivable_lab.cpp so that file stays focused on
// input/camera/tuning UI (the same "don't let the sample file get overloaded"
// lesson from PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md, Problem A).
//
// Not part of jozz_vehicle_validation: this is scenery for interactive
// testing, not something the headless vehicle smoke needs.

#include "box3d/box3d.h"

#include <vector>

struct JozzVehicleM5TestCourseProp
{
	b3BodyId bodyId;
	b3Pos spawnPosition;
	b3Quat spawnRotation;
};

struct JozzVehicleM5TestCourse
{
	std::vector<JozzVehicleM5TestCourseProp> props;
};

// groundTopY is the world Y of the flat ground's top surface (AddGroundBox
// puts it at 0 regardless of extent); the rough-terrain zone is offset above
// it so the two surfaces never double-contact the same footprint.
//
// terrainCategoryBits tags the drivable surfaces (ramps, washboard, rough
// terrain) with a collision category; dynamic props keep the default
// category. The M6 split wheel envelope keys on this: its smooth rolling
// sphere collides with terrain-category shapes only, while its true-width
// sidewall cylinder handles everything else. The default value 1 is the
// engine default category, i.e. no behavior change for the M5 lab.
JozzVehicleM5TestCourse CreateJozzVehicleM5TestCourse( b3WorldId worldId, float groundTopY,
													   uint64_t terrainCategoryBits = 1 );

// Prop/ramp/washboard bodies are freed by the world teardown like the rest of
// the sample; this only clears the prop bookkeeping vector.
void DestroyJozzVehicleM5TestCourse( JozzVehicleM5TestCourse* course );

// Teleports every prop back to its spawn transform and zeroes its velocity,
// for when testing has scattered them off the map.
void ResetJozzVehicleM5TestCourseProps( const JozzVehicleM5TestCourse& course );
