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
