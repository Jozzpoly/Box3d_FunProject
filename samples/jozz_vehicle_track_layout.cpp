// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_track_layout.h"

#include "jozz_vehicle_world_layout.h"

#include <algorithm>
#include <cmath>

namespace
{

constexpr JozzTrackPoint kMainStraight[] = {
	{ -120.0f, 150.0f, 0.00f },
	{ 120.0f, 150.0f, 0.00f },
};

// The three routes share start/meta and the long N straight, then use separate
// return lines. They are intentionally compact enough for one readable map
// view, while the return leg prevents any wrong-way overlap.
constexpr JozzTrackPoint kGreenRoute[] = {
	{ -120.0f, 150.0f, 0.00f }, { 120.0f, 150.0f, 0.00f }, { 145.0f, 150.0f, 0.00f },
	{ 165.0f, 138.0f, 0.00f }, { 170.0f, 115.0f, 0.00f }, { 160.0f, 112.0f, 0.00f },
	{ 140.0f, 100.0f, 0.00f }, { -140.0f, 100.0f, 0.00f }, { -160.0f, 112.0f, 0.00f },
	{ -170.0f, 126.0f, 0.00f }, { -165.0f, 138.0f, 0.00f }, { -145.0f, 150.0f, 0.00f },
	{ -120.0f, 150.0f, 0.00f },
};

constexpr JozzTrackPoint kYellowRoute[] = {
	{ -120.0f, 150.0f, 0.00f }, { 120.0f, 150.0f, 0.00f }, { 138.0f, 150.0f, 0.10f },
	{ 154.0f, 140.0f, 0.25f }, { 145.0f, 128.0f, 0.30f }, { 160.0f, 116.0f, 0.30f },
	{ 145.0f, 110.0f, 0.30f }, { 125.0f, 102.0f, 0.30f }, { -125.0f, 102.0f, 0.30f },
	{ -145.0f, 112.0f, 0.30f }, { -160.0f, 116.0f, 0.30f }, { -145.0f, 128.0f, 0.30f },
	{ -154.0f, 140.0f, 0.25f }, { -138.0f, 150.0f, 0.10f }, { -120.0f, 150.0f, 0.00f },
};

constexpr JozzTrackPoint kRedRoute[] = {
	{ -120.0f, 150.0f, 0.00f }, { 120.0f, 150.0f, 0.00f }, { 142.0f, 150.0f, 0.15f },
	{ 168.0f, 142.0f, 0.40f }, { 178.0f, 124.0f, 0.60f }, { 164.0f, 116.0f, 0.60f },
	{ 138.0f, 104.0f, 0.60f }, { 116.0f, 90.0f, 0.60f }, { -116.0f, 90.0f, 0.60f },
	{ -138.0f, 104.0f, 0.60f }, { -164.0f, 116.0f, 0.60f }, { -178.0f, 130.0f, 0.60f },
	{ -168.0f, 142.0f, 0.40f }, { -142.0f, 150.0f, 0.15f }, { -120.0f, 150.0f, 0.00f },
};

constexpr JozzTrackPoint kGreenFastArc[] = {
	{ 120.0f, 150.0f, 0.00f }, { 145.0f, 150.0f, 0.00f }, { 165.0f, 138.0f, 0.00f },
	{ 170.0f, 115.0f, 0.00f }, { 160.0f, 112.0f, 0.00f }, { 140.0f, 100.0f, 0.00f },
};
constexpr JozzTrackPoint kGreenReturn[] = {
	{ 140.0f, 100.0f, 0.00f }, { -140.0f, 100.0f, 0.00f },
};
constexpr JozzTrackPoint kGreenBypass[] = {
	{ -140.0f, 100.0f, 0.00f }, { -160.0f, 112.0f, 0.00f }, { -170.0f, 126.0f, 0.00f },
};

constexpr JozzTrackPoint kYellowFastArc[] = {
	{ 120.0f, 150.0f, 0.00f }, { 138.0f, 150.0f, 0.10f }, { 154.0f, 140.0f, 0.25f },
};
constexpr JozzTrackPoint kYellowChicane[] = {
	{ 154.0f, 140.0f, 0.25f }, { 145.0f, 128.0f, 0.30f }, { 160.0f, 116.0f, 0.30f },
	{ 145.0f, 110.0f, 0.30f }, { 125.0f, 102.0f, 0.30f },
};
constexpr JozzTrackPoint kYellowHairpin[] = {
	{ 125.0f, 102.0f, 0.30f }, { -125.0f, 102.0f, 0.30f }, { -145.0f, 112.0f, 0.30f },
};

constexpr JozzTrackPoint kRedFastArc[] = {
	{ 120.0f, 150.0f, 0.00f }, { 142.0f, 150.0f, 0.15f }, { 168.0f, 142.0f, 0.40f },
	{ 178.0f, 124.0f, 0.60f },
};
constexpr JozzTrackPoint kRedChicane[] = {
	{ 178.0f, 124.0f, 0.60f }, { 164.0f, 116.0f, 0.60f }, { 138.0f, 104.0f, 0.60f },
	{ 116.0f, 90.0f, 0.60f },
};
constexpr JozzTrackPoint kRedHairpin[] = {
	{ 116.0f, 90.0f, 0.60f }, { -116.0f, 90.0f, 0.60f }, { -138.0f, 104.0f, 0.60f },
};

constexpr JozzTrackVariantSpec kVariants[] = {
	{ "green", "Green Flow", JozzTrackVariant::GreenFlow, kJozzTrackGreenMask, kGreenRoute,
	  (int)( sizeof( kGreenRoute ) / sizeof( kGreenRoute[0] ) ), 12.0f, 16.0f, 0.0f, 0.15f },
	{ "yellow", "Yellow Technical", JozzTrackVariant::YellowTechnical, kJozzTrackYellowMask, kYellowRoute,
	  (int)( sizeof( kYellowRoute ) / sizeof( kYellowRoute[0] ) ), 11.0f, 14.0f, 0.0f, 0.35f },
	{ "red", "Red Stress", JozzTrackVariant::RedStress, kJozzTrackRedMask, kRedRoute,
	  (int)( sizeof( kRedRoute ) / sizeof( kRedRoute[0] ) ), 10.0f, 12.0f, 0.0f, 0.60f },
};

#define TRACK_SEGMENT( id, kind, mask, points, width, runoff, minY, maxY, speedMin, speedMax, tile ) \
	{ id, kind, mask, points, (int)( sizeof( points ) / sizeof( points[0] ) ), width, runoff, minY, maxY, speedMin, speedMax, tile }

constexpr JozzTrackSegmentSpec kSegments[] = {
	TRACK_SEGMENT( "start-meta-main-straight", JozzTrackSegmentKind::StartMeta, kJozzTrackAllVariantsMask,
				   kMainStraight, 10.0f, 16.0f, 0.0f, 0.0f, 60.0f, 140.0f, JozzTrackTileHint::N ),
	TRACK_SEGMENT( "green-fast-arc", JozzTrackSegmentKind::FastArc, kJozzTrackGreenMask, kGreenFastArc,
				   12.0f, 16.0f, 0.0f, 0.15f, 55.0f, 95.0f, JozzTrackTileHint::NE ),
	TRACK_SEGMENT( "green-constant-return", JozzTrackSegmentKind::ConstantRadiusArc, kJozzTrackGreenMask,
				   kGreenReturn, 12.0f, 16.0f, 0.0f, 0.15f, 45.0f, 80.0f, JozzTrackTileHint::N ),
	TRACK_SEGMENT( "green-flow-bypass", JozzTrackSegmentKind::BranchBypass, kJozzTrackGreenMask, kGreenBypass,
				   12.0f, 16.0f, 0.0f, 0.15f, 35.0f, 70.0f, JozzTrackTileHint::NW ),
	TRACK_SEGMENT( "yellow-fast-arc", JozzTrackSegmentKind::FastArc, kJozzTrackYellowMask, kYellowFastArc,
				   11.0f, 14.0f, 0.0f, 0.35f, 60.0f, 110.0f, JozzTrackTileHint::NE ),
	TRACK_SEGMENT( "yellow-chicane", JozzTrackSegmentKind::Chicane, kJozzTrackYellowMask, kYellowChicane,
				   11.0f, 14.0f, 0.0f, 0.35f, 25.0f, 55.0f, JozzTrackTileHint::NW ),
	TRACK_SEGMENT( "yellow-hairpin", JozzTrackSegmentKind::Hairpin, kJozzTrackYellowMask, kYellowHairpin,
				   11.0f, 14.0f, 0.0f, 0.35f, 20.0f, 45.0f, JozzTrackTileHint::NW ),
	TRACK_SEGMENT( "yellow-height-transition", JozzTrackSegmentKind::HeightSection, kJozzTrackYellowMask,
				   kYellowChicane, 11.0f, 14.0f, 0.0f, 0.35f, 25.0f, 60.0f, JozzTrackTileHint::NW ),
	TRACK_SEGMENT( "red-late-apex-fast-arc", JozzTrackSegmentKind::FastArc, kJozzTrackRedMask, kRedFastArc,
				   10.0f, 12.0f, 0.0f, 0.60f, 70.0f, 120.0f, JozzTrackTileHint::NE ),
	TRACK_SEGMENT( "red-alternating-chicane", JozzTrackSegmentKind::Chicane, kJozzTrackRedMask, kRedChicane,
				   10.0f, 12.0f, 0.0f, 0.60f, 25.0f, 60.0f, JozzTrackTileHint::NW ),
	TRACK_SEGMENT( "red-stress-hairpin", JozzTrackSegmentKind::Hairpin, kJozzTrackRedMask, kRedHairpin,
				   10.0f, 12.0f, 0.0f, 0.60f, 18.0f, 42.0f, JozzTrackTileHint::NW ),
	TRACK_SEGMENT( "red-height-articulation", JozzTrackSegmentKind::HeightSection, kJozzTrackRedMask, kRedChicane,
				   10.0f, 12.0f, 0.0f, 0.60f, 20.0f, 55.0f, JozzTrackTileHint::NW ),
};

#undef TRACK_SEGMENT

void AddError( std::vector<std::string>* errors, const std::string& message )
{
	if ( errors != nullptr )
	{
		errors->push_back( message );
	}
}

const char* VariantName( JozzTrackVariant variant )
{
	switch ( variant )
	{
	case JozzTrackVariant::GreenFlow: return "green";
	case JozzTrackVariant::YellowTechnical: return "yellow";
	case JozzTrackVariant::RedStress: return "red";
	}
	return "unknown";
}

bool Near( float a, float b, float epsilon = 0.01f )
{
	return std::fabs( a - b ) <= epsilon;
}

float CrossXZ( float ax, float az, float bx, float bz )
{
	return ax * bz - az * bx;
}

float PointSegmentDistanceXZ( float px, float pz, const JozzTrackPoint& a, const JozzTrackPoint& b )
{
	float dx = b.x - a.x;
	float dz = b.z - a.z;
	float lengthSq = dx * dx + dz * dz;
	if ( lengthSq < 0.0001f )
	{
		float ex = px - a.x;
		float ez = pz - a.z;
		return std::sqrt( ex * ex + ez * ez );
	}
	float t = ( ( px - a.x ) * dx + ( pz - a.z ) * dz ) / lengthSq;
	t = std::clamp( t, 0.0f, 1.0f );
	float ex = px - ( a.x + t * dx );
	float ez = pz - ( a.z + t * dz );
	return std::sqrt( ex * ex + ez * ez );
}

bool SegmentsIntersectXZ( const JozzTrackPoint& a, const JozzTrackPoint& b, const JozzTrackPoint& c,
							const JozzTrackPoint& d )
{
	float abx = b.x - a.x;
	float abz = b.z - a.z;
	float acx = c.x - a.x;
	float acz = c.z - a.z;
	float adx = d.x - a.x;
	float adz = d.z - a.z;
	float cdx = d.x - c.x;
	float cdz = d.z - c.z;
	float cax = a.x - c.x;
	float caz = a.z - c.z;
	float cbx = b.x - c.x;
	float cbz = b.z - c.z;
	float s1 = CrossXZ( abx, abz, acx, acz );
	float s2 = CrossXZ( abx, abz, adx, adz );
	float s3 = CrossXZ( cdx, cdz, cax, caz );
	float s4 = CrossXZ( cdx, cdz, cbx, cbz );
	return ( ( s1 > 0.0f && s2 < 0.0f ) || ( s1 < 0.0f && s2 > 0.0f ) ) &&
			( ( s3 > 0.0f && s4 < 0.0f ) || ( s3 < 0.0f && s4 > 0.0f ) );
}

float SegmentDistanceXZ( const JozzTrackPoint& a, const JozzTrackPoint& b, const JozzTrackPoint& c,
						 const JozzTrackPoint& d )
{
	if ( SegmentsIntersectXZ( a, b, c, d ) )
	{
		return 0.0f;
	}
	return std::min( std::min( PointSegmentDistanceXZ( a.x, a.z, c, d ),
							 PointSegmentDistanceXZ( b.x, b.z, c, d ) ),
						std::min( PointSegmentDistanceXZ( c.x, c.z, a, b ), PointSegmentDistanceXZ( d.x, d.z, a, b ) ) );
}

bool SegmentEndpointsMeet( const JozzTrackPoint& a, const JozzTrackPoint& b, const JozzTrackPoint& c,
						   const JozzTrackPoint& d )
{
	return PointSegmentDistanceXZ( a.x, a.z, c, c ) < 0.5f || PointSegmentDistanceXZ( a.x, a.z, d, d ) < 0.5f ||
			PointSegmentDistanceXZ( b.x, b.z, c, c ) < 0.5f || PointSegmentDistanceXZ( b.x, b.z, d, d ) < 0.5f;
}

} // namespace

