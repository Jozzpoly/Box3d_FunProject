// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_dimensions.h"

#include <cmath>

b3Vec3 JozzVehicleFindPointOrFallback( const JozzVehicleAuditMetadata& metadata, const char* assetFile,
									   const char* semanticName, b3Vec3 fallback )
{
	b3Vec3 result = fallback;
	FindJozzVehicleSemanticPoint( metadata, assetFile, semanticName, &result );
	return result;
}

JozzVehiclePrimitiveDefaults GetJozzVehicleM3ADefaults( const JozzVehicleAuditMetadata& metadata )
{
	// M3A/M3B.2-prep derives primitive defaults from runtime-loaded audit
	// metadata when available. If the report cannot be found from the executable
	// working directory, LoadJozzVehicleAuditMetadata() provides the same audited
	// fallback constants. This keeps the lab robust while moving toward runtime
	// asset metadata.
	constexpr float metersPerBlockbenchUnit = 0.35f;

	b3Vec3 wheelMount =
		JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount", { 0.25f, 0.5f, 0.0f } );
	b3Vec3 radiusOuter = JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter",
														 { -0.125f, 1.96875f, 0.0f } );
	b3Vec3 widthLeft = JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthLeft",
													   { -0.75f, 0.5f, 0.0f } );
	b3Vec3 widthRight = JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthRight",
														{ 0.5f, 0.5f, 0.0f } );
	b3Vec3 travelTop = JozzVehicleFindPointOrFallback( metadata, "One_Sided_wheel_mount.gltf",
													   "Axis_SuspensionTravel_Top", { -1.1875f, 1.5f, 0.0f } );
	b3Vec3 travelBottom = JozzVehicleFindPointOrFallback( metadata, "One_Sided_wheel_mount.gltf",
														  "Axis_SuspensionTravel_Bottom", { -1.1875f, -0.5f, 0.0f } );

	float wheelRadius = std::fabs( radiusOuter.y - wheelMount.y ) * metersPerBlockbenchUnit;
	float wheelWidth = std::fabs( widthRight.x - widthLeft.x ) * metersPerBlockbenchUnit;
	float assetSuspensionTravelHint = std::fabs( travelTop.y - travelBottom.y ) * metersPerBlockbenchUnit;

	return {
		metersPerBlockbenchUnit,
		wheelRadius,
		wheelWidth,
		assetSuspensionTravelHint,
		0.42f,
		0.32f,
		0.16f,
		1.70f,
		0.82f,
	};
}
