// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_visual_mesh.h"

#include "box3d/base.h"
#include "gfx/draw.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_image_decode.h"
#include "jozz_vehicle_json.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <vector>

using namespace jozz;

namespace
{

struct BufferView
{
	int buffer = -1;
	int byteOffset = 0;
	int byteLength = 0;
	int byteStride = 0;
};

struct Accessor
{
	int bufferView = -1;
	int byteOffset = 0;
	int componentType = 0;
	int count = 0;
	std::string type;
};

struct GltfNode
{
	int mesh = -1;
	std::vector<int> children;
	b3Vec3 translation = b3Vec3_zero;
	b3Quat rotation = b3Quat_identity;
	b3Vec3 scale = b3Vec3_one;
};

struct GltfSampler
{
	int magFilter = 9728; // NEAREST
	int minFilter = 9728; // NEAREST
	int wrapS = 33071;	 // CLAMP_TO_EDGE
	int wrapT = 33071;	 // CLAMP_TO_EDGE
};

struct GltfImage
{
	std::string uri;
	std::string mimeType;
};

struct GltfTexture
{
	int sampler = -1;
	int source = -1;
};

struct GltfMaterial
{
	int baseColorTexture = -1;
	bool alphaMask = false;
	float alphaCutoff = 0.5f;
};

struct GltfPrimitive
{
	int meshIndex = -1;
	int positionAccessor = -1;
	int normalAccessor = -1;
	int texcoordAccessor = -1;
	int indexAccessor = -1;
	int materialIndex = -1;
};

struct Matrix4
{
	float m[4][4];
};

int Base64Value( char c )
{
	if ( c >= 'A' && c <= 'Z' )
	{
		return c - 'A';
	}
	if ( c >= 'a' && c <= 'z' )
	{
		return c - 'a' + 26;
	}
	if ( c >= '0' && c <= '9' )
	{
		return c - '0' + 52;
	}
	if ( c == '+' )
	{
		return 62;
	}
	if ( c == '/' )
	{
		return 63;
	}
	return -1;
}

bool DecodeDataUri( const std::string& uri, std::vector<uint8_t>* bytes )
{
	size_t comma = uri.find( ',' );
	if ( comma == std::string::npos )
	{
		return false;
	}

	bytes->clear();
	int value = 0;
	int valueBits = -8;
	for ( size_t i = comma + 1; i < uri.size(); ++i )
	{
		char c = uri[i];
		if ( c == '=' )
		{
			break;
		}
		if ( c == '\r' || c == '\n' || c == '\t' || c == ' ' )
		{
			continue;
		}

		int decoded = Base64Value( c );
		if ( decoded < 0 )
		{
			return false;
		}

		value = ( value << 6 ) | decoded;
		valueBits += 6;
		if ( valueBits >= 0 )
		{
			bytes->push_back( (uint8_t)( ( value >> valueBits ) & 0xFF ) );
			valueBits -= 8;
		}
	}

	return bytes->empty() == false;
}

Matrix4 IdentityMatrix()
{
	Matrix4 result = {};
	for ( int i = 0; i < 4; ++i )
	{
		result.m[i][i] = 1.0f;
	}
	return result;
}

Matrix4 MulMatrix( const Matrix4& a, const Matrix4& b )
{
	Matrix4 result = {};
	for ( int r = 0; r < 4; ++r )
	{
		for ( int c = 0; c < 4; ++c )
		{
			for ( int k = 0; k < 4; ++k )
			{
				result.m[r][c] += a.m[r][k] * b.m[k][c];
			}
		}
	}
	return result;
}

b3Vec3 TransformPoint( const Matrix4& m, b3Vec3 p )
{
	return { m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
			 m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
			 m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3] };
}

b3Vec3 TransformVector( const Matrix4& m, b3Vec3 p )
{
	return { m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z,
			 m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z,
			 m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z };
}

Matrix4 NodeMatrix( const GltfNode& node )
{
	b3Quat q = node.rotation;
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
	float sx = node.scale.x;
	float sy = node.scale.y;
	float sz = node.scale.z;

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
	result.m[0][0] = r00 * sx;
	result.m[0][1] = r01 * sy;
	result.m[0][2] = r02 * sz;
	result.m[0][3] = node.translation.x;
	result.m[1][0] = r10 * sx;
	result.m[1][1] = r11 * sy;
	result.m[1][2] = r12 * sz;
	result.m[1][3] = node.translation.y;
	result.m[2][0] = r20 * sx;
	result.m[2][1] = r21 * sy;
	result.m[2][2] = r22 * sz;
	result.m[2][3] = node.translation.z;
	return result;
}

