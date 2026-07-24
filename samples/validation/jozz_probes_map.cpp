// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_central_test_campus.h"
#include "jozz_vehicle_world_layout.h"
// E3 track layout probe removed: track recovered-but-dormant per Jozz 2026-07-24.

#include <cstdio>
#include <vector>

bool RunCentralCampusLayoutProbe()
{
	std::vector<std::string> errors;
	bool ok = ValidateCentralCampusLayout( &errors );
	std::printf( "central campus layout: %d stations, %s\n", GetCentralCampusStationSpecCount(), ok ? "OK" : "FAILED" );
	for ( const std::string& error : errors )
	{
		std::printf( "central campus layout error: %s\n", error.c_str() );
	}
	std::vector<std::string> contentErrors;
	bool contentOk = ValidateCentralCampusContent( &contentErrors );
	std::printf( "central campus content: %d rock islands, %d bumper banks, W contact-profile slice inactive, %s\n",
				 GetCentralCampusRockIslandSpecCount(), GetCentralCampusBumperBankSpecCount(), contentOk ? "OK" : "FAILED" );
	for ( const std::string& error : contentErrors )
	{
		std::printf( "central campus content error: %s\n", error.c_str() );
	}
	ok = ok && contentOk;
	return ok;
}

bool RunMasterplanYardProbe()
{
	bool ok = JozzWorldLayout::kMasterplanYardCount >= 5;
	for ( int i = 0; i < JozzWorldLayout::kMasterplanYardCount; ++i )
	{
		const JozzWorldLayout::JozzWorldYard& yard = JozzWorldLayout::kMasterplanYards[i];
		bool boundsOk = yard.minX < yard.maxX && yard.minZ < yard.maxZ && yard.minX >= -200.0f && yard.maxX <= 200.0f &&
						 yard.minZ >= -200.0f && yard.maxZ <= 200.0f;
		bool outsideCore = yard.maxX <= -60.0f || yard.minX >= 60.0f || yard.maxZ <= -60.0f || yard.minZ >= 60.0f;
		std::printf( "masterplan yard %s: bounds %s, outside C %s\n", yard.name, boundsOk ? "OK" : "BAD",
					 outsideCore ? "OK" : "BAD" );
		ok = ok && boundsOk && outsideCore;
	}
	return ok;
}
