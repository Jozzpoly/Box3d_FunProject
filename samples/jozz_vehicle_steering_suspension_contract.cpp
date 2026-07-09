// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_steering_suspension_contract.h"

namespace
{

bool ReadRole( const JozzVehicleAssetContract& contract, const char* role, b3Vec3* out )
{
	const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( contract, role );
	if ( binding == nullptr || binding->resolved == false )
	{
		return false;
	}
	*out = binding->positionMeters;
	return true;
}

} // namespace

JozzVehicleSteeringSuspensionSockets ResolveJozzVehicleSteeringSuspensionSockets( const JozzVehicleAssetContract& contract )
{
	JozzVehicleSteeringSuspensionSockets sockets;
	bool ok = true;
	ok &= ReadRole( contract, "steering_suspension.visual.chassis_mount_a", &sockets.chassisMountA );
	ok &= ReadRole( contract, "steering_suspension.visual.chassis_mount_b", &sockets.chassisMountB );
	ok &= ReadRole( contract, "steering_suspension.visual.wheel_center", &sockets.wheelCenter );
	ok &= ReadRole( contract, "steering_suspension.visual.damper_mount", &sockets.damperMount );
	ok &= ReadRole( contract, "steering_suspension.visual.damper_upper", &sockets.damperUpper );
	ok &= ReadRole( contract, "steering_suspension.visual.damper_lower", &sockets.damperLower );
	ok &= ReadRole( contract, "steering_suspension.visual.steering_rod", &sockets.steeringRod );
	ok &= ReadRole( contract, "steering_suspension.visual.cardan_drive", &sockets.cardanDrive );
	ok &= ReadRole( contract, "steering_suspension.visual.cardan_hub", &sockets.cardanHub );
	ok &= ReadRole( contract, "steering_suspension.travel_axis.top", &sockets.travelAxisTop );
	ok &= ReadRole( contract, "steering_suspension.travel_axis.bottom", &sockets.travelAxisBottom );
	sockets.resolved = ok;
	return sockets;
}

bool ValidateJozzVehicleSteeringSuspensionContract( const JozzVehicleAssetContract& contract, std::vector<std::string>* errors )
{
	errors->clear();
	if ( contract.loaded == false )
	{
		errors->push_back( "steering suspension contract did not load" );
	}

	if ( contract.assetType != "steering_suspension_corner_visual" )
	{
		errors->push_back( "steering suspension contract has unexpected assetType: " + contract.assetType );
	}

	if ( contract.contractVersion < 2 )
	{
		errors->push_back( "steering suspension contract must be v2 or newer" );
	}

	if ( contract.metersPerBlockbenchUnit <= 0.0f )
	{
		errors->push_back( "steering suspension contract scale must be positive" );
	}

	const char* requiredRoles[] = {
		"steering_suspension.visual.chassis_mount_a",
		"steering_suspension.visual.chassis_mount_b",
		"steering_suspension.visual.wheel_center",
		"steering_suspension.visual.damper_mount",
		"steering_suspension.visual.damper_upper",
		"steering_suspension.visual.damper_lower",
		"steering_suspension.visual.steering_rod",
		"steering_suspension.visual.cardan_drive",
		"steering_suspension.visual.cardan_hub",
		"steering_suspension.travel_axis.top",
		"steering_suspension.travel_axis.bottom",
		"steering_suspension.visual.chassis_top",
		"steering_suspension.visual.chassis_bottom",
	};

	for ( const char* role : requiredRoles )
	{
		const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( contract, role );
		if ( binding == nullptr )
		{
			errors->push_back( std::string( "missing required role: " ) + role );
			continue;
		}
		if ( binding->required == false )
		{
			errors->push_back( std::string( "role should be required for v0 rig: " ) + role );
		}
		if ( binding->physicsAuthority )
		{
			errors->push_back( std::string( "visual steering-suspension role must not be physics authority: " ) + role );
		}
		if ( binding->resolved == false )
		{
			errors->push_back( std::string( "role did not resolve against source glTF: " ) + role );
		}
	}

	// Wide-tolerance shape sanity checks (this is a "did the geometry parse at
	// all" check, not a precision check - see the analysis derivation for the
	// authoring-unit numbers this is based on).
	JozzVehicleSteeringSuspensionSockets sockets = ResolveJozzVehicleSteeringSuspensionSockets( contract );
	if ( sockets.resolved )
	{
		float travel = b3Distance( sockets.travelAxisTop, sockets.travelAxisBottom );
		if ( travel < 0.55f || travel > 0.85f )
		{
			errors->push_back( "steering suspension travel axis length is outside expected prototype range" );
		}

		float damperSpan = b3Distance( sockets.damperUpper, sockets.damperLower );
		if ( damperSpan < 0.55f || damperSpan > 0.85f )
		{
			errors->push_back( "steering suspension damper span is outside expected prototype range" );
		}

		float uprightSpan = b3Distance( sockets.wheelCenter, sockets.chassisMountB );
		if ( uprightSpan < 0.10f || uprightSpan > 0.35f )
		{
			errors->push_back( "steering suspension wheel_center-to-chassis_mount_b span is outside expected prototype range" );
		}
	}

	for ( const std::string& error : contract.errors )
	{
		errors->push_back( error );
	}

	return errors->empty();
}
