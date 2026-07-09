// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "jozz_vehicle_asset_contract.h"

#include "box3d/math_functions.h"

#include <string>
#include <vector>

// Resolved authored-world-meters positions of every socket in
// one_sided_steering_suspension.asset.json, read from the contract's
// bindings. This is the new one-sided front (steering) rig - a separate model
// from one_sided_wheel_mount.gltf, NOT a drop-in replacement, see the
// contract's "notes" block for the parenting differences (chassis_mount_b and
// wheel_center ride the KNUCKLE despite one being named "ChassisMount";
// damper_lower rides the lower wishbone arm, not the knuckle).
struct JozzVehicleSteeringSuspensionSockets
{
	bool resolved = false;
	b3Vec3 chassisMountA = b3Vec3_zero;
	b3Vec3 chassisMountB = b3Vec3_zero;
	b3Vec3 wheelCenter = b3Vec3_zero;
	b3Vec3 damperMount = b3Vec3_zero;
	b3Vec3 damperUpper = b3Vec3_zero;
	b3Vec3 damperLower = b3Vec3_zero;
	b3Vec3 steeringRod = b3Vec3_zero;
	b3Vec3 cardanDrive = b3Vec3_zero;
	b3Vec3 cardanHub = b3Vec3_zero;
	b3Vec3 travelAxisTop = b3Vec3_zero;
	b3Vec3 travelAxisBottom = b3Vec3_zero;
};

// Reads every socket role out of an already-loaded contract. `resolved` is
// true only if every socket bound successfully; callers should fall back to
// hardcoded defaults (like the rest of this codebase does for
// one_sided_wheel_mount) if this is false rather than trusting zeroed fields.
JozzVehicleSteeringSuspensionSockets ResolveJozzVehicleSteeringSuspensionSockets( const JozzVehicleAssetContract& contract );

// Validates the contract structurally (required roles present, resolved,
// visual-only, positive scale) and sanity-checks a few derived distances
// against the known authored geometry (wide tolerances - this is a load/shape
// check, not a precision check). Mirrors
// ValidateJozzVehicleSuspensionCornerContract's shape for the OLD
// one_sided_wheel_mount contract, but with this model's own role set - the
// two contracts are intentionally not interchangeable.
bool ValidateJozzVehicleSteeringSuspensionContract( const JozzVehicleAssetContract& contract, std::vector<std::string>* errors );
