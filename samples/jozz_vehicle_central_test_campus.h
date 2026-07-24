// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/box3d.h"
#include "jozz_vehicle_obstacle_kit.h"

#include <string>
#include <vector>

// E2R contract: layout data is independent from obstacle construction. The
// first implementation deliberately exposes only the skeleton and validator;
// physical recipes will be added after the skeleton has passed visual review.
enum class JozzCampusStationId
{
	NorthComfort,
	WestDrift,
	EastTraction,
	SouthImpact,
};

enum class JozzCampusDirection
{
	WestToEast,
	SouthToNorth,
	NorthToSouth,
};

struct JozzTestStationSpec
{
	JozzCampusStationId id;
	const char* name;
	b3Vec2 centerXZ;
	float yawDegrees;
	b3Vec2 footprintHalfExtents;
	float approachLength;
	float runoffLength;
	float recommendedSpeedMin;
	float recommendedSpeedMax;
	JozzCampusDirection direction;
	bool bidirectional;
};

struct JozzRockIslandSpec
{
	const char* id;
	b3Vec2 centerXZ;
	float yawDegrees;
	float lengthX;
	float widthZ;
	int clusterCount;
	int rocksPerCluster;
	float clusterRadius;
	float minSize;
	float maxSize;
	uint32_t seedOffset;
};

struct JozzBumperBankSpec
{
	const char* id;
	b3Vec2 centerXZ;
	float yawDegrees;
	int count;
	float spacing;
	float radius;
	float width;
	float centerY;
	float sideOffset;
	JozzBumperPattern pattern;
};

constexpr float kCentralCampusTileHalfExtent = 60.0f;
constexpr float kCentralCampusCoreHalfExtent = 12.0f;
constexpr float kCentralCampusLoopHalfExtent = 56.0f;
constexpr float kCentralCampusLoopHalfWidth = 4.0f;

const JozzTestStationSpec* GetCentralCampusStationSpecs();
int GetCentralCampusStationSpecCount();
const JozzRockIslandSpec* GetCentralCampusRockIslandSpecs();
int GetCentralCampusRockIslandSpecCount();
const JozzBumperBankSpec* GetCentralCampusBumperBankSpecs();
int GetCentralCampusBumperBankSpecCount();

// Returns false for placement errors only. Geometry contact/continuity is a
// separate gate and must not be hidden inside this data validator.
bool ValidateCentralCampusLayout( std::vector<std::string>* errors );
bool ValidateCentralCampusContent( std::vector<std::string>* errors );

// First physical slice. It deliberately excludes ramps, tabletops and gap
// jumps until their contact-profile audit has passed.
void BuildCentralTestCampus( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits, uint32_t terrainSeed );
