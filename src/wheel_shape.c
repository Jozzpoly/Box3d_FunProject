// SPDX-FileCopyrightText: 2026 Erin Catto / Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// JOZZ PATCH - wheel shape (surface of revolution about the spin axis).
//
// Everything specific to this shape lives in this one file so the patch stays a
// reviewable delta and upstream merges keep working (owner rule, 2026-07-24).
// The only edits outside this file are: the enum entry, the shape union member,
// the switch cases that dispatch to the functions below, and the contact
// registration.
//
// WHY THIS SHAPE EXISTS
// ---------------------
// Measured in the full car on a perfectly flat plate (a perfectly round wheel
// must read zero there): every wheel built out of stock shapes shakes 25-40x
// more than a sphere, and it does NOT depend on the shape count - one cylinder
// hull is as bad as a ring of 64 capsules. The wheel loses ground contact in
// roughly half of the steps, and more solver substeps make it worse, not
// better. The sphere is smooth for one reason: it is the only stock shape whose
// contact geometry does not change as the body spins, so the contact points
// persist and the solver keeps its warm start. Nothing in the stock vocabulary
// is rotationally symmetric about a wheel's axle AND wider than a point (a
// capsule laid along the axle would need width >= 2R).
//
// THE SHAPE
// ---------
// Draw the tire's cross-section as a chain of points - distance along the axle
// across, distance from the axle up - spin that drawing about the axle, and
// sweep the result with a ball of cornerRadius so no corner is ever sharp:
//
//   two points, equal height        flat tread, rounded shoulder (road tire)
//   middle point higher             crowned tread (dirt, motorcycle)
//   uneven heights left and right   asymmetric tread
//   one point                       a disc with a rounded rim
//   cornerRadius = halfWidth        balloon
//
// The cross-section is the shape. Change the numbers and you get a different
// wheel, with no new shape type, no new contact code and nothing to register.
// Whatever chain you hand in is sorted and reduced to its upper convex hull,
// because a dent in a tread is not something a rigid contact can honour, and
// silently filling it in beats pretending it is there.
//
// THE POINT OF THE CONTACT CODE
// -----------------------------
// The manifold is computed from the AXIS, never from vertices or facets. On a
// plane it reports the actual support feature of the continuous, piecewise-
// linear cross-section: one vertex, or both endpoints of a segment parallel to
// the plane. Point number 3 is still point number 3 after the wheel has turned.
// That is what the sphere gets for free and what every faceted wheel loses.
//
// Speculative distance decides whether that support feature is close enough to
// create a contact. It does NOT promote nearby profile samples into a wider
// footprint. A rigid shape cannot grow a contact patch with load; any later
// widening must come from an explicit compliance model, not manifold sampling.

#include "core.h"
#include "shape.h"

#include "box3d/collision.h"

#include <float.h>
#include <math.h>

#define B3_WHEEL_EPS 1.0e-6f

// Two cross-section points closer together than this are the same point as far
// as the solver is concerned, and are merged when the wheel is built. Contacts
// a few millimetres apart carrying the whole weight of the car are a redundant
// constraint: the shared normal makes the pair nearly parallel and the twist
// and rolling terms are worked out from how far apart the points are. Measured,
// a crowned tread whose points landed one linear slop apart put a NaN into the
// wheel's velocity and took the suspension joint down with it.
#define B3_WHEEL_MIN_POINT_SPACING ( 2.0f * B3_LINEAR_SLOP )

// Any unit vector perpendicular to a. Used only in the degenerate case where
// the wheel lies flat on its side and there is no radial direction.
static b3Vec3 b3WheelPerpendicular( b3Vec3 a )
{
	b3Vec3 seed = ( fabsf( a.x ) < 0.7f ) ? b3Vec3_axisX : b3Vec3_axisY;
	return b3Normalize( b3Cross( a, seed ) );
}