void ComputeWorldNodeMatrices( const std::vector<GltfNode>& nodes, int index, const Matrix4& parent, std::vector<Matrix4>* world )
{
	if ( index < 0 || index >= (int)nodes.size() )
	{
		return;
	}

	( *world )[index] = MulMatrix( parent, NodeMatrix( nodes[index] ) );
	for ( int child : nodes[index].children )
	{
		ComputeWorldNodeMatrices( nodes, child, ( *world )[index], world );
	}
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

bool ParseBufferViews( std::string_view json, const std::vector<jsmntok_t>& tokens, int bufferViewsIndex,
					   std::vector<BufferView>* out )
{
	if ( bufferViewsIndex < 0 || bufferViewsIndex >= (int)tokens.size() || tokens[bufferViewsIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	out->clear();
	for ( int i = 0; i < tokens[bufferViewsIndex].size; ++i )
	{
		int viewIndex = GetArrayElement( tokens, bufferViewsIndex, i );
		if ( viewIndex < 0 || tokens[viewIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		BufferView view;
		int valueIndex = FindObjectValue( json, tokens, viewIndex, "buffer" );
		TokenInt( json, tokens[valueIndex], &view.buffer );
		valueIndex = FindObjectValue( json, tokens, viewIndex, "byteOffset" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &view.byteOffset );
		}
		valueIndex = FindObjectValue( json, tokens, viewIndex, "byteLength" );
		TokenInt( json, tokens[valueIndex], &view.byteLength );
		valueIndex = FindObjectValue( json, tokens, viewIndex, "byteStride" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &view.byteStride );
		}
		out->push_back( view );
	}

	return out->empty() == false;
}

bool ParseAccessors( std::string_view json, const std::vector<jsmntok_t>& tokens, int accessorsIndex, std::vector<Accessor>* out )
{
	if ( accessorsIndex < 0 || accessorsIndex >= (int)tokens.size() || tokens[accessorsIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	out->clear();
	for ( int i = 0; i < tokens[accessorsIndex].size; ++i )
	{
		int accessorIndex = GetArrayElement( tokens, accessorsIndex, i );
		if ( accessorIndex < 0 || tokens[accessorIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		Accessor accessor;
		int valueIndex = FindObjectValue( json, tokens, accessorIndex, "bufferView" );
		TokenInt( json, tokens[valueIndex], &accessor.bufferView );
		valueIndex = FindObjectValue( json, tokens, accessorIndex, "byteOffset" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &accessor.byteOffset );
		}
		valueIndex = FindObjectValue( json, tokens, accessorIndex, "componentType" );
		TokenInt( json, tokens[valueIndex], &accessor.componentType );
		valueIndex = FindObjectValue( json, tokens, accessorIndex, "count" );
		TokenInt( json, tokens[valueIndex], &accessor.count );
		valueIndex = FindObjectValue( json, tokens, accessorIndex, "type" );
		if ( valueIndex >= 0 )
		{
			accessor.type = TokenString( json, tokens[valueIndex] );
		}

		out->push_back( accessor );
	}

	return out->empty() == false;
}

bool ParseNodes( std::string_view json, const std::vector<jsmntok_t>& tokens, int nodesIndex, std::vector<GltfNode>* out )
{
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

		GltfNode node;
		int valueIndex = FindObjectValue( json, tokens, nodeIndex, "mesh" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &node.mesh );
		}

		ParseVec3Member( json, tokens, nodeIndex, "translation", &node.translation );
		ParseQuatMember( json, tokens, nodeIndex, "rotation", &node.rotation );
		ParseVec3Member( json, tokens, nodeIndex, "scale", &node.scale );

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

bool ParseSamplers( std::string_view json, const std::vector<jsmntok_t>& tokens, int samplersIndex, std::vector<GltfSampler>* out )
{
	out->clear();
	if ( samplersIndex < 0 )
	{
		return true;
	}
	if ( samplersIndex >= (int)tokens.size() || tokens[samplersIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	for ( int i = 0; i < tokens[samplersIndex].size; ++i )
	{
		int samplerIndex = GetArrayElement( tokens, samplersIndex, i );
		if ( samplerIndex < 0 || tokens[samplerIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		GltfSampler sampler;
		int valueIndex = FindObjectValue( json, tokens, samplerIndex, "magFilter" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &sampler.magFilter );
		}
		valueIndex = FindObjectValue( json, tokens, samplerIndex, "minFilter" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &sampler.minFilter );
		}
		valueIndex = FindObjectValue( json, tokens, samplerIndex, "wrapS" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &sampler.wrapS );
		}
		valueIndex = FindObjectValue( json, tokens, samplerIndex, "wrapT" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &sampler.wrapT );
		}
		out->push_back( sampler );
	}

	return true;
}

bool ParseImages( std::string_view json, const std::vector<jsmntok_t>& tokens, int imagesIndex, std::vector<GltfImage>* out )
{
	out->clear();
	if ( imagesIndex < 0 )
	{
		return true;
	}
	if ( imagesIndex >= (int)tokens.size() || tokens[imagesIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	for ( int i = 0; i < tokens[imagesIndex].size; ++i )
	{
		int imageIndex = GetArrayElement( tokens, imagesIndex, i );
		if ( imageIndex < 0 || tokens[imageIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		GltfImage image;
		int valueIndex = FindObjectValue( json, tokens, imageIndex, "uri" );
		if ( valueIndex >= 0 )
		{
			image.uri = TokenString( json, tokens[valueIndex] );
		}
		valueIndex = FindObjectValue( json, tokens, imageIndex, "mimeType" );
		if ( valueIndex >= 0 )
		{
			image.mimeType = TokenString( json, tokens[valueIndex] );
		}
		out->push_back( std::move( image ) );
	}

	return true;
}

bool ParseTextures( std::string_view json, const std::vector<jsmntok_t>& tokens, int texturesIndex, std::vector<GltfTexture>* out )
{
	out->clear();
	if ( texturesIndex < 0 )
	{
		return true;
	}
	if ( texturesIndex >= (int)tokens.size() || tokens[texturesIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	for ( int i = 0; i < tokens[texturesIndex].size; ++i )
	{
		int textureIndex = GetArrayElement( tokens, texturesIndex, i );
		if ( textureIndex < 0 || tokens[textureIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		GltfTexture texture;
		int valueIndex = FindObjectValue( json, tokens, textureIndex, "sampler" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &texture.sampler );
		}
		valueIndex = FindObjectValue( json, tokens, textureIndex, "source" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &texture.source );
		}
		out->push_back( texture );
	}

	return true;
}

bool ParseMaterials( std::string_view json, const std::vector<jsmntok_t>& tokens, int materialsIndex, std::vector<GltfMaterial>* out )
{
	out->clear();
	if ( materialsIndex < 0 )
	{
		return true;
	}
	if ( materialsIndex >= (int)tokens.size() || tokens[materialsIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	for ( int i = 0; i < tokens[materialsIndex].size; ++i )
	{
		int materialIndex = GetArrayElement( tokens, materialsIndex, i );
		if ( materialIndex < 0 || tokens[materialIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		GltfMaterial material;
		int valueIndex = FindObjectValue( json, tokens, materialIndex, "alphaMode" );
		if ( valueIndex >= 0 && TokenString( json, tokens[valueIndex] ) == "MASK" )
		{
			material.alphaMask = true;
		}
		valueIndex = FindObjectValue( json, tokens, materialIndex, "alphaCutoff" );
		if ( valueIndex >= 0 )
		{
			TokenFloat( json, tokens[valueIndex], &material.alphaCutoff );
		}

		int pbrIndex = FindObjectValue( json, tokens, materialIndex, "pbrMetallicRoughness" );
		int baseColorTextureIndex = FindObjectValue( json, tokens, pbrIndex, "baseColorTexture" );
		valueIndex = FindObjectValue( json, tokens, baseColorTextureIndex, "index" );
		if ( valueIndex >= 0 )
		{
			TokenInt( json, tokens[valueIndex], &material.baseColorTexture );
		}
		out->push_back( material );
	}

	return true;
}

bool ReadBuffers( std::string_view json, const std::vector<jsmntok_t>& tokens, int buffersIndex, std::vector<std::vector<uint8_t>>* out )
{
	if ( buffersIndex < 0 || buffersIndex >= (int)tokens.size() || tokens[buffersIndex].type != JSMN_ARRAY )
	{
		return false;
	}

	out->clear();
	for ( int i = 0; i < tokens[buffersIndex].size; ++i )
	{
		int bufferIndex = GetArrayElement( tokens, buffersIndex, i );
		if ( bufferIndex < 0 || tokens[bufferIndex].type != JSMN_OBJECT )
		{
			return false;
		}

		int uriIndex = FindObjectValue( json, tokens, bufferIndex, "uri" );
		if ( uriIndex < 0 )
		{
			return false;
		}

		std::vector<uint8_t> bytes;
		if ( DecodeDataUri( TokenString( json, tokens[uriIndex] ), &bytes ) == false )
		{
			return false;
		}

		out->push_back( std::move( bytes ) );
	}

	return out->empty() == false;
}

bool GetFirstPrimitive( std::string_view json, const std::vector<jsmntok_t>& tokens, int meshesIndex, GltfPrimitive* out )
{
	if ( meshesIndex < 0 || meshesIndex >= (int)tokens.size() || tokens[meshesIndex].type != JSMN_ARRAY || tokens[meshesIndex].size == 0 )
	{
		return false;
	}

	int meshIndex = GetArrayElement( tokens, meshesIndex, 0 );
	int primitivesIndex = FindObjectValue( json, tokens, meshIndex, "primitives" );
	int primitiveIndex = GetArrayElement( tokens, primitivesIndex, 0 );
	if ( primitiveIndex < 0 )
	{
		return false;
	}

	GltfPrimitive primitive;
	primitive.meshIndex = 0;

	int attributesIndex = FindObjectValue( json, tokens, primitiveIndex, "attributes" );
	if ( attributesIndex < 0 )
	{
		return false;
	}

	int positionIndex = FindObjectValue( json, tokens, attributesIndex, "POSITION" );
	int normalIndex = FindObjectValue( json, tokens, attributesIndex, "NORMAL" );
	int texcoordIndex = FindObjectValue( json, tokens, attributesIndex, "TEXCOORD_0" );
	int indicesIndex = FindObjectValue( json, tokens, primitiveIndex, "indices" );
	if ( positionIndex < 0 || indicesIndex < 0 )
	{
		return false;
	}

	if ( TokenInt( json, tokens[positionIndex], &primitive.positionAccessor ) == false ||
		 TokenInt( json, tokens[indicesIndex], &primitive.indexAccessor ) == false )
	{
		return false;
	}
	if ( normalIndex >= 0 && TokenInt( json, tokens[normalIndex], &primitive.normalAccessor ) == false )
	{
		return false;
	}
	if ( texcoordIndex >= 0 && TokenInt( json, tokens[texcoordIndex], &primitive.texcoordAccessor ) == false )
	{
		return false;
	}

	int materialIndex = FindObjectValue( json, tokens, primitiveIndex, "material" );
	if ( materialIndex >= 0 )
	{
		TokenInt( json, tokens[materialIndex], &primitive.materialIndex );
	}

	*out = primitive;
	return true;
}

int ComponentSize( int componentType )
{
	switch ( componentType )
	{
		case 5121:
			return 1;
		case 5123:
			return 2;
		case 5125:
		case 5126:
			return 4;
		default:
			return 0;
	}
}

bool ReadVec3Accessor( const std::vector<std::vector<uint8_t>>& buffers, const std::vector<BufferView>& views,
					   const std::vector<Accessor>& accessors, int accessorIndex, std::vector<b3Vec3>* out )
{
	if ( accessorIndex < 0 || accessorIndex >= (int)accessors.size() )
	{
		return false;
	}

	const Accessor& accessor = accessors[accessorIndex];
	if ( accessor.componentType != 5126 || accessor.type != "VEC3" || accessor.bufferView < 0 ||
		 accessor.bufferView >= (int)views.size() )
	{
		return false;
	}

	const BufferView& view = views[accessor.bufferView];
	if ( view.buffer < 0 || view.buffer >= (int)buffers.size() )
	{
		return false;
	}

	const std::vector<uint8_t>& buffer = buffers[view.buffer];
	int stride = view.byteStride > 0 ? view.byteStride : 3 * (int)sizeof( float );
	size_t base = (size_t)view.byteOffset + (size_t)accessor.byteOffset;
	if ( base + (size_t)( accessor.count - 1 ) * (size_t)stride + 3 * sizeof( float ) > buffer.size() )
	{
		return false;
	}

	out->clear();
	out->reserve( (size_t)accessor.count );
	for ( int i = 0; i < accessor.count; ++i )
	{
		size_t offset = base + (size_t)i * (size_t)stride;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		std::memcpy( &x, &buffer[offset + 0], sizeof( float ) );
		std::memcpy( &y, &buffer[offset + 4], sizeof( float ) );
		std::memcpy( &z, &buffer[offset + 8], sizeof( float ) );
		out->push_back( { x, y, z } );
	}

	return out->empty() == false;
}

bool ReadVec2Accessor( const std::vector<std::vector<uint8_t>>& buffers, const std::vector<BufferView>& views,
					   const std::vector<Accessor>& accessors, int accessorIndex, std::vector<b3Vec2>* out )
{
	if ( accessorIndex < 0 || accessorIndex >= (int)accessors.size() )
	{
		return false;
	}

	const Accessor& accessor = accessors[accessorIndex];
	if ( accessor.componentType != 5126 || accessor.type != "VEC2" || accessor.bufferView < 0 ||
		 accessor.bufferView >= (int)views.size() )
	{
		return false;
	}

	const BufferView& view = views[accessor.bufferView];
	if ( view.buffer < 0 || view.buffer >= (int)buffers.size() )
	{
		return false;
	}

	const std::vector<uint8_t>& buffer = buffers[view.buffer];
	int stride = view.byteStride > 0 ? view.byteStride : 2 * (int)sizeof( float );
	size_t base = (size_t)view.byteOffset + (size_t)accessor.byteOffset;
	if ( base + (size_t)( accessor.count - 1 ) * (size_t)stride + 2 * sizeof( float ) > buffer.size() )
	{
		return false;
	}

	out->clear();
	out->reserve( (size_t)accessor.count );
	for ( int i = 0; i < accessor.count; ++i )
	{
		size_t offset = base + (size_t)i * (size_t)stride;
		float x = 0.0f;
		float y = 0.0f;
		std::memcpy( &x, &buffer[offset + 0], sizeof( float ) );
		std::memcpy( &y, &buffer[offset + 4], sizeof( float ) );
		out->push_back( { x, y } );
	}

	return out->empty() == false;
}

bool ReadIndexAccessor( const std::vector<std::vector<uint8_t>>& buffers, const std::vector<BufferView>& views,
						const std::vector<Accessor>& accessors, int accessorIndex, std::vector<uint32_t>* out )
{
	if ( accessorIndex < 0 || accessorIndex >= (int)accessors.size() )
	{
		return false;
	}

	const Accessor& accessor = accessors[accessorIndex];
	if ( accessor.type != "SCALAR" || accessor.bufferView < 0 || accessor.bufferView >= (int)views.size() )
	{
		return false;
	}

	int componentSize = ComponentSize( accessor.componentType );
	if ( componentSize == 0 || accessor.componentType == 5126 )
	{
		return false;
	}

	const BufferView& view = views[accessor.bufferView];
	if ( view.buffer < 0 || view.buffer >= (int)buffers.size() )
	{
		return false;
	}

	const std::vector<uint8_t>& buffer = buffers[view.buffer];
	int stride = view.byteStride > 0 ? view.byteStride : componentSize;
	size_t base = (size_t)view.byteOffset + (size_t)accessor.byteOffset;
	if ( base + (size_t)( accessor.count - 1 ) * (size_t)stride + (size_t)componentSize > buffer.size() )
	{
		return false;
	}

	out->clear();
	out->reserve( (size_t)accessor.count );
	for ( int i = 0; i < accessor.count; ++i )
	{
		size_t offset = base + (size_t)i * (size_t)stride;
		uint32_t value = 0;
		if ( accessor.componentType == 5121 )
		{
			value = buffer[offset];
		}
		else if ( accessor.componentType == 5123 )
		{
			uint16_t v = 0;
			std::memcpy( &v, &buffer[offset], sizeof( uint16_t ) );
			value = v;
		}
		else if ( accessor.componentType == 5125 )
		{
			std::memcpy( &value, &buffer[offset], sizeof( uint32_t ) );
		}
		out->push_back( value );
	}

	return out->size() >= 3;
}

void FillMissingNormals( const std::vector<b3Vec3>& positions, const std::vector<uint32_t>& indices, std::vector<b3Vec3>* normals )
{
	normals->assign( positions.size(), b3Vec3_zero );

	for ( size_t i = 0; i + 2 < indices.size(); i += 3 )
	{
		uint32_t ia = indices[i + 0];
		uint32_t ib = indices[i + 1];
		uint32_t ic = indices[i + 2];
		if ( ia >= positions.size() || ib >= positions.size() || ic >= positions.size() )
		{
			continue;
		}

		b3Vec3 ab = b3Sub( positions[ib], positions[ia] );
		b3Vec3 ac = b3Sub( positions[ic], positions[ia] );
		b3Vec3 normal = b3Cross( ab, ac );
		if ( b3Length( normal ) > 1.0e-6f )
		{
			normal = b3Normalize( normal );
			( *normals )[ia] = b3Add( ( *normals )[ia], normal );
			( *normals )[ib] = b3Add( ( *normals )[ib], normal );
			( *normals )[ic] = b3Add( ( *normals )[ic], normal );
		}
	}

	for ( b3Vec3& normal : *normals )
	{
		if ( b3Length( normal ) > 1.0e-6f )
		{
			normal = b3Normalize( normal );
		}
		else
		{
			normal = b3Vec3_axisY;
		}
	}
}

uint32_t BuildMeshHash( const char* path, const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices,
						const JozzVehicleDecodedImage* texture )
{
	uint32_t hash = B3_HASH_INIT;
	hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( path ), (int)std::strlen( path ) );
	hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( vertices.data() ), (int)( vertices.size() * sizeof( MeshVertex ) ) );
	hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( indices.data() ), (int)( indices.size() * sizeof( uint32_t ) ) );
	if ( texture != nullptr && texture->rgba8.empty() == false )
	{
		hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( &texture->width ), (int)sizeof( texture->width ) );
		hash = b3Hash( hash, reinterpret_cast<const uint8_t*>( &texture->height ), (int)sizeof( texture->height ) );
		hash = b3Hash( hash, texture->rgba8.data(), (int)texture->rgba8.size() );
	}
	return hash != 0u ? hash : 1u;
}

bool IsPngDataUri( const std::string& uri )
{
	return uri.rfind( "data:image/png;base64,", 0 ) == 0;
}

bool IsNearestClampSampler( const GltfSampler& sampler )
{
	return sampler.magFilter == 9728 && sampler.minFilter == 9728 && sampler.wrapS == 33071 && sampler.wrapT == 33071;
}

std::string LoadedTextureStatus( int width, int height, float alphaCutoff )
{
	char buffer[128];
	if ( alphaCutoff > 0.0f )
	{
		std::snprintf( buffer, sizeof( buffer ), "texture: loaded baseColor %dx%d, alpha mask %.2f", width, height, alphaCutoff );
	}
	else
	{
		std::snprintf( buffer, sizeof( buffer ), "texture: loaded baseColor %dx%d", width, height );
	}
	return buffer;
}

// ---- Rigged (per-bone) loader helpers ----

bool ParseNodeNames( std::string_view json, const std::vector<jsmntok_t>& tokens, int nodesIndex, std::vector<std::string>* out )
{
	out->clear();
	if ( nodesIndex < 0 || nodesIndex >= (int)tokens.size() || tokens[nodesIndex].type != JSMN_ARRAY )
	{
		return false;
	}
	for ( int i = 0; i < tokens[nodesIndex].size; ++i )
	{
		int nodeIndex = GetArrayElement( tokens, nodesIndex, i );
		int nameIndex = FindObjectValue( json, tokens, nodeIndex, "name" );
		out->push_back( nameIndex >= 0 ? TokenString( json, tokens[nameIndex] ) : std::string() );
	}
	return true;
}

// Joints of the first skin (glTF skin.joints is the jointIndex -> node map).
bool ParseFirstSkinJoints( std::string_view json, const std::vector<jsmntok_t>& tokens, int skinsIndex,
						   std::vector<int>* out )
{
	out->clear();
	if ( skinsIndex < 0 || skinsIndex >= (int)tokens.size() || tokens[skinsIndex].type != JSMN_ARRAY || tokens[skinsIndex].size == 0 )
	{
		return false;
	}
	int skin0 = GetArrayElement( tokens, skinsIndex, 0 );
	int jointsIndex = FindObjectValue( json, tokens, skin0, "joints" );
	if ( jointsIndex < 0 || tokens[jointsIndex].type != JSMN_ARRAY )
	{
		return false;
	}
	for ( int i = 0; i < tokens[jointsIndex].size; ++i )
	{
		int elem = GetArrayElement( tokens, jointsIndex, i );
		int value = -1;
		if ( elem >= 0 && TokenInt( json, tokens[elem], &value ) )
		{
			out->push_back( value );
		}
	}
	return out->empty() == false;
}

// Accessor index of a named attribute on mesh[0].primitives[0], or -1.
int FindPrimitiveAttribute( std::string_view json, const std::vector<jsmntok_t>& tokens, int meshesIndex, const char* attr )
{
	if ( meshesIndex < 0 || tokens[meshesIndex].type != JSMN_ARRAY || tokens[meshesIndex].size == 0 )
	{
		return -1;
	}
	int meshIndex = GetArrayElement( tokens, meshesIndex, 0 );
	int primitivesIndex = FindObjectValue( json, tokens, meshIndex, "primitives" );
	int primitiveIndex = GetArrayElement( tokens, primitivesIndex, 0 );
	int attributesIndex = FindObjectValue( json, tokens, primitiveIndex, "attributes" );
	int attrIndex = FindObjectValue( json, tokens, attributesIndex, attr );
	int accessor = -1;
	if ( attrIndex >= 0 )
	{
		TokenInt( json, tokens[attrIndex], &accessor );
	}
	return accessor;
}

// VEC4 unsigned joints (componentType u8/u16), flat 4-per-vertex.
bool ReadVec4UintAccessor( const std::vector<std::vector<uint8_t>>& buffers, const std::vector<BufferView>& views,
						   const std::vector<Accessor>& accessors, int accessorIndex, std::vector<uint32_t>* out )
{
	if ( accessorIndex < 0 || accessorIndex >= (int)accessors.size() )
	{
		return false;
	}
	const Accessor& a = accessors[accessorIndex];
	int componentSize = ComponentSize( a.componentType );
	if ( a.type != "VEC4" || componentSize == 0 || a.componentType == 5126 || a.bufferView < 0 ||
		 a.bufferView >= (int)views.size() )
	{
		return false;
	}
	const BufferView& v = views[a.bufferView];
	if ( v.buffer < 0 || v.buffer >= (int)buffers.size() )
	{
		return false;
	}
	const std::vector<uint8_t>& buffer = buffers[v.buffer];
	int stride = v.byteStride > 0 ? v.byteStride : 4 * componentSize;
	size_t base = (size_t)v.byteOffset + (size_t)a.byteOffset;
	if ( base + (size_t)( a.count - 1 ) * (size_t)stride + (size_t)( 4 * componentSize ) > buffer.size() )
	{
		return false;
	}
	out->clear();
	out->reserve( (size_t)a.count * 4 );
	for ( int i = 0; i < a.count; ++i )
	{
		size_t off = base + (size_t)i * (size_t)stride;
		for ( int c = 0; c < 4; ++c )
		{
			uint32_t value = 0;
			if ( a.componentType == 5121 )
			{
				value = buffer[off + c];
			}
			else if ( a.componentType == 5123 )
			{
				uint16_t v16 = 0;
				std::memcpy( &v16, &buffer[off + (size_t)c * 2], sizeof( uint16_t ) );
				value = v16;
			}
			else if ( a.componentType == 5125 )
			{
				std::memcpy( &value, &buffer[off + (size_t)c * 4], sizeof( uint32_t ) );
			}
			out->push_back( value );
		}
	}
	return out->empty() == false;
}

// VEC4 float weights, flat 4-per-vertex.
bool ReadVec4FloatAccessor( const std::vector<std::vector<uint8_t>>& buffers, const std::vector<BufferView>& views,
							const std::vector<Accessor>& accessors, int accessorIndex, std::vector<float>* out )
{
	if ( accessorIndex < 0 || accessorIndex >= (int)accessors.size() )
	{
		return false;
	}
	const Accessor& a = accessors[accessorIndex];
	if ( a.type != "VEC4" || a.componentType != 5126 || a.bufferView < 0 || a.bufferView >= (int)views.size() )
	{
		return false;
	}
	const BufferView& v = views[a.bufferView];
	if ( v.buffer < 0 || v.buffer >= (int)buffers.size() )
	{
		return false;
	}
	const std::vector<uint8_t>& buffer = buffers[v.buffer];
	int stride = v.byteStride > 0 ? v.byteStride : 4 * (int)sizeof( float );
	size_t base = (size_t)v.byteOffset + (size_t)a.byteOffset;
	if ( base + (size_t)( a.count - 1 ) * (size_t)stride + 4 * sizeof( float ) > buffer.size() )
	{
		return false;
	}
	out->clear();
	out->reserve( (size_t)a.count * 4 );
	for ( int i = 0; i < a.count; ++i )
	{
		size_t off = base + (size_t)i * (size_t)stride;
		float vals[4] = {};
		std::memcpy( vals, &buffer[off], 4 * sizeof( float ) );
		for ( float f : vals )
		{
			out->push_back( f );
		}
	}
	return out->empty() == false;
}

} // namespace

bool JozzVehicleVisualMesh::LoadStaticGltf( const char* path, float metersPerBlockbenchUnit )
{
	Destroy();
	status = "not loaded";
	textureStatus = "texture: not loaded";

	std::string json;
	if ( ReadTextFile( path, &json ) == false )
	{
		status = "visual mesh: source glTF not found";
		return false;
	}

	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens[0].type != JSMN_OBJECT )
	{
		status = "visual mesh: glTF JSON parse failed";
		return false;
	}

	int buffersIndex = FindObjectValue( json, tokens, 0, "buffers" );
	int bufferViewsIndex = FindObjectValue( json, tokens, 0, "bufferViews" );
	int accessorsIndex = FindObjectValue( json, tokens, 0, "accessors" );
	int meshesIndex = FindObjectValue( json, tokens, 0, "meshes" );
	int nodesIndex = FindObjectValue( json, tokens, 0, "nodes" );
	int materialsIndex = FindObjectValue( json, tokens, 0, "materials" );
	int texturesIndex = FindObjectValue( json, tokens, 0, "textures" );
	int imagesIndex = FindObjectValue( json, tokens, 0, "images" );
	int samplersIndex = FindObjectValue( json, tokens, 0, "samplers" );

	std::vector<std::vector<uint8_t>> buffers;
	std::vector<BufferView> views;
	std::vector<Accessor> accessors;
	std::vector<GltfNode> nodes;
	std::vector<GltfMaterial> materials;
	std::vector<GltfTexture> textures;
	std::vector<GltfImage> images;
	std::vector<GltfSampler> samplers;
	if ( ReadBuffers( json, tokens, buffersIndex, &buffers ) == false || ParseBufferViews( json, tokens, bufferViewsIndex, &views ) == false ||
		 ParseAccessors( json, tokens, accessorsIndex, &accessors ) == false || ParseNodes( json, tokens, nodesIndex, &nodes ) == false ||
		 ParseMaterials( json, tokens, materialsIndex, &materials ) == false ||
		 ParseTextures( json, tokens, texturesIndex, &textures ) == false || ParseImages( json, tokens, imagesIndex, &images ) == false ||
		 ParseSamplers( json, tokens, samplersIndex, &samplers ) == false )
	{
		status = "visual mesh: unsupported glTF buffer/accessor/node/material layout";
		return false;
	}

	GltfPrimitive primitive;
	if ( GetFirstPrimitive( json, tokens, meshesIndex, &primitive ) == false )
	{
		status = "visual mesh: no supported first primitive";
		return false;
	}

	std::vector<b3Vec3> positionsBU;
	std::vector<b3Vec3> normalsBU;
	std::vector<b3Vec2> texcoords;
	std::vector<uint32_t> indices;
	if ( ReadVec3Accessor( buffers, views, accessors, primitive.positionAccessor, &positionsBU ) == false ||
		 ReadIndexAccessor( buffers, views, accessors, primitive.indexAccessor, &indices ) == false )
	{
		status = "visual mesh: failed to read positions or indices";
		return false;
	}

	bool hasNormals = primitive.normalAccessor >= 0 && ReadVec3Accessor( buffers, views, accessors, primitive.normalAccessor, &normalsBU );
	if ( hasNormals && normalsBU.size() != positionsBU.size() )
	{
		normalsBU.clear();
		hasNormals = false;
	}

	bool hasTexcoords = primitive.texcoordAccessor >= 0 && ReadVec2Accessor( buffers, views, accessors, primitive.texcoordAccessor, &texcoords );
	if ( hasTexcoords && texcoords.size() != positionsBU.size() )
	{
		texcoords.clear();
		hasTexcoords = false;
	}

	JozzVehicleDecodedImage decodedTexture;
	bool useTexture = false;
	float alphaCutoff = 0.0f;
	textureStatus = "texture: no baseColorTexture on first primitive";
	if ( primitive.materialIndex >= 0 && primitive.materialIndex < (int)materials.size() )
	{
		const GltfMaterial& material = materials[primitive.materialIndex];
		if ( material.baseColorTexture >= 0 )
		{
			if ( hasTexcoords == false )
			{
				textureStatus = "texture: baseColorTexture present, but TEXCOORD_0 is missing";
			}
			else if ( material.baseColorTexture >= (int)textures.size() )
			{
				textureStatus = "texture: baseColorTexture index is out of range";
			}
			else
			{
				const GltfTexture& texture = textures[material.baseColorTexture];
				const GltfSampler sampler =
					( texture.sampler >= 0 && texture.sampler < (int)samplers.size() ) ? samplers[texture.sampler] : GltfSampler{};
				if ( IsNearestClampSampler( sampler ) == false )
				{
					textureStatus = "texture: unsupported sampler, expected nearest + clamp";
				}
				else if ( texture.source < 0 || texture.source >= (int)images.size() )
				{
					textureStatus = "texture: image source index is out of range";
				}
				else if ( IsPngDataUri( images[texture.source].uri ) == false )
				{
					textureStatus = "texture: only data:image/png;base64 URI is supported";
				}
				else
				{
					std::vector<uint8_t> pngBytes;
					if ( DecodeDataUri( images[texture.source].uri, &pngBytes ) == false )
					{
						textureStatus = "texture: failed to decode base64 PNG data URI";
					}
					else if ( DecodeJozzVehiclePngRgba8( pngBytes.data(), pngBytes.size(), &decodedTexture ) == false )
					{
						textureStatus = decodedTexture.status;
					}
					else
					{
						useTexture = true;
						alphaCutoff = material.alphaMask ? material.alphaCutoff : 0.0f;
						textureStatus = LoadedTextureStatus( decodedTexture.width, decodedTexture.height, alphaCutoff );
					}
				}
			}
		}
	}

	std::vector<bool> isChild( nodes.size(), false );
	for ( const GltfNode& node : nodes )
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

	Matrix4 meshTransform = IdentityMatrix();
	for ( int i = 0; i < (int)nodes.size(); ++i )
	{
		if ( nodes[i].mesh == primitive.meshIndex )
		{
			meshTransform = world[i];
			break;
		}
	}

	std::vector<b3Vec3> positions;
	positions.reserve( positionsBU.size() );
	boundsMin = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	boundsMax = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };

	for ( b3Vec3 positionBU : positionsBU )
	{
		b3Vec3 position = b3MulSV( metersPerBlockbenchUnit, TransformPoint( meshTransform, positionBU ) );
		positions.push_back( position );
		boundsMin = b3Min( boundsMin, position );
		boundsMax = b3Max( boundsMax, position );
	}

	std::vector<b3Vec3> normals;
	if ( hasNormals )
	{
		normals.reserve( normalsBU.size() );
		for ( b3Vec3 normalBU : normalsBU )
		{
			b3Vec3 normal = TransformVector( meshTransform, normalBU );
			normals.push_back( b3Length( normal ) > 1.0e-6f ? b3Normalize( normal ) : b3Vec3_axisY );
		}
	}
	else
	{
		FillMissingNormals( positions, indices, &normals );
	}

	std::vector<MeshVertex> vertices;
	vertices.reserve( positions.size() );
	for ( size_t i = 0; i < positions.size(); ++i )
	{
		MeshVertex vertex = {};
		vertex.position[0] = positions[i].x;
		vertex.position[1] = positions[i].y;
		vertex.position[2] = positions[i].z;
		vertex.normal[0] = normals[i].x;
		vertex.normal[1] = normals[i].y;
		vertex.normal[2] = normals[i].z;
		if ( hasTexcoords )
		{
			vertex.texcoord[0] = texcoords[i].x;
			vertex.texcoord[1] = texcoords[i].y;
		}
		else
		{
			vertex.texcoord[0] = 0.0f;
			vertex.texcoord[1] = 0.0f;
		}
		vertices.push_back( vertex );
	}

	uint32_t hash = BuildMeshHash( path, vertices, indices, useTexture ? &decodedTexture : nullptr );
	handle = FindMesh( hash );
	if ( IsMeshHandleValid( handle ) )
	{
		AddMeshReference( handle );
	}
	else
	{
		if ( useTexture )
		{
			MeshTextureData textureData = {};
			textureData.width = decodedTexture.width;
			textureData.height = decodedTexture.height;
			textureData.rgba8 = decodedTexture.rgba8.data();
			textureData.byteCount = (int)decodedTexture.rgba8.size();
			handle = RegisterTexturedMesh( hash, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size(),
										   &textureData, "jozz_vehicle_gltf_visual_mesh" );
			if ( IsMeshHandleValid( handle ) == false )
			{
				useTexture = false;
				textureStatus = "texture: GPU upload rejected, using solid-color fallback";
				hash = BuildMeshHash( path, vertices, indices, nullptr );
				handle = FindMesh( hash );
				if ( IsMeshHandleValid( handle ) )
				{
					AddMeshReference( handle );
				}
			}
		}
		if ( IsMeshHandleValid( handle ) == false )
		{
			handle = RegisterMesh( hash, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size(),
								   "jozz_vehicle_gltf_visual_mesh" );
		}
	}

	if ( IsMeshHandleValid( handle ) == false )
	{
		status = "visual mesh: renderer registry rejected mesh";
		return false;
	}

	vertexCount = (int)vertices.size();
	triangleCount = (int)indices.size() / 3;
	textureLoaded = useTexture;
	textureWidth = useTexture ? decodedTexture.width : 0;
	textureHeight = useTexture ? decodedTexture.height : 0;
	textureAlphaCutoff = useTexture ? alphaCutoff : 0.0f;
	status = "visual mesh: loaded static glTF first primitive";
	return true;
}

void JozzVehicleVisualMesh::Destroy()
{
	if ( IsMeshHandleValid( handle ) )
	{
		ReleaseMeshReference( handle );
		handle = InvalidMeshHandle();
	}

	vertexCount = 0;
	triangleCount = 0;
	textureLoaded = false;
	textureWidth = 0;
	textureHeight = 0;
	textureAlphaCutoff = 0.0f;
	textureStatus = "texture: not loaded";
	boundsMin = b3Vec3_zero;
	boundsMax = b3Vec3_zero;
}

bool JozzVehicleVisualMesh::IsLoaded() const
{
	return IsMeshHandleValid( handle );
}

void JozzVehicleVisualMesh::Draw( b3Pos origin, Vec4 color ) const
{
	DrawAtTransform( { origin, b3Quat_identity }, color );
}

void JozzVehicleVisualMesh::DrawAtTransform( b3WorldTransform worldTransform, Vec4 color ) const
{
	if ( IsLoaded() == false )
	{
		return;
	}

	b3Transform relativeTransform = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	AppendMesh( handle, relativeTransform, b3Vec3_one, color, 0.0f, 0.58f,
				textureLoaded ? MESH_MATERIAL_MODE_TEXTURED : MESH_MATERIAL_MODE_SOLID, textureAlphaCutoff,
				TRANSPARENT_SHADOW_FULL );
}

bool JozzVehicleRiggedMesh::LoadSkinnedGltf( const char* path, float metersPerBlockbenchUnit, bool mirrorX )
{
	Destroy();
	status = "rigged mesh: not loaded";

	std::string json;
	if ( ReadTextFile( path, &json ) == false )
	{
		status = "rigged mesh: source glTF not found";
		return false;
	}

	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens[0].type != JSMN_OBJECT )
	{
		status = "rigged mesh: glTF JSON parse failed";
		return false;
	}

	int buffersIndex = FindObjectValue( json, tokens, 0, "buffers" );
	int bufferViewsIndex = FindObjectValue( json, tokens, 0, "bufferViews" );
	int accessorsIndex = FindObjectValue( json, tokens, 0, "accessors" );
	int meshesIndex = FindObjectValue( json, tokens, 0, "meshes" );
	int nodesIndex = FindObjectValue( json, tokens, 0, "nodes" );
	int materialsIndex = FindObjectValue( json, tokens, 0, "materials" );
	int texturesIndex = FindObjectValue( json, tokens, 0, "textures" );
	int imagesIndex = FindObjectValue( json, tokens, 0, "images" );
	int samplersIndex = FindObjectValue( json, tokens, 0, "samplers" );
	int skinsIndex = FindObjectValue( json, tokens, 0, "skins" );

	std::vector<std::vector<uint8_t>> buffers;
	std::vector<BufferView> views;
	std::vector<Accessor> accessors;
	std::vector<GltfNode> nodes;
	std::vector<GltfMaterial> materials;
	std::vector<GltfTexture> textures;
	std::vector<GltfImage> images;
	std::vector<GltfSampler> samplers;
	std::vector<std::string> nodeNames;
	std::vector<int> skinJoints;
	if ( ReadBuffers( json, tokens, buffersIndex, &buffers ) == false ||
		 ParseBufferViews( json, tokens, bufferViewsIndex, &views ) == false ||
		 ParseAccessors( json, tokens, accessorsIndex, &accessors ) == false ||
		 ParseNodes( json, tokens, nodesIndex, &nodes ) == false ||
		 ParseMaterials( json, tokens, materialsIndex, &materials ) == false ||
		 ParseTextures( json, tokens, texturesIndex, &textures ) == false ||
		 ParseImages( json, tokens, imagesIndex, &images ) == false ||
		 ParseSamplers( json, tokens, samplersIndex, &samplers ) == false ||
		 ParseNodeNames( json, tokens, nodesIndex, &nodeNames ) == false )
	{
		status = "rigged mesh: unsupported glTF layout";
		return false;
	}
	if ( ParseFirstSkinJoints( json, tokens, skinsIndex, &skinJoints ) == false )
	{
		status = "rigged mesh: model is not skinned (no skin.joints)";
		return false;
	}

	GltfPrimitive primitive;
	if ( GetFirstPrimitive( json, tokens, meshesIndex, &primitive ) == false )
	{
		status = "rigged mesh: no supported first primitive";
		return false;
	}
	int jointsAccessor = FindPrimitiveAttribute( json, tokens, meshesIndex, "JOINTS_0" );
	int weightsAccessor = FindPrimitiveAttribute( json, tokens, meshesIndex, "WEIGHTS_0" );

	std::vector<b3Vec3> positionsBU;
	std::vector<b3Vec3> normalsBU;
	std::vector<b3Vec2> texcoords;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> jointIndices;
	std::vector<float> weights;
	if ( ReadVec3Accessor( buffers, views, accessors, primitive.positionAccessor, &positionsBU ) == false ||
		 ReadIndexAccessor( buffers, views, accessors, primitive.indexAccessor, &indices ) == false ||
		 ReadVec4UintAccessor( buffers, views, accessors, jointsAccessor, &jointIndices ) == false ||
		 ReadVec4FloatAccessor( buffers, views, accessors, weightsAccessor, &weights ) == false )
	{
		status = "rigged mesh: failed to read positions/indices/joints/weights";
		return false;
	}

	bool hasNormals = primitive.normalAccessor >= 0 && ReadVec3Accessor( buffers, views, accessors, primitive.normalAccessor, &normalsBU );
	if ( hasNormals && normalsBU.size() != positionsBU.size() )
	{
		normalsBU.clear();
		hasNormals = false;
	}
	bool hasTexcoords = primitive.texcoordAccessor >= 0 && ReadVec2Accessor( buffers, views, accessors, primitive.texcoordAccessor, &texcoords );
	if ( hasTexcoords && texcoords.size() != positionsBU.size() )
	{
		texcoords.clear();
		hasTexcoords = false;
	}

	// Texture: identical decode path to LoadStaticGltf; all parts share it.
	JozzVehicleDecodedImage decodedTexture;
	bool useTexture = false;
	float alphaCutoff = 0.0f;
	if ( primitive.materialIndex >= 0 && primitive.materialIndex < (int)materials.size() )
	{
		const GltfMaterial& material = materials[primitive.materialIndex];
		if ( material.baseColorTexture >= 0 && hasTexcoords && material.baseColorTexture < (int)textures.size() )
		{
			const GltfTexture& texture = textures[material.baseColorTexture];
			const GltfSampler sampler =
				( texture.sampler >= 0 && texture.sampler < (int)samplers.size() ) ? samplers[texture.sampler] : GltfSampler{};
			if ( IsNearestClampSampler( sampler ) && texture.source >= 0 && texture.source < (int)images.size() &&
				 IsPngDataUri( images[texture.source].uri ) )
			{
				std::vector<uint8_t> pngBytes;
				if ( DecodeDataUri( images[texture.source].uri, &pngBytes ) &&
					 DecodeJozzVehiclePngRgba8( pngBytes.data(), pngBytes.size(), &decodedTexture ) )
				{
					useTexture = true;
					alphaCutoff = material.alphaMask ? material.alphaCutoff : 0.0f;
				}
			}
		}
	}

	// Bake to authored-world-meters (same space as JozzVehicleVisualMesh).
	std::vector<bool> isChild( nodes.size(), false );
	for ( const GltfNode& node : nodes )
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
	Matrix4 meshTransform = IdentityMatrix();
	for ( int i = 0; i < (int)nodes.size(); ++i )
	{
		if ( nodes[i].mesh == primitive.meshIndex )
		{
			meshTransform = world[i];
			break;
		}
	}

	std::vector<MeshVertex> vertices( positionsBU.size() );
	std::vector<b3Vec3> normals;
	if ( hasNormals )
	{
		normals.reserve( normalsBU.size() );
		for ( b3Vec3 nBU : normalsBU )
		{
			b3Vec3 n = TransformVector( meshTransform, nBU );
			normals.push_back( b3Length( n ) > 1.0e-6f ? b3Normalize( n ) : b3Vec3_axisY );
		}
	}
	boundsMin = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	boundsMax = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
	std::vector<b3Vec3> positions( positionsBU.size() );
	for ( size_t i = 0; i < positionsBU.size(); ++i )
	{
		b3Vec3 p = b3MulSV( metersPerBlockbenchUnit, TransformPoint( meshTransform, positionsBU[i] ) );
		positions[i] = p;
		boundsMin = b3Min( boundsMin, p );
		boundsMax = b3Max( boundsMax, p );
		MeshVertex v = {};
		v.position[0] = p.x;
		v.position[1] = p.y;
		v.position[2] = p.z;
		if ( hasTexcoords )
		{
			v.texcoord[0] = texcoords[i].x;
			v.texcoord[1] = texcoords[i].y;
		}
		vertices[i] = v;
	}
	if ( hasNormals == false )
	{
		FillMissingNormals( positions, indices, &normals );
	}
	for ( size_t i = 0; i < vertices.size(); ++i )
	{
		vertices[i].normal[0] = normals[i].x;
		vertices[i].normal[1] = normals[i].y;
		vertices[i].normal[2] = normals[i].z;
	}

	// Opposite-hand copy: reflect across the authored X axis (positions and
	// normals). Winding is fixed per triangle below so faces stay outward.
	if ( mirrorX )
	{
		float oldMinX = boundsMin.x;
		float oldMaxX = boundsMax.x;
		boundsMin.x = -oldMaxX;
		boundsMax.x = -oldMinX;
		for ( size_t i = 0; i < vertices.size(); ++i )
		{
			positions[i].x = -positions[i].x;
			vertices[i].position[0] = -vertices[i].position[0];
			vertices[i].normal[0] = -vertices[i].normal[0];
		}
	}

	// Dominant bone (rigid skin: one weight ~= 1). Map jointIdx -> node index.
	std::vector<int> boneOfVertex( positionsBU.size(), -1 );
	for ( size_t i = 0; i < positionsBU.size(); ++i )
	{
		int bestC = 0;
		float bestW = -1.0f;
		for ( int c = 0; c < 4; ++c )
		{
			float w = weights[i * 4 + c];
			if ( w > bestW )
			{
				bestW = w;
				bestC = c;
			}
		}
		uint32_t jointIdx = jointIndices[i * 4 + bestC];
		boneOfVertex[i] = ( jointIdx < skinJoints.size() ) ? skinJoints[jointIdx] : -1;
	}

	// Triangle bone = its first vertex's bone (rigid => all three agree).
	int triangleCount = (int)indices.size() / 3;
	std::vector<int> triangleBone( triangleCount, -1 );
	std::vector<bool> bonePresent( nodes.size(), false );
	for ( int t = 0; t < triangleCount; ++t )
	{
		int bn = boneOfVertex[indices[t * 3]];
		triangleBone[t] = bn;
		if ( bn >= 0 && bn < (int)bonePresent.size() )
		{
			bonePresent[bn] = true;
		}
	}

	// One registered mesh per bone group.
	for ( int bn = 0; bn < (int)bonePresent.size(); ++bn )
	{
		if ( bonePresent[bn] == false )
		{
			continue;
		}

		std::vector<int> remap( vertices.size(), -1 );
		std::vector<MeshVertex> groupVerts;
		std::vector<uint32_t> groupIndices;
		b3Vec3 partMin = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		b3Vec3 partMax = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
		for ( int t = 0; t < triangleCount; ++t )
		{
			if ( triangleBone[t] != bn )
			{
				continue;
			}
			uint32_t local[3];
			for ( int k = 0; k < 3; ++k )
			{
				uint32_t gi = indices[t * 3 + k];
				if ( remap[gi] < 0 )
				{
					remap[gi] = (int)groupVerts.size();
					groupVerts.push_back( vertices[gi] );
					partMin = b3Min( partMin, positions[gi] );
					partMax = b3Max( partMax, positions[gi] );
				}
				local[k] = (uint32_t)remap[gi];
			}
			// Reflection flips triangle orientation; swap two indices to keep
			// the winding (and so the outward face) correct.
			if ( mirrorX )
			{
				groupIndices.push_back( local[0] );
				groupIndices.push_back( local[2] );
				groupIndices.push_back( local[1] );
			}
			else
			{
				groupIndices.push_back( local[0] );
				groupIndices.push_back( local[1] );
				groupIndices.push_back( local[2] );
			}
		}
		if ( groupVerts.empty() || groupIndices.empty() )
		{
			continue;
		}

		std::string label = std::string( path ) + ( mirrorX ? "#mirror#" : "#" ) +
							 ( bn < (int)nodeNames.size() ? nodeNames[bn] : std::to_string( bn ) );
		uint32_t hash = BuildMeshHash( label.c_str(), groupVerts, groupIndices, useTexture ? &decodedTexture : nullptr );
		MeshHandle handle = FindMesh( hash );
		if ( IsMeshHandleValid( handle ) )
		{
			AddMeshReference( handle );
		}
		else if ( useTexture )
		{
			MeshTextureData textureData = {};
			textureData.width = decodedTexture.width;
			textureData.height = decodedTexture.height;
			textureData.rgba8 = decodedTexture.rgba8.data();
			textureData.byteCount = (int)decodedTexture.rgba8.size();
			handle = RegisterTexturedMesh( hash, groupVerts.data(), (int)groupVerts.size(), groupIndices.data(),
										   (int)groupIndices.size(), &textureData, "jozz_vehicle_rigged_part" );
		}
		if ( IsMeshHandleValid( handle ) == false )
		{
			handle = RegisterMesh( hash, groupVerts.data(), (int)groupVerts.size(), groupIndices.data(),
								   (int)groupIndices.size(), "jozz_vehicle_rigged_part" );
		}
		if ( IsMeshHandleValid( handle ) == false )
		{
			continue;
		}

		JozzVehicleRiggedPart part;
		part.handle = handle;
		part.boneName = bn < (int)nodeNames.size() ? nodeNames[bn] : std::string();
		part.boneNodeIndex = bn;
		part.vertexCount = (int)groupVerts.size();
		part.triangleCount = (int)groupIndices.size() / 3;
		part.restCenter = { 0.5f * ( partMin.x + partMax.x ), 0.5f * ( partMin.y + partMax.y ), 0.5f * ( partMin.z + partMax.z ) };
		part.restMin = partMin;
		part.restMax = partMax;
		// Bone rest world position (authored-world-metres), mirrored to match the
		// geometry. Used to pin a part's bone onto a live mount point.
		b3Vec3 boneRest =
			b3MulSV( metersPerBlockbenchUnit, { world[bn].m[0][3], world[bn].m[1][3], world[bn].m[2][3] } );
		if ( mirrorX )
		{
			boneRest.x = -boneRest.x;
		}
		part.boneRestWorld = boneRest;
		parts.push_back( part );
	}

	textureLoaded = useTexture;
	textureWidth = useTexture ? decodedTexture.width : 0;
	textureHeight = useTexture ? decodedTexture.height : 0;
	textureAlphaCutoff = useTexture ? alphaCutoff : 0.0f;

	if ( parts.empty() )
	{
		status = "rigged mesh: no bone parts produced";
		return false;
	}

	char buffer[128];
	std::snprintf( buffer, sizeof( buffer ), "rigged mesh: %d parts%s", (int)parts.size(), useTexture ? ", textured" : "" );
	status = buffer;
	return true;
}

void JozzVehicleRiggedMesh::Destroy()
{
	for ( JozzVehicleRiggedPart& part : parts )
	{
		if ( IsMeshHandleValid( part.handle ) )
		{
			ReleaseMeshReference( part.handle );
		}
	}
	parts.clear();
	textureLoaded = false;
	textureWidth = 0;
	textureHeight = 0;
	textureAlphaCutoff = 0.0f;
	boundsMin = b3Vec3_zero;
	boundsMax = b3Vec3_zero;
}

bool JozzVehicleRiggedMesh::IsLoaded() const
{
	return parts.empty() == false;
}

int JozzVehicleRiggedMesh::PartCount() const
{
	return (int)parts.size();
}

int JozzVehicleRiggedMesh::FindPart( const char* boneNameSubstring ) const
{
	for ( int i = 0; i < (int)parts.size(); ++i )
	{
		if ( parts[i].boneName.find( boneNameSubstring ) != std::string::npos )
		{
			return i;
		}
	}
	return -1;
}

void JozzVehicleRiggedMesh::DrawPart( int index, b3WorldTransform worldTransform, Vec4 color ) const
{
	if ( index < 0 || index >= (int)parts.size() || IsMeshHandleValid( parts[index].handle ) == false )
	{
		return;
	}
	b3Transform relativeTransform = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	AppendMesh( parts[index].handle, relativeTransform, b3Vec3_one, color, 0.0f, 0.58f,
				textureLoaded ? MESH_MATERIAL_MODE_TEXTURED : MESH_MATERIAL_MODE_SOLID, textureAlphaCutoff,
				TRANSPARENT_SHADOW_FULL );
}

void JozzVehicleRiggedMesh::DrawPartScaled( int index, b3Quat rotation, b3Vec3 scale, b3Vec3 pivotAuthored,
										   b3Pos targetWorld, Vec4 color ) const
{
	if ( index < 0 || index >= (int)parts.size() || IsMeshHandleValid( parts[index].handle ) == false )
	{
		return;
	}
	// worldVert = rotation * (scale . vert) + T. Choose T so the authored pivot
	// lands on targetWorld: T = targetWorld - rotation * (scale . pivot).
	b3Vec3 scaledPivot = { scale.x * pivotAuthored.x, scale.y * pivotAuthored.y, scale.z * pivotAuthored.z };
	b3Vec3 rotatedPivot = b3RotateVector( rotation, scaledPivot );
	b3WorldTransform worldTransform;
	worldTransform.q = rotation;
	worldTransform.p = { targetWorld.x - rotatedPivot.x, targetWorld.y - rotatedPivot.y, targetWorld.z - rotatedPivot.z };

	b3Transform relativeTransform = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	AppendMesh( parts[index].handle, relativeTransform, scale, color, 0.0f, 0.58f,
				textureLoaded ? MESH_MATERIAL_MODE_TEXTURED : MESH_MATERIAL_MODE_SOLID, textureAlphaCutoff,
				TRANSPARENT_SHADOW_FULL );
}

void JozzVehicleRiggedMesh::DrawPartBetween( int index, b3Vec3 authoredA, b3Vec3 authoredB, b3Pos liveA, b3Pos liveB,
											Vec4 color ) const
{
	if ( index < 0 || index >= (int)parts.size() || IsMeshHandleValid( parts[index].handle ) == false )
	{
		return;
	}
	b3Vec3 authoredDir = b3Sub( authoredB, authoredA );
	float authoredLen = b3Length( authoredDir );
	if ( authoredLen < 1.0e-5f )
	{
		return;
	}
	b3Vec3 ua = b3MulSV( 1.0f / authoredLen, authoredDir );
	b3Vec3 liveDir = b3Sub( liveB, liveA );
	float liveLen = b3Length( liveDir );
	b3Vec3 ul = liveLen > 1.0e-5f ? b3MulSV( 1.0f / liveLen, liveDir ) : ua;

	// A minimal rotation (b3ComputeQuatBetweenUnitVectors) leaves the roll about
	// the arm axis free, and it resolves differently for a left vs a mirrored
	// right corner - so the two arms twist unequally and the rig looks crooked.
	// Build the full orientation instead: keep the part's face up and its width
	// along the car's fore/aft, pinning the roll. Because it is derived the same
	// way from ua/ul on both sides, mirrored inputs give a mirrored result.
	const b3Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
	const b3Vec3 worldFwd = { 1.0f, 0.0f, 0.0f };
	// Authored frame: ua (long), upA (authored up made perpendicular to ua), wA.
	b3Vec3 upA = b3Sub( worldUp, b3MulSV( b3Dot( worldUp, ua ), ua ) );
	upA = b3Length( upA ) > 1.0e-4f ? b3Normalize( upA ) : b3Sub( worldFwd, b3MulSV( b3Dot( worldFwd, ua ), ua ) );
	upA = b3Normalize( upA );
	b3Vec3 wA = b3Cross( ua, upA );
	// Target frame: ul (long), upT (world up made perpendicular to ul), wT.
	b3Vec3 upT = b3Sub( worldUp, b3MulSV( b3Dot( worldUp, ul ), ul ) );
	upT = b3Length( upT ) > 1.0e-4f ? b3Normalize( upT ) : b3Sub( worldFwd, b3MulSV( b3Dot( worldFwd, ul ), ul ) );
	upT = b3Normalize( upT );
	b3Vec3 wT = b3Cross( ul, upT );
	// R maps the authored frame (ua, upA, wA) onto the target frame (ul, upT, wT):
	// column j = ul*ua[j] + upT*upA[j] + wT*wA[j].
	b3Matrix3 r;
	r.cx = b3Add( b3Add( b3MulSV( ua.x, ul ), b3MulSV( upA.x, upT ) ), b3MulSV( wA.x, wT ) );
	r.cy = b3Add( b3Add( b3MulSV( ua.y, ul ), b3MulSV( upA.y, upT ) ), b3MulSV( wA.y, wT ) );
	r.cz = b3Add( b3Add( b3MulSV( ua.z, ul ), b3MulSV( upA.z, upT ) ), b3MulSV( wA.z, wT ) );
	b3Quat rotation = b3MakeQuatFromMatrix( &r );

	// Stretch along whichever authored axis the endpoints run along (arms are on
	// authored X, the damper on Y), so the part exactly spans liveA..liveB.
	float s = liveLen / authoredLen;
	b3Vec3 scale = { 1.0f + ( s - 1.0f ) * std::fabs( ua.x ), 1.0f + ( s - 1.0f ) * std::fabs( ua.y ),
					 1.0f + ( s - 1.0f ) * std::fabs( ua.z ) };
	DrawPartScaled( index, rotation, scale, authoredA, liveA, color );
}

void JozzVehicleRiggedMesh::DrawTelescopingDamper( b3Pos topWorld, b3Pos botWorld, Vec4 color ) const
{
	int upper = FindPart( "Upper" );
	int stretch = FindPart( "Stretch" );
	int lower = FindPart( "Lower" );
	if ( upper < 0 || lower < 0 )
	{
		return;
	}

	b3Vec3 restSpan = b3Sub( parts[upper].boneRestWorld, parts[lower].boneRestWorld );
	float restGap = b3Length( restSpan );
	b3Vec3 authoredAxis = restGap > 1.0e-5f ? b3MulSV( 1.0f / restGap, restSpan ) : b3Vec3_axisY;

	b3Vec3 liveSpan = b3Sub( topWorld, botWorld );
	float liveGap = b3Length( liveSpan );
	b3Vec3 liveAxis = liveGap > 1.0e-5f ? b3MulSV( 1.0f / liveGap, liveSpan ) : b3Vec3_axisY;

	b3Quat rotation = b3ComputeQuatBetweenUnitVectors( authoredAxis, liveAxis );

	// Rigid tubes pinned to the two mount points.
	DrawPartScaled( upper, rotation, b3Vec3_one, parts[upper].boneRestWorld, topWorld, color );
	DrawPartScaled( lower, rotation, b3Vec3_one, parts[lower].boneRestWorld, botWorld, color );

	// Stretch section: scaled along the damper axis, placed at the same fraction
	// of the segment it occupies at rest so it stays exact when uncompressed.
	if ( stretch >= 0 && restGap > 1.0e-4f )
	{
		float f = b3ClampFloat( b3Dot( b3Sub( parts[upper].boneRestWorld, parts[stretch].boneRestWorld ), authoredAxis ) / restGap,
								0.0f, 1.0f );
		b3Vec3 target = b3Add( topWorld, b3MulSV( f, b3Sub( botWorld, topWorld ) ) );
		float scaleY = liveGap / restGap;
		DrawPartScaled( stretch, rotation, { 1.0f, scaleY, 1.0f }, parts[stretch].boneRestWorld, target, color );
	}
}

b3Transform ComputeJozzVehicleWheelVisualCorrection( const JozzVehicleVisualMesh& mesh,
													 const JozzVehicleAuditMetadata& metadata,
													 float metersPerBlockbenchUnit )
{
	b3Vec3 visualCenter;
	if ( mesh.IsLoaded() )
	{
		visualCenter = {
			0.5f * ( mesh.boundsMin.x + mesh.boundsMax.x ),
			0.5f * ( mesh.boundsMin.y + mesh.boundsMax.y ),
			0.5f * ( mesh.boundsMin.z + mesh.boundsMax.z ),
		};
	}
	else
	{
		b3Vec3 wheelMountBU = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		b3Vec3 radiusOuterBU = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter" );
		b3Vec3 wheelCenterBU = { radiusOuterBU.x, wheelMountBU.y, wheelMountBU.z };
		visualCenter = b3MulSV( metersPerBlockbenchUnit, wheelCenterBU );
	}

	// The authored wheel spin/width axis is +X, while the primitive wheel body
	// uses local +Y as its axle. Map authored +X to body +Y and center the
	// authored wheel center on the primitive body origin.
	b3Quat visualXToBodyY = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 0.5f * B3_PI );
	b3Quat visualUpToBodyRadial = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
	b3Quat correctionRotation = b3MulQuat( visualUpToBodyRadial, visualXToBodyY );
	return { b3Neg( b3RotateVector( correctionRotation, visualCenter ) ), correctionRotation };
}
