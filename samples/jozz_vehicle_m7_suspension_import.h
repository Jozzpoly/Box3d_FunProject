// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M7 suspension import: fills rig hardpoint structs from asset sidecar
// contracts. This is the first real "importer feeds the physics" path the
// hardpoint architecture was built for: the rig module keeps consuming plain
// structs and never learns where the numbers came from.
//
// v1 scope: the trailing-arm geometry of One_Sided_wheel_mount. The contract
// resolves socket positions against the source glTF (M4 contract runtime);
// this helper turns them into chassis-local offsets from the rest wheel
// center, with the ADR-0002 temporary orientation correction: the authored
// arm direction is yawed onto chassis +X so the pivot trails correctly
// regardless of how the model was oriented in Blockbench. Final orientation
// authority stays with Jozz's visual review, per ADR-0002.

#include "jozz_vehicle_m6_suspension_rig.h"

#include <string>

struct JozzVehicleM7TrailingArmImport
{
	bool ok = false;
	std::string status; // human-readable summary for the HUD/CLI, includes the applied yaw correction
	JozzVehicleM6TrailingArmGeometry geometry = JozzVehicleM6DefaultTrailingArmGeometry();
};

// Loads the sidecar contract (bare file name, resolved like the other asset
// loads) and builds the trailing-arm hardpoints. On any failure the result
// keeps the built-in default geometry and ok = false, so callers can always
// use result.geometry.
JozzVehicleM7TrailingArmImport LoadJozzVehicleM7TrailingArmGeometry( const char* contractFileName );
