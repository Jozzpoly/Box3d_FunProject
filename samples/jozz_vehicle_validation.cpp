// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_m5_vehicle.h"
#include "jozz_vehicle_m6_suspension_rig.h"
#include "jozz_vehicle_m7_suspension_import.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{

bool CheckSemanticPoint( const JozzVehicleAuditMetadata& metadata, const char* assetFile, const char* semanticName )
{
	b3Vec3 position = {};
	if ( FindJozzVehicleSemanticPoint( metadata, assetFile, semanticName, &position ) )
	{
		std::printf( "ok semantic %s / %s = (%.4f, %.4f, %.4f)\n", assetFile, semanticName, position.x, position.y,
					 position.z );
		return true;
	}

	std::printf( "missing semantic %s / %s\n", assetFile, semanticName );
	return false;
}

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

bool CheckContractRole( const JozzVehicleAssetContract& contract, const char* role, const char* expectedCategory )
{
	const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( contract, role );
	if ( binding == nullptr )
	{
		std::printf( "missing contract role %s\n", role );
		return false;
	}

	bool ok = true;
	if ( binding->resolved == false )
	{
		std::printf( "unresolved contract role %s\n", role );
		ok = false;
	}
	if ( binding->roleCategory != expectedCategory )
	{
		std::printf( "bad category for %s = %s, expected %s\n", role, binding->roleCategory.c_str(), expectedCategory );
		ok = false;
	}
	if ( binding->nodeIndexHint < 0 || binding->nodePathHint.empty() || binding->nameHint.empty() )
	{
		std::printf( "bad binding hints for %s\n", role );
		ok = false;
	}
	if ( binding->physicsAuthority )
	{
		std::printf( "bad physics authority for visual contract role %s\n", role );
		ok = false;
	}

	if ( ok )
	{
		std::printf( "ok contract %s -> %s node %d = (%.4f, %.4f, %.4f) BU\n", role, binding->nameHint.c_str(),
					 binding->nodeIndexHint, binding->positionBU.x, binding->positionBU.y, binding->positionBU.z );
	}

	return ok;
}

bool CheckTrue( const char* label, bool condition )
{
	std::printf( "%s %s\n", condition ? "ok" : "bad", label );
	return condition;
}

bool IsVehicleStateValid( const JozzVehicleM5& vehicle )
{
	b3Pos chassisPosition = b3Body_GetPosition( vehicle.chassisId );
	if ( b3IsValidVec3( b3ToVec3( chassisPosition ) ) == false ||
		 b3IsValidVec3( b3Body_GetLinearVelocity( vehicle.chassisId ) ) == false )
	{
		return false;
	}

	for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
	{
		if ( b3IsValidVec3( b3ToVec3( b3Body_GetPosition( vehicle.wheelIds[corner] ) ) ) == false )
		{
			return false;
		}
	}

	return true;
}

float ChassisUpDotWorldUp( const JozzVehicleM5& vehicle )
{
	b3Vec3 up = b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisY );
	return up.y;
}

float ChassisHeading( const JozzVehicleM5& vehicle )
{
	b3Vec3 forward = b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisX );
	return std::atan2( forward.z, forward.x );
}

// High-speed wheel smoothness probe. A 2026-07-05 playtest reported wheels
// "hopping"/losing ground contact at speed, worst on the unloaded front axle,
// and raising solver sub-steps did NOT help - which points away from solver
// convergence and toward the wheel geometry itself: a 32-side hull cylinder
// (the engine's hard cap) has ~2.5mm of radial ripple at this wheel radius, a
// washboard built into the wheel. This probe measures that hypothesis: same
// vehicle, same run, different wheel shapes, comparing front-wheel ground
// contact and vertical-velocity agitation at speed.
struct M5WheelProbeResult
{
	float topSpeed;
	float frontContactFraction; // fraction of sampled steps with both front wheels touching
	float frontVerticalRms;		// RMS of front wheel vertical velocity during the sampling window
};

