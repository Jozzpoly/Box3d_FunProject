// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// M5/M6 smoke + envelope probes (R1 move-only split of jozz_vehicle_validation.cpp).

#include "jozz_validation_helpers.h"

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_m5_vehicle.h"
#include "jozz_vehicle_m6_suspension_rig.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>

namespace
{

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

} // namespace

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
