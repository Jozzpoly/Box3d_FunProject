// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// Pure geometry reader for a private P2A "JSPREV2" preview pack: turns the
// content-addressed tile .bin files into collision-ready positions + int32
// triangle indices, one struct per tile.
//
// This translation unit deliberately has NO physics, renderer, texture, or
// ImGui dependency -- it produces only raw geometry. That keeps it usable by
// the scan-drive sample (which DOES create physics) WITHOUT dragging the
// render-only P2A pack reader (jozz_scan_preview_pack.*, physics-excluded by a
// static architecture test) across the physics boundary.
//
// The pack bytes are already leveled and in lab metres (baked offline by the
// P2A pipeline from one verified source revision). This reader does not
// transform geometry. The only option is an OPTIONAL winding flip for the
// single-sided mesh collider -- photogrammetry triangle winding is not
// guaranteed to match Box3D's front-face convention, so the drive sample can
// flip it if the car falls through the surface.

#include "box3d/math_functions.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// One tile's collision geometry: lab-metre positions and a tile-local triangle
// list (group-local indices from the pack are re-based into this tile's single
// contiguous vertex array).
struct JozzScanTileGeometry
{
	int tileId = -1;
	std::vector<b3Vec3> positions; // one per vertex, lab metres
	std::vector<int32_t> indices;  // 3 per triangle, tile-local
	b3AABB bounds = {};
};

// Loads every tile of the pack rooted at packDir (the directory holding
// COMPLETE.json) into out. Returns false and fills error on any structural
// mismatch; out is cleared first. When flipWinding is true each triangle's last
// two indices are swapped, flipping the collider's front face.
bool LoadJozzScanPackGeometry( const std::filesystem::path& packDir, std::vector<JozzScanTileGeometry>* out,
							   std::string* error, bool flipWinding = false );
