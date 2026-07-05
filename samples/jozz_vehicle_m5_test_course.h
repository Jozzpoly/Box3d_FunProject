// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M5 test playground: ramps, a washboard lane, a rough-terrain heightfield
// zone, and a scatter of dynamic props. Pure content/course building, kept
// out of jozz_vehicle_m5_drivable_lab.cpp so that file stays focused on
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
	b3HeightFieldData* roughTerrainField = nullptr;
	std::vector<JozzVehicleM5TestCourseProp> props;
};

// groundTopY is the world Y of the flat ground's top surface (AddGroundBox
// puts it at 0 regardless of extent); the rough-terrain zone is offset above
// it so the two surfaces never double-contact the same footprint.
JozzVehicleM5TestCourse CreateJozzVehicleM5TestCourse( b3WorldId worldId, float groundTopY );

// Frees the height field data (not owned by b3DestroyWorld; box3d only holds
// a reference to it, per b3CreateHeightFieldShape's contract). Prop/ramp/
// washboard bodies are freed by the world teardown like the rest of the
// sample, matching the existing course code's convention.
void DestroyJozzVehicleM5TestCourse( JozzVehicleM5TestCourse* course );

// Teleports every prop back to its spawn transform and zeroes its velocity,
// for when testing has scattered them off the map.
void ResetJozzVehicleM5TestCourseProps( const JozzVehicleM5TestCourse& course );
