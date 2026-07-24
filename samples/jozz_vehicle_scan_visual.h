// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// =============================================================================
// SCAN RENDER MESH  (M2 -- the textured skin of the scan island)
// =============================================================================
//
// The render counterpart to jozz_vehicle_scan_geometry (collision). It mirrors
// that module's split: geometry.* reads the pack for a COLLISION mesh (positions
// only, all groups merged, no render/texture deps); this reads the SAME pack for
// a RENDER mesh (position + normal + uv per vertex, ONE textured GPU mesh per
// group, each with its own baseColor PNG).
//
// Why a separate module: the collision reader is deliberately render-free so the
// physics-creating drive sample does not drag the renderer across the physics
// boundary (see jozz_vehicle_scan_geometry.h). This module is the opposite side
// -- render-only, no physics -- so it may depend on the geometry registry, the
// PNG decoder and the draw origin.
//
// The scan pack stores textures PER GROUP (JSPREV2: each tile has N groups, each
// group carries its own <=1024px baseColor PNG and a matching uv channel). So the
// render mesh is 25 small textured meshes, not one -- one AppendMesh per group.
//
// Placement: the caller passes the SAME world origin the collision body was
// created at (JozzScanTilePlacement::origin). Render and collision then overlap
// exactly -- no independent recomputation, no drift.

#include "box3d/math_functions.h" // b3Vec3, b3AABB, b3Vec3_zero
#include "gfx/geometry_registry.h" // MeshHandle, InvalidMeshHandle

#include <filesystem>
#include <string>
#include <vector>

// One registered textured GPU mesh (one scan group), ready to AppendMesh.
struct JozzScanVisualGroup
{
	MeshHandle handle = InvalidMeshHandle();
	int vertexCount = 0;
	int triangleCount = 0;
	bool textured = false; // false == PNG missing/undecodable, drawn solid as a fallback
};

// The whole scan's render skin: one textured mesh per group, all drawn at origin.
struct JozzScanVisual
{
	std::vector<JozzScanVisualGroup> groups;
	b3Vec3 origin = b3Vec3_zero; // world position of the scan local origin (== collision placement)
	b3AABB worldBounds = {};	 // render AABB in world space (origin applied)
	int totalVertices = 0;
	int totalTriangles = 0;
	int textureCount = 0;	  // groups whose PNG decoded and uploaded
	long long textureBytes = 0; // decoded RGBA8 bytes uploaded to the GPU
	bool loaded = false;
	std::string status; // human-readable outcome for the UI
};

// Load the pack's render geometry from packDir (the directory holding
// COMPLETE.json). For each group: build position+normal+uv vertices and
// group-local indices from the tile .bin, decode its baseColor PNG, and register
// one textured GPU mesh. `origin` MUST be the collision placement origin so the
// skin sits exactly on the collider. flipWinding swaps each triangle's last two
// indices (photogrammetry winding is not guaranteed to match the renderer's
// front-face / back-cull convention). Returns false and fills error on any
// structural mismatch; *out is left not-loaded.
bool LoadJozzScanVisual( const std::filesystem::path& packDir, b3Vec3 origin, bool flipWinding, JozzScanVisual* out,
						 std::string* error );

// Append every group to this frame's textured-mesh draw list, at the scan origin
// (demoted against the current draw origin, like JozzVehicleVisualMesh does).
// No-op when not loaded. Call once per frame from Render().
void DrawJozzScanVisual( const JozzScanVisual& visual );

// Release all registered GPU meshes and reset to not-loaded. Safe on empty.
void DestroyJozzScanVisual( JozzScanVisual* visual );
