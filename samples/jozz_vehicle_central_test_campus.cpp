// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_central_test_campus.h"

#include <cmath>

namespace
{

constexpr JozzTestStationSpec kStations[] = {
	// Expanded first-pass budgets after the skeleton review. The central core
	// and the outer loop remain clear; the usable station blocks grow toward
	// the tile edges so later micro-stations have room without touching spawn.
	{ JozzCampusStationId::NorthComfort, "N Komfort i rytm", { 0.0f, 38.0f }, 0.0f,
	  { 46.0f, 14.0f }, 10.0f, 10.0f, 8.0f, 12.0f, JozzCampusDirection::WestToEast, true },
	{ JozzCampusStationId::WestDrift, "W Drift / banki", { -34.0f, 0.0f }, -90.0f,
	  { 22.0f, 18.0f }, 10.0f, 10.0f, 2.0f, 6.0f, JozzCampusDirection::SouthToNorth, false },
	{ JozzCampusStationId::EastTraction, "E Teren punktowy", { 34.0f, 0.0f }, 90.0f,
	  { 22.0f, 18.0f }, 10.0f, 10.0f, 2.0f, 6.0f, JozzCampusDirection::NorthToSouth, false },
	{ JozzCampusStationId::SouthImpact, "S Uderzenie i niski lot", { 0.0f, -38.0f }, 0.0f,
	  { 46.0f, 14.0f }, 10.0f, 10.0f, 4.0f, 8.0f, JozzCampusDirection::WestToEast, true },
};

constexpr JozzRockIslandSpec kRockIslands[] = {
	{ "E1 Gravel Island", { 34.0f, -14.0f }, 90.0f, 8.0f, 14.0f, 3, 31, 2.4f, 0.16f, 0.34f, 910u },
	{ "E2 Mixed Rock Island", { 34.0f, 0.0f }, 90.0f, 8.0f, 14.0f, 4, 32, 2.5f, 0.20f, 0.42f, 920u },
	{ "E3 Heavy Boulder Island", { 34.0f, 14.0f }, 90.0f, 8.0f, 14.0f, 5, 36, 2.7f, 0.24f, 0.52f, 930u },
};

constexpr JozzBumperBankSpec kBumperBanks[] = {
	{ "N1 low rhythm", { -34.0f, 38.0f }, 0.0f, 9, 1.2f, 0.09f, 8.0f, 0.030f, 0.0f, kJozzBumperFullWidth },
	{ "N2 alternating", { -12.0f, 38.0f }, 0.0f, 14, 1.0f, 0.09f, 8.0f, 0.035f, 2.2f, kJozzBumperAlternatingSides },
	{ "N3 wave", { 12.0f, 38.0f }, 0.0f, 10, 1.4f, 0.12f, 8.0f, 0.045f, 2.0f, kJozzBumperWave },
	{ "N4 return rhythm", { 32.0f, 38.0f }, 0.0f, 8, 1.6f, 0.10f, 8.0f, 0.032f, 0.0f, kJozzBumperFullWidth },
	{ "W1 alternating", { -34.0f, -12.0f }, -90.0f, 10, 0.8f, 0.08f, 7.0f, 0.028f, 1.7f, kJozzBumperAlternatingSides },
	{ "W2 cross rhythm", { -34.0f, 0.0f }, -90.0f, 14, 1.0f, 0.10f, 7.0f, 0.035f, 2.0f, kJozzBumperWave },
	{ "W3 low exit", { -34.0f, 12.0f }, -90.0f, 10, 1.0f, 0.11f, 7.0f, 0.040f, 1.6f, kJozzBumperAlternatingSides },
	{ "E1 outside rhythm", { 22.0f, 0.0f }, 90.0f, 12, 1.0f, 0.09f, 6.0f, 0.030f, 1.5f, kJozzBumperWave },
	{ "E2 outside exit", { 46.0f, 0.0f }, 90.0f, 10, 1.0f, 0.10f, 6.0f, 0.034f, 1.5f, kJozzBumperAlternatingSides },
	{ "S1 approach", { -34.0f, -38.0f }, 0.0f, 12, 1.0f, 0.10f, 8.0f, 0.032f, 2.0f, kJozzBumperWave },
	{ "S2 alternating", { -10.0f, -38.0f }, 0.0f, 16, 0.9f, 0.08f, 8.0f, 0.025f, 2.2f, kJozzBumperAlternatingSides },
	{ "S3 medium", { 16.0f, -38.0f }, 0.0f, 12, 1.2f, 0.12f, 8.0f, 0.045f, 0.0f, kJozzBumperFullWidth },
	{ "S4 return", { 34.0f, -38.0f }, 0.0f, 10, 1.3f, 0.10f, 8.0f, 0.032f, 2.0f, kJozzBumperAlternatingSides },
};

struct AABB
{
	float minX, maxX, minZ, maxZ;
};

AABB RotatedFootprint( const JozzTestStationSpec& spec, bool includeApproachRunoff )
{
	float hx = spec.footprintHalfExtents.x;
	float hz = spec.footprintHalfExtents.y;
	if ( includeApproachRunoff )
	{
		// Approach and runoff extend from opposite ends of the same local-X
		// footprint. For an AABB envelope the conservative symmetric half-size
		// is the larger one, not their sum.
		hx += std::fmax( spec.approachLength, spec.runoffLength );
	}

	float angle = spec.yawDegrees * B3_PI / 180.0f;
	float c = std::fabs( std::cos( angle ) );
	float s = std::fabs( std::sin( angle ) );
	float worldHX = c * hx + s * hz;
	float worldHZ = s * hx + c * hz;
	return { spec.centerXZ.x - worldHX, spec.centerXZ.x + worldHX, spec.centerXZ.y - worldHZ,
			 spec.centerXZ.y + worldHZ };
}

bool Overlap( const AABB& a, const AABB& b )
{
	return a.minX < b.maxX && a.maxX > b.minX && a.minZ < b.maxZ && a.maxZ > b.minZ;
}

void AddError( std::vector<std::string>* errors, const std::string& message )
{
	if ( errors != nullptr )
	{
		errors->push_back( message );
	}
}

} // namespace

