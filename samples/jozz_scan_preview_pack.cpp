// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_scan_preview_pack.h"

#include "box3d/base.h"
#include "gfx/draw.h"
#include "jozz_vehicle_json.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <string_view>
#include <vector>

using namespace jozz;

namespace
{

constexpr uint32_t kBinaryVersion = 1;
constexpr size_t kHeaderBytes = 24;
constexpr size_t kVertexBytes = 24;
constexpr size_t kIndexBytes = 4;
constexpr int kMaxTiles = 64;
constexpr uint64_t kMaxTileBytes = 1024ull * 1024ull * 1024ull;

struct ManifestTile
{
	int tileId = -1;
	std::string path;
	uint64_t byteLength = 0;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	uint32_t triangleCount = 0;
	b3AABB bounds = {};
};

uint32_t ReadU32( const uint8_t* data )
{
	return (uint32_t)data[0] | ( (uint32_t)data[1] << 8 ) | ( (uint32_t)data[2] << 16 ) | ( (uint32_t)data[3] << 24 );
}

float ReadF32( const uint8_t* data )
{
	uint32_t bits = ReadU32( data );
	float value = 0.0f;
	std::memcpy( &value, &bits, sizeof( value ) );
	return value;
}

bool TokenU64( std::string_view json, const jsmntok_t& token, uint64_t* out )
{
	if ( token.start < 0 || token.end <= token.start )
	{
		return false;
	}

	std::string text = TokenString( json, token );
	if ( text.empty() || text[0] == '-' )
	{
		return false;
	}

	char* end = nullptr;
	errno = 0;
	unsigned long long value = std::strtoull( text.c_str(), &end, 10 );
	if ( errno != 0 || end == text.c_str() || *end != '\0' )
	{
		return false;
	}

	*out = (uint64_t)value;
	return true;
}

bool ReadStringMember( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key, std::string* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	if ( valueIndex < 0 || tokens[valueIndex].type != JSMN_STRING )
	{
		return false;
	}

	*out = TokenString( json, tokens[valueIndex] );
	return true;
}

bool ReadIntMember( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key, int* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	return valueIndex >= 0 && TokenInt( json, tokens[valueIndex], out );
}

bool ReadU64Member( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key, uint64_t* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	return valueIndex >= 0 && TokenU64( json, tokens[valueIndex], out );
}

bool ReadBoolMember( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key, bool* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	return valueIndex >= 0 && TokenBool( json, tokens[valueIndex], out );
}

bool ReadBounds( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, b3AABB* out )
{
	int minimumIndex = FindObjectValue( json, tokens, objectIndex, "min" );
	int maximumIndex = FindObjectValue( json, tokens, objectIndex, "max" );
	float minimum[3] = {};
	float maximum[3] = {};
	if ( ParseFloatArray( json, tokens, minimumIndex, minimum, 3 ) == false ||
		 ParseFloatArray( json, tokens, maximumIndex, maximum, 3 ) == false )
	{
		return false;
	}

	for ( int axis = 0; axis < 3; ++axis )
	{
		if ( b3IsValidFloat( minimum[axis] ) == false || b3IsValidFloat( maximum[axis] ) == false || minimum[axis] > maximum[axis] )
		{
			return false;
		}
	}

	out->lowerBound = { minimum[0], minimum[1], minimum[2] };
	out->upperBound = { maximum[0], maximum[1], maximum[2] };
	return true;
}

bool NearlyEqual( float a, float b )
{
	float scale = std::max( 1.0f, std::max( std::fabs( a ), std::fabs( b ) ) );
	return std::fabs( a - b ) <= 1.0e-4f * scale;
}

bool BoundsEqual( b3AABB a, b3AABB b )
{
	return NearlyEqual( a.lowerBound.x, b.lowerBound.x ) && NearlyEqual( a.lowerBound.y, b.lowerBound.y ) &&
		   NearlyEqual( a.lowerBound.z, b.lowerBound.z ) && NearlyEqual( a.upperBound.x, b.upperBound.x ) &&
		   NearlyEqual( a.upperBound.y, b.upperBound.y ) && NearlyEqual( a.upperBound.z, b.upperBound.z );
}

bool IsSafeRelativePath( const std::string& text )
{
	if ( text.empty() )
	{
		return false;
	}

	std::filesystem::path path( text );
	if ( path.is_absolute() || path.has_root_name() || path.has_root_directory() || path.generic_string() != text )
	{
		return false;
	}

	for ( const std::filesystem::path& part : path )
	{
		if ( part == ".." || part == "." )
		{
			return false;
		}
	}

	return true;
}

bool ReadBytes( const std::filesystem::path& path, uint64_t expectedSize, std::vector<uint8_t>* out )
{
	if ( expectedSize == 0 || expectedSize > kMaxTileBytes || expectedSize > (uint64_t)std::numeric_limits<int>::max() )
	{
		return false;
	}

	std::error_code error;
	if ( std::filesystem::is_regular_file( path, error ) == false || error || std::filesystem::is_symlink( path, error ) )
	{
		return false;
	}

	uint64_t actualSize = std::filesystem::file_size( path, error );
	if ( error || actualSize != expectedSize )
	{
		return false;
	}

	std::ifstream input( path, std::ios::binary );
	if ( input.is_open() == false )
	{
		return false;
	}

	out->resize( (size_t)expectedSize );
	input.read( reinterpret_cast<char*>( out->data() ), (std::streamsize)out->size() );
	return input.good();
}

bool ParseManifest( const std::filesystem::path& directory, std::vector<ManifestTile>* tiles, std::string* revision, std::string* error )
{
	std::string json;
	if ( ReadTextFile( directory / "COMPLETE.json", &json ) == false )
	{
		*error = "preview pack: COMPLETE.json not found";
		return false;
	}

	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens[0].type != JSMN_OBJECT )
	{
		*error = "preview pack: manifest JSON parse failed";
		return false;
	}