// The cross-section actually in force. A wheel that was never given one - a
// zeroed struct with only the three size fields filled in - reads as a flat
// tread, so hand-built wheels and anything written before the profile existed
// keep behaving exactly as they did.
int b3GetWheelProfile( const b3Wheel* wheel, b3Vec2* profile )
{
	if ( wheel->profileCount >= 1 && wheel->profileCount <= B3_MAX_WHEEL_PROFILE_POINTS )
	{
		for ( int i = 0; i < wheel->profileCount; ++i )
		{
			profile[i] = wheel->profile[i];
		}
		return wheel->profileCount;
	}

	float coreRadius = wheel->radius - wheel->cornerRadius;
	float coreHalfWidth = wheel->halfWidth - wheel->cornerRadius;
	coreRadius = coreRadius > 0.0f ? coreRadius : 0.0f;
	coreHalfWidth = coreHalfWidth > 0.0f ? coreHalfWidth : 0.0f;

	// A fully rounded shoulder leaves nothing between the two ends. Collapsing
	// to one point here means the manifold never hands the solver two contacts
	// at the same spot, which would double the normal impulse there.
	if ( coreHalfWidth <= B3_WHEEL_EPS )
	{
		profile[0] = ( b3Vec2 ){ 0.0f, coreRadius };
		return 1;
	}

	profile[0] = ( b3Vec2 ){ -coreHalfWidth, coreRadius };
	profile[1] = ( b3Vec2 ){ coreHalfWidth, coreRadius };
	return 2;
}

typedef struct b3WheelSupportFeature
{
	int index1;
	int index2;
	float value;
} b3WheelSupportFeature;

// Which feature of the piecewise-linear cross-section lies deepest in the
// direction described by its component along the axle and away from the axle.
// A unique maximum is a vertex. Adjacent equal maxima are the endpoints of the
// real support segment. The tolerance is only a few float ulps; it is not a
// geometric skin and is deliberately many orders smaller than speculative
// distance.
static b3WheelSupportFeature b3WheelProfileSupportFeature( const b3Vec2* profile, int count, float axial,
														   float radialLength )
{
	float values[B3_MAX_WHEEL_PROFILE_POINTS];
	int best = 0;
	float bestValue = -FLT_MAX;
	for ( int i = 0; i < count; ++i )
	{
		float value = profile[i].x * axial + profile[i].y * radialLength;
		values[i] = value;
		if ( value > bestValue )
		{
			bestValue = value;
			best = i;
		}
	}

	float tolerance = b3MaxFloat( B3_WHEEL_EPS, 8.0f * FLT_EPSILON * ( 1.0f + fabsf( bestValue ) ) );
	int first = best;
	int last = best;
	while ( first > 0 && fabsf( values[first - 1] - bestValue ) <= tolerance )
	{
		first -= 1;
	}
	while ( last + 1 < count && fabsf( values[last + 1] - bestValue ) <= tolerance )
	{
		last += 1;
	}

	return ( b3WheelSupportFeature ){ first, last, bestValue };
}

// Single support point used by generic support queries. If the support feature
// is a segment, either endpoint is a valid support point; choose the first one
// deterministically. Plane manifolds use the full feature above.
static int b3WheelProfileSupport( const b3Vec2* profile, int count, float axial, float radialLength )
{
	return b3WheelProfileSupportFeature( profile, count, axial, radialLength ).index1;
}

// Closest point of the cross-section REGION to q, in the half plane where x
// runs along the axle and y away from it. The region is everything under the
// chain, so the polygon is the chain plus its two feet on the axle - that
// region is what the support function above describes, and the two must agree.
static b3Vec2 b3ClosestPointInWheelProfile( const b3Vec2* profile, int count, b3Vec2 q )
{
	b3Vec2 polygon[B3_MAX_WHEEL_PROFILE_POINTS + 2];
	int n = 0;
	polygon[n++] = ( b3Vec2 ){ profile[0].x, 0.0f };
	for ( int i = 0; i < count; ++i )
	{
		polygon[n++] = profile[i];
	}
	polygon[n++] = ( b3Vec2 ){ profile[count - 1].x, 0.0f };

	// The chain runs left to right along the top, so the walk is clockwise and
	// the inside is on the right of every edge.
	bool inside = true;
	float twiceArea = 0.0f;
	float bestDistanceSqr = FLT_MAX;
	b3Vec2 best = polygon[0];

	for ( int i = 0; i < n; ++i )
	{
		b3Vec2 a = polygon[i];
		b3Vec2 b = polygon[( i + 1 ) % n];
		b3Vec2 edge = { b.x - a.x, b.y - a.y };
		b3Vec2 toQ = { q.x - a.x, q.y - a.y };

		twiceArea += a.x * b.y - b.x * a.y;
		if ( edge.x * toQ.y - edge.y * toQ.x > 0.0f )
		{
			inside = false;
		}

		float lengthSqr = edge.x * edge.x + edge.y * edge.y;
		float t = 0.0f;
		if ( lengthSqr > B3_WHEEL_EPS )
		{
			t = b3ClampFloat( ( toQ.x * edge.x + toQ.y * edge.y ) / lengthSqr, 0.0f, 1.0f );
		}
		b3Vec2 point = { a.x + t * edge.x, a.y + t * edge.y };
		float dx = q.x - point.x;
		float dy = q.y - point.y;
		float distanceSqr = dx * dx + dy * dy;
		if ( distanceSqr < bestDistanceSqr )
		{
			bestDistanceSqr = distanceSqr;
			best = point;
		}
	}

	// A single cross-section point, or a chain flat on the axle, encloses no
	// area: there is no inside to be in, only the boundary.
	if ( inside && fabsf( twiceArea ) > B3_WHEEL_EPS )
	{
		return q;
	}
	return best;
}

