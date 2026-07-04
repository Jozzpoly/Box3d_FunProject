// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"

#include <string>
#include <vector>

struct JozzVehicleAuditSemanticPoint
{
	std::string assetFile;
	std::string name;
	b3Vec3 positionBU;
};

struct JozzVehicleAuditMetadata
{
	bool loadedFromRuntimeReport = false;
	std::string sourcePath;
	std::string status;
	std::vector<JozzVehicleAuditSemanticPoint> semanticPoints;
};

JozzVehicleAuditMetadata LoadJozzVehicleAuditMetadata();

bool FindJozzVehicleSemanticPoint( const JozzVehicleAuditMetadata& metadata, const char* assetFile, const char* semanticName,
								  b3Vec3* outPositionBU );

// Canonical built-in fallback for a semantic point. This is the same table the
// metadata fallback path is populated from, so callers that need a literal
// fallback vector do not keep their own copy. Returns b3Vec3_zero for unknown
// points.
b3Vec3 JozzVehicleBuiltInSemanticPoint( const char* assetFile, const char* semanticName );

// Metadata lookup that falls back to the built-in table on a miss.
b3Vec3 JozzVehicleFindPointOrBuiltIn( const JozzVehicleAuditMetadata& metadata, const char* assetFile,
									  const char* semanticName );
