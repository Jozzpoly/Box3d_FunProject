// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_obstacle_kit.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{

constexpr float kSlabHalfThickness = 0.15f; // sharp-box "thickness" for ramps/tabletop/washboard/off-camber/berm

float DegToRad( float degrees )
{
	return degrees * B3_PI / 180.0f;
}

// Small deterministic xorshift PRNG, local to this module - AddRockGarden's
// own concern, kept independent from the terrain generator's noise (no
// coupling between unrelated content modules).
uint32_t NextRandom( uint32_t& state )
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

float RandomUnit( uint32_t& state )
{
	return (float)( NextRandom( state ) & 0x00FFFFFFu ) / (float)0x00FFFFFFu; // [0, 1]
}

float RandomRange( uint32_t& state, float lo, float hi )
{
	return lo + RandomUnit( state ) * ( hi - lo );
}

b3BodyId CreateKitBody( b3WorldId worldId, b3Pos anchor, float yawDegrees, const char* name )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = anchor;
	bodyDef.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisY, DegToRad( yawDegrees ) );
	bodyDef.name = name;
	return b3CreateBody( worldId, &bodyDef );
}

b3ShapeDef MakeKitShapeDef( uint64_t terrainCategoryBits, float friction, uint32_t customColorHex )
{
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = friction;
	shapeDef.baseMaterial.customColor = customColorHex;
	shapeDef.filter.categoryBits = terrainCategoryBits;
	return shapeDef;
}

// A sharp box ramp segment, described in the body's own (pre-yaw) local
// frame: forward=+X, up=+Y, right=+Z.
struct RampLocal
{
	b3Vec3 offset;
	b3Quat rotation;
	float halfLength;
	float halfHeight;
	float halfWidth;
};

// A ramp rising toward local +X: its LOW edge bottom corner touches ground
// exactly at `localEntry` (no gap, no floating). Derivation: rotating the
// box's own bottom-front corner (-halfLength, -halfHeight) by angleDegrees
// about local Z and solving for the center offset that puts that corner at
// the origin gives offsetX = hl*cos(a), offsetY = hl*sin(a) + hh*cos(a).
RampLocal ComputeAscendingRamp( b3Vec3 localEntry, float length, float halfHeight, float width, float angleDegrees )
{
	float angleRad = DegToRad( angleDegrees );
	float hl = length * 0.5f;
	float hw = width * 0.5f;
	float offsetX = localEntry.x + hl * std::cos( angleRad );
	float offsetY = localEntry.y + hl * std::sin( angleRad ) + halfHeight * std::cos( angleRad );
	b3Quat rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, angleRad );
	return { { offsetX, offsetY, localEntry.z }, rotation, hl, halfHeight, hw };
}

// Where an ascending ramp's far (raised) top surface ends - the point the
// next segment (a tabletop's table, a kicker's lip) should start from.
b3Vec3 RampFarTopEdge( b3Vec3 localEntry, float length, float halfHeight, float angleDegrees )
{
	float angleRad = DegToRad( angleDegrees );
	return { localEntry.x + length * std::cos( angleRad ), localEntry.y + length * std::sin( angleRad ) + 2.0f * halfHeight * std::cos( angleRad ),
			 localEntry.z };
}

// Mirror image of ComputeAscendingRamp: a ramp whose TOP sits at
// `topEntry` (already elevated) and falls away toward local +X, reaching
// ground after `length` meters. Used for tabletop/gap-jump landings, where
// the next segment is known by its high end, not its low one.
RampLocal ComputeDescendingRamp( b3Vec3 topEntry, float length, float halfHeight, float width, float angleDegrees )
{
	float angleRad = DegToRad( angleDegrees );
	float hl = length * 0.5f;
	float hw = width * 0.5f;
	float offsetX = topEntry.x + hl * std::cos( angleRad );
	float offsetY = topEntry.y - hl * std::sin( angleRad ) + halfHeight * std::cos( angleRad );
	b3Quat rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, -angleRad );
	return { { offsetX, offsetY, topEntry.z }, rotation, hl, halfHeight, hw };
}

