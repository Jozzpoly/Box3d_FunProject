// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_m6_config_io.h"
#include "jozz_vehicle_m6_suspension_rig.h"
#include "jozz_vehicle_m7_suspension_import.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

constexpr const char* kWheelAsset = "Offroad_Big_Wheels.gltf";
constexpr const char* kTrailingContract = "one_sided_wheel_mount.asset.json";

std::string ReadText( const std::filesystem::path& path )
{
	std::ifstream file( path, std::ios::binary );
	if ( file.is_open() == false )
	{
		throw std::runtime_error( "cannot read " + path.string() );
	}
	std::ostringstream out;
	out << file.rdbuf();
	return out.str();
}

std::string EscapeJson( const std::string& value )
{
	std::ostringstream out;
	for ( unsigned char c : value )
	{
		switch ( c )
		{
			case '\\': out << "\\\\"; break;
			case '"': out << "\\\""; break;
			case '\n': out << "\\n"; break;
			case '\r': out << "\\r"; break;
			case '\t': out << "\\t"; break;
			default:
				if ( c < 0x20 )
				{
					out << "\\u" << std::hex << std::setw( 4 ) << std::setfill( '0' ) << (int)c << std::dec;
				}
				else
				{
					out << c;
				}
		}
	}
	return out.str();
}

bool HasReportMarker( const JozzVehicleAuditMetadata& metadata, const char* asset, const char* marker )
{
	b3Vec3 ignored = b3Vec3_zero;
	return FindJozzVehicleSemanticPoint( metadata, asset, marker, &ignored );
}

void RequireFinitePositive( float value, const char* label )
{
	if ( std::isfinite( value ) == false || value <= 0.0f )
	{
		throw std::runtime_error( std::string( label ) + " must be finite and positive" );
	}
}

void RecomputeDerivedSteering( JozzVehicleM6Config* config )
{
	float wheelbase = 2.0f * config->axleHalfSpacing;
	float track = 2.0f * config->trackHalfWidth;
	float steerRadians = config->maxSteeringAngleDegrees * B3_PI / 180.0f;
	config->rackTravel = std::fabs( ComputeJozzVehicleM6RackStroke(
		config->wishbone, wheelbase, track, config->rackHalfWidth, steerRadians ) );
}

} // namespace