const JozzTestStationSpec* GetCentralCampusStationSpecs()
{
	return kStations;
}

int GetCentralCampusStationSpecCount()
{
	return (int)( sizeof( kStations ) / sizeof( kStations[0] ) );
}

const JozzRockIslandSpec* GetCentralCampusRockIslandSpecs()
{
	return kRockIslands;
}

int GetCentralCampusRockIslandSpecCount()
{
	return (int)( sizeof( kRockIslands ) / sizeof( kRockIslands[0] ) );
}

const JozzBumperBankSpec* GetCentralCampusBumperBankSpecs()
{
	return kBumperBanks;
}

int GetCentralCampusBumperBankSpecCount()
{
	return (int)( sizeof( kBumperBanks ) / sizeof( kBumperBanks[0] ) );
}

bool ValidateCentralCampusLayout( std::vector<std::string>* errors )
{
	if ( errors != nullptr )
	{
		errors->clear();
	}

	const AABB usable = { -kCentralCampusTileHalfExtent, kCentralCampusTileHalfExtent,
		- kCentralCampusTileHalfExtent, kCentralCampusTileHalfExtent };
	const AABB core = { -kCentralCampusCoreHalfExtent, kCentralCampusCoreHalfExtent,
		- kCentralCampusCoreHalfExtent, kCentralCampusCoreHalfExtent };

	for ( int i = 0; i < GetCentralCampusStationSpecCount(); ++i )
	{
		const JozzTestStationSpec& spec = kStations[i];
		AABB footprint = RotatedFootprint( spec, false );
		AABB travelEnvelope = RotatedFootprint( spec, true );

		if ( footprint.minX < usable.minX || footprint.maxX > usable.maxX || footprint.minZ < usable.minZ ||
			 footprint.maxZ > usable.maxZ )
		{
			AddError( errors, std::string( spec.name ) + ": footprint leaves usable central tile" );
		}
		if ( Overlap( footprint, core ) )
		{
			AddError( errors, std::string( spec.name ) + ": footprint enters Central Core" );
		}
		if ( spec.approachLength < 8.0f || spec.runoffLength < 8.0f )
		{
			AddError( errors, std::string( spec.name ) + ": approach/runoff below 8 m" );
		}
		if ( spec.bidirectional && ( spec.approachLength < 8.0f || spec.runoffLength < 8.0f ) )
		{
			AddError( errors, std::string( spec.name ) + ": bidirectional station lacks both runoffs" );
		}
		if ( travelEnvelope.minX < usable.minX || travelEnvelope.maxX > usable.maxX || travelEnvelope.minZ < usable.minZ ||
			 travelEnvelope.maxZ > usable.maxZ )
		{
			AddError( errors, std::string( spec.name ) + ": approach/runoff leaves usable central tile" );
		}
	}

	for ( int i = 0; i < GetCentralCampusStationSpecCount(); ++i )
	{
		for ( int j = i + 1; j < GetCentralCampusStationSpecCount(); ++j )
		{
			if ( Overlap( RotatedFootprint( kStations[i], false ), RotatedFootprint( kStations[j], false ) ) )
			{
				AddError( errors, std::string( kStations[i].name ) + " overlaps " + kStations[j].name );
			}
		}
	}

	return errors == nullptr || errors->empty();
}

bool ValidateCentralCampusContent( std::vector<std::string>* errors )
{
	if ( errors != nullptr )
	{
		errors->clear();
	}

	if ( GetCentralCampusRockIslandSpecCount() != 3 )
	{
		AddError( errors, "central campus must contain exactly 3 rock islands" );
	}

	int plannedRockShapes = 0;
	for ( int i = 0; i < GetCentralCampusRockIslandSpecCount(); ++i )
	{
		const JozzRockIslandSpec& island = kRockIslands[i];
		if ( island.clusterCount < 2 || island.rocksPerCluster < 20 || island.minSize <= 0.0f ||
			 island.maxSize < island.minSize )
		{
			AddError( errors, std::string( island.id ) + ": invalid island density or size range" );
		}
		plannedRockShapes += island.clusterCount * island.rocksPerCluster;
	}
	if ( plannedRockShapes < 400 )
	{
		AddError( errors, "rock island plan is below the dense-map budget" );
	}

	if ( GetCentralCampusBumperBankSpecCount() < 10 )
	{
		AddError( errors, "bumper plan needs at least 10 independent banks" );
	}
	int plannedBumperShapes = 0;
	for ( int i = 0; i < GetCentralCampusBumperBankSpecCount(); ++i )
	{
		const JozzBumperBankSpec& bank = kBumperBanks[i];
		if ( bank.count < 6 || bank.spacing <= 0.0f || bank.radius <= 0.0f || bank.width <= 0.0f )
		{
			AddError( errors, std::string( bank.id ) + ": invalid bumper bank dimensions" );
		}
		if ( bank.centerY <= 0.0f || bank.centerY + bank.radius > 0.22f || bank.centerY - bank.radius < -0.16f )
		{
			AddError( errors, std::string( bank.id ) + ": bumper contact profile is too high or too deeply embedded" );
		}
		plannedBumperShapes += bank.count;
	}
	if ( plannedBumperShapes < 130 )
	{
		AddError( errors, "bumper plan is below the dense-map budget" );
	}
	return errors == nullptr || errors->empty();
}
