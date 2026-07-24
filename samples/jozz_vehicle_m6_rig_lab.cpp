// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_rig_lab_internal.h"

JozzVehicleM6RigLab::JozzVehicleM6RigLab( SampleContext* context )
		: Sample( context )
	{
		// Fresh boot only: force the default free-orbit framing. On "R" restart
		// leave both alone - the T chase cam (if it was on) should survive the
		// restart instead of dropping back to a static orbit (Jozz's feedback:
		// pressing R while driving with T made the camera look "reset").
		if ( context->restart == false )
		{
			m_camera->m_thirdPerson = false;
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

		// Mapa Etap 1 (docs/MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md): a 400x400 m,
		// 3x3-tile plate plus a 400x400 m offroad heightfield chunk (ridged/warp
		// noise + a central mountain) that dips under its east edge. Tagged with
		// the terrain category so the split wheel envelope can separate rolling
		// contact (sphere vs terrain) from side contact (true-width cylinder vs
		// the rest).
		// Ramps/washboard/props from the test course get the same tag; props
		// do not.
		m_worldGround = CreateJozzWorldGround( m_worldId, JOZZ_M6_TERRAIN_CATEGORY );
		m_groundId = m_worldGround.plateBodyId;
		m_worldSeedInput = (int)m_worldGround.seed;
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

		// The lab's FACTORY state: engine defaults + everything the constructor
		// wires in above (asset-derived wheel dims, trailing-arm contract
		// geometry, both axles on wishbone). Stashed BEFORE the session load so
		// there is exactly one source of truth for "fabryczne ustawienia" -
		// preset loads overlay onto THIS (deterministic restore) and the reset
		// button restores THIS, instead of each site rebuilding its own idea of
		// the baseline.
		m_factoryConfig = m_config;

		// Restore whatever tuning was in effect last time this lab ran, if any.
		// Without this, the engine's global "R" restart shortcut - and simply
		// closing and reopening samples.exe - silently discarded every dial back
		// to JozzVehicleM6DefaultConfig with no warning (found while auditing the
		// preset workflow: restarting to re-drop the car for a fresh test was
		// quietly destroying the tuning session). The destructor writes this
		// file on the way out, so it always reflects the last config in memory.
		if ( LoadJozzVehicleM6Config( kSessionFilePath, &m_config ) )
		{
			// Defensive: the session file is hand-editable JSON; sanitize before
			// deriving anything from it (P6 - also closes the P5 gap where the
			// max-steer dead-point clamp only guarded the UI Apply path).
			SanitizeJozzVehicleM6Config( &m_config );
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
		// Also restores the CHECKPOINT: last teleport anchor + terrain seed
		// (Jozz's feedback - "R" was teleporting him back to Start and quietly
		// swapping the regenerated terrain for the default-seed one).
		LoadDebugViewState();
		// Per-fragment persistent spawns (committed assets/vehicle_spawns.txt) into
		// memory. Does NOT move the car: boot keeps the last checkpoint (m_spawnAnchor
		// restored above); a fragment's default spawn is an explicit target the Mapa
		// UI teleports to on demand.
		LoadFragmentSpawns();
		if ( (uint32_t)m_worldSeedInput != m_worldGround.seed )
		{
			RegenerateTerrain(); // before CreateVehicle: spawn height must sample the checkpoint's terrain
		}

		m_contactHertz = 30.0f;
		m_contactDampingRatio = 10.0f;
		m_contactSpeed = 3.0f;

		m_telemetryHead = 0;
		m_telemetryCount = 0;
		m_telemetryClock = 0.0f;

		m_vehicle = {};
		LoadWheelVisual();
		LoadMountVisual();
		LoadSteeringRig();		   // new front rig (rig-editor warm-up G1); needed before CreateVehicle bakes it
		ApplyBodyVisualFromConfig(); // body skin named by m_config.bodyVisualModel (already loaded/sanitized above)
		CreateVehicle(); // sets up the per-corner mount rig, needs the visuals loaded
		SyncEditFromConfig();

		// JOZZ_SCAN_AUTOLOAD (headless --screenshot aid, fundament v3): load the
		// scan island from JOZZ_SCAN_PREVIEW_PACK and teleport onto it on boot, so
		// a headless run can verify the scan physics without clicking the Mapa tab.
		if ( const char* scanAuto = std::getenv( "JOZZ_SCAN_AUTOLOAD" ) )
		{
			if ( atoi( scanAuto ) != 0 )
			{
				LoadScanTile();
				TeleportToScan();
			}
		}

		// --- JOZZ_M6_* env registry (headless testing / --screenshot / the
		// future rig editor's headless render) --------------------------------
		// The complete, current set - keep this list in sync when adding/removing
		// a hook (grep 'getenv( "JOZZ_M6' must match this comment):
		//   DIAG      0/1  show suspension diagnostic lines
		//   WHEEL     0/1  show the 3D wheel model
		//   MOUNT     0/1  show the 3D suspension mount model
		//   STEERING  0/1  draw the NEW front steering rig instead of the old mount (front only)
		//   BODY      0/1  draw the selected body skin as a rigid skin (view toggle)
		//   BODY_MODEL key  select the body skin by registry key (e.g. rama_rurowa)
		//   COLLIDER  0/1  force the chassis collision box visible on top of the skin
		//   DUMPER    0/1  show the telescoping dampers
		//   ARMTINT   0/1  tint arms (Top red / Bottom blue) vs the debug lines
		//   DUMP      0/1  print corner geometry numbers on a frame (hard data)
		//   CAM       "yaw,pitch,radius,px,py,pz"  camera pose
		//   HERTZ     float  live suspension spring hertz
		//   DAMP      float  live suspension damping ratio
		//   PRELOAD   float  ride-height preload (sets BOTH axles; sliders split them)
		//   DROOP     float  arm rest droop degrees (needs a rebuild)
		//   TAB       0-5  force one tab open for a frame (Zaw/Nad/Nap/Kier/Świat/Debug)
		//   PRESET    name  load a named preset on boot
		//   TELEPORT  name  move the spawn anchor to a named world anchor on boot
		//             (jozz_vehicle_world_layout.h kWorldAnchors, exact match, e.g. "Start")
		//   AUTODRIVE 0/1  force full-throttle straight-line drive every Step - headless
		//             drive-through testing (map seam/tile-join gates) with no human at
		//             the keyboard; see docs/MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md
		//   PERF_DUMP step  printf one averaged step_ms/fps/counters line to stdout once
		//             m_stepCount reaches this value (headless performance measurement)
		//   REGEN_SEED int  rebuild the offroad chunk with this seed right after boot
		//             (headless verification that the "Przebuduj teren" code path works
		//             and the seam stays correct with a different terrain)
		//   REGEN_COUNT n  regenerate the offroad chunk n times with varying seeds right
		//             after boot and printf the debug mesh registry count before/after
		//             (headless R4 check: regeneration must not leak meshes)
		//   TERRAIN_DUMP 0/1  printf the offroad summit (central mountain) world pos
		//             + height each time the chunk is built - camera-framing aid for
		//             the mountain --screenshot runs (jozz_vehicle_world_terrain.cpp)
		// Throwaway regression scaffolds were removed 2026-07-09 (their bugs are
		// fixed + guarded): JOZZ_M6_DIRTY_AT_FRAME (tab-identity), TEST_RESET_MODAL.
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
		if ( const char* v = std::getenv( "JOZZ_M6_STEERING_RIG" ) )
		{
			std::snprintf( m_config.frontSuspensionVisualModel, sizeof( m_config.frontSuspensionVisualModel ),
							atoi( v ) != 0 ? "rig_kierowniczy" : "klasyczny" );
			SetupSteeringRig(); // this hook runs AFTER CreateVehicle's own bake - re-bake needed
		}
		if ( const char* v = std::getenv( "JOZZ_M6_BODY" ) )
		{
			m_showBodyVisual = atoi( v ) != 0;
			UpdateChassisShapeVisibility();
		}
		if ( const char* v = std::getenv( "JOZZ_M6_BODY_MODEL" ) )
		{
			std::snprintf( m_config.bodyVisualModel, sizeof( m_config.bodyVisualModel ), "%s", v );
			ApplyBodyVisualFromConfig();
		}
		if ( const char* v = std::getenv( "JOZZ_M6_COLLIDER" ) )
		{
			m_showChassisCollider = atoi( v ) != 0;
			UpdateChassisShapeVisibility();
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
			// Sets both axles - matches the pre-P3 single-preload behavior for
			// existing headless-shot scripts. Use the front/rear sliders (or a
			// preset) to set them independently.
			m_config.suspensionPreloadFront = (float)atof( v );
			m_config.suspensionPreloadRear = (float)atof( v );
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
		if ( const char* v = std::getenv( "JOZZ_M6_TELEPORT" ) )
		{
			for ( int i = 0; i < JozzWorldLayout::kWorldAnchorCount; ++i )
			{
				const JozzWorldLayout::JozzWorldAnchor& anchor = JozzWorldLayout::kWorldAnchors[i];
				if ( std::strcmp( v, anchor.name ) == 0 )
				{
					TeleportTo( anchor.x, anchor.z );
					break;
				}
			}
		}
		if ( const char* v = std::getenv( "JOZZ_M6_TELEPORT_XZ" ) )
		{
			// Arbitrary-coordinate escape hatch for headless testing (e.g. driving
			// each Etap 2 lane individually) - the named-anchor hook above only
			// covers the small curated registry (P1, full set in Etap 6).
			float x = 0.0f, z = 0.0f;
			if ( std::sscanf( v, "%f,%f", &x, &z ) == 2 )
			{
				TeleportTo( x, z );
			}
		}
		if ( const char* v = std::getenv( "JOZZ_M6_AUTODRIVE" ) )
		{
			m_autoDrive = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M6_PERF_DUMP" ) )
		{
			m_perfDumpAtStep = atoi( v );
		}
		if ( const char* v = std::getenv( "JOZZ_M6_REGEN_SEED" ) )
		{
			m_worldSeedInput = atoi( v );
			RegenerateTerrain();
		}
		if ( const char* v = std::getenv( "JOZZ_M6_REGEN_COUNT" ) )
		{
			// Deferred to Step() (m_regenCheckAtStep), not run here: the debug
			// renderer's mesh pool is populated lazily by Render(), which has not
			// run yet this early in the constructor - checking GetDebugShapeCount()
			// right now would read 0 before and after regardless of any leak.
			m_regenCheckCount = atoi( v );
			m_regenCheckAtStep = 30;
		}
	}

JozzVehicleM6RigLab::~JozzVehicleM6RigLab()
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
		m_riggedSteeringL.Destroy();
		m_riggedSteeringR.Destroy();
		m_bodyVisual.Destroy();
		DestroyJozzVehicleM5TestCourse( &m_testCourse );
		DestroyJozzWorldGround( &m_worldGround );
		m_context->enableContinuous = m_savedEnableContinuous;
	}

	// The tabbed panel hosts plots and wide sliders, and the Polish tab names
	// (Zawieszenie, Kierownica, ...) run wider than the English originals in
	// the proportional UI font - the default 20 em drawer is too narrow for
	// either.
float JozzVehicleM6RigLab::InfoPanelWidthEm() const
	{
		return 31.0f;
	}

bool JozzVehicleM6RigLab::CondenseDebugOverlay() const
	{
		return true;
	}

float JozzVehicleM6RigLab::GetSpawnHeight() const
	{
		return m_config.restDrop + m_config.wheelEnvelope.radius + 0.05f;
	}

float JozzVehicleM6RigLab::GetGroundHeightAt( float x, float z ) const
	{
		// The scan island is a mesh, not part of the analytic plate/offroad height
		// field. When the query point is over the loaded island, sample it with a
		// downward raycast so teleport spawn-height (four-wheel footprint sampling
		// in CreateVehicle) lands the car on the scan surface, not on the plate.
		if ( m_scanLoaded )
		{
			const b3AABB& b = m_scanBodies.worldBounds;
			if ( x >= b.lowerBound.x && x <= b.upperBound.x && z >= b.lowerBound.z && z <= b.upperBound.z )
			{
				b3Pos from = { x, b.upperBound.y + 50.0f, z };
				b3Vec3 translation = { 0.0f, ( b.lowerBound.y - 50.0f ) - from.y, 0.0f };
				b3RayResult hit = b3World_CastRayClosest( m_worldId, from, translation, b3DefaultQueryFilter() );
				if ( B3_IS_NON_NULL( hit.shapeId ) )
				{
					return hit.point.y;
				}
			}
		}
		return SampleJozzWorldGroundHeight( m_worldGround, x, z );
	}

void JozzVehicleM6RigLab::TeleportTo( float x, float z )
	{
		m_spawnAnchorX = x;
		m_spawnAnchorZ = z;
		CreateVehicle();
	}

void JozzVehicleM6RigLab::RegenerateTerrain()
	{
		RegenerateJozzWorldOffroad( &m_worldGround, JOZZ_M6_TERRAIN_CATEGORY, (uint32_t)m_worldSeedInput );
	}

void JozzVehicleM6RigLab::CreateVehicle()
	{
		DestroyVehicle();
		// Spawn height: sample the terrain under ALL four wheel positions (with
		// a little padding), not just the anchor center, and take the max. On a
		// mountainside a single center sample leaves the uphill wheels starting
		// below ground (Jozz's feedback: teleport wbija kola w podloge). The
		// +0.30 m clearance also absorbs the difference between the smooth
		// analytic noise and the piecewise-linear heightfield triangles between
		// grid vertices (worst on sharp ridged crests). The car drops a few cm
		// and settles - much better than a wheel locked inside the ground.
		float footX = m_config.axleHalfSpacing + 0.3f;
		float footZ = m_config.trackHalfWidth + 0.2f;
		float groundY = GetGroundHeightAt( m_spawnAnchorX, m_spawnAnchorZ );
		groundY = b3MaxFloat( groundY, GetGroundHeightAt( m_spawnAnchorX + footX, m_spawnAnchorZ + footZ ) );
		groundY = b3MaxFloat( groundY, GetGroundHeightAt( m_spawnAnchorX + footX, m_spawnAnchorZ - footZ ) );
		groundY = b3MaxFloat( groundY, GetGroundHeightAt( m_spawnAnchorX - footX, m_spawnAnchorZ + footZ ) );
		groundY = b3MaxFloat( groundY, GetGroundHeightAt( m_spawnAnchorX - footX, m_spawnAnchorZ - footZ ) );
		m_vehicle = CreateJozzVehicleM6( m_worldId, m_groundId, m_config,
										  { m_spawnAnchorX, groundY + GetSpawnHeight() + 0.25f, m_spawnAnchorZ } );
		UpdateWheelShapeVisibility();
		UpdateChassisShapeVisibility();
		SetupMountRig();
		SetupSteeringRig();
	}

	// "Reset swiat" - a full simulation restart scoped to this running sample
	// object, so it can never touch m_config or the Debug-tab toggles (there is
	// nothing to reload them from; they simply aren't part of what this resets).
	// This is the button form of what "R" does at the process level; it exists
	// because "R" goes through the engine's global restart (destroy + rebuild
	// the whole sample), which is what was silently reviving the hardcoded
	// Debug defaults before LoadDebugViewState() existed.
void JozzVehicleM6RigLab::ResetWorld()
	{
		CreateVehicle();
		ResetJozzVehicleM5TestCourseProps( m_testCourse );
		m_telemetryHead = 0;
		m_telemetryCount = 0;
		m_telemetryClock = 0.0f;
	}

void JozzVehicleM6RigLab::DestroyVehicle()
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
			SetShapeHidden( m_vehicle.chassisShapeId, false );
		}

		DestroyJozzVehicleM6( &m_vehicle );
	}

void JozzVehicleM6RigLab::UpdateWheelShapeVisibility()
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

	// The collision box is the ONLY thing the debug draw shows for the chassis,
	// so unlike the wheels there is no extra "surowe kształty" toggle: box
	// visibility simply inverts body-skin visibility. With the skin hidden (or
	// "brak" selected) the box must come back, or the car has no chassis on
	// screen at all.
void JozzVehicleM6RigLab::UpdateChassisShapeVisibility()
	{
		if ( m_vehicle.valid == false )
		{
			return;
		}
		bool skinCoversBox = m_showBodyVisual && m_bodyVisual.IsLoaded();
		SetShapeHidden( m_vehicle.chassisShapeId, skinCoversBox && m_showChassisCollider == false );
	}

void JozzVehicleM6RigLab::ApplySuspensionTuning()
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
				float preload = isFront ? m_config.suspensionPreloadFront : m_config.suspensionPreloadRear;
				b3DistanceJoint_SetLength( runtime.coiloverJointId, design + preload * motionRatio );
				b3DistanceJoint_SetLengthRange( runtime.coiloverJointId,
												 b3MaxFloat( 0.05f, design - m_config.compressionTravel * motionRatio ),
												 design + m_config.reboundTravel * motionRatio );
				b3Joint_WakeBodies( runtime.coiloverJointId );
			}
		}
	}

