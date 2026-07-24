// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M5 test playground: the active E2R central campus built by
// jozz_vehicle_central_test_campus. The rough-terrain heightfield patch that
// used to live here was replaced by the offroad chunk in
// jozz_vehicle_world_terrain (Mapa Etap 1) - it stuck up above the flat
// ground instead of tucking under it, which is exactly the seam defect that
// track fixes. The four hand-placed ramps and two washboard lanes from M5/E1
// are gone. Pure content/course building, kept
// out of jozz_vehicle_m5_drivable_lab.cpp so that file stays focused on
// input/camera/tuning UI (the same "don't let the sample file get overloaded"
// lesson from PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md, Problem A).
//
// Not part of jozz_vehicle_validation: this is scenery for interactive
// testing, not something the headless vehicle smoke needs.

#include "box3d/box3d.h"

#include <string>
#include <vector>

struct JozzVehicleM5TestCourseProp
{
	b3BodyId bodyId;
	b3Pos spawnPosition;
	b3Quat spawnRotation;
};

// A station/lane name drawn with DrawString3D. Collected once at course-build
// time instead of computed per-shape in the render loop (E2 doc §5 - "not a
// per-shape hack"); the lab's Render() walks this array and distance-culls it
// against the camera (E2 doc §7 risk: too many labels far away clutter the HUD).
struct JozzCourseLabel
{
	b3Pos position;
	std::string text;
	uint32_t colorHex;
};

struct JozzVehicleM5TestCourse
{
	std::vector<JozzVehicleM5TestCourseProp> props;
	std::vector<JozzCourseLabel> labels;
};

// groundTopY is the world Y of the flat ground's top surface. The active
// campus keeps all current static content on that surface; future satellite
// yards will use their own explicit terrain/transition contract.
//
// terrainCategoryBits tags every drivable surface built here (the lane
// poligon's obstacle-kit stations) with a collision category; dynamic props
// keep the default category. The M6 split wheel envelope keys on this: its
// smooth rolling sphere collides with terrain-category shapes only, while its
// true-width sidewall cylinder handles everything else. The default value 1
// is the engine default category, i.e. no behavior change for the M5 lab.
JozzVehicleM5TestCourse CreateJozzVehicleM5TestCourse( b3WorldId worldId, float groundTopY,
														   uint64_t terrainCategoryBits = 1 );

// Prop/obstacle-kit bodies are freed by the world teardown like the rest of
// the sample; this only clears the prop/label bookkeeping vectors.
void DestroyJozzVehicleM5TestCourse( JozzVehicleM5TestCourse* course );

// Teleports every prop back to its spawn transform and zeroes its velocity,
// for when testing has scattered them off the map.
void ResetJozzVehicleM5TestCourseProps( const JozzVehicleM5TestCourse& course );
