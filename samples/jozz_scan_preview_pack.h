// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"
#include "gfx/geometry_registry.h"

#include <filesystem>
#include <string>
#include <vector>

struct JozzScanPreviewTile
{
	int tileId = -1;
	MeshHandle handle = InvalidMeshHandle();
	int vertexCount = 0;
	int triangleCount = 0;
	b3AABB bounds = {};
	bool visible = true;
};

// Render-only consumer for a private P2A source preview pack. The pack contains
// lab-space metres baked offline from one verified source revision. This type
// has no Box3D shape/body API and must never be used as collision or authored
// world truth.
struct JozzScanPreviewPack
{
	std::vector<JozzScanPreviewTile> tiles;
	std::filesystem::path sourcePath;
	std::string status = "preview pack: not loaded";
	std::string sourceRevisionId;
	b3AABB bounds = {};
	int vertexCount = 0;
	int triangleCount = 0;
	bool loaded = false;

	bool Load( const std::filesystem::path& directory );
	void Destroy();
	void Draw( bool showBounds ) const;
};

// Resolve a private preview without storing paths in Git. Environment variable
// JOZZ_SCAN_PREVIEW_PACK wins. Otherwise exactly one complete directory under
// build/scan_pipeline/previews is selected; zero or multiple candidates return
// an empty path and the lab remains safely unloaded.
std::filesystem::path FindJozzActiveScanPreviewPack();
