// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_config_io.h"

#include "jozz_vehicle_json.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace jozz;

namespace
{

// --- writer: hand-rolled, not a general JSON library -----------------------
// The config is a flat set of named floats/ints/bools/vec3s (plus small
// nested structs); a real serializer would be a lot of machinery for what is,
// in the end, a fixed list of "key: number" lines.

void WriteFloat( std::ostringstream& out, const char* indent, const char* key, float value, bool comma = true )
{
	out << indent << "\"" << key << "\": " << value << ( comma ? ",\n" : "\n" );
}

void WriteInt( std::ostringstream& out, const char* indent, const char* key, int value, bool comma = true )
{
	out << indent << "\"" << key << "\": " << value << ( comma ? ",\n" : "\n" );
}

void WriteBool( std::ostringstream& out, const char* indent, const char* key, bool value, bool comma = true )
{
	out << indent << "\"" << key << "\": " << ( value ? "true" : "false" ) << ( comma ? ",\n" : "\n" );
}

void WriteVec3( std::ostringstream& out, const char* indent, const char* key, b3Vec3 v, bool comma = true )
{
	out << indent << "\"" << key << "\": [" << v.x << ", " << v.y << ", " << v.z << "]" << ( comma ? ",\n" : "\n" );
}

void WriteString( std::ostringstream& out, const char* indent, const char* key, const char* value, bool comma = true )
{
	// Registry keys are [a-z0-9_] by construction (SanitizeJozzVehicleM6Config
	// enforces it), so no JSON escaping is needed - assert the assumption
	// instead of half-implementing an escaper.
	out << indent << "\"" << key << "\": \"" << value << "\"" << ( comma ? ",\n" : "\n" );
}

// --- reader helpers ----------------------------------------------------
// Every read is best-effort: a missing or malformed key leaves *outValue
// untouched, so loading an older preset that predates a field just keeps
// whatever the caller pre-filled (normally JozzVehicleM6DefaultConfig).

void ReadFloat( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key,
				 float* outValue )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	if ( valueIndex >= 0 )
	{
		TokenFloat( json, tokens[valueIndex], outValue );
	}
}

void ReadInt( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key,
			  int* outValue )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	if ( valueIndex >= 0 )
	{
		TokenInt( json, tokens[valueIndex], outValue );
	}
}

void ReadBool( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key,
			   bool* outValue )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	if ( valueIndex >= 0 )
	{
		TokenBool( json, tokens[valueIndex], outValue );
	}
}

void ReadVec3( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key,
			   b3Vec3* outValue )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	if ( valueIndex >= 0 )
	{
		float v[3];
		if ( ParseFloatArray( json, tokens, valueIndex, v, 3 ) )
		{
			*outValue = { v[0], v[1], v[2] };
		}
	}
}

void ReadString( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex, const char* key,
				  char* out, int cap )
{
	int valueIndex = FindObjectValue( json, tokens, objectIndex, key );
	if ( valueIndex >= 0 && tokens[valueIndex].type == JSMN_STRING )
	{
		std::string value = TokenString( json, tokens[valueIndex] );
		std::snprintf( out, (size_t)cap, "%s", value.c_str() );
	}
}

// --- R2: field table -------------------------------------------------------
// One row = one field, driving BOTH the writer and the reader, so a new
// tunable is one entry instead of two hand-kept call sites that can drift
// apart (the exact bug class this replaces: a field added to the writer and
// forgotten in the reader loads back as whatever the caller pre-filled, with
// no error - silently stale).
//
// Deliberately NOT a reflection of JozzVehicleM6Config: rackTravel is
// DERIVED (recomputed from wishbone geometry after every load - see the
// "stale rackTravel" tripwire in jozz_vehicle_m6_suspension_rig.cpp and the
// validator's p2 rackTravel regression probe) and filterGroupIndex is
// runtime-only. Neither belongs in a saved file; both are absent from the
// table below on purpose, exactly as they were absent from the old
// hand-written Write*/Read* call sequence.
//
// One array, not "plain arrays per type": the JSON key order interleaves
// vec3/float/int fields and three nested objects in a specific historical
// sequence (byte-identical output is the R2 acceptance bar), so a single
// ordered, type-tagged row is the only shape that reproduces that order
// without extra bookkeeping. The union is anonymous (not a named member), so
// each row is still a plain aggregate initializer - no macros, no builder
// functions.
enum class JozzFieldType
{
	Float,
	Int,
	Bool,
	Vec3,
	String,
};

