// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "validation/jozz_validation_helpers.h"

#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_steering_suspension_contract.h"
#include "jozz_vehicle_world_layout.h"

#include "box3d/box3d.h"

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

// Per-fragment spawn system (2026-07-24): the map-fragment classifier is pure
// geometry (world_layout.h), so it is verified here directly rather than in a
// physics probe. A drifted offroad seam or a scan-priority regression shows up
// as a "bad fragment" line instead of a silently mis-spawned car.
bool CheckFragment( const char* label, JozzWorldLayout::JozzMapFragment got, JozzWorldLayout::JozzMapFragment want )
{
	if ( got == want )
	{
		std::printf( "ok fragment %s = %d\n", label, (int)got );
		return true;
	}
	std::printf( "bad fragment %s = %d, expected %d\n", label, (int)got, (int)want );
	return false;
}

} // namespace

// Probe bodies live in validation/jozz_probes_*.cpp (R1 move-only split);
// declared here so the registry below can take their address. Every probe
// shares the signature bool(const JozzVehiclePrimitiveDefaults&).
bool RunM5DriveSmoke( const JozzVehiclePrimitiveDefaults& defaults );
bool RunM5WheelShapeExperiment( const JozzVehiclePrimitiveDefaults& defaults );
bool RunM6SuspensionRigSmoke( const JozzVehiclePrimitiveDefaults& defaults );
bool RunM6WheelEnvelopeProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunM7HandsOffAlignProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunM7LandingIntegrityProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunM7TorqueDriveProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunM7TrailingArmSmoke( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP2RackTravelRegressionProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP4CenteringAssistProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP1SteeringFenceProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP5SteeringSetupProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP4SteeringReturnProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunStraightPullDiagnosisProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunRideQualityDiagnosisProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP6MassAndLimitSanityProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP6StressMatrixProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunP3SuspensionPreloadProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunPresetDeterminismProbe( const JozzVehiclePrimitiveDefaults& defaults );
bool RunCentralCampusLayoutProbe(); // E2R map, recovered 2026-07-24 (track probe omitted)
bool RunMasterplanYardProbe();

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

	// New steering-suspension rig (OneSided_Steering_Suspension_Rig.gltf) - a
	// SEPARATE contract from one_sided_wheel_mount above, not interchangeable.
	// See the contract's "notes" block for the parenting differences (upright
	// named ChassisMount_b, damper lower eye on the lower arm, new steering rod
	// part) that make this model's rig binding different from the old one.
	JozzVehicleAssetContract steeringContract = LoadJozzVehicleAssetContract( "one_sided_steering_suspension.asset.json" );
	std::printf( "steering contract: %s\n", steeringContract.status.c_str() );
	std::printf( "steering contract source: %s\n",
				 steeringContract.sourcePath.empty() ? "missing" : steeringContract.sourcePath.c_str() );
	for ( const std::string& warning : steeringContract.warnings )
	{
		std::printf( "steering contract warning: %s\n", warning.c_str() );
	}

	std::vector<std::string> steeringContractErrors;
	ok &= ValidateJozzVehicleSteeringSuspensionContract( steeringContract, &steeringContractErrors );
	for ( const std::string& error : steeringContractErrors )
	{
		std::printf( "steering contract error: %s\n", error.c_str() );
	}

	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.chassis_mount_a", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.chassis_mount_b", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.wheel_center", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.damper_mount", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.damper_upper", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.damper_lower", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.steering_rod", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.cardan_drive", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.cardan_hub", "visual_endpoint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.travel_axis.top", "physics_hint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.travel_axis.bottom", "physics_hint" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.chassis_top", "visual_part" );
	ok &= CheckContractRole( steeringContract, "steering_suspension.visual.chassis_bottom", "visual_part" );

	// Derived-vector sanity numbers (printed, not just asserted - see the
	// "validator asserts loosely" rule in README_FOR_AGENTS.md): these are the
	// same authoring-unit distances the model analysis was built from, so a
	// bad re-export or a wrong nodeIndexHint edit shows up as a wrong number
	// here instead of silently passing.
	JozzVehicleSteeringSuspensionSockets steeringSockets = ResolveJozzVehicleSteeringSuspensionSockets( steeringContract );
	if ( steeringSockets.resolved )
	{
		float travel = b3Distance( steeringSockets.travelAxisTop, steeringSockets.travelAxisBottom );
		float damperSpan = b3Distance( steeringSockets.damperUpper, steeringSockets.damperLower );
		float uprightSpan = b3Distance( steeringSockets.wheelCenter, steeringSockets.chassisMountB );
		std::printf( "steering sockets: travelAxis=%.4f m, damperSpan=%.4f m, wheelCenter-chassisMountB=%.4f m\n", travel,
					 damperSpan, uprightSpan );
		ok &= CheckApprox( "steeringTravelAxis", travel, 0.700f, 0.03f );
		ok &= CheckApprox( "steeringDamperSpan", damperSpan, 0.689f, 0.03f );
		ok &= CheckApprox( "steeringWheelCenterToChassisMountB", uprightSpan, 0.217f, 0.03f );
	}
	else
	{
		std::printf( "steering sockets: FAILED to resolve\n" );
		ok = false;
	}

	// Map-fragment classifier (per-fragment spawn system). Pure floats, tested
	// directly. Scan bounds below are a stand-in island north of the plate.
	{
		using namespace JozzWorldLayout;
		const float sMinX = -100.0f, sMaxX = 100.0f, sMinZ = 300.0f, sMaxZ = 500.0f;
		ok &= CheckFragment( "plate center", ClassifyJozzMapFragment( 0.0f, 0.0f, false, 0, 0, 0, 0 ), FragmentPlate );
		ok &= CheckFragment( "plate east edge",
							 ClassifyJozzMapFragment( kOffroadOriginX - 1.0f, 0.0f, false, 0, 0, 0, 0 ), FragmentPlate );
		ok &= CheckFragment( "offroad past seam",
							 ClassifyJozzMapFragment( kOffroadOriginX + 1.0f, 0.0f, false, 0, 0, 0, 0 ), FragmentOffroad );
		ok &= CheckFragment( "scan over island",
							 ClassifyJozzMapFragment( 0.0f, 400.0f, true, sMinX, sMaxX, sMinZ, sMaxZ ), FragmentScan );
		ok &= CheckFragment( "scan bounds ignored when unloaded",
							 ClassifyJozzMapFragment( 0.0f, 400.0f, false, sMinX, sMaxX, sMinZ, sMaxZ ), FragmentPlate );
	}

	// Probe registry: ONE list, iterated. Adding a probe = one line here;
	// forgetting to register it now shows up as a smaller "ran N probes"
	// count instead of silently never running (the old hand-kept `ok &= Run`
	// chain let a written-but-unregistered probe pass unnoticed - a false
	// green). Every probe shares the signature bool(const
	// JozzVehiclePrimitiveDefaults&).
	struct Probe
	{
		const char* name;
		bool ( *run )( const JozzVehiclePrimitiveDefaults& );
	};
	const Probe probes[] = {
		{ "m5 drive smoke", RunM5DriveSmoke },
		{ "m5 wheel shape", RunM5WheelShapeExperiment },
		{ "m6 suspension rig smoke", RunM6SuspensionRigSmoke },
		{ "m6 wheel envelope", RunM6WheelEnvelopeProbe },
		{ "m7 landing integrity", RunM7LandingIntegrityProbe },
		{ "m7 hands-off align", RunM7HandsOffAlignProbe },
		{ "m7 torque drive", RunM7TorqueDriveProbe },
		{ "m7 trailing arm smoke", RunM7TrailingArmSmoke },
		{ "p2 rackTravel regression", RunP2RackTravelRegressionProbe },
		{ "p1 steering fence", RunP1SteeringFenceProbe },
		{ "p5 steering setup", RunP5SteeringSetupProbe },
		{ "p6 mass & limit sanity", RunP6MassAndLimitSanityProbe },
		{ "p6 stress matrix", RunP6StressMatrixProbe },
		{ "p3 suspension preload", RunP3SuspensionPreloadProbe },
		{ "p4 steering return", RunP4SteeringReturnProbe },
		{ "p4 centering assist", RunP4CenteringAssistProbe },
		{ "straight-pull diagnosis", RunStraightPullDiagnosisProbe },
		{ "ride quality diagnosis", RunRideQualityDiagnosisProbe },
		{ "preset determinism", RunPresetDeterminismProbe },
	};
	const int probeCount = (int)( sizeof( probes ) / sizeof( probes[0] ) );
	for ( const Probe& probe : probes )
	{
		ok &= probe.run( defaults );
	}
	// E2R map layout probes (recovered 2026-07-24): central campus + masterplan
	// yards. The E3 long-track probe is intentionally omitted (track dormant).
	ok &= RunCentralCampusLayoutProbe();
	ok &= RunMasterplanYardProbe();
	std::printf( "jozz_vehicle_validation: ran %d probes + 2 map probes\n", probeCount );

	if ( ok == false )
	{
		std::printf( "jozz_vehicle_validation: FAILED\n" );
		return 1;
	}

	std::printf( "jozz_vehicle_validation: OK\n" );
	return 0;
}
