// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_scan_visual.h"

#include "box3d/base.h" // b3Hash, B3_HASH_INIT

#include "gfx/draw.h"	  // GetDrawOrigin
#include "gfx/utility.h" // Vec4, MakeVec4

#include "jozz_vehicle_image_decode.h" // DecodeJozzVehiclePngRgba8
#include "jozz_vehicle_json.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

using namespace jozz;

namespace
{

// JSPREV2 tile .bin layout (authoritative copy of the format doc in
// jozz_scan_preview_pack.cpp; identical to jozz_vehicle_scan_geometry.cpp, kept
// separate so the collision reader stays render-free): header magic(8)"JSPREV2\0"
// + version(4) + tileId(4) + groupCount(4); then groupCount x (vertexCount(4),
// indexCount(4)); then per group vertexCount x 32B vertex (pos.xyz + normal.xyz +
// uv.xy float32) followed by indexCount x uint32 group-local triangle indices.
// Unlike the collision reader this keeps normals + uvs and does NOT merge/rebase:
// each group becomes its own textured mesh with 0-based indices.
constexpr int kHeaderBytes = 20;
constexpr int kGroupDescBytes = 8;
constexpr int kVertexBytes = 32;
constexpr int kIndexBytes = 4;
constexpr uint32_t kBinaryVersion = 2;
constexpr uint32_t kMaxGroupsPerTile = 4096u;
const char kMagic[8] = { 'J', 'S', 'P', 'R', 'E', 'V', '2', '\0' };

uint32_t ReadU32( const uint8_t* p )
{
	uint32_t value;
	std::memcpy( &value, p, sizeof( value ) );
	return value;
}

float ReadF32( const uint8_t* p )
{
	float value;
	std::memcpy( &value, p, sizeof( value ) );
	return value;
}

bool ReadWholeFile( const std::filesystem::path& path, std::vector<uint8_t>* out )
{
	std::error_code error;
	if ( std::filesystem::is_regular_file( path, error ) == false || error ||
		 std::filesystem::is_symlink( path, error ) || error )
	{
		return false;
	}
	uint64_t size = std::filesystem::file_size( path, error );
	if ( error || size == 0 || size > (uint64_t)std::numeric_limits<int>::max() )
	{
		return false;
	}
	std::ifstream input( path, std::ios::binary );
	if ( input.is_open() == false )
	{
		return false;
	}
	out->resize( (size_t)size );
	input.read( reinterpret_cast<char*>( out->data() ), (std::streamsize)out->size() );
	return input.bad() == false && input.gcount() == (std::streamsize)out->size();
}

// Reject absolute paths and any ".." so a manifest cannot point outside the pack.
bool IsSafeRelativePath( const std::filesystem::path& relative )
{
	if ( relative.empty() || relative.is_absolute() )
	{
		return false;
	}
	for ( const std::filesystem::path& part : relative )
	{
		if ( part == ".." )
		{
			return false;
		}
	}
	return true;
}

// One group's render geometry, ready for RegisterTexturedMesh.
struct RawGroup
{
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
};

// Parse one JSPREV2 tile .bin into per-group render geometry (keeps normal + uv,
// group-local 0-based indices). Accumulates the tile-local position AABB into
// localMin/localMax (caller seeds them to +/-inf across all tiles).
bool ParseTileGroups( const std::vector<uint8_t>& bytes, bool flipWinding, std::vector<RawGroup>* out, b3Vec3* localMin,
					  b3Vec3* localMax, std::string* error )
{
	if ( bytes.size() < (size_t)kHeaderBytes || std::memcmp( bytes.data(), kMagic, sizeof( kMagic ) ) != 0 )
	{
		*error = "scan visual: tile header magic mismatch";
		return false;
	}

	uint32_t version = ReadU32( bytes.data() + 8 );
	uint32_t groupCount = ReadU32( bytes.data() + 16 );
	if ( version != kBinaryVersion || groupCount == 0 || groupCount > kMaxGroupsPerTile )
	{
		*error = "scan visual: tile header out of range";
		return false;
	}

	size_t offset = kHeaderBytes;
	if ( bytes.size() < offset + (size_t)groupCount * kGroupDescBytes )
	{
		*error = "scan visual: tile group table truncated";
		return false;
	}

	std::vector<uint32_t> groupVertexCounts( groupCount );
	std::vector<uint32_t> groupIndexCounts( groupCount );
	uint64_t totalVertices = 0;
	uint64_t totalIndices = 0;
	for ( uint32_t g = 0; g < groupCount; ++g )
	{
		uint32_t vc = ReadU32( bytes.data() + offset );
		uint32_t ic = ReadU32( bytes.data() + offset + 4 );
		offset += kGroupDescBytes;
		if ( vc == 0 || ic == 0 || ic % 3 != 0 )
		{
			*error = "scan visual: tile group counts invalid";
			return false;
		}
		groupVertexCounts[g] = vc;
		groupIndexCounts[g] = ic;
		totalVertices += vc;
		totalIndices += ic;
	}

	uint64_t payloadBytes = totalVertices * (uint64_t)kVertexBytes + totalIndices * (uint64_t)kIndexBytes;
	if ( bytes.size() != offset + (size_t)payloadBytes )
	{
		*error = "scan visual: tile payload size mismatch";
		return false;
	}

	out->clear();
	out->resize( groupCount );
	for ( uint32_t g = 0; g < groupCount; ++g )
	{
		uint32_t vertexCount = groupVertexCounts[g];
		uint32_t indexCount = groupIndexCounts[g];
		RawGroup& group = ( *out )[g];
		group.vertices.reserve( vertexCount );
		group.indices.reserve( indexCount );

		for ( uint32_t v = 0; v < vertexCount; ++v )
		{
			MeshVertex vertex = {};
			vertex.position[0] = ReadF32( bytes.data() + offset + 0 );
			vertex.position[1] = ReadF32( bytes.data() + offset + 4 );
			vertex.position[2] = ReadF32( bytes.data() + offset + 8 );
			vertex.normal[0] = ReadF32( bytes.data() + offset + 12 );
			vertex.normal[1] = ReadF32( bytes.data() + offset + 16 );
			vertex.normal[2] = ReadF32( bytes.data() + offset + 20 );
			vertex.texcoord[0] = ReadF32( bytes.data() + offset + 24 );
			vertex.texcoord[1] = ReadF32( bytes.data() + offset + 28 );
			offset += kVertexBytes;

			if ( b3IsValidFloat( vertex.position[0] ) == false || b3IsValidFloat( vertex.position[1] ) == false ||
				 b3IsValidFloat( vertex.position[2] ) == false )
			{
				*error = "scan visual: tile contains non-finite position";
				return false;
			}
			localMin->x = b3MinFloat( localMin->x, vertex.position[0] );
			localMin->y = b3MinFloat( localMin->y, vertex.position[1] );
			localMin->z = b3MinFloat( localMin->z, vertex.position[2] );
			localMax->x = b3MaxFloat( localMax->x, vertex.position[0] );
			localMax->y = b3MaxFloat( localMax->y, vertex.position[1] );
			localMax->z = b3MaxFloat( localMax->z, vertex.position[2] );
			group.vertices.push_back( vertex );
		}

		for ( uint32_t i = 0; i < indexCount; i += 3 )
		{
			uint32_t i0 = ReadU32( bytes.data() + offset + 0 );
			uint32_t i1 = ReadU32( bytes.data() + offset + 4 );
			uint32_t i2 = ReadU32( bytes.data() + offset + 8 );
			offset += 3 * kIndexBytes;
			if ( i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount )
			{
				*error = "scan visual: tile index out of range";
				return false;
			}
			group.indices.push_back( i0 );
			if ( flipWinding )
			{
				group.indices.push_back( i2 );
				group.indices.push_back( i1 );
			}
			else
			{
				group.indices.push_back( i1 );
				group.indices.push_back( i2 );
			}
		}
	}

	return true;
}

uint32_t BuildGroupHash( const std::string& texturePath, const std::vector<MeshVertex>& vertices,
						 const std::vector<uint32_t>& indices, bool flipWinding )
{
	uint32_t hash = B3_HASH_INIT;
	hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( texturePath.data() ), (int)texturePath.size() );
	hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( vertices.data() ), (int)( vertices.size() * sizeof( MeshVertex ) ) );
	hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( indices.data() ), (int)( indices.size() * sizeof( uint32_t ) ) );
	uint8_t flip = flipWinding ? 1u : 0u;
	hash = b3Hash( hash, &flip, 1 );
	return hash != 0u ? hash : 1u;
}

