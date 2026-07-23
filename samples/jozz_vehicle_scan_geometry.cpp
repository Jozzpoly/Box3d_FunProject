// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_scan_geometry.h"

#include "jozz_vehicle_json.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace
{

// JSPREV2 tile .bin layout (authoritative copy of the format doc in
// jozz_scan_preview_pack.cpp): header magic(8)"JSPREV2\0" + version(4) +
// tileId(4) + groupCount(4); then groupCount x (vertexCount(4), indexCount(4));
// then per group vertexCount x 32B vertex (pos.xyz + normal.xyz + uv.xy float32)
// followed by indexCount x uint32 group-local triangle-list indices.
constexpr int kHeaderBytes = 20;
constexpr int kGroupDescBytes = 8;
constexpr int kVertexBytes = 32; // pos.xyz(12) + normal.xyz(12) + uv.xy(8)
constexpr int kIndexBytes = 4;
constexpr uint32_t kBinaryVersion = 2;
constexpr uint32_t kMaxGroupsPerTile = 4096u;
const char kMagic[8] = { 'J', 'S', 'P', 'R', 'E', 'V', '2', '\0' };

// The pack is written little-endian and samples run on little-endian hosts, so
// a straight copy is correct.
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

// Reject absolute paths and any ".." component so a manifest cannot point the
// reader outside its own pack directory (defense in depth; the pack is local
// and pipeline-built, but the parse stays strict).
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

// Parse one self-describing JSPREV2 tile .bin into positions + rebased indices.
bool ParseTileBin( const std::vector<uint8_t>& bytes, bool flipWinding, JozzScanTileGeometry* out, std::string* error )
{
	if ( bytes.size() < (size_t)kHeaderBytes || std::memcmp( bytes.data(), kMagic, sizeof( kMagic ) ) != 0 )
	{
		*error = "scan geometry: tile header magic mismatch";
		return false;
	}

	uint32_t version = ReadU32( bytes.data() + 8 );
	uint32_t tileId = ReadU32( bytes.data() + 12 );
	uint32_t groupCount = ReadU32( bytes.data() + 16 );
	if ( version != kBinaryVersion || groupCount == 0 || groupCount > kMaxGroupsPerTile )
	{
		*error = "scan geometry: tile header out of range";
		return false;
	}

	size_t offset = kHeaderBytes;
	if ( bytes.size() < offset + (size_t)groupCount * kGroupDescBytes )
	{
		*error = "scan geometry: tile group table truncated";
		return false;
	}

	// Read the descriptor table first so the payload size can be validated
	// end-to-end and the tile buffers sized exactly.
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
			*error = "scan geometry: tile group counts invalid";
			return false;
		}
		groupVertexCounts[g] = vc;
		groupIndexCounts[g] = ic;
		totalVertices += vc;
		totalIndices += ic;
	}
	if ( totalVertices > (uint64_t)std::numeric_limits<int32_t>::max() )
	{
		*error = "scan geometry: tile vertex count exceeds int32";
		return false;
	}

	uint64_t payloadBytes = totalVertices * (uint64_t)kVertexBytes + totalIndices * (uint64_t)kIndexBytes;
	if ( bytes.size() != offset + (size_t)payloadBytes )
	{
		*error = "scan geometry: tile payload size mismatch";
		return false;
	}

	out->tileId = (int)tileId;
	out->positions.clear();
	out->indices.clear();
	out->positions.reserve( (size_t)totalVertices );
	out->indices.reserve( (size_t)totalIndices );

	float lo = std::numeric_limits<float>::max();
	float hi = -std::numeric_limits<float>::max();
	b3AABB bounds = { { lo, lo, lo }, { hi, hi, hi } };

	for ( uint32_t g = 0; g < groupCount; ++g )
	{
		uint32_t vertexCount = groupVertexCounts[g];
		uint32_t indexCount = groupIndexCounts[g];
		int32_t base = (int32_t)out->positions.size();

		for ( uint32_t v = 0; v < vertexCount; ++v )
		{
			float x = ReadF32( bytes.data() + offset );
			float y = ReadF32( bytes.data() + offset + 4 );
			float z = ReadF32( bytes.data() + offset + 8 );
			offset += kVertexBytes; // skip normal.xyz + uv.xy: collision needs only position
			if ( b3IsValidFloat( x ) == false || b3IsValidFloat( y ) == false || b3IsValidFloat( z ) == false )
			{
				*error = "scan geometry: tile contains non-finite position";
				return false;
			}
			out->positions.push_back( { x, y, z } );
			bounds.lowerBound.x = std::min( bounds.lowerBound.x, x );
			bounds.lowerBound.y = std::min( bounds.lowerBound.y, y );
			bounds.lowerBound.z = std::min( bounds.lowerBound.z, z );
			bounds.upperBound.x = std::max( bounds.upperBound.x, x );
			bounds.upperBound.y = std::max( bounds.upperBound.y, y );
			bounds.upperBound.z = std::max( bounds.upperBound.z, z );
		}

		for ( uint32_t i = 0; i < indexCount; i += 3 )
		{
			uint32_t i0 = ReadU32( bytes.data() + offset );
			uint32_t i1 = ReadU32( bytes.data() + offset + 4 );
			uint32_t i2 = ReadU32( bytes.data() + offset + 8 );
			offset += 3 * kIndexBytes;
			if ( i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount )
			{
				*error = "scan geometry: tile index out of range";
				return false;
			}
			out->indices.push_back( base + (int32_t)i0 );
			if ( flipWinding )
			{
				out->indices.push_back( base + (int32_t)i2 );
				out->indices.push_back( base + (int32_t)i1 );
			}
			else
			{
				out->indices.push_back( base + (int32_t)i1 );
				out->indices.push_back( base + (int32_t)i2 );
			}
		}
	}

	out->bounds = bounds;
	return true;
}

} // namespace

