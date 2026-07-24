// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_track_builder.h"

#include "jozz_vehicle_obstacle_kit.h"
#include "jozz_vehicle_track_layout.h"

#include <algorithm>
#include <cmath>

namespace
{

constexpr float kTrackSlabHalfThickness = 0.12f;

struct TrackOffsetPoint
{
 	float leftX;
 	float leftZ;
 	float rightX;
 	float rightZ;
};

TrackOffsetPoint ComputeMiteredOffset( const JozzTrackPoint* points, int pointCount, int index, float halfWidth )
{
	const JozzTrackPoint& point = points[index];
	int previousIndex = index > 0 ? index - 1 : index;
	int nextIndex = index + 1 < pointCount ? index + 1 : index;
	float prevDx = point.x - points[previousIndex].x;
	float prevDz = point.z - points[previousIndex].z;
	float nextDx = points[nextIndex].x - point.x;
	float nextDz = points[nextIndex].z - point.z;
	float prevLength = std::sqrt( prevDx * prevDx + prevDz * prevDz );
	float nextLength = std::sqrt( nextDx * nextDx + nextDz * nextDz );
	if ( prevLength < 0.01f )
	{
		prevDx = nextDx;
		prevDz = nextDz;
		prevLength = nextLength;
	}
	if ( nextLength < 0.01f )
	{
		nextDx = prevDx;
		nextDz = prevDz;
		nextLength = prevLength;
	}
	float prevTx = prevDx / prevLength;
	float prevTz = prevDz / prevLength;
	float nextTx = nextDx / nextLength;
	float nextTz = nextDz / nextLength;
	float prevNx = -prevTz;
	float prevNz = prevTx;
	float nextNx = -nextTz;
	float nextNz = nextTx;
	float miterX = prevNx + nextNx;
	float miterZ = prevNz + nextNz;
	float miterLength = std::sqrt( miterX * miterX + miterZ * miterZ );
	if ( miterLength < 0.01f )
	{
		miterX = nextNx;
		miterZ = nextNz;
		miterLength = 1.0f;
	}
	miterX /= miterLength;
	miterZ /= miterLength;
	float denominator = miterX * nextNx + miterZ * nextNz;
	if ( std::fabs( denominator ) < 0.25f )
	{
		miterX = nextNx;
		miterZ = nextNz;
		denominator = 1.0f;
	}
	float miterDistance = std::clamp( halfWidth / denominator, -4.0f * halfWidth, 4.0f * halfWidth );
	return { point.x + miterX * miterDistance, point.z + miterZ * miterDistance,
				point.x - miterX * miterDistance, point.z - miterZ * miterDistance };
}

void AddMiteredSurfaceSegment( b3BodyId bodyId, const JozzTrackPoint& a, const JozzTrackPoint& b,
								 const TrackOffsetPoint& aOffset, const TrackOffsetPoint& bOffset, float groundTopY,
								 uint64_t terrainCategoryBits )
{
	b3Vec3 points[8] = {
		{ aOffset.leftX, groundTopY + a.y, aOffset.leftZ },
		{ aOffset.rightX, groundTopY + a.y, aOffset.rightZ },
		{ bOffset.rightX, groundTopY + b.y, bOffset.rightZ },
		{ bOffset.leftX, groundTopY + b.y, bOffset.leftZ },
		{ aOffset.leftX, groundTopY + a.y - 2.0f * kTrackSlabHalfThickness, aOffset.leftZ },
		{ aOffset.rightX, groundTopY + a.y - 2.0f * kTrackSlabHalfThickness, aOffset.rightZ },
		{ bOffset.rightX, groundTopY + b.y - 2.0f * kTrackSlabHalfThickness, bOffset.rightZ },
		{ bOffset.leftX, groundTopY + b.y - 2.0f * kTrackSlabHalfThickness, bOffset.leftZ },
	};
	b3HullData* hull = b3CreateHull( points, 8, 8 );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.95f;
	shapeDef.filter.categoryBits = terrainCategoryBits;
	b3CreateHullShape( bodyId, &shapeDef, hull );
	b3DestroyHull( hull );
}

void AddRouteSurfaces( b3BodyId bodyId, const JozzTrackVariantSpec& route, float groundTopY,
						 uint64_t terrainCategoryBits )
	{
	// Segment zero is the shared start/meta-to-main-straight entry. The main
	// straight itself is built once below, so route branches begin at point 1.
	for ( int i = 2; i < route.pointCount; ++i )
	{
		TrackOffsetPoint aOffset = ComputeMiteredOffset( route.centerline, route.pointCount, i - 1,
										 route.roadWidth * 0.5f );
		TrackOffsetPoint bOffset = ComputeMiteredOffset( route.centerline, route.pointCount, i,
										 route.roadWidth * 0.5f );
		AddMiteredSurfaceSegment( bodyId, route.centerline[i - 1], route.centerline[i], aOffset, bOffset, groundTopY,
								 terrainCategoryBits );
	}
}

enum class TrackProfileKind
{
	LowBumperRhythm,
	LowArticulation,
};

struct TrackProfileBuildSpec
{
	JozzTrackVariant variant;
	TrackProfileKind kind;
	int anchorPointIndex;
	int count;
	float spacing;
	float radius;
	float centerY;
	float sideOffset;
	JozzBumperPattern pattern;
	float length;
	float rampWidth;
	float angleDegrees;
	float offsetLR;
};

constexpr TrackProfileBuildSpec kProfileSpecs[] = {
	// Green: small full-width rhythm, kept below the tire's comfortable edge.
	{ JozzTrackVariant::GreenFlow, TrackProfileKind::LowBumperRhythm, 6, 5, 1.8f, 0.07f, 0.035f, 0.0f,
	  kJozzBumperFullWidth, 0.0f, 0.0f, 0.0f, 0.0f },
	// Yellow: alternating wave, still low but requiring active wheel placement.
	{ JozzTrackVariant::YellowTechnical, TrackProfileKind::LowBumperRhythm, 6, 6, 1.5f, 0.09f, 0.050f, 1.5f,
	  kJozzBumperWave, 0.0f, 0.0f, 0.0f, 0.0f },
	// Red: a complete up/down two-wheel rhythm, 6 m each way at 1.2 degrees.
	{ JozzTrackVariant::RedStress, TrackProfileKind::LowArticulation, 7, 0, 0.0f, 0.0f, 0.0f, 0.0f,
	  kJozzBumperFullWidth, 6.0f, 1.6f, 1.2f, 1.6f },
};

int VariantIndex( JozzTrackVariant variant )
{
	return (int)variant;
}

float HeadingDegrees( const JozzTrackPoint& a, const JozzTrackPoint& b )
{
	return std::atan2( -( b.z - a.z ), b.x - a.x ) * 180.0f / B3_PI;
}

} // namespace

