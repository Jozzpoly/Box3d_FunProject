// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// Shared jsmn token helpers for the Jozz Vehicle JSON readers (audit metadata,
// asset contracts, glTF visual mesh). These were previously duplicated per
// translation unit; keep additions here so a parser fix lands everywhere.

#define JSMN_STATIC
#include "jsmn.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace jozz
{

bool TokenEquals( std::string_view json, const jsmntok_t& token, const char* text );
std::string TokenString( std::string_view json, const jsmntok_t& token );
bool TokenFloat( std::string_view json, const jsmntok_t& token, float* out );
bool TokenInt( std::string_view json, const jsmntok_t& token, int* out );
bool TokenBool( std::string_view json, const jsmntok_t& token, bool* out );

// Returns the token index just past the subtree rooted at index.
int SkipToken( const std::vector<jsmntok_t>& tokens, int index );

// Returns the value token index for key inside the object at objectIndex, or -1.
int FindObjectValue( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key );

// Returns the token index of array element `element`, or -1.
int GetArrayElement( const std::vector<jsmntok_t>& tokens, int arrayIndex, int element );

// Fails unless the array holds exactly expectedCount numeric tokens.
bool ParseFloatArray( std::string_view json, const std::vector<jsmntok_t>& tokens, int arrayIndex, float* out,
					  int expectedCount );

// Two-pass jsmn parse. Root type is not checked here because the Jozz readers
// consume both object-rooted (glTF, contracts) and array-rooted (audit report)
// documents; callers must validate tokens[0].type themselves.
bool ParseJson( const std::string& json, std::vector<jsmntok_t>* tokens );

bool ReadTextFile( const std::filesystem::path& path, std::string* out );

} // namespace jozz
