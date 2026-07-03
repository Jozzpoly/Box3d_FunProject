// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_metadata.h"

#define JSMN_STATIC
#include "jsmn.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <string_view>

namespace
{

bool TokenEquals( std::string_view json, const jsmntok_t& token, const char* text )
{
	if ( token.type != JSMN_STRING || token.start < 0 || token.end < token.start )
	{
		return false;
	}

	return json.substr( (size_t)token.start, (size_t)( token.end - token.start ) ) == text;
}

std::string TokenString( std::string_view json, const jsmntok_t& token )
{
	if ( token.start < 0 || token.end < token.start )
	{
		return {};
	}

	return std::string( json.substr( (size_t)token.start, (size_t)( token.end - token.start ) ) );
}

bool TokenFloat( std::string_view json, const jsmntok_t& token, float* out )
{
	if ( token.start < 0 || token.end < token.start )
	{
		return false;
	}

	std::string text = TokenString( json, token );
	char* end = nullptr;
	errno = 0;
	float value = std::strtof( text.c_str(), &end );
	if ( errno != 0 || end == text.c_str() || *end != '\0' )
	{
		return false;
	}

	*out = value;
	return true;
}

int SkipToken( const std::vector<jsmntok_t>& tokens, int index )
{
	if ( index < 0 || index >= (int)tokens.size() )
	{
		return index + 1;
	}

	const jsmntok_t& token = tokens[index];
	int next = index + 1;

	if ( token.type == JSMN_OBJECT )
	{
		for ( int i = 0; i < token.size; ++i )
		{
			next = SkipToken( tokens, next ); // key
			next = SkipToken( tokens, next ); // value
		}
	}
	else if ( token.type == JSMN_ARRAY )
	{
		for ( int i = 0; i < token.size; ++i )
		{
			next = SkipToken( tokens, next );
		}
	}

	return next;
}

int FindObjectValue( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key )
{
	if ( objectIndex < 0 || objectIndex >= (int)tokens.size() || tokens[objectIndex].type != JSMN_OBJECT )
	{
		return -1;
	}

	int current = objectIndex + 1;
	for ( int i = 0; i < tokens[objectIndex].size; ++i )
	{
		int keyIndex = current;
		int valueIndex = SkipToken( tokens, keyIndex );
		if ( keyIndex < (int)tokens.size() && TokenEquals( json, tokens[keyIndex], key ) )
		{
			return valueIndex;
		}

		current = SkipToken( tokens, valueIndex );
	}

	return -1;
}

bool ParseVec3Array( std::string_view json, const std::vector<jsmntok_t>& tokens, int arrayIndex, b3Vec3* out )
{
	if ( arrayIndex < 0 || arrayIndex >= (int)tokens.size() || tokens[arrayIndex].type != JSMN_ARRAY || tokens[arrayIndex].size != 3 )
	{
		return false;
	}

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	if ( TokenFloat( json, tokens[arrayIndex + 1], &x ) == false || TokenFloat( json, tokens[arrayIndex + 2], &y ) == false ||
		 TokenFloat( json, tokens[arrayIndex + 3], &z ) == false )
	{
		return false;
	}

	*out = { x, y, z };
	return true;
}

void AddFallbackPoint( JozzVehicleAuditMetadata* metadata, const char* assetFile, const char* name, b3Vec3 positionBU )
{
	metadata->semanticPoints.push_back( { assetFile, name, positionBU } );
}

void PopulateFallbackMetadata( JozzVehicleAuditMetadata* metadata, const char* reason )
{
	metadata->loadedFromRuntimeReport = false;
	metadata->sourcePath.clear();
	metadata->status = reason;
	metadata->semanticPoints.clear();

	AddFallbackPoint( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount", { 0.25f, 0.5f, 0.0f } );
	AddFallbackPoint( metadata, "Offroad_Big_Wheels.gltf", "Axis_WheelSpin_A", { 0.4375f, 0.5f, 0.0f } );
	AddFallbackPoint( metadata, "Offroad_Big_Wheels.gltf", "Axis_WheelSpin_B", { -1.0625f, 0.5f, 0.0f } );
	AddFallbackPoint( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter", { -0.125f, 1.96875f, 0.0f } );
	AddFallbackPoint( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthLeft", { -0.75f, 0.5f, 0.0f } );
	AddFallbackPoint( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireWidthRight", { 0.5f, 0.5f, 0.0f } );

	AddFallbackPoint( metadata, "One_Sided_wheel_mount.gltf", "Socket_WheelCenter", { -1.1875f, 0.5f, -0.0625f } );
	AddFallbackPoint( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Top", { -1.1875f, 1.5f, 0.0f } );
	AddFallbackPoint( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Bottom", { -1.1875f, -0.5f, 0.0f } );
}

bool ReadTextFile( const char* path, std::string* out )
{
	std::ifstream input( path, std::ios::binary );
	if ( input.is_open() == false )
	{
		return false;
	}

	input.seekg( 0, std::ios::end );
	std::streampos fileSize = input.tellg();
	if ( fileSize <= 0 )
	{
		return false;
	}

	std::streamsize size = static_cast<std::streamsize>( fileSize );
	out->resize( (size_t)size );
	input.seekg( 0, std::ios::beg );
	input.read( out->data(), size );
	return input.good();
}

bool ParseAuditReport( std::string_view json, JozzVehicleAuditMetadata* metadata )
{
	jsmn_parser parser;
	jsmn_init( &parser );
	int tokenCount = jsmn_parse( &parser, json.data(), json.size(), nullptr, 0 );
	if ( tokenCount <= 0 )
	{
		return false;
	}

	std::vector<jsmntok_t> tokens( (size_t)tokenCount );
	jsmn_init( &parser );
	int parsedCount = jsmn_parse( &parser, json.data(), json.size(), tokens.data(), (unsigned int)tokens.size() );
	if ( parsedCount <= 0 || tokens[0].type != JSMN_ARRAY )
	{
		return false;
	}

	metadata->semanticPoints.clear();
	int assetIndex = 1;
	for ( int asset = 0; asset < tokens[0].size; ++asset )
	{
		int nextAsset = SkipToken( tokens, assetIndex );
		if ( assetIndex >= (int)tokens.size() || tokens[assetIndex].type != JSMN_OBJECT )
		{
			assetIndex = nextAsset;
			continue;
		}

		int fileIndex = FindObjectValue( json, tokens, assetIndex, "file" );
		int semanticNodesIndex = FindObjectValue( json, tokens, assetIndex, "semantic_nodes" );
		if ( fileIndex < 0 || semanticNodesIndex < 0 || tokens[semanticNodesIndex].type != JSMN_ARRAY )
		{
			assetIndex = nextAsset;
			continue;
		}

		std::string assetFile = TokenString( json, tokens[fileIndex] );
		int semanticIndex = semanticNodesIndex + 1;
		for ( int node = 0; node < tokens[semanticNodesIndex].size; ++node )
		{
			int nextSemantic = SkipToken( tokens, semanticIndex );
			if ( semanticIndex >= (int)tokens.size() || tokens[semanticIndex].type != JSMN_OBJECT )
			{
				semanticIndex = nextSemantic;
				continue;
			}

			int nameIndex = FindObjectValue( json, tokens, semanticIndex, "name" );
			int positionIndex = FindObjectValue( json, tokens, semanticIndex, "world_position" );
			b3Vec3 positionBU = b3Vec3_zero;
			if ( nameIndex >= 0 && ParseVec3Array( json, tokens, positionIndex, &positionBU ) )
			{
				metadata->semanticPoints.push_back( { assetFile, TokenString( json, tokens[nameIndex] ), positionBU } );
			}

			semanticIndex = nextSemantic;
		}

		assetIndex = nextAsset;
	}

	return metadata->semanticPoints.empty() == false;
}

} // namespace

JozzVehicleAuditMetadata LoadJozzVehicleAuditMetadata()
{
	JozzVehicleAuditMetadata metadata;

	const char* candidates[] = {
		"assets/reports/asset_audit_latest.json",
		"../assets/reports/asset_audit_latest.json",
		"../../assets/reports/asset_audit_latest.json",
		"../../../assets/reports/asset_audit_latest.json",
		"../../../../assets/reports/asset_audit_latest.json",
	};

	std::string json;
	for ( const char* path : candidates )
	{
		json.clear();
		if ( ReadTextFile( path, &json ) && ParseAuditReport( json, &metadata ) )
		{
			metadata.loadedFromRuntimeReport = true;
			metadata.sourcePath = path;
			metadata.status = "loaded runtime asset audit report";
			return metadata;
		}
	}

	PopulateFallbackMetadata( &metadata, "using built-in audited fallback; runtime asset audit report not found" );	
	return metadata;
}

bool FindJozzVehicleSemanticPoint( const JozzVehicleAuditMetadata& metadata, const char* assetFile, const char* semanticName,
								  b3Vec3* outPositionBU )
{
	for ( const JozzVehicleAuditSemanticPoint& point : metadata.semanticPoints )
	{
		if ( point.assetFile == assetFile && point.name == semanticName )
		{
			*outPositionBU = point.positionBU;
			return true;
		}
	}

	return false;
}
