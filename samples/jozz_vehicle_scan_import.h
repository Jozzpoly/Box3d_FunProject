// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// =============================================================================
// SCAN IMPORT CONTRACT  (fundament v3 -- docs/PLAN_FUNDAMENT_SKANU_v3_KONTRAKT_PL.md)
// =============================================================================
//
// This is the DURABLE SEAM for bringing photogrammetry scans into the world.
// Everything ABOVE it (where a mesh's role comes from -- a separate GLB file, a
// material name, a submesh name) and everything BELOW it (how a collider is
// built) may change independently. The seam itself does not.
//
// The engine CONSUMES roles; it does not assign them. For the current dirty
// scan there is exactly one role (Terrain). For future CLEANED scans Jozz will
// export meshes already separated by role (his external workflow), and pass
// many ScanMeshInput with different roles -- WITHOUT changing BuildScanTile.
// The in-engine auto-classifier explored in plan v2 is deliberately gone: it
// was "the automat we will not use" (Jozz, 2026-07-24).
//
// Physics anchor: role -> collision category. Box3D's split-wheel envelope
// (jozz_vehicle_m6_suspension_rig.h) already keys on this: the rolling sphere
// masks to JOZZ_M6_TERRAIN_CATEGORY (0x2), the sidewall to ~terrain. That is
// the ONLY place "what is ground" enters the simulation -- so role is the
// right seam, and classifying inside the engine buys nothing.

#include "box3d/box3d.h" // b3WorldId, b3BodyId, b3Vec3, b3AABB, b3MeshDef, b3CreateMesh

#include "jozz_vehicle_scan_geometry.h" // JozzScanTileGeometry -- the M0 source

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Roles. A role decides collision CATEGORY and collider SHAPE. Only Terrain is
// the accurate driving surface; everything else is a simplified obstacle.
// -----------------------------------------------------------------------------
enum class JozzScanRole
{
	// Accurate triangle mesh (b3CreateMesh, identifyEdges = true), category =
	// JOZZ_M6_TERRAIN_CATEGORY (0x2). This is what the wheels roll on. ONLY this
	// role gets 0x2 -- tagging a whole scan as terrain breaks the split wheel
	// envelope (PoC defect D1).
	Terrain,

	// House / wall / building. Simplified collision: b3CreateHull or
	// b3CreateCompound, category = JOZZ_M6_OBJECT_CATEGORY (0x1). The car bounces
	// off, it does not drive up.
	Structure,

	// Tree / bush. Category 0x1. collide == true -> a hull of the trunk only;
	// collide == false -> render-only (no invisible wall in the air where the
	// fuzzy photogrammetry canopy was).
	Vegetation,

	// Render only, never collides.
	Decoration,
};

// -----------------------------------------------------------------------------
// One role-tagged mesh handed to the importer. Format-agnostic on purpose: the
// same struct is filled from the JSPREV2 reader today and from a glTF reader
// later. Memory is borrowed (caller owns it for the duration of BuildScanTile).
// -----------------------------------------------------------------------------
struct JozzScanMeshInput
{
	const b3Vec3* vertices = nullptr;  // one per vertex, tile-local metres
	const int32_t* indices = nullptr;  // 3 per triangle, into `vertices`
	int vertexCount = 0;               // must be >= 3 for a collider
	int triangleCount = 0;             // must be >= 1 for a collider

	JozzScanRole role = JozzScanRole::Terrain;
	bool collide = true; // Vegetation: trunk hull (true) vs render-only (false)
};

// -----------------------------------------------------------------------------
// Where the scan tile stands in the world. Chosen once, persisted world-space,
// survives reimport of a better scan into the same spot (teleports + Jozz's
// manual work do not move). This scan: NORTH of the plate (z > 200), reached by
// TELEPORT only, its footprint edges are cliffs (island, decision D4/D5).
// -----------------------------------------------------------------------------
struct JozzScanTilePlacement
{
	b3Vec3 origin = { 0.0f, 0.0f, 0.0f }; // world position of the scan's local origin
	// (rotation/scale intentionally omitted for M0 -- the pack is already leveled
	// in lab metres; add only if a real scan needs it.)
};

// -----------------------------------------------------------------------------
// What BuildScanTile produced. One body per role bucket (Terrain merges ALL its
// input meshes into a SINGLE b3CreateMesh so identifyEdges sees one continuous
// surface -- shared-edge adjacency only works within one mesh, so splitting
// terrain per source-tile/material would break concave-edge flags at every seam,
// which was PoC defect D2b).
// -----------------------------------------------------------------------------
struct JozzScanTileBodies
{
	b3BodyId terrainBody = b3_nullBodyId;
	b3MeshData* terrainMesh = nullptr; // kept alive for the shape; freed by DestroyJozzScanTile
	std::vector<b3BodyId> structureBodies;
	std::vector<b3BodyId> vegetationBodies;
	b3AABB worldBounds = {};       // scan AABB in WORLD space (origin already applied)
	int terrainVertexCount = 0;
	int terrainTriangleCount = 0;
	int deferredMeshCount = 0;     // non-Terrain inputs skipped this milestone (M4 does them)
	bool ok = false;
	std::string status;            // human-readable outcome for the UI
};

// -----------------------------------------------------------------------------
// THE seam. Builds colliders per role and places the tile in the world.
//   - Terrain inputs are merged into one static body / one b3CreateMesh, edges
//     identified, category = JOZZ_M6_TERRAIN_CATEGORY.
//   - Structure/Vegetation inputs each become a simplified static collider,
//     category = JOZZ_M6_OBJECT_CATEGORY (Vegetation with collide==false skipped).
//   - Decoration inputs are ignored here (renderer's job).
// terrainCategoryBits is passed in (not hard-coded) so labs can override it,
// exactly like CreateJozzWorldGround does.
// -----------------------------------------------------------------------------
JozzScanTileBodies BuildJozzScanTile( b3WorldId worldId, const JozzScanMeshInput* meshes, int meshCount,
									  const JozzScanTilePlacement& placement,
									  uint64_t terrainCategoryBits /* = JOZZ_M6_TERRAIN_CATEGORY */ );

// Convenience adapter for M0: the whole dirty scan is ONE Terrain role. Flattens
// every tile from the reader into a single merged Terrain input and calls
// BuildJozzScanTile. Returns the same result struct.
JozzScanTileBodies BuildJozzScanTileFromPack( b3WorldId worldId, const std::vector<JozzScanTileGeometry>& tiles,
											  const JozzScanTilePlacement& placement, uint64_t terrainCategoryBits );

// Frees the mesh blob and destroys the bodies created by BuildJozzScanTile.
// Safe to call on a zero-initialized / failed result.
void DestroyJozzScanTile( b3WorldId worldId, JozzScanTileBodies* bodies );

// Resolve the private scan pack directory WITHOUT hard-coding a path in Git:
// environment variable JOZZ_SCAN_PREVIEW_PACK wins (points at the directory
// holding COMPLETE.json). Returns an empty path when unset -- the lab then
// stays safely unloaded. (A build/scan_pipeline/previews auto-scan like the
// render preview's can be added later; env keeps M0 simple and private.)
std::filesystem::path FindJozzScanPackDir();
