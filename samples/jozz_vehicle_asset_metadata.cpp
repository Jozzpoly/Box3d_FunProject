// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_metadata.h"

#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_json.h"

#include <cstring>

using namespace jozz;

namespace
{

struct BuiltInSemanticPoint
{
	const char* assetFile;
	const char* name;
	b3Vec3 positionBU;
};

// Single authority for the audited fallback values. The runtime report path
// and every caller that needs a literal fallback must go through this table.
const BuiltInSemanticPoint s_builtInPoints[] = {
	{ "Offroad_Big_Wheels.gltf", "Socket_WheelMount", { 0.25f, 0.5f, 0.0f } },
	{ "Offroad_Big_Wheels.gltf", "Axis_WheelSpin_A", { 0.4375f, 0.5f, 0.0f } },
	{ "Offroad_Big_Wheels.gltf", "Axis_WheelSpin_B", { -1.0625f, 0.5f, 0.0f } },
	{ "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter", { -0.125f, 1.96875f, 0.0f } },
	{ "Offroad_Big_Wheels.gltf", "Marker_TireWidthLeft", { -0.75f, 0.5f, 0.0f } },
	{ "Offroad_Big_Wheels.gltf", "Marker_TireWidthRight", { 0.5f, 0.5f, 0.0f } },
	{ "One_Sided_wheel_mount.gltf", "Socket_WheelCenter", { -1.1875f, 0.5f, -0.0625f } },
	{ "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Top", { -1.1875f, 1.5f, 0.0f } },
	{ "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Bottom", { -1.1875f, -0.5f, 0.0f } },
};

bool ParseVec3Array( std::string_view json, const std::vector<jsmntok_t>& tokens, int arrayIndex, b3Vec3* out )
{
	float values[3] = {};
	if ( ParseFloatArray( json, tokens, arrayIndex, values, 3 ) == false )
	{
		return false;
	}

	*out = { values[0], values[1], values[2] };
	return true;
}

void PopulateFallbackMetadata( JozzVehicleAuditMetadata* metadata, const char* reason )
{
	metadata->loadedFromRuntimeReport = false;
	metadata->sourcePath.clear();
	metadata->status = reason;
	metadata->semanticPoints.clear();

	for ( const BuiltInSemanticPoint& point : s_builtInPoints )
	{
		metadata->semanticPoints.push_back( { point.assetFile, point.name, point.positionBU } );
	}
}

bool ParseAuditReport( std::string_view json, const std::vector<jsmntok_t>& tokens, JozzVehicleAuditMetadata* metadata )
{
	if ( tokens.empty() || tokens[0].type != JSMN_ARRAY )
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

	std::string resolvedPath;
	if ( FindJozzVehicleAssetFile( "assets/reports/asset_audit_latest.json", &resolvedPath ) )
	{
		std::string json;
		std::vector<jsmntok_t> tokens;
		if ( ReadTextFile( resolvedPath, &json ) && ParseJson( json, &tokens ) && ParseAuditReport( json, tokens, &metadata ) )
		{
			metadata.loadedFromRuntimeReport = true;
			metadata.sourcePath = resolvedPath;
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

b3Vec3 JozzVehicleBuiltInSemanticPoint( const char* assetFile, const char* semanticName )
{
	for ( const BuiltInSemanticPoint& point : s_builtInPoints )
	{
		if ( std::strcmp( point.assetFile, assetFile ) == 0 && std::strcmp( point.name, semanticName ) == 0 )
		{
			return point.positionBU;
		}
	}

	return b3Vec3_zero;
}

b3Vec3 JozzVehicleFindPointOrBuiltIn( const JozzVehicleAuditMetadata& metadata, const char* assetFile,
									  const char* semanticName )
{
	b3Vec3 position = b3Vec3_zero;
	if ( FindJozzVehicleSemanticPoint( metadata, assetFile, semanticName, &position ) )
	{
		return position;
	}

	return JozzVehicleBuiltInSemanticPoint( assetFile, semanticName );
}
