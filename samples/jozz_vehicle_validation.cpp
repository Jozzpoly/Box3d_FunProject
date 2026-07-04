// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"

#include <cmath>
#include <cstdio>

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

	if ( ok == false )
	{
		std::printf( "jozz_vehicle_validation: FAILED\n" );
		return 1;
	}

	std::printf( "jozz_vehicle_validation: OK\n" );
	return 0;
}