// Register one group as a textured mesh, or as a solid mesh when the PNG is
// missing / undecodable (a gray patch is better than a hole). Returns the handle
// and reports whether the texture took, plus the uploaded RGBA byte count.
MeshHandle RegisterGroup( const std::filesystem::path& packDir, const std::string& texturePath, const RawGroup& group,
						  bool flipWinding, bool* textured, long long* textureBytes )
{
	*textured = false;
	*textureBytes = 0;

	uint32_t hash = BuildGroupHash( texturePath, group.vertices, group.indices, flipWinding );
	MeshHandle existing = FindMesh( hash );
	if ( IsMeshHandleValid( existing ) )
	{
		AddMeshReference( existing );
		return existing; // texture flag/bytes only matter on first registration for stats
	}

	std::filesystem::path abs = packDir / std::filesystem::path( texturePath );
	std::vector<uint8_t> png;
	JozzVehicleDecodedImage decoded;
	bool haveTexture = texturePath.empty() == false && IsSafeRelativePath( std::filesystem::path( texturePath ) ) &&
					   ReadWholeFile( abs, &png ) && DecodeJozzVehiclePngRgba8( png.data(), png.size(), &decoded );

	MeshHandle handle = InvalidMeshHandle();
	if ( haveTexture )
	{
		MeshTextureData texData = {};
		texData.width = decoded.width;
		texData.height = decoded.height;
		texData.rgba8 = decoded.rgba8.data();
		texData.byteCount = (int)decoded.rgba8.size();
		handle = RegisterTexturedMesh( hash, group.vertices.data(), (int)group.vertices.size(), group.indices.data(),
									   (int)group.indices.size(), &texData, "jozz_scan_group" );
		if ( IsMeshHandleValid( handle ) )
		{
			*textured = true;
			*textureBytes = (long long)decoded.rgba8.size();
		}
	}
	if ( IsMeshHandleValid( handle ) == false )
	{
		handle = RegisterMesh( hash, group.vertices.data(), (int)group.vertices.size(), group.indices.data(),
							   (int)group.indices.size(), "jozz_scan_group" );
	}
	if ( IsMeshHandleValid( handle ) )
	{
		SetMeshKind( handle, MESH_KIND_MESH );
	}
	return handle;
}

} // namespace