const JozzTrackPoint* GetJozzTrackMainStraightPoints() { return kMainStraight; }
int GetJozzTrackMainStraightPointCount() { return (int)( sizeof( kMainStraight ) / sizeof( kMainStraight[0] ) ); }
const JozzTrackVariantSpec* GetJozzTrackVariantSpecs() { return kVariants; }
int GetJozzTrackVariantSpecCount() { return (int)( sizeof( kVariants ) / sizeof( kVariants[0] ) ); }
const JozzTrackSegmentSpec* GetJozzTrackSegmentSpecs() { return kSegments; }
int GetJozzTrackSegmentSpecCount() { return (int)( sizeof( kSegments ) / sizeof( kSegments[0] ) ); }

float ComputeJozzTrackPolylineLength( const JozzTrackPoint* points, int pointCount )
{
	if ( points == nullptr || pointCount < 2 )
	{
		return 0.0f;
	}

	float length = 0.0f;
	for ( int i = 1; i < pointCount; ++i )
	{
		float dx = points[i].x - points[i - 1].x;
		float dz = points[i].z - points[i - 1].z;
		float dy = points[i].y - points[i - 1].y;
		length += std::sqrt( dx * dx + dz * dz + dy * dy );
	}
	return length;
}

bool ValidateJozzTrackLayout( std::vector<std::string>* errors )
{
	if ( errors != nullptr )
	{
		errors->clear();
	}

	if ( GetJozzTrackVariantSpecCount() != 3 )
	{
		AddError( errors, "E3 track must expose exactly green, yellow and red variants" );
	}

	const JozzTrackPoint* straight = GetJozzTrackMainStraightPoints();
	float straightLength = ComputeJozzTrackPolylineLength( straight, GetJozzTrackMainStraightPointCount() );
	if ( straightLength < kJozzTrackMinimumMainStraight )
	{
		AddError( errors, "N main straight is shorter than 220 m" );
	}

	const JozzTrackPoint& start = kVariants[0].centerline[0];
	for ( int i = 0; i < GetJozzTrackVariantSpecCount(); ++i )
	{
		const JozzTrackVariantSpec& route = kVariants[i];
		const char* name = VariantName( route.variant );
		float length = ComputeJozzTrackPolylineLength( route.centerline, route.pointCount );
		if ( length < kJozzTrackMinimumLapLength || length > kJozzTrackMaximumLapLength )
		{
			AddError( errors, std::string( name ) + " lap length is outside the safe E3 envelope" );
		}
		if ( route.pointCount < 3 || !Near( route.centerline[0].x, route.centerline[route.pointCount - 1].x ) ||
				!Near( route.centerline[0].z, route.centerline[route.pointCount - 1].z ) )
		{
			AddError( errors, std::string( name ) + " route is not explicitly closed" );
		}
		if ( !Near( route.centerline[0].x, start.x ) || !Near( route.centerline[0].z, start.z ) )
		{
			AddError( errors, std::string( name ) + " does not share the common start/meta point" );
		}
		if ( route.roadWidth < 10.0f || route.roadWidth > 12.0f )
		{
			AddError( errors, std::string( name ) + " road width is outside 10-12 m" );
		}
		if ( route.runoffWidth < kJozzTrackMinimumRunoff )
		{
			AddError( errors, std::string( name ) + " runoff is below 12 m" );
		}
		const float corridorMargin = route.roadWidth * 0.5f + route.runoffWidth;
		for ( int p = 0; p < route.pointCount; ++p )
		{
			const JozzTrackPoint& point = route.centerline[p];
			if ( point.x < JozzWorldLayout::kLongTrackMinX || point.x > JozzWorldLayout::kLongTrackMaxX ||
					point.z < JozzWorldLayout::kLongTrackMinZ || point.z > JozzWorldLayout::kLongTrackMaxZ )
			{
				AddError( errors, std::string( name ) + " centerline leaves the N/NW/NE envelope" );
			}
			if ( point.x - corridorMargin < -JozzWorldLayout::kPlateHalfExtent ||
					point.x + corridorMargin > JozzWorldLayout::kPlateHalfExtent ||
					point.z - corridorMargin < JozzWorldLayout::kPlateTileHalf )
			{
				AddError( errors, std::string( name ) + " road plus runoff leaves N/NW/NE or enters Central tile" );
			}
			if ( point.x > -60.0f && point.x < 60.0f && point.z > -60.0f && point.z < 60.0f )
			{
				AddError( errors, std::string( name ) + " centerline enters Central Core" );
			}
			if ( point.y < route.allowedMinHeight - 0.001f || point.y > route.allowedMaxHeight + 0.001f )
			{
				AddError( errors, std::string( name ) + " exceeds its provisional height envelope" );
			}
			if ( p > 0 && std::fabs( point.y - route.centerline[p - 1].y ) > kJozzTrackMaximumAdjacentRise )
			{
				AddError( errors, std::string( name ) + " has a hard adjacent profile step" );
			}
		}
	}

	const JozzTrackSegmentKind requiredKinds[] = {
		JozzTrackSegmentKind::FastArc,
		JozzTrackSegmentKind::ConstantRadiusArc,
		JozzTrackSegmentKind::Hairpin,
		JozzTrackSegmentKind::Chicane,
		JozzTrackSegmentKind::HeightSection,
		JozzTrackSegmentKind::BranchBypass,
	};
	for ( JozzTrackSegmentKind kind : requiredKinds )
	{
		bool found = false;
		for ( int i = 0; i < GetJozzTrackSegmentSpecCount(); ++i )
		{
			if ( kSegments[i].kind == kind )
			{
				found = true;
				break;
			}
		}
		if ( !found )
		{
			AddError( errors, "E3 segment contract is missing a required topology kind" );
		}
	}

	for ( int i = 0; i < GetJozzTrackSegmentSpecCount(); ++i )
	{
		const JozzTrackSegmentSpec& segment = kSegments[i];
		if ( segment.centerline == nullptr || segment.pointCount < 2 || segment.roadWidth < 10.0f ||
				segment.roadWidth > 12.0f || segment.runoffWidth < kJozzTrackMinimumRunoff ||
				(segment.variantMask & kJozzTrackAllVariantsMask) == 0 )
		{
			AddError( errors, std::string( segment.id ) + " has an invalid footprint contract" );
		}
	}

	// A plan-view overlap is not automatically wrong: an explicit elevated
	// branch can pass over a lower branch. It is wrong when the road slabs
	// occupy the same X/Z corridor without the declared vertical clearance.
	// The common start/meta straight is built once and is intentionally exempt.
	for ( int left = 0; left < GetJozzTrackVariantSpecCount(); ++left )
	{
		for ( int right = left + 1; right < GetJozzTrackVariantSpecCount(); ++right )
		{
			const JozzTrackVariantSpec& aRoute = kVariants[left];
			const JozzTrackVariantSpec& bRoute = kVariants[right];
			bool overlapReported = false;
			for ( int a = 2; a < aRoute.pointCount; ++a )
			{
				for ( int b = 2; b < bRoute.pointCount; ++b )
				{
					// The first and last few points are the intentional common
					// fork/merge zones around start/meta. They are built once at
					// the same neutral level in the physical course contract;
					// variant-specific separation starts after these gates.
					bool startMerge = a <= 4 && b <= 4;
					bool finishMerge = a >= aRoute.pointCount - 4 && b >= bRoute.pointCount - 4;
					if ( startMerge || finishMerge )
					{
						continue;
					}
					const JozzTrackPoint& a0 = aRoute.centerline[a - 1];
					const JozzTrackPoint& a1 = aRoute.centerline[a];
					const JozzTrackPoint& b0 = bRoute.centerline[b - 1];
					const JozzTrackPoint& b1 = bRoute.centerline[b];
					if ( SegmentEndpointsMeet( a0, a1, b0, b1 ) )
					{
						continue;
					}
					float distance = SegmentDistanceXZ( a0, a1, b0, b1 );
					float roadCorridor = 0.5f * ( aRoute.roadWidth + bRoute.roadWidth );
					if ( distance < roadCorridor )
					{
						float aHeight = 0.5f * ( a0.y + a1.y );
						float bHeight = 0.5f * ( b0.y + b1.y );
						if ( std::fabs( aHeight - bHeight ) < kJozzTrackMinimumLayerClearance )
						{
							AddError( errors, std::string( VariantName( aRoute.variant ) ) + " / " + VariantName( bRoute.variant ) +
									" segments " + std::to_string( a - 1 ) + "/" + std::to_string( b - 1 ) +
									": overlapping road corridors need a wider separation or an explicit raised crossing" );
							overlapReported = true;
						}
					}
					if ( overlapReported )
					{
						break;
					}
				}
				if ( overlapReported )
				{
					break;
				}
			}
		}
	}

	return errors == nullptr || errors->empty();
}