void BuildJozzTrackBase( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = { 0.0f, 0.0f, 0.0f };
	bodyDef.name = "track_e3_neutral_base";
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef sharedShapeDef = b3DefaultShapeDef();
	sharedShapeDef.baseMaterial.friction = 0.95f;
	sharedShapeDef.filter.categoryBits = terrainCategoryBits;
	const JozzTrackPoint* mainStraight = GetJozzTrackMainStraightPoints();
	for ( int i = 1; i < GetJozzTrackMainStraightPointCount(); ++i )
	{
		const JozzTrackPoint& a = mainStraight[i - 1];
		const JozzTrackPoint& b = mainStraight[i];
		float dx = b.x - a.x;
		float dy = b.y - a.y;
		float dz = b.z - a.z;
		float length = std::sqrt( dx * dx + dy * dy + dz * dz );
		b3Vec3 center = { ( a.x + b.x ) * 0.5f, groundTopY + a.y - kTrackSlabHalfThickness,
									 ( a.z + b.z ) * 0.5f };
		b3BoxHull hull = b3MakeOffsetBoxHull( length * 0.5f, kTrackSlabHalfThickness, 6.0f, center );
		b3CreateHullShape( bodyId, &sharedShapeDef, &hull.base );
	}

	// Re-use the exact same body and terrain category for all branches. The
	// layout validator is responsible for deciding whether close plan-view
	// branches are intentionally layered or must be moved apart.
	const JozzTrackVariantSpec* variants = GetJozzTrackVariantSpecs();
	for ( int i = 0; i < GetJozzTrackVariantSpecCount(); ++i )
	{
		AddRouteSurfaces( bodyId, variants[i], groundTopY, terrainCategoryBits );
	}
}

void BuildJozzTrackProfiles( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits )
{
	const JozzTrackVariantSpec* variants = GetJozzTrackVariantSpecs();
	for ( const TrackProfileBuildSpec& profile : kProfileSpecs )
	{
		const JozzTrackVariantSpec& route = variants[VariantIndex( profile.variant )];
		if ( profile.anchorPointIndex < 0 || profile.anchorPointIndex + 1 >= route.pointCount )
		{
			continue;
		}
		const JozzTrackPoint& anchor = route.centerline[profile.anchorPointIndex];
		const JozzTrackPoint& next = route.centerline[profile.anchorPointIndex + 1];
		float yawDegrees = HeadingDegrees( anchor, next );
		b3Pos position = { anchor.x, groundTopY + anchor.y, anchor.z };
		if ( profile.kind == TrackProfileKind::LowBumperRhythm )
		{
			AddBumperBank( worldId, position, yawDegrees, profile.count, profile.spacing, profile.radius,
						   route.roadWidth - 1.0f, profile.centerY, profile.sideOffset, profile.pattern,
						   terrainCategoryBits );
		}
		else
		{
			AddArticulationRamps( worldId, position, yawDegrees, profile.length, profile.rampWidth,
							  profile.angleDegrees, profile.offsetLR, terrainCategoryBits );
		}
	}
}
