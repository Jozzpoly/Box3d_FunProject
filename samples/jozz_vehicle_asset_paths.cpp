// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_paths.h"

#include <fstream>

bool FindJozzVehicleAssetFile( const char* relativePath, std::string* resolvedPath )
{
	const char* prefixes[] = {
		"", "../", "../../", "../../../", "../../../../",
	};

	for ( const char* prefix : prefixes )
	{
		std::string candidate = std::string( prefix ) + relativePath;
		std::ifstream probe( candidate, std::ios::binary );
		if ( probe.is_open() )
		{
			*resolvedPath = candidate;
			return true;
		}
	}

	resolvedPath->clear();
	return false;
}