	std::string schema;
	std::string status;
	std::string privacy;
	std::string purpose;
	int version = 0;
	if ( ReadStringMember( json, tokens, 0, "schema", &schema ) == false ||
		 ReadIntMember( json, tokens, 0, "schemaVersion", &version ) == false ||
		 ReadStringMember( json, tokens, 0, "status", &status ) == false ||
		 ReadStringMember( json, tokens, 0, "privacyClass", &privacy ) == false ||
		 ReadStringMember( json, tokens, 0, "purpose", &purpose ) == false ||
		 ReadStringMember( json, tokens, 0, "sourceRevisionId", revision ) == false )
	{
		*error = "preview pack: required manifest fields are missing";
		return false;
	}

	if ( schema != "jozz.scan-source-visual-preview-pack" || version != 1 || status != "COMPLETE" ||
		 privacy != "PRIVATE_LOCAL_ONLY" || purpose != "SOURCE_VISUAL_PREVIEW_ONLY" )
	{
		*error = "preview pack: schema, privacy or purpose boundary mismatch";
		return false;
	}

	int capabilitiesIndex = FindObjectValue( json, tokens, 0, "capabilities" );
	bool sourceVisible = false;
	bool textures = true;
	bool correspondence = true;
	bool acceptedWorld = true;
	bool collisionReady = true;
	if ( capabilitiesIndex < 0 ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "sourceGeometryVisible", &sourceVisible ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "texturesIncluded", &textures ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "internalGeometryCorrespondencePassed", &correspondence ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "acceptedWorld", &acceptedWorld ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "collisionReady", &collisionReady ) == false ||
		 sourceVisible == false || textures || correspondence || acceptedWorld || collisionReady )
	{
		*error = "preview pack: capability overclaim or incomplete boundary";
		return false;
	}

	int tileCount = 0;
	int tilesIndex = FindObjectValue( json, tokens, 0, "tiles" );
	if ( ReadIntMember( json, tokens, 0, "tileCount", &tileCount ) == false || tileCount <= 0 || tileCount > kMaxTiles ||
		 tilesIndex < 0 || tokens[tilesIndex].type != JSMN_ARRAY || tokens[tilesIndex].size != tileCount )
	{
		*error = "preview pack: tileCount or tiles array is invalid";
		return false;
	}

	tiles->clear();
	tiles->reserve( (size_t)tileCount );
	std::set<int> seen;
	int previousTileId = -1;
	for ( int index = 0; index < tileCount; ++index )
	{
		int recordIndex = GetArrayElement( tokens, tilesIndex, index );
		if ( recordIndex < 0 || tokens[recordIndex].type != JSMN_OBJECT )
		{
			*error = "preview pack: tile record is not an object";
			return false;
		}

		ManifestTile tile;
		uint64_t vertexCount = 0;
		uint64_t indexCount = 0;
		uint64_t triangleCount = 0;
		int boundsIndex = FindObjectValue( json, tokens, recordIndex, "boundsLabMeters" );
		if ( ReadIntMember( json, tokens, recordIndex, "tileId", &tile.tileId ) == false ||
			 ReadStringMember( json, tokens, recordIndex, "path", &tile.path ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "byteLength", &tile.byteLength ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "vertexCount", &vertexCount ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "indexCount", &indexCount ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "triangleCount", &triangleCount ) == false ||
			 ReadBounds( json, tokens, boundsIndex, &tile.bounds ) == false )
		{
			*error = "preview pack: malformed tile record";
			return false;
		}

		if ( tile.tileId < 0 || tile.tileId <= previousTileId || seen.insert( tile.tileId ).second == false ||
			 vertexCount == 0 || vertexCount > UINT32_MAX || indexCount == 0 || indexCount > UINT32_MAX ||
			 triangleCount == 0 || triangleCount > UINT32_MAX || indexCount != 3 * triangleCount ||
			 IsSafeRelativePath( tile.path ) == false )
		{
			*error = "preview pack: non-canonical tile identity, counts or path";
			return false;
		}

		char expectedPath[64];
		std::snprintf( expectedPath, sizeof( expectedPath ), "tiles/tile_%03d.bin", tile.tileId );
		if ( tile.path != expectedPath )
		{
			*error = "preview pack: tile path does not match tile id";
			return false;
		}

		tile.vertexCount = (uint32_t)vertexCount;
		tile.indexCount = (uint32_t)indexCount;
		tile.triangleCount = (uint32_t)triangleCount;
		uint64_t expectedBytes = kHeaderBytes + vertexCount * kVertexBytes + indexCount * kIndexBytes;
		if ( tile.byteLength != expectedBytes || tile.byteLength > kMaxTileBytes )
		{
			*error = "preview pack: tile byteLength does not match binary counts";
			return false;
		}

		previousTileId = tile.tileId;
		tiles->push_back( tile );
	}

	return true;
}