void JozzVehicleM6RigLab::ApplySteeringTuning()
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

void JozzVehicleM6RigLab::ApplyWheelFriction()
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

void JozzVehicleM6RigLab::ApplyContactTuning()
	{
		b3World_SetContactTuning( m_worldId, m_contactHertz, m_contactDampingRatio, m_contactSpeed );
	}

void JozzVehicleM6RigLab::Keyboard( int key, int action, int modifiers )
	{
		if ( key == KEY_T && action == ACTION_PRESS )
		{
			ToggleThirdPerson();
		}

		Sample::Keyboard( key, action, modifiers );
	}

void JozzVehicleM6RigLab::SampleTelemetry()
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

void JozzVehicleM6RigLab::Step()
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

		if ( m_autoDrive )
		{
			// Headless drive-through testing aid (JOZZ_M6_AUTODRIVE) - straight
			// line, full throttle, no human at the keyboard. See the env registry
			// comment in the constructor.
			input.drive = 1.0f;
			input.brake = false;
		}

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

		if ( m_regenCheckCount > 0 && m_stepCount == m_regenCheckAtStep )
		{
			// Baseline: by this step, Render() has drawn every shape at least
			// once, so the mesh pool already reflects steady state.
			int shapesBefore = GetDebugShapeCount();
			for ( int i = 0; i < m_regenCheckCount; ++i )
			{
				m_worldSeedInput = (int)m_worldGround.seed + 1000 + i;
				RegenerateTerrain();
			}
			std::printf( "JOZZ_M6_REGEN_CHECK count=%d shapes_before=%d\n", m_regenCheckCount, shapesBefore );
			// The final regenerated shape has not been drawn yet this frame, so
			// the pool count right now would under-count by one. Wait a couple
			// of frames for Render() to settle before reading it again.
			m_regenCheckAtStep = m_stepCount + 3;
			m_regenCheckCount = -1; // next hit: print settled count only
		}
		else if ( m_regenCheckCount < 0 && m_stepCount == m_regenCheckAtStep )
		{
			std::printf( "JOZZ_M6_REGEN_CHECK shapes_settled=%d\n", GetDebugShapeCount() );
			m_regenCheckCount = 0; // done
		}
	}

