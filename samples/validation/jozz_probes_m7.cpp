// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// M7 landing/hands-off/torque/trailing probes (R1 move-only split of jozz_vehicle_validation.cpp).

#include "jozz_validation_helpers.h"

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_m6_suspension_rig.h"
#include "jozz_vehicle_m7_suspension_import.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>

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
			// Frozen rack: hands-off friction so large the linkage cannot move
			// (base term alone does it - no load needed).
			config.rackFrictionBase = 1.0e6f;
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
