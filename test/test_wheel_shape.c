// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "test_macros.h"

#include "box3d/box3d.h"
#include "box3d/collision.h"
#include "box3d/constants.h"
#include "box3d/math_functions.h"

static b3Wheel MakeCrownedWheel( void )
{
	// Odd point count gives one real crown vertex. The shoulders are close
	// enough to enter the speculative band under deeper overlap, which is the
	// confound WHEEL-RIGID-01 must remove from manifold topology.
	b3Vec2 profile[5] = {
		{ -0.20f, 0.480f },
		{ -0.10f, 0.497f },
		{ 0.00f, 0.500f },
		{ 0.10f, 0.497f },
		{ 0.20f, 0.480f },
	};
	return b3MakeWheelProfile( b3Vec3_zero, b3Vec3_axisZ, profile, ARRAY_COUNT( profile ), 0.014f );
}

static b3Wheel MakeFlatWheel( void )
{
	return b3MakeWheel( b3Vec3_zero, b3Vec3_axisZ, 0.514f, 0.21875f, 0.014f );
}

static void CollideWithHorizontalTriangle( b3LocalManifold* manifold, b3LocalManifoldPoint* points, int capacity,
										   const b3Wheel* wheel, float separation, b3Quat wheelRotation )
{
	// In world space the wheel center is zero and the ground is horizontal.
	// Transform the triangle into the wheel frame so changing wheelRotation is
	// a true spin/camber test of the low-level manifold.
	float groundY = -wheel->radius - separation;
	b3Vec3 worldVertices[3] = {
		{ -3.0f, groundY, -3.0f },
		{ 0.0f, groundY, 3.0f },
		{ 3.0f, groundY, -3.0f },
	};
	b3Vec3 localVertices[3];
	for ( int i = 0; i < 3; ++i )
	{
		localVertices[i] = b3InvRotateVector( wheelRotation, worldVertices[i] );
	}

	*manifold = ( b3LocalManifold ){ 0 };
	manifold->points = points;
	b3CollideWheelAndTriangle( manifold, capacity, wheel, localVertices[0], localVertices[1], localVertices[2] );
}

static int WheelCrownUsesOneStrictSupportVertex( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	b3Vec2 normalized[B3_MAX_WHEEL_PROFILE_POINTS];
	ENSURE( b3GetWheelProfile( &wheel, normalized ) == 5 );

	const float separations[] = { 0.018f, -0.005f, -0.030f };
	uint8_t feature = UINT8_MAX;
	for ( int i = 0; i < ARRAY_COUNT( separations ); ++i )
	{
		b3LocalManifoldPoint points[8] = { 0 };
		b3LocalManifold manifold;
		CollideWithHorizontalTriangle( &manifold, points, ARRAY_COUNT( points ), &wheel, separations[i], b3Quat_identity );

		ENSURE( manifold.pointCount == 1 );
		ENSURE( manifold.points[0].pair.index1 == 2 );
		ENSURE( manifold.points[0].pair.index2 == 2 );
		ENSURE_SMALL( manifold.points[0].separation - separations[i], 2.0e-6f );
		if ( i == 0 )
		{
			feature = manifold.points[0].pair.index1;
		}
		else
		{
			ENSURE( manifold.points[0].pair.index1 == feature );
		}
	}
	return 0;
}

static int WheelFlatTreadUsesTwoSupportEndpoints( void )
{
	b3Wheel wheel = MakeFlatWheel();
	const float separations[] = { 0.018f, -0.005f, -0.030f };

	for ( int i = 0; i < ARRAY_COUNT( separations ); ++i )
	{
		b3LocalManifoldPoint points[8] = { 0 };
		b3LocalManifold manifold;
		CollideWithHorizontalTriangle( &manifold, points, ARRAY_COUNT( points ), &wheel, separations[i], b3Quat_identity );

		ENSURE( manifold.pointCount == 2 );
		ENSURE( manifold.points[0].pair.index1 == 0 );
		ENSURE( manifold.points[1].pair.index1 == 1 );
		ENSURE( manifold.points[0].pair.index1 != manifold.points[1].pair.index1 );
		ENSURE_SMALL( manifold.points[0].separation - separations[i], 2.0e-6f );
		ENSURE_SMALL( manifold.points[1].separation - separations[i], 2.0e-6f );
	}
	return 0;
}

static int WheelSupportFeaturesSurviveSpin( void )
{
	b3Wheel crown = MakeCrownedWheel();
	b3Wheel flat = MakeFlatWheel();
	b3Quat spin = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 1.173f );

	b3LocalManifoldPoint crownPoints[8] = { 0 };
	b3LocalManifold crownManifold;
	CollideWithHorizontalTriangle( &crownManifold, crownPoints, ARRAY_COUNT( crownPoints ), &crown, -0.005f, spin );
	ENSURE( crownManifold.pointCount == 1 );
	ENSURE( crownManifold.points[0].pair.index1 == 2 );

	b3LocalManifoldPoint flatPoints[8] = { 0 };
	b3LocalManifold flatManifold;
	CollideWithHorizontalTriangle( &flatManifold, flatPoints, ARRAY_COUNT( flatPoints ), &flat, -0.005f, spin );
	ENSURE( flatManifold.pointCount == 2 );
	ENSURE( flatManifold.points[0].pair.index1 == 0 );
	ENSURE( flatManifold.points[1].pair.index1 == 1 );
	return 0;
}