bool LoadTile( const std::filesystem::path& root, const ManifestTile& record, const std::string& revision,
			   JozzScanPreviewTile* out, std::string* error )
{
	std::vector<uint8_t> bytes;
	if ( ReadBytes( root / std::filesystem::path( record.path ), record.byteLength, &bytes ) == false )
	{
		*error = "preview pack: tile file missing, linked or wrong size";
		return false;
	}

	if ( bytes.size() < kHeaderBytes || std::memcmp( bytes.data(), "JSPREV1", 7 ) != 0 )
	{
		*error = "preview pack: tile header magic mismatch";
		return false;
	}

	uint32_t version = ReadU32( bytes.data() + 8 );
	uint32_t tileId = ReadU32( bytes.data() + 12 );
	uint32_t vertexCount = ReadU32( bytes.data() + 16 );
	uint32_t indexCount = ReadU32( bytes.data() + 20 );
	if ( version != kBinaryVersion || tileId != (uint32_t)record.tileId || vertexCount != record.vertexCount ||
		 indexCount != record.indexCount || indexCount % 3 != 0 )
	{
		*error = "preview pack: tile header disagrees with manifest";
		return false;
	}

	std::vector<MeshVertex> vertices;
	vertices.resize( vertexCount );
	b3AABB actualBounds = {
		{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() },
		{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() },
	};

	size_t offset = kHeaderBytes;
	for ( uint32_t index = 0; index < vertexCount; ++index )
	{
		float values[6];
		for ( int component = 0; component < 6; ++component )
		{
			values[component] = ReadF32( bytes.data() + offset + (size_t)component * 4 );
			if ( b3IsValidFloat( values[component] ) == false )
			{
				*error = "preview pack: tile contains non-finite vertex data";
				return false;
			}
		}
		offset += kVertexBytes;

		MeshVertex vertex = {};
		vertex.position[0] = values[0];
		vertex.position[1] = values[1];
		vertex.position[2] = values[2];
		vertex.normal[0] = values[3];
		vertex.normal[1] = values[4];
		vertex.normal[2] = values[5];
		vertex.texcoord[0] = 0.0f;
		vertex.texcoord[1] = 0.0f;
		vertices[index] = vertex;

		actualBounds.lowerBound.x = std::min( actualBounds.lowerBound.x, values[0] );
		actualBounds.lowerBound.y = std::min( actualBounds.lowerBound.y, values[1] );
		actualBounds.lowerBound.z = std::min( actualBounds.lowerBound.z, values[2] );
		actualBounds.upperBound.x = std::max( actualBounds.upperBound.x, values[0] );
		actualBounds.upperBound.y = std::max( actualBounds.upperBound.y, values[1] );
		actualBounds.upperBound.z = std::max( actualBounds.upperBound.z, values[2] );
	}

	std::vector<uint32_t> indices;
	indices.resize( indexCount );
	for ( uint32_t index = 0; index < indexCount; ++index )
	{
		uint32_t value = ReadU32( bytes.data() + offset );
		offset += kIndexBytes;
		if ( value >= vertexCount )
		{
			*error = "preview pack: tile index is out of range";
			return false;
		}
		indices[index] = value;
	}

	if ( offset != bytes.size() || BoundsEqual( actualBounds, record.bounds ) == false )
	{
		*error = "preview pack: tile payload or bounds disagree with manifest";
		return false;
	}

	uint32_t hash = B3_HASH_INIT;
	hash = b3Hash( hash, bytes.data(), (int)bytes.size() );
	hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( revision.data() ), (int)revision.size() );
	MeshHandle handle = FindMesh( hash );
	if ( IsMeshHandleValid( handle ) )
	{
		AddMeshReference( handle );
	}
	else
	{
		handle = RegisterMesh( hash, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size(),
							   "jozz_scan_source_preview_tile" );
	}
	if ( IsMeshHandleValid( handle ) == false )
	{
		*error = "preview pack: renderer rejected tile mesh";
		return false;
	}

	out->tileId = record.tileId;
	out->handle = handle;
	out->vertexCount = (int)vertexCount;
	out->triangleCount = (int)( indexCount / 3 );
	out->bounds = actualBounds;
	out->visible = true;
	return true;
}

