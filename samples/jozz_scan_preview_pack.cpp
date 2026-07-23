// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_scan_preview_pack.h"

#include "box3d/base.h"
#include "gfx/draw.h"
#include "jozz_vehicle_image_decode.h"
#include "jozz_vehicle_json.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace jozz;

namespace
{

constexpr uint8_t kMagic[8] = { 'J', 'S', 'P', 'R', 'E', 'V', '2', 0 };
constexpr uint32_t kBinaryVersion = 2;
// v2 tile header: magic(8) + version(4) + tileId(4) + groupCount(4).
constexpr size_t kHeaderBytes = 20;
// Per-group descriptor in the header table: vertexCount(4) + indexCount(4).
constexpr size_t kGroupDescBytes = 8;
// Per-vertex: position.xyz + normal.xyz + uv.xy, all float32.
constexpr size_t kVertexBytes = 32;
constexpr size_t kIndexBytes = 4;
constexpr int kMaxTiles = 64;
constexpr int kMaxGroupsPerTile = 4096;
constexpr uint32_t kMaxTextureDim = 1024;
constexpr uint64_t kMaxTextureBytes = 16ull * 1024ull * 1024ull;
constexpr uint64_t kMaxManifestBytes = 2ull * 1024ull * 1024ull;
constexpr uint64_t kMaxTileBytes = 1024ull * 1024ull * 1024ull;

struct ManifestGroup
{
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	uint32_t triangleCount = 0;
	std::string texturePath;
	uint64_t textureByteLength = 0;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
};

struct ManifestTile
{
	int tileId = -1;
	std::string path;
	std::string sha256;
	uint64_t byteLength = 0;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	uint32_t triangleCount = 0;
	uint32_t groupCount = 0;
	b3AABB bounds = {};
	std::vector<ManifestGroup> groups;
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

bool IsLowerHexSha256( const std::string& value )
{
	if ( value.size() != 64 )
	{
		return false;
	}
	for ( char character : value )
	{
		if ( ( character < '0' || character > '9' ) && ( character < 'a' || character > 'f' ) )
		{
			return false;
		}
	}
	return true;
}

bool IsRevisionId( const std::string& value )
{
	return value.size() == 71 && value.rfind( "sha256:", 0 ) == 0 && IsLowerHexSha256( value.substr( 7 ) );
}

bool TokenU64( std::string_view json, const jsmntok_t& token, uint64_t* out )
{
	if ( token.type != JSMN_PRIMITIVE || token.start < 0 || token.end <= token.start )
	{
		return false;
	}

	std::string text = TokenString( json, token );
	if ( text.empty() || text[0] == '-' )
	{
		return false;
	}
	for ( char character : text )
	{
		if ( character < '0' || character > '9' )
		{
			return false;
		}
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

bool ObjectHasExactKeys( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex,
					 std::initializer_list<const char*> expectedKeys )
{
	if ( objectIndex < 0 || objectIndex >= (int)tokens.size() || tokens[objectIndex].type != JSMN_OBJECT ||
		 tokens[objectIndex].size != (int)expectedKeys.size() )
	{
		return false;
	}

	std::set<std::string> expected;
	for ( const char* key : expectedKeys )
	{
		expected.insert( key );
	}
	std::set<std::string> observed;
	int current = objectIndex + 1;
	for ( int index = 0; index < tokens[objectIndex].size; ++index )
	{
		int keyIndex = current;
		int valueIndex = SkipToken( tokens, keyIndex );
		if ( keyIndex < 0 || keyIndex >= (int)tokens.size() || tokens[keyIndex].type != JSMN_STRING )
		{
			return false;
		}
		std::string key = TokenString( json, tokens[keyIndex] );
		if ( expected.contains( key ) == false || observed.insert( key ).second == false )
		{
			return false;
		}
		current = SkipToken( tokens, valueIndex );
	}
	return observed == expected;
}

bool ReadStringMember( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key,
				   std::string* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	if ( valueIndex < 0 || tokens[valueIndex].type != JSMN_STRING )
	{
		return false;
	}
	*out = TokenString( json, tokens[valueIndex] );
	return true;
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
	if ( ObjectHasExactKeys( json, tokens, objectIndex, { "min", "max" } ) == false )
	{
		return false;
	}
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

void MergeBounds( b3AABB* target, b3AABB value, bool first )
{
	if ( first )
	{
		*target = value;
		return;
	}
	target->lowerBound.x = std::min( target->lowerBound.x, value.lowerBound.x );
	target->lowerBound.y = std::min( target->lowerBound.y, value.lowerBound.y );
	target->lowerBound.z = std::min( target->lowerBound.z, value.lowerBound.z );
	target->upperBound.x = std::max( target->upperBound.x, value.upperBound.x );
	target->upperBound.y = std::max( target->upperBound.y, value.upperBound.y );
	target->upperBound.z = std::max( target->upperBound.z, value.upperBound.z );
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

bool IsRealFile( const std::filesystem::path& path )
{
	std::error_code error;
	bool regular = std::filesystem::is_regular_file( path, error );
	if ( error || regular == false )
	{
		return false;
	}
	bool linked = std::filesystem::is_symlink( path, error );
	return error == std::error_code{} && linked == false;
}

bool ReadBytes( const std::filesystem::path& path, uint64_t expectedSize, std::vector<uint8_t>* out )
{
	if ( expectedSize == 0 || expectedSize > kMaxTileBytes || expectedSize > (uint64_t)std::numeric_limits<int>::max() ||
		 IsRealFile( path ) == false )
	{
		return false;
	}

	std::error_code error;
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
	return input.bad() == false && input.gcount() == (std::streamsize)out->size();
}

bool ParseManifest( const std::filesystem::path& directory, std::vector<ManifestTile>* tiles, std::string* revision,
					std::string* error )
{
	std::filesystem::path manifestPath = directory / "COMPLETE.json";
	std::error_code fileError;
	if ( IsRealFile( manifestPath ) == false || std::filesystem::file_size( manifestPath, fileError ) > kMaxManifestBytes || fileError )
	{
		*error = "preview pack: COMPLETE.json missing, linked or too large";
		return false;
	}

	std::string json;
	if ( ReadTextFile( manifestPath, &json ) == false )
	{
		*error = "preview pack: COMPLETE.json could not be read";
		return false;
	}
	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens[0].type != JSMN_OBJECT ||
		 ObjectHasExactKeys( json, tokens, 0,
			{ "schema", "schemaVersion", "status", "privacyClass", "purpose", "sourceBundleContentSha256", "packageId",
			  "sourceRevisionId", "sourceFrameContractSha256", "capabilities", "tileFormat", "levelingCorrection", "tileCount",
			  "globalBoundsLabMeters", "tiles", "previewContentSha256" } ) == false )
	{
		*error = "preview pack: manifest JSON boundary failed";
		return false;
	}

	std::string schema;
	std::string status;
	std::string privacy;
	std::string purpose;
	std::string bundleSha;
	std::string frameSha;
	std::string previewSha;
	std::string packageId;
	uint64_t version = 0;
	if ( ReadStringMember( json, tokens, 0, "schema", &schema ) == false ||
		 ReadU64Member( json, tokens, 0, "schemaVersion", &version ) == false ||
		 ReadStringMember( json, tokens, 0, "status", &status ) == false ||
		 ReadStringMember( json, tokens, 0, "privacyClass", &privacy ) == false ||
		 ReadStringMember( json, tokens, 0, "purpose", &purpose ) == false ||
		 ReadStringMember( json, tokens, 0, "sourceBundleContentSha256", &bundleSha ) == false ||
		 ReadStringMember( json, tokens, 0, "sourceFrameContractSha256", &frameSha ) == false ||
		 ReadStringMember( json, tokens, 0, "previewContentSha256", &previewSha ) == false ||
		 ReadStringMember( json, tokens, 0, "packageId", &packageId ) == false ||
		 ReadStringMember( json, tokens, 0, "sourceRevisionId", revision ) == false )
	{
		*error = "preview pack: required manifest fields are missing";
		return false;
	}
	if ( schema != "jozz.scan-source-visual-preview-pack" || version != kBinaryVersion || status != "COMPLETE" ||
		 privacy != "PRIVATE_LOCAL_ONLY" || purpose != "SOURCE_VISUAL_PREVIEW_ONLY" || packageId.empty() ||
		 IsLowerHexSha256( bundleSha ) == false || IsLowerHexSha256( frameSha ) == false ||
		 IsLowerHexSha256( previewSha ) == false || IsRevisionId( *revision ) == false )
	{
		*error = "preview pack: schema, identity, privacy or purpose mismatch";
		return false;
	}

	int capabilitiesIndex = FindObjectValue( json, tokens, 0, "capabilities" );
	bool sourceVisible = false;
	bool textures = false;
	bool correspondence = true;
	bool acceptedWorld = true;
	bool collisionReady = true;
	if ( ObjectHasExactKeys( json, tokens, capabilitiesIndex,
			{ "sourceGeometryVisible", "texturesIncluded", "internalGeometryCorrespondencePassed", "acceptedWorld", "collisionReady" } ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "sourceGeometryVisible", &sourceVisible ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "texturesIncluded", &textures ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "internalGeometryCorrespondencePassed", &correspondence ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "acceptedWorld", &acceptedWorld ) == false ||
		 ReadBoolMember( json, tokens, capabilitiesIndex, "collisionReady", &collisionReady ) == false ||
		 sourceVisible == false || textures == false || correspondence || acceptedWorld || collisionReady )
	{
		*error = "preview pack: capability overclaim or incomplete boundary";
		return false;
	}

	int formatIndex = FindObjectValue( json, tokens, 0, "tileFormat" );
	std::string formatMagic;
	std::string groupLayout;
	std::string vertexLayout;
	std::string indexLayout;
	std::string textureLayout;
	uint64_t formatVersion = 0;
	if ( ObjectHasExactKeys( json, tokens, formatIndex,
			{ "magic", "version", "groupLayout", "vertexLayout", "indexLayout", "textureLayout" } ) == false ||
		 ReadStringMember( json, tokens, formatIndex, "magic", &formatMagic ) == false ||
		 ReadU64Member( json, tokens, formatIndex, "version", &formatVersion ) == false ||
		 ReadStringMember( json, tokens, formatIndex, "groupLayout", &groupLayout ) == false ||
		 ReadStringMember( json, tokens, formatIndex, "vertexLayout", &vertexLayout ) == false ||
		 ReadStringMember( json, tokens, formatIndex, "indexLayout", &indexLayout ) == false ||
		 ReadStringMember( json, tokens, formatIndex, "textureLayout", &textureLayout ) == false ||
		 formatMagic != "JSPREV2" || formatVersion != kBinaryVersion ||
		 groupLayout != "int32 groupCount, then (vertexCount,indexCount) table" ||
		 vertexLayout != "float32 position.xyz + float32 normal.xyz + float32 uv.xy" ||
		 indexLayout != "uint32 triangle-list, group-local" ||
		 textureLayout != "baseColor png rgba8, longest side <= 1024" )
	{
		*error = "preview pack: tile format contract mismatch";
		return false;
	}

	// levelingCorrection: a per-scan tilt already baked into the tile bytes. The
	// reader does not re-apply it; it only validates the contract shape for
	// defense in depth (the Python verifier is the cryptographic trust boundary).
	int levelingIndex = FindObjectValue( json, tokens, 0, "levelingCorrection" );
	int levelingAxesIndex = FindObjectValue( json, tokens, levelingIndex, "labAxisDegrees" );
	std::string levelingOrder;
	bool levelingApplied = false;
	if ( ObjectHasExactKeys( json, tokens, levelingIndex, { "labAxisDegrees", "order", "applied" } ) == false ||
		 ObjectHasExactKeys( json, tokens, levelingAxesIndex, { "x", "z" } ) == false ||
		 ReadStringMember( json, tokens, levelingIndex, "order", &levelingOrder ) == false ||
		 ReadBoolMember( json, tokens, levelingIndex, "applied", &levelingApplied ) == false ||
		 levelingOrder != "Rz*Rx" )
	{
		*error = "preview pack: leveling correction contract mismatch";
		return false;
	}
	(void)levelingApplied;

	uint64_t tileCount64 = 0;
	int tilesIndex = FindObjectValue( json, tokens, 0, "tiles" );
	if ( ReadU64Member( json, tokens, 0, "tileCount", &tileCount64 ) == false || tileCount64 == 0 || tileCount64 > kMaxTiles ||
		 tilesIndex < 0 || tokens[tilesIndex].type != JSMN_ARRAY || tokens[tilesIndex].size != (int)tileCount64 )
	{
		*error = "preview pack: tileCount or tiles array is invalid";
		return false;
	}

	int globalBoundsIndex = FindObjectValue( json, tokens, 0, "globalBoundsLabMeters" );
	b3AABB declaredGlobal = {};
	if ( ReadBounds( json, tokens, globalBoundsIndex, &declaredGlobal ) == false )
	{
		*error = "preview pack: global bounds are invalid";
		return false;
	}

	tiles->clear();
	tiles->reserve( (size_t)tileCount64 );
	std::set<int> seen;
	int previousTileId = -1;
	b3AABB mergedBounds = {};
	for ( int index = 0; index < (int)tileCount64; ++index )
	{
		int recordIndex = GetArrayElement( tokens, tilesIndex, index );
		if ( ObjectHasExactKeys( json, tokens, recordIndex,
				{ "tileId", "vertexCount", "indexCount", "triangleCount", "groupCount", "boundsLabMeters", "path", "byteLength",
				  "sha256", "groups" } ) == false )
		{
			*error = "preview pack: tile record boundary failed";
			return false;
		}

		ManifestTile tile;
		uint64_t tileId64 = 0;
		uint64_t vertexCount = 0;
		uint64_t indexCount = 0;
		uint64_t triangleCount = 0;
		uint64_t groupCount64 = 0;
		int boundsIndex = FindObjectValue( json, tokens, recordIndex, "boundsLabMeters" );
		if ( ReadU64Member( json, tokens, recordIndex, "tileId", &tileId64 ) == false || tileId64 > (uint64_t)std::numeric_limits<int>::max() ||
			 ReadStringMember( json, tokens, recordIndex, "path", &tile.path ) == false ||
			 ReadStringMember( json, tokens, recordIndex, "sha256", &tile.sha256 ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "byteLength", &tile.byteLength ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "vertexCount", &vertexCount ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "indexCount", &indexCount ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "triangleCount", &triangleCount ) == false ||
			 ReadU64Member( json, tokens, recordIndex, "groupCount", &groupCount64 ) == false ||
			 ReadBounds( json, tokens, boundsIndex, &tile.bounds ) == false )
		{
			*error = "preview pack: malformed tile record";
			return false;
		}
		tile.tileId = (int)tileId64;
		if ( tile.tileId <= previousTileId || seen.insert( tile.tileId ).second == false || IsLowerHexSha256( tile.sha256 ) == false ||
			 vertexCount == 0 || vertexCount > std::numeric_limits<uint32_t>::max() ||
			 indexCount == 0 || indexCount > std::numeric_limits<uint32_t>::max() ||
			 triangleCount == 0 || triangleCount > std::numeric_limits<uint32_t>::max() ||
			 indexCount != 3 * triangleCount || groupCount64 == 0 || groupCount64 > (uint64_t)kMaxGroupsPerTile ||
			 IsSafeRelativePath( tile.path ) == false )
		{
			*error = "preview pack: non-canonical tile identity, counts, hash or path";
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
		tile.groupCount = (uint32_t)groupCount64;

		int groupsIndex = FindObjectValue( json, tokens, recordIndex, "groups" );
		if ( groupsIndex < 0 || tokens[groupsIndex].type != JSMN_ARRAY || tokens[groupsIndex].size != (int)groupCount64 )
		{
			*error = "preview pack: groups array does not match groupCount";
			return false;
		}

		uint64_t groupVertexTotal = 0;
		uint64_t groupIndexTotal = 0;
		uint64_t payloadBytes = 0;
		tile.groups.reserve( (size_t)groupCount64 );
		for ( int groupOrdinal = 0; groupOrdinal < (int)groupCount64; ++groupOrdinal )
		{
			int groupRecord = GetArrayElement( tokens, groupsIndex, groupOrdinal );
			if ( ObjectHasExactKeys( json, tokens, groupRecord,
					{ "groupId", "vertexCount", "indexCount", "triangleCount", "texturePath", "textureByteLength", "textureSha256",
					  "textureWidth", "textureHeight" } ) == false )
			{
				*error = "preview pack: group record boundary failed";
				return false;
			}

			ManifestGroup group;
			uint64_t groupId64 = 0;
			uint64_t groupVertexCount = 0;
			uint64_t groupIndexCount = 0;
			uint64_t groupTriangleCount = 0;
			uint64_t textureWidth = 0;
			uint64_t textureHeight = 0;
			std::string textureSha;
			if ( ReadU64Member( json, tokens, groupRecord, "groupId", &groupId64 ) == false ||
				 ReadU64Member( json, tokens, groupRecord, "vertexCount", &groupVertexCount ) == false ||
				 ReadU64Member( json, tokens, groupRecord, "indexCount", &groupIndexCount ) == false ||
				 ReadU64Member( json, tokens, groupRecord, "triangleCount", &groupTriangleCount ) == false ||
				 ReadStringMember( json, tokens, groupRecord, "texturePath", &group.texturePath ) == false ||
				 ReadU64Member( json, tokens, groupRecord, "textureByteLength", &group.textureByteLength ) == false ||
				 ReadStringMember( json, tokens, groupRecord, "textureSha256", &textureSha ) == false ||
				 ReadU64Member( json, tokens, groupRecord, "textureWidth", &textureWidth ) == false ||
				 ReadU64Member( json, tokens, groupRecord, "textureHeight", &textureHeight ) == false )
			{
				*error = "preview pack: malformed group record";
				return false;
			}
			if ( groupId64 != (uint64_t)groupOrdinal || groupVertexCount < 3 || groupVertexCount > std::numeric_limits<uint32_t>::max() ||
				 groupIndexCount < 3 || groupIndexCount > std::numeric_limits<uint32_t>::max() || groupIndexCount % 3 != 0 ||
				 groupIndexCount != 3 * groupTriangleCount || IsLowerHexSha256( textureSha ) == false ||
				 group.textureByteLength == 0 || group.textureByteLength > kMaxTextureBytes ||
				 textureWidth == 0 || textureWidth > kMaxTextureDim || textureHeight == 0 || textureHeight > kMaxTextureDim ||
				 IsSafeRelativePath( group.texturePath ) == false )
			{
				*error = "preview pack: non-canonical group counts, hash, texture size or path";
				return false;
			}

			char expectedTexture[80];
			std::snprintf( expectedTexture, sizeof( expectedTexture ), "textures/tile_%03d_group_%03d.png", tile.tileId, groupOrdinal );
			if ( group.texturePath != expectedTexture )
			{
				*error = "preview pack: group texture path does not match tile and group id";
				return false;
			}

			group.vertexCount = (uint32_t)groupVertexCount;
			group.indexCount = (uint32_t)groupIndexCount;
			group.triangleCount = (uint32_t)groupTriangleCount;
			group.textureWidth = (uint32_t)textureWidth;
			group.textureHeight = (uint32_t)textureHeight;
			groupVertexTotal += groupVertexCount;
			groupIndexTotal += groupIndexCount;
			payloadBytes += groupVertexCount * kVertexBytes + groupIndexCount * kIndexBytes;
			tile.groups.push_back( group );
		}

		if ( groupVertexTotal != vertexCount || groupIndexTotal != indexCount )
		{
			*error = "preview pack: group counts do not sum to tile totals";
			return false;
		}

		uint64_t expectedBytes = kHeaderBytes + groupCount64 * kGroupDescBytes + payloadBytes;
		if ( tile.byteLength != expectedBytes || tile.byteLength > kMaxTileBytes )
		{
			*error = "preview pack: tile byteLength does not match binary counts";
			return false;
		}

		MergeBounds( &mergedBounds, tile.bounds, tiles->empty() );
		previousTileId = tile.tileId;
		tiles->push_back( tile );
	}

	if ( BoundsEqual( mergedBounds, declaredGlobal ) == false )
	{
		*error = "preview pack: global bounds do not match tile bounds";
		return false;
	}
	return true;
}

bool ValidateFileSet( const std::filesystem::path& root, const std::vector<ManifestTile>& records )
{
	std::set<std::string> expected = { "COMPLETE.json" };
	for ( const ManifestTile& record : records )
	{
		expected.insert( record.path );
		for ( const ManifestGroup& group : record.groups )
		{
			expected.insert( group.texturePath );
		}
	}
	std::set<std::string> actual;
	std::error_code error;
	for ( std::filesystem::recursive_directory_iterator iterator( root, error );
		  error == std::error_code{} && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment( error ) )
	{
		const std::filesystem::directory_entry& entry = *iterator;
		std::error_code entryError;
		if ( entry.is_symlink( entryError ) || entryError )
		{
			return false;
		}
		if ( entry.is_regular_file( entryError ) )
		{
			if ( entryError )
			{
				return false;
			}
			std::filesystem::path relative = entry.path().lexically_relative( root );
			actual.insert( relative.generic_string() );
		}
	}
	return error == std::error_code{} && actual == expected;
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
	if ( bytes.size() < kHeaderBytes || std::memcmp( bytes.data(), kMagic, sizeof( kMagic ) ) != 0 )
	{
		*error = "preview pack: tile header magic mismatch";
		return false;
	}

	uint32_t version = ReadU32( bytes.data() + 8 );
	uint32_t tileId = ReadU32( bytes.data() + 12 );
	uint32_t groupCount = ReadU32( bytes.data() + 16 );
	if ( version != kBinaryVersion || tileId != (uint32_t)record.tileId || groupCount != record.groupCount ||
		 groupCount == 0 || groupCount > (uint32_t)kMaxGroupsPerTile )
	{
		*error = "preview pack: tile header disagrees with manifest";
		return false;
	}

	// Read the group descriptor table and cross-check it against the manifest.
	size_t offset = kHeaderBytes;
	if ( bytes.size() < offset + (size_t)groupCount * kGroupDescBytes )
	{
		*error = "preview pack: tile group table is truncated";
		return false;
	}
	for ( uint32_t g = 0; g < groupCount; ++g )
	{
		uint32_t vc = ReadU32( bytes.data() + offset );
		uint32_t ic = ReadU32( bytes.data() + offset + 4 );
		offset += kGroupDescBytes;
		if ( vc != record.groups[g].vertexCount || ic != record.groups[g].indexCount || ic % 3 != 0 )
		{
			*error = "preview pack: tile group table disagrees with manifest";
			return false;
		}
	}

	// Release any textured group meshes already registered if we fail partway.
	auto releaseGroups = [&]() {
		for ( JozzScanPreviewTileGroup& registered : out->groups )
		{
			if ( IsMeshHandleValid( registered.handle ) )
			{
				ReleaseMeshReference( registered.handle );
				registered.handle = InvalidMeshHandle();
			}
		}
		out->groups.clear();
	};

	b3AABB actualBounds = {
		{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() },
		{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() },
	};
	int totalVertices = 0;
	int totalTriangles = 0;
	out->groups.clear();
	out->groups.reserve( groupCount );

	for ( uint32_t g = 0; g < groupCount; ++g )
	{
		uint32_t vertexCount = record.groups[g].vertexCount;
		uint32_t indexCount = record.groups[g].indexCount;

		std::vector<MeshVertex> vertices( vertexCount );
		for ( uint32_t index = 0; index < vertexCount; ++index )
		{
			float values[8];
			for ( int component = 0; component < 8; ++component )
			{
				values[component] = ReadF32( bytes.data() + offset + (size_t)component * 4 );
				if ( b3IsValidFloat( values[component] ) == false )
				{
					releaseGroups();
					*error = "preview pack: tile contains non-finite vertex data";
					return false;
				}
			}
			offset += kVertexBytes;
			float normalLength = std::sqrt( values[3] * values[3] + values[4] * values[4] + values[5] * values[5] );
			if ( std::fabs( normalLength - 1.0f ) > 1.0e-3f )
			{
				releaseGroups();
				*error = "preview pack: tile contains non-unit normal";
				return false;
			}

			MeshVertex vertex = {};
			vertex.position[0] = values[0];
			vertex.position[1] = values[1];
			vertex.position[2] = values[2];
			vertex.normal[0] = values[3];
			vertex.normal[1] = values[4];
			vertex.normal[2] = values[5];
			vertex.texcoord[0] = values[6];
			vertex.texcoord[1] = values[7];
			vertices[index] = vertex;

			actualBounds.lowerBound.x = std::min( actualBounds.lowerBound.x, values[0] );
			actualBounds.lowerBound.y = std::min( actualBounds.lowerBound.y, values[1] );
			actualBounds.lowerBound.z = std::min( actualBounds.lowerBound.z, values[2] );
			actualBounds.upperBound.x = std::max( actualBounds.upperBound.x, values[0] );
			actualBounds.upperBound.y = std::max( actualBounds.upperBound.y, values[1] );
			actualBounds.upperBound.z = std::max( actualBounds.upperBound.z, values[2] );
		}

		std::vector<uint32_t> indices( indexCount );
		for ( uint32_t index = 0; index < indexCount; ++index )
		{
			uint32_t value = ReadU32( bytes.data() + offset );
			offset += kIndexBytes;
			if ( value >= vertexCount )
			{
				releaseGroups();
				*error = "preview pack: tile index is out of range";
				return false;
			}
			indices[index] = value;
		}

		// Decode the group's downscaled baseColor texture (png rgba8).
		std::vector<uint8_t> textureBytes;
		if ( ReadBytes( root / std::filesystem::path( record.groups[g].texturePath ), record.groups[g].textureByteLength,
						&textureBytes ) == false )
		{
			releaseGroups();
			*error = "preview pack: group texture missing, linked or wrong size";
			return false;
		}
		JozzVehicleDecodedImage decoded;
		if ( DecodeJozzVehiclePngRgba8( textureBytes.data(), textureBytes.size(), &decoded ) == false ||
			 decoded.width != (int)record.groups[g].textureWidth || decoded.height != (int)record.groups[g].textureHeight ||
			 decoded.rgba8.size() != (size_t)decoded.width * (size_t)decoded.height * 4u )
		{
			releaseGroups();
			*error = "preview pack: group texture failed to decode or disagrees with manifest";
			return false;
		}

		uint32_t hash = B3_HASH_INIT;
		hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( vertices.data() ), (int)( vertices.size() * sizeof( MeshVertex ) ) );
		hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( indices.data() ), (int)( indices.size() * sizeof( uint32_t ) ) );
		hash = b3Hash( hash, decoded.rgba8.data(), (int)decoded.rgba8.size() );
		hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( revision.data() ), (int)revision.size() );
		if ( hash == 0u )
		{
			hash = 1u;
		}

		MeshHandle handle = FindMesh( hash );
		if ( IsMeshHandleValid( handle ) )
		{
			AddMeshReference( handle );
		}
		else
		{
			MeshTextureData textureData = {};
			textureData.width = decoded.width;
			textureData.height = decoded.height;
			textureData.rgba8 = decoded.rgba8.data();
			textureData.byteCount = (int)decoded.rgba8.size();
			handle = RegisterTexturedMesh( hash, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size(),
										   &textureData, "jozz_scan_source_preview_group" );
		}
		if ( IsMeshHandleValid( handle ) == false )
		{
			releaseGroups();
			*error = "preview pack: renderer rejected textured group mesh";
			return false;
		}

		JozzScanPreviewTileGroup group;
		group.handle = handle;
		group.vertexCount = (int)vertexCount;
		group.triangleCount = (int)( indexCount / 3 );
		out->groups.push_back( group );
		totalVertices += (int)vertexCount;
		totalTriangles += (int)( indexCount / 3 );
	}

	if ( offset != bytes.size() || BoundsEqual( actualBounds, record.bounds ) == false )
	{
		releaseGroups();
		*error = "preview pack: tile payload or bounds disagree with manifest";
		return false;
	}

	out->tileId = record.tileId;
	out->vertexCount = totalVertices;
	out->triangleCount = totalTriangles;
	out->bounds = actualBounds;
	out->visible = true;
	return true;
}

Vec4 TileColor( int tileId )
{
	static const Vec4 colors[] = {
		{ 0.78f, 0.34f, 0.26f, 1.0f }, { 0.25f, 0.58f, 0.82f, 1.0f }, { 0.30f, 0.72f, 0.42f, 1.0f },
		{ 0.70f, 0.44f, 0.80f, 1.0f }, { 0.86f, 0.66f, 0.22f, 1.0f }, { 0.36f, 0.72f, 0.72f, 1.0f },
		{ 0.82f, 0.42f, 0.62f, 1.0f },
	};
	return colors[tileId % (int)( sizeof( colors ) / sizeof( colors[0] ) )];
}

bool IsCompleteDirectory( const std::filesystem::path& path )
{
	std::error_code error;
	if ( std::filesystem::is_directory( path, error ) == false || error || std::filesystem::is_symlink( path, error ) )
	{
		return false;
	}
	return IsRealFile( path / "COMPLETE.json" );
}

} // namespace

bool JozzScanPreviewPack::Load( const std::filesystem::path& directory )
{
	Destroy();
	sourcePath = directory;
	if ( IsCompleteDirectory( directory ) == false )
	{
		status = "preview pack: directory missing, linked or incomplete";
		return false;
	}

	std::vector<ManifestTile> records;
	std::string parseError;
	if ( ParseManifest( directory, &records, &sourceRevisionId, &parseError ) == false )
	{
		status = parseError;
		return false;
	}
	if ( ValidateFileSet( directory, records ) == false )
	{
		status = "preview pack: missing, linked or unexpected files";
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
		if ( tile.vertexCount > std::numeric_limits<int>::max() - vertexCount ||
			 tile.triangleCount > std::numeric_limits<int>::max() - triangleCount )
		{
			for ( JozzScanPreviewTileGroup& group : tile.groups )
			{
				if ( IsMeshHandleValid( group.handle ) )
				{
					ReleaseMeshReference( group.handle );
				}
			}
			return fail( "preview pack: total geometry count exceeds runtime limits" );
		}
		MergeBounds( &bounds, tile.bounds, tiles.empty() );
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
		for ( JozzScanPreviewTileGroup& group : tile.groups )
		{
			if ( IsMeshHandleValid( group.handle ) )
			{
				ReleaseMeshReference( group.handle );
				group.handle = InvalidMeshHandle();
			}
		}
		tile.groups.clear();
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
	b3Transform relativeTransform = b3ToRelativeTransform( b3WorldTransform_identity, GetDrawOrigin() );
	const Vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
	for ( const JozzScanPreviewTile& tile : tiles )
	{
		if ( tile.visible == false )
		{
			continue;
		}
		for ( const JozzScanPreviewTileGroup& group : tile.groups )
		{
			if ( IsMeshHandleValid( group.handle ) == false )
			{
				continue;
			}
			// White tint so the source baseColor texture renders unmodified.
			AppendMesh( group.handle, relativeTransform, b3Vec3_one, white, 0.0f, 0.58f, MESH_MATERIAL_MODE_TEXTURED, 0.0f,
						TRANSPARENT_SHADOW_FULL );
		}
		if ( showBounds )
		{
			DrawAabb( tile.bounds.lowerBound, tile.bounds.upperBound, TileColor( tile.tileId ) );
		}
	}
}

std::filesystem::path FindJozzActiveScanPreviewPack()
{
	const char* environment = std::getenv( "JOZZ_SCAN_PREVIEW_PACK" );
	if ( environment != nullptr && environment[0] != '\0' )
	{
		std::filesystem::path path( environment );
		if ( IsCompleteDirectory( path ) )
		{
			return path;
		}
	}

	std::filesystem::path root = std::filesystem::path( "build" ) / "scan_pipeline" / "previews";
	std::error_code error;
	if ( std::filesystem::is_directory( root, error ) == false || error || std::filesystem::is_symlink( root, error ) )
	{
		return {};
	}

	std::vector<std::filesystem::path> candidates;
	for ( std::filesystem::directory_iterator iterator( root, error );
		  error == std::error_code{} && iterator != std::filesystem::directory_iterator(); iterator.increment( error ) )
	{
		if ( IsCompleteDirectory( iterator->path() ) )
		{
			candidates.push_back( iterator->path() );
		}
	}
	if ( error || candidates.size() != 1 )
	{
		return {};
	}
	return candidates[0];
}
