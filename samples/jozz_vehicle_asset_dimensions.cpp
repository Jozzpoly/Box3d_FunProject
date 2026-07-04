// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_dimensions.h"

#include <cmath>

JozzVehiclePrimitiveDefaults GetJozzVehicleM3ADefaults( const JozzVehicleAuditMetadata& metadata )
{
	// M3A/M3B.2-prep derives primitive defaults from runtime-loaded audit
	// metadata when available. On a miss the lookup falls back to the built-in
	// audited table owned by the metadata module, so the constants live in one
	// place. This keeps the lab robust while moving toward runtime asset
	// metadata.
	constexpr float metersPerBlockbenchUnit = 0.35f;

	b3Vec3 wheelMount = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
	b3Vec3 radiusOuter = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter" );
	b3Vec3 widthLeft = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthLeft" );
	b3Vec3 widthRight = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthRight" );
	b3Vec3 travelTop = JozzVehicleFindPointOrBuiltIn( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Top" );
	b3Vec3 travelBottom = JozzVehicleFindPointOrBuiltIn( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Bottom" );

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
