// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_rig_lab.h"

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "implot.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_m5_test_course.h"
#include "jozz_vehicle_m6_config_io.h"
#include "jozz_vehicle_m6_suspension_rig.h"
#include "jozz_vehicle_m7_suspension_import.h"
#include "jozz_vehicle_visual_mesh.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "box3d/box3d.h"

#include <cmath>

// M6/M7 Suspension Rig Lab: the multi-body suspension vehicle on the M7 real-
// forces foundation. The physics rig lives in jozz_vehicle_m6_suspension_rig
// (shared with the headless validation smoke); this sample adds input, camera,
// the M5 test course, live tuning behind a tabbed panel, hardpoint/link debug
// drawing, the visual-only glTF wheels, and Jozz's One_Sided_wheel_mount model
// riding the LIVE trailing-arm bodies. The M5 First Drivable sample stays
// untouched as the strut baseline.
//
// Session/preset files (JozzVehicleM6Config serialized via
// jozz_vehicle_m6_config_io): kSessionFilePath is an auto-save the destructor
// writes and the constructor restores, so restarting the sample (or the whole
// app) resumes the last tuning. kPresetDirectory holds named, user-saved
// presets (assets/vehicle_presets/*.json, three ship built-in) for switching
// between whole setups - drift/offroad/street - in two clicks instead of
// re-dragging every slider. build/ is gitignored so the session file never
// shows up as repo noise; the preset directory is committed on purpose.
static const char* kSessionFilePath = "build/jozz_vehicle_m6_session.json";
static const char* kPresetDirectory = "assets/vehicle_presets";

// Debug/view toggles (Debug tab checkboxes) are deliberately NOT part of
// JozzVehicleM6Config: they are not vehicle tuning, so they must not leak
// into named presets or get wiped by "Przywroc wszystkie ustawienia
// domyslne". But they were also not being restored across the engine's
// global "R" restart, which destroys and reconstructs this whole sample -
// the constructor hardcoded them back to fixed defaults every time. That
// meant every "R" silently re-enabled the rig diagnostic lines even after
// Jozz had turned them off. Small standalone key=value file, same
// build/-is-gitignored auto-save idea as kSessionFilePath, just a separate
// file so it stays out of the config JSON format entirely.
static const char* kDebugSessionFilePath = "build/jozz_vehicle_m6_debug_session.txt";

class JozzVehicleM6RigLab : public Sample
{
public:
	explicit JozzVehicleM6RigLab( SampleContext* context )
		: Sample( context )
	{
		m_camera->m_thirdPerson = false;
		if ( context->restart == false )
		{
			m_camera->SetView( -135.0f, 14.0f, 13.0f, { 0.0f, 1.2f, 0.0f } );
		}

		// Vehicle worlds run without continuous collision (M7 decision, see the
		// validator's CreateM6SmokeGround for the full story): above ~15 m/s
		// the wheels themselves become "fast" bodies and a rolling wheel's
		// ground sweep starts in contact, which trips the debug TOI validation
		// every time. No thin geometry exists in this course, so CCD buys
		// nothing. The previous value is restored when the sample closes; the
		// Solver panel checkbox stays functional for experiments.
		m_savedEnableContinuous = context->enableContinuous;
		context->enableContinuous = false;

		// Same footprint as AddGroundBox(120), but tagged with the terrain
		// category so the split wheel envelope can separate rolling contact
		// (sphere vs terrain) from side contact (true-width cylinder vs the
		// rest). Ramps/washboard/heightfield get the same tag; props do not.
		{
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.name = "ground";
			bodyDef.position = { 0.0f, -1.0f, 0.0f };
			m_groundId = b3CreateBody( m_worldId, &bodyDef );

			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.filter.categoryBits = JOZZ_M6_TERRAIN_CATEGORY;
			b3BoxHull hull = b3MakeBoxHull( 120.0f, 1.0f, 120.0f );
			b3ShapeId shapeId = b3CreateHullShape( m_groundId, &shapeDef, &hull.base );
			SetGroundShape( shapeId );
		}
		m_testCourse = CreateJozzVehicleM5TestCourse( m_worldId, 0.0f, JOZZ_M6_TERRAIN_CATEGORY );

		m_assetMetadata = LoadJozzVehicleAuditMetadata();
		JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_assetMetadata );
		m_metersPerBlockbenchUnit = defaults.metersPerBlockbenchUnit;
		m_config = JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

		// Trailing-arm geometry from the sidecar contract (first contract-to-
		// physics import); the helper falls back to the built-in geometry and
		// says so in its status string. The lab boots with Jozz's model LIVE
		// on the rear axle (wishbone front + trailing rear, the exact mix the
		// validator smokes); the Rig tab switches axle types freely.
		m_trailingImport = LoadJozzVehicleM7TrailingArmGeometry( "one_sided_wheel_mount.asset.json" );
		m_config.trailingArm = m_trailingImport.geometry;
		// All four corners are double wishbone so every corner carries a knuckle
		// (upright) body for Jozz's suspension model to ride. The hub part
		// follows the knuckle (travel + steer, no spin); the brackets and arm
		// stay on the chassis. The Rig tab still switches axle types freely.
		m_config.frontRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
		m_config.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;

		// Restore whatever tuning was in effect last time this lab ran, if any.
		// Without this, the engine's global "R" restart shortcut - and simply
		// closing and reopening samples.exe - silently discarded every dial back
		// to JozzVehicleM6DefaultConfig with no warning (found while auditing the
		// preset workflow: restarting to re-drop the car for a fresh test was
		// quietly destroying the tuning session). The destructor writes this
		// file on the way out, so it always reflects the last config in memory.
		if ( LoadJozzVehicleM6Config( kSessionFilePath, &m_config ) )
		{
			RecomputeRackTravel();
		}
		RefreshPresetList();

		m_showWheelVisuals = true;
		m_showMountVisuals = true;
		m_showPrimitiveWheelShapes = false;
		m_showRigDiagnostics = true;
		m_invertSteering = false;

		// Restore whatever debug/view toggles were in effect last time, for the
		// same reason the config load above exists: "R" restart must not
		// silently undo a choice Jozz made in the Debug tab.
		LoadDebugViewState();

		m_contactHertz = 30.0f;
		m_contactDampingRatio = 10.0f;
		m_contactSpeed = 3.0f;

		m_telemetryHead = 0;
		m_telemetryCount = 0;
		m_telemetryClock = 0.0f;

		m_vehicle = {};
		LoadWheelVisual();
		LoadMountVisual();
		CreateVehicle(); // sets up the per-corner mount rig, needs the visuals loaded
		SyncEditFromConfig();