// Deepest point of the wheel surface in direction d (d must be unit).
b3Vec3 b3ComputeWheelSupport( const b3Wheel* wheel, b3Vec3 d )
{
	b3Vec2 profile[B3_MAX_WHEEL_PROFILE_POINTS];
	int count = b3GetWheelProfile( wheel, profile );

	float axial = b3Dot( d, wheel->axis );
	b3Vec3 radial = b3MulSub( d, axial, wheel->axis );
	float length = b3Length( radial );

	int best = b3WheelProfileSupport( profile, count, axial, length );

	b3Vec3 point = b3MulAdd( wheel->center, profile[best].x, wheel->axis );
	if ( length > B3_WHEEL_EPS )
	{
		point = b3MulAdd( point, profile[best].y / length, radial );
	}
	return b3MulAdd( point, wheel->cornerRadius, d );
}

b3Wheel b3MakeWheelProfile( b3Vec3 center, b3Vec3 axis, const b3Vec2* profile, int count, float cornerRadius )
{
	b3Wheel wheel = { 0 };
	wheel.center = center;
	wheel.axis = b3Normalize( axis );
	wheel.cornerRadius = cornerRadius > 0.0f ? cornerRadius : 0.0f;

	b3Vec2 points[B3_MAX_WHEEL_PROFILE_POINTS];
	int n = 0;
	for ( int i = 0; i < count && n < B3_MAX_WHEEL_PROFILE_POINTS; ++i )
	{
		b3Vec2 point = profile[i];
		if ( b3IsValidFloat( point.x ) == false || b3IsValidFloat( point.y ) == false )
		{
			continue;
		}
		// Behind the axle is the same place as in front of it once the drawing
		// is spun, so a negative height is a typo, not a shape.
		points[n].x = point.x;
		points[n].y = point.y > 0.0f ? point.y : 0.0f;
		n += 1;
	}
	if ( n == 0 )
	{
		points[0] = ( b3Vec2 ){ 0.0f, 0.0f };
		n = 1;
	}

	// Sort across the tread. Insertion sort: at most eight points.
	for ( int i = 1; i < n; ++i )
	{
		b3Vec2 key = points[i];
		int j = i - 1;
		while ( j >= 0 && points[j].x > key.x )
		{
			points[j + 1] = points[j];
			j -= 1;
		}
		points[j + 1] = key;
	}

	// Two points at the same place across the tread are one point; keep the
	// taller. Two contacts at one spot would double the load carried there.
	int unique = 0;
	for ( int i = 0; i < n; ++i )
	{
		if ( unique > 0 && points[i].x - points[unique - 1].x <= B3_WHEEL_EPS )
		{
			if ( points[i].y > points[unique - 1].y )
			{
				points[unique - 1].y = points[i].y;
			}
			continue;
		}
		points[unique] = points[i];
		unique += 1;
	}
	n = unique;

	// Upper convex hull. A point that sits below the line between its
	// neighbours is inside the solid and can never touch anything, so keeping
	// it would only cost a contact slot.
	int hullCount = 0;
	for ( int i = 0; i < n; ++i )
	{
		while ( hullCount >= 2 )
		{
			b3Vec2 o = wheel.profile[hullCount - 2];
			b3Vec2 a = wheel.profile[hullCount - 1];
			float cross = ( a.x - o.x ) * ( points[i].y - o.y ) - ( a.y - o.y ) * ( points[i].x - o.x );
			if ( cross < 0.0f )
			{
				break;
			}
			hullCount -= 1;
		}
		wheel.profile[hullCount] = points[i];
		hullCount += 1;
	}
	// A cross-section drawn finer than the solver can tell apart is thinned to
	// its two ends and its peak. Thinning symmetrically matters as much as
	// thinning at all: dropping points from one side would leave the car
	// standing on a tread that is taller on the left than on the right.
	bool tooFine = false;
	for ( int i = 1; i < hullCount; ++i )
	{
		if ( wheel.profile[i].x - wheel.profile[i - 1].x < B3_WHEEL_MIN_POINT_SPACING )
		{
			tooFine = true;
			break;
		}
	}
	if ( tooFine && hullCount > 2 )
	{
		b3Vec2 first = wheel.profile[0];
		b3Vec2 last = wheel.profile[hullCount - 1];
		int peak = 0;
		for ( int i = 1; i < hullCount - 1; ++i )
		{
			if ( wheel.profile[i].y > wheel.profile[peak].y )
			{
				peak = i;
			}
		}

		hullCount = 0;
		wheel.profile[hullCount++] = first;
		if ( peak > 0 && wheel.profile[peak].x - first.x >= B3_WHEEL_MIN_POINT_SPACING &&
			 last.x - wheel.profile[peak].x >= B3_WHEEL_MIN_POINT_SPACING )
		{
			wheel.profile[hullCount++] = wheel.profile[peak];
		}
		if ( last.x - first.x >= B3_WHEEL_MIN_POINT_SPACING )
		{
			wheel.profile[hullCount++] = last;
		}
		else if ( last.y > wheel.profile[0].y )
		{
			wheel.profile[0] = last;
		}
	}
	wheel.profileCount = hullCount;

	// The outer bounds. Everything outside this file - the broad phase, the
	// query proxy, the ray cast - works off these two numbers and must never
	// under-report, so an off-center cross-section is measured by its far side.
	float maxRadius = 0.0f;
	float maxHalfWidth = 0.0f;
	for ( int i = 0; i < hullCount; ++i )
	{
		float height = wheel.profile[i].y;
		float across = fabsf( wheel.profile[i].x );
		maxRadius = height > maxRadius ? height : maxRadius;
		maxHalfWidth = across > maxHalfWidth ? across : maxHalfWidth;
	}
	wheel.radius = maxRadius + wheel.cornerRadius;
	wheel.halfWidth = maxHalfWidth + wheel.cornerRadius;
	return wheel;
}