template <typename Owner>
struct JozzFieldDesc
{
	const char* key;
	JozzFieldType type;
	bool lastInObject; // false -> writer emits a trailing comma
	union
	{
		float Owner::*floatMember;
		int Owner::*intMember;
		bool Owner::*boolMember;
		b3Vec3 Owner::*vec3Member;
		// Fixed-size buffer, not std::string, to keep JozzVehicleM6Config a
		// plain aggregate (probes copy it by value). JOZZ_M6_MODEL_KEY_CAP is
		// shared by every string field of the config today; a field with a
		// different cap would need a second union member here.
		char ( Owner::*stringMember )[JOZZ_M6_MODEL_KEY_CAP];
	};
};

template <typename Owner>
void WriteFieldTable( std::ostringstream& out, const char* indent, const JozzFieldDesc<Owner>* fields, int count,
					   const Owner& obj )
{
	for ( int i = 0; i < count; ++i )
	{
		const JozzFieldDesc<Owner>& f = fields[i];
		bool comma = f.lastInObject == false;
		switch ( f.type )
		{
			case JozzFieldType::Float:
				WriteFloat( out, indent, f.key, obj.*f.floatMember, comma );
				break;
			case JozzFieldType::Int:
				WriteInt( out, indent, f.key, obj.*f.intMember, comma );
				break;
			case JozzFieldType::Bool:
				WriteBool( out, indent, f.key, obj.*f.boolMember, comma );
				break;
			case JozzFieldType::Vec3:
				WriteVec3( out, indent, f.key, obj.*f.vec3Member, comma );
				break;
			case JozzFieldType::String:
				WriteString( out, indent, f.key, obj.*f.stringMember, comma );
				break;
		}
	}
}

template <typename Owner>
void ReadFieldTable( std::string_view json, const std::vector<jsmntok_t>& tokens, int objectIndex,
					  const JozzFieldDesc<Owner>* fields, int count, Owner* obj )
{
	for ( int i = 0; i < count; ++i )
	{
		const JozzFieldDesc<Owner>& f = fields[i];
		switch ( f.type )
		{
			case JozzFieldType::Float:
				ReadFloat( json, tokens, objectIndex, f.key, &( obj->*f.floatMember ) );
				break;
			case JozzFieldType::Int:
				ReadInt( json, tokens, objectIndex, f.key, &( obj->*f.intMember ) );
				break;
			case JozzFieldType::Bool:
				ReadBool( json, tokens, objectIndex, f.key, &( obj->*f.boolMember ) );
				break;
			case JozzFieldType::Vec3:
				ReadVec3( json, tokens, objectIndex, f.key, &( obj->*f.vec3Member ) );
				break;
			case JozzFieldType::String:
				ReadString( json, tokens, objectIndex, f.key, obj->*f.stringMember, JOZZ_M6_MODEL_KEY_CAP );
				break;
		}
	}
}

// Root fields, split into the three segments the two nested objects
// (wishbone, wheelEnvelope) and trailingArm fall between - same three
// segments the old hand-written writer visited in the same order.
using RootField = JozzFieldDesc<JozzVehicleM6Config>;

// Segment A: chassisHalfExtents .. rearRigType (before the wishbone object).
const RootField kRootFieldsA[] = {
	{ .key = "chassisHalfExtents", .type = JozzFieldType::Vec3, .lastInObject = false, .vec3Member = &JozzVehicleM6Config::chassisHalfExtents },
	{ .key = "chassisDensity", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::chassisDensity },
	{ .key = "cgVerticalOffset", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::cgVerticalOffset },
	{ .key = "axleHalfSpacing", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::axleHalfSpacing },
	{ .key = "trackHalfWidth", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::trackHalfWidth },
	{ .key = "restDrop", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::restDrop },
	{ .key = "frontRigType", .type = JozzFieldType::Int, .lastInObject = false, .intMember = &JozzVehicleM6Config::frontRigType },
	{ .key = "rearRigType", .type = JozzFieldType::Int, .lastInObject = false, .intMember = &JozzVehicleM6Config::rearRigType },
};

// Segment B: knuckleMass .. rackServoMaxSpeed (between trailingArm and wheelEnvelope).
const RootField kRootFieldsB[] = {
	{ .key = "knuckleMass", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::knuckleMass },
	{ .key = "armMass", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::armMass },
	{ .key = "rackMass", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackMass },
	{ .key = "rackHalfWidth", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackHalfWidth },
	{ .key = "rackServoForce", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackServoForce },
	{ .key = "rackServoSpeedGain", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackServoSpeedGain },
	{ .key = "rackServoMaxSpeed", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackServoMaxSpeed },
};

