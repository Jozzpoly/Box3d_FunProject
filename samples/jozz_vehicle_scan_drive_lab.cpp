// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_scan_drive_lab.h"

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "jozz_scan_preview_pack.h"
#include "jozz_vehicle_scan_geometry.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_m6_config_io.h"
#include "jozz_vehicle_m6_setup_ui.h"
#include "jozz_vehicle_m6_suspension_rig.h"
#include "jozz_vehicle_m6_visual_skin.h"
#include "sample.h"

#include "box3d/box3d.h"
#include "box3d/collision.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

// P2B Scan Drive (M6): the drive test that proves the leveled scan is real
// ground. It reads the active P2A pack through the pure jozz_scan_pack_geometry
// reader (no textures, no P2A physics boundary), bakes one static BVH mesh per
// tile, tags them JOZZ_M6_TERRAIN_CATEGORY so the M6 split wheel envelope
// collides, spawns the current M6 car by ray-casting the surface for height,
// and drives it. Everything is pack-driven (JOZZ_SCAN_PREVIEW_PACK): importing
// the next scan and re-pinning the env makes this same sample drive it with no
// code change.
namespace
{

// Baking a ~1.8M triangle BVH is the one part of this sample that is brutally
// sensitive to optimization: measured on this pack the bake is 40.4 s unoptimized
// against 0.96 s optimized (~42x), turning a 1.7 s load into a 42 s frozen
// window. Nothing else here is config-sensitive. Worth saying out loud in the HUD.
#if defined( NDEBUG )
constexpr const char* kBuildConfigName = "optimized";
constexpr bool kBuildIsDebug = false;
#else
constexpr const char* kBuildConfigName = "debug";
constexpr bool kBuildIsDebug = true;
#endif

// The M6 workshop's own tuning files, so the car you drive on the scan is the
// car you tuned there rather than a factory-default stranger. These MUST match
// kSessionFilePath / kPresetDirectory in jozz_vehicle_m6_rig_lab_internal.h,
// which is the source of truth; that header is private to the lab, so the
// agreement is held by test_scan_drive_runtime_contract.py instead of #include.
constexpr const char* kM6SessionFilePath = "build/jozz_vehicle_m6_session.json";
constexpr const char* kM6PresetDirectory = "assets/vehicle_presets";

// P2B's OWN persistence, split exactly the way the workshop splits its own and
// for the same reasons:
//
//   * The engine's global "R" reconstructs the sample from scratch. Without a
//     matching save on the way out, every dial edited here is silently thrown
//     away - the precise failure the workshop's session file was invented to
//     stop, reintroduced the moment a second sample grew a tuning UI.
//   * Vehicle tuning and view state live in SEPARATE files because a preset
//     load or "Fabryczne" must be free to replace the whole config without
//     also flipping the user's texture/collision toggles or teleporting them.
//   * A separate session file from the workshop's, so tuning for the scan can
//     never silently overwrite the workshop's saved setup. The first run seeds
//     itself from that file, and the two explicit buttons move tuning between
//     them on demand - importing is a choice, not a side effect.
constexpr const char* kScanSessionFilePath = "build/jozz_scan_drive_session.json";
constexpr const char* kScanViewStatePath = "build/jozz_scan_drive_view.txt";

class JozzVehicleScanDriveLab final : public Sample
{
public:
	explicit JozzVehicleScanDriveLab( SampleContext* context )
		: Sample( context )
	{
		// Fresh boot only: free-orbit framing (leave the T chase cam alone on "R"
		// restart, same rule as the M6 rig lab). The framing itself has to wait
		// until the car exists: unlike the M6 rig lab, whose procedural plate sits
		// at the origin, a scan lives wherever the survey put it - this one spans
		// ~1.2 km and floats 250-400 m up the Y axis. Pointing the camera at the
		// world origin here (as the rig lab does) aims it a quarter kilometre
		// below the terrain and renders a completely empty screen.
		if ( context->restart == false )
		{
			m_camera->m_thirdPerson = false;
			m_frameCameraOnBoot = true;
		}

		// Same CCD decision as the M6 lab: vehicle worlds run without continuous
		// collision (fast rolling wheels otherwise trip TOI validation and no
		// thin geometry here needs it). Restored when the sample closes.
		m_savedEnableContinuous = context->enableContinuous;
		context->enableContinuous = false;
		b3World_SetContactTuning( m_worldId, 30.0f, 10.0f, 3.0f );

		// Restore view/session state FIRST - it carries flipWinding and fastBake,
		// which decide how the ground below is baked. Env hooks override it, so
		// headless runs stay reproducible regardless of what a human left behind.
		LoadViewState();

		// Winding is the one thing the pack bytes can't guarantee for a
		// single-sided collider; flip it if the car falls through (UI checkbox
		// or this env hook for headless runs).
		if ( const char* v = std::getenv( "JOZZ_SCANDRIVE_FLIP_WINDING" ) )
		{
			m_flipWinding = std::atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_SCANDRIVE_FAST_BAKE" ) )
		{
			m_fastBake = std::atoi( v ) != 0;
		}

		BuildScanGround();

		// M6 config: factory baseline first (presets overlay onto it), then the
		// workshop's own saved tuning on top, so the car you drive here is the
		// car you built there.
		m_metadata = LoadJozzVehicleAuditMetadata();
		JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_metadata );
		m_factoryConfig =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		m_config = m_factoryConfig;
		// Our own auto-saved session wins: it is what "R" and reopening the app
		// must restore. Only when it does not exist yet do we seed from the
		// workshop, so the very first drive inherits the car you built there.
		// Partial/in-place is the right load for both: keys a file omits keep
		// the factory value already filled in.
		if ( LoadJozzVehicleM6Config( kScanSessionFilePath, &m_config ) )
		{
			m_settingsSource = "ustawienia: sesja jazdy po skanie (auto-zapis)";
		}
		else if ( LoadJozzVehicleM6Config( kM6SessionFilePath, &m_config ) )
		{
			m_settingsSource = "ustawienia: przejete z sesji warsztatu M6";
		}
		else
		{
			m_settingsSource = "ustawienia: fabryczne";
		}
		SanitizeJozzVehicleM6Config( &m_config );
		m_config.frontRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
		m_config.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
		m_presets = ListJozzVehicleM6Presets( kM6PresetDirectory );
		// The split wheel envelope keys on the terrain category to see the
		// ground as a rolling surface; tag it the same as our mesh shapes.
		m_config.wheelEnvelope.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;
		m_edit.SyncFromConfig( m_config );

		// Jozz's real model instead of the debug box + cylinders. Loaded once
		// here; CreateVehicle re-hides the collision primitives underneath it.
		m_metersPerBlockbenchUnit = defaults.metersPerBlockbenchUnit;
		m_skin.Load( m_config.bodyVisualModel, m_metersPerBlockbenchUnit, m_metadata );

		// Only fall back to the scan centre when there is no remembered drop
		// point - otherwise "R" would teleport you off your test spot.
		if ( m_hasSavedSpawn == false )
		{
			ResetSpawnAnchorToScanCentre();
		}
		if ( const char* v = std::getenv( "JOZZ_SCANDRIVE_SPAWN_XZ" ) )
		{
			float x = 0.0f, z = 0.0f;
			if ( std::sscanf( v, "%f,%f", &x, &z ) == 2 )
			{
				m_spawnAnchorX = x;
				m_spawnAnchorZ = z;
			}
		}

		if ( m_groundValid )
		{
			CreateVehicle();
		}

		// Now that the car has a position on the scan, put the camera on it.
		// Without this the sample opens on empty sky (see the constructor note).
		if ( m_frameCameraOnBoot )
		{
			FrameCameraOnCar();
		}

		// Headless / --screenshot env hooks, mirroring the M6 registry so the
		// same automation drives future scans with no human at the keyboard.
		if ( const char* v = std::getenv( "JOZZ_SCANDRIVE_CAM" ) )
		{
			float y = -135, p = 14, r = 13, px = 0, py = 1.2f, pz = 0;
			if ( std::sscanf( v, "%f,%f,%f,%f,%f,%f", &y, &p, &r, &px, &py, &pz ) >= 3 )
			{
				m_camera->SetView( y, p, r, { px, py, pz } );
			}
		}
		if ( const char* v = std::getenv( "JOZZ_SCANDRIVE_AUTODRIVE" ) )
		{
			m_autoDrive = std::atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_SCANDRIVE_SETTLE_DUMP" ) )
		{
			m_settleDumpAtStep = std::atoi( v );
		}
	}

	~JozzVehicleScanDriveLab() override
	{
		// The matching half of the constructor's restore. SelectSample deletes
		// the old sample BEFORE constructing the new one (sample.cpp), so this
		// always lands before the reload - which is what makes "R" keep the
		// tuning instead of wiping it.
		SaveJozzVehicleM6Config( m_config, kScanSessionFilePath );
		SaveViewState();

		DestroyJozzVehicleM6( &m_vehicle );
		m_skin.Destroy();
		DestroyScanGround(); // frees the app-owned mesh data (shapes only held references)
		m_context->enableContinuous = m_savedEnableContinuous;
	}

	// "F" is the recover-the-car key. Framing the whole scan instead would zoom
	// out to a 1.2 km box in which the car is a sub-pixel speck, which is the
	// opposite of useful while driving. The whole-scan view is still one press
	// away on the no-selection path below.
	bool FocusBounds( b3AABB* bounds ) override
	{
		if ( m_vehicle.valid == false )
		{
			return false;
		}
		*bounds = b3Body_ComputeAABB( m_vehicle.chassisId );
		return true;
	}

	// "F" with no car (load failed, or it was destroyed): fit the whole scan.
	void FocusHome() override
	{
		if ( m_groundValid == false )
		{
			Sample::FocusHome();
			return;
		}
		float aspect = m_context->windowHeight > 0
						   ? (float)m_context->windowWidth / (float)m_context->windowHeight
						   : 16.0f / 9.0f;
		m_camera->Frame( m_scanBounds, aspect, 1.25f );
	}

	bool CondenseDebugOverlay() const override
	{
		return true;
	}

	// The ported M6 setup tabs need more room than the default panel: at the
	// stock width the tab bar scrolls and "Kierownica" hides behind an arrow.
	float InfoPanelWidthEm() const override
	{
		return 30.0f;
	}

	void Keyboard( int key, int action, int modifiers ) override
	{
		if ( key == KEY_T && action == ACTION_PRESS )
		{
			ToggleThirdPerson();
		}
		Sample::Keyboard( key, action, modifiers );
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "P2B Scan Drive (M6)" );
		ImGui::Separator();
		ImGui::TextColored( ImVec4( 0.4f, 0.85f, 0.5f, 1.0f ), "Static scan mesh = collision ground" );
		ImGui::TextWrapped( "%s", m_status.c_str() );
		if ( m_packName.empty() == false )
		{
			ImGui::Text( "Pack: %s", m_packName.c_str() );
		}

		if ( m_groundValid )
		{
			ImGui::Text( "Tile meshes: %d", (int)m_tileMeshes.size() );
			ImGui::Text( "Triangles: %d", m_triangleCount );
			ImGui::Text( "Degenerate: %d", m_degenerateCount );
			ImGui::Text( "Spawn: (%.1f, %.1f)", m_spawnAnchorX, m_spawnAnchorZ );
			ImGui::Text( "Ladowanie: %.1fs (odczyt %.1f + BVH %.1f + tekstury %.1f)",
						 m_readSeconds + m_bakeSeconds + m_textureSeconds, m_readSeconds, m_bakeSeconds,
						 m_textureSeconds );
			if ( kBuildIsDebug )
			{
				ImGui::TextColored( ImVec4( 1.0f, 0.55f, 0.2f, 1.0f ), "Build DEBUG - pieczenie BVH ~42x wolniej." );
				ImGui::TextWrapped( "Do jazdy buduj Release: cmake --build build --config Release --target samples" );
			}
			ImGui::Spacing();

			if ( ImGui::Button( "Kamera na auto" ) )
			{
				FrameCameraOnCar();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Kamera na caly skan" ) )
			{
				FocusHome();
			}
			ImGui::Spacing();

			if ( ImGui::Checkbox( "Model 3D auta", &m_showCarModel ) )
			{
				ApplyCarVisibility();
			}
			if ( m_skin.status.empty() == false )
			{
				ImGui::TextDisabled( "%s", m_skin.status.c_str() );
			}
			ImGui::Checkbox( "Tekstury skanu", &m_showTextures );
			if ( ImGui::Checkbox( "Siatka kolizji", &m_showCollisionMesh ) )
			{
				ApplyGroundVisibility();
			}
			ImGui::Spacing();

			// Live winding flip: rebuild the ground + re-drop the car. This is
			// the empirical "did it fall through?" fix if the pack winding is
			// inverted for the single-sided collider.
			if ( ImGui::Checkbox( "Odwroc nawijanie trojkatow (flip winding)", &m_flipWinding ) )
			{
				RebuildGroundAndCar();
			}
			if ( ImGui::Checkbox( "Szybkie pieczenie BVH (median split)", &m_fastBake ) )
			{
				RebuildGroundAndCar();
			}
			ImGui::Spacing();

			DrawCarSetupSection();
			if ( ImGui::Button( "Upusc auto na srodku skanu" ) )
			{
				ResetSpawnAnchorToScanCentre();
				CreateVehicle();
				FrameCameraOnCar();
			}
		}
		else
		{
			ImGui::Spacing();
			ImGui::TextWrapped( "Zbuduj dokladnie jeden pack (scan_oneshot.py) albo ustaw JOZZ_SCAN_PREVIEW_PACK "
								"przed uruchomieniem samples.exe." );
		}
		return true;
	}

	// The tuning that means something while driving a scan: which setup the car
	// wears, and the handful of dials you reach for after a lap. The workshop
	// keeps the full editor (arm geometry, chassis construction, terrain
	// generator, rig diagnostics) - this is the driving-seat subset, and it
	// reads/writes the SAME config and preset files, so the two agree.
	void DrawCarSetupSection()
	{
		if ( ImGui::CollapsingHeader( "Ustawienia auta (M6)", ImGuiTreeNodeFlags_DefaultOpen ) == false )
		{
			return;
		}

		ImGui::TextDisabled( "%s", m_settingsSource.c_str() );

		if ( m_presets.empty() == false )
		{
			const char* current = m_presetIndex >= 0 ? m_presets[m_presetIndex].c_str() : "(wybierz)";
			if ( ImGui::BeginCombo( "Preset", current ) )
			{
				for ( int i = 0; i < (int)m_presets.size(); ++i )
				{
					bool selected = i == m_presetIndex;
					if ( ImGui::Selectable( m_presets[i].c_str(), selected ) )
					{
						LoadPreset( i );
					}
				}
				ImGui::EndCombo();
			}
		}

		if ( ImGui::Button( "Wczytaj sesje warsztatu" ) )
		{
			m_config = m_factoryConfig;
			m_settingsSource = LoadJozzVehicleM6Config( kM6SessionFilePath, &m_config )
								   ? "ustawienia: sesja warsztatu M6"
								   : "ustawienia: brak sesji warsztatu";
			m_presetIndex = -1;
			m_config.frontRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
			m_config.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
			m_config.wheelEnvelope.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;
			ApplyConfigAndRebuildCar();
		}
		ImGui::SameLine();
		// The only path that writes the workshop's file, and it is a button -
		// never a side effect of driving here.
		if ( ImGui::Button( "Zapisz do warsztatu" ) )
		{
			m_settingsSource = SaveJozzVehicleM6Config( m_config, kM6SessionFilePath )
								   ? "ustawienia: zapisane do sesji warsztatu M6"
								   : "ustawienia: NIE udalo sie zapisac do warsztatu";
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Fabryczne" ) )
		{
			m_config = m_factoryConfig;
			m_config.frontRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
			m_config.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
			m_config.wheelEnvelope.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;
			m_presetIndex = -1;
			m_settingsSource = "ustawienia: fabryczne";
			ApplyConfigAndRebuildCar();
		}

		ImGui::Spacing();

		// The workshop's own tabs, verbatim from the shared setup-UI module -
		// same sliders, same help text, same live/structural split. Its World
		// and Debug tabs are deliberately absent: they tune a procedural
		// terrain generator and the rig lab's diagnostics, neither of which
		// exists here (the world IS the scan).
		bool live = false;
		ImGui::PushItemWidth( 9.0f * ImGui::GetFontSize() );
		if ( ImGui::BeginTabBar( "ScanDriveSetupTabs", ImGuiTabBarFlags_None ) )
		{
			// "###TabX" pins tab identity so appending " *" on a pending edit
			// does not read as a different tab and bounce the selection.
			if ( ImGui::BeginTabItem( m_edit.structuralDirty ? "Zawieszenie *###TabSuspension"
															 : "Zawieszenie###TabSuspension" ) )
			{
				live |= DrawJozzVehicleM6SuspensionTab( &m_config, &m_edit );
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( m_edit.structuralDirty ? "Nadwozie *###TabChassis" : "Nadwozie###TabChassis" ) )
			{
				live |= DrawJozzVehicleM6ChassisTab( &m_config, &m_edit );
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Naped" ) )
			{
				live |= DrawJozzVehicleM6DriveTab( &m_config );
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Kierownica" ) )
			{
				live |= DrawJozzVehicleM6SteeringTab( &m_config, &m_edit );
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::PopItemWidth();

		// Visual-only: a new body skin needs a reload, not a rebuild.
		if ( m_edit.bodyModelChanged )
		{
			m_skin.Load( m_config.bodyVisualModel, m_metersPerBlockbenchUnit, m_metadata );
			m_skin.AttachToVehicle( m_vehicle, m_metadata );
			ApplyCarVisibility();
			m_edit.bodyModelChanged = false;
		}

		// Live dials bite immediately; geometry waits for Apply.
		if ( live )
		{
			ApplyJozzVehicleM6LiveTuning( m_vehicle, m_config );
		}

		if ( m_edit.structuralDirty )
		{
			ImGui::Separator();
			ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ), "Sa niezastosowane zmiany geometrii" );
			if ( ImGui::Button( "Zastosuj (przebuduj auto)" ) )
			{
				m_edit.ApplyToConfig( &m_config );
				ApplyConfigAndRebuildCar();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Odrzuc" ) )
			{
				m_edit.SyncFromConfig( m_config );
			}
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
		if ( m_edit.invertSteering )
		{
			input.steer = -input.steer;
		}

		if ( m_autoDrive )
		{
			input.drive = 1.0f;
			input.brake = false;
		}

		if ( m_vehicle.valid )
		{
			UpdateJozzVehicleM6Drive( m_vehicle, input );
		}

		if ( m_camera->m_thirdPerson && m_vehicle.valid )
		{
			b3WorldTransform transform = b3Body_GetTransform( m_vehicle.chassisId );
			m_camera->m_pivot = transform.p;
			m_camera->UpdateTransform();
		}

		Sample::Step();

		MaybeSettleDump();
	}

	void Render() override
	{
		Sample::Render();

		if ( m_showTextures && m_visualPack.loaded )
		{
			m_visualPack.Draw( false );
		}

		if ( m_showCarModel )
		{
			m_skin.Draw( m_vehicle );
		}

		DrawTextLine( "P2B Scan Drive (M6) - W/S jazda, A/D skret, Spacja hamulec, T kamera z auta, F wroc do auta" );
		DrawTextLine( "%s", m_status.c_str() );

		if ( m_vehicle.valid )
		{
			float speed = GetJozzVehicleM6ForwardSpeed( m_vehicle );
			DrawTextLine( "predkosc %.1f m/s (%.0f km/h)  kontakt PL:%s PP:%s TL:%s TP:%s", speed, 3.6f * speed,
						  WheelContact( JOZZ_M6_FRONT_LEFT ), WheelContact( JOZZ_M6_FRONT_RIGHT ),
						  WheelContact( JOZZ_M6_REAR_LEFT ), WheelContact( JOZZ_M6_REAR_RIGHT ) );
		}
	}

private:
	const char* WheelContact( int corner ) const
	{
		return GetJozzVehicleM6WheelTelemetry( m_vehicle, corner ).groundContact ? "T" : "-";
	}

	// Orbit the car where it actually stands on the scan. Falls back to the scan
	// centre so a car-less load (bad pack) still shows the terrain rather than
	// empty space.
	void FrameCameraOnCar()
	{
		b3Pos pivot = { m_spawnAnchorX, 0.0f, m_spawnAnchorZ };
		if ( m_vehicle.valid )
		{
			pivot = b3Body_GetTransform( m_vehicle.chassisId ).p;
		}
		else if ( m_groundValid )
		{
			pivot.y = 0.5 * ( m_scanBounds.lowerBound.y + m_scanBounds.upperBound.y );
		}
		else
		{
			return;
		}
		m_camera->SetView( -135.0f, 18.0f, 22.0f, pivot );
	}

	void ResetSpawnAnchorToScanCentre()
	{
		if ( m_groundValid )
		{
			m_spawnAnchorX = 0.5f * ( m_scanBounds.lowerBound.x + m_scanBounds.upperBound.x );
			m_spawnAnchorZ = 0.5f * ( m_scanBounds.lowerBound.z + m_scanBounds.upperBound.z );
		}
	}

	// Cast a ray straight down through the whole scan height range and return the
	// surface Y at (x, z), or fallback if the ray misses (a hole in the mesh).
	float SampleScanGroundHeight( float x, float z, float fallback ) const
	{
		b3QueryFilter filter = b3DefaultQueryFilter();
		filter.maskBits = JOZZ_M6_TERRAIN_CATEGORY; // only the scan ground, never the car
		float top = m_scanBounds.upperBound.y + 50.0f;
		float span = ( m_scanBounds.upperBound.y - m_scanBounds.lowerBound.y ) + 100.0f;
		b3Pos origin = { x, top, z };
		b3Vec3 translation = { 0.0f, -span, 0.0f };
		b3RayResult result = b3World_CastRayClosest( m_worldId, origin, translation, filter );
		return result.hit ? (float)result.point.y : fallback;
	}

	void BuildScanGround()
	{
		std::filesystem::path packDir = FindJozzActiveScanPreviewPack();
		if ( packDir.empty() )
		{
			m_status = "scan drive: no pack selected; build exactly one pack or set JOZZ_SCAN_PREVIEW_PACK";
			return;
		}
		m_packName = packDir.filename().string();

		using Clock = std::chrono::steady_clock;
		auto seconds = []( Clock::time_point a, Clock::time_point b ) {
			return std::chrono::duration<float>( b - a ).count();
		};
		Clock::time_point readStart = Clock::now();

		std::vector<JozzScanTileGeometry> tiles;
		std::string error;
		if ( LoadJozzScanPackGeometry( packDir, &tiles, &error, m_flipWinding ) == false )
		{
			m_status = "scan drive: " + error;
			return;
		}
		m_readSeconds = seconds( readStart, Clock::now() );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.name = "scan_ground"; // static by default; geometry already carries world positions
		bodyDef.position = { 0.0f, 0.0f, 0.0f };
		m_groundBodyId = b3CreateBody( m_worldId, &bodyDef );
		m_groundBodyValid = true;

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.baseMaterial.friction = 0.9f;
		shapeDef.filter.categoryBits = JOZZ_M6_TERRAIN_CATEGORY;

		int triangleTotal = 0;
		int degenerateTotal = 0;
		bool boundsInit = false;
		Clock::time_point bakeStart = Clock::now();
		for ( JozzScanTileGeometry& tile : tiles )
		{
			if ( tile.positions.size() < 3 || tile.indices.size() < 3 )
			{
				continue;
			}

			// b3CreateMesh COPIES the geometry into a self-contained BVH blob
			// (b3MeshData holds its own vertex/triangle arrays), so the input
			// vectors need not outlive this call - only the returned mesh must,
			// which we keep for teardown.
			b3MeshDef def = {};
			def.vertices = tile.positions.data();
			def.vertexCount = (int)tile.positions.size();
			def.indices = tile.indices.data();
			def.triangleCount = (int)( tile.indices.size() / 3 );
			def.materialIndices = nullptr; // single material for the whole scan
			def.weldTolerance = 0.0f;
			def.weldVertices = false; // welding at mm scale can collapse thin walls; off until proven needed
			// Binned SAH builds the better tree and is what a driven scan wants.
			// Median split is the escape hatch when the bake itself is the thing
			// hurting (an unoptimized build, or a much larger future scan).
			def.useMedianSplit = m_fastBake;
			def.identifyEdges = false;

			b3MeshData* mesh = b3CreateMesh( &def, nullptr, 0 );
			if ( mesh == nullptr )
			{
				continue;
			}
			m_tileMeshes.push_back( mesh );

			b3ShapeId shapeId = b3CreateMeshShape( m_groundBodyId, &shapeDef, mesh, b3Vec3_one );
			m_tileShapeIds.push_back( shapeId );
			triangleTotal += def.triangleCount;
			degenerateTotal += mesh->degenerateCount;

			// The debug renderer's procedural grid keys on a single shape id;
			// register the first good tile (same convention as BuildPlate).
			if ( m_groundShapeRegistered == false )
			{
				SetGroundShape( shapeId );
				m_groundShapeRegistered = true;
			}

			if ( boundsInit == false )
			{
				m_scanBounds = tile.bounds;
				boundsInit = true;
			}
			else
			{
				m_scanBounds.lowerBound.x = std::min( m_scanBounds.lowerBound.x, tile.bounds.lowerBound.x );
				m_scanBounds.lowerBound.y = std::min( m_scanBounds.lowerBound.y, tile.bounds.lowerBound.y );
				m_scanBounds.lowerBound.z = std::min( m_scanBounds.lowerBound.z, tile.bounds.lowerBound.z );
				m_scanBounds.upperBound.x = std::max( m_scanBounds.upperBound.x, tile.bounds.upperBound.x );
				m_scanBounds.upperBound.y = std::max( m_scanBounds.upperBound.y, tile.bounds.upperBound.y );
				m_scanBounds.upperBound.z = std::max( m_scanBounds.upperBound.z, tile.bounds.upperBound.z );
			}
		}

		m_bakeSeconds = seconds( bakeStart, Clock::now() );

		m_groundValid = m_tileMeshes.empty() == false;
		m_triangleCount = triangleTotal;
		m_degenerateCount = degenerateTotal;
		if ( m_groundValid )
		{
			char message[192];
			std::snprintf( message, sizeof( message ), "scan drive: %d tile meshes, %d triangles, %d degenerate",
						   (int)m_tileMeshes.size(), triangleTotal, degenerateTotal );
			m_status = message;
		}
		else
		{
			m_status = "scan drive: pack loaded but no drivable geometry";
		}

		Clock::time_point textureStart = Clock::now();
		if ( m_groundValid )
		{
			// Load the textured render of the SAME pack. Costs a second parse,
			// but it guarantees the visible surface and the collision surface
			// are the identical geometry rather than two things kept in sync.
			m_visualPack.Load( packDir );
		}
		ApplyGroundVisibility();
		m_textureSeconds = seconds( textureStart, Clock::now() );

		// Loading blocks the whole app, so every future scan should report what
		// it cost. Launch from a terminal and this line is the answer to "is it
		// hung or just slow?", and the number to compare across build configs.
		std::printf( "JOZZ_SCANDRIVE_LOAD read=%.2fs bake=%.2fs textures=%.2fs total=%.2fs tris=%d build=%s\n",
					 m_readSeconds, m_bakeSeconds, m_textureSeconds,
					 m_readSeconds + m_bakeSeconds + m_textureSeconds, m_triangleCount, kBuildConfigName );
		std::fflush( stdout );
	}

	// With textures on, the collision mesh's debug wireframe would just sit on
	// top of the texture, so it is hidden unless explicitly asked for.
	void ApplyGroundVisibility()
	{
		for ( b3ShapeId shapeId : m_tileShapeIds )
		{
			SetShapeHidden( shapeId, m_showCollisionMesh == false );
		}
	}

	void DestroyScanGround()
	{
		if ( m_groundBodyValid )
		{
			b3DestroyBody( m_groundBodyId ); // also destroys its mesh shapes
			m_groundBodyValid = false;
		}
		for ( b3MeshData* mesh : m_tileMeshes )
		{
			if ( mesh != nullptr )
			{
				b3DestroyMesh( mesh );
			}
		}
		m_tileMeshes.clear();
		m_tileShapeIds.clear();
		m_visualPack.Destroy();
		m_groundValid = false;
		m_groundShapeRegistered = false;
		m_triangleCount = 0;
		m_degenerateCount = 0;
	}

	void CreateVehicle()
	{
		DestroyJozzVehicleM6( &m_vehicle );
		if ( m_groundValid == false )
		{
			return;
		}

		// Sample the surface under all four wheel positions (plus padding) and
		// take the max, so on a slope no uphill wheel starts buried - the same
		// logic the M6 rig lab uses against its analytic heightfield, but here
		// via the mesh ray cast.
		float footX = m_config.axleHalfSpacing + 0.3f;
		float footZ = m_config.trackHalfWidth + 0.2f;
		float fallback = m_scanBounds.upperBound.y;
		float groundY = SampleScanGroundHeight( m_spawnAnchorX, m_spawnAnchorZ, fallback );
		groundY = b3MaxFloat( groundY, SampleScanGroundHeight( m_spawnAnchorX + footX, m_spawnAnchorZ + footZ, fallback ) );
		groundY = b3MaxFloat( groundY, SampleScanGroundHeight( m_spawnAnchorX + footX, m_spawnAnchorZ - footZ, fallback ) );
		groundY = b3MaxFloat( groundY, SampleScanGroundHeight( m_spawnAnchorX - footX, m_spawnAnchorZ + footZ, fallback ) );
		groundY = b3MaxFloat( groundY, SampleScanGroundHeight( m_spawnAnchorX - footX, m_spawnAnchorZ - footZ, fallback ) );

		float clearance = m_config.restDrop + m_config.wheelEnvelope.radius + 0.30f;
		m_vehicle = CreateJozzVehicleM6( m_worldId, m_groundBodyId, m_config,
										 { m_spawnAnchorX, groundY + clearance, m_spawnAnchorZ } );
		// Bake the suspension rig against the car at rest, before it moves.
		m_skin.AttachToVehicle( m_vehicle, m_metadata );
		ApplyCarVisibility();
	}

	// The chassis box and wheel cylinders are what the debug renderer draws; with
	// the model on they sit inside it, so hide them (same SetShapeHidden pattern
	// the M6 workshop uses). Turning the skin off brings them straight back, which
	// is what you want when checking what the physics actually is.
	void ApplyCarVisibility()
	{
		if ( m_vehicle.valid == false )
		{
			return;
		}
		bool skinCovers = m_showCarModel && m_skin.IsLoaded();
		SetShapeHidden( m_vehicle.chassisShapeId, skinCovers && m_skin.body.IsLoaded() );
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			for ( int i = 0; i < runtime.wheelShapeCount; ++i )
			{
				SetShapeHidden( runtime.wheelShapeIds[i], skinCovers && m_skin.wheel.IsLoaded() );
			}
		}
	}

	// View toggles + where the car was dropped. Deliberately NOT the vehicle
	// config: a preset load or "Fabryczne" replaces the whole car, and must not
	// take the user's texture/collision toggles or their spawn point with it.
	void SaveViewState() const
	{
		std::error_code ec;
		std::filesystem::create_directories( std::filesystem::path( kScanViewStatePath ).parent_path(), ec );
		std::ofstream file( kScanViewStatePath, std::ios::binary | std::ios::trunc );
		if ( file.is_open() == false )
		{
			return;
		}
		file << "showCarModel=" << ( m_showCarModel ? 1 : 0 ) << "\n";
		file << "showTextures=" << ( m_showTextures ? 1 : 0 ) << "\n";
		file << "showCollisionMesh=" << ( m_showCollisionMesh ? 1 : 0 ) << "\n";
		file << "flipWinding=" << ( m_flipWinding ? 1 : 0 ) << "\n";
		file << "fastBake=" << ( m_fastBake ? 1 : 0 ) << "\n";
		file << "invertSteering=" << ( m_edit.invertSteering ? 1 : 0 ) << "\n";
		// Checkpoint semantics, same rule the workshop settled on: "R" must
		// re-drop the car WHERE YOU WERE TESTING, not silently back at the scan
		// centre a kilometre away.
		file << "spawnAnchorX=" << m_spawnAnchorX << "\n";
		file << "spawnAnchorZ=" << m_spawnAnchorZ << "\n";
	}

	// Must run BEFORE BuildScanGround: flipWinding and fastBake change how the
	// collision mesh is baked, so restoring them afterwards would leave the
	// checkboxes disagreeing with the ground actually in the world.
	void LoadViewState()
	{
		std::ifstream file( kScanViewStatePath );
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
			std::string raw = line.substr( eq + 1 );
			bool on = raw == "1";
			if ( key == "showCarModel" ) m_showCarModel = on;
			else if ( key == "showTextures" ) m_showTextures = on;
			else if ( key == "showCollisionMesh" ) m_showCollisionMesh = on;
			else if ( key == "flipWinding" ) m_flipWinding = on;
			else if ( key == "fastBake" ) m_fastBake = on;
			else if ( key == "invertSteering" ) m_edit.invertSteering = on;
			else if ( key == "spawnAnchorX" ) { m_spawnAnchorX = std::strtof( raw.c_str(), nullptr ); m_hasSavedSpawn = true; }
			else if ( key == "spawnAnchorZ" ) { m_spawnAnchorZ = std::strtof( raw.c_str(), nullptr ); m_hasSavedSpawn = true; }
		}
	}

	// A config change can swap the body skin too (a preset describes the WHOLE
	// car), and geometry changes need a fresh vehicle - so reload the skin and
	// rebuild the car. The scan ground and its BVH are untouched, which is why
	// this is cheap enough to hang off a button.
	void ApplyConfigAndRebuildCar()
	{
		SanitizeJozzVehicleM6Config( &m_config );
		m_edit.SyncFromConfig( m_config );
		m_skin.Load( m_config.bodyVisualModel, m_metersPerBlockbenchUnit, m_metadata );
		CreateVehicle();
	}

	void LoadPreset( int index )
	{
		if ( index < 0 || index >= (int)m_presets.size() )
		{
			return;
		}
		std::string path = std::string( kM6PresetDirectory ) + "/" + m_presets[index] + ".json";
		// Deterministic preset semantics: factory defaults overridden by the
		// keys the preset actually sets - never in-place over current values.
		if ( LoadJozzVehicleM6PresetConfig( path, m_factoryConfig, &m_config ) )
		{
			m_presetIndex = index;
			m_settingsSource = "ustawienia: preset " + m_presets[index];
			m_config.frontRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
			m_config.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
			m_config.wheelEnvelope.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;
			ApplyConfigAndRebuildCar();
		}
		else
		{
			m_settingsSource = "ustawienia: nie udalo sie wczytac presetu " + m_presets[index];
		}
	}

	void RebuildGroundAndCar()
	{
		DestroyJozzVehicleM6( &m_vehicle );
		DestroyScanGround();
		BuildScanGround();
		ResetSpawnAnchorToScanCentre();
		if ( m_groundValid )
		{
			CreateVehicle();
			FrameCameraOnCar();
		}
	}

	// Headless proof: print car Y vs ray-cast ground Y and per-wheel contact at
	// a chosen step, so an automated run can assert the car settled ON the scan
	// surface and did not fall through.
	void MaybeSettleDump()
	{
		if ( m_settleDumpAtStep <= 0 || m_stepCount < m_settleDumpAtStep || m_vehicle.valid == false )
		{
			return;
		}
		b3WorldTransform transform = b3Body_GetTransform( m_vehicle.chassisId );
		float carX = (float)transform.p.x;
		float carY = (float)transform.p.y;
		float carZ = (float)transform.p.z;
		float groundY = SampleScanGroundHeight( carX, carZ, m_scanBounds.lowerBound.y );
		int contacts = 0;
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			if ( GetJozzVehicleM6WheelTelemetry( m_vehicle, corner ).groundContact )
			{
				contacts += 1;
			}
		}
		std::printf( "JOZZ_SCANDRIVE_SETTLE step=%d car=(%.2f, %.3f, %.2f) ground_y=%.3f above=%.3f wheel_contacts=%d/4 "
					 "tris=%d degenerate=%d\n",
					 m_stepCount, carX, carY, carZ, groundY, carY - groundY, contacts, m_triangleCount, m_degenerateCount );
		std::fflush( stdout );
		m_settleDumpAtStep = -1; // once
	}

	bool m_savedEnableContinuous = true;
	JozzVehicleAuditMetadata m_metadata = {};
	JozzVehicleM6Config m_config = {};
	JozzVehicleM6 m_vehicle = {};

	b3BodyId m_groundBodyId = b3_nullBodyId;
	bool m_groundBodyValid = false;
	std::vector<b3MeshData*> m_tileMeshes;
	std::vector<b3ShapeId> m_tileShapeIds;
	b3AABB m_scanBounds = {};
	bool m_groundValid = false;
	bool m_groundShapeRegistered = false;
	bool m_flipWinding = false;
	bool m_fastBake = false;
	bool m_frameCameraOnBoot = false;
	bool m_hasSavedSpawn = false;
	float m_readSeconds = 0.0f;
	float m_bakeSeconds = 0.0f;
	float m_textureSeconds = 0.0f;

	// Textured visuals come from the SAME pack bytes through the render-only
	// P2A path, so the texture lands exactly on the collision surface. We are
	// only a consumer here - P2A itself stays physics-free.
	JozzScanPreviewPack m_visualPack;
	JozzVehicleM6VisualSkin m_skin;
	JozzVehicleM6Config m_factoryConfig = {};
	std::vector<std::string> m_presets;
	std::string m_settingsSource;
	int m_presetIndex = -1;
	JozzVehicleM6SetupEdit m_edit;
	float m_metersPerBlockbenchUnit = 0.0f;
	bool m_showCarModel = true;
	bool m_showTextures = true;
	bool m_showCollisionMesh = false;
	int m_triangleCount = 0;
	int m_degenerateCount = 0;

	std::string m_status = "scan drive: not loaded";
	std::string m_packName;
	float m_spawnAnchorX = 0.0f;
	float m_spawnAnchorZ = 0.0f;

	bool m_autoDrive = false;
	int m_settleDumpAtStep = 0;
};

} // namespace

Sample* CreateJozzVehicleScanDriveLab( SampleContext* context )
{
	return new JozzVehicleScanDriveLab( context );
}
