// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "test_macros.h"

#include "box3d/box3d.h"
#include "box3d/collision.h"
#include "box3d/constants.h"
#include "box3d/math_functions.h"

static b3Wheel MakeDenseCrownedWheel( void )
{
	b3Vec2 profile[B3_MAX_WHEEL_PROFILE_POINTS];
	for ( int i = 0; i < B3_MAX_WHEEL_PROFILE_POINTS; ++i )
	{
		float t = 2.0f * (float)i / (float)( B3_MAX_WHEEL_PROFILE_POINTS - 1 ) - 1.0f;
		profile[i].x = 0.07f * t;
		profile[i].y = 0.50f - 0.004f * t * t;
	}

	return b3MakeWheelProfile( b3Vec3_zero, b3Vec3_axisZ, profile, B3_MAX_WHEEL_PROFILE_POINTS, 0.0f );
}

static int WheelManifoldPointLimit( void )
{
	b3Wheel wheel = MakeDenseCrownedWheel();
	b3Vec2 normalized[B3_MAX_WHEEL_PROFILE_POINTS];
	ENSURE( b3GetWheelProfile( &wheel, normalized ) == B3_MAX_WHEEL_PROFILE_POINTS );

	b3BoxHull ground = b3MakeBoxHull( 2.0f, 0.1f, 2.0f );
	b3Transform groundToWheel = b3Transform_identity;
	groundToWheel.p.y = -0.599f;

	b3LocalManifoldPoint points[32] = { 0 };
	b3LocalManifold manifold = { 0 };
	manifold.points = points;
	b3CollideWheelAndHull( &manifold, ARRAY_COUNT( points ), &wheel, &ground.base, groundToWheel );

	ENSURE( manifold.pointCount == B3_MAX_MANIFOLD_POINTS );
	ENSURE( manifold.points[0].pair.index1 == 0 );
	ENSURE( manifold.points[1].pair.index1 == 2 );
	ENSURE( manifold.points[2].pair.index1 == 5 );
	ENSURE( manifold.points[3].pair.index1 == 7 );
	return 0;
}

static int WheelWorldContactPointLimit( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	groundDef.position = ( b3Pos ){ 0.0f, -0.1f, 0.0f };
	b3BodyId groundId = b3CreateBody( worldId, &groundDef );
	b3BoxHull ground = b3MakeBoxHull( 2.0f, 0.1f, 2.0f );
	b3ShapeDef groundShapeDef = b3DefaultShapeDef();
	b3CreateHullShape( groundId, &groundShapeDef, &ground.base );

	b3BodyDef wheelBodyDef = b3DefaultBodyDef();
	wheelBodyDef.type = b3_dynamicBody;
	wheelBodyDef.position = ( b3Pos ){ 0.0f, 0.499f, 0.0f };
	wheelBodyDef.motionLocks = ( b3MotionLocks ){ true, true, true, true, true, true };
	b3BodyId wheelBodyId = b3CreateBody( worldId, &wheelBodyDef );

	b3Wheel wheel = MakeDenseCrownedWheel();
	b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
	wheelShapeDef.density = 1.0f;
	b3ShapeId wheelShapeId = b3CreateWheelShape( wheelBodyId, &wheelShapeDef, &wheel );

	b3World_Step( worldId, 1.0f / 60.0f, 1 );

	b3ContactData contacts[1];
	int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
	ENSURE( contactCount == 1 );
	ENSURE( contacts[0].manifoldCount == 1 );
	ENSURE( contacts[0].manifolds[0].pointCount == B3_MAX_MANIFOLD_POINTS );

	b3DestroyWorld( worldId );
	return 0;
}

int WheelShapeTest( void )
{
	RUN_SUBTEST( WheelManifoldPointLimit );
	RUN_SUBTEST( WheelWorldContactPointLimit );
	return 0;
}
