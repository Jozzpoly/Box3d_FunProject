// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_json.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace jozz
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

bool TokenInt( std::string_view json, const jsmntok_t& token, int* out )
{
	float value = 0.0f;
	if ( TokenFloat( json, token, &value ) == false )
	{
		return false;
	}

	*out = (int)value;
	return std::fabs( value - (float)*out ) < 0.001f;
}

bool TokenBool( std::string_view json, const jsmntok_t& token, bool* out )
{
	if ( token.type != JSMN_PRIMITIVE )
	{
		return false;
	}

	std::string text = TokenString( json, token );
	if ( text == "true" )
	{
		*out = true;
		return true;
	}

	if ( text == "false" )
	{
		*out = false;
		return true;
	}

	return false;
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

int GetArrayElement( const std::vector<jsmntok_t>& tokens, int arrayIndex, int element )
{
	if ( arrayIndex < 0 || arrayIndex >= (int)tokens.size() || tokens[arrayIndex].type != JSMN_ARRAY || element < 0 ||
		 element >= tokens[arrayIndex].size )
	{
		return -1;
	}

	int current = arrayIndex + 1;
	for ( int i = 0; i < element; ++i )
	{
		current = SkipToken( tokens, current );
	}
	return current;
}

bool ParseFloatArray( std::string_view json, const std::vector<jsmntok_t>& tokens, int arrayIndex, float* out,
					  int expectedCount )
{
	if ( arrayIndex < 0 || arrayIndex >= (int)tokens.size() || tokens[arrayIndex].type != JSMN_ARRAY ||
		 tokens[arrayIndex].size != expectedCount )
	{
		return false;
	}

	for ( int i = 0; i < expectedCount; ++i )
	{
		int elementIndex = GetArrayElement( tokens, arrayIndex, i );
		if ( elementIndex < 0 || TokenFloat( json, tokens[elementIndex], &out[i] ) == false )
		{
			return false;
		}
	}

	return true;
}

bool ParseJson( const std::string& json, std::vector<jsmntok_t>* tokens )
{
	jsmn_parser parser;
	jsmn_init( &parser );
	int tokenCount = jsmn_parse( &parser, json.data(), json.size(), nullptr, 0 );
	if ( tokenCount <= 0 )
	{
		return false;
	}

	tokens->resize( (size_t)tokenCount );
	jsmn_init( &parser );
	int parsedCount = jsmn_parse( &parser, json.data(), json.size(), tokens->data(), (unsigned int)tokens->size() );
	return parsedCount > 0 && tokens->empty() == false;
}

bool ReadTextFile( const std::filesystem::path& path, std::string* out )
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

} // namespace jozz
