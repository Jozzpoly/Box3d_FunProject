// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m7_suspension_import.h"

#include "jozz_vehicle_asset_contract.h"

#include <cmath>
#include <cstdio>

namespace
{

bool ResolveRolePosition( const JozzVehicleAssetContract& contract, const char* role, b3Vec3* outMeters,
						  std::string* missing )
{
	const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( contract, role );
	if ( binding == nullptr || binding->resolved == false )
	{
		if ( missing->empty() == false )
		{
			*missing += ", ";
		}
		*missing += role;
		return false;
	}

	*outMeters = binding->positionMeters;
	return true;
}

// Yaw about +Y by the angle that maps the authored horizontal direction onto
// chassis +X: (x,z) -> (x*c + z*s, -x*s + z*c) with c/s from the direction.
b3Vec3 YawOntoForward( b3Vec3 v, float c, float s )
{
	return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

} // namespace

JozzVehicleM7TrailingArmImport LoadJozzVehicleM7TrailingArmGeometry( const char* contractFileName )
{
	JozzVehicleM7TrailingArmImport result;

	JozzVehicleAssetContract contract = LoadJozzVehicleAssetContract( contractFileName );
	if ( contract.loaded == false )
	{
		result.status = "trailing arm import: contract not loaded (" + contract.status + "), using built-in geometry";
		return result;
	}

	std::string missing;
	b3Vec3 pivot = b3Vec3_zero;
	b3Vec3 wheelCenter = b3Vec3_zero;
	b3Vec3 damperUpperR = b3Vec3_zero;
	b3Vec3 damperUpperL = b3Vec3_zero;
	b3Vec3 damperLowerR = b3Vec3_zero;
	b3Vec3 damperLowerL = b3Vec3_zero;

	bool resolved = true;
	resolved &= ResolveRolePosition( contract, "suspension.visual.chassis_mount", &pivot, &missing );
	resolved &= ResolveRolePosition( contract, "suspension.visual.wheel_center", &wheelCenter, &missing );
	resolved &= ResolveRolePosition( contract, "suspension.visual.damper_upper_r", &damperUpperR, &missing );
	resolved &= ResolveRolePosition( contract, "suspension.visual.damper_upper_l", &damperUpperL, &missing );
	resolved &= ResolveRolePosition( contract, "suspension.visual.damper_lower_r", &damperLowerR, &missing );
	resolved &= ResolveRolePosition( contract, "suspension.visual.damper_lower_l", &damperLowerL, &missing );

	if ( resolved == false )
	{
		result.status = "trailing arm import: unresolved roles (" + missing + "), using built-in geometry";
		return result;
	}

	// Offsets from the wheel center in authoring axes (already in meters).
	b3Vec3 pivotDelta = b3Sub( pivot, wheelCenter );
	b3Vec3 damperChassisDelta = b3Sub( b3MulSV( 0.5f, b3Add( damperUpperR, damperUpperL ) ), wheelCenter );
	b3Vec3 damperArmDelta = b3Sub( b3MulSV( 0.5f, b3Add( damperLowerR, damperLowerL ) ), wheelCenter );

	// ADR-0002 temporary orientation correction: whatever direction the arm
	// was authored in, yaw everything so the pivot sits along chassis +X
	// (a trailing arm trails its hinge). The same yaw hits every point, so
	// the geometry stays rigid.
	float horizontal = std::sqrt( pivotDelta.x * pivotDelta.x + pivotDelta.z * pivotDelta.z );
	if ( horizontal < 0.15f )
	{
		result.status = "trailing arm import: pivot sits on top of the wheel center (arm too short), using built-in geometry";
		return result;
	}

	float c = pivotDelta.x / horizontal;
	float s = pivotDelta.z / horizontal;
	float appliedYawDeg = 180.0f / B3_PI * std::atan2( s, c );

	result.geometry.pivotOffset = YawOntoForward( pivotDelta, c, s );
	result.geometry.damperChassisOffset = YawOntoForward( damperChassisDelta, c, s );
	result.geometry.damperArmOffset = YawOntoForward( damperArmDelta, c, s );
	result.geometry.armMass = JozzVehicleM6DefaultTrailingArmGeometry().armMass;
	result.geometry.loadedFromContract = true;
	result.ok = true;

	char summary[256];
	std::snprintf( summary, sizeof( summary ),
				   "trailing arm import: %s, pivot {%.2f, %.2f, %.2f} m, damper rest %.2f m, yaw correction %.0f deg",
				   contract.assetId.c_str(), result.geometry.pivotOffset.x, result.geometry.pivotOffset.y,
				   result.geometry.pivotOffset.z,
				   b3Length( b3Sub( result.geometry.damperChassisOffset, result.geometry.damperArmOffset ) ),
				   appliedYawDeg );
	result.status = summary;
	return result;
}