void CreateRampHull( b3BodyId bodyId, const b3ShapeDef& shapeDef, const RampLocal& r )
{
	b3BoxHull hull = b3MakeTransformedBoxHull( r.halfLength, r.halfHeight, r.halfWidth, { r.offset, r.rotation } );
	b3CreateHullShape( bodyId, &shapeDef, &hull.base );
}

// A row of `count` capsules spanning `width`, centered on local X=0, spaced
// `spacing` apart, embedded by `embedFraction` of their own radius (0 =
// resting exactly on the surface, 1 = fully buried).
void CreateCapsuleRow( b3BodyId bodyId, const b3ShapeDef& shapeDef, int count, float spacing, float radius, float width,
						float embedFraction )
{
	float totalLength = spacing * (float)( count - 1 );
	float startX = -totalLength * 0.5f;
	float hw = b3MaxFloat( 0.1f, width * 0.5f - radius );
	float centerY = radius * ( 1.0f - embedFraction );
	for ( int i = 0; i < count; ++i )
	{
		float x = startX + spacing * (float)i;
		b3Capsule capsule = { { x, centerY, -hw }, { x, centerY, hw }, radius };
		b3CreateCapsuleShape( bodyId, &shapeDef, &capsule );
	}
}

void CreateCapsuleAlongZ( b3BodyId bodyId, const b3ShapeDef& shapeDef, float localX, float localZ, float radius,
						  float width, float centerY )
{
	float halfSpan = b3MaxFloat( 0.1f, width * 0.5f - radius );
	b3Capsule capsule = { { localX, centerY, localZ - halfSpan }, { localX, centerY, localZ + halfSpan }, radius };
	b3CreateCapsuleShape( bodyId, &shapeDef, &capsule );
}

} // namespace

// ---- Skocznie / rampy ------------------------------------------------------

void AddBumperBank( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float spacing, float radius,
					float width, float centerY, float sideOffset, JozzBumperPattern pattern, uint64_t terrainCategoryBits,
					uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_bumper_bank" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.85f, customColorHex );

	float startX = -spacing * (float)( count - 1 ) * 0.5f;
	for ( int i = 0; i < count; ++i )
	{
		float localX = startX + spacing * (float)i;
		float localZ = 0.0f;
		float elementWidth = width;
		if ( pattern == kJozzBumperAlternatingSides )
		{
			localZ = ( i & 1 ) == 0 ? -sideOffset : sideOffset;
			elementWidth = width * 0.58f;
		}
		else if ( pattern == kJozzBumperWave )
		{
			localZ = ( ( i % 4 ) < 2 ? -1.0f : 1.0f ) * sideOffset;
			elementWidth = width * ( ( i & 1 ) == 0 ? 0.72f : 0.48f );
		}
		CreateCapsuleAlongZ( bodyId, shapeDef, localX, localZ, radius, elementWidth, centerY );
	}
}

void AddWedgeRamp( b3WorldId worldId, b3Pos entry, float yawDegrees, float length, float width, float angleDegrees,
					uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, entry, yawDegrees, "kit_wedge_ramp" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );
	RampLocal ramp = ComputeAscendingRamp( b3Vec3_zero, length, kSlabHalfThickness, width, angleDegrees );
	CreateRampHull( bodyId, shapeDef, ramp );
}

void AddKicker( b3WorldId worldId, b3Pos entry, float yawDegrees, float length, float width, float angleDegrees,
				 float lipRadius, uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, entry, yawDegrees, "kit_kicker" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );

	RampLocal ramp = ComputeAscendingRamp( b3Vec3_zero, length, kSlabHalfThickness, width, angleDegrees );
	CreateRampHull( bodyId, shapeDef, ramp );

	// Rounded lip: a capsule laid across the take-off edge, centered ON the
	// ramp's own top-front corner (radius straddles the surface, half
	// embedded into the wedge, half protruding as the rounded launch edge).
	b3Vec3 lip = RampFarTopEdge( b3Vec3_zero, length, kSlabHalfThickness, angleDegrees );
	float hw = b3MaxFloat( 0.1f, width * 0.5f - lipRadius );
	b3Capsule capsule = { { lip.x, lip.y, -hw }, { lip.x, lip.y, hw }, lipRadius };
	b3CreateCapsuleShape( bodyId, &shapeDef, &capsule );
}