M5WheelProbeResult RunM5WheelSmoothnessProbe( const JozzVehiclePrimitiveDefaults& defaults, int wheelShape, int cylinderSides )
{
	M5WheelProbeResult result = {};

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyId groundId;
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.position = { 0.0f, -1.0f, 0.0f };
		bodyDef.name = "m5_probe_ground";
		groundId = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.baseMaterial.friction = 0.8f;
		b3BoxHull ground = b3MakeBoxHull( 400.0f, 1.0f, 50.0f );
		b3CreateHullShape( groundId, &shapeDef, &ground.base );
	}

	JozzVehicleM5Config config =
		JozzVehicleM5DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
	config.wheelShape = wheelShape;
	config.wheelCylinderSides = cylinderSides;
	float spawnHeight = config.restDrop + config.wheelRadius + 0.05f;
	JozzVehicleM5 vehicle = CreateJozzVehicleM5( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

	const float timeStep = 1.0f / 60.0f;
	const int subStepCount = 4;

	// Settle, then accelerate to cruise.
	JozzVehicleM5DriveInput input = {};
	for ( int i = 0; i < 90; ++i )
	{
		UpdateJozzVehicleM5Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	input.drive = 1.0f;
	for ( int i = 0; i < 360; ++i )
	{
		UpdateJozzVehicleM5Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	// Sampling window at speed: 3 seconds of steady full throttle.
	int sampleSteps = 180;
	int frontContactSamples = 0;
	double verticalSquaredSum = 0.0;
	for ( int i = 0; i < sampleSteps; ++i )
	{
		UpdateJozzVehicleM5Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );

		JozzVehicleM5WheelTelemetry frontLeft = GetJozzVehicleM5WheelTelemetry( vehicle, JOZZ_M5_FRONT_LEFT );
		JozzVehicleM5WheelTelemetry frontRight = GetJozzVehicleM5WheelTelemetry( vehicle, JOZZ_M5_FRONT_RIGHT );
		if ( frontLeft.groundContact && frontRight.groundContact )
		{
			frontContactSamples += 1;
		}

		float vyLeft = b3Body_GetLinearVelocity( vehicle.wheelIds[JOZZ_M5_FRONT_LEFT] ).y;
		float vyRight = b3Body_GetLinearVelocity( vehicle.wheelIds[JOZZ_M5_FRONT_RIGHT] ).y;
		verticalSquaredSum += 0.5 * ( (double)vyLeft * vyLeft + (double)vyRight * vyRight );

		float speed = GetJozzVehicleM5ForwardSpeed( vehicle );
		if ( speed > result.topSpeed )
		{
			result.topSpeed = speed;
		}
	}

	result.frontContactFraction = (float)frontContactSamples / (float)sampleSteps;
	result.frontVerticalRms = (float)std::sqrt( verticalSquaredSum / (double)sampleSteps );

	DestroyJozzVehicleM5( &vehicle );
	b3DestroyWorld( worldId );
	return result;
}

bool RunM5WheelShapeExperiment( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m5 wheel smoothness probe (front axle, full throttle cruise):\n" );

	struct
	{
		const char* label;
		int shape;
		int sides;
	} cases[] = {
		{ "cylinder 12 sides", JOZZ_M5_WHEEL_CYLINDER, 12 },
		{ "cylinder 32 sides", JOZZ_M5_WHEEL_CYLINDER, 32 },
		{ "sphere (smooth)  ", JOZZ_M5_WHEEL_SPHERE, 32 },
	};

	bool ok = true;
	for ( auto& probe : cases )
	{
		M5WheelProbeResult result = RunM5WheelSmoothnessProbe( defaults, probe.shape, probe.sides );
		std::printf( "m5 probe %s: contact %.0f%%, front vy rms %.3f m/s, top speed %.1f m/s\n", probe.label,
					 100.0f * result.frontContactFraction, result.frontVerticalRms, result.topSpeed );
		ok &= CheckTrue( "m5 probe reaches cruise speed", result.topSpeed > 8.0f );
		ok &= CheckTrue( "m5 probe metrics are finite",
						 b3IsValidFloat( result.frontVerticalRms ) && b3IsValidFloat( result.frontContactFraction ) );
	}

	return ok;
}

// Headless M5 drive smoke: settle, drive straight, then steer. This guards the
// vehicle prefab against regressions in frames, motor conventions, and tuning
// without opening the samples GUI.
bool RunM5DriveSmoke( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m5 drive smoke: begin\n" );

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyId groundId;
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.position = { 0.0f, -1.0f, 0.0f };
		bodyDef.name = "m5_smoke_ground";
		groundId = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.baseMaterial.friction = 0.8f;
		b3BoxHull ground = b3MakeBoxHull( 150.0f, 1.0f, 150.0f );
		b3CreateHullShape( groundId, &shapeDef, &ground.base );
	}

	JozzVehicleM5Config config =
		JozzVehicleM5DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
	float spawnHeight = config.restDrop + config.wheelRadius + 0.05f;
	JozzVehicleM5 vehicle = CreateJozzVehicleM5( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

	const float timeStep = 1.0f / 60.0f;
	const int subStepCount = 4;

	bool ok = CheckTrue( "m5 vehicle created", vehicle.valid );

	// Settle under gravity with no input.
	JozzVehicleM5DriveInput input = {};
	for ( int i = 0; i < 120; ++i )
	{
		UpdateJozzVehicleM5Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	b3Pos settled = b3Body_GetPosition( vehicle.chassisId );
	float settleSag = spawnHeight - (float)settled.y;
	std::printf( "m5 settle sag %.3f m of %.3f m compression travel\n", settleSag, config.compressionTravel );
	ok &= CheckTrue( "m5 settle state is finite", IsVehicleStateValid( vehicle ) );
	// The springs must carry the chassis without collapsing onto the
	// compression limit; a sag near the limit means the suspension is not
	// actually supporting the vehicle.
	ok &= CheckTrue( "m5 suspension supports the chassis", settleSag > -0.05f && settleSag < 0.8f * config.compressionTravel );
	ok &= CheckTrue( "m5 settled upright", ChassisUpDotWorldUp( vehicle ) > 0.95f );

	// Steer while stationary (zero drive input). This is the scenario a 2026-07-05
	// manual playtest found broken: steering that only "works" once already
	// rolling, is very heavy at rest, and sometimes only one front wheel reaches
	// the target angle. The full-throttle steering check further below happens
	// only after 6 seconds of driving, so it could never have caught this - a
	// rolling tire needs much less torque to change its slip angle than a
	// stationary contact patch needs to twist in place against friction.
	{
		JozzVehicleM5DriveInput stationaryInput = {};
		stationaryInput.steer = 1.0f;
		for ( int i = 0; i < 90; ++i )
		{
			UpdateJozzVehicleM5Drive( vehicle, stationaryInput );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
		float angleLeft = b3WheelJoint_GetSteeringAngle( vehicle.wheelJointIds[JOZZ_M5_FRONT_LEFT] );
		float angleRight = b3WheelJoint_GetSteeringAngle( vehicle.wheelJointIds[JOZZ_M5_FRONT_RIGHT] );
		std::printf( "m5 stationary steer: left %.1f deg, right %.1f deg, rack target %.1f deg\n", 180.0f / B3_PI * angleLeft,
					 180.0f / B3_PI * angleRight, 180.0f / B3_PI * maxAngle );

		ok &= CheckTrue( "m5 stationary steer state is finite", IsVehicleStateValid( vehicle ) );

		// steer=+1 must be a LEFT turn: positive steering angles on both wheels
		// (left = -Z, positive rotation about +Y swings +X toward -Z). The sign
		// itself is asserted here because the first two attempts at this
		// convention got it wrong and only a manual playtest caught it.
		ok &= CheckTrue( "m5 left wheel steers left (+) while stationary", angleLeft > 0.5f * maxAngle );
		ok &= CheckTrue( "m5 right wheel steers left (+) while stationary", angleRight > 0.4f * maxAngle );

		// With Ackermann geometry, steering left makes the LEFT wheel the inner
		// wheel, so it must be steered tighter than the right. The expected
		// split comes from the exact function the drive path uses.
		float expectedLeft = 0.0f;
		float expectedRight = 0.0f;
		GetJozzVehicleM5SteeringTargets( config, maxAngle, &expectedLeft, &expectedRight );
		std::printf( "m5 ackermann targets: left %.1f deg, right %.1f deg\n", 180.0f / B3_PI * expectedLeft,
					 180.0f / B3_PI * expectedRight );
		if ( config.ackermannGeometry )
		{
			ok &= CheckTrue( "m5 ackermann inner (left) target exceeds outer", expectedLeft > expectedRight + 0.01f );
			ok &= CheckTrue( "m5 inner wheel actually steers tighter", angleLeft > angleRight + 0.005f );
		}

		// Linked tie rod: the actual angles must stay near the commanded
		// Ackermann split; a big divergence means the wheels steer independently.
		float divergence = std::fabs( ( angleLeft - angleRight ) - ( expectedLeft - expectedRight ) );
		float tieRodTolerance = config.tieRodToleranceDegrees * B3_PI / 180.0f;
		ok &= CheckTrue( "m5 tie rod keeps wheels coupled", divergence < tieRodTolerance + 0.03f );

		// Return the steering to center before the driving checks below, so
		// they start from the same clean state as before this sub-test existed.
		stationaryInput.steer = 0.0f;
		for ( int i = 0; i < 60; ++i )
		{
			UpdateJozzVehicleM5Drive( vehicle, stationaryInput );
			b3World_Step( worldId, timeStep, subStepCount );
		}
	}

	// Full throttle straight ahead.
	input.drive = 1.0f;
	for ( int i = 0; i < 360; ++i )
	{
		UpdateJozzVehicleM5Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	b3Pos afterDrive = b3Body_GetPosition( vehicle.chassisId );
	float driveDx = (float)( afterDrive.x - settled.x );
	float driveDz = (float)( afterDrive.z - settled.z );
	float forwardSpeed = GetJozzVehicleM5ForwardSpeed( vehicle );
	std::printf( "m5 drive displacement dx %.2f m, dz %.2f m, forward speed %.2f m/s\n", driveDx, driveDz, forwardSpeed );

	ok &= CheckTrue( "m5 drive state is finite", IsVehicleStateValid( vehicle ) );
	ok &= CheckTrue( "m5 drives forward (+x)", driveDx > 4.0f );
	ok &= CheckTrue( "m5 tracks straight", std::fabs( driveDz ) < 0.5f * std::fabs( driveDx ) );
	ok &= CheckTrue( "m5 forward speed positive", forwardSpeed > 2.0f );
	ok &= CheckTrue( "m5 stays upright while driving", ChassisUpDotWorldUp( vehicle ) > 0.85f );

	// Keep driving and steer.
	float headingBefore = ChassisHeading( vehicle );
	input.steer = 1.0f;
	for ( int i = 0; i < 240; ++i )
	{
		UpdateJozzVehicleM5Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	float headingDelta = ChassisHeading( vehicle ) - headingBefore;
	while ( headingDelta > B3_PI )
	{
		headingDelta -= 2.0f * B3_PI;
	}
	while ( headingDelta < -B3_PI )
	{
		headingDelta += 2.0f * B3_PI;
	}
	std::printf( "m5 steering heading delta %.3f rad\n", headingDelta );

	ok &= CheckTrue( "m5 steering state is finite", IsVehicleStateValid( vehicle ) );
	// Signed: steer=+1 = left turn = forward swinging from +X toward -Z, which
	// DECREASES heading = atan2(f.z, f.x). Asserting the sign, not just the
	// magnitude, is what would have caught the inverted A/D much earlier.
	ok &= CheckTrue( "m5 steering left turns left (heading decreases)", headingDelta < -0.15f );
	ok &= CheckTrue( "m5 stays upright while steering", ChassisUpDotWorldUp( vehicle ) > 0.80f );

	// Brake to a stop.
	input.drive = 0.0f;
	input.steer = 0.0f;
	input.brake = true;
	for ( int i = 0; i < 240; ++i )
	{
		UpdateJozzVehicleM5Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	b3Vec3 velocity = b3Body_GetLinearVelocity( vehicle.chassisId );
	ok &= CheckTrue( "m5 brake stops the vehicle", b3Length( velocity ) < 0.8f );

	DestroyJozzVehicleM5( &vehicle );
	b3DestroyWorld( worldId );

	std::printf( "m5 drive smoke: %s\n", ok ? "ok" : "FAILED" );
	return ok;
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

// Headless M6 rig smoke: the multi-body double-wishbone vehicle must settle
// on its coilovers, drive straight, steer with the SIGNED M5.2 convention
// through the physical rack/tie-rod trapezoid, self-center through the caster
// when the input is released at speed (M7 back-drivable steering), brake, and
// hold a stationary steer. This is the same gate RunM5DriveSmoke provides for
// the strut vehicle.
bool RunM6SuspensionRigSmoke( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m6 rig smoke: begin\n" );

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

	const float timeStep = 1.0f / 60.0f;
	const int subStepCount = 4;

	bool ok = CheckTrue( "m6 vehicle created", vehicle.valid );

	JozzVehicleM6DriveInput input = {};
	for ( int i = 0; i < 150; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	b3Pos settled = b3Body_GetPosition( vehicle.chassisId );
	float settleSag = spawnHeight - (float)settled.y;
	std::printf( "m6 settle sag %.3f m of %.3f m compression travel\n", settleSag, config.compressionTravel );
	ok &= CheckTrue( "m6 settle state is finite", IsM6VehicleStateValid( vehicle ) );
	ok &= CheckTrue( "m6 coilovers support the chassis", settleSag > -0.05f && settleSag < 0.8f * config.compressionTravel );
	ok &= CheckTrue( "m6 settled upright", M6ChassisUpDotWorldUp( vehicle ) > 0.95f );

	// The knuckles must sit where the arm geometry says, not sag away from
	// the chassis: compare each wheel center against its rest point.
	{
		float worstDrop = 0.0f;
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			JozzVehicleM6WheelTelemetry telemetry = GetJozzVehicleM6WheelTelemetry( vehicle, corner );
			float drop = -telemetry.suspensionTravel; // positive = wheel hanging below rest
			if ( drop > worstDrop )
			{
				worstDrop = drop;
			}
		}
		std::printf( "m6 worst wheel drop below rest %.3f m (rebound limit %.3f m)\n", worstDrop, config.reboundTravel );
		ok &= CheckTrue( "m6 arms hold the knuckles near rest", worstDrop < config.reboundTravel + 0.05f );
	}

	// Full throttle straight (torque-based drive).
	b3Pos beforeDrive = b3Body_GetPosition( vehicle.chassisId );
	input.drive = 1.0f;
	for ( int i = 0; i < 360; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	b3Pos afterDrive = b3Body_GetPosition( vehicle.chassisId );
	float driveDx = (float)( afterDrive.x - beforeDrive.x );
	float driveDz = (float)( afterDrive.z - beforeDrive.z );
	float forwardSpeed = GetJozzVehicleM6ForwardSpeed( vehicle );
	std::printf( "m6 drive displacement dx %.2f m, dz %.2f m, forward speed %.2f m/s\n", driveDx, driveDz, forwardSpeed );

	ok &= CheckTrue( "m6 drive state is finite", IsM6VehicleStateValid( vehicle ) );
	ok &= CheckTrue( "m6 drives forward (+x)", driveDx > 4.0f );
	ok &= CheckTrue( "m6 tracks straight", std::fabs( driveDz ) < 0.5f * std::fabs( driveDx ) );
	ok &= CheckTrue( "m6 stays upright while driving", M6ChassisUpDotWorldUp( vehicle ) > 0.85f );

	// Steer while driving: signed heading check, identical convention to M5.
	float headingBefore = M6ChassisHeading( vehicle );
	input.steer = 1.0f;
	for ( int i = 0; i < 240; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	float headingDelta = M6ChassisHeading( vehicle ) - headingBefore;
	while ( headingDelta > B3_PI )
	{
		headingDelta -= 2.0f * B3_PI;
	}
	while ( headingDelta < -B3_PI )
	{
		headingDelta += 2.0f * B3_PI;
	}
	std::printf( "m6 steering heading delta %.3f rad\n", headingDelta );
	ok &= CheckTrue( "m6 steering state is finite", IsM6VehicleStateValid( vehicle ) );
	ok &= CheckTrue( "m6 steering left turns left (heading decreases)", headingDelta < -0.10f );
	ok &= CheckTrue( "m6 stays upright while steering", M6ChassisUpDotWorldUp( vehicle ) > 0.80f );

	// Release the steering at speed: the hands-off rack must be back-driven
	// toward center by the caster trail alone (M7). This is the physical
	// self-centering every real car has on corner exit.
	{
		float angleAtRelease =
			0.5f * ( GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle +
					 GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT ).steeringAngle );
		input.steer = 0.0f;
		for ( int i = 0; i < 180; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float angleAfterRelease =
			0.5f * ( GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle +
					 GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT ).steeringAngle );
		std::printf( "m6 caster self-centering: %.1f deg at release -> %.1f deg after 3 s hands-off\n",
					 180.0f / B3_PI * angleAtRelease, 180.0f / B3_PI * angleAfterRelease );
		ok &= CheckTrue( "m6 released wheels self-center at speed (caster, no servo)",
						 std::fabs( angleAfterRelease ) < 0.5f * std::fabs( angleAtRelease ) &&
							 std::fabs( angleAfterRelease ) < 12.0f * B3_PI / 180.0f );
	}

	// Brake to a stop.
	input.drive = 0.0f;
	input.steer = 0.0f;
	input.brake = true;
	for ( int i = 0; i < 300; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}
	ok &= CheckTrue( "m6 brake stops the vehicle", b3Length( b3Body_GetLinearVelocity( vehicle.chassisId ) ) < 0.8f );
	input.brake = false;

	// Stationary steer, signed: steer=+1 must yaw both front knuckles LEFT
	// (positive steering angle) through the physical rack + tie rods, against
	// the full parking torque of loaded tires (the rack servo muscle).
	{
		JozzVehicleM6DriveInput steerInput = {};
		steerInput.steer = 1.0f;
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, steerInput );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
		JozzVehicleM6WheelTelemetry left = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT );
		JozzVehicleM6WheelTelemetry right = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT );
		std::printf( "m6 stationary steer: left %.1f deg, right %.1f deg (rack limit %.1f deg)\n",
					 180.0f / B3_PI * left.steeringAngle, 180.0f / B3_PI * right.steeringAngle, 180.0f / B3_PI * maxAngle );
		std::printf( "m6 rack calibration: translation %.4f m of %.4f m travel\n",
					 b3PrismaticJoint_GetTranslation( vehicle.rackJointId ), config.rackTravel );

		ok &= CheckTrue( "m6 stationary steer state is finite", IsM6VehicleStateValid( vehicle ) );
		ok &= CheckTrue( "m6 left knuckle steers left (+)", left.steeringAngle > 0.25f * maxAngle );
		ok &= CheckTrue( "m6 right knuckle steers left (+)", right.steeringAngle > 0.20f * maxAngle );
		// Ackermann from the physical trapezoid: turning left makes the LEFT
		// wheel the inner wheel, so it must steer tighter than the right.
		ok &= CheckTrue( "m6 trapezoid steers the inner wheel tighter",
						 left.steeringAngle > right.steeringAngle + 0.005f );

		// Hands off at standstill: friction beats the (near-zero) aligning
		// forces, so the wheels must STAY roughly where the driver left them -
		// parked wheels do not magically re-center.
		float angleBeforeRelease = left.steeringAngle;
		steerInput.steer = 0.0f;
		for ( int i = 0; i < 90; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, steerInput );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float angleAfterRelease = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
		std::printf( "m6 parked release: %.1f deg -> %.1f deg\n", 180.0f / B3_PI * angleBeforeRelease,
					 180.0f / B3_PI * angleAfterRelease );
		ok &= CheckTrue( "m6 parked wheels hold their angle (friction beats standstill forces)",
						 angleAfterRelease > 0.5f * angleBeforeRelease );
	}

	// Mixed-rig sanity: strut front + wishbone rear must also build and settle
	// (this is the per-axle flexibility the foundation promises).
	{
		JozzVehicleM6Config mixed = config;
		mixed.frontRigType = JOZZ_M6_RIG_INTEGRATED_STRUT;
		mixed.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
		JozzVehicleM6 mixedVehicle = CreateJozzVehicleM6( worldId, groundId, mixed, { 20.0f, spawnHeight, 20.0f } );
		JozzVehicleM6DriveInput idle = {};
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( mixedVehicle, idle );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		ok &= CheckTrue( "m6 mixed strut/wishbone vehicle settles finite", IsM6VehicleStateValid( mixedVehicle ) );
		ok &= CheckTrue( "m6 mixed vehicle stays upright", M6ChassisUpDotWorldUp( mixedVehicle ) > 0.95f );
		DestroyJozzVehicleM6( &mixedVehicle );
	}

	DestroyJozzVehicleM6( &vehicle );
	b3DestroyWorld( worldId );

	std::printf( "m6 rig smoke: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// Hands-off alignment probe (M7): launch the vehicle into a slide on a slick
// surface with the steering released. The M6 software assist that blended the
// command toward the travel direction is gone; counter-steer must now be
// back-driven MECHANICALLY through the tie rods and the free rack by the
// contact forces (caster trail, scrub radius, unsprung mass offset - all of
// them real). The falsification pass FREEZES the rack with an absurd friction
// force: if the alignment were scripted anywhere above the physics, it would
// survive; because it is a torque path through the linkage, a frozen rack
// must kill it.
//
// Measured note (2026-07-06): zeroing the CASTER alone does NOT kill the
// effect - the scrub radius (kingpin offset) and the knuckle/wheel mass
// offset from the kingpin axis also align the wheels in a slide. Real
// suspension design fights exactly these terms; recorded here so nobody
// "fixes" them into an assertion again.
bool RunM7HandsOffAlignProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m7 hands-off align probe:\n" );

	bool ok = true;
	float meanAlign[2] = {};
	float meanSteer[2] = {};
	float rackSpeedRms[2] = {};

	for ( int pass = 0; pass < 2; ++pass )
	{
		bool rackFree = pass == 0;

		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		// Slick ground makes the slide easy to hold and repeat.
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.5f );

		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		if ( rackFree == false )
		{
			// Frozen rack: hands-off friction so large the linkage cannot move.
			config.rackStaticFrictionForce = 1.0e6f;
			config.rackKineticFrictionForce = 1.0e6f;
		}
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;

		JozzVehicleM6DriveInput input = {};
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		// Build straight-line speed first.
		input.drive = 1.0f;
		for ( int i = 0; i < 330; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		// Force a deterministic slide: rotate every body's velocity 25 degrees
		// toward +Z (travel right of the nose) while the chassis keeps
		// pointing forward. A scripted steering flick cannot HOLD a slide -
		// grip realigns the car within a fraction of a second and the
		// measurement window sees nothing - while this is the exact
		// "car moving one way, nose another" state that makes real wheels
		// chase the travel direction.
		{
			float slideAngle = 25.0f * B3_PI / 180.0f;
			b3Quat slideRotation = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -slideAngle );
			b3BodyId slideBodies[22];
			int slideBodyCount = 0;
			slideBodies[slideBodyCount++] = vehicle.chassisId;
			slideBodies[slideBodyCount++] = vehicle.rackId;
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				slideBodies[slideBodyCount++] = vehicle.corners[corner].wheelId;
				slideBodies[slideBodyCount++] = vehicle.corners[corner].knuckleId;
				slideBodies[slideBodyCount++] = vehicle.corners[corner].upperArmId;
				slideBodies[slideBodyCount++] = vehicle.corners[corner].lowerArmId;
				slideBodies[slideBodyCount++] = vehicle.corners[corner].trailingArmId;
			}
			for ( int i = 0; i < slideBodyCount; ++i )
			{
				if ( B3_IS_NON_NULL( slideBodies[i] ) )
				{
					b3Body_SetLinearVelocity( slideBodies[i],
											  b3RotateVector( slideRotation, b3Body_GetLinearVelocity( slideBodies[i] ) ) );
				}
			}
		}

		// Hands fully off, coast, and watch the wheels during the slide.
		input.drive = 0.0f;
		input.steer = 0.0f;
		float alignAccum = 0.0f;
		float steerAccum = 0.0f;
		float rackSpeedSquaredAccum = 0.0f;
		int samples = 0;
		for ( int i = 0; i < 45; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );

			float align = GetJozzVehicleM6AlignmentAngle( vehicle );
			JozzVehicleM6WheelTelemetry left = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT );
			JozzVehicleM6WheelTelemetry right = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT );
			alignAccum += align;
			steerAccum += 0.5f * ( left.steeringAngle + right.steeringAngle );
			float rackSpeed = B3_IS_NON_NULL( vehicle.rackJointId ) ? b3PrismaticJoint_GetSpeed( vehicle.rackJointId ) : 0.0f;
			rackSpeedSquaredAccum += rackSpeed * rackSpeed;
			samples += 1;
		}

		meanAlign[pass] = alignAccum / (float)samples;
		meanSteer[pass] = steerAccum / (float)samples;
		rackSpeedRms[pass] = std::sqrt( rackSpeedSquaredAccum / (float)samples );
		std::printf( "m7 hands-off probe rack %s: mean align target %.1f deg, mean front steer %.1f deg, rack rms %.2f m/s\n",
					 rackFree ? "FREE  " : "FROZEN", 180.0f / B3_PI * meanAlign[pass], 180.0f / B3_PI * meanSteer[pass],
					 rackSpeedRms[pass] );

		ok &= CheckTrue( "m7 hands-off probe state is finite", IsM6VehicleStateValid( vehicle ) );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	// The flick was LEFT, so during the slide the car noses left of its travel
	// direction and the aligning angle points RIGHT (negative) - signed, like
	// every steering assertion since the M5.2 A/D lesson.
	ok &= CheckTrue( "m7 slide creates a rightward align target (car noses left)", meanAlign[0] < -0.03f );

	// The free rack must be back-driven toward the slide: same sign as the
	// align target and a clearly nonzero magnitude.
	ok &= CheckTrue( "m7 contact forces steer the released wheels into the slide",
					 meanSteer[0] * meanAlign[0] > 0.0f &&
						 std::fabs( meanSteer[0] ) > 2.0f * B3_PI / 180.0f );

	// Falsification: freezing the rack must kill the counter-steer. A scripted
	// alignment would not care whether the linkage can move.
	ok &= CheckTrue( "m7 frozen rack kills the counter-steer (proof the path is mechanical)",
					 std::fabs( meanSteer[1] ) < 0.35f * std::fabs( meanSteer[0] ) );

	// Anti-shimmy: the free rack must not oscillate violently (death wobble).
	ok &= CheckTrue( "m7 free rack stays calm (no shimmy)", rackSpeedRms[0] < 1.5f );

	return ok;
}

// Landing integrity probe (M7): fly the default vehicle off a virtual crest
// at road speed, land, and demand the suspension SURVIVES: geometry near
// rest, then full driving function. This is the exact abuse that snapped the
// M6 rod-based wishbones into their mirrored solution branch ("the car falls
// apart on the ramp" - Jozz, 2026-07-06). Two passes with increasing drop.
bool RunM7LandingIntegrityProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m7 landing integrity probe:\n" );

	bool ok = true;

	const float dropHeights[2] = { 2.0f, 3.5f };
	for ( int pass = 0; pass < 2; ++pass )
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );

		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f + dropHeights[pass];
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		// Launch forward at road speed, every body together.
		{
			b3Vec3 launchVelocity = { 14.0f, 0.0f, 0.0f };
			b3BodyId bodies[22];
			int bodyCount = 0;
			bodies[bodyCount++] = vehicle.chassisId;
			bodies[bodyCount++] = vehicle.rackId;
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				bodies[bodyCount++] = vehicle.corners[corner].wheelId;
				bodies[bodyCount++] = vehicle.corners[corner].knuckleId;
				bodies[bodyCount++] = vehicle.corners[corner].upperArmId;
				bodies[bodyCount++] = vehicle.corners[corner].lowerArmId;
				bodies[bodyCount++] = vehicle.corners[corner].trailingArmId;
			}
			for ( int i = 0; i < bodyCount; ++i )
			{
				if ( B3_IS_NON_NULL( bodies[i] ) )
				{
					b3Body_SetLinearVelocity( bodies[i], launchVelocity );
				}
			}
		}

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;

		// Flight + landing + recovery (the landing itself is allowed to be
		// violent; what matters is the state afterwards).
		JozzVehicleM6DriveInput input = {};
		for ( int i = 0; i < 240; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		ok &= CheckTrue( "m7 landing state is finite", IsM6VehicleStateValid( vehicle ) );
		ok &= CheckTrue( "m7 lands upright", M6ChassisUpDotWorldUp( vehicle ) > 0.90f );

		float worstCamberDeg = 0.0f;
		float worstRearSteerDeg = 0.0f;
		float worstTravel = 0.0f;
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			JozzVehicleM6WheelTelemetry telemetry = GetJozzVehicleM6WheelTelemetry( vehicle, corner );
			worstCamberDeg = std::fmax( worstCamberDeg, std::fabs( 180.0f / B3_PI * telemetry.camberAngle ) );
			worstTravel = std::fmax( worstTravel, std::fabs( telemetry.suspensionTravel ) );
			if ( corner == JOZZ_M6_REAR_LEFT || corner == JOZZ_M6_REAR_RIGHT )
			{
				worstRearSteerDeg = std::fmax( worstRearSteerDeg, std::fabs( 180.0f / B3_PI * telemetry.steeringAngle ) );
			}
		}
		std::printf( "m7 landing (%.1f m drop): worst camber %.1f deg, rear steer %.1f deg, |travel| %.3f m\n",
					 dropHeights[pass], worstCamberDeg, worstRearSteerDeg, worstTravel );
		ok &= CheckTrue( "m7 landing keeps camber sane (no folded arms)", worstCamberDeg < 12.0f );
		ok &= CheckTrue( "m7 landing keeps rear toe (no twisted knuckles)", worstRearSteerDeg < 6.0f );
		ok &= CheckTrue( "m7 landing keeps travel inside the stops",
						 worstTravel < b3MaxFloat( config.compressionTravel, config.reboundTravel ) + 0.08f );

		// Full function after the abuse: it must still drive and steer.
		b3Pos beforeDrive = b3Body_GetPosition( vehicle.chassisId );
		input.drive = 1.0f;
		for ( int i = 0; i < 240; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		b3Pos afterDrive = b3Body_GetPosition( vehicle.chassisId );
		float driveDx = (float)( afterDrive.x - beforeDrive.x );
		float driveDz = (float)( afterDrive.z - beforeDrive.z );
		std::printf( "m7 post-landing drive dx %.2f m, dz %.2f m\n", driveDx, driveDz );
		ok &= CheckTrue( "m7 drives on after landing", driveDx > 3.0f );
		ok &= CheckTrue( "m7 tracks straight after landing", std::fabs( driveDz ) < 0.6f * std::fabs( driveDx ) );

		float headingBefore = M6ChassisHeading( vehicle );
		input.steer = 1.0f;
		for ( int i = 0; i < 180; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float headingDelta = M6ChassisHeading( vehicle ) - headingBefore;
		while ( headingDelta > B3_PI )
		{
			headingDelta -= 2.0f * B3_PI;
		}
		while ( headingDelta < -B3_PI )
		{
			headingDelta += 2.0f * B3_PI;
		}
		ok &= CheckTrue( "m7 steers on after landing", headingDelta < -0.05f );
		ok &= CheckTrue( "m7 post-landing state is finite", IsM6VehicleStateValid( vehicle ) );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	return ok;
}

// Torque-drive probe (M7): the drive is torque-vs-grip now, not a speed
// servo. Three claims: default torque launches WITHOUT wheelspin; oversized
// torque DOES light the tires up; and the wheel speed converges to the rev
// limit instead of running away.
bool RunM7TorqueDriveProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m7 torque drive probe:\n" );

	bool ok = true;

	const float timeStep = 1.0f / 60.0f;
	const int subStepCount = 4;

	float torqueCases[2] = { 0.0f, 2600.0f }; // 0 = default config torque
	float maxSlipRatio[2] = {};

	for ( int pass = 0; pass < 2; ++pass )
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.9f );

		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		if ( torqueCases[pass] > 0.0f )
		{
			config.maxDriveTorque = torqueCases[pass];
		}
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		JozzVehicleM6DriveInput input = {};
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		// Standing start, full throttle: measure the worst driven-wheel slip
		// ratio in the first second. The first quarter second is excluded -
		// while the chassis speed is still near zero ANY wheel rotation reads
		// as slip ~1 even though the tire is rolling, which is a divide
		// artifact, not wheelspin. Sustained slip with the wheel surface
		// clearly moving is the real signal.
		input.drive = 1.0f;
		for ( int i = 0; i < 60; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );

			if ( i < 15 )
			{
				continue;
			}

			float speed = std::fabs( GetJozzVehicleM6ForwardSpeed( vehicle ) );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				JozzVehicleM6WheelTelemetry telemetry = GetJozzVehicleM6WheelTelemetry( vehicle, corner );
				float wheelSurface = std::fabs( telemetry.spinSpeed ) * config.wheelEnvelope.radius;
				if ( wheelSurface > 3.0f )
				{
					float slip = ( wheelSurface - speed ) / wheelSurface;
					maxSlipRatio[pass] = std::fmax( maxSlipRatio[pass], slip );
				}
			}
		}

		std::printf( "m7 torque %.0f N*m: max slip ratio %.2f, speed after 1 s %.2f m/s\n",
					 config.maxDriveTorque, maxSlipRatio[pass], GetJozzVehicleM6ForwardSpeed( vehicle ) );

		if ( pass == 0 )
		{
			ok &= CheckTrue( "m7 default torque launches without wheelspin", maxSlipRatio[0] < 0.4f );
			ok &= CheckTrue( "m7 default torque still accelerates", GetJozzVehicleM6ForwardSpeed( vehicle ) > 1.5f );

			// Long pull: wheel speed must converge to the rev limit and the
			// chassis speed must stay consistent with it (no runaway servo).
			for ( int i = 0; i < 540; ++i )
			{
				UpdateJozzVehicleM6Drive( vehicle, input );
				b3World_Step( worldId, timeStep, subStepCount );
			}
			float topSpeed = GetJozzVehicleM6ForwardSpeed( vehicle );
			float wheelSpin = std::fabs( GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_REAR_LEFT ).spinSpeed );
			std::printf( "m7 long pull: speed %.2f m/s, wheel spin %.1f rad/s (rev limit %.1f)\n", topSpeed, wheelSpin,
						 config.maxDriveSpeed );
			ok &= CheckTrue( "m7 wheel spin respects the rev limit", wheelSpin < 1.05f * config.maxDriveSpeed );
			ok &= CheckTrue( "m7 top speed consistent with the rev limit",
							 topSpeed < 1.05f * config.maxDriveSpeed * config.wheelEnvelope.radius );
			ok &= CheckTrue( "m7 actually goes fast on the long pull", topSpeed > 10.0f );
		}

		ok &= CheckTrue( "m7 torque probe state is finite", IsM6VehicleStateValid( vehicle ) );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	ok &= CheckTrue( "m7 oversized torque lights the tires up (wheelspin exists)", maxSlipRatio[1] > 0.5f );
	ok &= CheckTrue( "m7 wheelspin scales with torque", maxSlipRatio[1] > maxSlipRatio[0] + 0.2f );

	return ok;
}

