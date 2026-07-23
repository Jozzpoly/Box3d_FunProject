// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_central_test_campus_visual.h"

#include "gfx/draw.h"
#include "jozz_vehicle_central_test_campus.h"
#include "jozz_vehicle_world_layout.h"

#include <cmath>

namespace
{

b3Pos Point( float x, float z, float y = 0.08f )
{
	return { x, y, z };
}

b3Pos TransformLocal( const JozzTestStationSpec& spec, float localX, float localZ )
{
	float angle = spec.yawDegrees * B3_PI / 180.0f;
	float c = std::cos( angle );
	float s = std::sin( angle );
	return Point( spec.centerXZ.x + c * localX - s * localZ, spec.centerXZ.y + s * localX + c * localZ );
}

void DrawRectangle( const b3Vec2& center, const b3Vec2& halfExtents, Vec4 color )
{
	b3Pos a = Point( center.x - halfExtents.x, center.y - halfExtents.y );
	b3Pos b = Point( center.x + halfExtents.x, center.y - halfExtents.y );
	b3Pos c = Point( center.x + halfExtents.x, center.y + halfExtents.y );
	b3Pos d = Point( center.x - halfExtents.x, center.y + halfExtents.y );
	DrawLineEx( a, b, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( b, c, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( c, d, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( d, a, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
}

void DrawRotatedRectangle( const JozzTestStationSpec& spec, Vec4 color )
{
	b3Pos a = TransformLocal( spec, -spec.footprintHalfExtents.x, -spec.footprintHalfExtents.y );
	b3Pos b = TransformLocal( spec, spec.footprintHalfExtents.x, -spec.footprintHalfExtents.y );
	b3Pos c = TransformLocal( spec, spec.footprintHalfExtents.x, spec.footprintHalfExtents.y );
	b3Pos d = TransformLocal( spec, -spec.footprintHalfExtents.x, spec.footprintHalfExtents.y );
	DrawLineEx( a, b, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( b, c, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( c, d, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( d, a, color, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );

	b3Pos entry = TransformLocal( spec, -spec.footprintHalfExtents.x - spec.approachLength, 0.0f );
	b3Pos exit = TransformLocal( spec, spec.footprintHalfExtents.x + spec.runoffLength, 0.0f );
	DrawLineEx( entry, exit, MakeVec4( 0.75f, 0.75f, 0.75f, 0.9f ), 2.0f, OVERLAY_THICKNESS_PIXELS,
					OVERLAY_OCCLUSION_HIDE );
	DrawString3D( Point( spec.centerXZ.x, spec.centerXZ.y, 0.5f ), MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ), "%s", spec.name );
}

void DrawMasterplanYards()
{
	const Vec4 yardColor = MakeVec4( 0.25f, 0.55f, 0.75f, 0.55f );
	for ( int i = 0; i < JozzWorldLayout::kMasterplanYardCount; ++i )
	{
		const JozzWorldLayout::JozzWorldYard& yard = JozzWorldLayout::kMasterplanYards[i];
		b3Vec2 center = { ( yard.minX + yard.maxX ) * 0.5f, ( yard.minZ + yard.maxZ ) * 0.5f };
		b3Vec2 halfExtents = { ( yard.maxX - yard.minX ) * 0.5f, ( yard.maxZ - yard.minZ ) * 0.5f };
		DrawRectangle( center, halfExtents, yardColor );
		DrawString3D( Point( center.x, center.y, 0.22f ), yardColor, "%s", yard.name );
	}
}

} // namespace

void DrawCentralCampusSkeleton()
{
	const Vec4 boundary = MakeVec4( 0.35f, 0.65f, 0.85f, 0.8f );
	const Vec4 core = MakeVec4( 0.95f, 0.95f, 0.95f, 0.95f );
	const Vec4 station = MakeVec4( 0.9f, 0.9f, 0.9f, 1.0f );

	DrawRectangle( { 0.0f, 0.0f }, { kCentralCampusTileHalfExtent, kCentralCampusTileHalfExtent }, boundary );
	DrawMasterplanYards();
	DrawRectangle( { 0.0f, 0.0f }, { kCentralCampusCoreHalfExtent, kCentralCampusCoreHalfExtent }, core );
	DrawRectangle( { 0.0f, 0.0f }, { kCentralCampusLoopHalfExtent, kCentralCampusLoopHalfExtent }, boundary );
	DrawString3D( Point( 0.0f, 0.0f, 0.35f ), core, "CENTRAL CORE 24x24 - SPAWN / TUNING" );

	const JozzTestStationSpec* specs = GetCentralCampusStationSpecs();
	for ( int i = 0; i < GetCentralCampusStationSpecCount(); ++i )
	{
		DrawRotatedRectangle( specs[i], station );
	}
}