b3Wheel b3MakeWheel( b3Vec3 center, b3Vec3 axis, float radius, float halfWidth, float cornerRadius )
{
	float limit = radius < halfWidth ? radius : halfWidth;
	cornerRadius = b3ClampFloat( cornerRadius, 0.0f, limit > 0.0f ? limit : 0.0f );

	float coreRadius = radius - cornerRadius;
	float coreHalfWidth = halfWidth - cornerRadius;
	coreRadius = coreRadius > 0.0f ? coreRadius : 0.0f;
	coreHalfWidth = coreHalfWidth > 0.0f ? coreHalfWidth : 0.0f;

	b3Vec2 profile[2] = { { -coreHalfWidth, coreRadius }, { coreHalfWidth, coreRadius } };
	return b3MakeWheelProfile( center, axis, profile, 2, cornerRadius );
}

b3AABB b3ComputeWheelAABB( const b3Wheel* wheel, b3Transform transform )
{
	b3Vec3 center = b3TransformPoint( transform, wheel->center );
	b3Vec3 axis = b3RotateVector( transform.q, wheel->axis );

	b3Vec2 profile[B3_MAX_WHEEL_PROFILE_POINTS];
	int count = b3GetWheelProfile( wheel, profile );

	// Cylinder bound, taken over the cross-section: the extent along i is the
	// largest of (across the tread)*|a_i| + (height)*sin(angle between axis
	// and i), plus the sweep. A crowned tread is narrower than its own bounding
	// cylinder and this notices.
	float a[3] = { axis.x, axis.y, axis.z };
	float e[3];
	for ( int i = 0; i < 3; ++i )
	{
		float sine2 = 1.0f - a[i] * a[i];
		float sine = sqrtf( sine2 > 0.0f ? sine2 : 0.0f );
		float extent = 0.0f;
		for ( int j = 0; j < count; ++j )
		{
			float candidate = fabsf( profile[j].x ) * fabsf( a[i] ) + profile[j].y * sine;
			extent = candidate > extent ? candidate : extent;
		}
		e[i] = extent + wheel->cornerRadius;
	}
	b3Vec3 extent = ( b3Vec3 ){ e[0], e[1], e[2] };

	b3AABB aabb;
	aabb.lowerBound = b3Sub( center, extent );
	aabb.upperBound = b3Add( center, extent );
	return aabb;
}