void AddTabletop( b3WorldId worldId, b3Pos entry, float yawDegrees, float approachLength, float tableLength,
				   float landingLength, float height, float width, uint64_t terrainCategoryBits,
				   uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, entry, yawDegrees, "kit_tabletop" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );
	float hh = kSlabHalfThickness;

	float approachAngleDeg = std::atan2( height, approachLength ) * 180.0f / B3_PI;
	RampLocal approach = ComputeAscendingRamp( b3Vec3_zero, approachLength, hh, width, approachAngleDeg );
	CreateRampHull( bodyId, shapeDef, approach );

	b3Vec3 tableEntry = RampFarTopEdge( b3Vec3_zero, approachLength, hh, approachAngleDeg );
	float tableCenterX = tableEntry.x + tableLength * 0.5f;
	float tableCenterY = tableEntry.y - hh;
	b3BoxHull tableHull = b3MakeOffsetBoxHull( tableLength * 0.5f, hh, width * 0.5f, { tableCenterX, tableCenterY, tableEntry.z } );
	b3CreateHullShape( bodyId, &shapeDef, &tableHull.base );

	b3Vec3 landingTopEntry = { tableEntry.x + tableLength, tableEntry.y, tableEntry.z };
	float landingAngleDeg = std::atan2( height, landingLength ) * 180.0f / B3_PI;
	RampLocal landing = ComputeDescendingRamp( landingTopEntry, landingLength, hh, width, landingAngleDeg );
	CreateRampHull( bodyId, shapeDef, landing );
}

void AddGapJump( b3WorldId worldId, b3Pos entry, float yawDegrees, float rampHeight, float rampAngleDegrees,
				  float gapLength, float width, uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, entry, yawDegrees, "kit_gap_jump" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );
	float hh = kSlabHalfThickness;
	float angleRad = DegToRad( rampAngleDegrees );
	float rampLength = rampHeight / std::sin( angleRad );

	RampLocal takeoff = ComputeAscendingRamp( b3Vec3_zero, rampLength, hh, width, rampAngleDegrees );
	CreateRampHull( bodyId, shapeDef, takeoff );

	b3Vec3 takeoffTop = RampFarTopEdge( b3Vec3_zero, rampLength, hh, rampAngleDegrees );
	b3Vec3 landingTop = { takeoffTop.x + gapLength, takeoffTop.y, takeoffTop.z };
	RampLocal landing = ComputeDescendingRamp( landingTop, rampLength, hh, width, rampAngleDegrees );
	CreateRampHull( bodyId, shapeDef, landing );
}

// ---- Uskoki -----------------------------------------------------------------

namespace
{
void BuildStepBlock( b3WorldId worldId, b3Pos entry, float yawDegrees, float width, float height, float depth,
					  uint64_t terrainCategoryBits, uint32_t customColorHex, const char* name )
{
	b3BodyId bodyId = CreateKitBody( worldId, entry, yawDegrees, name );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );
	float hh = height * 0.5f;
	float hd = depth * 0.5f;
	b3BoxHull hull = b3MakeOffsetBoxHull( hd, hh, width * 0.5f, { hd, hh, 0.0f } );
	b3CreateHullShape( bodyId, &shapeDef, &hull.base );
}
} // namespace

void AddStepUp( b3WorldId worldId, b3Pos entry, float yawDegrees, float width, float height, float depth,
				 uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	BuildStepBlock( worldId, entry, yawDegrees, width, height, depth, terrainCategoryBits, customColorHex, "kit_step_up" );
}

void AddStepDown( b3WorldId worldId, b3Pos entry, float yawDegrees, float width, float height, float depth,
				   uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	BuildStepBlock( worldId, entry, yawDegrees, width, height, depth, terrainCategoryBits, customColorHex, "kit_step_down" );
}

// ---- Rytmiczne ---------------------------------------------------------------

void AddWhoops( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float spacing, float radius, float width,
				 uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_whoops" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.85f, customColorHex );
	CreateCapsuleRow( bodyId, shapeDef, count, spacing, radius, width, 0.35f );
}