bool LoadJozzScanVisual( const std::filesystem::path& packDir, b3Vec3 origin, bool flipWinding, JozzScanVisual* out,
						 std::string* error )
{
	*out = JozzScanVisual{};
	out->origin = origin;

	std::string json;
	if ( ReadTextFile( packDir / "COMPLETE.json", &json ) == false )
	{
		*error = "scan visual: COMPLETE.json missing or unreadable";
		return false;
	}

	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens.empty() || tokens[0].type != JSMN_OBJECT )
	{
		*error = "scan visual: manifest JSON did not parse";
		return false;
	}

	int tilesIndex = FindObjectValue( json, tokens, 0, "tiles" );
	if ( tilesIndex < 0 || tokens[tilesIndex].type != JSMN_ARRAY || tokens[tilesIndex].size <= 0 )
	{
		*error = "scan visual: manifest tiles array missing or empty";
		return false;
	}

	b3Vec3 localMin = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	b3Vec3 localMax = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };

	int tileCount = tokens[tilesIndex].size;
	for ( int t = 0; t < tileCount; ++t )
	{
		int recordIndex = GetArrayElement( tokens, tilesIndex, t );
		int pathIndex = recordIndex >= 0 ? FindObjectValue( json, tokens, recordIndex, "path" ) : -1;
		int groupsIndex = recordIndex >= 0 ? FindObjectValue( json, tokens, recordIndex, "groups" ) : -1;
		if ( pathIndex < 0 || groupsIndex < 0 || tokens[groupsIndex].type != JSMN_ARRAY )
		{
			*error = "scan visual: tile record missing path or groups";
			DestroyJozzScanVisual( out );
			return false;
		}

		std::filesystem::path relative = std::filesystem::path( TokenString( json, tokens[pathIndex] ) );
		if ( IsSafeRelativePath( relative ) == false )
		{
			*error = "scan visual: tile path is unsafe or empty";
			DestroyJozzScanVisual( out );
			return false;
		}

		std::vector<uint8_t> bytes;
		if ( ReadWholeFile( packDir / relative, &bytes ) == false )
		{
			*error = "scan visual: tile file missing, linked or unreadable";
			DestroyJozzScanVisual( out );
			return false;
		}

		std::vector<RawGroup> rawGroups;
		if ( ParseTileGroups( bytes, flipWinding, &rawGroups, &localMin, &localMax, error ) == false )
		{
			DestroyJozzScanVisual( out );
			return false;
		}
		if ( (int)rawGroups.size() != tokens[groupsIndex].size )
		{
			*error = "scan visual: manifest group count does not match tile .bin";
			DestroyJozzScanVisual( out );
			return false;
		}

		for ( int g = 0; g < (int)rawGroups.size(); ++g )
		{
			int groupIndex = GetArrayElement( tokens, groupsIndex, g );
			int texPathIndex = groupIndex >= 0 ? FindObjectValue( json, tokens, groupIndex, "texturePath" ) : -1;
			std::string texturePath = texPathIndex >= 0 ? TokenString( json, tokens[texPathIndex] ) : std::string();

			bool textured = false;
			long long texBytes = 0;
			MeshHandle handle =
				RegisterGroup( packDir, texturePath, rawGroups[g], flipWinding, &textured, &texBytes );
			if ( IsMeshHandleValid( handle ) == false )
			{
				*error = "scan visual: renderer registry rejected a group";
				DestroyJozzScanVisual( out );
				return false;
			}

			JozzScanVisualGroup vg;
			vg.handle = handle;
			vg.vertexCount = (int)rawGroups[g].vertices.size();
			vg.triangleCount = (int)( rawGroups[g].indices.size() / 3 );
			vg.textured = textured;
			out->groups.push_back( vg );
			out->totalVertices += vg.vertexCount;
			out->totalTriangles += vg.triangleCount;
			if ( textured )
			{
				out->textureCount += 1;
				out->textureBytes += texBytes;
			}
		}
	}

	if ( out->groups.empty() )
	{
		*error = "scan visual: no groups registered";
		DestroyJozzScanVisual( out );
		return false;
	}

	out->worldBounds.lowerBound = { localMin.x + origin.x, localMin.y + origin.y, localMin.z + origin.z };
	out->worldBounds.upperBound = { localMax.x + origin.x, localMax.y + origin.y, localMax.z + origin.z };
	out->loaded = true;

	char buf[192];
	std::snprintf( buf, sizeof( buf ), "OK: %d grup, %d trojkatow, tekstury %d/%d (%.0f MB GPU)", (int)out->groups.size(),
				   out->totalTriangles, out->textureCount, (int)out->groups.size(),
				   (double)out->textureBytes / ( 1024.0 * 1024.0 ) );
	out->status = buf;
	return true;
}

