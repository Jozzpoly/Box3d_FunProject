// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_central_test_campus.h"

#include "jozz_vehicle_obstacle_kit.h"

void BuildCentralTestCampus( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits, uint32_t terrainSeed )
{
	// All content is data-driven. This keeps the course thin and makes the
	// validator inspect the same density/profile contract that the builder uses.
	const JozzRockIslandSpec* islands = GetCentralCampusRockIslandSpecs();
	for ( int i = 0; i < GetCentralCampusRockIslandSpecCount(); ++i )
	{
		const JozzRockIslandSpec& spec = islands[i];
		AddRockIsland( worldId, { spec.centerXZ.x, groundTopY, spec.centerXZ.y }, spec.yawDegrees, spec.lengthX,
					   spec.widthZ, spec.clusterCount, spec.rocksPerCluster, spec.clusterRadius, spec.minSize, spec.maxSize,
					   terrainCategoryBits, terrainSeed + spec.seedOffset );
	}

	const JozzBumperBankSpec* banks = GetCentralCampusBumperBankSpecs();
	for ( int i = 0; i < GetCentralCampusBumperBankSpecCount(); ++i )
	{
		const JozzBumperBankSpec& spec = banks[i];
		AddBumperBank( worldId, { spec.centerXZ.x, groundTopY, spec.centerXZ.y }, spec.yawDegrees, spec.count,
					   spec.spacing, spec.radius, spec.width, spec.centerY, spec.sideOffset, spec.pattern,
					   terrainCategoryBits );
	}
	// E2R.4b W articulation/off-camber was rejected in visual review. The
	// obstacle-kit generators remain available, but this central builder must
	// not activate the ambiguous W slice again.
}
