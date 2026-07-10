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

// IN-PLACE partial load: keys present in the file overwrite fields of
// *outConfig, keys absent leave the CURRENT value untouched. This is the
// right semantics for exactly ONE caller: the session auto-save, which is
// always written complete by the destructor over a freshly-defaulted config.
// It is the WRONG semantics for presets - see LoadJozzVehicleM6PresetConfig.
bool LoadJozzVehicleM6Config( const std::string& path, JozzVehicleM6Config* outConfig );

// DETERMINISTIC preset load: the result is factoryDefaults overridden by the
// keys present in the file - independent of whatever config is currently
// active. Presets are deliberately PARTIAL files ("only what makes this
// setup different"), so loading one in place would silently inherit every
// leftover experimental slider value for the keys it doesn't set. Jozz hit
// exactly that on 2026-07-09: fiddle with dials -> load a preset expecting a
// full restore -> the preset "kept" the experiments (and the session
// auto-save then persisted them across restarts). RULE: any load path that
// presents itself to the user as "restore a saved setup" MUST go through
// this function, never the in-place one. Guarded by the validator's
// preset-determinism probe.
bool LoadJozzVehicleM6PresetConfig( const std::string& path, const JozzVehicleM6Config& factoryDefaults,
									JozzVehicleM6Config* outConfig );

// Lists preset names (filename without ".json") found in directoryPath,
// sorted alphabetically. Empty if the directory doesn't exist yet.
std::vector<std::string> ListJozzVehicleM6Presets( const std::string& directoryPath );
