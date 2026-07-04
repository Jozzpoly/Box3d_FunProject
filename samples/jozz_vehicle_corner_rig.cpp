// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_corner_rig.h"

namespace
{

b3Quat SuspensionAuthoringToGameRotation()
{
	// Temporary visual-only correction for the current one-sided suspension proof:
	// authoring +Y stays vertical and authoring +X points along the wheel side.
	return b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
}

bool FindResolvedRole( const JozzVehicleAssetContract& contract, const char* role, const JozzVehicleContractBinding** out )
{
	const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( contract, role );
	if ( binding == nullptr || binding->resolved == false )
	{
		*out = nullptr;
		return false;
	}

	*out = binding;
	return true;
}

} // namespace

JozzVehicleCornerRigState MakeJozzVehicleCornerRigState( b3Pos liveChassisMount, b3Pos liveRestWheelCenter,
														 b3WorldTransform liveWheelBody )
{
	JozzVehicleCornerRigState rig;
	rig.liveChassisMount = liveChassisMount;
	rig.liveRestWheelCenter = liveRestWheelCenter;
	rig.liveWheelBody = liveWheelBody;
	rig.liveWheelTravel = (float)( liveWheelBody.p.y - liveRestWheelCenter.y );
	return rig;
}

b3Pos TransformJozzVehicleContractPoint( b3WorldTransform transform, const JozzVehicleContractBinding& binding )
{
	return b3TransformWorldPoint( transform, binding.positionMeters );
}

JozzVehicleSuspensionVisualFrame MakeJozzVehicleSuspensionVisualFrame( const JozzVehicleAssetContract& contract,
																	   const JozzVehicleCornerRigState& rig )
{
	JozzVehicleSuspensionVisualFrame frame;

	const JozzVehicleContractBinding* wheelCenter = nullptr;
	const JozzVehicleContractBinding* chassisMount = nullptr;
	const JozzVehicleContractBinding* travelTop = nullptr;
	const JozzVehicleContractBinding* travelBottom = nullptr;
	if ( FindResolvedRole( contract, "suspension.visual.wheel_center", &wheelCenter ) == false ||
		 FindResolvedRole( contract, "suspension.visual.chassis_mount", &chassisMount ) == false ||
		 FindResolvedRole( contract, "suspension.travel_axis.top", &travelTop ) == false ||
		 FindResolvedRole( contract, "suspension.travel_axis.bottom", &travelBottom ) == false )
	{
		return frame;
	}

	b3Quat correctionRotation = SuspensionAuthoringToGameRotation();
	b3Vec3 rotatedWheelCenter = b3RotateVector( correctionRotation, wheelCenter->positionMeters );
	b3Vec3 correctionTranslation = b3Sub( b3ToVec3( rig.liveRestWheelCenter ), rotatedWheelCenter );
	frame.mountTransform = { correctionTranslation, correctionRotation };

	frame.contractWheelCenterWorld = TransformJozzVehicleContractPoint( frame.mountTransform, *wheelCenter );
	frame.contractChassisMountWorld = TransformJozzVehicleContractPoint( frame.mountTransform, *chassisMount );
	frame.contractTravelTopWorld = TransformJozzVehicleContractPoint( frame.mountTransform, *travelTop );
	frame.contractTravelBottomWorld = TransformJozzVehicleContractPoint( frame.mountTransform, *travelBottom );
	frame.valid = true;
	return frame;
}
