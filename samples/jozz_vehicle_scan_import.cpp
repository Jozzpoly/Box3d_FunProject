// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_scan_import.h"

#include "box3d/box3d.h"
#include "box3d/collision.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

// =============================================================================
// Implements the scan-import contract (jozz_vehicle_scan_import.h). M1 wires the
// Terrain role only: all Terrain inputs merge into ONE b3CreateMesh so shared-edge
// adjacency (identifyEdges) spans the whole surface. Structure / Vegetation are
// counted and skipped this milestone -- shipping untested hull code would violate
// "render is the gate"; they land in M4 with the first real separated scan.
// =============================================================================

namespace
{

// friction of the scan surface. Matches the offroad heightfield (0.85) so the
// car feels the same grip driving from procedural terrain onto the scan.
constexpr float kScanFriction = 0.85f;

// Weld coincident vertices before building the BVH. Photogrammetry tiles are
// reconstructed independently, so a shared physical seam has TWO near-coincident
// vertices, one per tile -- without welding, identifyEdges cannot connect
// triangles across a tile boundary (the whole point of one merged mesh). A small
// tolerance stitches the seam without collapsing real detail (edges are cm-scale,
// see plan v2 measurements).
constexpr float kScanWeldTolerance = 0.01f; // 1 cm

} // namespace

JozzScanTileBodies BuildJozzScanTile( b3WorldId worldId, const JozzScanMeshInput* meshes, int meshCount,
									  const JozzScanTilePlacement& placement, uint64_t terrainCategoryBits )
{
	JozzScanTileBodies result;

	// --- 1. Merge every Terrain input into one contiguous vertex/index array ---
	std::vector<b3Vec3> verts;
	std::vector<int32_t> indices;
	for ( int m = 0; m < meshCount; ++m )
	{
		const JozzScanMeshInput& in = meshes[m];
		if ( in.role != JozzScanRole::Terrain )
		{
			// Structure / Vegetation / Decoration: not this milestone (M4).
			if ( in.role != JozzScanRole::Decoration )
			{
				result.deferredMeshCount += 1;
			}
			continue;
		}
		if ( in.vertices == nullptr || in.indices == nullptr || in.vertexCount < 3 || in.triangleCount < 1 )
		{
			continue;
		}
		const int32_t base = (int32_t)verts.size();
		verts.insert( verts.end(), in.vertices, in.vertices + in.vertexCount );
		indices.reserve( indices.size() + (size_t)in.triangleCount * 3 );
		for ( int i = 0; i < in.triangleCount * 3; ++i )
		{
			indices.push_back( in.indices[i] + base );
		}
	}

	if ( verts.size() < 3 || indices.size() < 3 )
	{
		result.ok = false;
		result.status = "brak trojkatow terenu w wejsciu";
		return result;
	}

	// --- 2. Build the mesh blob (BVH inside). Collect any degenerate triangles ---
	std::vector<int> degenerate( 256 );
	b3MeshDef def = {};
	def.vertices = verts.data();
	def.indices = indices.data();
	def.materialIndices = nullptr; // single surface material for M1 (per-triangle materials: later)
	def.vertexCount = (int)verts.size();
	def.triangleCount = (int)( indices.size() / 3 );
	def.weldVertices = true;
	def.weldTolerance = kScanWeldTolerance;
	def.useMedianSplit = false; // SAH: the scan is irregular, not a grid
	def.identifyEdges = true;   // concave-edge flags across the whole merged surface

	b3MeshData* mesh = b3CreateMesh( &def, degenerate.data(), (int)degenerate.size() );
	if ( mesh == nullptr )
	{
		result.ok = false;
		result.status = "b3CreateMesh zwrocil null";
		return result;
	}

	// --- 3. Static body at the island origin + the mesh shape (Terrain category) ---
	b3BodyDef bodyDef = b3DefaultBodyDef(); // default type == b3_staticBody
	bodyDef.name = "scan_terrain";
	bodyDef.position = placement.origin;
	result.terrainBody = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = kScanFriction;
	shapeDef.filter.categoryBits = terrainCategoryBits; // ONLY Terrain gets this (split wheel envelope)
	b3CreateMeshShape( result.terrainBody, &shapeDef, mesh, { 1.0f, 1.0f, 1.0f } );

	result.terrainMesh = mesh;
	result.terrainVertexCount = def.vertexCount;
	result.terrainTriangleCount = def.triangleCount;

	// --- 4. World-space AABB (local min/max + origin) for teleport framing ---
	b3Vec3 lo = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	b3Vec3 hi = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
	for ( const b3Vec3& v : verts )
	{
		lo.x = v.x < lo.x ? v.x : lo.x;
		lo.y = v.y < lo.y ? v.y : lo.y;
		lo.z = v.z < lo.z ? v.z : lo.z;
		hi.x = v.x > hi.x ? v.x : hi.x;
		hi.y = v.y > hi.y ? v.y : hi.y;
		hi.z = v.z > hi.z ? v.z : hi.z;
	}
	result.worldBounds.lowerBound = { lo.x + placement.origin.x, lo.y + placement.origin.y, lo.z + placement.origin.z };
	result.worldBounds.upperBound = { hi.x + placement.origin.x, hi.y + placement.origin.y, hi.z + placement.origin.z };

	// Count degenerate triangles that were reported (capacity-limited; a full count
	// is not needed, just visibility that the scan is dirty).
	int degenerateReported = 0;
	for ( int idx : degenerate )
	{
		if ( idx != 0 )
		{
			degenerateReported += 1;
		}
	}

	char buf[192];
	std::snprintf( buf, sizeof( buf ), "OK: %d wierzcholkow, %d trojkatow%s%s", result.terrainVertexCount,
				   result.terrainTriangleCount, result.deferredMeshCount > 0 ? " (role nie-teren odroczone: " : "",
				   result.deferredMeshCount > 0 ? "M4)" : "" );
	result.status = buf;
	if ( std::getenv( "JOZZ_SCAN_DUMP" ) != nullptr )
	{
		std::printf( "[scan] %s; degenerate>=%d; world AABB x[%.1f,%.1f] y[%.1f,%.1f] z[%.1f,%.1f]\n", result.status.c_str(),
					 degenerateReported, result.worldBounds.lowerBound.x, result.worldBounds.upperBound.x,
					 result.worldBounds.lowerBound.y, result.worldBounds.upperBound.y, result.worldBounds.lowerBound.z,
					 result.worldBounds.upperBound.z );
		std::fflush( stdout );
	}

	result.ok = true;
	return result;
}