// Segment C: wheelDensity .. uprightDampingRatio (after wheelEnvelope, to the
// end of the object - the LAST row here carries the file's final key).
const RootField kRootFieldsC[] = {
	{ .key = "wheelDensity", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::wheelDensity },
	{ .key = "wheelFriction", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::wheelFriction },
	{ .key = "wheelRollingResistance", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::wheelRollingResistance },
	{ .key = "suspensionHertz", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::suspensionHertz },
	{ .key = "suspensionDampingRatio", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::suspensionDampingRatio },
	{ .key = "frontSuspensionScale", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::frontSuspensionScale },
	{ .key = "rearSuspensionScale", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rearSuspensionScale },
	{ .key = "reboundTravel", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::reboundTravel },
	{ .key = "compressionTravel", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::compressionTravel },
	{ .key = "suspensionPreloadFront", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::suspensionPreloadFront },
	{ .key = "suspensionPreloadRear", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::suspensionPreloadRear },
	{ .key = "arbFrontStiffness", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::arbFrontStiffness },
	{ .key = "arbRearStiffness", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::arbRearStiffness },
	{ .key = "aeroDragArea", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::aeroDragArea },
	{ .key = "maxDriveSpeed", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::maxDriveSpeed },
	{ .key = "maxDriveTorque", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::maxDriveTorque },
	{ .key = "driveTaperStart", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::driveTaperStart },
	{ .key = "brakeTorque", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::brakeTorque },
	{ .key = "coastTorque", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::coastTorque },
	{ .key = "allWheelDrive", .type = JozzFieldType::Bool, .lastInObject = false, .boolMember = &JozzVehicleM6Config::allWheelDrive },
	{ .key = "maxSteeringAngleDegrees", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::maxSteeringAngleDegrees },
	{ .key = "frontToeDeg", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::frontToeDeg },
	{ .key = "rearToeDeg", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rearToeDeg },
	{ .key = "steeringHertz", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::steeringHertz },
	{ .key = "steeringDampingRatio", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::steeringDampingRatio },
	{ .key = "maxSteeringTorque", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::maxSteeringTorque },
	{ .key = "rackFrictionBase", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackFrictionBase },
	{ .key = "rackFrictionLoadCoeff", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackFrictionLoadCoeff },
	{ .key = "steeringFrictionTorque", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::steeringFrictionTorque },
	{ .key = "steerInputDeadzone", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::steerInputDeadzone },
	{ .key = "rackCenteringHertz", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::rackCenteringHertz },
	{ .key = "ackermannGeometry", .type = JozzFieldType::Bool, .lastInObject = false, .boolMember = &JozzVehicleM6Config::ackermannGeometry },
	{ .key = "strutCasterDeg", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::strutCasterDeg },
	{ .key = "uprightAssist", .type = JozzFieldType::Bool, .lastInObject = false, .boolMember = &JozzVehicleM6Config::uprightAssist },
	{ .key = "uprightHertz", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::uprightHertz },
	{ .key = "uprightDampingRatio", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::uprightDampingRatio },
	// Visual identity (plan finalizacji 2026-07-11): part of the config on
	// purpose - a preset describes the whole car, look included.
	{ .key = "bodyVisualModel", .type = JozzFieldType::String, .lastInObject = false, .stringMember = &JozzVehicleM6Config::bodyVisualModel },
	{ .key = "bodyVisualOffset", .type = JozzFieldType::Vec3, .lastInObject = false, .vec3Member = &JozzVehicleM6Config::bodyVisualOffset },
	{ .key = "frontSuspensionVisualModel", .type = JozzFieldType::String, .lastInObject = true, .stringMember = &JozzVehicleM6Config::frontSuspensionVisualModel },
};

using WishboneField = JozzFieldDesc<JozzVehicleM6WishboneGeometry>;
const WishboneField kWishboneFields[] = {
	{ .key = "uprightHalfHeight", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::uprightHalfHeight },
	{ .key = "kingpinOffset", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::kingpinOffset },
	{ .key = "casterDeg", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::casterDeg },
	{ .key = "kingpinInclinationDeg", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::kingpinInclinationDeg },
	{ .key = "upperArmLength", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::upperArmLength },
	{ .key = "lowerArmLength", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::lowerArmLength },
	{ .key = "armHalfSpread", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::armHalfSpread },
	{ .key = "steeringArmBack", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::steeringArmBack },
	{ .key = "ackermannTrapezoid", .type = JozzFieldType::Bool, .lastInObject = false, .boolMember = &JozzVehicleM6WishboneGeometry::ackermannTrapezoid },
	{ .key = "ackermannFraction", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::ackermannFraction },
	{ .key = "coiloverTopHeight", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::coiloverTopHeight },
	{ .key = "coiloverTopInboard", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WishboneGeometry::coiloverTopInboard },
	{ .key = "restArmDroopDeg", .type = JozzFieldType::Float, .lastInObject = true, .floatMember = &JozzVehicleM6WishboneGeometry::restArmDroopDeg },
};