		// Env overrides so a headless --screenshot can frame a corner and hide
		// the debug links, without any UI interaction.
		if ( const char* v = std::getenv( "JOZZ_M6_DIAG" ) )
		{
			m_showRigDiagnostics = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M6_WHEEL" ) )
		{
			m_showWheelVisuals = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M6_CAM" ) )
		{
			float y = -135, p = 14, r = 13, px = 0, py = 1.2f, pz = 0;
			if ( std::sscanf( v, "%f,%f,%f,%f,%f,%f", &y, &p, &r, &px, &py, &pz ) >= 3 )
			{
				m_camera->SetView( y, p, r, { px, py, pz } );
			}
		}
		if ( const char* v = std::getenv( "JOZZ_M6_DUMPER" ) )
		{
			m_showDumper = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M6_MOUNT" ) )
		{
			m_showMountVisuals = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M6_ARMTINT" ) )
		{
			m_armTint = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M6_DUMP" ) )
		{
			m_dumpGeometry = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M6_HERTZ" ) )
		{
			m_config.suspensionHertz = (float)atof( v );
			ApplySuspensionTuning();
			SyncEditFromConfig();
		}
		if ( const char* v = std::getenv( "JOZZ_M6_DAMP" ) )
		{
			m_config.suspensionDampingRatio = (float)atof( v );
			ApplySuspensionTuning();
			SyncEditFromConfig();
		}
		if ( const char* v = std::getenv( "JOZZ_M6_PRELOAD" ) )
		{
			m_config.suspensionPreload = (float)atof( v );
			ApplySuspensionTuning();
			SyncEditFromConfig();
		}
		if ( const char* v = std::getenv( "JOZZ_M6_DROOP" ) )
		{
			m_editWishbone.restArmDroopDeg = (float)atof( v );
			ApplyPendingStructuralSetup();
		}
		if ( const char* v = std::getenv( "JOZZ_M6_TAB" ) )
		{
			m_forceTabIndex = atoi( v );
		}
		if ( const char* v = std::getenv( "JOZZ_M6_PRESET" ) )
		{
			LoadPresetByName( v );
		}
		if ( std::getenv( "JOZZ_M6_TEST_RESET_MODAL" ) )
		{
			m_testOpenResetModal = true;
		}
	}

	~JozzVehicleM6RigLab() override
	{
		// See the matching restore in the constructor: this is what makes "R"
		// restart and reopening the app resume the last tuning instead of
		// wiping it.
		SaveJozzVehicleM6Config( m_config, kSessionFilePath );
		SaveDebugViewState();

		DestroyVehicle();
		m_wheelVisual.Destroy();
		m_riggedMountL.Destroy();
		m_riggedMountR.Destroy();
		m_dumper.Destroy();
		DestroyJozzVehicleM5TestCourse( &m_testCourse );
		m_context->enableContinuous = m_savedEnableContinuous;
	}

	// The tabbed panel hosts plots and wide sliders, and the Polish tab names
	// (Zawieszenie, Kierownica, ...) run wider than the English originals in
	// the proportional UI font - the default 20 em drawer is too narrow for
	// either.
	float InfoPanelWidthEm() const override
	{
		return 31.0f;
	}

	bool CondenseDebugOverlay() const override
	{
		return true;
	}

	// See kDebugSessionFilePath above for why this is a separate file from the
	// vehicle config: these are view toggles, not tuning, and must survive "R"
	// without ever being reset by a preset load or "restore defaults".
	void SaveDebugViewState()
	{
		std::error_code ec;
		std::filesystem::create_directories( std::filesystem::path( kDebugSessionFilePath ).parent_path(), ec );

		std::ofstream file( kDebugSessionFilePath, std::ios::binary | std::ios::trunc );
		if ( file.is_open() == false )
		{
			return;
		}
		file << "showWheelVisuals=" << ( m_showWheelVisuals ? 1 : 0 ) << "\n";
		file << "showMountVisuals=" << ( m_showMountVisuals ? 1 : 0 ) << "\n";
		file << "showPrimitiveWheelShapes=" << ( m_showPrimitiveWheelShapes ? 1 : 0 ) << "\n";
		file << "showRigDiagnostics=" << ( m_showRigDiagnostics ? 1 : 0 ) << "\n";
		file << "armTint=" << ( m_armTint ? 1 : 0 ) << "\n";
	}

	void LoadDebugViewState()
	{
		std::ifstream file( kDebugSessionFilePath );
		if ( file.is_open() == false )
		{
			return;
		}

		std::string line;
		while ( std::getline( file, line ) )
		{
			size_t eq = line.find( '=' );
			if ( eq == std::string::npos )
			{
				continue;
			}
			std::string key = line.substr( 0, eq );
			bool value = line.substr( eq + 1 ) == "1";
			if ( key == "showWheelVisuals" )
			{
				m_showWheelVisuals = value;
			}
			else if ( key == "showMountVisuals" )
			{
				m_showMountVisuals = value;
			}
			else if ( key == "showPrimitiveWheelShapes" )
			{
				m_showPrimitiveWheelShapes = value;
			}
			else if ( key == "showRigDiagnostics" )
			{
				m_showRigDiagnostics = value;
			}
			else if ( key == "armTint" )
			{
				m_armTint = value;
			}
		}
	}

	void SyncEditFromConfig()
	{
		m_editFrontRigType = m_config.frontRigType;
		m_editRearRigType = m_config.rearRigType;
		m_editWishbone = m_config.wishbone;
		m_editTrailingArm = m_config.trailingArm;
		m_editKnuckleMass = m_config.knuckleMass;
		m_editArmMass = m_config.armMass;
		m_editEnvelopeMode = m_config.wheelEnvelope.mode;
		m_editEnvelopeLayers = m_config.wheelEnvelope.unionLayerCount;
		m_editStrutCasterDeg = m_config.strutCasterDeg;
		m_editChassisHalfExtents = m_config.chassisHalfExtents;
		m_editChassisDensity = m_config.chassisDensity;
		m_editCgVerticalOffset = m_config.cgVerticalOffset;
		m_editAxleHalfSpacing = m_config.axleHalfSpacing;
		m_editTrackHalfWidth = m_config.trackHalfWidth;
		m_editRestDrop = m_config.restDrop;
		m_editWheelDensity = m_config.wheelDensity;
		m_structuralSetupDirty = false;
	}

	void ApplyPendingStructuralSetup()
	{
		m_config.frontRigType = m_editFrontRigType;
		m_config.rearRigType = m_editRearRigType;
		m_config.wishbone = m_editWishbone;
		m_config.trailingArm = m_editTrailingArm;
		m_config.knuckleMass = m_editKnuckleMass;
		m_config.armMass = m_editArmMass;
		m_config.wheelEnvelope.mode = m_editEnvelopeMode;
		m_config.wheelEnvelope.unionLayerCount = m_editEnvelopeLayers;
		m_config.strutCasterDeg = m_editStrutCasterDeg;
		m_config.chassisHalfExtents = m_editChassisHalfExtents;
		m_config.chassisDensity = m_editChassisDensity;
		m_config.cgVerticalOffset = m_editCgVerticalOffset;
		m_config.axleHalfSpacing = m_editAxleHalfSpacing;
		m_config.trackHalfWidth = m_editTrackHalfWidth;
		m_config.restDrop = m_editRestDrop;
		m_config.wheelDensity = m_editWheelDensity;
		CreateVehicle();
		m_structuralSetupDirty = false;
	}

	// rackTravel is derived from the steering geometry (kingpin offset, arm
	// back, Ackermann, track/wheelbase) rather than an independent tuning
	// value, so it is deliberately left out of the saved config - a preset or
	// session file only needs to describe what makes it different. Call this
	// after loading either so the rack limit always matches whatever geometry
	// came with it, even if a future preset changes steering hardpoints.
	void RecomputeRackTravel()
	{
		float maxAngle = m_config.maxSteeringAngleDegrees * B3_PI / 180.0f;
		m_config.rackTravel = ComputeJozzVehicleM6RackStroke( m_config.wishbone, 2.0f * m_config.axleHalfSpacing,
															   m_config.trackHalfWidth, m_config.rackHalfWidth, maxAngle );
	}

	void RefreshPresetList()
	{
		m_availablePresets = ListJozzVehicleM6Presets( kPresetDirectory );
		// Covers both directions: index fell out of range because the list
		// shrank, AND the starting sentinel (-1, "nothing selected yet") once
		// the list is non-empty - a plain ">=" check only caught the first one,
		// so the combo opened with nothing selected even with 3 presets sitting
		// right there (caught by inspecting the loaded list, not by eyeballing
		// the render - the blank combo was easy to miss in a screenshot).
		if ( m_selectedPresetIndex < 0 || m_selectedPresetIndex >= (int)m_availablePresets.size() )
		{
			m_selectedPresetIndex = m_availablePresets.empty() ? -1 : 0;
		}
	}

	// Loading a preset changes structural fields (chassis size, wishbone
	// geometry, ...) that normally sit behind the pending-edit + Apply pattern,
	// but making the user press Apply right after picking a preset would just
	// be one more click in the way of "switch setup and go" - so this commits
	// immediately, the same way "Przywroc wszystkie ustawienia domyslne" does.
	void LoadPresetByName( const std::string& name )
	{
		std::string path = std::string( kPresetDirectory ) + "/" + name + ".json";
		if ( LoadJozzVehicleM6Config( path, &m_config ) == false )
		{
			m_presetStatus = "Nie udało się wczytać presetu '" + name + "'.";
			return;
		}
		RecomputeRackTravel();
		CreateVehicle();
		SyncEditFromConfig();
		m_presetStatus = "Wczytano preset: " + name;
	}

	void SaveCurrentAsPreset( const std::string& name )
	{
		if ( name.empty() )
		{
			m_presetStatus = "Podaj nazwę presetu przed zapisem.";
			return;
		}
		std::string path = std::string( kPresetDirectory ) + "/" + name + ".json";
		if ( SaveJozzVehicleM6Config( m_config, path ) == false )
		{
			m_presetStatus = "Nie udało się zapisać presetu '" + name + "'.";
			return;
		}
		m_presetStatus = "Zapisano preset: " + name;
		RefreshPresetList();
		for ( size_t i = 0; i < m_availablePresets.size(); ++i )
		{
			if ( m_availablePresets[i] == name )
			{
				m_selectedPresetIndex = (int)i;
				break;
			}
		}
	}

	void LoadWheelVisual()
	{
		m_wheelVisual.Destroy();

		std::string resolvedPath;
		if ( FindJozzVehicleAssetFile( "assets/source/Offroad_Big_Wheels.gltf", &resolvedPath ) )
		{
			m_wheelVisual.LoadStaticGltf( resolvedPath.c_str(), m_metersPerBlockbenchUnit );
		}

		m_wheelVisualCorrection =
			ComputeJozzVehicleWheelVisualCorrection( m_wheelVisual, m_assetMetadata, m_metersPerBlockbenchUnit );
	}

	// Jozz's suspension model, rigged per bone onto the LIVE corner bodies: the
	// WheelCenter hub part follows the knuckle (upright: travel + steer, no
	// spin), the chassis brackets and the arm stay on the chassis. Left corners
	// use the model as authored (yaw -90); right corners use the mirrored copy
	// (same yaw) so the asymmetric one-sided model reflects across the car
	// centreline. Per-corner local transforms are baked once in SetupMountRig.
	void LoadMountVisual()
	{
		m_riggedMountL.Destroy();
		m_riggedMountR.Destroy();

		m_mountContract = LoadJozzVehicleAssetContract( "one_sided_wheel_mount.asset.json" );
		float mpbu = m_mountContract.metersPerBlockbenchUnit > 0.0f ? m_mountContract.metersPerBlockbenchUnit
																	 : m_metersPerBlockbenchUnit;
		std::string mountPath = m_mountContract.sourcePath;
		if ( mountPath.empty() )
		{
			FindJozzVehicleAssetFile( "assets/source/One_Sided_wheel_mount.gltf", &mountPath );
		}
		if ( mountPath.empty() == false )
		{
			m_riggedMountL.LoadSkinnedGltf( mountPath.c_str(), mpbu, false );
			m_riggedMountR.LoadSkinnedGltf( mountPath.c_str(), mpbu, true );
		}

		std::string dumperPath;
		if ( FindJozzVehicleAssetFile( "assets/source/Asset_Dumper.gltf", &dumperPath ) )
		{
			m_dumper.LoadSkinnedGltf( dumperPath.c_str(), m_metersPerBlockbenchUnit );
		}

		m_mountWheelCenterAuthored = { -0.416f, 0.175f, 0.0f };
		const JozzVehicleContractBinding* wc =
			FindJozzVehicleContractBindingByRole( m_mountContract, "suspension.visual.wheel_center" );
		if ( wc != nullptr && wc->resolved )
		{
			m_mountWheelCenterAuthored = wc->positionMeters;
		}

		// Damper sockets: two coilovers per corner, straddling the arm (see the
		// contract - DamperUpper/Lower _L and _R differ only in Z, not X). They
		// are "visual_endpoint"/physicsAuthority:false markers, same status as
		// wheel_center above, so they get the exact same treatment: read once
		// here in the authored (left/unmirrored) frame, mirrored per corner in
		// SetupMountRig-adjacent draw code by negating X only - Z is untouched
		// because the L/R damper pair is not the car's left/right side.
		// Fallbacks below are the shipped One_Sided_wheel_mount.gltf values
		// (BU * 0.35 m/BU) in case the contract fails to load.
		auto readDamperSocket = [this]( const char* role, b3Vec3 fallback ) {
			const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( m_mountContract, role );
			return ( binding != nullptr && binding->resolved ) ? binding->positionMeters : fallback;
		};
		m_damperUpperLAuthored =
			readDamperSocket( "suspension.visual.damper_upper_l", { 0.0164f, 0.6453f, 0.2844f } );
		m_damperUpperRAuthored =
			readDamperSocket( "suspension.visual.damper_upper_r", { 0.0164f, 0.6453f, -0.2844f } );
		m_damperLowerLAuthored =
			readDamperSocket( "suspension.visual.damper_lower_l", { -0.2516f, 0.0109f, 0.2844f } );
		m_damperLowerRAuthored =
			readDamperSocket( "suspension.visual.damper_lower_r", { -0.2516f, 0.0109f, -0.2844f } );
	}

	static bool CornerIsLeft( int corner )
	{
		return corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_REAR_LEFT;
	}

	// Chassis-end and wheel-end of a wishbone-arm part along its authored X axis.
	// The wheel end is the X extreme nearer the wheel centre: authored -X for a
	// left (unmirrored) mesh, +X for a right (mirrored) mesh.
	static void ArmEnds( const JozzVehicleRiggedPart& part, bool wheelNegX, b3Vec3& chassisEnd, b3Vec3& wheelEnd )
	{
		float y = 0.5f * ( part.restMin.y + part.restMax.y );
		float z = 0.5f * ( part.restMin.z + part.restMax.z );
		chassisEnd = { wheelNegX ? part.restMax.x : part.restMin.x, y, z };
		wheelEnd = { wheelNegX ? part.restMin.x : part.restMax.x, y, z };
	}

	// Bake each corner's model placement into two body-local transforms: the
	// brackets/arm relative to the chassis, the hub relative to the knuckle.
	// Live, the parts follow those bodies, so the model articulates exactly as
	// the physics does. Called after CreateJozzVehicleM6, when bodies sit at
	// rest (no spin/steer yet).
	void SetupMountRig()
	{
		b3Vec3 mountBU = JozzVehicleFindPointOrBuiltIn( m_assetMetadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
		b3WorldTransform chassisRest = b3Body_GetTransform( m_vehicle.chassisId );

		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			m_cornerHasMount[corner] = runtime.rigType == JOZZ_M6_RIG_DOUBLE_WISHBONE && B3_IS_NON_NULL( runtime.knuckleId );
			if ( m_cornerHasMount[corner] == false )
			{
				continue;
			}

			bool isLeft = CornerIsLeft( corner );

			// Attach point: the wheel's Socket_WheelMount (inboard hub face). The
			// offset from the wheel centre is along the axle; reflect it for the
			// right side so the model attaches inboard on both.
			b3WorldTransform wheelDraw = b3MulWorldTransforms( b3Body_GetTransform( runtime.wheelId ), m_wheelVisualCorrection );
			b3Pos wheelCentre = b3Body_GetPosition( runtime.wheelId );
			b3Pos mountWorld = b3TransformWorldPoint( wheelDraw, b3MulSV( m_metersPerBlockbenchUnit, mountBU ) );
			b3Vec3 offset = b3Sub( mountWorld, wheelCentre );
			if ( isLeft == false )
			{
				offset.z = -offset.z;
			}
			b3Pos attach = b3OffsetPos( wheelCentre, offset );

			// Model wheel-centre socket (mirrored copy has authored X negated).
			b3Vec3 pwc = m_mountWheelCenterAuthored;
			if ( isLeft == false )
			{
				pwc.x = -pwc.x;
			}
			b3Vec3 rp = b3RotateVector( yaw, pwc );

			b3WorldTransform placementRest;
			placementRest.q = yaw;
			placementRest.p = { attach.x - rp.x, attach.y - rp.y, attach.z - rp.z };

			m_bracketLocal[corner] = b3InvMulWorldTransforms( chassisRest, placementRest );
			m_hubLocal[corner] = b3InvMulWorldTransforms( b3Body_GetTransform( runtime.knuckleId ), placementRest );
		}
	}

	float GetSpawnHeight() const
	{
		return m_config.restDrop + m_config.wheelEnvelope.radius + 0.05f;
	}

	void CreateVehicle()
	{
		DestroyVehicle();
		m_vehicle = CreateJozzVehicleM6( m_worldId, m_groundId, m_config, { 0.0f, GetSpawnHeight(), 0.0f } );
		UpdateWheelShapeVisibility();
		SetupMountRig();
	}

	// "Reset swiat" - a full simulation restart scoped to this running sample
	// object, so it can never touch m_config or the Debug-tab toggles (there is
	// nothing to reload them from; they simply aren't part of what this resets).
	// This is the button form of what "R" does at the process level; it exists
	// because "R" goes through the engine's global restart (destroy + rebuild
	// the whole sample), which is what was silently reviving the hardcoded
	// Debug defaults before LoadDebugViewState() existed.
	void ResetWorld()
	{
		CreateVehicle();
		ResetJozzVehicleM5TestCourseProps( m_testCourse );
		m_telemetryHead = 0;
		m_telemetryCount = 0;
		m_telemetryClock = 0.0f;
	}

	void DestroyVehicle()
	{
		if ( m_vehicle.valid )
		{
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
				for ( int i = 0; i < runtime.wheelShapeCount; ++i )
				{
					SetShapeHidden( runtime.wheelShapeIds[i], false );
				}
			}
		}

		DestroyJozzVehicleM6( &m_vehicle );
	}

	void UpdateWheelShapeVisibility()
	{
		if ( m_vehicle.valid == false )
		{
			return;
		}

		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			for ( int i = 0; i < runtime.wheelShapeCount; ++i )
			{
				SetShapeHidden( runtime.wheelShapeIds[i], m_showPrimitiveWheelShapes == false );
			}
		}
	}

	void ApplySuspensionTuning()
	{
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			bool isFront = corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
			float scale = isFront ? m_config.frontSuspensionScale : m_config.rearSuspensionScale;

			if ( runtime.rigType == JOZZ_M6_RIG_INTEGRATED_STRUT )
			{
				b3WheelJoint_SetSuspensionHertz( runtime.strutJointId, m_config.suspensionHertz * scale );
				b3WheelJoint_SetSuspensionDampingRatio( runtime.strutJointId, m_config.suspensionDampingRatio * scale );
				b3Joint_WakeBodies( runtime.strutJointId );
			}
			else
			{
				// Trailing corners carry the effective-mass compensation the
				// corner was built with (1.0 on wishbone corners).
				float hertzScale = runtime.rigType == JOZZ_M6_RIG_TRAILING_ARM ? runtime.trailingCoiloverHertzScale : 1.0f;
				b3DistanceJoint_SetSpringHertz( runtime.coiloverJointId, m_config.suspensionHertz * scale * hertzScale );
				b3DistanceJoint_SetSpringDampingRatio( runtime.coiloverJointId, m_config.suspensionDampingRatio * scale );

				// Ride height (preload) and the bump/droop travel stops are pushed
				// onto the live joint from the cached design length, so dragging
				// these sliders adjusts the standing pose and travel range without
				// a full vehicle rebuild (unlike the arm droop angle, which bakes
				// into the hardpoints and does need one). Trailing corners map the
				// wheel-space preload/travel through their motion ratio so it means
				// the same thing at the wheel as it does on a wishbone corner.
				float motionRatio = runtime.rigType == JOZZ_M6_RIG_TRAILING_ARM ? runtime.trailingMotionRatio : 1.0f;
				float design = runtime.coiloverDesignLength;
				b3DistanceJoint_SetLength( runtime.coiloverJointId, design + m_config.suspensionPreload * scale * motionRatio );
				b3DistanceJoint_SetLengthRange( runtime.coiloverJointId,
												 b3MaxFloat( 0.05f, design - m_config.compressionTravel * motionRatio ),
												 design + m_config.reboundTravel * motionRatio );
				b3Joint_WakeBodies( runtime.coiloverJointId );
			}
		}
	}

	void ApplySteeringTuning()
	{
		if ( B3_IS_NON_NULL( m_vehicle.rackJointId ) )
		{
			b3PrismaticJoint_SetSpringHertz( m_vehicle.rackJointId, m_config.steeringHertz );
			b3PrismaticJoint_SetSpringDampingRatio( m_vehicle.rackJointId, m_config.steeringDampingRatio );
			b3Joint_WakeBodies( m_vehicle.rackJointId );
		}

		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			bool isFront = corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
			if ( isFront && runtime.rigType == JOZZ_M6_RIG_INTEGRATED_STRUT )
			{
				float maxAngle = m_config.maxSteeringAngleDegrees * B3_PI / 180.0f;
				b3WheelJoint_SetSteeringLimits( runtime.strutJointId, -maxAngle, maxAngle );
				b3WheelJoint_SetSteeringHertz( runtime.strutJointId, m_config.steeringHertz );
				b3WheelJoint_SetSteeringDampingRatio( runtime.strutJointId, m_config.steeringDampingRatio );
				b3WheelJoint_SetMaxSteeringTorque( runtime.strutJointId, m_config.maxSteeringTorque );
				b3Joint_WakeBodies( runtime.strutJointId );
			}
		}
	}

	void ApplyWheelFriction()
	{
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			for ( int i = 0; i < runtime.wheelShapeCount; ++i )
			{
				b3Shape_SetFriction( runtime.wheelShapeIds[i], m_config.wheelFriction );
			}
		}
	}