// Mass of the enclosing solid cylinder. The rounded shoulder and the shape of
// the cross-section are both ignored, which overestimates the volume - by a few
// percent for a road tread, by more for a strongly crowned one. The vehicle
// code does not rely on this: it freezes wheel mass from a reference sphere
// (hard rule 1 of the wheel program), so this only has to be sane. Anything
// that does rely on it should be given a real solid-of-revolution integral.
b3MassData b3ComputeWheelMass( const b3Wheel* wheel, float density )
{
	float radius = wheel->radius;
	float width = 2.0f * wheel->halfWidth;
	float mass = density * B3_PI * radius * radius * width;

	float spin = 0.5f * mass * radius * radius;
	float transverse = ( 1.0f / 12.0f ) * mass * ( 3.0f * radius * radius + width * width );

	// I = spin * (a a^T) + transverse * (Identity - a a^T)
	b3Vec3 a = wheel->axis;
	float d = spin - transverse;
	b3MassData massData = { 0 };
	massData.mass = mass;
	massData.center = wheel->center;
	massData.inertia.cx = ( b3Vec3 ){ transverse + d * a.x * a.x, d * a.x * a.y, d * a.x * a.z };
	massData.inertia.cy = ( b3Vec3 ){ d * a.y * a.x, transverse + d * a.y * a.y, d * a.y * a.z };
	massData.inertia.cz = ( b3Vec3 ){ d * a.z * a.x, d * a.z * a.y, transverse + d * a.z * a.z };
	return massData;
}

// Ray cast. Uses the ENCLOSING square-edged cylinder: neither the shoulder
// rounding nor the cross-section is honoured, so a ray can report a hit early
// near the shoulder of a crowned tread. That is deliberate - it keeps the cast
// conservative (never a false miss), which is what mouse picking and ground
// probes need. The contact path does not go through here; it uses the analytic
// manifold above.
b3CastOutput b3RayCastWheel( const b3Wheel* wheel, const b3RayCastInput* input )
{
	b3CastOutput output = { 0 };

	float rayLength;
	b3Vec3 d = b3GetLengthAndNormalize( &rayLength, input->translation );
	if ( rayLength == 0.0f )
	{
		return output;
	}

	b3Vec3 axis = wheel->axis;
	b3Vec3 s = b3Sub( input->origin, wheel->center );
	float radius = wheel->radius;
	float halfWidth = wheel->halfWidth;

	float axialOrigin = b3Dot( s, axis );
	float axialDir = b3Dot( d, axis );
	b3Vec3 radialOrigin = b3MulSub( s, axialOrigin, axis );
	b3Vec3 radialDir = b3MulSub( d, axialDir, axis );

	float best = FLT_MAX;
	b3Vec3 bestNormal = b3Vec3_zero;

	// Side surface: |radialOrigin + t * radialDir|^2 = radius^2
	float qa = b3Dot( radialDir, radialDir );
	if ( qa > B3_WHEEL_EPS )
	{
		float qb = 2.0f * b3Dot( radialOrigin, radialDir );
		float qc = b3Dot( radialOrigin, radialOrigin ) - radius * radius;
		float discriminant = qb * qb - 4.0f * qa * qc;
		if ( discriminant >= 0.0f )
		{
			float root = sqrtf( discriminant );
			float candidates[2] = { ( -qb - root ) / ( 2.0f * qa ), ( -qb + root ) / ( 2.0f * qa ) };
			for ( int i = 0; i < 2; ++i )
			{
				float t = candidates[i];
				if ( t < 0.0f || t >= best )
				{
					continue;
				}
				if ( fabsf( axialOrigin + t * axialDir ) <= halfWidth )
				{
					best = t;
					bestNormal = b3Normalize( b3MulAdd( radialOrigin, t, radialDir ) );
				}
			}
		}
	}

	// The two flat sides of the wheel.
	if ( fabsf( axialDir ) > B3_WHEEL_EPS )
	{
		for ( int side = 0; side < 2; ++side )
		{
			float sign = ( side == 0 ) ? -1.0f : 1.0f;
			float t = ( sign * halfWidth - axialOrigin ) / axialDir;
			if ( t < 0.0f || t >= best )
			{
				continue;
			}
			b3Vec3 hit = b3MulAdd( radialOrigin, t, radialDir );
			if ( b3Dot( hit, hit ) <= radius * radius )
			{
				best = t;
				bestNormal = b3MulSV( sign, axis );
			}
		}
	}

	if ( best <= rayLength )
	{
		output.hit = true;
		output.fraction = best / rayLength;
		output.point = b3MulAdd( input->origin, best, d );
		output.normal = bestNormal;
	}
	return output;
}