using TrailingArmField = JozzFieldDesc<JozzVehicleM6TrailingArmGeometry>;
const TrailingArmField kTrailingArmFields[] = {
	{ .key = "pivotOffset", .type = JozzFieldType::Vec3, .lastInObject = false, .vec3Member = &JozzVehicleM6TrailingArmGeometry::pivotOffset },
	{ .key = "damperArmOffset", .type = JozzFieldType::Vec3, .lastInObject = false, .vec3Member = &JozzVehicleM6TrailingArmGeometry::damperArmOffset },
	{ .key = "damperChassisOffset", .type = JozzFieldType::Vec3, .lastInObject = false, .vec3Member = &JozzVehicleM6TrailingArmGeometry::damperChassisOffset },
	{ .key = "armMass", .type = JozzFieldType::Float, .lastInObject = true, .floatMember = &JozzVehicleM6TrailingArmGeometry::armMass },
};

using WheelEnvelopeField = JozzFieldDesc<JozzVehicleM6WheelEnvelopeDesc>;
const WheelEnvelopeField kWheelEnvelopeFields[] = {
	{ .key = "mode", .type = JozzFieldType::Int, .lastInObject = false, .intMember = &JozzVehicleM6WheelEnvelopeDesc::mode },
	{ .key = "cylinderSides", .type = JozzFieldType::Int, .lastInObject = false, .intMember = &JozzVehicleM6WheelEnvelopeDesc::cylinderSides },
	{ .key = "unionLayerCount", .type = JozzFieldType::Int, .lastInObject = false, .intMember = &JozzVehicleM6WheelEnvelopeDesc::unionLayerCount },
	// Appended 2026-08-03 with the torus. Files written before it simply lack
	// these keys; the reader leaves the struct defaults in place and the
	// sanitizer then raises the segment count to a sealed ring, so an old
	// preset switched to torus by hand still builds a wheel without holes.
	{ .key = "torusSegments", .type = JozzFieldType::Int, .lastInObject = false, .intMember = &JozzVehicleM6WheelEnvelopeDesc::torusSegments },
	{ .key = "torusCrownRadius", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WheelEnvelopeDesc::torusCrownRadius },
	{ .key = "wheelCrownDrop", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6WheelEnvelopeDesc::wheelCrownDrop },
	{ .key = "wheelProfilePoints", .type = JozzFieldType::Int, .lastInObject = true, .intMember = &JozzVehicleM6WheelEnvelopeDesc::wheelProfilePoints },
};

template <typename T, size_t N>
constexpr int ArrayCount( const T ( & )[N] )
{
	return (int)N;
}

} // namespace

bool SaveJozzVehicleM6Config( const JozzVehicleM6Config& c, const std::string& path )
{
	std::error_code ec;
	std::filesystem::create_directories( std::filesystem::path( path ).parent_path(), ec );

	std::ostringstream out;
	out << "{\n";
	WriteFieldTable( out, "  ", kRootFieldsA, ArrayCount( kRootFieldsA ), c );

	out << "  \"wishbone\": {\n";
	WriteFieldTable( out, "    ", kWishboneFields, ArrayCount( kWishboneFields ), c.wishbone );
	out << "  },\n";

	out << "  \"trailingArm\": {\n";
	WriteFieldTable( out, "    ", kTrailingArmFields, ArrayCount( kTrailingArmFields ), c.trailingArm );
	out << "  },\n";

	WriteFieldTable( out, "  ", kRootFieldsB, ArrayCount( kRootFieldsB ), c );

	out << "  \"wheelEnvelope\": {\n";
	WriteFieldTable( out, "    ", kWheelEnvelopeFields, ArrayCount( kWheelEnvelopeFields ), c.wheelEnvelope );
	out << "  },\n";

	WriteFieldTable( out, "  ", kRootFieldsC, ArrayCount( kRootFieldsC ), c );
	out << "}\n";

	std::ofstream file( path, std::ios::binary | std::ios::trunc );
	if ( file.is_open() == false )
	{
		return false;
	}
	file << out.str();
	return file.good();
}

