// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_debug_preview.h"

#include "gfx/draw.h"
#include "gfx/utility.h"
#include "jozz_vehicle_asset_dimensions.h"

namespace
{

b3Pos WheelAuditPointBU( const JozzVehicleSemanticPreviewConfig& config, b3Vec3 pointBU, b3Vec3 centerBU )
{
	// Schematic mapping only: authoring Y is vertical, authoring X is drawn
	// across world Z, authoring Z is drawn across world X. This is not the
	// final glTF visual transform.
	b3Pos origin = config.wheelPreviewOrigin;
	float scale = config.metersPerBlockbenchUnit;
	return { origin.x + ( pointBU.z - centerBU.z ) * scale, origin.y + ( pointBU.y - centerBU.y ) * scale,
			 origin.z + ( pointBU.x - centerBU.x ) * scale };
}

b3Pos SuspensionAuditPointBU( const JozzVehicleSemanticPreviewConfig& config, b3Vec3 pointBU, b3Vec3 travelTopBU )
{
	// Schematic mapping only. Travel top is anchored to the live chassis/root
	// side, not the wheel/rest-drop side. This previews suspension semantics
	// without making them physics authority.
	b3Pos origin = config.suspensionPreviewOrigin;
	float scale = config.metersPerBlockbenchUnit;
	return { origin.x + ( pointBU.z - travelTopBU.z ) * scale, origin.y + ( pointBU.y - travelTopBU.y ) * scale,
			 origin.z + ( pointBU.x - travelTopBU.x ) * scale };
}

} // namespace

void DrawJozzVehicleSemanticPreview( const JozzVehicleSemanticPreviewConfig& config )
{
	if ( config.metadata == nullptr )
	{
		return;
	}

	const JozzVehicleAuditMetadata& metadata = *config.metadata;

	b3Vec3 wheelMountBU =
		JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount", { 0.25f, 0.5f, 0.0f } );
	b3Vec3 radiusOuterBU = JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter",
														   { -0.125f, 1.96875f, 0.0f } );
	b3Vec3 widthLeftBU = JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthLeft",
														 { -0.75f, 0.5f, 0.0f } );
	b3Vec3 widthRightBU = JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthRight",
														  { 0.5f, 0.5f, 0.0f } );
	b3Vec3 spinABU =
		JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Axis_WheelSpin_A", { 0.4375f, 0.5f, 0.0f } );
	b3Vec3 spinBBU =
		JozzVehicleFindPointOrFallback( metadata, "Offroad_Big_Wheels.gltf", "Axis_WheelSpin_B", { -1.0625f, 0.5f, 0.0f } );
	b3Vec3 wheelCenterBU = { radiusOuterBU.x, wheelMountBU.y, wheelMountBU.z };

	b3Pos wheelCenter = WheelAuditPointBU( config, wheelCenterBU, wheelCenterBU );
	b3Pos wheelMount = WheelAuditPointBU( config, wheelMountBU, wheelCenterBU );
	b3Pos radiusOuter = WheelAuditPointBU( config, radiusOuterBU, wheelCenterBU );
	b3Pos widthLeft = WheelAuditPointBU( config, widthLeftBU, wheelCenterBU );
	b3Pos widthRight = WheelAuditPointBU( config, widthRightBU, wheelCenterBU );
	b3Pos spinA = WheelAuditPointBU( config, spinABU, wheelCenterBU );
	b3Pos spinB = WheelAuditPointBU( config, spinBBU, wheelCenterBU );

	DrawCross( wheelCenter, 0.10f, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
	DrawCross( wheelMount, 0.08f, MakeVec4( 0.1f, 0.8f, 1.0f, 1.0f ) );
	DrawCross( radiusOuter, 0.08f, MakeVec4( 1.0f, 0.85f, 0.1f, 1.0f ) );
	DrawCross( widthLeft, 0.08f, MakeVec4( 0.2f, 0.4f, 1.0f, 1.0f ) );
	DrawCross( widthRight, 0.08f, MakeVec4( 0.2f, 0.4f, 1.0f, 1.0f ) );
	DrawCross( spinA, 0.07f, MakeVec4( 0.2f, 1.0f, 0.2f, 1.0f ) );
	DrawCross( spinB, 0.07f, MakeVec4( 0.2f, 1.0f, 0.2f, 1.0f ) );
	DrawLine( wheelCenter, radiusOuter, MakeVec4( 1.0f, 0.85f, 0.1f, 1.0f ) );
	DrawLine( widthLeft, widthRight, MakeVec4( 0.2f, 0.4f, 1.0f, 1.0f ) );
	DrawLine( spinA, spinB, MakeVec4( 0.2f, 1.0f, 0.2f, 1.0f ) );

	b3Vec3 suspensionWheelCenterBU = JozzVehicleFindPointOrFallback( metadata, "One_Sided_wheel_mount.gltf", "Socket_WheelCenter",
																	 { -1.1875f, 0.5f, -0.0625f } );
	b3Vec3 travelTopBU = JozzVehicleFindPointOrFallback( metadata, "One_Sided_wheel_mount.gltf",
														 "Axis_SuspensionTravel_Top", { -1.1875f, 1.5f, 0.0f } );
	b3Vec3 travelBottomBU = JozzVehicleFindPointOrFallback( metadata, "One_Sided_wheel_mount.gltf",
															"Axis_SuspensionTravel_Bottom", { -1.1875f, -0.5f, 0.0f } );

	b3Pos suspensionWheelCenter = SuspensionAuditPointBU( config, suspensionWheelCenterBU, travelTopBU );
	b3Pos travelTop = SuspensionAuditPointBU( config, travelTopBU, travelTopBU );
	b3Pos travelBottom = SuspensionAuditPointBU( config, travelBottomBU, travelTopBU );
	DrawCross( suspensionWheelCenter, 0.10f, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
	DrawCross( travelTop, 0.08f, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
	DrawCross( travelBottom, 0.08f, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
	DrawLine( travelBottom, travelTop, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
}
