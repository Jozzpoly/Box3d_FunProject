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
// A solid cylinder of radius (radius - cornerRadius) and half width
// (halfWidth - cornerRadius), swept by a ball of cornerRadius. Convex, with an
// analytic support function:
//
//   cornerRadius = 0          -> square-edged cylinder (slick)
//   0 < cornerRadius < W/2    -> flat tread, rounded shoulder (road tire)
//   cornerRadius = W/2        -> fully rounded shoulder (balloon/offroad)
//
// THE POINT OF THE CONTACT CODE
// -----------------------------
// The manifold is computed from the AXIS, never from vertices or facets. On a
// plane it is exactly two points, one at each end of the tread width, and those
// two points are the same two points after the wheel rotates. That is what the
// sphere gets for free and what every faceted wheel loses.

#include "core.h"
#include "shape.h"

#include "box3d/collision.h"

#include <float.h>
#include <math.h>

#define B3_WHEEL_EPS 1.0e-6f

// Any unit vector perpendicular to a. Used only in the degenerate case where
// the wheel lies flat on its side and there is no radial direction.
static b3Vec3 b3WheelPerpendicular( b3Vec3 a )
{
	b3Vec3 seed = ( fabsf( a.x ) < 0.7f ) ? b3Vec3_axisX : b3Vec3_axisY;
	return b3Normalize( b3Cross( a, seed ) );
}

// Deepest point of the wheel surface in direction d (d must be unit).
b3Vec3 b3ComputeWheelSupport( const b3Wheel* wheel, b3Vec3 d )
{
	float coreRadius = wheel->radius - wheel->cornerRadius;
	float coreHalfWidth = wheel->halfWidth - wheel->cornerRadius;

	float axial = b3Dot( d, wheel->axis );
	b3Vec3 radial = b3MulSub( d, axial, wheel->axis );
	float length = b3Length( radial );

	b3Vec3 point = wheel->center;
	if ( length > B3_WHEEL_EPS )
	{
		point = b3MulAdd( point, coreRadius / length, radial );
	}
	point = b3MulAdd( point, axial >= 0.0f ? coreHalfWidth : -coreHalfWidth, wheel->axis );
	return b3MulAdd( point, wheel->cornerRadius, d );
}

b3AABB b3ComputeWheelAABB( const b3Wheel* wheel, b3Transform transform )
{
	b3Vec3 center = b3TransformPoint( transform, wheel->center );
	b3Vec3 axis = b3RotateVector( transform.q, wheel->axis );

	// Standard cylinder bound: the extent along i is halfWidth*|a_i| plus
	// radius*sin(angle between the axis and i).
	b3Vec3 extent;
	float a[3] = { axis.x, axis.y, axis.z };
	float e[3];
	for ( int i = 0; i < 3; ++i )
	{
		float sine2 = 1.0f - a[i] * a[i];
		e[i] = wheel->halfWidth * fabsf( a[i] ) + wheel->radius * sqrtf( sine2 > 0.0f ? sine2 : 0.0f );
	}
	extent = ( b3Vec3 ){ e[0], e[1], e[2] };

	b3AABB aabb;
	aabb.lowerBound = b3Sub( center, extent );
	aabb.upperBound = b3Add( center, extent );
	return aabb;
}

// Mass of the equivalent solid cylinder. The rounded shoulder is ignored, which
// overestimates the volume by at most a few percent at cornerRadius = W/2. The
// vehicle code does not rely on this: it freezes wheel mass from a reference
// sphere (hard rule 1 of the wheel program), so this only has to be sane.
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

// Ray cast. Uses the ENCLOSING square-edged cylinder: the shoulder rounding is
// ignored, so a ray can report a hit up to cornerRadius early near the corner.
// That is deliberate - it keeps the cast conservative (never a false miss),
// which is what mouse picking and ground probes need. The contact path does not
// go through here; it uses the analytic manifold above.
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
// planeNormal points away from the other body (up, for ground). Produces the
// two tread-edge points; their separations differ under camber, which is
// exactly the behaviour a real contact patch has.
//
// The feature ids are 0 and 1 and never change while the wheel spins - that is
// the whole reason this shape exists, because it is what keeps the solver's
// warm start alive from step to step.
static void b3CollideWheelAndPlane( b3LocalManifold* manifold, int capacity, const b3Wheel* wheel, b3Vec3 planeNormal,
									float planeOffset )
{
	manifold->pointCount = 0;
	if ( capacity <= 0 )
	{
		return;
	}

	b3Vec3 toPlane = b3Neg( planeNormal );
	float axial = b3Dot( toPlane, wheel->axis );
	b3Vec3 radial = b3MulSub( toPlane, axial, wheel->axis );
	float length = b3Length( radial );

	float coreRadius = wheel->radius - wheel->cornerRadius;
	float coreHalfWidth = wheel->halfWidth - wheel->cornerRadius;

	manifold->normal = toPlane;

	if ( length <= B3_WHEEL_EPS )
	{
		// Wheel lying flat on its side: the rim face meets the plane. One point
		// is enough - this is not a driving pose.
		b3Vec3 core = b3MulAdd( wheel->center, axial >= 0.0f ? coreHalfWidth : -coreHalfWidth, wheel->axis );
		core = b3MulAdd( core, coreRadius, b3WheelPerpendicular( wheel->axis ) );
		b3Vec3 surface = b3MulAdd( core, wheel->cornerRadius, toPlane );
		manifold->points[0].point = surface;
		manifold->points[0].separation = b3Dot( planeNormal, surface ) - planeOffset;
		manifold->points[0].pair = ( b3FeaturePair ){ 0 };
		manifold->points[0].triangleIndex = 0;
		manifold->pointCount = 1;
		return;
	}

	b3Vec3 radialDirection = b3MulSV( 1.0f / length, radial );

	int count = 0;
	for ( int side = 0; side < 2 && count < capacity; ++side )
	{
		float offset = ( side == 0 ) ? -coreHalfWidth : coreHalfWidth;
		b3Vec3 core = b3MulAdd( wheel->center, coreRadius, radialDirection );
		core = b3MulAdd( core, offset, wheel->axis );
		b3Vec3 surface = b3MulAdd( core, wheel->cornerRadius, toPlane );

		manifold->points[count].point = surface;
		manifold->points[count].separation = b3Dot( planeNormal, surface ) - planeOffset;
		manifold->points[count].pair = ( b3FeaturePair ){ 0 };
		// Stable id: which side of the tread this point is. Survives rotation.
		manifold->points[count].pair.index1 = (uint8_t)side;
		manifold->points[count].pair.index2 = (uint8_t)side;
		manifold->points[count].triangleIndex = 0;
		count += 1;
	}

	// A zero-width tread degenerates to one point; do not hand the solver two
	// coincident points, it would double the normal impulse at that spot.
	if ( count == 2 && coreHalfWidth <= B3_WHEEL_EPS )
	{
		count = 1;
	}

	manifold->pointCount = count;
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