bool LoadJozzVehicleM6Config( const std::string& path, JozzVehicleM6Config* outConfig )
{
	std::string json;
	if ( ReadTextFile( path, &json ) == false )
	{
		return false;
	}

	std::vector<jsmntok_t> tokens;
	if ( ParseJson( json, &tokens ) == false || tokens.empty() || tokens[0].type != JSMN_OBJECT )
	{
		return false;
	}

	int root = 0;
	JozzVehicleM6Config& c = *outConfig;
	ReadFieldTable( json, tokens, root, kRootFieldsA, ArrayCount( kRootFieldsA ), &c );

	int wishbone = FindObjectValue( json, tokens, root, "wishbone" );
	if ( wishbone >= 0 )
	{
		ReadFieldTable( json, tokens, wishbone, kWishboneFields, ArrayCount( kWishboneFields ), &c.wishbone );
	}

	int trailingArm = FindObjectValue( json, tokens, root, "trailingArm" );
	if ( trailingArm >= 0 )
	{
		ReadFieldTable( json, tokens, trailingArm, kTrailingArmFields, ArrayCount( kTrailingArmFields ),
						 &c.trailingArm );
	}

	ReadFieldTable( json, tokens, root, kRootFieldsB, ArrayCount( kRootFieldsB ), &c );

	int envelope = FindObjectValue( json, tokens, root, "wheelEnvelope" );
	if ( envelope >= 0 )
	{
		ReadFieldTable( json, tokens, envelope, kWheelEnvelopeFields, ArrayCount( kWheelEnvelopeFields ),
						 &c.wheelEnvelope );
	}

	ReadFieldTable( json, tokens, root, kRootFieldsC, ArrayCount( kRootFieldsC ), &c );

	{
		// Backward compat: pre-P3 files (session/presets saved before the
		// front/rear split) only have a single "suspensionPreload" key, applied
		// equally to both axles. Only takes effect if that legacy key is present
		// - the front/rear reads above already handle new-format files. Not
		// table-driven on purpose: this is a 1-key-to-2-fields migration, not a
		// field, so it stays hand-written exactly as before R2.
		int legacyIndex = FindObjectValue( json, tokens, root, "suspensionPreload" );
		if ( legacyIndex >= 0 )
		{
			float legacyPreload = c.suspensionPreloadFront;
			TokenFloat( json, tokens[legacyIndex], &legacyPreload );
			c.suspensionPreloadFront = legacyPreload;
			c.suspensionPreloadRear = legacyPreload;
		}
	}

	{
		// Legacy rack-friction keys (pre-P4 "rackFrictionForce" and the P4
		// flat static/kinetic pair) describe a DIFFERENT model - a flat force
		// cap - and there is no honest numeric conversion to the P4b
		// load-dependent pair (base + coeff * tie-rod load). Deliberately NOT
		// converted: files carrying only legacy keys get the current model
		// defaults, with a printed note so nobody wonders why an old tuned
		// value "disappeared". (Old files keep loading fine - every other key
		// is unchanged.) Not table-driven: these keys map to nothing, only to
		// a diagnostic printf, so there is no field to put in the table.
		const char* legacyKeys[] = { "rackFrictionForce", "rackStaticFrictionForce", "rackKineticFrictionForce" };
		for ( const char* key : legacyKeys )
		{
			if ( FindObjectValue( json, tokens, root, key ) >= 0 )
			{
				std::printf( "jozz m6 note: legacy key '%s' ignored - rack friction is load-dependent now "
							 "(rackFrictionBase/rackFrictionLoadCoeff, P4b)\n",
							 key );
			}
		}
	}

	return true;
}

bool LoadJozzVehicleM6PresetConfig( const std::string& path, const JozzVehicleM6Config& factoryDefaults,
									JozzVehicleM6Config* outConfig )
{
	// Deterministic by construction: start from the factory baseline, then let
	// the file's keys override. On failure *outConfig is untouched.
	JozzVehicleM6Config fresh = factoryDefaults;
	if ( LoadJozzVehicleM6Config( path, &fresh ) == false )
	{
		return false;
	}
	*outConfig = fresh;
	return true;
}

std::vector<std::string> ListJozzVehicleM6Presets( const std::string& directoryPath )
{
	std::vector<std::string> names;
	std::error_code ec;
	if ( std::filesystem::exists( directoryPath, ec ) == false )
	{
		return names;
	}
	for ( const auto& entry : std::filesystem::directory_iterator( directoryPath, ec ) )
	{
		if ( entry.is_regular_file() && entry.path().extension() == ".json" )
		{
			names.push_back( entry.path().stem().string() );
		}
	}
	std::sort( names.begin(), names.end() );
	return names;
}