void AddSpeedBump( b3WorldId worldId, b3Pos center, float yawDegrees, float radius, float width,
					uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_speed_bump" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.85f, customColorHex );
	CreateCapsuleRow( bodyId, shapeDef, 1, 0.0f, radius, width, 0.35f );
}

void AddWashboard( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float spacing, float height,
					float width, uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_washboard" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );
	float totalLength = spacing * (float)( count - 1 );
	float startX = -totalLength * 0.5f;
	float hh = height * 0.5f;
	for ( int i = 0; i < count; ++i )
	{
		float x = startX + spacing * (float)i;
		b3BoxHull hull = b3MakeOffsetBoxHull( 0.4f, hh, width * 0.5f, { x, hh * 0.6f, 0.0f } );
		b3CreateHullShape( bodyId, &shapeDef, &hull.base );
	}
}

// ---- Teren trudny -------------------------------------------------------------

void AddRockGarden( b3WorldId worldId, b3Pos center, float yawDegrees, float lengthX, float widthZ, float density,
						 float minSize, float maxSize, uint64_t terrainCategoryBits, uint32_t seed,
						 uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_rock_garden" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 1.0f, customColorHex );

	int count = (int)( density * lengthX * widthZ );
	uint32_t state = seed != 0u ? seed : 1u;
	for ( int i = 0; i < count; ++i )
	{
		float rx = RandomRange( state, -lengthX * 0.5f, lengthX * 0.5f );
		float rz = RandomRange( state, -widthZ * 0.5f, widthZ * 0.5f );
		float size = RandomRange( state, minSize, maxSize );
		float yawRad = DegToRad( RandomRange( state, 0.0f, 360.0f ) );
		float tiltRad = DegToRad( RandomRange( state, -20.0f, 20.0f ) );

		float hs = size * 0.5f;
		float embed = hs * 0.4f; // partly buried, not floating loose on the surface
		b3Quat rockRotation = b3MulQuat( b3MakeQuatFromAxisAngle( b3Vec3_axisY, yawRad ),
										  b3MakeQuatFromAxisAngle( b3Vec3_axisX, tiltRad ) );
		b3BoxHull hull = b3MakeTransformedBoxHull( hs, hs, hs, { { rx, hs - embed, rz }, rockRotation } );
		b3CreateHullShape( bodyId, &shapeDef, &hull.base );
	}
}

void AddRockIsland( b3WorldId worldId, b3Pos center, float yawDegrees, float lengthX, float widthZ, int clusterCount,
					int rocksPerCluster, float clusterRadius, float minSize, float maxSize, uint64_t terrainCategoryBits,
					uint32_t seed, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_rock_island" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 1.0f, customColorHex );

	clusterCount = std::max( 1, clusterCount );
	rocksPerCluster = std::max( 1, rocksPerCluster );
	float safeRadius = b3MaxFloat( 0.05f, clusterRadius );
	float safeMinSize = b3MaxFloat( 0.05f, minSize );
	float safeMaxSize = b3MaxFloat( safeMinSize, maxSize );
	uint32_t state = seed != 0u ? seed : 1u;

	// Place cluster centers on a compact grid with deterministic jitter. This
	// makes each island read as several contiguous masses rather than a single
	// uniform random cloud.
	int columns = (int)std::ceil( std::sqrt( (float)clusterCount ) );
	for ( int cluster = 0; cluster < clusterCount; ++cluster )
	{
		int row = cluster / columns;
		int column = cluster % columns;
		int rows = ( clusterCount + columns - 1 ) / columns;
		float u = columns == 1 ? 0.5f : (float)column / (float)( columns - 1 );
		float v = rows == 1 ? 0.5f : (float)row / (float)( rows - 1 );
		float clusterX = ( u - 0.5f ) * lengthX * 0.58f + RandomRange( state, -1.0f, 1.0f );
		float clusterZ = ( v - 0.5f ) * widthZ * 0.58f + RandomRange( state, -1.0f, 1.0f );

		for ( int rock = 0; rock < rocksPerCluster; ++rock )
		{
			float angle = RandomRange( state, 0.0f, 2.0f * B3_PI );
			float radial = std::sqrt( RandomUnit( state ) ) * safeRadius;
			float rx = clusterX + std::cos( angle ) * radial;
			float rz = clusterZ + std::sin( angle ) * radial;
			float size = RandomRange( state, safeMinSize, safeMaxSize );
			float hx = size * RandomRange( state, 0.70f, 1.25f ) * 0.5f;
			float hy = size * RandomRange( state, 0.55f, 1.15f ) * 0.5f;
			float hz = size * RandomRange( state, 0.70f, 1.25f ) * 0.5f;
			rx = std::clamp( rx, -lengthX * 0.5f + hx, lengthX * 0.5f - hx );
			rz = std::clamp( rz, -widthZ * 0.5f + hz, widthZ * 0.5f - hz );

			float embed = hy * RandomRange( state, 0.35f, 0.55f );
			float yawRad = DegToRad( RandomRange( state, 0.0f, 360.0f ) );
			float tiltRad = DegToRad( RandomRange( state, -18.0f, 18.0f ) );
			b3Quat rockRotation = b3MulQuat( b3MakeQuatFromAxisAngle( b3Vec3_axisY, yawRad ),
										 b3MakeQuatFromAxisAngle( b3Vec3_axisX, tiltRad ) );
			b3BoxHull hull = b3MakeTransformedBoxHull( hx, hy, hz, { { rx, hy - embed, rz }, rockRotation } );
			b3CreateHullShape( bodyId, &shapeDef, &hull.base );
		}
	}
}

