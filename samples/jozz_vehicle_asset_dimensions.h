// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"
#include "jozz_vehicle_asset_metadata.h"

struct JozzVehiclePrimitiveDefaults
{
	float metersPerBlockbenchUnit;
	float wheelRadius;
	float wheelWidth;
	float assetSuspensionTravelHint;
	float reboundTravel;
	float compressionTravel;
	float chassisHalfHeight;
	float rigHeight;
	float restDrop;
};

JozzVehiclePrimitiveDefaults GetJozzVehicleM3ADefaults( const JozzVehicleAuditMetadata& metadata );