// Contact against a single outward plane expressed in the wheel's frame.
// planeNormal points away from the other body (up, for ground). A rigid,
// continuous profile has only two possible support topologies against a plane:
// one vertex, or a segment whose two endpoints share the maximum support value.
//
// Stable feature ids are the normalized profile endpoint indices. They survive
// spin because the profile is defined around the axis, not around circumference
// facets. Speculative distance only decides whether the support feature exists;
// it never adds neighbouring samples to the manifold.
static void b3CollideWheelAndPlane( b3LocalManifold* manifold, int capacity, const b3Wheel* wheel, b3Vec3 planeNormal,
										float planeOffset )
{
	manifold->pointCount = 0;
	int manifoldCapacity = b3MinInt( capacity, B3_MAX_MANIFOLD_POINTS );
	if ( manifoldCapacity <= 0 )
	{
		return;
	}

	b3Vec2 profile[B3_MAX_WHEEL_PROFILE_POINTS];
	int profileCount = b3GetWheelProfile( wheel, profile );

	b3Vec3 toPlane = b3Neg( planeNormal );
	float axial = b3Dot( toPlane, wheel->axis );
	b3Vec3 radial = b3MulSub( toPlane, axial, wheel->axis );
	float radialLength = b3Length( radial );
	b3Vec3 radialDirection = radialLength > B3_WHEEL_EPS ? b3MulSV( 1.0f / radialLength, radial )
														 : b3WheelPerpendicular( wheel->axis );

	manifold->normal = toPlane;

	// separation(i) = base - supportValue(i). The support feature maximises
	// supportValue, therefore its separation is the minimum separation of the
	// whole wheel against this plane.
	float base = b3Dot( planeNormal, wheel->center ) - wheel->cornerRadius - planeOffset;
	b3WheelSupportFeature feature = b3WheelProfileSupportFeature( profile, profileCount, axial, radialLength );
	float supportSeparation = base - feature.value;
	if ( supportSeparation > B3_SPECULATIVE_DISTANCE )
	{
		return;
	}

	int indices[2] = { feature.index1, feature.index2 };
	int supportCount = feature.index1 == feature.index2 ? 1 : 2;
	int pointCount = b3MinInt( supportCount, manifoldCapacity );
	for ( int i = 0; i < pointCount; ++i )
	{
		int index = indices[i];
		b3Vec3 core = b3MulAdd( wheel->center, profile[index].x, wheel->axis );
		core = b3MulAdd( core, profile[index].y, radialDirection );
		b3Vec3 surface = b3MulAdd( core, wheel->cornerRadius, toPlane );

		manifold->points[i].point = surface;
		manifold->points[i].separation = base - ( profile[index].x * axial + profile[index].y * radialLength );
		manifold->points[i].pair = ( b3FeaturePair ){ 0 };
		manifold->points[i].pair.index1 = (uint8_t)index;
		manifold->points[i].pair.index2 = (uint8_t)index;
		manifold->points[i].triangleIndex = 0;
	}

	manifold->pointCount = pointCount;
}