Vec4 TileColor( int tileId )
{
	static const Vec4 colors[] = {
		{ 0.78f, 0.34f, 0.26f, 1.0f },
		{ 0.25f, 0.58f, 0.82f, 1.0f },
		{ 0.30f, 0.72f, 0.42f, 1.0f },
		{ 0.70f, 0.44f, 0.80f, 1.0f },
		{ 0.86f, 0.66f, 0.22f, 1.0f },
		{ 0.36f, 0.72f, 0.72f, 1.0f },
		{ 0.82f, 0.42f, 0.62f, 1.0f },
	};
	return colors[tileId % (int)( sizeof( colors ) / sizeof( colors[0] ) )];
}

} // namespace

bool JozzScanPreviewPack::Load( const std::filesystem::path& directory )
{
	Destroy();
	sourcePath = directory;

	std::error_code fileError;
	if ( std::filesystem::is_directory( directory, fileError ) == false || fileError || std::filesystem::is_symlink( directory, fileError ) )
	{
		status = "preview pack: directory missing or linked";
		return false;
	}

	std::vector<ManifestTile> records;
	std::string parseError;
	if ( ParseManifest( directory, &records, &sourceRevisionId, &parseError ) == false )
	{
		status = parseError;
		return false;
	}

	auto fail = [&]( const std::string& message ) {
		Destroy();
		sourcePath = directory;
		status = message;
		return false;
	};

	for ( const ManifestTile& record : records )
	{
		JozzScanPreviewTile tile;
		std::string tileError;
		if ( LoadTile( directory, record, sourceRevisionId, &tile, &tileError ) == false )
		{
			return fail( tileError );
		}

		if ( tiles.empty() )
		{
			bounds = tile.bounds;
		}
		else
		{
			bounds.lowerBound.x = std::min( bounds.lowerBound.x, tile.bounds.lowerBound.x );
			bounds.lowerBound.y = std::min( bounds.lowerBound.y, tile.bounds.lowerBound.y );
			bounds.lowerBound.z = std::min( bounds.lowerBound.z, tile.bounds.lowerBound.z );
			bounds.upperBound.x = std::max( bounds.upperBound.x, tile.bounds.upperBound.x );
			bounds.upperBound.y = std::max( bounds.upperBound.y, tile.bounds.upperBound.y );
			bounds.upperBound.z = std::max( bounds.upperBound.z, tile.bounds.upperBound.z );
		}

		vertexCount += tile.vertexCount;
		triangleCount += tile.triangleCount;
		tiles.push_back( tile );
	}

	loaded = tiles.empty() == false;
	if ( loaded )
	{
		char message[192];
		std::snprintf( message, sizeof( message ), "preview pack: loaded %d tiles, %d vertices, %d triangles", (int)tiles.size(),
					   vertexCount, triangleCount );
		status = message;
	}
	return loaded;
}

