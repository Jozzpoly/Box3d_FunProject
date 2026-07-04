// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"
#include "jozz_vehicle_asset_metadata.h"

struct JozzVehicleSemanticPreviewConfig
{
	const JozzVehicleAuditMetadata* metadata;
	float metersPerBlockbenchUnit;
	b3Pos wheelPosition;
	b3Pos wheelPreviewOrigin;
	b3Pos suspensionPreviewOrigin;
};

void DrawJozzVehicleSemanticPreview( const JozzVehicleSemanticPreviewConfig& config );