int main( int argc, char** argv )
{
	try
	{
		if ( argc != 2 )
		{
			std::cerr << "usage: jozz_vehicle_factory_receipt <output-payload.json>\n";
			return 2;
		}

		const std::filesystem::path outputPath = argv[1];
		std::filesystem::create_directories( outputPath.parent_path() );
		const std::filesystem::path temporaryDirectory = outputPath.parent_path() / ".factory-receipt-tmp";
		std::filesystem::create_directories( temporaryDirectory );
		const std::filesystem::path factoryConfigPath = temporaryDirectory / "factory-config.json";
		const std::filesystem::path sanitizedConfigPath = temporaryDirectory / "sanitized-config.json";

		JozzVehicleAuditMetadata metadata = LoadJozzVehicleAuditMetadata();
		JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( metadata );
		RequireFinitePositive( defaults.wheelRadius, "wheel radius" );
		RequireFinitePositive( defaults.wheelWidth, "wheel width" );
		RequireFinitePositive( defaults.assetSuspensionTravelHint, "asset suspension travel hint" );

		JozzVehicleM6Config factory = JozzVehicleM6DefaultConfig(
			defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		JozzVehicleM7TrailingArmImport trailing = LoadJozzVehicleM7TrailingArmGeometry( kTrailingContract );
		factory.trailingArm = trailing.geometry;
		factory.frontRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
		factory.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
		RecomputeDerivedSteering( &factory );

		JozzVehicleM6Config sanitized = factory;
		bool sanitizerChanged = SanitizeJozzVehicleM6Config( &sanitized );
		RecomputeDerivedSteering( &sanitized );

		if ( SaveJozzVehicleM6Config( factory, factoryConfigPath.string() ) == false ||
			 SaveJozzVehicleM6Config( sanitized, sanitizedConfigPath.string() ) == false )
		{
			throw std::runtime_error( "native config writer failed" );
		}

		const bool wheelMountFromReport = HasReportMarker( metadata, kWheelAsset, "Socket_WheelMount" );
		const bool radiusFromReport = HasReportMarker( metadata, kWheelAsset, "Marker_TireRadiusOuter" );
		const bool widthLeftFromReport = HasReportMarker( metadata, kWheelAsset, "Marker_TireWidthLeft" );
		const bool widthRightFromReport = HasReportMarker( metadata, kWheelAsset, "Marker_TireWidthRight" );
		const bool travelTopFromReport =
			HasReportMarker( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Top" );
		const bool travelBottomFromReport =
			HasReportMarker( metadata, "One_Sided_wheel_mount.gltf", "Axis_SuspensionTravel_Bottom" );
		const bool wheelDimensionsFallbackUsed =
			!( wheelMountFromReport && radiusFromReport && widthLeftFromReport && widthRightFromReport );
		const bool travelHintFallbackUsed = !( travelTopFromReport && travelBottomFromReport );

		const float wheelbase = 2.0f * factory.axleHalfSpacing;
		const float track = 2.0f * factory.trackHalfWidth;
		const float steeringDeadPoint = ComputeJozzVehicleM6SteeringDeadPointDeg(
			factory.wishbone, wheelbase, track, factory.rackHalfWidth );
		const int minimumTorusSegments = JozzVehicleM6MinTorusSegments( &factory.wheelEnvelope );

		std::ofstream output( outputPath, std::ios::binary | std::ios::trunc );
		if ( output.is_open() == false )
		{
			throw std::runtime_error( "cannot open output " + outputPath.string() );
		}
		output << std::setprecision( 9 );
		output << "{\n";
		output << "  \"format\": \"jv-web-factory-payload\",\n";
		output << "  \"schemaVersion\": 1,\n";
		output << "  \"fieldSource\": \"SaveJozzVehicleM6Config/JozzFieldDesc\",\n";
		output << "  \"factoryComposition\": [\n";
		output << "    \"LoadJozzVehicleAuditMetadata\",\n";
		output << "    \"GetJozzVehicleM3ADefaults\",\n";
		output << "    \"JozzVehicleM6DefaultConfig\",\n";
		output << "    \"LoadJozzVehicleM7TrailingArmGeometry\",\n";
		output << "    \"frontRigType=DOUBLE_WISHBONE\",\n";
		output << "    \"rearRigType=DOUBLE_WISHBONE\"\n";
		output << "  ],\n";
		output << "  \"factoryConfig\": " << ReadText( factoryConfigPath ) << ",\n";
		output << "  \"sanitizedConfig\": " << ReadText( sanitizedConfigPath ) << ",\n";
		output << "  \"sanitizerChanged\": " << ( sanitizerChanged ? "true" : "false" ) << ",\n";
		output << "  \"derived\": {\n";
		output << "    \"rackTravel\": " << factory.rackTravel << ",\n";
		output << "    \"steeringDeadPointDegrees\": " << steeringDeadPoint << ",\n";
		output << "    \"wheelRadius\": " << factory.wheelEnvelope.radius << ",\n";
		output << "    \"wheelWidth\": " << factory.wheelEnvelope.width << ",\n";
		output << "    \"terrainCategoryBitsHex\": \"0x" << std::hex
			   << factory.wheelEnvelope.terrainCategoryBits << std::dec << "\",\n";
		output << "    \"minimumTorusSegments\": " << minimumTorusSegments << "\n";
		output << "  },\n";
		output << "  \"runtimeOnly\": {\n";
		output << "    \"filterGroupIndex\": " << factory.filterGroupIndex << "\n";
		output << "  },\n";
		output << "  \"solverProfile\": {\n";
		output << "    \"gravity\": [0, -10, 0],\n";
		output << "    \"fixedDt\": 0.016666666666666666,\n";
		output << "    \"substeps\": 4,\n";
		output << "    \"contactHertz\": 30,\n";
		output << "    \"contactDampingRatio\": 10,\n";
		output << "    \"contactSpeed\": 3,\n";
		output << "    \"enableContinuous\": false,\n";
		output << "    \"workerCount\": 0\n";
		output << "  },\n";
		output << "  \"features\": {\n";
		output << "    \"supportedRigTypes\": [0, 1, 2],\n";
		output << "    \"supportedWheelEnvelopeModes\": [0, 1, 2, 3, 4, 5],\n";
		output << "    \"activeFrontRigType\": " << factory.frontRigType << ",\n";
		output << "    \"activeRearRigType\": " << factory.rearRigType << ",\n";
		output << "    \"activeWheelEnvelopeMode\": " << factory.wheelEnvelope.mode << ",\n";
		output << "    \"rackCenteringAssistEnabled\": "
			   << ( factory.rackCenteringHertz > 0.0f ? "true" : "false" ) << ",\n";
		output << "    \"uprightAssistEnabled\": " << ( factory.uprightAssist ? "true" : "false" ) << "\n";
		output << "  },\n";
		output << "  \"assetResolution\": {\n";
		output << "    \"metadataLoadedFromRuntimeReport\": "
			   << ( metadata.loadedFromRuntimeReport ? "true" : "false" ) << ",\n";
		output << "    \"metadataSourcePath\": \"" << EscapeJson( metadata.sourcePath ) << "\",\n";
		output << "    \"metadataStatus\": \"" << EscapeJson( metadata.status ) << "\",\n";
		output << "    \"metersPerBlockbenchUnit\": " << defaults.metersPerBlockbenchUnit << ",\n";
		output << "    \"wheelRadius\": " << defaults.wheelRadius << ",\n";
		output << "    \"wheelWidth\": " << defaults.wheelWidth << ",\n";
		output << "    \"assetSuspensionTravelHint\": " << defaults.assetSuspensionTravelHint << ",\n";
		output << "    \"wheelDimensionsFallbackUsed\": "
			   << ( wheelDimensionsFallbackUsed ? "true" : "false" ) << ",\n";
		output << "    \"travelHintFallbackUsed\": " << ( travelHintFallbackUsed ? "true" : "false" ) << ",\n";
		output << "    \"trailingArmContractLoaded\": " << ( trailing.ok ? "true" : "false" ) << ",\n";
		output << "    \"trailingArmFallbackUsed\": " << ( trailing.ok ? "false" : "true" ) << ",\n";
		output << "    \"trailingArmStatus\": \"" << EscapeJson( trailing.status ) << "\"\n";
		output << "  }\n";
		output << "}\n";
		if ( output.good() == false )
		{
			throw std::runtime_error( "failed while writing payload" );
		}

		std::filesystem::remove_all( temporaryDirectory );
		std::cout << "factory receipt payload: " << outputPath.string() << "\n";
		std::cout << "wheel: radius=" << defaults.wheelRadius << " width=" << defaults.wheelWidth
				  << " fallback=" << wheelDimensionsFallbackUsed << "\n";
		std::cout << "trailing contract: ok=" << trailing.ok << " status=" << trailing.status << "\n";
		std::cout << "sanitizer changed factory: " << sanitizerChanged << "\n";
		return 0;
	}
	catch ( const std::exception& error )
	{
		std::cerr << "factory receipt failed: " << error.what() << "\n";
		return 1;
	}
}