// Trailing-arm smoke (M7): rear axle on Jozz's one-sided trailing arm, with
// geometry imported from the sidecar contract when the asset is reachable
// (the first contract-to-physics import) and the built-in fallback otherwise.
bool RunM7TrailingArmSmoke( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m7 trailing arm smoke: begin\n" );

	JozzVehicleM7TrailingArmImport import = LoadJozzVehicleM7TrailingArmGeometry( "one_sided_wheel_mount.asset.json" );
	std::printf( "m7 %s\n", import.status.c_str() );

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
	config.rearRigType = JOZZ_M6_RIG_TRAILING_ARM;
	config.trailingArm = import.geometry;
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

	const float timeStep = 1.0f / 60.0f;
	const int subStepCount = 4;

	bool ok = CheckTrue( "m7 trailing vehicle created", vehicle.valid );

	JozzVehicleM6DriveInput input = {};
	for ( int i = 0; i < 150; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	// Per-corner travel, not chassis sag: the imported damper geometry has its
	// own motion ratio (the contract decides where the coilover sits on the
	// arm), so the rear legitimately rides lower than the wishbone front. The
	// real requirement is that no corner is near bottoming out at rest.
	float worstTravel = 0.0f;
	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		worstTravel = std::fmax( worstTravel, GetJozzVehicleM6WheelTelemetry( vehicle, corner ).suspensionTravel );
	}
	b3Pos settled = b3Body_GetPosition( vehicle.chassisId );
	std::printf( "m7 trailing settle sag %.3f m, worst corner travel %.3f m of %.3f m\n",
				 spawnHeight - (float)settled.y, worstTravel, config.compressionTravel );
	ok &= CheckTrue( "m7 trailing settle state is finite", IsM6VehicleStateValid( vehicle ) );
	ok &= CheckTrue( "m7 trailing corners stay clear of the bump stops at rest",
					 worstTravel < 0.9f * config.compressionTravel );
	ok &= CheckTrue( "m7 trailing settles upright", M6ChassisUpDotWorldUp( vehicle ) > 0.95f );

	b3Pos beforeDrive = b3Body_GetPosition( vehicle.chassisId );
	input.drive = 1.0f;
	for ( int i = 0; i < 300; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}
	b3Pos afterDrive = b3Body_GetPosition( vehicle.chassisId );
	float driveDx = (float)( afterDrive.x - beforeDrive.x );
	float driveDz = (float)( afterDrive.z - beforeDrive.z );
	std::printf( "m7 trailing drive dx %.2f m, dz %.2f m\n", driveDx, driveDz );
	ok &= CheckTrue( "m7 trailing drives forward", driveDx > 4.0f );
	ok &= CheckTrue( "m7 trailing tracks straight", std::fabs( driveDz ) < 0.5f * std::fabs( driveDx ) );
	ok &= CheckTrue( "m7 trailing stays upright while driving", M6ChassisUpDotWorldUp( vehicle ) > 0.85f );

	input.drive = 0.0f;
	input.brake = true;
	for ( int i = 0; i < 300; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}
	ok &= CheckTrue( "m7 trailing brake stops the vehicle", b3Length( b3Body_GetLinearVelocity( vehicle.chassisId ) ) < 0.8f );
	ok &= CheckTrue( "m7 trailing final state is finite", IsM6VehicleStateValid( vehicle ) );

	DestroyJozzVehicleM6( &vehicle );
	b3DestroyWorld( worldId );

	std::printf( "m7 trailing arm smoke: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// P2 regression: rackTravel is derived from steering geometry
// (ComputeJozzVehicleM6RackStroke) and must be recomputed whenever that
// geometry changes, or the rack limit silently goes stale (the linkage can
// then overshoot its own tie-rod dead point before the joint limit stops it -
// see AUDIT_PHYSICS_STEERING_2026_07_08_PL.md). This does not assert on the
// tripwire's stdout line directly (it is a printf, not a bool, by design -
// see the "NIE assert" note in the plan); instead it proves recomputing
// actually changes the number, which is the thing ApplyPendingStructuralSetup
// and LoadPresetByName now do before every CreateVehicle() call.
bool RunP2RackTravelRegressionProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p2 rackTravel regression probe:\n" );

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
	float oldRackTravel = config.rackTravel;

	config.wishbone.steeringArmBack = 0.22f;
	float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
	float newRackTravel = ComputeJozzVehicleM6RackStroke( config.wishbone, 2.0f * config.axleHalfSpacing,
														   config.trackHalfWidth, config.rackHalfWidth, maxAngle );
	config.rackTravel = newRackTravel;

	std::printf( "p2 rackTravel steeringArmBack 0.17 -> 0.22: %.4f m -> %.4f m\n", oldRackTravel, newRackTravel );

	bool ok = CheckTrue( "p2 recomputing rackTravel after a geometry change actually changes it",
						 std::fabs( newRackTravel - oldRackTravel ) > 1.0e-4f );

	// Build the vehicle with the freshly recomputed value - CreateJozzVehicleM6's
	// tripwire should stay silent here (watch the stdout above this line for
	// "stale rackTravel"; per plan §2 this is a printf, not an assert).
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
	ok &= CheckTrue( "p2 vehicle builds with the recomputed rackTravel", vehicle.valid );

	DestroyJozzVehicleM6( &vehicle );
	b3DestroyWorld( worldId );

	std::printf( "p2 rackTravel regression probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// Opt-in arcade centering assist (config.rackCenteringHertz): with it OFF
// (default 0), a wheel knocked off-center on a STATIONARY car stays there
// (realistic - no caster force at rest). With it ON, a weak spring pulls the
// rack toward center even at a standstill. This probe asserts exactly that
// contrast, so the honest default and the assist are both pinned by a test.
bool RunP4CenteringAssistProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p4 centering assist probe:\n" );
	bool ok = true;

	auto kickAtRestAndSettle = []( const JozzVehicleM6Config& config ) {
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;
		JozzVehicleM6DriveInput input = {};
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		b3BodyId flWheel = vehicle.corners[JOZZ_M6_FRONT_LEFT].wheelId;
		b3Vec3 v = b3Body_GetLinearVelocity( flWheel );
		b3Body_SetLinearVelocity( flWheel, { v.x, v.y, v.z + 14.0f } );
		// Stationary the whole time - never driven.
		for ( int i = 0; i < 400; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float angleDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
		return angleDeg;
	};

	JozzVehicleM6Config realistic =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
	float restRealistic = kickAtRestAndSettle( realistic );

	// Measured (2026-07-08): centering a STATIONARY loaded tyre means scrubbing
	// it against the ground (the parking-torque wall), so a weak spring can't do
	// it - hz=2 barely moved the wheel, hz=6 got halfway, hz>=10 fully centers.
	// The assist therefore only bites from ~8 Hz up; the UI range and this test
	// value reflect that.
	JozzVehicleM6Config assisted = realistic;
	assisted.rackCenteringHertz = 12.0f;
	float restAssisted = kickAtRestAndSettle( assisted );

	std::printf( "p4 kicked wheel at rest: realistic (hz=0) %.1f deg, assist (hz=12) %.1f deg\n", restRealistic,
				 restAssisted );
	ok &= CheckTrue( "p4 realistic default does NOT self-center at rest (no caster force)",
					 std::fabs( restRealistic ) > 8.0f );
	ok &= CheckTrue( "p4 opt-in assist DOES self-center at rest", std::fabs( restAssisted ) < 5.0f );

	// Liveness: assist ON must not destabilise normal driving (it is opt-in but
	// must never blow up or send the car sideways).
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = assisted.restDrop + assisted.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, assisted, { 0.0f, spawnHeight, 0.0f } );
		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;
		JozzVehicleM6DriveInput input = {};
		input.drive = 1.0f;
		b3Pos before = b3Body_GetPosition( vehicle.chassisId );
		for ( int i = 0; i < 300; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		b3Pos after = b3Body_GetPosition( vehicle.chassisId );
		float dx = (float)( after.x - before.x );
		float dz = (float)( after.z - before.z );
		std::printf( "p4 assist-on driving: dx %.1f m, dz %.1f m\n", dx, dz );
		ok &= CheckTrue( "p4 assist-on still drives forward and roughly straight",
						 dx > 4.0f && std::fabs( dz ) < 0.6f * std::fabs( dx ) );
		ok &= CheckTrue( "p4 assist-on state is finite", IsM6VehicleStateValid( vehicle ) );
		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	std::printf( "p4 centering assist probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// P1: the tie-rod steering linkage has its own over-center "dead point" - a
// steering angle past which the linkage has no more mechanical advantage
// (rackTravel stops increasing with angle - see ComputeJozzVehicleM6RackStroke).
// The knuckle ball-joint's twist limit used to be a flat hardcoded +-70 deg,
// well past that dead point; this probe (a) locates the dead point for the
// default geometry and asserts the config-derived fence stays clear of it,
// and (c) proves the (now tighter) fence does not cut normal full-lock
// steering. Both hold and are asserted below.
//
// (b) fires a lateral impulse at the front-left wheel, THEN drives forward.
// History worth keeping straight: earlier this section measured "does the
// wheel return to straight while hands-off and STATIONARY" and reported it
// "did NOT return" - which got written up as a scary unresolved branch-lock
// (old TECH_DEBT #9). A deeper root-cause test (2026-07-08, decisive
// strut-vs-wishbone comparison + rack-translation readout) DISPROVED that:
// when "jammed" the rack is simply pinned at its travel LIMIT (-rackTravel)
// held by friction, NOT centered-with-offset-wheel, and the wheel moves
// freely when commanded. The reason it doesn't self-straighten at rest is
// that a back-drivable steering has NO centering force at a standstill -
// caster trail only centers once the tyre is rolling. Drive forward and the
// wheel snaps back (measured: -29 deg at rest -> 1.4 deg at 12.7 m/s). That
// is correct physics, the same reason M7 removed the fake software
// self-align. So (b) now asserts the CORRECT thing: the wheel stays within
// the fence during the hit, and self-centers once ROLLING. (Opt-in arcade
// centering-at-rest lives behind config.rackCenteringHertz, tested separately
// in RunP4CenteringAssistProbe.)
bool RunP1SteeringFenceProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p1 steering fence probe:\n" );
	bool ok = true;

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

	// (a) Dead point for the default geometry (shared helper - see P5, where
	// the same computation also drives a LIVE clamp on the max-steer slider).
	float wheelbase = 2.0f * config.axleHalfSpacing;
	float deadPointDeg =
		ComputeJozzVehicleM6SteeringDeadPointDeg( config.wishbone, wheelbase, config.trackHalfWidth, config.rackHalfWidth );
	std::printf( "p1 tie-rod dead point (default geometry): %.1f deg\n", deadPointDeg );

	float frontFenceDeg = config.maxSteeringAngleDegrees + 10.0f;
	std::printf( "p1 front twist fence: %.1f deg (maxSteer %.0f + 10 margin)\n", frontFenceDeg,
				 config.maxSteeringAngleDegrees );
	ok &= CheckTrue( "p1 front fence stays clear of the tie-rod dead point", frontFenceDeg <= deadPointDeg - 3.0f );

	// (b) Impact probe: three lateral-velocity impulses on the front-left wheel,
	// then DRIVE FORWARD. Two claims: the fence contains the wheel during the
	// hit, and once rolling the caster trail self-centers it. (Centering at
	// rest is NOT expected - that is the arcade assist, tested elsewhere.)
	const float impactSpeeds[3] = { 6.0f, 10.0f, 14.0f };
	for ( float impactSpeed : impactSpeeds )
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );

		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
		ok &= CheckTrue( "p1 impact vehicle created", vehicle.valid );

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;

		JozzVehicleM6DriveInput input = {};
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		b3BodyId flWheel = vehicle.corners[JOZZ_M6_FRONT_LEFT].wheelId;
		b3Vec3 before = b3Body_GetLinearVelocity( flWheel );
		b3Vec3 impactVelocity = { before.x, before.y, before.z + impactSpeed };
		b3Body_SetLinearVelocity( flWheel, impactVelocity );

		// Hands-off settle after the hit (wheel is free to be knocked off-center).
		float worstAngleDeg = 0.0f;
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
			float flDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
			float frDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT ).steeringAngle;
			worstAngleDeg = b3MaxFloat( worstAngleDeg, b3MaxFloat( std::fabs( flDeg ), std::fabs( frDeg ) ) );
		}
		float atRestDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;

		// Now drive forward: caster trail must self-center the released wheel.
		input.drive = 1.0f;
		for ( int i = 0; i < 300; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float speed = GetJozzVehicleM6ForwardSpeed( vehicle );
		float rollingDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;

		std::printf( "p1 impact V=%.0f: at rest %.1f deg -> driving (%.1f m/s) %.1f deg (worst during hit %.1f)\n",
					 impactSpeed, atRestDeg, speed, rollingDeg, worstAngleDeg );

		char label[64];
		std::snprintf( label, sizeof( label ), "p1 impact V=%.0f stays within fence + 2 deg", impactSpeed );
		ok &= CheckTrue( label, worstAngleDeg < frontFenceDeg + 2.0f );
		std::snprintf( label, sizeof( label ), "p1 impact V=%.0f self-centers once rolling", impactSpeed );
		ok &= CheckTrue( label, std::fabs( rollingDeg ) < 8.0f );
		ok &= CheckTrue( "p1 impact state is finite", IsM6VehicleStateValid( vehicle ) );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	// (c) Non-interference: existing full-lock steering must still reach
	// close to the commanded max angle - the fence must not cut normal
	// steering. Front-left is the Ackermann inner wheel on a left lock, so it
	// steers MORE than the commanded angle - the harder case for "does the
	// fence get in the way".
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );

		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;

		JozzVehicleM6DriveInput input = {};
		input.steer = 1.0f;
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float flDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
		std::printf( "p1 full lock (steer=1.0, 120 steps): FL %.1f deg (commanded limit %.0f deg)\n", flDeg,
					 config.maxSteeringAngleDegrees );
		ok &= CheckTrue( "p1 fence does not cut full-lock steering", flDeg >= 30.0f );
		ok &= CheckTrue( "p1 full lock state is finite", IsM6VehicleStateValid( vehicle ) );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	std::printf( "p1 steering fence probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// P5: max-steer slider (structural, live-clamped against the dead point) and
// static toe (turnbuckle-style link length shift, front + rear).
bool RunP5SteeringSetupProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p5 steering setup probe:\n" );
	bool ok = true;

	// (a) Max steer 40 deg at the default ackermannFraction (0.6, dead point
	// 59.5 deg per P1) is comfortably safe (fence 50 <= 56.5) - full lock
	// should reach ~40 deg and the P1 fence math (asserted in
	// RunP1SteeringFenceProbe) must still track it.
	{
		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		config.maxSteeringAngleDegrees = 40.0f;
		float wheelbase = 2.0f * config.axleHalfSpacing;
		config.rackTravel = ComputeJozzVehicleM6RackStroke( config.wishbone, wheelbase, config.trackHalfWidth,
															config.rackHalfWidth, 40.0f * B3_PI / 180.0f );

		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;
		JozzVehicleM6DriveInput input = {};
		input.steer = 1.0f;
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float flDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
		std::printf( "  maxSteer=40: full lock FL %.1f deg\n", flDeg );
		ok &= CheckTrue( "p5 maxSteer=40 full lock reaches close to 40 deg", flDeg >= 36.0f );
		ok &= CheckTrue( "p5 maxSteer=40 state is finite", IsM6VehicleStateValid( vehicle ) );
		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	// (b) The live-clamp math (rig_lab's ApplyPendingStructuralSetup) uses this
	// exact shared helper - confirm the dangerous combination the audit flagged
	// (max steer 45 + full Ackermann) really is unsafe before the clamp, and
	// that the clamp's own formula (deadPoint - 13) brings it back in bounds.
	{
		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		config.wishbone.ackermannFraction = 1.0f;
		float wheelbase = 2.0f * config.axleHalfSpacing;
		float deadPointDeg =
			ComputeJozzVehicleM6SteeringDeadPointDeg( config.wishbone, wheelbase, config.trackHalfWidth, config.rackHalfWidth );
		float safeMaxSteer = deadPointDeg - 13.0f;
		std::printf( "  ackermannFraction=1.0: deadPoint=%.1f deg, clamp would cap max-steer at %.1f deg\n",
					 deadPointDeg, safeMaxSteer );
		ok &= CheckTrue( "p5 uncapped 45 deg would violate the fence at full Ackermann",
						 ( 45.0f + 10.0f ) > deadPointDeg - 3.0f );
		ok &= CheckTrue( "p5 the clamp formula itself stays inside the fence margin",
						 ( safeMaxSteer + 10.0f ) <= deadPointDeg - 3.0f );
		ok &= CheckTrue( "p5 clamp still leaves a usable steering angle", safeMaxSteer > 15.0f );
	}

	// (c) Static toe: build front toe=+1 deg (rear 0), rear toe=+1 deg (front
	// 0), read steeringAngle at rest. Toe-in convention (positive) must give
	// opposite-signed L/R angles - sign verified here, not assumed (per the
	// plan's own warning that this is the likeliest bug). Note toeDeg=0 is
	// NOT exactly 0/0 at rest - the Ackermann trapezoid itself gives a small
	// permanent resting asymmetry (measured ~-0.39/+0.39 deg L/R with the
	// default geometry) that toe then adds on top of; the assertions below
	// check the TOE contribution (opposite signs, right magnitude), not an
	// absolute zero baseline.
	//
	// Measured HANDS-OFF first (2026-07-08): results were inconsistent
	// (0.04/0.78 deg instead of a clean +-1) - the toe-induced tie-rod
	// asymmetry is worth only a few mm of rack shift, easily swallowed by the
	// 250 N static friction hold (P4), so the rack "sticks" wherever settling
	// left it rather than reaching the true equilibrium. HANDS-ON with a
	// near-zero commanded angle instead pins the rack to translation=0 with
	// the full servo, giving a clean, friction-independent reading - which is
	// also more representative of how toe actually shows up in play (a static
	// alignment spec, not a parking-brake artifact). Rear corners have no
	// rack/servo at all (fixed toe-link to chassis), so they're unaffected by
	// this choice either way - only the front pass's input matters here.
	for ( int pass = 0; pass < 2; ++pass )
	{
		bool frontPass = pass == 0;
		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		if ( frontPass )
		{
			config.frontToeDeg = 1.0f;
		}
		else
		{
			config.rearToeDeg = 1.0f;
		}
		config.steerInputDeadzone = 0.0f;

		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;
		JozzVehicleM6DriveInput input = {};
		input.steer = 0.0001f; // hair above the (now zero) deadzone, commands ~0 deg
		for ( int i = 0; i < 180; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		int leftCorner = frontPass ? JOZZ_M6_FRONT_LEFT : JOZZ_M6_REAR_LEFT;
		int rightCorner = frontPass ? JOZZ_M6_FRONT_RIGHT : JOZZ_M6_REAR_RIGHT;
		float leftDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, leftCorner ).steeringAngle;
		float rightDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, rightCorner ).steeringAngle;
		std::printf( "  %s toe=+1 deg: left %.2f deg, right %.2f deg\n", frontPass ? "front" : "rear", leftDeg,
					 rightDeg );

		char label[80];
		std::snprintf( label, sizeof( label ), "p5 %s toe=+1 gives opposite-signed L/R angles (toe-in)",
						frontPass ? "front" : "rear" );
		ok &= CheckTrue( label, leftDeg * rightDeg < 0.0f );
		std::snprintf( label, sizeof( label ), "p5 %s toe=+1 magnitude is in the right ballpark (0.3-3 deg)",
						frontPass ? "front" : "rear" );
		ok &= CheckTrue( label, std::fabs( leftDeg ) > 0.3f && std::fabs( leftDeg ) < 3.0f );
		ok &= CheckTrue( "p5 toe probe state is finite", IsM6VehicleStateValid( vehicle ) );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	std::printf( "p5 steering setup probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// P6a: mass & limit sanity. Prints every body mass and the ratios the solver
// actually cares about, and settles two audit questions with numbers:
//  - S3 "wheel mass probably doubled by the split envelope": code inspection
//    shows the sidewall cylinder is created with density 0 (the sphere carries
//    all mass/inertia) - this probe pins that with a measurement so it can
//    never silently regress.
//  - S2 "HingeSwingLimit saturates at default travel": prints the saturation
//    ratio so the number is visible on every run (the UI warning added in P6
//    uses the same condition).
bool RunP6MassAndLimitSanityProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p6 mass & limit sanity probe:\n" );
	bool ok = true;

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
	ok &= CheckTrue( "p6 sanity vehicle created", vehicle.valid );

	float chassisMass = b3Body_GetMass( vehicle.chassisId );
	float wheelMass = b3Body_GetMass( vehicle.corners[JOZZ_M6_FRONT_LEFT].wheelId );
	float knuckleMass = b3Body_GetMass( vehicle.corners[JOZZ_M6_FRONT_LEFT].knuckleId );
	float armMass = b3Body_GetMass( vehicle.corners[JOZZ_M6_FRONT_LEFT].upperArmId );
	float rackMass = B3_IS_NON_NULL( vehicle.rackId ) ? b3Body_GetMass( vehicle.rackId ) : 0.0f;

	// Expected wheel mass if ONLY the sphere carries density (the S3 guard).
	float r = config.wheelEnvelope.radius;
	float sphereMass = config.wheelDensity * ( 4.0f / 3.0f ) * B3_PI * r * r * r;

	std::printf( "  masses kg: chassis %.1f, wheel %.1f (sphere-only expected %.1f), knuckle %.1f, arm %.1f, rack %.1f\n",
				 chassisMass, wheelMass, sphereMass, knuckleMass, armMass, rackMass );
	std::printf( "  ratios: chassis/wheel %.1f, wheel/knuckle %.1f, knuckle/arm %.1f\n", chassisMass / wheelMass,
				 wheelMass / knuckleMass, knuckleMass / armMass );

	ok &= CheckTrue( "p6 split-envelope wheel mass equals the sphere alone (S3 guard holds)",
					 std::fabs( wheelMass - sphereMass ) < 0.05f * sphereMass );
	ok &= CheckTrue( "p6 all masses positive and finite",
					 chassisMass > 1.0f && wheelMass > 1.0f && knuckleMass > 0.5f && armMass > 0.1f &&
						 std::isfinite( chassisMass + wheelMass + knuckleMass + armMass + rackMass ) );

	// S2: swing-limit saturation ratio at the default config. sine >= 0.95 is
	// the clamp ceiling - the hinge guard has no margin left beyond it.
	float travel = b3MaxFloat( config.compressionTravel, config.reboundTravel );
	float saturation = 1.25f * travel / config.wishbone.lowerArmLength;
	std::printf( "  hinge swing saturation: 1.25*travel/armLength = %.2f (>= 0.95 means the guard sits at its ceiling)\n",
				 saturation );

	DestroyJozzVehicleM6( &vehicle );
	b3DestroyWorld( worldId );

	// Sanitize regression: a deliberately broken "hand-edited preset" must
	// come out solver-safe, and the default config must pass through UNTOUCHED
	// (a sanitizer that rewrites healthy values would corrupt every load).
	{
		JozzVehicleM6Config clean =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		ok &= CheckTrue( "p6 sanitize leaves the default config untouched", SanitizeJozzVehicleM6Config( &clean ) == false );

		JozzVehicleM6Config broken = clean;
		broken.wishbone.lowerArmLength = 0.0f;                       // zero-length arm
		broken.wheelDensity = -50.0f;                                // negative mass
		broken.maxSteeringAngleDegrees = 45.0f;                      // combined with...
		broken.wishbone.ackermannFraction = 1.0f;                    // ...the P5 fence-bypass hole
		broken.compressionTravel = std::numeric_limits<float>::quiet_NaN(); // hand-edit gone wrong
		ok &= CheckTrue( "p6 sanitize flags the broken config", SanitizeJozzVehicleM6Config( &broken ) );
		float wheelbase = 2.0f * broken.axleHalfSpacing;
		float deadPointDeg = ComputeJozzVehicleM6SteeringDeadPointDeg( broken.wishbone, wheelbase,
																	   broken.trackHalfWidth, broken.rackHalfWidth );
		std::printf( "  sanitized broken preset: arm %.2f m, density %.0f, maxSteer %.1f deg (deadPoint %.1f), travel %.2f m\n",
					 broken.wishbone.lowerArmLength, broken.wheelDensity, broken.maxSteeringAngleDegrees, deadPointDeg,
					 broken.compressionTravel );
		ok &= CheckTrue( "p6 sanitized max steer respects the dead-point fence",
						 broken.maxSteeringAngleDegrees + 10.0f <= deadPointDeg - 3.0f + 0.01f );

		// And the sanitized config must actually BUILD and survive a settle.
		b3WorldDef brokenWorldDef = b3DefaultWorldDef();
		b3WorldId brokenWorldId = b3CreateWorld( &brokenWorldDef );
		b3BodyId brokenGroundId = CreateM6SmokeGround( brokenWorldId, 0.8f );
		broken.rackTravel = ComputeJozzVehicleM6RackStroke( broken.wishbone, wheelbase, broken.trackHalfWidth,
															broken.rackHalfWidth,
															broken.maxSteeringAngleDegrees * B3_PI / 180.0f );
		JozzVehicleM6 sanitizedVehicle =
			CreateJozzVehicleM6( brokenWorldId, brokenGroundId, broken,
								 { 0.0f, broken.restDrop + broken.wheelEnvelope.radius + 0.05f, 0.0f } );
		for ( int i = 0; i < 120; ++i )
		{
			JozzVehicleM6DriveInput input = {};
			UpdateJozzVehicleM6Drive( sanitizedVehicle, input );
			b3World_Step( brokenWorldId, 1.0f / 60.0f, 4 );
		}
		ok &= CheckTrue( "p6 sanitized config builds and settles finite", IsM6VehicleStateValid( sanitizedVehicle ) );
		DestroyJozzVehicleM6( &sanitizedVehicle );
		b3DestroyWorld( brokenWorldId );
	}

	std::printf( "p6 mass & limit sanity probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// P6b: extreme-config stress matrix. Each variant is a deliberately abusive
// but slider-reachable setup, run through a brutal script (full throttle,
// full-lock steer at speed, brake, settle). The point is NOT that the car
// stays pretty - flipping under 2000 N*m AWD at full lock is legal physics -
// but that the RIG never lies afterwards: state stays finite, nothing
// teleports, and after the abuse the suspension geometry is still intact
// (camber/rear-toe within the same "not snapped onto a wrong branch"
// detectors the M7 landing probe uses) and the wheels are not jittering at
// rest (unstable constraints show up as standstill vibration).
bool RunP6StressMatrixProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p6 stress matrix probe:\n" );
	bool ok = true;

	struct Variant
	{
		const char* name;
		void ( *mutate )( JozzVehicleM6Config& );
		float dropHeight; // extra spawn height, 0 = start on the ground
	};

	Variant variants[] = {
		{ "torque2000-awd-grip", []( JozzVehicleM6Config& c ) {
			 c.maxDriveTorque = 2000.0f;
			 c.allWheelDrive = true;
			 c.wheelFriction = 2.5f;
		 }, 0.0f },
		{ "light-unsprung", []( JozzVehicleM6Config& c ) {
			 c.wheelDensity = 20.0f;
			 c.knuckleMass = 10.0f;
			 c.armMass = 2.0f;
		 }, 0.0f },
		{ "inverted-mass-ratio", []( JozzVehicleM6Config& c ) {
			 c.chassisDensity = 50.0f;
			 c.wheelDensity = 300.0f;
		 }, 0.0f },
		{ "max-preload-max-travel", []( JozzVehicleM6Config& c ) {
			 c.suspensionPreloadFront = 0.20f;
			 c.suspensionPreloadRear = 0.20f;
			 c.compressionTravel = 0.70f;
			 c.reboundTravel = 0.60f;
		 }, 0.0f },
		{ "stiffest-drop2m", []( JozzVehicleM6Config& c ) {
			 c.suspensionHertz = 12.0f;
			 c.suspensionDampingRatio = 2.0f;
			 c.frontSuspensionScale = 2.0f;
			 c.rearSuspensionScale = 2.0f;
		 }, 2.0f },
	};

	for ( const Variant& variant : variants )
	{
		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		variant.mutate( config );

		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f + variant.dropHeight;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;
		bool finiteThroughout = vehicle.valid;

		JozzVehicleM6DriveInput input = {};
		auto runPhase = [&]( int steps ) {
			for ( int i = 0; i < steps; ++i )
			{
				UpdateJozzVehicleM6Drive( vehicle, input );
				b3World_Step( worldId, timeStep, subStepCount );
			}
			finiteThroughout &= IsM6VehicleStateValid( vehicle );
		};

		runPhase( 120 ); // settle (and land, for the drop variant)
		input.drive = 1.0f;
		runPhase( 240 ); // full throttle
		input.steer = 1.0f;
		runPhase( 120 ); // full lock at speed
		float topSpeed = GetJozzVehicleM6ForwardSpeed( vehicle );
		input.drive = 0.0f;
		input.steer = 0.0f;
		input.brake = true;
		runPhase( 120 ); // brake
		input.brake = false;

		// Hands-off settle; measure standstill jitter over the last 60 steps.
		float jitterAccum = 0.0f;
		int jitterSamples = 0;
		for ( int i = 0; i < 120; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
			if ( i >= 60 )
			{
				for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
				{
					float vy = (float)b3Body_GetLinearVelocity( vehicle.corners[corner].wheelId ).y;
					jitterAccum += vy * vy;
					jitterSamples += 1;
				}
			}
		}
		finiteThroughout &= IsM6VehicleStateValid( vehicle );
		float jitterRms = std::sqrt( jitterAccum / (float)b3MaxFloat( (float)jitterSamples, 1.0f ) );

		float worstCamberDeg = 0.0f;
		float worstRearSteerDeg = 0.0f;
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			JozzVehicleM6WheelTelemetry telemetry = GetJozzVehicleM6WheelTelemetry( vehicle, corner );
			worstCamberDeg = b3MaxFloat( worstCamberDeg, std::fabs( 180.0f / B3_PI * telemetry.camberAngle ) );
			if ( corner == JOZZ_M6_REAR_LEFT || corner == JOZZ_M6_REAR_RIGHT )
			{
				worstRearSteerDeg =
					b3MaxFloat( worstRearSteerDeg, std::fabs( 180.0f / B3_PI * telemetry.steeringAngle ) );
			}
		}
		b3Pos finalPos = b3Body_GetPosition( vehicle.chassisId );
		float posMagnitude = std::sqrt( (float)( finalPos.x * finalPos.x + finalPos.y * finalPos.y + finalPos.z * finalPos.z ) );
		float uprightDot = M6ChassisUpDotWorldUp( vehicle );

		std::printf( "  %-24s: top %.1f m/s, camber %.1f deg, rear steer %.1f deg, jitter %.3f m/s, |pos| %.0f m, upright %.2f\n",
					 variant.name, topSpeed, worstCamberDeg, worstRearSteerDeg, jitterRms, posMagnitude, uprightDot );

		char label[80];
		std::snprintf( label, sizeof( label ), "p6 %s stays finite", variant.name );
		ok &= CheckTrue( label, finiteThroughout );
		std::snprintf( label, sizeof( label ), "p6 %s does not teleport", variant.name );
		ok &= CheckTrue( label, posMagnitude < 500.0f );
		std::snprintf( label, sizeof( label ), "p6 %s rig geometry survives (camber/rear toe)", variant.name );
		ok &= CheckTrue( label, worstCamberDeg < 15.0f && worstRearSteerDeg < 8.0f );
		std::snprintf( label, sizeof( label ), "p6 %s no standstill jitter", variant.name );
		ok &= CheckTrue( label, jitterRms < 0.5f );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	std::printf( "p6 stress matrix probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

struct P3SettleResult
{
	float chassisY;
	float flTravel;
	float rlTravel;
};

P3SettleResult SettleAndMeasureP3( const JozzVehicleM6Config& config )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

	const float timeStep = 1.0f / 60.0f;
	const int subStepCount = 4;
	JozzVehicleM6DriveInput input = {};
	for ( int i = 0; i < 300; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	P3SettleResult result;
	result.chassisY = (float)b3Body_GetPosition( vehicle.chassisId ).y;
	result.flTravel = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).suspensionTravel;
	result.rlTravel = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_REAR_LEFT ).suspensionTravel;

	DestroyJozzVehicleM6( &vehicle );
	b3DestroyWorld( worldId );
	return result;
}

// P3: suspension preload (ride height) used to be coupled to the stiffness
// scale in the wrong direction (K3 in the audit - see the suspensionPreload*
// header field comment). Pre-fix measurement (recorded in CHECKPOINTS_PL.md):
// chassis.y went 0.9204 -> 1.0682 -> 1.1194 m as frontSuspensionScale swept
// 0.5 -> 1.0 -> 2.0 m (a 0.199 m spread). Post-fix: 0.9337 -> 1.0682 -> 1.0903 m
// (0.1566 m spread) - identical at scale=1.0 (sanity check: preload*1.0 was
// always a no-op), and NOT tiny at the extremes, because a real, expected
// effect remains: static deflection under the same load is F/k, and k grows
// with scale^2 (hertz scales with scale, k with hertz^2), so a 4x stiffness
// sweep genuinely produces real ride-height change even with zero coupling in
// the code - a stiffer spring naturally sags less. That is normal spring
// behavior, not the K3 bug; K3 was specifically the EXTRA, backwards-signed
// term stacking on top of it. This probe confirms two claims post-fix: (a)
// the spread shrinks from the pre-fix number (some improvement, not zero -
// don't "fix" this further by chasing full height/stiffness decoupling,
// that reintroduces the auto-compensation complexity P3 deliberately removed
// in favor of two independent, honest dials), and (b) front/rear preload
// genuinely doesn't cross-talk - only the axle you touch moves.
bool RunP3SuspensionPreloadProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p3 suspension preload probe:\n" );
	bool ok = true;

	JozzVehicleM6Config baseConfig =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

	// (a) Ride height vs stiffness scale - should now stay close together.
	float minY = 1.0e9f, maxY = -1.0e9f;
	const float scales[3] = { 0.5f, 1.0f, 2.0f };
	for ( float scale : scales )
	{
		JozzVehicleM6Config config = baseConfig;
		config.frontSuspensionScale = scale;
		P3SettleResult r = SettleAndMeasureP3( config );
		std::printf( "  frontSuspensionScale=%.1f: chassis.y=%.4f m\n", scale, r.chassisY );
		minY = b3MinFloat( minY, r.chassisY );
		maxY = b3MaxFloat( maxY, r.chassisY );
	}
	std::printf( "p3 ride height spread across scale 0.5-2.0: %.4f m (pre-fix was 0.199 m)\n", maxY - minY );
	ok &= CheckTrue( "p3 ride height spread improves on the pre-fix number (residual is natural deflection, not K3)",
					 ( maxY - minY ) < 0.18f );

	// (b) Front/rear preload independence: bump front preload only, rear must
	// not move (within the noise of a shared chassis + ARB), front must.
	P3SettleResult baseline = SettleAndMeasureP3( baseConfig );

	JozzVehicleM6Config frontUp = baseConfig;
	frontUp.suspensionPreloadFront = 0.12f;
	P3SettleResult altered = SettleAndMeasureP3( frontUp );

	float flDelta = altered.flTravel - baseline.flTravel;
	float rlDelta = altered.rlTravel - baseline.rlTravel;
	std::printf( "p3 preloadFront 0.07->0.12 (rear unchanged): FL travel delta %.4f m, RL travel delta %.4f m\n",
				 flDelta, rlDelta );
	ok &= CheckTrue( "p3 raising front preload measurably moves the front", std::fabs( flDelta ) > 0.02f );
	ok &= CheckTrue( "p3 raising front preload leaves the rear alone", std::fabs( rlDelta ) < 0.3f * std::fabs( flDelta ) );

	std::printf( "p3 suspension preload probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// P4: hands-off rack resistance is now a Coulomb static/kinetic pair instead
// of one flat force (S1 in the audit - a flat cap made the rack stop dead
// exactly where the speed-independent caster force first dropped below it).
//
// The audit expected lowering kinetic friction (~80-120 N) to let inertia
// carry a released wheel past center ("przestrzal"). Measured finding
// (2026-07-08, validator sweep across kinetic 40-250 N, both from a static
// full-lock-release and from a moderate hands-off kick while driving): true
// overshoot (crossing to a NEGATIVE angle) never occurs anywhere in that
// range - a weak perturbation just settles back near zero without crossing,
// and a strong enough one instead snaps the linkage onto the SAME wrong
// branch documented in TECH_DEBT_PL.md #9 (the P1 finding), which does not
// recover at all. There is no friction value that produces a clean
// "swings past center, then settles" - only "settles smoothly" or "jams".
// Worse, kinetic friction below ~140 N (sharp cliff, not gradual) reactivates
// that same branch-snap under a plain 3.5 m landing shock: worst camber goes
// from ~0.6-0.8 deg to 11-12 deg and the post-landing drive goes backward/
// sideways instead of forward (see RunM7LandingIntegrityProbe). So the
// audit's suggested range is actively unsafe, not just insufficiently lively.
//
// Chosen defaults (static 200 N / kinetic 150 N) sit safely above that cliff.
// This still delivers what P4 actually needs for feel: settling MUCH closer
// to dead center after a hard lock (release-to-final error dropped from
// ~0.7-0.9 deg pre-fix to ~0.2-0.5 deg here) with no sustained oscillation,
// plus a genuinely separate, tunable static (parking-hold) dial - just not
// literal overshoot. The overshoot measurement stays below as a printed,
// NON-gating diagnostic; don't chase it further by lowering kinetic without
// re-checking the landing probe first.
bool RunP4SteeringReturnProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "p4 steering return probe:\n" );
	bool ok = true;

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

	// (a) Hard lock at speed, release, watch the return.
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;

		JozzVehicleM6DriveInput input = {};
		input.drive = 1.0f;
		for ( int i = 0; i < 260; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float speed = GetJozzVehicleM6ForwardSpeed( vehicle );

		input.steer = 1.0f;
		for ( int i = 0; i < 60; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float releaseAngleDeg =
			180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;

		input.steer = 0.0f;
		float minAngleDeg = releaseAngleDeg;
		float lastAngleDeg = releaseAngleDeg;
		float last60Min = 1.0e9f, last60Max = -1.0e9f;
		for ( int i = 0; i < 240; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
			lastAngleDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
			minAngleDeg = b3MinFloat( minAngleDeg, lastAngleDeg );
			if ( i >= 180 )
			{
				last60Min = b3MinFloat( last60Min, lastAngleDeg );
				last60Max = b3MaxFloat( last60Max, lastAngleDeg );
			}
		}
		float amplitudeDeg = last60Max - last60Min;

		std::printf( "  speed %.1f m/s, release %.2f deg, overshoot(min) %.2f deg, final %.2f deg, last-60 amp %.2f deg\n",
					 speed, releaseAngleDeg, minAngleDeg, lastAngleDeg, amplitudeDeg );
		// NOT gated - see the function comment above (unachievable in this
		// range without reactivating the TECH_DEBT #9 branch-snap).
		std::printf( "  overshoot %s (min angle %.2f deg)\n", minAngleDeg < -1.0f ? "YES" : "no", minAngleDeg );
		ok &= CheckTrue( "p4 settles near straight", std::fabs( lastAngleDeg ) < 3.0f );
		ok &= CheckTrue( "p4 no sustained oscillation (shimmy) after settling", amplitudeDeg < 1.0f );
		ok &= CheckTrue( "p4 return probe state is finite", IsM6VehicleStateValid( vehicle ) );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	// (b) Parking hold: parked, hands-off, static friction must hold the angle.
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;
		JozzVehicleM6DriveInput input = {};
		for ( int i = 0; i < 60; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float startAngleDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
		for ( int i = 0; i < 180; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		float endAngleDeg = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;

		std::printf( "  parking hold: start %.2f deg, after 180 steps %.2f deg (drift %.2f deg)\n", startAngleDeg,
					 endAngleDeg, endAngleDeg - startAngleDeg );
		ok &= CheckTrue( "p4 parking hold drift stays under 1 deg", std::fabs( endAngleDeg - startAngleDeg ) < 1.0f );

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	std::printf( "p4 steering return probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// Wheel envelope probe. Three claims to verify:
// 1) Width: hull-based envelopes (cylinder, union, split sidewall) must not
//    stick out past the visual tire, unlike the single sphere (which bulges
//    by radius - width/2, the "invisible wall" next to props).
// 2) Invisible wall: a prop parked just past the tire's side face must NOT
//    touch a split-envelope wheel, and MUST touch a plain sphere wheel.
// 3) Smoothness: the split envelope must keep the sphere's perfect terrain
//    contact, and the phased-union results stay recorded as the measured
//    negative result (it rolls WORSE than one cylinder: the contact hops
//    between layered hulls and loses the solver warm start).
bool RunM6WheelEnvelopeProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "m6 wheel envelope probe:\n" );

	bool ok = true;
	float halfWidth = 0.5f * defaults.wheelWidth;

	// Width check on a throwaway body: measure the hulls' LOCAL aabb along
	// the wheel's local Y spin axis (tight bound, no broadphase margin).
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
		b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );
		b3ShapeDef shapeDef = b3DefaultShapeDef();

		struct
		{
			const char* label;
			int mode;
		} cases[] = {
			{ "sphere       ", JOZZ_M6_ENVELOPE_SPHERE },
			{ "cylinder     ", JOZZ_M6_ENVELOPE_CYLINDER },
			{ "phased union ", JOZZ_M6_ENVELOPE_PHASED_UNION },
			{ "split        ", JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL },
		};

		for ( auto& testCase : cases )
		{
			JozzVehicleM6WheelEnvelopeDesc desc = {};
			desc.mode = testCase.mode;
			desc.cylinderSides = 32;
			desc.unionLayerCount = 4;
			desc.radius = defaults.wheelRadius;
			desc.width = defaults.wheelWidth;
			desc.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;

			b3ShapeId shapeIds[JOZZ_M6_MAX_WHEEL_SHAPES];
			int shapeCount = CreateJozzVehicleM6WheelEnvelope( bodyId, &shapeDef, &desc, shapeIds );

			// Hull shapes carry a tight local aabb; spheres are just the radius.
			float hullHalfWidth = 0.0f;
			float sphereHalfWidth = 0.0f;
			for ( int i = 0; i < shapeCount; ++i )
			{
				const b3HullData* hull = b3Shape_GetType( shapeIds[i] ) == b3_hullShape ? b3Shape_GetHull( shapeIds[i] ) : nullptr;
				if ( hull != nullptr )
				{
					float extent = b3MaxFloat( std::fabs( (float)hull->aabb.lowerBound.y ),
											   std::fabs( (float)hull->aabb.upperBound.y ) );
					hullHalfWidth = b3MaxFloat( hullHalfWidth, extent );
				}
				else
				{
					sphereHalfWidth = desc.radius;
				}
			}

			std::printf( "m6 envelope %s: %d shape(s), hull half width %.3f m, sphere half width %.3f m (tire %.3f m)\n",
						 testCase.label, shapeCount, hullHalfWidth, sphereHalfWidth, halfWidth );

			if ( hullHalfWidth > 0.0f )
			{
				ok &= CheckTrue( "m6 hull envelope stays inside the tire width", hullHalfWidth < halfWidth + 0.005f );
			}
			if ( testCase.mode == JOZZ_M6_ENVELOPE_SPHERE )
			{
				ok &= CheckTrue( "m6 sphere bulges past the tire (the documented trade-off)",
								 sphereHalfWidth > halfWidth + 0.05f );
			}

			for ( int i = 0; i < shapeCount; ++i )
			{
				b3DestroyShape( shapeIds[i], false );
			}
		}

		b3DestroyWorld( worldId );
	}

	// Invisible-wall check: a static prop face sits 5 cm past the tire's side
	// face - closer than the sphere bulge (~29 cm), farther than the tire.
	// The split wheel must ignore it; the sphere wheel must hit it.
	for ( int pass = 0; pass < 2; ++pass )
	{
		bool split = pass == 0;

		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		CreateM6SmokeGround( worldId, 0.8f );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = { 0.0f, defaults.wheelRadius + 0.01f, 0.0f };
		bodyDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
		b3BodyId wheelId = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = 80.0f;
		JozzVehicleM6WheelEnvelopeDesc desc = {};
		desc.mode = split ? JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL : JOZZ_M6_ENVELOPE_SPHERE;
		desc.cylinderSides = 32;
		desc.unionLayerCount = 4;
		desc.radius = defaults.wheelRadius;
		desc.width = defaults.wheelWidth;
		desc.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;
		b3ShapeId shapeIds[JOZZ_M6_MAX_WHEEL_SHAPES];
		CreateJozzVehicleM6WheelEnvelope( wheelId, &shapeDef, &desc, shapeIds );

		// Prop wall: near face at tire half width + 5 cm from the wheel center.
		float wallHalf = 0.5f;
		b3BodyDef wallDef = b3DefaultBodyDef();
		wallDef.position = { 0.0f, defaults.wheelRadius, halfWidth + 0.05f + wallHalf };
		wallDef.name = "m6_prop_wall";
		b3BodyId wallId = b3CreateBody( worldId, &wallDef );
		b3ShapeDef wallShapeDef = b3DefaultShapeDef();
		// Obstacle, not terrain: the engine default category is ALL bits and
		// would match the rolling sphere's terrain-only mask.
		wallShapeDef.filter.categoryBits = JOZZ_M6_OBJECT_CATEGORY;
		b3BoxHull wallBox = b3MakeBoxHull( wallHalf, wallHalf, wallHalf );
		b3ShapeId wallShapeId = b3CreateHullShape( wallId, &wallShapeDef, &wallBox.base );

		for ( int i = 0; i < 60; ++i )
		{
			b3World_Step( worldId, 1.0f / 60.0f, 4 );
		}

		// Touching means an actual (or grazing) contact, not a speculative
		// manifold point parked at positive separation by the broadphase.
		bool touching = false;
		b3ContactData contacts[8];
		int contactCount = b3Shape_GetContactData( wallShapeId, contacts, 8 );
		for ( int i = 0; i < contactCount; ++i )
		{
			for ( int m = 0; m < contacts[i].manifoldCount; ++m )
			{
				for ( int p = 0; p < contacts[i].manifolds[m].pointCount; ++p )
				{
					if ( contacts[i].manifolds[m].points[p].separation < 0.001f )
					{
						touching = true;
					}
				}
			}
		}
		std::printf( "m6 invisible-wall check (%s): prop contact %s\n", split ? "split envelope" : "single sphere",
					 touching ? "YES" : "no" );
		if ( split )
		{
			ok &= CheckTrue( "m6 split envelope does not touch the prop past the tire face", touching == false );
		}
		else
		{
			ok &= CheckTrue( "m6 single sphere hits the prop past the tire face (the reported bug)", touching );
		}

		b3DestroyWorld( worldId );
	}

	// Rolling smoothness, M5.2 methodology, on the M6 strut vehicle so the
	// only variable is the wheel envelope itself.
	struct
	{
		const char* label;
		int mode;
		int layers;
	} rollCases[] = {
		{ "cylinder 32   ", JOZZ_M6_ENVELOPE_CYLINDER, 1 },
		{ "phased union 4", JOZZ_M6_ENVELOPE_PHASED_UNION, 4 },
		{ "split         ", JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL, 1 },
		{ "sphere        ", JOZZ_M6_ENVELOPE_SPHERE, 1 },
	};

	float unionContact4 = 0.0f;
	float cylinderContact = 0.0f;
	float sphereContact = 0.0f;
	float splitContact = 0.0f;

	for ( auto& rollCase : rollCases )
	{
		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );

		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		// Strut on both axles isolates the envelope from the new rig.
		config.frontRigType = JOZZ_M6_RIG_INTEGRATED_STRUT;
		config.rearRigType = JOZZ_M6_RIG_INTEGRATED_STRUT;
		config.wheelEnvelope.mode = rollCase.mode;
		config.wheelEnvelope.unionLayerCount = rollCase.layers;

		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

		const float timeStep = 1.0f / 60.0f;
		const int subStepCount = 4;

		JozzVehicleM6DriveInput input = {};
		for ( int i = 0; i < 90; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
		input.drive = 1.0f;
		for ( int i = 0; i < 360; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}

		int sampleSteps = 180;
		int contactSamples = 0;
		double verticalSquaredSum = 0.0;
		float topSpeed = 0.0f;
		for ( int i = 0; i < sampleSteps; ++i )
		{
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );

			JozzVehicleM6WheelTelemetry frontLeft = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT );
			JozzVehicleM6WheelTelemetry frontRight = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT );
			if ( frontLeft.groundContact && frontRight.groundContact )
			{
				contactSamples += 1;
			}

			float vyLeft = b3Body_GetLinearVelocity( vehicle.corners[JOZZ_M6_FRONT_LEFT].wheelId ).y;
			float vyRight = b3Body_GetLinearVelocity( vehicle.corners[JOZZ_M6_FRONT_RIGHT].wheelId ).y;
			verticalSquaredSum += 0.5 * ( (double)vyLeft * vyLeft + (double)vyRight * vyRight );

			float speed = GetJozzVehicleM6ForwardSpeed( vehicle );
			topSpeed = b3MaxFloat( topSpeed, speed );
		}

		float contactFraction = (float)contactSamples / (float)sampleSteps;
		float verticalRms = (float)std::sqrt( verticalSquaredSum / (double)sampleSteps );
		std::printf( "m6 roll %s: contact %.0f%%, front vy rms %.3f m/s, top speed %.1f m/s\n", rollCase.label,
					 100.0f * contactFraction, verticalRms, topSpeed );

		ok &= CheckTrue( "m6 roll probe reaches cruise speed", topSpeed > 8.0f );
		ok &= CheckTrue( "m6 roll metrics are finite", b3IsValidFloat( verticalRms ) && b3IsValidFloat( contactFraction ) );

		if ( rollCase.mode == JOZZ_M6_ENVELOPE_CYLINDER )
		{
			cylinderContact = contactFraction;
		}
		else if ( rollCase.mode == JOZZ_M6_ENVELOPE_SPHERE )
		{
			sphereContact = contactFraction;
		}
		else if ( rollCase.mode == JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL )
		{
			splitContact = contactFraction;
		}
		else
		{
			unionContact4 = contactFraction;
		}

		DestroyJozzVehicleM6( &vehicle );
		b3DestroyWorld( worldId );
	}

	// The claim the M6 default rides on: the split envelope rolls terrain on
	// its sphere only, so it must keep the sphere's (near-)perfect contact.
	ok &= CheckTrue( "m6 split envelope keeps sphere-class ground contact", splitContact > 0.95f );
	std::printf( "m6 envelope summary: cylinder %.0f%%, union-4 %.0f%%, split %.0f%%, sphere %.0f%% front contact\n",
				 100.0f * cylinderContact, 100.0f * unionContact4, 100.0f * splitContact, 100.0f * sphereContact );

	return ok;
}

} // namespace

int main()
{
	// Unbuffered stdout: if a physics assert aborts the process mid-run, the
	// log must show the last line actually reached, not a 4 KiB block cut.
	std::setvbuf( stdout, nullptr, _IONBF, 0 );

	JozzVehicleAuditMetadata metadata = LoadJozzVehicleAuditMetadata();
	std::printf( "metadata: %s\n", metadata.status.c_str() );
	if ( metadata.loadedFromRuntimeReport )
	{
		std::printf( "source: %s\n", metadata.sourcePath.c_str() );
	}
	else
	{
		std::printf( "source: built-in fallback\n" );
	}

	bool ok = true;
	ok &= CheckSemanticPoint( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
	ok &= CheckSemanticPoint( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter" );
	ok &= CheckSemanticPoint( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthLeft" );
	ok &= CheckSemanticPoint( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthRight" );
	ok &= CheckSemanticPoint( metadata, "One_Sided_wheel_mount.gltf", "Socket_WheelCenter" );
	ok &= CheckSemanticPoint( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Top" );
	ok &= CheckSemanticPoint( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Bottom" );

	JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( metadata );
	ok &= CheckApprox( "metersPerBlockbenchUnit", defaults.metersPerBlockbenchUnit, 0.35f, 0.001f );
	ok &= CheckApprox( "wheelRadius", defaults.wheelRadius, 0.514f, 0.02f );
	ok &= CheckApprox( "wheelWidth", defaults.wheelWidth, 0.438f, 0.02f );
	ok &= CheckApprox( "assetSuspensionTravelHint", defaults.assetSuspensionTravelHint, 0.700f, 0.03f );

	JozzVehicleAssetContract suspensionContract = LoadJozzVehicleAssetContract( "one_sided_wheel_mount.asset.json" );
	std::printf( "contract: %s\n", suspensionContract.status.c_str() );
	std::printf( "contract source: %s\n", suspensionContract.sourcePath.empty() ? "missing" : suspensionContract.sourcePath.c_str() );
	for ( const std::string& warning : suspensionContract.warnings )
	{
		std::printf( "contract warning: %s\n", warning.c_str() );
	}

	std::vector<std::string> contractErrors;
	ok &= ValidateJozzVehicleSuspensionCornerContract( suspensionContract, &contractErrors );
	for ( const std::string& error : contractErrors )
	{
		std::printf( "contract error: %s\n", error.c_str() );
	}

	ok &= CheckContractRole( suspensionContract, "suspension.visual.chassis_mount", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.wheel_center", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.damper_upper_r", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.damper_upper_l", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.damper_lower_r", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.damper_lower_l", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.cardan_drive", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.cardan_hub", "visual_endpoint" );
	ok &= CheckContractRole( suspensionContract, "suspension.travel_axis.top", "physics_hint" );
	ok &= CheckContractRole( suspensionContract, "suspension.travel_axis.bottom", "physics_hint" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.chassis_top", "visual_part" );
	ok &= CheckContractRole( suspensionContract, "suspension.visual.chassis_bottom", "visual_part" );

	ok &= RunM5DriveSmoke( defaults );
	ok &= RunM5WheelShapeExperiment( defaults );
	ok &= RunM6SuspensionRigSmoke( defaults );
	ok &= RunM6WheelEnvelopeProbe( defaults );
	ok &= RunM7LandingIntegrityProbe( defaults );
	ok &= RunM7HandsOffAlignProbe( defaults );
	ok &= RunM7TorqueDriveProbe( defaults );
	ok &= RunM7TrailingArmSmoke( defaults );
	ok &= RunP2RackTravelRegressionProbe( defaults );
	ok &= RunP1SteeringFenceProbe( defaults );
	ok &= RunP5SteeringSetupProbe( defaults );
	ok &= RunP6MassAndLimitSanityProbe( defaults );
	ok &= RunP6StressMatrixProbe( defaults );
	ok &= RunP3SuspensionPreloadProbe( defaults );
	ok &= RunP4SteeringReturnProbe( defaults );
	ok &= RunP4CenteringAssistProbe( defaults );

	if ( ok == false )
	{
		std::printf( "jozz_vehicle_validation: FAILED\n" );
		return 1;
	}

	std::printf( "jozz_vehicle_validation: OK\n" );
	return 0;
}
