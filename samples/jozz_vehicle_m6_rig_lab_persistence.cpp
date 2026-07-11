// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_rig_lab_internal.h"

	// See kDebugSessionFilePath above for why this is a separate file from the
	// vehicle config: these are view toggles, not tuning, and must survive "R"
	// without ever being reset by a preset load or "restore defaults".
void JozzVehicleM6RigLab::SaveDebugViewState()
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
		// Not a view toggle, but the same class: a sample-level preference that
		// is NOT vehicle config, so it belongs here (survives "R", never leaks
		// into a preset). See SUBSYSTEM_UI_PRESETS_PL.md §1b (TECH_DEBT #12).
		file << "invertSteering=" << ( m_invertSteering ? 1 : 0 ) << "\n";
		// Whether the body mesh is drawn at all - a pure view toggle. WHICH body
		// is selected is m_config.bodyVisualModel (vehicle identity, Etap 2).
		file << "showBodyVisual=" << ( m_showBodyVisual ? 1 : 0 ) << "\n";
	}

void JozzVehicleM6RigLab::LoadDebugViewState()
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
			else if ( key == "invertSteering" )
			{
				m_invertSteering = value;
			}
			else if ( key == "showBodyVisual" )
			{
				m_showBodyVisual = value;
			}
		}
	}

void JozzVehicleM6RigLab::SyncEditFromConfig()
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
		m_editMaxSteeringAngleDegrees = m_config.maxSteeringAngleDegrees;
		m_editFrontToeDeg = m_config.frontToeDeg;
		m_editRearToeDeg = m_config.rearToeDeg;
		m_editChassisHalfExtents = m_config.chassisHalfExtents;
		m_editChassisDensity = m_config.chassisDensity;
		m_editCgVerticalOffset = m_config.cgVerticalOffset;
		m_editAxleHalfSpacing = m_config.axleHalfSpacing;
		m_editTrackHalfWidth = m_config.trackHalfWidth;
		m_editRestDrop = m_config.restDrop;
		m_editWheelDensity = m_config.wheelDensity;
		m_structuralSetupDirty = false;
	}

void JozzVehicleM6RigLab::ApplyPendingStructuralSetup()
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
		m_config.maxSteeringAngleDegrees = m_editMaxSteeringAngleDegrees;
		m_config.frontToeDeg = m_editFrontToeDeg;
		m_config.rearToeDeg = m_editRearToeDeg;
		m_config.chassisHalfExtents = m_editChassisHalfExtents;
		m_config.chassisDensity = m_editChassisDensity;
		m_config.cgVerticalOffset = m_editCgVerticalOffset;
		m_config.axleHalfSpacing = m_editAxleHalfSpacing;
		m_config.trackHalfWidth = m_editTrackHalfWidth;
		m_config.restDrop = m_editRestDrop;
		m_config.wheelDensity = m_editWheelDensity;

		// P5 safety clamp: the P1 twist-fence (maxSteer + 10 deg margin) must
		// stay clear of the tie-rod's own over-center dead point, which shifts
		// with THIS SAME wishbone geometry (ackermannFraction alone moves it
		// from ~75 deg at 0 to ~50 deg at 1.0 - measured 2026-07-08). A static
		// slider range can't be safe for every combination, so clamp live
		// instead, the same way the arm-droop slider clamps to 16 deg.
		{
			float wheelbase = 2.0f * m_config.axleHalfSpacing;
			float deadPointDeg = ComputeJozzVehicleM6SteeringDeadPointDeg( m_config.wishbone, wheelbase,
																		   m_config.trackHalfWidth, m_config.rackHalfWidth );
			float safeMaxSteer = deadPointDeg - 13.0f; // fence margin (10) + P1's own 3 deg clearance
			if ( m_config.maxSteeringAngleDegrees > safeMaxSteer )
			{
				m_steeringClampStatus =
					"Maksymalny skręt zacieśniony z " + std::to_string( (int)m_config.maxSteeringAngleDegrees ) +
					" do " + std::to_string( (int)safeMaxSteer ) +
					" st. - przy tej geometrii wahaczy/Ackermanna wyższa wartość wchodzi w martwy punkt drążka.";
				m_config.maxSteeringAngleDegrees = b3MaxFloat( 15.0f, safeMaxSteer );
				m_editMaxSteeringAngleDegrees = m_config.maxSteeringAngleDegrees;
			}
			else
			{
				m_steeringClampStatus.clear();
			}
		}

		// Steering geometry (wishbone, axle spacing, track, max steer) can all
		// change above, and rackTravel is derived from them - without this the
		// rack limit stays stale from whatever geometry built the previous
		// vehicle, letting the linkage overshoot past its own tie-rod dead
		// point on the next hard steer input.
		RecomputeRackTravel();
		CreateVehicle();
		m_structuralSetupDirty = false;
	}

	// rackTravel is derived from the steering geometry (kingpin offset, arm
	// back, Ackermann, track/wheelbase) rather than an independent tuning
	// value, so it is deliberately left out of the saved config - a preset or
	// session file only needs to describe what makes it different. Call this
	// after loading either so the rack limit always matches whatever geometry
	// came with it, even if a future preset changes steering hardpoints.
void JozzVehicleM6RigLab::RecomputeRackTravel()
	{
		float maxAngle = m_config.maxSteeringAngleDegrees * B3_PI / 180.0f;
		m_config.rackTravel = ComputeJozzVehicleM6RackStroke( m_config.wishbone, 2.0f * m_config.axleHalfSpacing,
															   m_config.trackHalfWidth, m_config.rackHalfWidth, maxAngle );
	}

void JozzVehicleM6RigLab::RefreshPresetList()
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
void JozzVehicleM6RigLab::LoadPresetByName( const std::string& name )
	{
		std::string path = std::string( kPresetDirectory ) + "/" + name + ".json";
		// DETERMINISTIC restore (2026-07-09 fix): overlay the preset onto the
		// factory baseline, never onto the live config. Presets are partial
		// files; the old in-place load silently kept every experimental slider
		// value the preset didn't mention - "the preset saved my changes
		// without permission" from the user's chair (and the session auto-save
		// then made it permanent). Caught by Jozz in manual play; pinned by the
		// validator's preset-determinism probe.
		if ( LoadJozzVehicleM6PresetConfig( path, m_factoryConfig, &m_config ) == false )
		{
			m_presetStatus = "Nie udało się wczytać presetu '" + name + "'.";
			return;
		}
		// Same defensive boundary as the session load in the constructor:
		// presets are hand-editable JSON (P6).
		bool sanitized = SanitizeJozzVehicleM6Config( &m_config );
		RecomputeRackTravel();
		CreateVehicle();
		// Visual identity fields (body skin, offset, front rig model) are part of
		// the config now too (2026-07-11 doctrine); CreateVehicle() re-bakes the
		// rig geometry but not the body mesh, so that funnels through here (plan
		// risk R4 - every config-replacing path must call this).
		ApplyBodyVisualFromConfig();
		SyncEditFromConfig();
		m_presetStatus = sanitized ? "Wczytano preset: " + name +
										 " (część wartości poza bezpiecznym zakresem - przycięte, szczegóły w konsoli)"
								   : "Wczytano preset: " + name;
	}

void JozzVehicleM6RigLab::SaveCurrentAsPreset( const std::string& name )
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

