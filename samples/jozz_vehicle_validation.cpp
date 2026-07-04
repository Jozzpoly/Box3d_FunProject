// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_metadata.h"

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

	if ( ok == false )
	{
		std::printf( "jozz_vehicle_validation: FAILED\n" );
		return 1;
	}

	std::printf( "jozz_vehicle_validation: OK\n" );
	return 0;
}