static int WheelCamberSelectsOneEndpoint( void )
{
	b3Wheel wheel = MakeFlatWheel();
	b3Quat camber = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 5.0f * B3_PI / 180.0f );

	b3LocalManifoldPoint points[8] = { 0 };
	b3LocalManifold manifold;
	CollideWithHorizontalTriangle( &manifold, points, ARRAY_COUNT( points ), &wheel, -0.005f, camber );

	ENSURE( manifold.pointCount == 1 );
	ENSURE( manifold.points[0].pair.index1 == 1 );
	return 0;
}

static int WheelManifoldHonorsCallerCapacity( void )
{
	b3Wheel wheel = MakeFlatWheel();
	b3LocalManifoldPoint point = { 0 };
	b3LocalManifold manifold;
	CollideWithHorizontalTriangle( &manifold, &point, 1, &wheel, -0.005f, b3Quat_identity );

	ENSURE( manifold.pointCount == 1 );
	ENSURE( manifold.points[0].pair.index1 == 0 );
	return 0;
}

static int WheelOutsideSpeculativeDistanceHasNoContact( void )
{
	b3Wheel crown = MakeCrownedWheel();
	b3Wheel flat = MakeFlatWheel();
	float separation = B3_SPECULATIVE_DISTANCE + 0.001f;

	b3LocalManifoldPoint crownPoints[2] = { 0 };
	b3LocalManifold crownManifold;
	CollideWithHorizontalTriangle( &crownManifold, crownPoints, ARRAY_COUNT( crownPoints ), &crown, separation,
								   b3Quat_identity );
	ENSURE( crownManifold.pointCount == 0 );

	b3LocalManifoldPoint flatPoints[2] = { 0 };
	b3LocalManifold flatManifold;
	CollideWithHorizontalTriangle( &flatManifold, flatPoints, ARRAY_COUNT( flatPoints ), &flat, separation,
								   b3Quat_identity );
	ENSURE( flatManifold.pointCount == 0 );
	return 0;
}

static int WheelWorldFeaturePersistsWhileSpinning( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	groundDef.position = ( b3Pos ){ 0.0f, -0.1f, 0.0f };
	b3BodyId groundId = b3CreateBody( worldId, &groundDef );
	b3BoxHull ground = b3MakeBoxHull( 2.0f, 0.1f, 2.0f );
	b3ShapeDef groundShapeDef = b3DefaultShapeDef();
	groundShapeDef.baseMaterial.friction = 0.0f;
	b3CreateHullShape( groundId, &groundShapeDef, &ground.base );

	b3BodyDef wheelBodyDef = b3DefaultBodyDef();
	wheelBodyDef.type = b3_dynamicBody;
	wheelBodyDef.position = ( b3Pos ){ 0.0f, 0.509f, 0.0f };
	wheelBodyDef.angularVelocity = ( b3Vec3 ){ 0.0f, 0.0f, 8.0f };
	wheelBodyDef.motionLocks.linearX = true;
	wheelBodyDef.motionLocks.linearY = true;
	wheelBodyDef.motionLocks.linearZ = true;
	wheelBodyDef.motionLocks.angularX = true;
	wheelBodyDef.motionLocks.angularY = true;
	b3BodyId wheelBodyId = b3CreateBody( worldId, &wheelBodyDef );

	b3Wheel wheel = MakeCrownedWheel();
	b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
	wheelShapeDef.density = 1.0f;
	wheelShapeDef.baseMaterial.friction = 0.0f;
	b3ShapeId wheelShapeId = b3CreateWheelShape( wheelBodyId, &wheelShapeDef, &wheel );

	uint32_t featureId = UINT32_MAX;
	for ( int step = 0; step < 8; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );

		b3ContactData contacts[1];
		int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
		ENSURE( contactCount == 1 );
		ENSURE( contacts[0].manifoldCount == 1 );
		ENSURE( contacts[0].manifolds[0].pointCount == 1 );
		const b3ManifoldPoint* point = contacts[0].manifolds[0].points;
		if ( step == 0 )
		{
			featureId = point->featureId;
		}
		else
		{
			ENSURE( point->featureId == featureId );
			ENSURE( point->persisted );
		}
	}

	b3DestroyWorld( worldId );
	return 0;
}

int WheelShapeTest( void )
{
	RUN_SUBTEST( WheelCrownUsesOneStrictSupportVertex );
	RUN_SUBTEST( WheelFlatTreadUsesTwoSupportEndpoints );
	RUN_SUBTEST( WheelSupportFeaturesSurviveSpin );
	RUN_SUBTEST( WheelCamberSelectsOneEndpoint );
	RUN_SUBTEST( WheelManifoldHonorsCallerCapacity );
	RUN_SUBTEST( WheelOutsideSpeculativeDistanceHasNoContact );
	RUN_SUBTEST( WheelWorldFeaturePersistsWhileSpinning );
	return 0;
}