void JozzScanPreviewPack::Destroy()
{
	for ( JozzScanPreviewTile& tile : tiles )
	{
		if ( IsMeshHandleValid( tile.handle ) )
		{
			ReleaseMeshReference( tile.handle );
			tile.handle = InvalidMeshHandle();
		}
	}
	tiles.clear();
	sourceRevisionId.clear();
	bounds = {};
	vertexCount = 0;
	triangleCount = 0;
	loaded = false;
	status = "preview pack: not loaded";
}

void JozzScanPreviewPack::Draw( bool showBounds ) const
{
	if ( loaded == false )
	{
		return;
	}

	b3WorldTransform worldTransform = b3WorldTransform_identity;
	b3Transform relativeTransform = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	for ( const JozzScanPreviewTile& tile : tiles )
	{
		if ( tile.visible == false || IsMeshHandleValid( tile.handle ) == false )
		{
			continue;
		}

		Vec4 color = TileColor( tile.tileId );
		AppendMesh( tile.handle, relativeTransform, b3Vec3_one, color, 0.0f, 0.82f, MESH_MATERIAL_MODE_SOLID, 0.0f,
					TRANSPARENT_SHADOW_FULL );
		if ( showBounds )
		{
			DrawAabb( tile.bounds.lowerBound, tile.bounds.upperBound, color );
		}
	}
}

std::filesystem::path FindJozzActiveScanPreviewPack()
{
	const char* environment = std::getenv( "JOZZ_SCAN_PREVIEW_PACK" );
	if ( environment != nullptr && environment[0] != '\0' )
	{
		std::filesystem::path path( environment );
		std::error_code error;
		if ( std::filesystem::is_directory( path, error ) && error == std::error_code{} &&
			 std::filesystem::is_regular_file( path / "COMPLETE.json", error ) )
		{
			return path;
		}
	}

	std::filesystem::path root = std::filesystem::path( "build" ) / "scan_pipeline" / "previews";
	std::error_code error;
	if ( std::filesystem::is_directory( root, error ) == false || error )
	{
		return {};
	}

	std::vector<std::filesystem::path> candidates;
	for ( std::filesystem::directory_iterator iterator( root, error ); error == std::error_code{} && iterator != std::filesystem::directory_iterator();
		  iterator.increment( error ) )
	{
		const std::filesystem::directory_entry& entry = *iterator;
		std::error_code entryError;
		if ( entry.is_directory( entryError ) && entryError == std::error_code{} && entry.is_symlink( entryError ) == false &&
			 std::filesystem::is_regular_file( entry.path() / "COMPLETE.json", entryError ) )
		{
			candidates.push_back( entry.path() );
		}
	}

	if ( error || candidates.size() != 1 )
	{
		return {};
	}

	return candidates[0];
}
