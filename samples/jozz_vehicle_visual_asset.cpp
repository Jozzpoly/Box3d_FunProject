// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_visual_asset.h"

bool JozzVehicleVisualAsset::LoadFromContract( const JozzVehicleAssetContract& contract )
{
	Destroy();
	assetId = contract.assetId;
	sourcePath = contract.sourcePath;

	if ( contract.loaded == false || contract.sourcePath.empty() )
	{
		status = "visual asset: contract not loaded or source path missing";
		return false;
	}

	if ( mesh.LoadStaticGltf( contract.sourcePath.c_str(), contract.metersPerBlockbenchUnit ) )
	{
		status = "visual asset: loaded from asset contract";
		return true;
	}

	status = "visual asset: failed to load source glTF";
	return false;
}

void JozzVehicleVisualAsset::Destroy()
{
	mesh.Destroy();
}

bool JozzVehicleVisualAsset::IsLoaded() const
{
	return mesh.IsLoaded();
}

void JozzVehicleVisualAsset::DrawAtTransform( b3WorldTransform worldTransform, Vec4 color ) const
{
	mesh.DrawAtTransform( worldTransform, color );
}