	void ApplyContactTuning()
	{
		b3World_SetContactTuning( m_worldId, m_contactHertz, m_contactDampingRatio, m_contactSpeed );
	}

	void Keyboard( int key, int action, int modifiers ) override
	{
		if ( key == KEY_T && action == ACTION_PRESS )
		{
			ToggleThirdPerson();
		}

		Sample::Keyboard( key, action, modifiers );
	}

	// ---- Tabbed control panel. Live sliders act immediately; anything that
	// rebuilds bodies goes through the pending-edit + Apply pattern, and the
	// Apply bar at the bottom stays visible from every tab.
	//
	// UX conventions used throughout these tabs:
	//  - One short line of context per group, not a paragraph. Deeper "why /
	//    when / what breaks" explanations live behind a "(?)" HelpMarker so the
	//    panel stays scannable but nothing is hidden.
	//  - Slider ranges are cropped tight around a sensible vehicle span (drift
	//    car to light truck) so dragging can actually hit a precise value.
	//    Ctrl+click any slider to type an exact number outside that range.
	//  - SectionHeader marks a logical group; CollapsingHeader (closed by
	//    default) hides advanced hardpoint geometry most users never touch.

	// Small "(?)" marker that shows a tooltip on hover - the standard Dear
	// ImGui pattern for keeping detail out of the main flow without hiding it.
	static void HelpMarker( const char* text )
	{
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos( ImGui::GetFontSize() * 28.0f );
			ImGui::TextUnformatted( text );
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	static void SectionHeader( const char* title )
	{
		ImGui::Spacing();
		ImGui::TextColored( ImVec4( 0.45f, 0.75f, 1.0f, 1.0f ), "%s", title );
		ImGui::Separator();
	}

	void DrawDriveTab()
	{
		ImGui::TextWrapped( "Silnik trzyma limit obrotów, gaz skaluje moment." );
		HelpMarker( "To, czy koło się trzyma czy traci przyczepność (pali gumę), zależy od momentu względem "
					"przyczepności w kontakcie z podłożem - nie ma tu osobnego 'przełącznika poślizgu'." );
		ImGui::SliderFloat( "Moment napędowy", &m_config.maxDriveTorque, 0.0f, 2000.0f, "%.0f N*m" );
		ImGui::SliderFloat( "Limit obrotów", &m_config.maxDriveSpeed, 5.0f, 100.0f, "%.0f rad/s" );
		ImGui::SliderFloat( "Próg spadku momentu", &m_config.driveTaperStart, 0.2f, 0.95f, "%.2f x obr." );
		HelpMarker( "Od jakiej części limitu obrotów moment zaczyna maleć w stronę zera - symuluje silnik "
					"dochodzący do czerwonego pola." );
		ImGui::SliderFloat( "Moment hamowania", &m_config.brakeTorque, 0.0f, 2500.0f, "%.0f N*m" );
		ImGui::SliderFloat( "Moment na biegu jałowym", &m_config.coastTorque, 0.0f, 40.0f, "%.0f N*m" );
		HelpMarker( "Lekki opór silnika, gdy nie dotykasz gazu ani hamulca - jak puszczenie sprzęgła bez gazu." );
		ImGui::Checkbox( "Napęd na wszystkie koła", &m_config.allWheelDrive );
		ImGui::Separator();
		ImGui::SliderFloat( "Opór aerodynamiczny", &m_config.aeroDragArea, 0.2f, 2.0f, "%.2f m^2" );
		HelpMarker( "Opór powietrza rosnący z kwadratem prędkości. To ON ogranicza prędkość maksymalną, nie sztywny "
					"limit - większa wartość = niższy V-max." );
	}

	void DrawSteeringTab()
	{
		ImGui::TextWrapped( "Puszczona kierownica sama wraca do środka dzięki fizyce, nie skryptowi." );
		HelpMarker( "Trzymanie A/D włącza sprężynę zębatki + serwo (wspomaganie). Puszczenie zostawia tylko tarcie "
					"zębatki, więc geometria zwrotnicy i siły z kontaktu z podłożem same kierują kołami - kontra w "
					"poślizgu i prostowanie na wyjściu z zakrętu wynikają z sił, nie ze skryptu." );
		ImGui::Checkbox( "Odwróć kierowanie (preferencja)", &m_invertSteering );
		ImGui::Separator();
		if ( ImGui::SliderFloat( "Sztywność kierownicy", &m_config.steeringHertz, 2.0f, 25.0f, "%.1f Hz" ) )
		{
			ApplySteeringTuning();
		}
		HelpMarker( "Jak szybko zębatka goni zadany kąt skrętu, gdy trzymasz A/D. Wyżej = ostrzejsza, bardziej "
					"'gokartowa' reakcja." );
		if ( ImGui::SliderFloat( "Tłumienie kierownicy", &m_config.steeringDampingRatio, 0.2f, 3.0f, "%.2f" ) )
		{
			ApplySteeringTuning();
		}
		// Live: the drive update pushes these to the joints every step.
		ImGui::SliderFloat( "Siła wspomagania", &m_config.rackServoForce, 0.0f, 20000.0f, "%.0f N" );
		HelpMarker( "Ile siły ma wspomaganie, gdy trzymasz kierownicę - musi pokonać moment parkingowy obciążonej "
					"opony (~700 N*m na koło), inaczej auto 'nie posłucha' przy postoju." );
		ImGui::SliderFloat( "Tarcie zębatki (ręce puszczone)", &m_config.rackFrictionForce, 0.0f, 1000.0f, "%.0f N" );
		HelpMarker( "Opór, jaki musi pokonać sam ślad koła (caster), żeby wykręcić kierownicę za Ciebie w poślizgu. "
					"Mniej = żywszy, szybszy powrót do środka. Więcej = spokojniej, aż w końcu kierownica przestaje "
					"się ruszać sama." );
		ImGui::SliderFloat( "Tarcie skrętu kolumny", &m_config.steeringFrictionTorque, 0.0f, 200.0f, "%.0f N*m" );
		HelpMarker( "To samo co tarcie zębatki, ale dla osi na kolumnie McPhersona zamiast wahaczy." );
	}

	// The single "Zawieszenie" tab, front to back: what kind of suspension ->
	// where it sits and how far it moves (the thing people actually came here
	// for) -> spring feel -> anti-roll -> advanced hardpoint geometry, folded
	// away by default because most users never need it.
	void DrawSuspensionTab()
	{
		bool edited = false; // any control here that needs "Apply rig rebuild"

		SectionHeader( "Typ zawieszenia" );
		const char* rigTypes[] = { "Kolumna (prosta, tania - McPherson)", "Podwójny wahacz (widoczne ramiona)",
								   "Wahacz wleczony (model Jozza)" };
		edited |= ImGui::Combo( "Przednia oś", &m_editFrontRigType, rigTypes, 3 );
		edited |= ImGui::Combo( "Tylna oś", &m_editRearRigType, rigTypes, 3 );
		if ( m_editFrontRigType == JOZZ_M6_RIG_TRAILING_ARM )
		{
			ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ),
								 "Wahacz wleczony nie skręca - z przodu auto pojedzie tylko na wprost." );
		}

