// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

// Resolves a repo-relative asset path (for example
// "assets/reports/asset_audit_latest.json") against the current working
// directory and a small ladder of parent directories. The samples host and the
// validation CLI are commonly launched from the repo root, build/, or
// build/bin/<Config>/, so the ladder covers those layouts in one place instead
// of each loader keeping its own candidate list.
//
// Returns true and writes the first openable candidate to resolvedPath.
bool FindJozzVehicleAssetFile( const char* relativePath, std::string* resolvedPath );
