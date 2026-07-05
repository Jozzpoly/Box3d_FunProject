// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_m5_vehicle.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>
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

} // namespace

int main()
{
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

	if ( ok == false )
	{
		std::printf( "jozz_vehicle_validation: FAILED\n" );
		return 1;
	}

	std::printf( "jozz_vehicle_validation: OK\n" );
	return 0;
}