		SectionHeader( "Postawa - jak stoi auto (najważniejsze ustawienia)" );
		edited |= ImGui::SliderFloat( "Opadanie wahacza", &m_editWishbone.restArmDroopDeg, 0.0f, 16.0f, "%.1f st." );
		HelpMarker( "Wahacze zwisają W DÓŁ do koła w spoczynku (jak w BeamNG) zamiast wyginać się do góry. 16 st. to "
					"zbadany bezpieczny sufit - wyżej kierownica traci geometrię (jedno koło blokuje się do oporu). "
					"Działa tylko na osiach z podwójnym wahaczem. Wymaga Zastosuj." );
		if ( ImGui::SliderFloat( "Prześwit", &m_config.suspensionPreload, -0.08f, 0.20f, "%.3f m" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Docisk wstępny sprężyny: podnosi lub obniża auto, na żywo, bez przebudowy. Jak regulacja "
					"talerzyka w regulowanym coiloverze - zmiana sztywności sprężyny (poniżej) lekko przesuwa "
					"osiadłą wysokość, dostrój wtedy ponownie." );
		if ( ImGui::SliderFloat( "Skok ściskania", &m_config.compressionTravel, 0.10f, 0.70f, "%.2f m" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Skok odbicia", &m_config.reboundTravel, 0.10f, 0.60f, "%.2f m" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Jak daleko koło może się ruszyć w górę (ściskanie) i w dół (odbicie) od pozycji spoczynkowej, "
					"zanim amortyzator dojdzie do ogranicznika. Offroad chce obu dużo, drift/tor chce ciasno. Na "
					"żywo, bez przebudowy." );

		SectionHeader( "Sprężyny i tłumienie (na żywo)" );
		if ( ImGui::SliderFloat( "Twardość sprężyny", &m_config.suspensionHertz, 1.0f, 12.0f, "%.1f Hz" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Miękko = więcej komfortu i przyczepności w terenie, ale więcej przechyłu. Twardo = szybsza, "
					"bardziej torowa reakcja." );
		if ( ImGui::SliderFloat( "Tłumienie", &m_config.suspensionDampingRatio, 0.2f, 2.0f, "%.2f" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Jak szybko gasną drgania po odbiciu. Za mało = auto 'skacze' po nierównościach; za dużo = "
					"zawieszenie sztywnieje na nierównym terenie." );
		if ( ImGui::SliderFloat( "Mnożnik twardości - przód", &m_config.frontSuspensionScale, 0.5f, 2.0f, "%.2f x" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Mnożnik twardości - tył", &m_config.rearSuspensionScale, 0.5f, 2.0f, "%.2f x" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Mnożnik twardości i tłumienia osobno dla przodu i tyłu - podbij tył dla auta z ciężkim "
					"bagażnikiem albo przód dla nosowego silnika." );

		SectionHeader( "Stabilizatory przechyłu (na żywo)" );
		ImGui::SliderFloat( "Stabilizator przód", &m_config.arbFrontStiffness, 0.0f, 40000.0f, "%.0f N/m" );
		ImGui::SliderFloat( "Stabilizator tył", &m_config.arbRearStiffness, 0.0f, 40000.0f, "%.0f N/m" );
		HelpMarker( "Ogranicza przechył nadwozia w zakręcie, przenosząc obciążenie między lewym a prawym kołem tej "
					"samej osi. Mocniejszy przedni = więcej podsterowności; mocniejszy tylny = auto chętniej "
					"'wchodzi w tył' (żywsza rotacja)." );
		if ( ImGui::Checkbox( "Wspomaganie pionowania (podpórka ratunkowa)", &m_config.uprightAssist ) )
		{
			CreateVehicle();
		}
		HelpMarker( "Sztuczna siła trzymająca nadwozie poziomo - włącz tylko gdy auto się przewraca mimo dobrze "
					"ustawionych stabilizatorów. Domyślnie wyłączone: przechył kontrolują stabilizatory powyżej, "
					"uczciwie." );

		if ( ImGui::CollapsingHeader( "Zaawansowane: geometria wahaczy" ) )
		{
			ImGui::Indent();
			ImGui::TextWrapped( "Punkty mocowania zawieszenia. Zmieniają charakter jazdy w subtelny sposób - "
								 "większość osób nigdy nie musi tu wchodzić." );
			edited |= ImGui::SliderFloat( "Caster (wyprzedzenie)", &m_editWishbone.casterDeg, -2.0f, 12.0f, "%.1f st." );
			HelpMarker( "Większy caster = silniejsze samo-centrowanie kierownicy i mocniejsza kontra w poślizgu. "
						"Ustawienia driftowe: 7-10 st." );
			edited |= ImGui::SliderFloat( "Pochylenie sworznia", &m_editWishbone.kingpinInclinationDeg, 0.0f, 15.0f,
										   "%.1f st." );
			edited |= ImGui::SliderFloat( "Offset sworznia", &m_editWishbone.kingpinOffset, 0.05f, 0.25f, "%.2f m" );
			edited |= ImGui::SliderFloat( "Wysokość zwrotnicy", &m_editWishbone.uprightHalfHeight, 0.10f, 0.30f, "%.2f m" );
			edited |= ImGui::SliderFloat( "Długość górnego wahacza", &m_editWishbone.upperArmLength, 0.20f, 0.55f, "%.2f m" );
			edited |= ImGui::SliderFloat( "Długość dolnego wahacza", &m_editWishbone.lowerArmLength, 0.25f, 0.70f, "%.2f m" );
			HelpMarker( "Dłuższe dolne wahacze = łagodniejszy przyrost kąta pochylenia koła (camber) przy skoku." );
			edited |= ImGui::SliderFloat( "Rozstaw mocowań wahacza", &m_editWishbone.armHalfSpread, 0.12f, 0.40f, "%.2f m" );
			edited |= ImGui::SliderFloat( "Cofnięcie ramienia kierown.", &m_editWishbone.steeringArmBack, 0.10f, 0.25f, "%.2f m" );
			edited |= ImGui::Checkbox( "Trapez Ackermanna (mechaniczny)", &m_editWishbone.ackermannTrapezoid );
			if ( m_editWishbone.ackermannTrapezoid )
			{
				edited |= ImGui::SliderFloat( "Udział Ackermanna", &m_editWishbone.ackermannFraction, 0.0f, 1.0f, "%.2f" );
			}
			edited |= ImGui::SliderFloat( "Wysokość mocowania amortyzatora", &m_editWishbone.coiloverTopHeight, 0.25f,
										   0.60f, "%.2f m" );
			edited |= ImGui::SliderFloat( "Masa zwrotnicy", &m_editKnuckleMass, 10.0f, 50.0f, "%.0f kg" );
			edited |= ImGui::SliderFloat( "Masa wahacza", &m_editArmMass, 2.0f, 15.0f, "%.1f kg" );
			edited |= ImGui::SliderFloat( "Caster kolumny (osie kolumnowe)", &m_editStrutCasterDeg, -2.0f, 12.0f, "%.1f st." );
			ImGui::Unindent();
		}

		if ( ImGui::CollapsingHeader( "Zaawansowane: wahacz wleczony (model Jozza)" ) )
		{
			ImGui::Indent();
			ImGui::TextWrapped( "%s", m_trailingImport.status.c_str() );
			if ( ImGui::Button( "Wczytaj ponownie z kontraktu" ) )
			{
				m_trailingImport = LoadJozzVehicleM7TrailingArmGeometry( "one_sided_wheel_mount.asset.json" );
				m_editTrailingArm = m_trailingImport.geometry;
				LoadMountVisual();
				m_structuralSetupDirty = true;
			}
			edited |= ImGui::SliderFloat( "Oś obrotu przed kołem", &m_editTrailingArm.pivotOffset.x, 0.30f, 0.90f, "%.2f m" );
			edited |= ImGui::SliderFloat( "Oś obrotu nad kołem", &m_editTrailingArm.pivotOffset.y, -0.05f, 0.35f, "%.2f m" );
			edited |= ImGui::SliderFloat( "Masa wahacza wleczonego", &m_editTrailingArm.armMass, 6.0f, 25.0f, "%.0f kg" );
			ImGui::Unindent();
		}

		if ( ImGui::CollapsingHeader( "Zaawansowane: kształt kolizji koła" ) )
		{
			ImGui::Indent();
			const char* envelopes[] = { "Sfera (gładka, wybrzusza się na boki)",
										"Walec (prawdziwa szerokość, graniasty)", "Suma fazowa (eksperymentalne)",
										"Mieszana: sfera + prawdziwa szerokość (domyślne)" };
			edited |= ImGui::Combo( "Kształt", &m_editEnvelopeMode, envelopes, 4 );
			if ( m_editEnvelopeMode == JOZZ_M6_ENVELOPE_PHASED_UNION )
			{
				edited |= ImGui::SliderInt( "Warstwy sumy", &m_editEnvelopeLayers, 2, 4 );
			}
			ImGui::Unindent();
		}

		if ( edited )
		{
			m_structuralSetupDirty = true;
		}

		ImGui::Spacing();
		// Confirmed, not instant: this wipes every dial across all tabs, and
		// since restarting/reopening the app now RESTORES last session (the R
		// fix above), an accidental click here would otherwise get silently
		// baked in as the new "last session" the moment the app closes -
		// exactly the kind of quiet data loss this whole pass was hunting for.
		if ( ImGui::Button( "Przywróć wszystkie ustawienia domyślne" ) || m_testOpenResetModal )
		{
			m_testOpenResetModal = false;
			ImGui::OpenPopup( "Potwierdz reset##ConfirmDefaults" );
		}
		HelpMarker( "Resetuje CAŁY pojazd do domyślnych ustawień fabrycznych - łącznie z nadwoziem (zakładka "
					"Nadwozie)." );
		if ( ImGui::BeginPopupModal( "Potwierdz reset##ConfirmDefaults", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
		{
			ImGui::TextWrapped( "To wyzeruje WSZYSTKIE ustawienia (Zawieszenie, Nadwozie, Napęd, Kierownica) do "
								 "wartości fabrycznych. Jeśli chcesz zachować obecne strojenie, zamknij to okno i "
								 "najpierw zapisz je jako preset (pole 'nazwa nowego presetu' powyżej)." );
			if ( ImGui::Button( "Tak, resetuj" ) )
			{
				JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_assetMetadata );
				m_config = JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth,
														defaults.assetSuspensionTravelHint );
				m_config.trailingArm = m_trailingImport.geometry;
				CreateVehicle();
				SyncEditFromConfig();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Anuluj" ) )
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	// Body dimensions and axle layout - "what car am I building", separate from
	// "how does each corner's suspension move" (Zawieszenie tab). Everything
	// here rebuilds the vehicle.
	void DrawChassisTab()
	{
		bool edited = false;

		SectionHeader( "Wymiary nadwozia" );
		edited |= ImGui::SliderFloat( "Połowa długości", &m_editChassisHalfExtents.x, 0.8f, 2.5f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Połowa wysokości", &m_editChassisHalfExtents.y, 0.15f, 0.70f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Połowa szerokości", &m_editChassisHalfExtents.z, 0.35f, 1.00f, "%.2f m" );
		HelpMarker( "Rozmiar skrzyni nadwozia (pudełka). 'Połowa' bo liczone od środka - realna długość/szerokość to "
					"dwa razy tyle." );
		edited |= ImGui::SliderFloat( "Gęstość (masa)", &m_editChassisDensity, 50.0f, 600.0f, "%.0f kg/m^3" );
		HelpMarker( "Razem z wymiarami wyżej decyduje o masie nadwozia. Lekki drifter: nisko; ciężarówka: wysoko." );
		edited |= ImGui::SliderFloat( "Obniżenie środka ciężkości", &m_editCgVerticalOffset, -0.10f, 0.40f, "%.2f m" );
		HelpMarker( "Jak nisko pod geometrycznym środkiem nadwozia leży faktyczny środek ciężkości (silnik, "
					"pasażerowie, ładunek). Niżej = stabilniej w zakrętach." );

		SectionHeader( "Rozstaw osi i kół" );
		edited |= ImGui::SliderFloat( "Połowa rozstawu osi", &m_editAxleHalfSpacing, 0.6f, 2.5f, "%.2f m" );
		HelpMarker( "Odległość przedniej/tylnej osi od środka auta - razem dają rozstaw osi (wheelbase)." );
		edited |= ImGui::SliderFloat( "Połowa rozstawu kół", &m_editTrackHalfWidth, 0.6f, 1.8f, "%.2f m" );
		HelpMarker( "Odległość lewego/prawego koła od środka auta - razem dają rozstaw kół (track). Szerzej = "
					"stabilniej w zakrętach, węziej = zwrotniej." );
		edited |= ImGui::SliderFloat( "Opuszczenie spoczynkowe", &m_editRestDrop, 0.20f, 1.20f, "%.2f m" );
		HelpMarker( "Jak daleko pod nadwoziem leży środek koła w pozycji spoczynkowej - bazowy prześwit przed "
					"dostrojeniem suwakiem 'Prześwit' w zakładce Zawieszenie." );
		edited |= ImGui::SliderFloat( "Gęstość koła", &m_editWheelDensity, 20.0f, 300.0f, "%.0f kg/m^3" );
		HelpMarker( "Masa koła (niesprężona) - wpływa na to, jak szybko koło reaguje na nierówności." );

		if ( edited )
		{
			m_structuralSetupDirty = true;
		}
	}

	// The sandbox itself - grip against the ground and how firmly the physics
	// solver resolves contact - plus the two everyday reset buttons. Visual
	// toggles and status live in Debug now; this tab is short on purpose.
	void DrawWorldTab()
	{
		// Moved here from above the tab bar (2026-07-08, UI compaction pass):
		// switching a whole setup (drift/offroad/street) is a "which car am I
		// driving" decision, not something tied to whichever tuning tab happens
		// to be open, but it also doesn't need to sit in front of every tab all
		// the time - Świat (the sandbox/reset tab) is a natural home for a
		// once-in-a-while pick. Loading a preset still commits immediately (like
		// "Przywróć wszystkie ustawienia domyślne" already does) rather than
		// staging through Apply - loading half of a preset would leave the car
		// in a state that was never actually designed.
		SectionHeader( "Presety pojazdu" );
		{
			std::vector<const char*> items;
			items.reserve( m_availablePresets.size() );
			for ( const std::string& name : m_availablePresets )
			{
				items.push_back( name.c_str() );
			}
			ImGui::SetNextItemWidth( 12.0f * ImGui::GetFontSize() );
			if ( items.empty() )
			{
				ImGui::TextDisabled( "(brak zapisanych presetów - zapisz jeden poniżej)" );
			}
			else
			{
				ImGui::Combo( "##PresetSelect", &m_selectedPresetIndex, items.data(), (int)items.size() );
				ImGui::SameLine();
				bool validSelection = m_selectedPresetIndex >= 0 && m_selectedPresetIndex < (int)m_availablePresets.size();
				if ( ImGui::Button( "Wczytaj" ) && validSelection )
				{
					LoadPresetByName( m_availablePresets[m_selectedPresetIndex] );
				}
			}
			ImGui::SetNextItemWidth( 12.0f * ImGui::GetFontSize() );
			ImGui::InputTextWithHint( "##PresetName", "nazwa nowego presetu...", m_presetNameBuffer,
									   sizeof( m_presetNameBuffer ) );
			ImGui::SameLine();
			if ( ImGui::Button( "Zapisz jako" ) )
			{
				SaveCurrentAsPreset( m_presetNameBuffer );
			}
			// Quiet heads-up, not a confirm popup: overwriting your OWN preset by
			// re-saving under the same name is a normal, expected part of
			// iterating on a setup, so this only needs to be visible, not gate
			// the click behind another dialog.
			bool nameExists = false;
			for ( const std::string& name : m_availablePresets )
			{
				if ( name == m_presetNameBuffer )
				{
					nameExists = true;
					break;
				}
			}
			if ( nameExists )
			{
				ImGui::TextDisabled( "Preset '%s' już istnieje - zapis go nadpisze.", m_presetNameBuffer );
			}
			if ( m_presetStatus.empty() == false )
			{
				ImGui::TextColored( ImVec4( 0.6f, 0.85f, 0.6f, 1.0f ), "%s", m_presetStatus.c_str() );
			}
		}

		SectionHeader( "Przyczepność" );
		if ( ImGui::SliderFloat( "Tarcie opon", &m_config.wheelFriction, 0.4f, 2.5f, "%.2f" ) )
		{
			ApplyWheelFriction();
		}
		HelpMarker( "Mnożnik przyczepności opon o podłoże. Poniżej 1.0 = ślisko (lód, mokra nawierzchnia); powyżej "
					"1.0 = lepka nawierzchnia (slicki, asfalt na sucho)." );

		SectionHeader( "Solver kontaktu (zaawansowane)" );
		bool contactEdited = false;
		contactEdited |= ImGui::SliderFloat( "Sztywność kontaktu", &m_contactHertz, 10.0f, 100.0f, "%.0f Hz" );
		contactEdited |= ImGui::SliderFloat( "Tłumienie kontaktu", &m_contactDampingRatio, 2.0f, 25.0f, "%.1f" );
		contactEdited |= ImGui::SliderFloat( "Prędkość wypychania", &m_contactSpeed, 0.5f, 8.0f, "%.1f m/s" );
		HelpMarker( "Jak twardo silnik fizyki rozwiązuje przenikanie koła z podłożem. Domyślne wartości silnika są "
					"dobre dla większości przypadków - dotykaj tylko jeśli koła 'tuną' albo zapadają się w podłoże." );
		if ( contactEdited )
		{
			ApplyContactTuning();
		}
		if ( ImGui::Button( "Przywróć domyślne solvera" ) )
		{
			m_contactHertz = 30.0f;
			m_contactDampingRatio = 10.0f;
			m_contactSpeed = 3.0f;
			ApplyContactTuning();
		}

		SectionHeader( "Reset" );
		if ( ImGui::Button( "Zresetuj swiat" ) )
		{
			ResetWorld();
		}
		HelpMarker( "Pelny restart symulacji: auto na miejsce startowe, przeszkody na miejsce, telemetria od zera. "
					"Nie rusza dostrojenia (Zawieszenie/Nadwozie/Naped/Kierownica) ani ustawien Debug - to samo co "
					"robi klawisz R, ale bez ryzyka utraty wlasnie zmienionych suwakow czy checkboxow." );
		ImGui::Spacing();
		if ( ImGui::Button( "Zresetuj pojazd" ) )
		{
			CreateVehicle();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Zresetuj przeszkody" ) )
		{
			ResetJozzVehicleM5TestCourseProps( m_testCourse );
		}
	}

	// Everything here is for LOOKING AT the rig, never for tuning how it
	// drives: visualization toggles, raw per-corner numbers, live plots, and
	// asset load status. Nothing on this tab changes vehicle behavior.
	void DrawDebugTab()
	{
		SectionHeader( "Wizualizacje" );
		ImGui::Checkbox( "Model 3D kół", &m_showWheelVisuals );
		ImGui::Checkbox( "Model 3D zawieszenia", &m_showMountVisuals );
		if ( ImGui::Checkbox( "Surowe kształty kolizji kół", &m_showPrimitiveWheelShapes ) )
		{
			UpdateWheelShapeVisibility();
		}
		ImGui::Checkbox( "Linie geometrii zawieszenia (wahacze/drążki)", &m_showRigDiagnostics );
		ImGui::Checkbox( "Podświetl wahacze (góra=czerwony, dół=niebieski)", &m_armTint );
		if ( ImGui::Button( "Wypisz geometrię narożników do konsoli" ) )
		{
			DumpCornerGeometry();
		}

		SectionHeader( "Status wczytanych assetów" );
		ImGui::TextWrapped( "koło: %s", m_wheelVisual.status.c_str() );
		ImGui::TextWrapped( "mocowanie L: %s", m_riggedMountL.status.c_str() );
		ImGui::TextWrapped( "mocowanie R: %s", m_riggedMountR.status.c_str() );
		ImGui::TextWrapped( "metadane: %s", m_assetMetadata.status.c_str() );

		SectionHeader( "Telemetria na żywo (PL/PP/TL/TP)" );
		const char* cornerNames[JOZZ_M6_CORNER_COUNT] = { "PL", "PP", "TL", "TP" };
		JozzVehicleM6WheelTelemetry current[JOZZ_M6_CORNER_COUNT];
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			current[corner] = GetJozzVehicleM6WheelTelemetry( m_vehicle, corner );
		}

		ImGui::Text( "kontakt z podłożem: PL %s  PP %s  TL %s  TP %s", current[0].groundContact ? "TAK" : "powietrze",
					 current[1].groundContact ? "TAK" : "powietrze", current[2].groundContact ? "TAK" : "powietrze",
					 current[3].groundContact ? "TAK" : "powietrze" );
		ImGui::Text( "obciążenie N: PL %.0f  PP %.0f  TL %.0f  TP %.0f", current[0].suspensionLoad,
					 current[1].suspensionLoad, current[2].suspensionLoad, current[3].suspensionLoad );
		ImGui::Text( "poślizg st.: PL %.1f  PP %.1f  TL %.1f  TP %.1f", 180.0f / B3_PI * current[0].slipAngle,
					 180.0f / B3_PI * current[1].slipAngle, 180.0f / B3_PI * current[2].slipAngle,
					 180.0f / B3_PI * current[3].slipAngle );
		ImGui::Text( "pochylenie koła st.: PL %.1f  PP %.1f  TL %.1f  TP %.1f", 180.0f / B3_PI * current[0].camberAngle,
					 180.0f / B3_PI * current[1].camberAngle, 180.0f / B3_PI * current[2].camberAngle,
					 180.0f / B3_PI * current[3].camberAngle );
		ImGui::Text( "obroty rad/s: PL %.1f  PP %.1f  TL %.1f  TP %.1f", current[0].spinSpeed, current[1].spinSpeed,
					 current[2].spinSpeed, current[3].spinSpeed );

		if ( m_telemetryCount < 2 )
		{
			return;
		}

		float latestTime = m_telemetryTime[( m_telemetryHead + TELEMETRY_CAPACITY - 1 ) % TELEMETRY_CAPACITY];
		int count = m_telemetryCount;
		int start = ( m_telemetryHead + TELEMETRY_CAPACITY - count ) % TELEMETRY_CAPACITY;

		static float times[TELEMETRY_CAPACITY];
		static float series[JOZZ_M6_CORNER_COUNT][TELEMETRY_CAPACITY];
		for ( int i = 0; i < count; ++i )
		{
			int index = ( start + i ) % TELEMETRY_CAPACITY;
			times[i] = m_telemetryTime[index];
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				series[corner][i] = m_telemetryTravel[corner][index];
			}
		}

		ImVec2 plotSize( ImGui::GetContentRegionAvail().x, 130.0f );
		if ( ImPlot::BeginPlot( "Skok zawieszenia", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "skok m" );
			ImPlot::SetupAxisLimits( ImAxis_X1, latestTime - 10.0, latestTime, ImPlotCond_Always );
			ImPlot::SetupAxisLimits( ImAxis_Y1, -m_config.reboundTravel * 1.2, m_config.compressionTravel * 1.2 );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				ImPlot::PlotLine( cornerNames[corner], times, series[corner], count );
			}
			ImPlot::EndPlot();
		}

		for ( int i = 0; i < count; ++i )
		{
			int index = ( start + i ) % TELEMETRY_CAPACITY;
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				series[corner][i] = m_telemetrySlip[corner][index];
			}
		}

		if ( ImPlot::BeginPlot( "Kąt poślizgu", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "poślizg st." );
			ImPlot::SetupAxisLimits( ImAxis_X1, latestTime - 10.0, latestTime, ImPlotCond_Always );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				ImPlot::PlotLine( cornerNames[corner], times, series[corner], count );
			}
			ImPlot::EndPlot();
		}
	}

	// The engine/camera stats block above this (frame time, step count, camera
	// pivot) is folded behind a closed CollapsingHeader for this lab
	// (CondenseDebugOverlay override below) - with 6 tabs of sliders to fit,
	// that block ate a third of the panel for numbers nobody tunes with. Same
	// reasoning killed the multi-line control hint here: one line + a "(?)"
	// tooltip instead of three permanent TextDisabled lines. Presets used to
	// live here too, above the tab bar; they now live in the Świat tab
	// (DrawWorldTab) - "which car am I driving" is a per-session choice, not
	// something that needs to occupy prime panel space on every tab, and the
	// tab bar itself is what should greet you first.
	bool DrawControls() override
	{
		ImGui::Text( "prędkość %.1f m/s (%.0f km/h)", GetJozzVehicleM6ForwardSpeed( m_vehicle ),
					 3.6f * GetJozzVehicleM6ForwardSpeed( m_vehicle ) );
		ImGui::TextDisabled( "Sterowanie i R restart" );
		HelpMarker( "W/S jazda, A/D skręt, Spacja hamulec, T kamera, R restart.\n"
					"R restart zachowuje strojenie i ustawienia Debug (auto-zapis sesji).\n"
					"Przycisk 'Zresetuj świat' (zakładka Świat) robi to samo bez klawiatury." );
		ImGui::Separator();

		// The narrow default item width fits the old single-column panel; the
		// tabs host wider sliders.
		ImGui::PushItemWidth( 9.0f * ImGui::GetFontSize() );

		// Tab order follows a natural setup flow: what kind of suspension and
		// how it sits (Zawieszenie) -> what car it's bolted to (Nadwozie) ->
		// engine/steering feel -> sandbox -> look under the hood (Debug).
		// JOZZ_M6_TAB env (0-5) force-selects a tab once, for headless
		// --screenshot verification of a specific tab without UI interaction.
		auto tabFlags = [this]( int index ) {
			return ( m_forceTabIndex == index ) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		};
		if ( ImGui::BeginTabBar( "M6RigLabTabs", ImGuiTabBarFlags_None ) )
		{
			// "###TabX" pins the tab's identity to a fixed string regardless of
			// the visible label: ImGui hashes everything from "###" onward as the
			// ID and ignores what's before it for that purpose (see ImHashStr).
			// Without it, "Zawieszenie" and "Zawieszenie *" are two DIFFERENT
			// tabs as far as the tab bar is concerned - the moment a pending edit
			// appends " *", the bar sees the old tab vanish and a new one appear,
			// and its own closed-tab fallback jumps the active selection to
			// whatever tab it last remembers as second-most-recent (observed:
			// editing anything on Zawieszenie booted the user to Kierownica).
			// Same bug fires in reverse on Apply/Discard when " *" drops back off.
			if ( ImGui::BeginTabItem( m_structuralSetupDirty ? "Zawieszenie *###TabSuspension" : "Zawieszenie###TabSuspension",
									   nullptr, tabFlags( 0 ) ) )
			{
				DrawSuspensionTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( m_structuralSetupDirty ? "Nadwozie *###TabChassis" : "Nadwozie###TabChassis", nullptr,
									   tabFlags( 1 ) ) )
			{
				DrawChassisTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Napęd", nullptr, tabFlags( 2 ) ) )
			{
				DrawDriveTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Kierownica", nullptr, tabFlags( 3 ) ) )
			{
				DrawSteeringTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Świat", nullptr, tabFlags( 4 ) ) )
			{
				DrawWorldTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Debug", nullptr, tabFlags( 5 ) ) )
			{
				DrawDebugTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		m_forceTabIndex = -1;

		ImGui::PopItemWidth();

		// Apply bar: visible from every tab so a pending rebuild is never lost
		// behind the Zawieszenie/Nadwozie tabs.
		if ( m_structuralSetupDirty )
		{
			ImGui::Separator();
			ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ), "Są niezastosowane zmiany geometrii" );
			ImGui::SameLine();
			if ( ImGui::Button( "Zastosuj (przebuduj pojazd)" ) )
			{
				ApplyPendingStructuralSetup();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Odrzuć" ) )
			{
				SyncEditFromConfig();
			}
		}

		return true;
	}

	void SampleTelemetry()
	{
		float dt = m_context->hertz > 0.0f ? 1.0f / m_context->hertz : 1.0f / 60.0f;
		m_telemetryClock += dt;

		m_telemetryTime[m_telemetryHead] = m_telemetryClock;
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			JozzVehicleM6WheelTelemetry telemetry = GetJozzVehicleM6WheelTelemetry( m_vehicle, corner );
			m_telemetryTravel[corner][m_telemetryHead] = telemetry.suspensionTravel;
			m_telemetrySlip[corner][m_telemetryHead] = 180.0f / B3_PI * telemetry.slipAngle;
		}

		m_telemetryHead = ( m_telemetryHead + 1 ) % TELEMETRY_CAPACITY;
		if ( m_telemetryCount < TELEMETRY_CAPACITY )
		{
			m_telemetryCount += 1;
		}
	}

	void Step() override
	{
		JozzVehicleM6DriveInput input = {};
		if ( IsKeyDown( KEY_W ) )
		{
			input.drive += 1.0f;
		}
		if ( IsKeyDown( KEY_S ) )
		{
			input.drive -= 1.0f;
		}
		if ( IsKeyDown( KEY_A ) )
		{
			input.steer += 1.0f;
		}
		if ( IsKeyDown( KEY_D ) )
		{
			input.steer -= 1.0f;
		}
		input.brake = IsKeyDown( KEY_SPACE );

		if ( m_invertSteering )
		{
			input.steer = -input.steer;
		}

		m_lastInput = input;
		UpdateJozzVehicleM6Drive( m_vehicle, input );

		if ( m_camera->m_thirdPerson && m_vehicle.valid )
		{
			b3WorldTransform transform = b3Body_GetTransform( m_vehicle.chassisId );
			m_camera->m_pivot = transform.p;
			m_camera->UpdateTransform();
		}

		Sample::Step();

		if ( m_vehicle.valid )
		{
			SampleTelemetry();
		}
	}

	void DrawRigDiagnostics()
	{
		// Live rig geometry without any suspension model mounted: yellow =
		// upper arms, orange = lower arms, green = coilover, cyan = tie/toe
		// link, magenta = kingpin axis / strut, white = trailing arm.
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];

			if ( runtime.rigType == JOZZ_M6_RIG_TRAILING_ARM && B3_IS_NON_NULL( runtime.trailingArmId ) )
			{
				b3Pos pivot = b3Body_GetPosition( runtime.trailingArmId );
				b3Pos wheel = b3Body_GetPosition( runtime.wheelId );
				b3Pos damperChassis = b3Body_GetWorldPoint( m_vehicle.chassisId, runtime.trailingDamperChassisLocal );
				b3Pos damperArm = b3Body_GetWorldPoint(
					runtime.trailingArmId, b3Sub( runtime.trailingDamperArmLocal, runtime.trailingPivotLocal ) );

				DrawCross( pivot, 0.07f, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
				DrawLine( pivot, wheel, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
				DrawLine( damperChassis, damperArm, MakeVec4( 0.2f, 1.0f, 0.3f, 1.0f ) );
				continue;
			}

			if ( runtime.rigType != JOZZ_M6_RIG_DOUBLE_WISHBONE || B3_IS_NULL( runtime.knuckleId ) )
			{
				b3Pos rest = GetJozzVehicleM6RestWheelCenter( m_vehicle, corner );
				b3Pos wheel = b3Body_GetPosition( runtime.wheelId );
				DrawCross( rest, 0.09f, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
				DrawLine( rest, wheel, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
				continue;
			}

			const JozzVehicleM6WishboneHardpoints& hp = runtime.hardpoints;
			b3Vec3 wheelCenterLocal = runtime.restWheelCenterLocal;

			// Ball joints from the LIVE arm bodies (origin = hinge midpoint),
			// so the drawn arms are the actual constraint geometry, stops and
			// all - not a schematic.
			b3Vec3 upperHingeMid = b3MulSV( 0.5f, b3Add( hp.upperFrontChassis, hp.upperRearChassis ) );
			b3Vec3 lowerHingeMid = b3MulSV( 0.5f, b3Add( hp.lowerFrontChassis, hp.lowerRearChassis ) );
			b3Pos upperBall = b3Body_GetWorldPoint( runtime.upperArmId, b3Sub( hp.upperBallJoint, upperHingeMid ) );
			b3Pos lowerBall = b3Body_GetWorldPoint( runtime.lowerArmId, b3Sub( hp.lowerBallJoint, lowerHingeMid ) );
			b3Pos steeringArm = b3Body_GetWorldPoint( runtime.knuckleId, b3Sub( hp.steeringArm, wheelCenterLocal ) );
			b3Pos coiloverKnuckle = b3Body_GetWorldPoint( runtime.knuckleId, b3Sub( hp.coiloverKnuckle, wheelCenterLocal ) );

			b3Pos upperFront = b3Body_GetWorldPoint( m_vehicle.chassisId, hp.upperFrontChassis );
			b3Pos upperRear = b3Body_GetWorldPoint( m_vehicle.chassisId, hp.upperRearChassis );
			b3Pos lowerFront = b3Body_GetWorldPoint( m_vehicle.chassisId, hp.lowerFrontChassis );
			b3Pos lowerRear = b3Body_GetWorldPoint( m_vehicle.chassisId, hp.lowerRearChassis );
			b3Pos coiloverChassis = b3Body_GetWorldPoint( m_vehicle.chassisId, hp.coiloverChassis );

			DrawLine( upperFront, upperBall, MakeVec4( 1.0f, 1.0f, 0.2f, 1.0f ) );
			DrawLine( upperRear, upperBall, MakeVec4( 1.0f, 1.0f, 0.2f, 1.0f ) );
			DrawLine( lowerFront, lowerBall, MakeVec4( 1.0f, 0.6f, 0.1f, 1.0f ) );
			DrawLine( lowerRear, lowerBall, MakeVec4( 1.0f, 0.6f, 0.1f, 1.0f ) );
			DrawLine( coiloverChassis, coiloverKnuckle, MakeVec4( 0.2f, 1.0f, 0.3f, 1.0f ) );
			DrawLine( upperBall, lowerBall, MakeVec4( 1.0f, 0.2f, 1.0f, 1.0f ) );
			DrawCross( upperBall, 0.05f, MakeVec4( 1.0f, 0.2f, 1.0f, 1.0f ) );
			DrawCross( lowerBall, 0.05f, MakeVec4( 1.0f, 0.2f, 1.0f, 1.0f ) );

			if ( B3_IS_NON_NULL( m_vehicle.rackId ) && ( corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT ) )
			{
				float rackEndZ = corner == JOZZ_M6_FRONT_LEFT ? -m_config.rackHalfWidth : m_config.rackHalfWidth;
				b3Pos rackEnd = b3Body_GetWorldPoint( m_vehicle.rackId, { 0.0f, 0.0f, rackEndZ } );
				DrawLine( rackEnd, steeringArm, MakeVec4( 0.2f, 0.9f, 1.0f, 1.0f ) );
			}
			else
			{
				float in = ( corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_REAR_LEFT ) ? 1.0f : -1.0f;
				b3Vec3 toeChassis = b3Add( hp.steeringArm, { 0.0f, 0.0f, in * m_config.wishbone.lowerArmLength } );
				b3Pos toeWorld = b3Body_GetWorldPoint( m_vehicle.chassisId, toeChassis );
				DrawLine( toeWorld, steeringArm, MakeVec4( 0.2f, 0.9f, 1.0f, 1.0f ) );
			}
		}
	}

	// Prints exact world geometry for every wishbone corner once the car has
	// settled, so symmetry (FL vs FR should be a clean Z-mirror) and tire reach
	// (ball-joint Z vs the tire Z-band) can be judged from numbers, not pixels.
	void DumpCornerGeometry()
	{
		const char* names[JOZZ_M6_CORNER_COUNT] = { "FL", "FR", "RL", "RR" };
		b3WorldTransform chassisLive = b3Body_GetTransform( m_vehicle.chassisId );
		float halfW = 0.5f * m_config.wheelEnvelope.width;
		b3Pos chassisP = b3Body_GetPosition( m_vehicle.chassisId );
		printf( "[DUMP] hertz=%.2f chassisY=%.3f tire R=%.3f W=%.3f trackHalf=%.3f\n", m_config.suspensionHertz,
				chassisP.y, m_config.wheelEnvelope.radius, m_config.wheelEnvelope.width, m_config.trackHalfWidth );
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			if ( runtime.rigType != JOZZ_M6_RIG_DOUBLE_WISHBONE || B3_IS_NULL( runtime.knuckleId ) )
			{
				continue;
			}
			const JozzVehicleM6WishboneHardpoints& hp = runtime.hardpoints;
			b3Vec3 wl = runtime.restWheelCenterLocal;
			b3WorldTransform knuckleLive = b3Body_GetTransform( runtime.knuckleId );
			b3Pos wheelC = b3Body_GetPosition( runtime.wheelId );
			b3Pos upBall = b3TransformPoint( knuckleLive, b3Sub( hp.upperBallJoint, wl ) );
			b3Pos loBall = b3TransformPoint( knuckleLive, b3Sub( hp.lowerBallJoint, wl ) );
			b3Pos upChM =
				b3TransformPoint( chassisLive, b3MulSV( 0.5f, b3Add( hp.upperFrontChassis, hp.upperRearChassis ) ) );
			b3Pos loChM =
				b3TransformPoint( chassisLive, b3MulSV( 0.5f, b3Add( hp.lowerFrontChassis, hp.lowerRearChassis ) ) );
			printf( "[DUMP] %s isLeft=%d wheelC=(% .3f,% .3f,% .3f)\n", names[corner], CornerIsLeft( corner ) ? 1 : 0,
					wheelC.x, wheelC.y, wheelC.z );
			printf( "   upBall=(% .3f,% .3f,% .3f) loBall=(% .3f,% .3f,% .3f)\n", upBall.x, upBall.y, upBall.z,
					loBall.x, loBall.y, loBall.z );
			printf( "   upChM =(% .3f,% .3f,% .3f) loChM =(% .3f,% .3f,% .3f)\n", upChM.x, upChM.y, upChM.z, loChM.x,
					loChM.y, loChM.z );
			printf( "   tireZband=[% .3f,% .3f] upBallZ=% .3f loBallZ=% .3f (inboard=|z|<|%.3f|)\n", wheelC.z - halfW,
					wheelC.z + halfW, upBall.z, loBall.z, wheelC.z );
		}
		fflush( stdout );
	}

	void Render() override
	{
		Sample::Render();

		if ( m_vehicle.valid == false )
		{
			return;
		}

		if ( m_dumpGeometry && ++m_dumpFrame == 140 )
		{
			DumpCornerGeometry();
		}

		// Regression probe for the tab-identity bug (fixed 2026-07-08): flips
		// the dirty flag mid-run, after the tab bar has already rendered several
		// clean frames with a tab selected - exactly the transition that broke
		// under the old unstable "Zawieszenie"/"Zawieszenie *" tab IDs. Combine
		// with JOZZ_M6_TAB=0 and a --frames count past the trigger to confirm
		// the active tab stays put across the flip.
		if ( const char* v = std::getenv( "JOZZ_M6_DIRTY_AT_FRAME" ) )
		{
			if ( ++m_dirtyProbeFrame == atoi( v ) )
			{
				m_structuralSetupDirty = true;
			}
		}

		if ( m_showWheelVisuals && m_wheelVisual.IsLoaded() )
		{
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				b3WorldTransform wheelTransform = b3Body_GetTransform( m_vehicle.corners[corner].wheelId );
				b3WorldTransform visualTransform = b3MulWorldTransforms( wheelTransform, m_wheelVisualCorrection );
				m_wheelVisual.DrawAtTransform( visualTransform, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
			}
		}

		// Jozz's suspension model, rigged per bone on each corner: the hub part
		// follows the live knuckle (upright), the brackets and arm follow the
		// chassis. Left corners draw the authored mesh, right corners the
		// mirrored copy. What you see moves exactly as the physics does.
		if ( m_showMountVisuals )
		{
			b3WorldTransform chassisLive = b3Body_GetTransform( m_vehicle.chassisId );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				if ( m_cornerHasMount[corner] == false )
				{
					continue;
				}
				const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
				const JozzVehicleRiggedMesh& mesh = CornerIsLeft( corner ) ? m_riggedMountL : m_riggedMountR;
				if ( mesh.IsLoaded() == false )
				{
					continue;
				}

				b3WorldTransform knuckleLive = b3Body_GetTransform( runtime.knuckleId );
				b3WorldTransform bracketWorld = b3MulWorldTransforms( chassisLive, m_bracketLocal[corner] );
				b3WorldTransform hubWorld = b3MulWorldTransforms( knuckleLive, m_hubLocal[corner] );

				// The wishbone arms stay glued to the authored assembly: the chassis end
				// rides the ChassisMount bracket and the wheel end rides the WheelCenter
				// hub - the very parts it joins in the source model. Driving the ends with
				// the same bracketWorld/hubWorld transforms that draw those parts makes them
				// coincide exactly (no offset), and the arm still flexes as the knuckle travels.
				bool wheelNegX = CornerIsLeft( corner );

				const Vec4 white = MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f );
				const Vec4 topColor = m_armTint ? MakeVec4( 1.0f, 0.15f, 0.15f, 1.0f ) : white;
				const Vec4 botColor = m_armTint ? MakeVec4( 0.2f, 0.4f, 1.0f, 1.0f ) : white;
				for ( int i = 0; i < mesh.PartCount(); ++i )
				{
					const std::string& name = mesh.parts[i].boneName;
					bool isTop = name.find( "Chassis_Top" ) != std::string::npos;
					bool isBottom = name.find( "Chassis_Bottom" ) != std::string::npos;
					if ( isTop || isBottom )
					{
						b3Vec3 chassisEnd, wheelEnd;
						ArmEnds( mesh.parts[i], wheelNegX, chassisEnd, wheelEnd );
						mesh.DrawPartBetween( i, chassisEnd, wheelEnd, b3TransformPoint( bracketWorld, chassisEnd ),
							  b3TransformPoint( hubWorld, wheelEnd ), isTop ? topColor : botColor );
					}
					else if ( name.find( "WheelCenter" ) != std::string::npos )
					{
						mesh.DrawPart( i, hubWorld, white );
					}
					else
					{
						mesh.DrawPart( i, bracketWorld, white );
					}
				}
			}
		}

		// Two telescoping shocks per corner (2026-07-08 fix), pinned to the
		// model's own Socket_DamperUpper/Lower_L/R markers instead of guessed
		// offsets from the wheel centre - the old formula ("+0.62m above",
		// "0.28m toward centre") had no relationship to the authored geometry,
		// which is why the shock rendered floating in the air past the tire.
		// _L/_R differ only in Z in the contract (two shocks straddling the
		// arm, not the car's left/right side), so both get the SAME mirror
		// treatment as Socket_WheelCenter: negate X for right-side corners,
		// leave Z alone. Upper rides bracketWorld (chassis-relative, same as
		// the ChassisMount bracket parts); lower rides hubWorld (knuckle-
		// relative, same as the WheelCenter hub part) - the exact transforms
		// that pin the Chassis_Top/Chassis_Bottom arms above, so the shocks
		// stay glued to the same live geometry the arms articulate against.
		if ( m_showDumper && m_dumper.IsLoaded() )
		{
			b3WorldTransform chassisLive = b3Body_GetTransform( m_vehicle.chassisId );
			const Vec4 damperColor = MakeVec4( 0.82f, 0.84f, 0.9f, 1.0f );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				if ( m_cornerHasMount[corner] == false )
				{
					continue;
				}
				const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
				b3WorldTransform bracketWorld = b3MulWorldTransforms( chassisLive, m_bracketLocal[corner] );
				b3WorldTransform hubWorld =
					b3MulWorldTransforms( b3Body_GetTransform( runtime.knuckleId ), m_hubLocal[corner] );

				b3Vec3 upperL = m_damperUpperLAuthored;
				b3Vec3 upperR = m_damperUpperRAuthored;
				b3Vec3 lowerL = m_damperLowerLAuthored;
				b3Vec3 lowerR = m_damperLowerRAuthored;
				if ( CornerIsLeft( corner ) == false )
				{
					upperL.x = -upperL.x;
					upperR.x = -upperR.x;
					lowerL.x = -lowerL.x;
					lowerR.x = -lowerR.x;
				}

				m_dumper.DrawTelescopingDamper( b3TransformPoint( bracketWorld, upperL ),
												 b3TransformPoint( hubWorld, lowerL ), damperColor );
				m_dumper.DrawTelescopingDamper( b3TransformPoint( bracketWorld, upperR ),
												 b3TransformPoint( hubWorld, lowerR ), damperColor );
			}
		}

		if ( m_showRigDiagnostics )
		{
			DrawRigDiagnostics();
		}

		JozzVehicleM6WheelTelemetry frontLeft = GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_FRONT_LEFT );
		JozzVehicleM6WheelTelemetry frontRight = GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_FRONT_RIGHT );

		auto rigName = []( int rigType ) {
			switch ( rigType )
			{
				case JOZZ_M6_RIG_DOUBLE_WISHBONE:
					return "wahacz";
				case JOZZ_M6_RIG_TRAILING_ARM:
					return "wleczony";
				default:
					return "kolumna";
			}
		};

		bool handsOn = std::fabs( m_lastInput.steer ) > m_config.steerInputDeadzone;
		DrawTextLine( "Warsztat zawieszenia M6/M7 - W/S jazda, A/D skręt, Spacja hamulec, T kamera, R restart" );
		DrawTextLine( "prędkość %.1f m/s (%.0f km/h)  poślizg %.1f st.  kierownica %s",
					  GetJozzVehicleM6ForwardSpeed( m_vehicle ), 3.6f * GetJozzVehicleM6ForwardSpeed( m_vehicle ),
					  180.0f / B3_PI * GetJozzVehicleM6AlignmentAngle( m_vehicle ),
					  handsOn ? "wspomagana (ręce na kierownicy)" : "swobodna (prowadzi caster)" );
		DrawTextLine( "przód: %s, tył: %s", rigName( m_config.frontRigType ), rigName( m_config.rearRigType ) );
		DrawTextLine( "skręt %.1f/%.1f st., poślizg %.1f/%.1f st.", 180.0f / B3_PI * frontLeft.steeringAngle,
					  180.0f / B3_PI * frontRight.steeringAngle, 180.0f / B3_PI * frontLeft.slipAngle,
					  180.0f / B3_PI * frontRight.slipAngle );
		DrawTextLine( "kontakt PL:%s PP:%s TL:%s TP:%s", frontLeft.groundContact ? "T" : "-",
					  frontRight.groundContact ? "T" : "-",
					  GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_REAR_LEFT ).groundContact ? "T" : "-",
					  GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_REAR_RIGHT ).groundContact ? "T" : "-" );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehicleM6RigLab( context );
	}

	b3BodyId m_groundId;
	JozzVehicleM5TestCourse m_testCourse;
	JozzVehicleM6 m_vehicle;
	JozzVehicleM6Config m_config;
	JozzVehicleAuditMetadata m_assetMetadata;
	JozzVehicleVisualMesh m_wheelVisual;
	b3Transform m_wheelVisualCorrection;
	JozzVehicleAssetContract m_mountContract;
	JozzVehicleRiggedMesh m_riggedMountL; // authored (left corners)
	JozzVehicleRiggedMesh m_riggedMountR; // mirrored copy (right corners)
	JozzVehicleRiggedMesh m_dumper;		  // telescoping shock, per corner
	bool m_showDumper = true;
	bool m_armTint = false; // debug: tint wishbone arms to compare with debug lines
	bool m_dumpGeometry = false;
	int m_dumpFrame = 0;
	int m_forceTabIndex = -1; // JOZZ_M6_TAB: force a tab open once, for screenshots
	int m_dirtyProbeFrame = 0; // JOZZ_M6_DIRTY_AT_FRAME: tab-identity regression probe

	std::vector<std::string> m_availablePresets;
	int m_selectedPresetIndex = -1;
	char m_presetNameBuffer[64] = "";
	std::string m_presetStatus;
	bool m_testOpenResetModal = false; // JOZZ_M6_TEST_RESET_MODAL: render-verify the confirm popup headless
	b3Vec3 m_mountWheelCenterAuthored = { -0.416f, 0.175f, 0.0f };
	b3Vec3 m_damperUpperLAuthored = { 0.0164f, 0.6453f, 0.2844f };
	b3Vec3 m_damperUpperRAuthored = { 0.0164f, 0.6453f, -0.2844f };
	b3Vec3 m_damperLowerLAuthored = { -0.2516f, 0.0109f, 0.2844f };
	b3Vec3 m_damperLowerRAuthored = { -0.2516f, 0.0109f, -0.2844f };
	b3Transform m_bracketLocal[JOZZ_M6_CORNER_COUNT];
	b3Transform m_hubLocal[JOZZ_M6_CORNER_COUNT];
	bool m_cornerHasMount[JOZZ_M6_CORNER_COUNT] = {};
	JozzVehicleM7TrailingArmImport m_trailingImport;
	JozzVehicleM6DriveInput m_lastInput = {};
	float m_metersPerBlockbenchUnit;
	bool m_savedEnableContinuous;
	bool m_showWheelVisuals;
	bool m_showMountVisuals;
	bool m_showPrimitiveWheelShapes;
	bool m_showRigDiagnostics;
	bool m_invertSteering;

	float m_contactHertz;
	float m_contactDampingRatio;
	float m_contactSpeed;

	// Pending structural edits; require "Apply rig rebuild".
	int m_editFrontRigType;
	int m_editRearRigType;
	JozzVehicleM6WishboneGeometry m_editWishbone;
	JozzVehicleM6TrailingArmGeometry m_editTrailingArm;
	float m_editKnuckleMass;
	float m_editArmMass;
	int m_editEnvelopeMode;
	int m_editEnvelopeLayers;
	float m_editStrutCasterDeg;
	b3Vec3 m_editChassisHalfExtents;
	float m_editChassisDensity;
	float m_editCgVerticalOffset;
	float m_editAxleHalfSpacing;
	float m_editTrackHalfWidth;
	float m_editRestDrop;
	float m_editWheelDensity;
	bool m_structuralSetupDirty;

	static constexpr int TELEMETRY_CAPACITY = 600;
	float m_telemetryTime[TELEMETRY_CAPACITY];
	float m_telemetryTravel[JOZZ_M6_CORNER_COUNT][TELEMETRY_CAPACITY];
	float m_telemetrySlip[JOZZ_M6_CORNER_COUNT][TELEMETRY_CAPACITY];
	int m_telemetryHead;
	int m_telemetryCount;
	float m_telemetryClock;
};

Sample* CreateJozzVehicleM6RigLab( SampleContext* context )
{
	return JozzVehicleM6RigLab::Create( context );
}
