// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_contract.h"

#include "jozz_vehicle_json.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>

using namespace jozz;

namespace
{

struct Matrix4
{
	float m[4][4];
};

struct GltfContractNode
{
	std::string name;
	std::vector<int> children;
	Matrix4 local;
};

Matrix4 IdentityMatrix()
{
	Matrix4 out = {};
	out.m[0][0] = 1.0f;
	out.m[1][1] = 1.0f;
	out.m[2][2] = 1.0f;
	out.m[3][3] = 1.0f;
	return out;
}

Matrix4 MulMatrix( const Matrix4& a, const Matrix4& b )
{
	Matrix4 out = {};
	for ( int r = 0; r < 4; ++r )
	{
		for ( int c = 0; c < 4; ++c )
		{
			for ( int k = 0; k < 4; ++k )
			{
				out.m[r][c] += a.m[r][k] * b.m[k][c];
			}
		}
	}
	return out;
}

b3Vec3 TransformPoint( const Matrix4& m, b3Vec3 p )
{
	return { m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
			 m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
			 m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3] };
}

Matrix4 NodeMatrixFromTRS( b3Vec3 translation, b3Quat rotation, b3Vec3 scale )
{
	b3Quat q = rotation;
	float n = std::sqrt( q.v.x * q.v.x + q.v.y * q.v.y + q.v.z * q.v.z + q.s * q.s );
	if ( n > 0.0f )
	{
		q.v.x /= n;
		q.v.y /= n;
		q.v.z /= n;
		q.s /= n;
	}

	float x = q.v.x;
	float y = q.v.y;
	float z = q.v.z;
	float w = q.s;

	float r00 = 1.0f - 2.0f * y * y - 2.0f * z * z;
	float r01 = 2.0f * x * y - 2.0f * z * w;
	float r02 = 2.0f * x * z + 2.0f * y * w;
	float r10 = 2.0f * x * y + 2.0f * z * w;
	float r11 = 1.0f - 2.0f * x * x - 2.0f * z * z;
	float r12 = 2.0f * y * z - 2.0f * x * w;
	float r20 = 2.0f * x * z - 2.0f * y * w;
	float r21 = 2.0f * y * z + 2.0f * x * w;
	float r22 = 1.0f - 2.0f * x * x - 2.0f * y * y;

	Matrix4 result = IdentityMatrix();
	result.m[0][0] = r00 * scale.x;
	result.m[0][1] = r01 * scale.y;
	result.m[0][2] = r02 * scale.z;
	result.m[0][3] = translation.x;
	result.m[1][0] = r10 * scale.x;
	result.m[1][1] = r11 * scale.y;
	result.m[1][2] = r12 * scale.z;
	result.m[1][3] = translation.y;
	result.m[2][0] = r20 * scale.x;
	result.m[2][1] = r21 * scale.y;
	result.m[2][2] = r22 * scale.z;
	result.m[2][3] = translation.z;
	return result;
}

bool ParseVec3Member( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key, b3Vec3* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	float values[3] = {};
	if ( ParseFloatArray( json, tokens, valueIndex, values, 3 ) == false )
	{
		return false;
	}

	*out = { values[0], values[1], values[2] };
	return true;
}

bool ParseQuatMember( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key, b3Quat* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	float values[4] = {};
	if ( ParseFloatArray( json, tokens, valueIndex, values, 4 ) == false )
	{
		return false;
	}

	*out = { { values[0], values[1], values[2] }, values[3] };
	return true;
}

bool ParseMatrixMember( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, Matrix4* out )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, "matrix" );
	float values[16] = {};
	if ( ParseFloatArray( json, tokens, valueIndex, values, 16 ) == false )
	{
		return false;
	}

	// glTF stores matrices column-major. The local helper uses row-major.
	*out = IdentityMatrix();
	for ( int r = 0; r < 4; ++r )
	{
		for ( int c = 0; c < 4; ++c )
		{
			out->m[r][c] = values[c * 4 + r];
		}
	}
	return true;
}

