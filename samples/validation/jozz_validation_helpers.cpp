// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_validation_helpers.h"

#include <cmath>
#include <cstdio>

bool CheckApprox( const char* label, float actual, float expected, float tolerance )
{
	float delta = std::fabs( actual - expected );
	if ( delta <= tolerance )
	{
		std::printf( "ok %s = %.4f (expected %.4f +/- %.4f)\n", label, actual, expected, tolerance );
		return true;
	}

	std::printf( "bad %s = %.4f (expected %.4f +/- %.4f)\n", label, actual, expected, tolerance );
	return false;
}

bool CheckTrue( const char* label, bool condition )
{
	std::printf( "%s %s\n", condition ? "ok" : "bad", label );
	return condition;
}

bool IsM6VehicleStateValid( const JozzVehicleM6& vehicle )
{
	if ( b3IsValidVec3( b3ToVec3( b3Body_GetPosition( vehicle.chassisId ) ) ) == false ||
		 b3IsValidVec3( b3Body_GetLinearVelocity( vehicle.chassisId ) ) == false )
	{
		return false;
	}

	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		if ( b3IsValidVec3( b3ToVec3( b3Body_GetPosition( vehicle.corners[corner].wheelId ) ) ) == false )
		{
			return false;
		}
	}

	return true;
}

float M6ChassisUpDotWorldUp( const JozzVehicleM6& vehicle )
{
	return b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisY ).y;
}

float M6ChassisHeading( const JozzVehicleM6& vehicle )
{
	b3Vec3 forward = b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisX );
	return std::atan2( forward.z, forward.x );
}

b3BodyId CreateM6SmokeGround( b3WorldId worldId, float friction )
{
	// Vehicle worlds run WITHOUT continuous collision (M7 decision). The
	// solver flags any body whose per-step motion exceeds half its smallest
	// shape extent as fast and sweeps it; above ~15 m/s that includes the
	// wheels themselves, and a ROLLING wheel's sweep starts in contact with
	// the ground, which trips the debug-build validation in the TOI push-back
	// (distance.c) every time. Vehicle worlds have no thin geometry - the
	// thinnest course collider is far thicker than one step of motion at any
	// reachable speed - so CCD buys nothing here. Future thin-wall content
	// must re-enable it (and cap speeds) or use thick colliders.
	b3World_EnableContinuous( worldId, false );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = { 0.0f, -1.0f, 0.0f };
	bodyDef.name = "m6_smoke_ground";
	b3BodyId groundId = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = friction;
	// Drivable surface: carries the terrain category the M6 split wheel
	// envelope keys on (rolling sphere = terrain only, sidewall = the rest).
	shapeDef.filter.categoryBits = JOZZ_M6_TERRAIN_CATEGORY;
	b3BoxHull ground = b3MakeBoxHull( 200.0f, 1.0f, 200.0f );
	b3CreateHullShape( groundId, &shapeDef, &ground.base );
	return groundId;
}