void DrawJozzScanVisual( const JozzScanVisual& visual )
{
	if ( visual.loaded == false )
	{
		return;
	}

	// One draw per group at the scan origin, demoted against the camera-focus draw
	// origin (the scan sits hundreds of metres north, far from the float origin --
	// same double-precision shift JozzVehicleVisualMesh::DrawAtTransform uses).
	b3WorldTransform worldTransform = { visual.origin, b3Quat_identity };
	b3Transform relative = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	const Vec4 white = MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f );
	for ( const JozzScanVisualGroup& group : visual.groups )
	{
		if ( IsMeshHandleValid( group.handle ) == false )
		{
			continue;
		}
		// Matte terrain: no metal, high roughness. Textured groups sample the PNG;
		// a fallback (untextured) group draws solid white so the hole is obvious.
		AppendMesh( group.handle, relative, b3Vec3_one, white, 0.0f, 0.92f,
					group.textured ? MESH_MATERIAL_MODE_TEXTURED : MESH_MATERIAL_MODE_SOLID, 0.0f,
					TRANSPARENT_SHADOW_FULL );
	}
}

void DestroyJozzScanVisual( JozzScanVisual* visual )
{
	if ( visual == nullptr )
	{
		return;
	}
	for ( JozzScanVisualGroup& group : visual->groups )
	{
		if ( IsMeshHandleValid( group.handle ) )
		{
			ReleaseMeshReference( group.handle );
		}
	}
	*visual = JozzScanVisual{};
}