void JozzVehicleM6RigLab::Render()
	{
		Sample::Render();

		// Textured scan skin (M2): one AppendMesh per group at the island origin.
		// Drawn before the vehicle-valid gate so the island always shows, even with
		// no car spawned. Collision stays as-is; this is the visible surface.
		DrawJozzScanVisual( m_scanVisual );

		if ( m_vehicle.valid == false )
		{
			return;
		}

		if ( m_dumpGeometry && ++m_dumpFrame == 140 )
		{
			DumpCornerGeometry();
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
				// Front corners show the new steering rig instead of the old mount
				// when the toggle is on (rig-editor warm-up G1); rear keeps the old
				// mount, so you see both suspensions at once (decision D1a).
				bool isFront = corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
				if ( UseSteeringRig() && isFront )
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

		// New front steering rig (rig-editor warm-up G1): each part on the live
		// body its kinematic role demands (chassis / lower arm / knuckle). Drawn
		// only for the FRONT corners; the rear keeps the old mount above.
		if ( m_showMountVisuals && UseSteeringRig() )
		{
			DrawSteeringRig();
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
				// Front dampers belong to the old mount; when the steering rig is
				// on, the front's dampers come with it (its own damper is gejt G3).
				bool isFront = corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
				if ( UseSteeringRig() && isFront )
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

		if ( m_showBodyVisual )
		{
			DrawBodyVisual();
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

		// Etap 2 obstacle-kit station labels (docs/MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md
		// §5): collected once at course-build time, distance-culled here so a
		// distant camera doesn't drown the HUD in text (§7 risk).
		{
			constexpr float kLabelCullDistance = 80.0f;
			b3Pos eye = m_camera->m_worldEye;
			for ( const JozzCourseLabel& label : m_testCourse.labels )
			{
				float dx = (float)( label.position.x - eye.x );
				float dy = (float)( label.position.y - eye.y );
				float dz = (float)( label.position.z - eye.z );
				if ( dx * dx + dy * dy + dz * dz > kLabelCullDistance * kLabelCullDistance )
				{
					continue;
				}
				DrawString3D( label.position, MakeColor( (b3HexColor)label.colorHex ), "%s", label.text.c_str() );
			}
		}

		if ( m_perfDumpAtStep > 0 && m_stepCount >= m_perfDumpAtStep )
		{
			int available = m_profileWriteIndex - m_profileReadIndex;
			int count = available < 60 ? available : 60;
			float totalStepMs = 0.0f;
			for ( int i = 0; i < count; ++i )
			{
				int idx = ( m_profileWriteIndex - 1 - i ) & ( m_profileCapacity - 1 );
				totalStepMs += m_profiles[idx].step;
			}
			float avgStepMs = count > 0 ? totalStepMs / (float)count : 0.0f;
			b3Counters counters = b3World_GetCounters( m_worldId );
			std::printf( "JOZZ_M6_PERF step_ms=%.4f fps=%.1f bodies=%d shapes=%d contacts=%d anchor=(%.1f,%.1f)\n",
						 avgStepMs, avgStepMs > 0.0f ? 1000.0f / avgStepMs : 0.0f, counters.bodyCount, counters.shapeCount,
						 counters.contactCount, m_spawnAnchorX, m_spawnAnchorZ );
			m_perfDumpAtStep = -1; // print exactly once
		}
	}

Sample* JozzVehicleM6RigLab::Create( SampleContext* context )
	{
		return new JozzVehicleM6RigLab( context );
	}

Sample* CreateJozzVehicleM6RigLab( SampleContext* context )
{
	return JozzVehicleM6RigLab::Create( context );
}