bool ParseGltfNodes( std::string_view json, const std::vector<jsmntok_t>& tokens, std::vector<GltfContractNode>* out )
{
	int nodesIndex = FindObjectValue( json, tokens, 0, "nodes" );
	if ( nodesIndex < 0 || nodesIndex >= (int)tokens.size() || tokens[nodesIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	out->clear();
	for ( int i = 0; i < tokens[nodesIndex].size; ++i )
	{
		int nodeIndex = GetArrayElement( tokens, nodesIndex, i );
		if ( nodeIndex < 0 || tokens[nodeIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		GltfContractNode node;
		node.local = IdentityMatrix();

		int valueIndex = FindObjectValue( json, tokens, nodeIndex, "name" );
		if ( valueIndex >= 0 )
		{
			node.name = TokenString( json, tokens[valueIndex] );
		}

		if ( ParseMatrixMember( json, tokens, nodeIndex, &node.local ) == false )
		{
			b3Vec3 translation = b3Vec3_zero;
			b3Quat rotation = b3Quat_identity;
			b3Vec3 scale = b3Vec3_one;
			ParseVec3Member( json, tokens, nodeIndex, "translation", &translation );
			ParseQuatMember( json, tokens, nodeIndex, "rotation", &rotation );
			ParseVec3Member( json, tokens, nodeIndex, "scale", &scale );
			node.local = NodeMatrixFromTRS( translation, rotation, scale );
		}

		valueIndex = FindObjectValue( json, tokens, nodeIndex, "children" );
		if ( valueIndex >= 0 && tokens[valueIndex].type == JSMN_ARRAY )
		{
			for ( int child = 0; child < tokens[valueIndex].size; ++child )
			{
				int childToken = GetArrayElement( tokens, valueIndex, child );
				int childIndex = -1;
				if ( childToken >= 0 && TokenInt( json, tokens[childToken], &childIndex ) )
				{
					node.children.push_back( childIndex );
				}
			}
		}

		out->push_back( node );
	}

	return true;
}

void ComputeWorldNodeMatrices( const std::vector<GltfContractNode>& nodes, int index, const Matrix4& parent, std::vector<Matrix4>* world )
{
	if ( index < 0 || index >= (int)nodes.size() )
	{
		return;
	}

	( *world )[index] = MulMatrix( parent, nodes[index].local );
	for ( int child : nodes[index].children )
	{
		ComputeWorldNodeMatrices( nodes, child, ( *world )[index], world );
	}
}

std::filesystem::path FindReadablePath( const char* directPath, const char* folder )
{
	std::filesystem::path candidates[] = {
		std::filesystem::path( directPath ),
		std::filesystem::path( folder ) / directPath,
		std::filesystem::path( ".." ) / folder / directPath,
		std::filesystem::path( ".." ) / ".." / folder / directPath,
		std::filesystem::path( ".." ) / ".." / ".." / folder / directPath,
		std::filesystem::path( ".." ) / ".." / ".." / ".." / folder / directPath,
	};

	for ( const std::filesystem::path& path : candidates )
	{
		std::error_code ec;
		if ( std::filesystem::exists( path, ec ) )
		{
			return path.lexically_normal();
		}
	}

	return {};
}

std::string PathString( const std::filesystem::path& path )
{
	return path.lexically_normal().generic_string();
}

bool ParseBindingObject( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const std::string& path,
						 JozzVehicleContractBinding* out )
{
	if ( objectIndex < 0 || objectIndex >= (int)tokens.size() || tokens[objectIndex].type != JSMN_OBJECT )
	{
		return false;
	}

	int roleIndex = FindObjectValue( json, tokens, objectIndex, "role" );
	int categoryIndex = FindObjectValue( json, tokens, objectIndex, "roleCategory" );
	int nameIndex = FindObjectValue( json, tokens, objectIndex, "nameHint" );
	if ( roleIndex < 0 || categoryIndex < 0 || nameIndex < 0 )
	{
		return false;
	}

	JozzVehicleContractBinding binding;
	binding.path = path;
	binding.role = TokenString( json, tokens[roleIndex] );
	binding.roleCategory = TokenString( json, tokens[categoryIndex] );
	binding.nameHint = TokenString( json, tokens[nameIndex] );

	int valueIndex = FindObjectValue( json, tokens, objectIndex, "nodePathHint" );
	if ( valueIndex >= 0 )
	{
		binding.nodePathHint = TokenString( json, tokens[valueIndex] );
	}

	valueIndex = FindObjectValue( json, tokens, objectIndex, "space" );
	if ( valueIndex >= 0 )
	{
		binding.space = TokenString( json, tokens[valueIndex] );
	}

	valueIndex = FindObjectValue( json, tokens, objectIndex, "nodeIndexHint" );
	if ( valueIndex >= 0 )
	{
		TokenInt( json, tokens[valueIndex], &binding.nodeIndexHint );
	}

	valueIndex = FindObjectValue( json, tokens, objectIndex, "required" );
	if ( valueIndex >= 0 )
	{
		TokenBool( json, tokens[valueIndex], &binding.required );
	}

	valueIndex = FindObjectValue( json, tokens, objectIndex, "physicsAuthority" );
	if ( valueIndex >= 0 )
	{
		TokenBool( json, tokens[valueIndex], &binding.physicsAuthority );
	}

	*out = binding;
	return true;
}

void CollectBindingObjects( std::string_view json, const std::vector<jsmntok_t>& tokens, int tokenIndex, const std::string& path,
							std::vector<JozzVehicleContractBinding>* bindings )
{
	if ( tokenIndex < 0 || tokenIndex >= (int)tokens.size() )
	{
		return;
	}

	JozzVehicleContractBinding binding;
	if ( ParseBindingObject( json, tokens, tokenIndex, path, &binding ) )
	{
		bindings->push_back( binding );
		return;
	}

	if ( tokens[tokenIndex].type == JSMN_OBJECT )
	{
		int current = tokenIndex + 1;
		for ( int i = 0; i < tokens[tokenIndex].size; ++i )
		{
			int keyIndex = current;
			int valueIndex = SkipToken( tokens, keyIndex );
			std::string key = TokenString( json, tokens[keyIndex] );
			std::string childPath = path.empty() ? key : path + "." + key;
			CollectBindingObjects( json, tokens, valueIndex, childPath, bindings );
			current = SkipToken( tokens, valueIndex );
		}
	}
	else if ( tokens[tokenIndex].type == JSMN_ARRAY )
	{
		for ( int i = 0; i < tokens[tokenIndex].size; ++i )
		{
			int valueIndex = GetArrayElement( tokens, tokenIndex, i );
			CollectBindingObjects( json, tokens, valueIndex, path + "[" + std::to_string( i ) + "]", bindings );
		}
	}
}

void ParseContractJson( const std::string& json, const std::vector<jsmntok_t>& tokens, JozzVehicleAssetContract* contract,
						std::string* sourceRelativePath )
{
	int valueIndex = FindObjectValue( json, tokens, 0, "assetId" );
	if ( valueIndex >= 0 )
	{
		contract->assetId = TokenString( json, tokens[valueIndex] );
	}

	valueIndex = FindObjectValue( json, tokens, 0, "assetType" );
	if ( valueIndex >= 0 )
	{
		contract->assetType = TokenString( json, tokens[valueIndex] );
	}

	int sourceIndex = FindObjectValue( json, tokens, 0, "source" );
	if ( sourceIndex >= 0 )
	{
		valueIndex = FindObjectValue( json, tokens, sourceIndex, "gltf" );
		if ( valueIndex >= 0 )
		{
			*sourceRelativePath = TokenString( json, tokens[valueIndex] );
		}

		valueIndex = FindObjectValue( json, tokens, sourceIndex, "contractVersion" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &contract->contractVersion );
		}
	}

	int unitsIndex = FindObjectValue( json, tokens, 0, "units" );
	if ( unitsIndex >= 0 )
	{
		valueIndex = FindObjectValue( json, tokens, unitsIndex, "metersPerBlockbenchUnit" );
		if ( valueIndex >= 0 )
		{
			TokenFloat( json, tokens[valueIndex], &contract->metersPerBlockbenchUnit );
		}
	}

	int orientationIndex = FindObjectValue( json, tokens, 0, "orientation" );
	if ( orientationIndex >= 0 )
	{
		valueIndex = FindObjectValue( json, tokens, orientationIndex, "status" );
		if ( valueIndex >= 0 )
		{
			contract->orientationStatus = TokenString( json, tokens[valueIndex] );
		}

		int correctionIndex = FindObjectValue( json, tokens, orientationIndex, "temporaryImporterCorrection" );
		if ( correctionIndex >= 0 )
		{
			valueIndex = FindObjectValue( json, tokens, correctionIndex, "enabled" );
			if ( valueIndex >= 0 )
			{
				TokenBool( json, tokens[valueIndex], &contract->temporaryImporterCorrectionEnabled );
			}
		}
	}

	int semanticsIndex = FindObjectValue( json, tokens, 0, "semantics" );
	if ( semanticsIndex >= 0 )
	{
		CollectBindingObjects( json, tokens, semanticsIndex, "semantics", &contract->bindings );
	}
}

void ResolveBindingsFromGltf( const std::filesystem::path& gltfPath, JozzVehicleAssetContract* contract )
{
	std::string json;
	if ( ReadTextFile( gltfPath, &json ) == false )
	{
		contract->errors.push_back( "source glTF could not be read" );
		return;
	}

	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens[0].type != JSMN_OBJECT )
	{
		contract->errors.push_back( "source glTF JSON could not be parsed" );
		return;
	}

	std::vector<GltfContractNode> nodes;
	if ( ParseGltfNodes( json, tokens, &nodes ) == false || nodes.empty() )
	{
		contract->errors.push_back( "source glTF nodes could not be parsed" );
		return;
	}

	std::map<std::string, int> nameCounts;
	for ( const GltfContractNode& node : nodes )
	{
		if ( node.name.empty() == false )
		{
			nameCounts[node.name] += 1;
		}
	}

	for ( const auto& item : nameCounts )
	{
		if ( item.second > 1 )
		{
			contract->warnings.push_back( "duplicate node name in source glTF: " + item.first + " x" + std::to_string( item.second ) );
		}
	}

	std::vector<bool> isChild( nodes.size(), false );
	for ( const GltfContractNode& node : nodes )
	{
		for ( int child : node.children )
		{
			if ( child >= 0 && child < (int)isChild.size() )
			{
				isChild[child] = true;
			}
		}
	}

	std::vector<Matrix4> world( nodes.size(), IdentityMatrix() );
	for ( int i = 0; i < (int)nodes.size(); ++i )
	{
		if ( isChild[i] == false )
		{
			ComputeWorldNodeMatrices( nodes, i, IdentityMatrix(), &world );
		}
	}

	for ( JozzVehicleContractBinding& binding : contract->bindings )
	{
		if ( binding.nodeIndexHint < 0 || binding.nodeIndexHint >= (int)nodes.size() )
		{
			if ( binding.required )
			{
				contract->errors.push_back( binding.path + ": required nodeIndexHint is out of range" );
			}
			continue;
		}

		const GltfContractNode& node = nodes[binding.nodeIndexHint];
		if ( binding.nameHint.empty() == false && node.name != binding.nameHint )
		{
			std::string issue = binding.path + ": nodeIndexHint resolves to `" + node.name + "` instead of `" + binding.nameHint + "`";
			if ( binding.required )
			{
				contract->errors.push_back( issue );
			}
			else
			{
				contract->warnings.push_back( issue );
			}
			continue;
		}

		binding.positionBU = TransformPoint( world[binding.nodeIndexHint], b3Vec3_zero );
		binding.positionMeters = b3MulSV( contract->metersPerBlockbenchUnit, binding.positionBU );
		binding.resolved = true;
	}
}

void AddRequiredStringError( const char* label, const std::string& value, std::vector<std::string>* errors )
{
	if ( value.empty() )
	{
		errors->push_back( std::string( "missing " ) + label );
	}
}

} // namespace

JozzVehicleAssetContract LoadJozzVehicleAssetContract( const char* contractFileName )
{
	JozzVehicleAssetContract contract;

	std::filesystem::path contractPath = FindReadablePath( contractFileName, "assets/contracts" );
	if ( contractPath.empty() )
	{
		contract.status = std::string( "asset contract not found: " ) + contractFileName;
		contract.errors.push_back( contract.status );
		return contract;
	}

	contract.contractPath = PathString( contractPath );

	std::string json;
	if ( ReadTextFile( contractPath, &json ) == false )
	{
		contract.status = "asset contract could not be read";
		contract.errors.push_back( contract.status );
		return contract;
	}

	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens[0].type != JSMN_OBJECT )
	{
		contract.status = "asset contract JSON could not be parsed";
		contract.errors.push_back( contract.status );
		return contract;
	}

	std::string sourceRelativePath;
	ParseContractJson( json, tokens, &contract, &sourceRelativePath );
	if ( sourceRelativePath.empty() )
	{
		contract.errors.push_back( "contract source.gltf is missing" );
	}
	else
	{
		std::filesystem::path sourcePath = ( contractPath.parent_path() / sourceRelativePath ).lexically_normal();
		std::error_code ec;
		if ( std::filesystem::exists( sourcePath, ec ) )
		{
			contract.sourcePath = PathString( sourcePath );
			ResolveBindingsFromGltf( sourcePath, &contract );
		}
		else
		{
			contract.errors.push_back( "contract source.gltf does not exist: " + sourcePath.generic_string() );
		}
	}

	if ( contract.bindings.empty() )
	{
		contract.errors.push_back( "contract has no runtime bindings" );
	}

	if ( contract.errors.empty() )
	{
		contract.loaded = true;
		contract.status = "contract runtime: loaded sidecar + source glTF";
	}
	else
	{
		contract.status = "contract runtime: failed";
	}

	return contract;
}

