// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_scan_import.h"

#include "box3d/box3d.h"
#include "box3d/collision.h"

#include <chrono>
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

// Read a 0/1 env override; returns fallback when unset. Used to A/B the mesh-build
// flags (weld / identifyEdges / median-split) against load time without rebuilding.
bool EnvBool( const char* name, bool fallback )
{
	const char* v = std::getenv( name );
	return v == nullptr ? fallback : ( std::atoi( v ) != 0 );
}

} // namespace

JozzScanTileBodies BuildJozzScanTile( b3WorldId worldId, const JozzScanMeshInput* meshes, int meshCount,
									  const JozzScanTilePlacement& placement, uint64_t terrainCategoryBits,
									  const char* cachePath )
{
	JozzScanTileBodies result;
	const bool dump = std::getenv( "JOZZ_SCAN_DUMP" ) != nullptr;

	// --- 1. Tally the Terrain inputs (no merge yet -- a cache hit skips merging) ---
	long long totalVerts = 0;
	long long totalTris = 0;
	for ( int m = 0; m < meshCount; ++m )
	{
		const JozzScanMeshInput& in = meshes[m];
		if ( in.role != JozzScanRole::Terrain )
		{
			if ( in.role != JozzScanRole::Decoration ) // Structure / Vegetation: M4
			{
				result.deferredMeshCount += 1;
			}
			continue;
		}
		if ( in.vertices != nullptr && in.indices != nullptr && in.vertexCount >= 3 && in.triangleCount >= 1 )
		{
			totalVerts += in.vertexCount;
			totalTris += in.triangleCount;
		}
	}
	if ( totalTris < 1 )
	{
		result.status = "brak trojkatow terenu w wejsciu";
		return result;
	}

	// --- 2. The cooked BVH: load from cache, else cook once and write the cache ---
	// This is the load-time fix. b3CreateMesh (BVH build) dominates load time
	// (~14 s Debug / ~1.8 s Release on 1.8M tris); reading back the cooked blob is
	// ~instant. b3MeshData is a self-contained relocatable blob (offsets from its
	// own address), so box3d's own b3WriteBinaryFile / b3ReadBinaryFile round-trip
	// it (exactly how box3d dumps/reloads meshes, src/shape.c). b3ReadBinaryFile
	// allocates via b3Alloc, so b3DestroyMesh (== b3Free) frees a cached mesh too.
	b3MeshData* mesh = nullptr;
	if ( cachePath != nullptr && cachePath[0] != '\0' )
	{
		int cachedSize = 0;
		b3MeshData* cached = (b3MeshData*)b3ReadBinaryFile( "", cachePath, &cachedSize );
		if ( cached != nullptr )
		{
			if ( cached->version == B3_MESH_VERSION && cached->byteCount == cachedSize )
			{
				mesh = cached;
				result.fromCache = true;
				if ( dump )
				{
					std::printf( "[scan] cache HIT (%d B, %s)\n", cachedSize, cachePath );
					std::fflush( stdout );
				}
			}
			else
			{
				b3DestroyMesh( cached ); // stale version / corrupt -> recook below
			}
		}
	}

	// Mesh-build flags. Defaults = QUALITY (the config Jozz validated as good to
	// drive): weld stitches tile seams and removes slivers, identifyEdges flags
	// concave edges so the car does not catch on internal triangle edges, SAH
	// gives the fastest runtime queries. The cook cost is paid once (then cached),
	// so quality wins. Each flag is env-overridable to re-measure the trade-off.
	const bool useWeld = EnvBool( "JOZZ_SCAN_WELD", true );
	const bool useEdges = EnvBool( "JOZZ_SCAN_EDGES", true );
	const bool useMedian = EnvBool( "JOZZ_SCAN_MEDIAN", false ); // false == SAH

	if ( mesh == nullptr )
	{
		// Cache miss: merge all Terrain inputs into ONE contiguous array and cook.
		std::vector<b3Vec3> verts;
		std::vector<int32_t> indices;
		verts.reserve( (size_t)totalVerts );
		indices.reserve( (size_t)totalTris * 3 );
		for ( int m = 0; m < meshCount; ++m )
		{
			const JozzScanMeshInput& in = meshes[m];
			if ( in.role != JozzScanRole::Terrain || in.vertices == nullptr || in.indices == nullptr ||
				 in.vertexCount < 3 || in.triangleCount < 1 )
			{
				continue;
			}
			const int32_t base = (int32_t)verts.size();
			verts.insert( verts.end(), in.vertices, in.vertices + in.vertexCount );
			for ( int i = 0; i < in.triangleCount * 3; ++i )
			{
				indices.push_back( in.indices[i] + base );
			}
		}

		std::vector<int> degenerate( 256 );
		b3MeshDef def = {};
		def.vertices = verts.data();
		def.indices = indices.data();
		def.materialIndices = nullptr; // single surface material for M1 (per-triangle materials: later)
		def.vertexCount = (int)verts.size();
		def.triangleCount = (int)( indices.size() / 3 );
		def.weldVertices = useWeld;
		def.weldTolerance = kScanWeldTolerance;
		def.useMedianSplit = useMedian;
		def.identifyEdges = useEdges;

		auto meshT0 = std::chrono::steady_clock::now();
		mesh = b3CreateMesh( &def, degenerate.data(), (int)degenerate.size() );
		double meshMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - meshT0 ).count();
		if ( dump )
		{
			std::printf( "[scan] b3CreateMesh %.0f ms  (weld=%d edges=%d median=%d, %d trojkatow)\n", meshMs, useWeld,
						 useEdges, useMedian, def.triangleCount );
			std::fflush( stdout );
		}
		if ( mesh == nullptr )
		{
			result.status = "b3CreateMesh zwrocil null";
			return result;
		}
		if ( cachePath != nullptr && cachePath[0] != '\0' )
		{
			b3WriteBinaryFile( (void*)mesh, mesh->byteCount, cachePath ); // cook once for next time
			if ( dump )
			{
				std::printf( "[scan] cache WRITE (%d B, %s)\n", mesh->byteCount, cachePath );
				std::fflush( stdout );
			}
		}
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
	result.terrainVertexCount = (int)totalVerts;
	result.terrainTriangleCount = (int)totalTris;

	// World-space AABB straight from the cooked blob's own local bounds + origin.
	result.worldBounds.lowerBound = { mesh->bounds.lowerBound.x + placement.origin.x,
									  mesh->bounds.lowerBound.y + placement.origin.y,
									  mesh->bounds.lowerBound.z + placement.origin.z };
	result.worldBounds.upperBound = { mesh->bounds.upperBound.x + placement.origin.x,
									  mesh->bounds.upperBound.y + placement.origin.y,
									  mesh->bounds.upperBound.z + placement.origin.z };

	char buf[192];
	std::snprintf( buf, sizeof( buf ), "OK (%s): %d wierzcholkow, %d trojkatow%s", result.fromCache ? "z cache" : "ugotowany",
				   result.terrainVertexCount, result.terrainTriangleCount,
				   result.deferredMeshCount > 0 ? " (role nie-teren: M4)" : "" );
	result.status = buf;
	if ( dump )
	{
		std::printf( "[scan] %s; world AABB x[%.1f,%.1f] y[%.1f,%.1f] z[%.1f,%.1f]\n", result.status.c_str(),
					 result.worldBounds.lowerBound.x, result.worldBounds.upperBound.x, result.worldBounds.lowerBound.y,
					 result.worldBounds.upperBound.y, result.worldBounds.lowerBound.z, result.worldBounds.upperBound.z );
		std::fflush( stdout );
	}

	result.ok = true;
	return result;
}

JozzScanTileBodies BuildJozzScanTileFromPack( b3WorldId worldId, const std::vector<JozzScanTileGeometry>& tiles,
											  const JozzScanTilePlacement& placement, uint64_t terrainCategoryBits,
											  const char* cachePath )
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
	return BuildJozzScanTile( worldId, inputs.data(), (int)inputs.size(), placement, terrainCategoryBits, cachePath );
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