JozzScanTileBodies BuildJozzScanTileFromPack( b3WorldId worldId, const std::vector<JozzScanTileGeometry>& tiles,
											  const JozzScanTilePlacement& placement, uint64_t terrainCategoryBits )
{
	// Every tile of this (dirty, one-piece) scan is Terrain. Wrap each tile as a
	// Terrain input and hand the whole set to the one seam -- exercising exactly
	// the path a future multi-role scan will use.
	std::vector<JozzScanMeshInput> inputs;
	inputs.reserve( tiles.size() );
	for ( const JozzScanTileGeometry& t : tiles )
	{
		if ( t.positions.empty() || t.indices.empty() )
		{
			continue;
		}
		JozzScanMeshInput in;
		in.vertices = t.positions.data();
		in.indices = t.indices.data();
		in.vertexCount = (int)t.positions.size();
		in.triangleCount = (int)( t.indices.size() / 3 );
		in.role = JozzScanRole::Terrain;
		in.collide = true;
		inputs.push_back( in );
	}
	if ( inputs.empty() )
	{
		JozzScanTileBodies empty;
		empty.status = "paczka nie ma geometrii";
		return empty;
	}
	return BuildJozzScanTile( worldId, inputs.data(), (int)inputs.size(), placement, terrainCategoryBits );
}

void DestroyJozzScanTile( b3WorldId worldId, JozzScanTileBodies* bodies )
{
	if ( bodies == nullptr )
	{
		return;
	}
	if ( B3_IS_NON_NULL( bodies->terrainBody ) )
	{
		b3DestroyBody( bodies->terrainBody );
		bodies->terrainBody = b3_nullBodyId;
	}
	for ( b3BodyId id : bodies->structureBodies )
	{
		if ( B3_IS_NON_NULL( id ) )
		{
			b3DestroyBody( id );
		}
	}
	for ( b3BodyId id : bodies->vegetationBodies )
	{
		if ( B3_IS_NON_NULL( id ) )
		{
			b3DestroyBody( id );
		}
	}
	bodies->structureBodies.clear();
	bodies->vegetationBodies.clear();
	if ( bodies->terrainMesh != nullptr )
	{
		b3DestroyMesh( bodies->terrainMesh );
		bodies->terrainMesh = nullptr;
	}
	bodies->ok = false;
	(void)worldId;
}

std::filesystem::path FindJozzScanPackDir()
{
	const char* env = std::getenv( "JOZZ_SCAN_PREVIEW_PACK" );
	if ( env == nullptr || env[0] == '\0' )
	{
		return {};
	}
	std::error_code ec;
	std::filesystem::path p( env );
	if ( std::filesystem::exists( p, ec ) )
	{
		return p;
	}
	return {};
}