void AddRuts( b3WorldId worldId, b3Pos center, float yawDegrees, float length, float depth, float trackWidth,
			   uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_ruts" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );

	float hl = length * 0.5f;
	float hh = depth * 0.5f;
	float wallHalfWidth = 0.15f;
	float rollDeg = 28.0f; // fixed V-wall bank; depth/trackWidth control the groove's footprint, not its steepness
	float embedY = -hh * 0.35f;

	// Two trenches (one per wheel track), each made of two boxes tilted
	// toward each other so they meet near the bottom, forming a V-groove.
	for ( float trackSign : { -1.0f, 1.0f } )
	{
		float trackZ = trackSign * trackWidth * 0.5f;
		for ( float wallSign : { -1.0f, 1.0f } )
		{
			float wallZ = trackZ + wallSign * wallHalfWidth;
			b3Quat rollQuat = b3MakeQuatFromAxisAngle( b3Vec3_axisX, DegToRad( -wallSign * rollDeg ) );
			b3BoxHull hull = b3MakeTransformedBoxHull( hl, hh, wallHalfWidth, { { 0.0f, embedY, wallZ }, rollQuat } );
			b3CreateHullShape( bodyId, &shapeDef, &hull.base );
		}
	}
}

void AddOffCamber( b3WorldId worldId, b3Pos center, float yawDegrees, float length, float width, float tiltDegrees,
					uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_off_camber" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );

	float hl = length * 0.5f;
	float hh = kSlabHalfThickness;
	float hw = width * 0.5f;
	float tiltRad = DegToRad( tiltDegrees );
	// Same "touch the ground, no gap" trick as ComputeAscendingRamp, applied
	// across Z (roll) instead of X (pitch).
	float embedY = hw * std::fabs( std::sin( tiltRad ) ) + hh * std::cos( tiltRad );
	b3Quat rollQuat = b3MakeQuatFromAxisAngle( b3Vec3_axisX, tiltRad );
	b3BoxHull hull = b3MakeTransformedBoxHull( hl, hh, hw, { { 0.0f, embedY, 0.0f }, rollQuat } );
	b3CreateHullShape( bodyId, &shapeDef, &hull.base );
}

