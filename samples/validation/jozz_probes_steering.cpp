// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// P1/P2/P4/P4b/pull/P5 steering probes (R1 move-only split of jozz_vehicle_validation.cpp).

#include "jozz_validation_helpers.h"

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_m6_suspension_rig.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>

// P2 regression: rackTravel is derived from steering geometry
// (ComputeJozzVehicleM6RackStroke) and must be recomputed whenever that
// geometry changes, or the rack limit silently goes stale (the linkage can
// then overshoot its own tie-rod dead point before the joint limit stops it -
// see docs/archive/vehicle_legacy_2026-07/AUDIT_PHYSICS_STEERING_2026_07_08_PL.md). This does not assert on the
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
// (historyczny wpis #9 w docs/archive/ledgers/TECH_DEBT_LEGACY_2026-07_PL.md). A deeper root-cause test (2026-07-08, decisive
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
		int leftCorner = frontPass ? JOZZ_M6_FRONT_LEFT : JOZZ_M6_REAR_LEFT;
		int rightCorner = frontPass ? JOZZ_M6_FRONT_RIGHT : JOZZ_M6_REAR_RIGHT;

		// Calibrated dial (audit A4, plan tolerance restored): the settle
		// splay documented above sits ON TOP of the commanded toe, so the
		// dial is judged on the DELTA between a toe=+1 build and an otherwise
		// identical toe=0 baseline build - measuring the absolute angle mixes
		// the two and is exactly how the old 1.43x dial slipped through a
		// widened "ballpark" band. With SteeringArmWithToe's exact
		// kingpin-axis rotation the delta must land within +-0.3 deg of the
		// commanded 1 deg on BOTH wheels (measured 2026-07-09: front
		// -0.99/+0.96, rear -0.88/+0.89).
		float angles[2][2] = {}; // [toePass][left/right]
		bool finiteOk = true;
		for ( int toePass = 0; toePass < 2; ++toePass )
		{
			JozzVehicleM6Config config = JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth,
																	 defaults.assetSuspensionTravelHint );
			float toe = toePass == 0 ? 0.0f : 1.0f;
			if ( frontPass )
			{
				config.frontToeDeg = toe;
			}
			else
			{
				config.rearToeDeg = toe;
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

			angles[toePass][0] = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, leftCorner ).steeringAngle;
			angles[toePass][1] = 180.0f / B3_PI * GetJozzVehicleM6WheelTelemetry( vehicle, rightCorner ).steeringAngle;
			finiteOk &= IsM6VehicleStateValid( vehicle );

			DestroyJozzVehicleM6( &vehicle );
			b3DestroyWorld( worldId );
		}

		float leftDelta = angles[1][0] - angles[0][0];
		float rightDelta = angles[1][1] - angles[0][1];
		std::printf( "  %s toe=+1 deg: baseline %.2f/%.2f, toed %.2f/%.2f -> delta left %+.2f, right %+.2f deg\n",
					 frontPass ? "front" : "rear", angles[0][0], angles[0][1], angles[1][0], angles[1][1], leftDelta,
					 rightDelta );

		char label[96];
		std::snprintf( label, sizeof( label ), "p5 %s toe=+1 delta has opposite-signed L/R angles (toe-in)",
						frontPass ? "front" : "rear" );
		ok &= CheckTrue( label, leftDelta < 0.0f && rightDelta > 0.0f );
		std::snprintf( label, sizeof( label ), "p5 %s toe=+1 left delta within +-0.3 deg of commanded",
						frontPass ? "front" : "rear" );
		ok &= CheckTrue( label, std::fabs( leftDelta + 1.0f ) <= 0.3f );
		std::snprintf( label, sizeof( label ), "p5 %s toe=+1 right delta within +-0.3 deg of commanded",
						frontPass ? "front" : "rear" );
		ok &= CheckTrue( label, std::fabs( rightDelta - 1.0f ) <= 0.3f );
		ok &= CheckTrue( "p5 toe probe state is finite", finiteOk );
	}

	std::printf( "p5 steering setup probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}

// P4b (2026-07-09, Jozz's decision): hands-off rack resistance is
// LOAD-DEPENDENT - cap = stiction * (base + coeff * transverse tie-rod load)
// - replacing the flat static/kinetic pair. See the rackFrictionBase field
// comment in jozz_vehicle_m6_suspension_rig.h for the model.
//
// History that shaped it (flat-model sweep, 2026-07-08, kept as the record
// of WHY flat friction could not work): a flat cap needed >= ~200 N to keep
// the 3.5 m landing stable (sharp branch-snap cliff below ~140 N: worst
// camber 11-12 deg instead of 0.6-0.8; separate yaw-drift threshold ~200 N),
// but any cap that high also parked the rack ~1 mm off-center after a
// drive-torque transient and near-straight caster forces could never break
// it loose again - the diagnosed left-pull. One flat number could not serve
// both. The load-proportional term resolves it: a landing loads the tie rods
// with kN so friction spikes exactly when stability needs it (3.5 m landing
// re-measured with base=40/coeff=0.10: worst camber 0.6 deg - identical to
// the old 200 N floor), while near-straight cruising leaves the rack free
// enough to self-center (straight-pull probe: heading @10 s dropped from
// +14 deg to a ~+-3 deg wander around zero, rack no longer parks off-center).
//
// Overshoot ("przestrzal") after a full-lock release stays a printed,
// NON-gating diagnostic: measured min angle stays slightly positive even
// with the free-er rack (the caster force fades exactly at center - an
// honest property of this linkage, not a friction artifact; the flat-model
// sweep already showed no friction value produces a clean crossing).
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
		// range without reactivating the historical branch-snap failure).
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

// Straight-line pull regression (born as the diagnosis of Jozz's 2026-07-09
// report: the car visibly pulled LEFT in ordinary straight driving; steering
// convention: positive = LEFT). Diagnosis found: rest state symmetric, the
// kick came from the drive-torque transient (AWD +14 deg/10 s vs RWD +3.4),
// and the flat 250 N static friction then PARKED the rack ~1 mm off-center
// where near-straight caster forces could never break it loose. Fixed by the
// P4b load-dependent friction model: with the rack nearly free when
// unloaded, small caster forces keep re-centering it (measured after fix:
// heading @10 s ~-2.7 deg slow wander around zero instead of a one-sided
// +14 deg march; rack oscillates around 0 instead of parking). GATED on that
// fixed behavior below; the AWD/RWD/reverse variants stay as diagnostics
// (hands-off reverse flopping to the rack limit is correct caster physics).
bool RunStraightPullDiagnosisProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "straight-pull diagnosis probe:\n" );
	bool ok = true;

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

	const float timeStep = 1.0f / 60.0f;
	const int subStepCount = 4;
	JozzVehicleM6DriveInput input = {};

	for ( int i = 0; i < 240; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
	}

	const char* cornerNames[JOZZ_M6_CORNER_COUNT] = { "FL", "FR", "RL", "RR" };
	std::printf( "  rest after settle: rack %+.5f m\n", b3PrismaticJoint_GetTranslation( vehicle.rackJointId ) );
	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		JozzVehicleM6WheelTelemetry t = GetJozzVehicleM6WheelTelemetry( vehicle, corner );
		std::printf( "    %s: steer %+.3f deg, camber %+.3f deg, travel %+.4f m\n", cornerNames[corner],
					 180.0f / B3_PI * t.steeringAngle, 180.0f / B3_PI * t.camberAngle, t.suspensionTravel );
	}

	// Hands-off straight drive: steer stays 0 (below the deadzone), so the
	// rack rides on friction only - exactly the reported scenario.
	input.drive = 1.0f;
	for ( int i = 1; i <= 600; ++i )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );
		if ( i % 120 == 0 )
		{
			b3Vec3 forward = b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisX );
			float headingDeg = 180.0f / B3_PI * std::atan2( -forward.z, forward.x ); // + = nose LEFT
			b3Pos p = b3Body_GetPosition( vehicle.chassisId );
			JozzVehicleM6WheelTelemetry fl = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT );
			JozzVehicleM6WheelTelemetry fr = GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT );
			std::printf( "  t=%.0fs: heading %+.2f deg, x %.1f m, z %+.3f m, rack %+.5f m, FL %+.2f FR %+.2f deg\n",
						 (double)( i * timeStep ), headingDeg, p.x, p.z,
						 b3PrismaticJoint_GetTranslation( vehicle.rackJointId ), 180.0f / B3_PI * fl.steeringAngle,
						 180.0f / B3_PI * fr.steeringAngle );
		}
	}

	ok &= CheckTrue( "straight-pull probe state is finite", IsM6VehicleStateValid( vehicle ) );
	// Regression gate (post-P4b numbers +100% margin): the pre-fix pull
	// measured +14.0 deg heading and -15.0 m lateral at t=10 s; fixed model
	// wanders within ~+-2.7 deg / +-1.9 m. Gate at the midpoint so a
	// re-introduced systematic pull fails loudly while the honest slow wander
	// of a nearly-free rack does not flake the build.
	{
		b3Vec3 forward = b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisX );
		float headingDeg = 180.0f / B3_PI * std::atan2( -forward.z, forward.x );
		b3Pos p = b3Body_GetPosition( vehicle.chassisId );
		ok &= CheckTrue( "straight-pull heading stays under 6 deg after 10 s", std::fabs( headingDeg ) < 6.0f );
		ok &= CheckTrue( "straight-pull lateral drift stays under 5 m after 10 s", std::fabs( p.z ) < 5.0f );
	}

	DestroyJozzVehicleM6( &vehicle );
	b3DestroyWorld( worldId );

	// Variant runs to pin the source of the throttle-on kick: if the drift
	// flips with REVERSE drive, the kick is mediated by the drive-torque
	// reaction; if it persists with the same sign regardless of direction and
	// drivetrain, it is a systematic solver-order bias. Either way the HOLD
	// half of the pull (rack parked ~1 mm off-center by static friction) is
	// the friction model's doing - Gate 2 territory.
	struct PullVariant
	{
		const char* label;
		float drive;
		bool awd;
	};
	PullVariant variants[] = {
		{ "reverse-awd", -1.0f, true },
		{ "forward-rwd", 1.0f, false },
	};
	for ( const PullVariant& variant : variants )
	{
		JozzVehicleM6Config vconfig = config;
		vconfig.allWheelDrive = variant.awd;

		b3WorldId vworldId = b3CreateWorld( &worldDef );
		b3BodyId vgroundId = CreateM6SmokeGround( vworldId, 0.8f );
		JozzVehicleM6 vvehicle = CreateJozzVehicleM6( vworldId, vgroundId, vconfig, { 0.0f, spawnHeight, 0.0f } );

		JozzVehicleM6DriveInput vinput = {};
		for ( int i = 0; i < 240; ++i )
		{
			UpdateJozzVehicleM6Drive( vvehicle, vinput );
			b3World_Step( vworldId, timeStep, subStepCount );
		}
		vinput.drive = variant.drive;
		for ( int i = 1; i <= 600; ++i )
		{
			UpdateJozzVehicleM6Drive( vvehicle, vinput );
			b3World_Step( vworldId, timeStep, subStepCount );
		}
		b3Vec3 forward = b3RotateVector( b3Body_GetRotation( vvehicle.chassisId ), b3Vec3_axisX );
		float headingDeg = 180.0f / B3_PI * std::atan2( -forward.z, forward.x );
		b3Pos p = b3Body_GetPosition( vvehicle.chassisId );
		std::printf( "  variant %s: heading %+.2f deg, x %.1f m, z %+.3f m, rack %+.5f m\n", variant.label, headingDeg,
					 p.x, p.z, b3PrismaticJoint_GetTranslation( vvehicle.rackJointId ) );
		ok &= CheckTrue( "straight-pull variant state is finite", IsM6VehicleStateValid( vvehicle ) );

		DestroyJozzVehicleM6( &vvehicle );
		b3DestroyWorld( vworldId );
	}

	std::printf( "straight-pull diagnosis probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}