// Contact against a segment with radius: covers both the capsule and (with a
// zero-length segment) the sphere. The bumpers on the map are capsules, and a
// wheel that has no capsule pair registered drives straight THROUGH them - that
// is exactly what happened when this was missing.
//
// Both shapes are "core plus radius", so the problem reduces to the closest
// points between the wheel's CROSS-SECTION SOLID and the segment, with the two
// radii added at the end. Alternating projection converges in a few passes
// because both cores are convex.
static void b3CollideWheelAndSegment( b3LocalManifold* manifold, int capacity, const b3Wheel* wheel, b3Vec3 p1,
									  b3Vec3 p2, float radiusB )
{
	manifold->pointCount = 0;
	if ( capacity <= 0 )
	{
		return;
	}

	b3Vec2 profile[B3_MAX_WHEEL_PROFILE_POINTS];
	int profileCount = b3GetWheelProfile( wheel, profile );
	b3Vec3 axis = wheel->axis;
	b3Vec3 edge = b3Sub( p2, p1 );
	float edgeLengthSqr = b3Dot( edge, edge );

	b3Vec3 onCore = wheel->center;
	b3Vec3 onSegment = p1;

	for ( int iteration = 0; iteration < 4; ++iteration )
	{
		// Closest point on the segment to the current core point.
		float t = 0.0f;
		if ( edgeLengthSqr > B3_WHEEL_EPS )
		{
			t = b3Dot( b3Sub( onCore, p1 ), edge ) / edgeLengthSqr;
			t = b3ClampFloat( t, 0.0f, 1.0f );
		}
		onSegment = b3MulAdd( p1, t, edge );

		// Closest point of the cross-section solid to that segment point. Done
		// in the flat drawing, then spun back out onto the wheel.
		b3Vec3 delta = b3Sub( onSegment, wheel->center );
		float axial = b3Dot( delta, axis );
		b3Vec3 radial = b3MulSub( delta, axial, axis );
		float radialLength = b3Length( radial );

		b3Vec2 closest = b3ClosestPointInWheelProfile( profile, profileCount, ( b3Vec2 ){ axial, radialLength } );

		onCore = b3MulAdd( wheel->center, closest.x, axis );
		if ( radialLength > B3_WHEEL_EPS )
		{
			onCore = b3MulAdd( onCore, closest.y / radialLength, radial );
		}
		else if ( closest.y > B3_WHEEL_EPS )
		{
			// The segment point sits on the axle, so every radial direction is
			// equally close. Pick one rather than collapsing onto the axle.
			onCore = b3MulAdd( onCore, closest.y, b3WheelPerpendicular( axis ) );
		}
	}

	b3Vec3 separationVector = b3Sub( onSegment, onCore );
	float distance = b3Length( separationVector );
	b3Vec3 normal;
	if ( distance > B3_WHEEL_EPS )
	{
		normal = b3MulSV( 1.0f / distance, separationVector );
	}
	else
	{
		// Cores overlap: push out radially, which is the only direction that
		// makes sense for a wheel and never points along the axle.
		b3Vec3 radial = b3MulSub( b3Sub( onSegment, wheel->center ), b3Dot( b3Sub( onSegment, wheel->center ), axis ),
								  axis );
		float radialLength = b3Length( radial );
		normal = radialLength > B3_WHEEL_EPS ? b3MulSV( 1.0f / radialLength, radial ) : b3WheelPerpendicular( axis );
	}

	float separation = distance - ( wheel->cornerRadius + radiusB );
	if ( separation > B3_SPECULATIVE_DISTANCE )
	{
		return;
	}

	manifold->normal = normal;
	manifold->points[0].point = b3MulAdd( onCore, wheel->cornerRadius, normal );
	manifold->points[0].separation = separation;
	manifold->points[0].pair = ( b3FeaturePair ){ 0 };
	manifold->points[0].triangleIndex = 0;
	manifold->pointCount = 1;
}

void b3CollideWheelAndCapsule( b3LocalManifold* manifold, int capacity, const b3Wheel* wheelA, const b3Capsule* capsuleB,
							   b3Transform transformBtoA )
{
	b3Vec3 p1 = b3TransformPoint( transformBtoA, capsuleB->center1 );
	b3Vec3 p2 = b3TransformPoint( transformBtoA, capsuleB->center2 );
	b3CollideWheelAndSegment( manifold, capacity, wheelA, p1, p2, capsuleB->radius );
}

void b3CollideWheelAndSphere( b3LocalManifold* manifold, int capacity, const b3Wheel* wheelA, const b3Sphere* sphereB,
							  b3Transform transformBtoA )
{
	b3Vec3 center = b3TransformPoint( transformBtoA, sphereB->center );
	b3CollideWheelAndSegment( manifold, capacity, wheelA, center, center, sphereB->radius );
}

