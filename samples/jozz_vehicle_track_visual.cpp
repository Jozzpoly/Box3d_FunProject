// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_track_visual.h"

#include "gfx/draw.h"
#include "jozz_vehicle_track_layout.h"
#include "jozz_vehicle_world_layout.h"

#include <cmath>

namespace
{

b3Pos TrackPoint( const JozzTrackPoint& point, float yOffset = 0.18f )
{
	return { point.x, yOffset + point.y, point.z };
}

Vec4 VariantColor( JozzTrackVariant variant, float alpha )
{
	switch ( variant )
	{
	case JozzTrackVariant::GreenFlow: return MakeVec4( 0.18f, 0.90f, 0.30f, alpha );
	case JozzTrackVariant::YellowTechnical: return MakeVec4( 1.00f, 0.78f, 0.08f, alpha );
	case JozzTrackVariant::RedStress: return MakeVec4( 0.95f, 0.18f, 0.16f, alpha );
	}
	return MakeVec4( 1.0f, 1.0f, 1.0f, alpha );
}

void DrawRoute( const JozzTrackVariantSpec& route )
{
	const Vec4 centerColor = VariantColor( route.variant, 0.95f );
	const Vec4 edgeColor = VariantColor( route.variant, 0.42f );
	// The first segment is the shared N straight. It is drawn once in neutral
	// form below; variant colors start at the first branch so the skeleton does
	// not falsely read as three parallel main straights.
	for ( int i = 2; i < route.pointCount; ++i )
	{
		const JozzTrackPoint& a = route.centerline[i - 1];
		const JozzTrackPoint& b = route.centerline[i];
		float dx = b.x - a.x;
		float dz = b.z - a.z;
		float length = std::sqrt( dx * dx + dz * dz );
		if ( length < 0.01f )
		{
			continue;
		}
		float nx = -dz / length;
		float nz = dx / length;
		float halfWidth = route.roadWidth * 0.5f;
		b3Pos leftA = { a.x + nx * halfWidth, 0.13f + a.y, a.z + nz * halfWidth };
		b3Pos leftB = { b.x + nx * halfWidth, 0.13f + b.y, b.z + nz * halfWidth };
		b3Pos rightA = { a.x - nx * halfWidth, 0.13f + a.y, a.z - nz * halfWidth };
		b3Pos rightB = { b.x - nx * halfWidth, 0.13f + b.y, b.z - nz * halfWidth };
		DrawLineEx( leftA, leftB, edgeColor, 2.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
		DrawLineEx( rightA, rightB, edgeColor, 2.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
		DrawLineEx( TrackPoint( a ), TrackPoint( b ), centerColor, 4.0f, OVERLAY_THICKNESS_PIXELS,
					OVERLAY_OCCLUSION_HIDE );
	}
}

void DrawSharedMainStraight()
{
	const JozzTrackPoint* points = GetJozzTrackMainStraightPoints();
	const JozzTrackPoint& a = points[0];
	const JozzTrackPoint& b = points[1];
	const Vec4 edgeColor = MakeVec4( 0.85f, 0.85f, 0.85f, 0.72f );
	const Vec4 centerColor = MakeVec4( 1.0f, 1.0f, 1.0f, 0.98f );
	float halfWidth = 6.0f;
	b3Pos leftA = { a.x, 0.13f, a.z - halfWidth };
	b3Pos leftB = { b.x, 0.13f, b.z - halfWidth };
	b3Pos rightA = { a.x, 0.13f, a.z + halfWidth };
	b3Pos rightB = { b.x, 0.13f, b.z + halfWidth };
	DrawLineEx( leftA, leftB, edgeColor, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( rightA, rightB, edgeColor, 3.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( TrackPoint( a ), TrackPoint( b ), centerColor, 4.0f, OVERLAY_THICKNESS_PIXELS,
				OVERLAY_OCCLUSION_HIDE );
	DrawString3D( { 0.0f, 0.62f, 158.0f }, centerColor, "SHARED N MAIN STRAIGHT - 240 m" );
}

void DrawStartMeta()
{
	b3Pos a = { -120.0f, 0.32f, 144.0f };
	b3Pos b = { -120.0f, 0.32f, 156.0f };
	DrawLineEx( a, b, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ), 5.0f, OVERLAY_THICKNESS_PIXELS,
				OVERLAY_OCCLUSION_HIDE );
	DrawString3D( { -120.0f, 0.65f, 162.0f }, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ), "START / META - E3" );
}

void DrawTrackEnvelope()
{
	b3Pos a = { JozzWorldLayout::kLongTrackMinX, 0.10f, JozzWorldLayout::kLongTrackMinZ };
	b3Pos b = { JozzWorldLayout::kLongTrackMaxX, 0.10f, JozzWorldLayout::kLongTrackMinZ };
	b3Pos c = { JozzWorldLayout::kLongTrackMaxX, 0.10f, JozzWorldLayout::kLongTrackMaxZ };
	b3Pos d = { JozzWorldLayout::kLongTrackMinX, 0.10f, JozzWorldLayout::kLongTrackMaxZ };
	const Vec4 envelope = MakeVec4( 0.30f, 0.50f, 0.75f, 0.28f );
	DrawLineEx( a, b, envelope, 2.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( b, c, envelope, 2.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( c, d, envelope, 2.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawLineEx( d, a, envelope, 2.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_HIDE );
	DrawString3D( { -178.0f, 0.36f, 178.0f }, envelope, "E3 LONG LOOP ENVELOPE - N / NW / NE" );
}

} // namespace

void DrawJozzTrackSkeleton()
{
	DrawTrackEnvelope();
	DrawSharedMainStraight();
	const JozzTrackVariantSpec* variants = GetJozzTrackVariantSpecs();
	for ( int i = 0; i < GetJozzTrackVariantSpecCount(); ++i )
	{
		DrawRoute( variants[i] );
		const JozzTrackPoint& labelPoint = variants[i].centerline[3];
		DrawString3D( { labelPoint.x, 0.60f + labelPoint.y, labelPoint.z + 7.0f },
					VariantColor( variants[i].variant, 1.0f ), "%s  %.0fm", variants[i].displayName,
					ComputeJozzTrackPolylineLength( variants[i].centerline, variants[i].pointCount ) );
	}
	DrawStartMeta();
	DrawString3D( { 72.0f, 0.45f, 164.0f }, VariantColor( JozzTrackVariant::GreenFlow, 1.0f ), "NE FAST" );
	DrawString3D( { -165.0f, 0.45f, 112.0f }, VariantColor( JozzTrackVariant::YellowTechnical, 1.0f ), "NW TECH / CHICANE" );
}
