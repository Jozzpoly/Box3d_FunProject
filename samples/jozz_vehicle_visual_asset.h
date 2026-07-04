// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"
#include "gfx/utility.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_visual_mesh.h"

#include <string>

struct JozzVehicleVisualAsset
{
	JozzVehicleVisualMesh mesh;
	std::string assetId;
	std::string sourcePath;
	std::string status;

	bool LoadFromContract( const JozzVehicleAssetContract& contract );
	void Destroy();
	bool IsLoaded() const;
	void DrawAtTransform( b3WorldTransform worldTransform, Vec4 color ) const;
};
