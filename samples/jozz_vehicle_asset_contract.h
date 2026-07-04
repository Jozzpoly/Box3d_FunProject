// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"

#include <string>
#include <vector>

struct JozzVehicleContractBinding
{
	std::string path;
	std::string role;
	std::string roleCategory;
	std::string nameHint;
	std::string nodePathHint;
	std::string space;
	int nodeIndexHint = -1;
	bool required = false;
	bool physicsAuthority = false;
	bool resolved = false;
	b3Vec3 positionBU = b3Vec3_zero;
	b3Vec3 positionMeters = b3Vec3_zero;
};

struct JozzVehicleAssetContract
{
	bool loaded = false;
	std::string status;
	std::string contractPath;
	std::string sourcePath;
	std::string assetId;
	std::string assetType;
	std::string orientationStatus;
	int contractVersion = 0;
	float metersPerBlockbenchUnit = 1.0f;
	bool temporaryImporterCorrectionEnabled = false;
	std::vector<JozzVehicleContractBinding> bindings;
	std::vector<std::string> warnings;
	std::vector<std::string> errors;
};

JozzVehicleAssetContract LoadJozzVehicleAssetContract( const char* contractFileName );

const JozzVehicleContractBinding* FindJozzVehicleContractBindingByRole( const JozzVehicleAssetContract& contract,
																		const char* role );

bool ValidateJozzVehicleSuspensionCornerContract( const JozzVehicleAssetContract& contract, std::vector<std::string>* errors );