const JozzVehicleContractBinding* FindJozzVehicleContractBindingByRole( const JozzVehicleAssetContract& contract, const char* role )
{
	for ( const JozzVehicleContractBinding& binding : contract.bindings )
	{
		if ( binding.role == role )
		{
			return &binding;
		}
	}

	return nullptr;
}

bool ValidateJozzVehicleSuspensionCornerContract( const JozzVehicleAssetContract& contract, std::vector<std::string>* errors )
{
	errors->clear();
	if ( contract.loaded == false )
	{
		errors->push_back( "suspension contract did not load" );
	}

	if ( contract.assetType != "suspension_corner_visual" )
	{
		errors->push_back( "suspension contract has unexpected assetType: " + contract.assetType );
	}

	if ( contract.contractVersion < 2 )
	{
		errors->push_back( "suspension contract must be v2 or newer" );
	}

	if ( contract.metersPerBlockbenchUnit <= 0.0f )
	{
		errors->push_back( "suspension contract scale must be positive" );
	}

	const char* requiredRoles[] = {
		"suspension.visual.chassis_mount",
		"suspension.visual.wheel_center",
		"suspension.visual.damper_upper_r",
		"suspension.visual.damper_upper_l",
		"suspension.visual.damper_lower_r",
		"suspension.visual.damper_lower_l",
		"suspension.visual.cardan_drive",
		"suspension.visual.cardan_hub",
		"suspension.travel_axis.top",
		"suspension.travel_axis.bottom",
		"suspension.visual.chassis_top",
		"suspension.visual.chassis_bottom",
	};

	for ( const char* role : requiredRoles )
	{
		const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( contract, role );
		if ( binding == nullptr )
		{
			errors->push_back( std::string( "missing required role: " ) + role );
			continue;
		}

		AddRequiredStringError( ( std::string( role ) + " roleCategory" ).c_str(), binding->roleCategory, errors );
		AddRequiredStringError( ( std::string( role ) + " nameHint" ).c_str(), binding->nameHint, errors );
		AddRequiredStringError( ( std::string( role ) + " nodePathHint" ).c_str(), binding->nodePathHint, errors );
		if ( binding->nodeIndexHint < 0 )
		{
			errors->push_back( std::string( "missing nodeIndexHint for role: " ) + role );
		}
		if ( binding->required == false )
		{
			errors->push_back( std::string( "role should be required for v0 rig: " ) + role );
		}
		if ( binding->physicsAuthority )
		{
			errors->push_back( std::string( "visual suspension role must not be physics authority: " ) + role );
		}
		if ( binding->resolved == false )
		{
			errors->push_back( std::string( "role did not resolve against source glTF: " ) + role );
		}
	}

	const JozzVehicleContractBinding* travelTop = FindJozzVehicleContractBindingByRole( contract, "suspension.travel_axis.top" );
	const JozzVehicleContractBinding* travelBottom = FindJozzVehicleContractBindingByRole( contract, "suspension.travel_axis.bottom" );
	if ( travelTop != nullptr && travelBottom != nullptr && travelTop->resolved && travelBottom->resolved )
	{
		float travelMeters = b3Distance( travelTop->positionMeters, travelBottom->positionMeters );
		if ( travelMeters < 0.60f || travelMeters > 0.80f )
		{
			errors->push_back( "suspension travel axis length is outside expected prototype range" );
		}
	}

	for ( const std::string& error : contract.errors )
	{
		errors->push_back( error );
	}

	return errors->empty();
}
