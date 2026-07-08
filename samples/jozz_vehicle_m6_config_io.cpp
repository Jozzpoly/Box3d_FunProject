// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_config_io.h"

#include "jozz_vehicle_json.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace jozz;

namespace
{

// --- writer: hand-rolled, not a general JSON library -----------------------
// The config is a flat set of named floats/ints/bools/vec3s (plus two small
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

} // namespace

bool SaveJozzVehicleM6Config( const JozzVehicleM6Config& c, const std::string& path )
{
	std::error_code ec;
	std::filesystem::create_directories( std::filesystem::path( path ).parent_path(), ec );

	std::ostringstream out;
	out << "{\n";
	WriteVec3( out, "  ", "chassisHalfExtents", c.chassisHalfExtents );
	WriteFloat( out, "  ", "chassisDensity", c.chassisDensity );
	WriteFloat( out, "  ", "cgVerticalOffset", c.cgVerticalOffset );
	WriteFloat( out, "  ", "axleHalfSpacing", c.axleHalfSpacing );
	WriteFloat( out, "  ", "trackHalfWidth", c.trackHalfWidth );
	WriteFloat( out, "  ", "restDrop", c.restDrop );
	WriteInt( out, "  ", "frontRigType", c.frontRigType );
	WriteInt( out, "  ", "rearRigType", c.rearRigType );

	out << "  \"wishbone\": {\n";
	WriteFloat( out, "    ", "uprightHalfHeight", c.wishbone.uprightHalfHeight );
	WriteFloat( out, "    ", "kingpinOffset", c.wishbone.kingpinOffset );
	WriteFloat( out, "    ", "casterDeg", c.wishbone.casterDeg );
	WriteFloat( out, "    ", "kingpinInclinationDeg", c.wishbone.kingpinInclinationDeg );
	WriteFloat( out, "    ", "upperArmLength", c.wishbone.upperArmLength );
	WriteFloat( out, "    ", "lowerArmLength", c.wishbone.lowerArmLength );
	WriteFloat( out, "    ", "armHalfSpread", c.wishbone.armHalfSpread );
	WriteFloat( out, "    ", "steeringArmBack", c.wishbone.steeringArmBack );
	WriteBool( out, "    ", "ackermannTrapezoid", c.wishbone.ackermannTrapezoid );
	WriteFloat( out, "    ", "ackermannFraction", c.wishbone.ackermannFraction );
	WriteFloat( out, "    ", "coiloverTopHeight", c.wishbone.coiloverTopHeight );
	WriteFloat( out, "    ", "coiloverTopInboard", c.wishbone.coiloverTopInboard );
	WriteFloat( out, "    ", "restArmDroopDeg", c.wishbone.restArmDroopDeg, false );
	out << "  },\n";

	out << "  \"trailingArm\": {\n";
	WriteVec3( out, "    ", "pivotOffset", c.trailingArm.pivotOffset );
	WriteVec3( out, "    ", "damperArmOffset", c.trailingArm.damperArmOffset );
	WriteVec3( out, "    ", "damperChassisOffset", c.trailingArm.damperChassisOffset );
	WriteFloat( out, "    ", "armMass", c.trailingArm.armMass, false );
	out << "  },\n";

	WriteFloat( out, "  ", "knuckleMass", c.knuckleMass );
	WriteFloat( out, "  ", "armMass", c.armMass );
	WriteFloat( out, "  ", "rackMass", c.rackMass );
	WriteFloat( out, "  ", "rackHalfWidth", c.rackHalfWidth );
	WriteFloat( out, "  ", "rackServoForce", c.rackServoForce );
	WriteFloat( out, "  ", "rackServoSpeedGain", c.rackServoSpeedGain );
	WriteFloat( out, "  ", "rackServoMaxSpeed", c.rackServoMaxSpeed );

	out << "  \"wheelEnvelope\": {\n";
	WriteInt( out, "    ", "mode", c.wheelEnvelope.mode );
	WriteInt( out, "    ", "cylinderSides", c.wheelEnvelope.cylinderSides );
	WriteInt( out, "    ", "unionLayerCount", c.wheelEnvelope.unionLayerCount, false );
	out << "  },\n";

	WriteFloat( out, "  ", "wheelDensity", c.wheelDensity );
	WriteFloat( out, "  ", "wheelFriction", c.wheelFriction );
	WriteFloat( out, "  ", "wheelRollingResistance", c.wheelRollingResistance );

	WriteFloat( out, "  ", "suspensionHertz", c.suspensionHertz );
	WriteFloat( out, "  ", "suspensionDampingRatio", c.suspensionDampingRatio );
	WriteFloat( out, "  ", "frontSuspensionScale", c.frontSuspensionScale );
	WriteFloat( out, "  ", "rearSuspensionScale", c.rearSuspensionScale );
	WriteFloat( out, "  ", "reboundTravel", c.reboundTravel );
	WriteFloat( out, "  ", "compressionTravel", c.compressionTravel );
	WriteFloat( out, "  ", "suspensionPreloadFront", c.suspensionPreloadFront );
	WriteFloat( out, "  ", "suspensionPreloadRear", c.suspensionPreloadRear );

	WriteFloat( out, "  ", "arbFrontStiffness", c.arbFrontStiffness );
	WriteFloat( out, "  ", "arbRearStiffness", c.arbRearStiffness );

	WriteFloat( out, "  ", "aeroDragArea", c.aeroDragArea );

	WriteFloat( out, "  ", "maxDriveSpeed", c.maxDriveSpeed );
	WriteFloat( out, "  ", "maxDriveTorque", c.maxDriveTorque );
	WriteFloat( out, "  ", "driveTaperStart", c.driveTaperStart );
	WriteFloat( out, "  ", "brakeTorque", c.brakeTorque );
	WriteFloat( out, "  ", "coastTorque", c.coastTorque );
	WriteBool( out, "  ", "allWheelDrive", c.allWheelDrive );

	WriteFloat( out, "  ", "maxSteeringAngleDegrees", c.maxSteeringAngleDegrees );
	WriteFloat( out, "  ", "steeringHertz", c.steeringHertz );
	WriteFloat( out, "  ", "steeringDampingRatio", c.steeringDampingRatio );
	WriteFloat( out, "  ", "maxSteeringTorque", c.maxSteeringTorque );
	WriteFloat( out, "  ", "rackFrictionForce", c.rackFrictionForce );
	WriteFloat( out, "  ", "steeringFrictionTorque", c.steeringFrictionTorque );
	WriteFloat( out, "  ", "steerInputDeadzone", c.steerInputDeadzone );
	WriteBool( out, "  ", "ackermannGeometry", c.ackermannGeometry );
	WriteFloat( out, "  ", "strutCasterDeg", c.strutCasterDeg );

	WriteBool( out, "  ", "uprightAssist", c.uprightAssist );
	WriteFloat( out, "  ", "uprightHertz", c.uprightHertz );
	WriteFloat( out, "  ", "uprightDampingRatio", c.uprightDampingRatio, false );
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
	ReadVec3( json, tokens, root, "chassisHalfExtents", &c.chassisHalfExtents );
	ReadFloat( json, tokens, root, "chassisDensity", &c.chassisDensity );
	ReadFloat( json, tokens, root, "cgVerticalOffset", &c.cgVerticalOffset );
	ReadFloat( json, tokens, root, "axleHalfSpacing", &c.axleHalfSpacing );
	ReadFloat( json, tokens, root, "trackHalfWidth", &c.trackHalfWidth );
	ReadFloat( json, tokens, root, "restDrop", &c.restDrop );
	ReadInt( json, tokens, root, "frontRigType", &c.frontRigType );
	ReadInt( json, tokens, root, "rearRigType", &c.rearRigType );

	int wishbone = FindObjectValue( json, tokens, root, "wishbone" );
	if ( wishbone >= 0 )
	{
		ReadFloat( json, tokens, wishbone, "uprightHalfHeight", &c.wishbone.uprightHalfHeight );
		ReadFloat( json, tokens, wishbone, "kingpinOffset", &c.wishbone.kingpinOffset );
		ReadFloat( json, tokens, wishbone, "casterDeg", &c.wishbone.casterDeg );
		ReadFloat( json, tokens, wishbone, "kingpinInclinationDeg", &c.wishbone.kingpinInclinationDeg );
		ReadFloat( json, tokens, wishbone, "upperArmLength", &c.wishbone.upperArmLength );
		ReadFloat( json, tokens, wishbone, "lowerArmLength", &c.wishbone.lowerArmLength );
		ReadFloat( json, tokens, wishbone, "armHalfSpread", &c.wishbone.armHalfSpread );
		ReadFloat( json, tokens, wishbone, "steeringArmBack", &c.wishbone.steeringArmBack );
		ReadBool( json, tokens, wishbone, "ackermannTrapezoid", &c.wishbone.ackermannTrapezoid );
		ReadFloat( json, tokens, wishbone, "ackermannFraction", &c.wishbone.ackermannFraction );
		ReadFloat( json, tokens, wishbone, "coiloverTopHeight", &c.wishbone.coiloverTopHeight );
		ReadFloat( json, tokens, wishbone, "coiloverTopInboard", &c.wishbone.coiloverTopInboard );
		ReadFloat( json, tokens, wishbone, "restArmDroopDeg", &c.wishbone.restArmDroopDeg );
	}

	int trailingArm = FindObjectValue( json, tokens, root, "trailingArm" );
	if ( trailingArm >= 0 )
	{
		ReadVec3( json, tokens, trailingArm, "pivotOffset", &c.trailingArm.pivotOffset );
		ReadVec3( json, tokens, trailingArm, "damperArmOffset", &c.trailingArm.damperArmOffset );
		ReadVec3( json, tokens, trailingArm, "damperChassisOffset", &c.trailingArm.damperChassisOffset );
		ReadFloat( json, tokens, trailingArm, "armMass", &c.trailingArm.armMass );
	}

	ReadFloat( json, tokens, root, "knuckleMass", &c.knuckleMass );
	ReadFloat( json, tokens, root, "armMass", &c.armMass );
	ReadFloat( json, tokens, root, "rackMass", &c.rackMass );
	ReadFloat( json, tokens, root, "rackHalfWidth", &c.rackHalfWidth );
	ReadFloat( json, tokens, root, "rackServoForce", &c.rackServoForce );
	ReadFloat( json, tokens, root, "rackServoSpeedGain", &c.rackServoSpeedGain );
	ReadFloat( json, tokens, root, "rackServoMaxSpeed", &c.rackServoMaxSpeed );

	int envelope = FindObjectValue( json, tokens, root, "wheelEnvelope" );
	if ( envelope >= 0 )
	{
		ReadInt( json, tokens, envelope, "mode", &c.wheelEnvelope.mode );
		ReadInt( json, tokens, envelope, "cylinderSides", &c.wheelEnvelope.cylinderSides );
		ReadInt( json, tokens, envelope, "unionLayerCount", &c.wheelEnvelope.unionLayerCount );
	}

	ReadFloat( json, tokens, root, "wheelDensity", &c.wheelDensity );
	ReadFloat( json, tokens, root, "wheelFriction", &c.wheelFriction );
	ReadFloat( json, tokens, root, "wheelRollingResistance", &c.wheelRollingResistance );

	ReadFloat( json, tokens, root, "suspensionHertz", &c.suspensionHertz );
	ReadFloat( json, tokens, root, "suspensionDampingRatio", &c.suspensionDampingRatio );
	ReadFloat( json, tokens, root, "frontSuspensionScale", &c.frontSuspensionScale );
	ReadFloat( json, tokens, root, "rearSuspensionScale", &c.rearSuspensionScale );
	ReadFloat( json, tokens, root, "reboundTravel", &c.reboundTravel );
	ReadFloat( json, tokens, root, "compressionTravel", &c.compressionTravel );
	ReadFloat( json, tokens, root, "suspensionPreloadFront", &c.suspensionPreloadFront );
	ReadFloat( json, tokens, root, "suspensionPreloadRear", &c.suspensionPreloadRear );
	{
		// Backward compat: pre-P3 files (session/presets saved before the
		// front/rear split) only have a single "suspensionPreload" key, applied
		// equally to both axles. Only takes effect if that legacy key is present
		// - the front/rear reads above already handle new-format files.
		int legacyIndex = FindObjectValue( json, tokens, root, "suspensionPreload" );
		if ( legacyIndex >= 0 )
		{
			float legacyPreload = c.suspensionPreloadFront;
			TokenFloat( json, tokens[legacyIndex], &legacyPreload );
			c.suspensionPreloadFront = legacyPreload;
			c.suspensionPreloadRear = legacyPreload;
		}
	}

	ReadFloat( json, tokens, root, "arbFrontStiffness", &c.arbFrontStiffness );
	ReadFloat( json, tokens, root, "arbRearStiffness", &c.arbRearStiffness );

	ReadFloat( json, tokens, root, "aeroDragArea", &c.aeroDragArea );

	ReadFloat( json, tokens, root, "maxDriveSpeed", &c.maxDriveSpeed );
	ReadFloat( json, tokens, root, "maxDriveTorque", &c.maxDriveTorque );
	ReadFloat( json, tokens, root, "driveTaperStart", &c.driveTaperStart );
	ReadFloat( json, tokens, root, "brakeTorque", &c.brakeTorque );
	ReadFloat( json, tokens, root, "coastTorque", &c.coastTorque );
	ReadBool( json, tokens, root, "allWheelDrive", &c.allWheelDrive );

	ReadFloat( json, tokens, root, "maxSteeringAngleDegrees", &c.maxSteeringAngleDegrees );
	ReadFloat( json, tokens, root, "steeringHertz", &c.steeringHertz );
	ReadFloat( json, tokens, root, "steeringDampingRatio", &c.steeringDampingRatio );
	ReadFloat( json, tokens, root, "maxSteeringTorque", &c.maxSteeringTorque );
	ReadFloat( json, tokens, root, "rackFrictionForce", &c.rackFrictionForce );
	ReadFloat( json, tokens, root, "steeringFrictionTorque", &c.steeringFrictionTorque );
	ReadFloat( json, tokens, root, "steerInputDeadzone", &c.steerInputDeadzone );
	ReadBool( json, tokens, root, "ackermannGeometry", &c.ackermannGeometry );
	ReadFloat( json, tokens, root, "strutCasterDeg", &c.strutCasterDeg );

	ReadBool( json, tokens, root, "uprightAssist", &c.uprightAssist );
	ReadFloat( json, tokens, root, "uprightHertz", &c.uprightHertz );
	ReadFloat( json, tokens, root, "uprightDampingRatio", &c.uprightDampingRatio );

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
