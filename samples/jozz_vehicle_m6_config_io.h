// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// Save/load the whole tunable JozzVehicleM6Config to/from a JSON file, so a
// tuning pass (suspension, drive, steering, chassis) can be named, shared, and
// switched back to in seconds instead of re-dragging every slider by hand.
// Two callers:
//   - Named presets under assets/vehicle_presets/*.json (committed, shareable;
//     three ship built-in: uliczny/drift/offroad).
//   - The lab's own auto-saved "last session" file, so the engine's global "R"
//     restart shortcut (which reconstructs the sample from scratch) restores
//     the tuning that was in effect instead of silently reverting to
//     JozzVehicleM6DefaultConfig - restart was resetting hours of tuning with
//     no warning before this existed.
// Unknown/missing keys are left untouched in *outConfig on load (the caller
// pre-fills it, normally from JozzVehicleM6DefaultConfig), so a preset saved
// before a new field was added still loads cleanly instead of zeroing it.

#include "jozz_vehicle_m6_suspension_rig.h"

#include <string>
#include <vector>

bool SaveJozzVehicleM6Config( const JozzVehicleM6Config& config, const std::string& path );
bool LoadJozzVehicleM6Config( const std::string& path, JozzVehicleM6Config* outConfig );

// Lists preset names (filename without ".json") found in directoryPath,
// sorted alphabetically. Empty if the directory doesn't exist yet.
std::vector<std::string> ListJozzVehicleM6Presets( const std::string& directoryPath );
