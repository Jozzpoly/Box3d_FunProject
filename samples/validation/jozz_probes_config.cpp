// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// P3/P6/sanitize/preset-determinism/stress probes (R1 move-only split of jozz_vehicle_validation.cpp).

#include "jozz_validation_helpers.h"

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_m6_config_io.h"
#include "jozz_vehicle_m6_suspension_rig.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

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

namespace
{

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

} // namespace

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

// Preset determinism regression (2026-07-09, caught by Jozz in manual play):
// presets are PARTIAL files ("only what makes this setup different") and were
// loaded IN PLACE, so every field a preset didn't mention silently kept
// whatever experimental value the sliders had left there - from the user's
// chair "the preset saved my changes without permission", and the session
// auto-save then persisted them across restarts. The contract pinned here:
// loading a preset = FACTORY DEFAULTS + the preset's own keys, regardless of
// the config state before the load (LoadJozzVehicleM6PresetConfig).
bool RunPresetDeterminismProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "preset determinism probe:\n" );
	bool ok = true;

	JozzVehicleM6Config factory =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

	// Simulate a slider-fiddling session: fields offroad.json does NOT define
	// (must come back as factory values after the load)...
	JozzVehicleM6Config fiddled = factory;
	fiddled.rackFrictionBase = 999.0f;
	fiddled.brakeTorque = 123.0f;
	fiddled.frontToeDeg = 3.0f;
	// ...and one it DOES define (must come back as the preset's value).
	fiddled.suspensionHertz = 11.0f;
	// Etap 2 (persystencja, 2026-07-11): visual identity is now config, not
	// UI-only state - offroad.json doesn't define these either, so they must
	// return to factory the same as any other unlisted field. The fiddled
	// values MUST differ from JozzVehicleM6DefaultConfig or the checks below
	// are vacuous - since Etap 3 the factory is rama_rurowa/rig_kierowniczy
	// (decyzje D1/D2), so the sabotage is "brak"/"klasyczny".
	std::snprintf( fiddled.bodyVisualModel, sizeof( fiddled.bodyVisualModel ), "brak" );
	fiddled.bodyVisualOffset = { 0.11f, 0.22f, 0.33f };
	std::snprintf( fiddled.frontSuspensionVisualModel, sizeof( fiddled.frontSuspensionVisualModel ), "klasyczny" );

	std::string presetPath;
	bool found = FindJozzVehicleAssetFile( "assets/vehicle_presets/offroad.json", &presetPath );
	ok &= CheckTrue( "preset determinism: offroad.json found", found );
	if ( found )
	{
		JozzVehicleM6Config loaded = fiddled;
		ok &= CheckTrue( "preset determinism: offroad.json loads",
						 LoadJozzVehicleM6PresetConfig( presetPath, factory, &loaded ) );
		std::printf( "  after load: rackFrictionBase %.0f (factory %.0f), brakeTorque %.0f (factory %.0f), "
					 "frontToeDeg %.1f (factory %.1f), suspensionHertz %.1f (preset 3.5)\n",
					 loaded.rackFrictionBase, factory.rackFrictionBase, loaded.brakeTorque, factory.brakeTorque,
					 loaded.frontToeDeg, factory.frontToeDeg, loaded.suspensionHertz );
		std::printf( "  after load: bodyVisualModel '%s' (factory '%s'), bodyVisualOffset [%.2f %.2f %.2f] "
					 "(factory [%.2f %.2f %.2f]), frontSuspensionVisualModel '%s' (factory '%s')\n",
					 loaded.bodyVisualModel, factory.bodyVisualModel, loaded.bodyVisualOffset.x,
					 loaded.bodyVisualOffset.y, loaded.bodyVisualOffset.z, factory.bodyVisualOffset.x,
					 factory.bodyVisualOffset.y, factory.bodyVisualOffset.z, loaded.frontSuspensionVisualModel,
					 factory.frontSuspensionVisualModel );
		ok &= CheckTrue( "preset determinism: unlisted rackFrictionBase returns to factory",
						 loaded.rackFrictionBase == factory.rackFrictionBase );
		ok &= CheckTrue( "preset determinism: unlisted brakeTorque returns to factory",
						 loaded.brakeTorque == factory.brakeTorque );
		ok &= CheckTrue( "preset determinism: unlisted frontToeDeg returns to factory",
						 loaded.frontToeDeg == factory.frontToeDeg );
		ok &= CheckTrue( "preset determinism: listed suspensionHertz takes the preset value",
						 std::fabs( loaded.suspensionHertz - 3.5f ) < 1.0e-4f );
		ok &= CheckTrue( "preset determinism: unlisted bodyVisualModel returns to factory",
						 std::strcmp( loaded.bodyVisualModel, factory.bodyVisualModel ) == 0 );
		ok &= CheckTrue( "preset determinism: unlisted bodyVisualOffset returns to factory",
						 loaded.bodyVisualOffset.x == factory.bodyVisualOffset.x &&
							 loaded.bodyVisualOffset.y == factory.bodyVisualOffset.y &&
							 loaded.bodyVisualOffset.z == factory.bodyVisualOffset.z );
		ok &= CheckTrue( "preset determinism: unlisted frontSuspensionVisualModel returns to factory",
						 std::strcmp( loaded.frontSuspensionVisualModel, factory.frontSuspensionVisualModel ) == 0 );
	}

	// Round-trip save/load (Etap 2 §4.3): guards the lastInObject trap from
	// config_io.cpp's field table directly - a broken trailing comma makes the
	// whole file fail jsmn parsing, which LoadJozzVehicleM6Config reports as
	// false / leaves outConfig untouched, so this would go red immediately.
	{
		const char* roundtripPath = "build/jozz_vehicle_probe_roundtrip.json";
		ok &= CheckTrue( "preset determinism: round-trip save", SaveJozzVehicleM6Config( fiddled, roundtripPath ) );

		JozzVehicleM6Config reloaded =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		ok &= CheckTrue( "preset determinism: round-trip load", LoadJozzVehicleM6Config( roundtripPath, &reloaded ) );
		std::printf( "  round-trip: bodyVisualModel '%s', bodyVisualOffset [%.2f %.2f %.2f], "
					 "frontSuspensionVisualModel '%s', suspensionHertz %.1f\n",
					 reloaded.bodyVisualModel, reloaded.bodyVisualOffset.x, reloaded.bodyVisualOffset.y,
					 reloaded.bodyVisualOffset.z, reloaded.frontSuspensionVisualModel, reloaded.suspensionHertz );
		ok &= CheckTrue( "preset determinism: round-trip bodyVisualModel",
						 std::strcmp( reloaded.bodyVisualModel, fiddled.bodyVisualModel ) == 0 );
		ok &= CheckTrue( "preset determinism: round-trip bodyVisualOffset",
						 reloaded.bodyVisualOffset.x == fiddled.bodyVisualOffset.x &&
							 reloaded.bodyVisualOffset.y == fiddled.bodyVisualOffset.y &&
							 reloaded.bodyVisualOffset.z == fiddled.bodyVisualOffset.z );
		ok &= CheckTrue( "preset determinism: round-trip frontSuspensionVisualModel",
						 std::strcmp( reloaded.frontSuspensionVisualModel, fiddled.frontSuspensionVisualModel ) == 0 );
		ok &= CheckTrue( "preset determinism: round-trip suspensionHertz (control field, unrelated segment)",
						 std::fabs( reloaded.suspensionHertz - fiddled.suspensionHertz ) < 1.0e-4f );
	}

	std::printf( "preset determinism probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}