bool LoadJozzScanPackGeometry( const std::filesystem::path& packDir, std::vector<JozzScanTileGeometry>* out,
							   std::string* error, bool flipWinding )
{
	out->clear();

	std::string json;
	if ( jozz::ReadTextFile( packDir / "COMPLETE.json", &json ) == false )
	{
		*error = "scan geometry: COMPLETE.json missing or unreadable";
		return false;
	}

	std::vector<jsmntok_t> tokens;
	if ( jozz::ParseJson( json, &tokens ) == false || tokens.empty() || tokens[0].type != JSMN_OBJECT )
	{
		*error = "scan geometry: manifest JSON did not parse";
		return false;
	}

	int tilesIndex = jozz::FindObjectValue( json, tokens, 0, "tiles" );
	if ( tilesIndex < 0 || tokens[tilesIndex].type != JSMN_ARRAY || tokens[tilesIndex].size <= 0 )
	{
		*error = "scan geometry: manifest tiles array missing or empty";
		return false;
	}

	int tileCount = tokens[tilesIndex].size;
	out->reserve( (size_t)tileCount );
	for ( int t = 0; t < tileCount; ++t )
	{
		int recordIndex = jozz::GetArrayElement( tokens, tilesIndex, t );
		int pathIndex = recordIndex >= 0 ? jozz::FindObjectValue( json, tokens, recordIndex, "path" ) : -1;
		if ( pathIndex < 0 )
		{
			*error = "scan geometry: tile record missing path";
			return false;
		}

		std::filesystem::path relative = std::filesystem::path( jozz::TokenString( json, tokens[pathIndex] ) );
		if ( IsSafeRelativePath( relative ) == false )
		{
			*error = "scan geometry: tile path is unsafe or empty";
			return false;
		}

		std::vector<uint8_t> bytes;
		if ( ReadWholeFile( packDir / relative, &bytes ) == false )
		{
			*error = "scan geometry: tile file missing, linked or unreadable";
			return false;
		}

		JozzScanTileGeometry tile;
		if ( ParseTileBin( bytes, flipWinding, &tile, error ) == false )
		{
			return false;
		}
		out->push_back( std::move( tile ) );
	}

	return true;
}
