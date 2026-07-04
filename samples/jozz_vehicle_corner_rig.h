// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"
#include "jozz_vehicle_asset_contract.h"

struct JozzVehicleCornerRigState
{
	b3Pos liveChassisMount = b3Pos_zero;
	b3Pos liveRestWheelCenter = b3Pos_zero;
	b3WorldTransform liveWheelBody = b3WorldTransform_identity;
	float liveWheelTravel = 0.0f;
};

struct JozzVehicleSuspensionVisualFrame
{
	bool valid = false;
	b3WorldTransform mountTransform = b3WorldTransform_identity;
	b3Pos contractWheelCenterWorld = b3Pos_zero;
	b3Pos contractChassisMountWorld = b3Pos_zero;
	b3Pos contractTravelTopWorld = b3Pos_zero;
	b3Pos contractTravelBottomWorld = b3Pos_zero;
};

JozzVehicleCornerRigState MakeJozzVehicleCornerRigState( b3Pos liveChassisMount, b3Pos liveRestWheelCenter,
														 b3WorldTransform liveWheelBody );

JozzVehicleSuspensionVisualFrame MakeJozzVehicleSuspensionVisualFrame( const JozzVehicleAssetContract& contract,
																	   const JozzVehicleCornerRigState& rig );

b3Pos TransformJozzVehicleContractPoint( b3WorldTransform transform, const JozzVehicleContractBinding& binding );