void b3CollideWheelAndHull( b3LocalManifold* manifold, int capacity, const b3Wheel* wheelA, const b3HullData* hullB,
							b3Transform transformBtoA )
{
	manifold->pointCount = 0;

	const b3Plane* planes = b3GetHullPlanes( hullB );
	int faceCount = hullB->faceCount;
	if ( planes == NULL || faceCount == 0 )
	{
		return;
	}

	// Face-normal SAT using the wheel's analytic support. The reference face is
	// the one the wheel is least deep into.
	float bestSeparation = -FLT_MAX;
	b3Vec3 bestNormal = b3Vec3_zero;
	float bestOffset = 0.0f;

	for ( int i = 0; i < faceCount; ++i )
	{
		b3Vec3 normal = b3RotateVector( transformBtoA.q, planes[i].normal );
		b3Vec3 pointOnPlane = b3TransformPoint( transformBtoA, b3MulSV( planes[i].offset, planes[i].normal ) );
		float offset = b3Dot( normal, pointOnPlane );

		b3Vec3 support = b3ComputeWheelSupport( wheelA, b3Neg( normal ) );
		float separation = b3Dot( normal, support ) - offset;

		if ( separation > bestSeparation )
		{
			bestSeparation = separation;
			bestNormal = normal;
			bestOffset = offset;
		}
	}

	if ( bestSeparation > B3_SPECULATIVE_DISTANCE )
	{
		return;
	}

	b3CollideWheelAndPlane( manifold, capacity, wheelA, bestNormal, bestOffset );
}

void b3CollideWheelAndTriangle( b3LocalManifold* manifold, int capacity, const b3Wheel* wheelA, b3Vec3 v1, b3Vec3 v2,
								b3Vec3 v3 )
{
	manifold->pointCount = 0;

	b3Vec3 edge1 = b3Sub( v2, v1 );
	b3Vec3 edge2 = b3Sub( v3, v1 );
	b3Vec3 normal = b3Cross( edge1, edge2 );
	float area = b3Length( normal );
	if ( area <= B3_WHEEL_EPS )
	{
		return;
	}
	normal = b3MulSV( 1.0f / area, normal );
	float offset = b3Dot( normal, v1 );

	b3Vec3 support = b3ComputeWheelSupport( wheelA, b3Neg( normal ) );
	if ( b3Dot( normal, support ) - offset > B3_SPECULATIVE_DISTANCE )
	{
		return;
	}

	b3CollideWheelAndPlane( manifold, capacity, wheelA, normal, offset );

	// SIGN. The hull path wants the normal pointing from the wheel to the other
	// shape; the TRIANGLE path wants the opposite - b3CollideSphereAndTriangle
	// builds it as normalize(sphereCenter - closestPointOnTriangle), i.e. from
	// the triangle towards the convex shape. Getting this backwards drives the
	// wheel into the ground: measured, the car stood still and shook at
	// 26.5 m/s2 instead of rolling.
	manifold->normal = normal;
	manifold->triangleNormal = normal;
	// The wheel always meets the triangle's face: its surface is smooth, so the
	// deepest point can never be a vertex or an edge of the wheel.
	manifold->feature = b3_featureTriangleFace;

	// Drop points that fall outside the triangle. Without this a wheel spanning
	// several triangles would collect phantom contacts from the neighbours'
	// planes. Barycentric test on the point projected onto the triangle plane.
	int kept = 0;
	for ( int i = 0; i < manifold->pointCount; ++i )
	{
		b3Vec3 p = manifold->points[i].point;
		b3Vec3 projected = b3MulSub( p, b3Dot( normal, p ) - offset, normal );
		b3Vec3 r = b3Sub( projected, v1 );

		float d11 = b3Dot( edge1, edge1 );
		float d12 = b3Dot( edge1, edge2 );
		float d22 = b3Dot( edge2, edge2 );
		float dr1 = b3Dot( r, edge1 );
		float dr2 = b3Dot( r, edge2 );
		float denominator = d11 * d22 - d12 * d12;
		if ( denominator <= B3_WHEEL_EPS )
		{
			continue;
		}
		float u = ( d22 * dr1 - d12 * dr2 ) / denominator;
		float v = ( d11 * dr2 - d12 * dr1 ) / denominator;
		if ( u < 0.0f || v < 0.0f || u + v > 1.0f )
		{
			continue;
		}

		manifold->points[kept] = manifold->points[i];
		manifold->points[kept].triangleIndex = manifold->triangleIndex;
		kept += 1;
	}
	manifold->pointCount = kept;
}