void AddBerm( b3WorldId worldId, b3Pos center, float yawDegrees, float archRadius, float archDegrees, float width,
			   float bankAngleDegrees, int segmentCount, uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_berm" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );

	float hh = kSlabHalfThickness;
	float hw = width * 0.5f;
	float segArcDeg = archDegrees / (float)segmentCount;
	// 15% overlap between neighboring segments so the chord-vs-arc gap never
	// opens a seam a wheel could catch on.
	float segHalfLength = archRadius * std::sin( DegToRad( segArcDeg ) * 0.5f ) * 1.15f;
	float bankRad = DegToRad( bankAngleDegrees );
	float embedY = hw * std::fabs( std::sin( bankRad ) ) + hh * std::cos( bankRad );

	for ( int i = 0; i < segmentCount; ++i )
	{
		float t = DegToRad( -archDegrees * 0.5f + segArcDeg * ( (float)i + 0.5f ) );
		float x = archRadius * std::sin( t );
		float z = archRadius * ( 1.0f - std::cos( t ) );
		// Tangent direction of the arc at parameter t, expressed as a local
		// yaw (forward(theta) = (cos theta, 0, -sin theta), see the header's
		// yaw convention note) so consecutive segments line up edge to edge.
		b3Quat tangentYaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -t );
		b3Quat bankQuat = b3MakeQuatFromAxisAngle( b3Vec3_axisX, bankRad );
		b3Quat segmentRotation = b3MulQuat( tangentYaw, bankQuat );
		b3BoxHull hull = b3MakeTransformedBoxHull( segHalfLength, hh, hw, { { x, embedY, z }, segmentRotation } );
		b3CreateHullShape( bodyId, &shapeDef, &hull.base );
	}
}

void AddStairs( b3WorldId worldId, b3Pos entry, float yawDegrees, int stepCount, float stepHeight, float stepDepth,
				  float width, uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, entry, yawDegrees, "kit_stairs" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );

	// Each step's box reaches all the way down to a shared deep base, so
	// consecutive treads overlap underground instead of leaving a gap - a
	// real staircase profile without needing separate riser shapes.
	float deepBase = -( stepHeight * (float)stepCount + 1.0f );
	for ( int i = 0; i < stepCount; ++i )
	{
		float top = -stepHeight * (float)( i + 1 );
		float hh = ( top - deepBase ) * 0.5f;
		float cy = ( top + deepBase ) * 0.5f;
		float cx = stepDepth * ( (float)i + 0.5f );
		b3BoxHull hull = b3MakeOffsetBoxHull( stepDepth * 0.5f, hh, width * 0.5f, { cx, cy, 0.0f } );
		b3CreateHullShape( bodyId, &shapeDef, &hull.base );
	}
}

void AddLogs( b3WorldId worldId, b3Pos center, float yawDegrees, int count, float radius, float spacing, float width,
			   uint64_t terrainCategoryBits, uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_logs" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.8f, customColorHex );
	CreateCapsuleRow( bodyId, shapeDef, count, spacing, radius, width, 0.15f );
}

void AddArticulationRamps( b3WorldId worldId, b3Pos center, float yawDegrees, float length, float width,
							 float angleDegrees, float offsetLR, uint64_t terrainCategoryBits,
							 uint32_t customColorHex )
{
	b3BodyId bodyId = CreateKitBody( worldId, center, yawDegrees, "kit_articulation" );
	b3ShapeDef shapeDef = MakeKitShapeDef( terrainCategoryBits, 0.9f, customColorHex );
	float hh = kSlabHalfThickness;
	float laneGap = 0.15f; // small gap so the two ramps never overlap in Z
	float trackZ = width * 0.5f + laneGap * 0.5f;

	// `center` is the CENTER anchor: each lane spans roughly [-length,+length]
	// in local X. The phase offset shifts when the left/right wheel path is on
	// the raised portion, but neither path leaves the station with a dangling
	// elevated end. This fixes the old one-sided ascending-only implementation.
	for ( float trackSign : { -1.0f, 1.0f } )
	{
		float startX = -length + trackSign * offsetLR * 0.5f;
		float localZ = trackSign * trackZ;
		RampLocal up = ComputeAscendingRamp( { startX, 0.0f, localZ }, length, hh, width, angleDegrees );
		CreateRampHull( bodyId, shapeDef, up );

		b3Vec3 top = RampFarTopEdge( { startX, 0.0f, localZ }, length, hh, angleDegrees );
		RampLocal down = ComputeDescendingRamp( top, length, hh, width, angleDegrees );
		CreateRampHull( bodyId, shapeDef, down );
	}
}
