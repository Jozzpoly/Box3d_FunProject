// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class JozzTrackVariant
{
	GreenFlow,
	YellowTechnical,
	RedStress,
};

enum class JozzTrackSegmentKind
{
	StartMeta,
	Straight,
	ConstantRadiusArc,
	FastArc,
	Hairpin,
	Chicane,
	HeightSection,
	BranchBypass,
	Runoff,
};

enum class JozzTrackTileHint
{
	N,
	NW,
	NE,
};

constexpr uint32_t kJozzTrackGreenMask = 1u << 0;
constexpr uint32_t kJozzTrackYellowMask = 1u << 1;
constexpr uint32_t kJozzTrackRedMask = 1u << 2;
constexpr uint32_t kJozzTrackAllVariantsMask = kJozzTrackGreenMask | kJozzTrackYellowMask | kJozzTrackRedMask;

// Coordinates are world X/Z and a planned surface offset Y above the neutral
// plate. E3.0 is only a centerline contract: no physical shapes are created
// from these points yet.
struct JozzTrackPoint
{
	float x;
	float z;
	float y;
};

struct JozzTrackSegmentSpec
{
	const char* id;
	JozzTrackSegmentKind kind;
	uint32_t variantMask;
	const JozzTrackPoint* centerline;
	int pointCount;
	float roadWidth;
	float runoffWidth;
	float minHeight;
	float maxHeight;
	float recommendedSpeedMin;
	float recommendedSpeedMax;
	JozzTrackTileHint tile;
};

struct JozzTrackVariantSpec
{
	const char* id;
	const char* displayName;
	JozzTrackVariant variant;
	uint32_t mask;
	const JozzTrackPoint* centerline;
	int pointCount;
	float roadWidth;
	float runoffWidth;
	float allowedMinHeight;
	float allowedMaxHeight;
};

constexpr float kJozzTrackMinimumMainStraight = 220.0f;
// A closed, no-wrong-way loop with a 220 m straight needs a return leg of
// comparable order. A 260-340 m full lap would be geometrically impossible
// without reusing the same line in opposite directions, so E3.0 makes this
// constraint explicit instead of accepting a misleading layout.
constexpr float kJozzTrackMinimumLapLength = 440.0f;
constexpr float kJozzTrackMaximumLapLength = 800.0f;
constexpr float kJozzTrackMinimumRunoff = 12.0f;
// The physical slab is 0.24 m thick. A 0.28 m top-surface separation leaves
// a small but real static gap when two branches intentionally stack.
constexpr float kJozzTrackMinimumLayerClearance = 0.28f;
constexpr float kJozzTrackMaximumAdjacentRise = 0.30f;

const JozzTrackPoint* GetJozzTrackMainStraightPoints();
int GetJozzTrackMainStraightPointCount();
const JozzTrackVariantSpec* GetJozzTrackVariantSpecs();
int GetJozzTrackVariantSpecCount();
const JozzTrackSegmentSpec* GetJozzTrackSegmentSpecs();
int GetJozzTrackSegmentSpecCount();

float ComputeJozzTrackPolylineLength( const JozzTrackPoint* points, int pointCount );
bool ValidateJozzTrackLayout( std::vector<std::string>* errors );
