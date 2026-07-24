// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m5_drivable_lab.h"

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "implot.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_m5_test_course.h"
#include "jozz_vehicle_central_test_campus_visual.h"
#include "jozz_vehicle_m5_vehicle.h"
#include "jozz_vehicle_visual_mesh.h"
#include "jozz_vehicle_world_layout.h"
#include "jozz_vehicle_world_terrain.h"

#include "box3d/box3d.h"

// M5 First Drivable: the first Jozz vehicle that actually drives. The physics
// vehicle lives in jozz_vehicle_m5_vehicle (shared with the headless
// validation smoke); this sample adds input, camera, a small test course,
// live tuning, and the visual-only glTF wheel attach proven in M3B.3.
class JozzVehicleM5FirstDrivable : public Sample
{
public:
	explicit JozzVehicleM5FirstDrivable( SampleContext* context )
		: Sample( context )
	{
		// Rear three-quarter chase view: the car drives away from the camera.
		// History note: the reported inverted A/D was blamed on the previous
		// front-facing camera at first, but the actual bug was a sign error in
		// the steering convention ("left is +Z" - wrong, left is -Z), fixed in
		// jozz_vehicle_m5_vehicle.cpp in M5.2 and asserted with a signed check
		// in the validation smoke. The chase view stays because it is the
		// better default regardless; m_invertSteering remains as a preference.
		// Fresh boot only: on "R" restart leave both alone so the T chase cam
		// (if it was on) survives instead of dropping to a static orbit view
		// (same fix as the M6 rig lab; Jozz's feedback: "R restartuje kamere").
		if ( context->restart == false )
		{
			m_camera->m_thirdPerson = false;
			m_camera->SetView( -135.0f, 14.0f, 13.0f, { 0.0f, 1.2f, 0.0f } );
		}

		// Mapa Etap 1 (docs/MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md): the same 3x3-tile
		// plate + offroad FBM chunk the M6 lab uses, built with the engine
		// default category (1) since this lab's single-sphere wheel model has
		// no split envelope to key on JOZZ_M6_TERRAIN_CATEGORY. Replaces the old
		// AddGroundBox(120) - the plate's center tile keeps the same top-at-y=0,
		// zero-behavior-change footprint the test course positions assume.
		m_worldGround = CreateJozzWorldGround( m_worldId, 1 );
		m_groundId = m_worldGround.plateBodyId;
		m_testCourse = CreateJozzVehicleM5TestCourse( m_worldId, 0.0f );

		m_assetMetadata = LoadJozzVehicleAuditMetadata();
		JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_assetMetadata );
		m_metersPerBlockbenchUnit = defaults.metersPerBlockbenchUnit;
		m_config = JozzVehicleM5DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

		m_showWheelVisuals = true;
		m_showPrimitiveWheelShapes = false;
		m_showAxisDiagnostics = false;
		m_invertSteering = false;

		// Engine defaults from b3DefaultWorldDef; the sliders tune the live world.
		m_contactHertz = 30.0f;
		m_contactDampingRatio = 10.0f;
		m_contactSpeed = 3.0f;

		m_telemetryHead = 0;
		m_telemetryCount = 0;
		m_telemetryClock = 0.0f;

		m_vehicle = {};
		CreateVehicle();
		LoadWheelVisual();
		SyncEditFromConfig();
	}

	~JozzVehicleM5FirstDrivable() override
	{
		DestroyVehicle();
		m_wheelVisual.Destroy();
		DestroyJozzVehicleM5TestCourse( &m_testCourse );
		DestroyJozzWorldGround( &m_worldGround );
	}

	void SyncEditFromConfig()
	{
		m_editChassisHalfExtents = m_config.chassisHalfExtents;
		m_editChassisDensity = m_config.chassisDensity;
		m_editCgVerticalOffset = m_config.cgVerticalOffset;
		m_editAxleHalfSpacing = m_config.axleHalfSpacing;
		m_editTrackHalfWidth = m_config.trackHalfWidth;
		m_editRestDrop = m_config.restDrop;
		m_editWheelDensity = m_config.wheelDensity;
		m_editWheelShape = m_config.wheelShape;
		m_editWheelCylinderSides = m_config.wheelCylinderSides;
		m_editWheelRollingResistance = m_config.wheelRollingResistance;
		m_structuralSetupDirty = false;
	}

	void ApplyPendingStructuralSetup()
	{
		m_config.chassisHalfExtents = m_editChassisHalfExtents;
		m_config.chassisDensity = m_editChassisDensity;
		m_config.cgVerticalOffset = m_editCgVerticalOffset;
		m_config.axleHalfSpacing = m_editAxleHalfSpacing;
		m_config.trackHalfWidth = m_editTrackHalfWidth;
		m_config.restDrop = m_editRestDrop;
		m_config.wheelDensity = m_editWheelDensity;
		m_config.wheelShape = m_editWheelShape;
		m_config.wheelCylinderSides = m_editWheelCylinderSides;
		m_config.wheelRollingResistance = m_editWheelRollingResistance;
		CreateVehicle();
		m_structuralSetupDirty = false;
	}

	void ApplyWheelFriction()
	{
		for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
		{
			b3Shape_SetFriction( m_vehicle.wheelShapeIds[corner], m_config.wheelFriction );
		}
	}

	void ApplyContactTuning()
	{
		b3World_SetContactTuning( m_worldId, m_contactHertz, m_contactDampingRatio, m_contactSpeed );
	}

	void LoadWheelVisual()
	{
		m_wheelVisual.Destroy();
		m_wheelVisualSource.clear();

		std::string resolvedPath;
		if ( FindJozzVehicleAssetFile( "assets/source/Offroad_Big_Wheels.gltf", &resolvedPath ) &&
			 m_wheelVisual.LoadStaticGltf( resolvedPath.c_str(), m_metersPerBlockbenchUnit ) )
		{
			m_wheelVisualSource = resolvedPath;
		}

		m_wheelVisualCorrection = ComputeJozzVehicleWheelVisualCorrection( m_wheelVisual, m_assetMetadata, m_metersPerBlockbenchUnit );
	}

	float GetSpawnHeight() const
	{
		return m_config.restDrop + m_config.wheelRadius + 0.05f;
	}

	void CreateVehicle()
	{
		DestroyVehicle();
		m_vehicle = CreateJozzVehicleM5( m_worldId, m_groundId, m_config, { 0.0f, GetSpawnHeight(), 0.0f } );
		UpdateWheelShapeVisibility();
	}

	void DestroyVehicle()
	{
		if ( m_vehicle.valid )
		{
			// Unhide before destroying so the debug adapter does not keep stale
			// hidden-shape entries, mirroring the corner lab teardown.
			for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
			{
				SetShapeHidden( m_vehicle.wheelShapeIds[corner], false );
			}
		}

		DestroyJozzVehicleM5( &m_vehicle );
	}

	void UpdateWheelShapeVisibility()
	{
		if ( m_vehicle.valid == false )
		{
			return;
		}

		for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
		{
			SetShapeHidden( m_vehicle.wheelShapeIds[corner], m_showPrimitiveWheelShapes == false );
		}
	}

	void ApplySuspensionTuning()
	{
		for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
		{
			bool isFront = corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_FRONT_RIGHT;
			float scale = isFront ? m_config.frontSuspensionScale : m_config.rearSuspensionScale;
			b3WheelJoint_SetSuspensionHertz( m_vehicle.wheelJointIds[corner], m_config.suspensionHertz * scale );
			b3WheelJoint_SetSuspensionDampingRatio( m_vehicle.wheelJointIds[corner], m_config.suspensionDampingRatio * scale );
			b3Joint_WakeBodies( m_vehicle.wheelJointIds[corner] );
		}
	}

	void ApplySteeringTuning()
	{
		float maxAngle = m_config.maxSteeringAngleDegrees * B3_PI / 180.0f;
		b3JointId fronts[2] = { m_vehicle.wheelJointIds[JOZZ_M5_FRONT_LEFT], m_vehicle.wheelJointIds[JOZZ_M5_FRONT_RIGHT] };
		for ( b3JointId jointId : fronts )
		{
			b3WheelJoint_SetSteeringLimits( jointId, -maxAngle, maxAngle );
			b3WheelJoint_SetSteeringHertz( jointId, m_config.steeringHertz );
			b3WheelJoint_SetSteeringDampingRatio( jointId, m_config.steeringDampingRatio );
			b3WheelJoint_SetMaxSteeringTorque( jointId, m_config.maxSteeringTorque );
			b3Joint_WakeBodies( jointId );
		}
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
		ImGui::TextUnformatted( "Jozz Vehicle M5 First Drivable" );
		ImGui::Separator();
		ImGui::TextWrapped( "Four-corner primitive vehicle on the validated M2.4/M2.5 rest-anchor wheel-joint model. "
							"Physics lives in the shared M5 vehicle module; the glTF wheels are visual-only (M3B.3 attach)." );
		ImGui::TextUnformatted( "Input: W/S drive, A/D steer, Space brake, T third-person camera, R restart." );
		ImGui::Text( "speed %.1f m/s (%.0f km/h)", GetJozzVehicleM5ForwardSpeed( m_vehicle ),
					 3.6f * GetJozzVehicleM5ForwardSpeed( m_vehicle ) );
		ImGui::TextWrapped( "M5.2: the at-speed wheel hopping was the faceted cylinder itself (engine caps hull "
							"cylinders at 32 sides; probe data: front ground contact 31%% on cylinder-32 vs 100%% on "
							"sphere). Default wheel is now the smooth sphere; switch back under Wheels to feel the "
							"facet washboard." );
		ImGui::Separator();

		ImGui::TextUnformatted( "Drive (wide ranges: this lab is for stress-testing until it breaks)" );
		ImGui::SliderFloat( "Drive torque", &m_config.maxDriveTorque, 0.0f, 6000.0f, "%.0f" );
		ImGui::SliderFloat( "Drive speed", &m_config.maxDriveSpeed, 0.0f, 150.0f, "%.0f rad/s" );
		ImGui::SliderFloat( "Brake torque", &m_config.brakeTorque, 0.0f, 6000.0f, "%.0f" );
		ImGui::Checkbox( "All wheel drive", &m_config.allWheelDrive );
		ImGui::Separator();

		ImGui::TextUnformatted( "Suspension" );
		if ( ImGui::SliderFloat( "Spring hertz", &m_config.suspensionHertz, 0.2f, 30.0f, "%.2f" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Damping ratio", &m_config.suspensionDampingRatio, 0.0f, 6.0f, "%.2f" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Front axle scale", &m_config.frontSuspensionScale, 0.3f, 3.0f, "%.2f" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Rear axle scale", &m_config.rearSuspensionScale, 0.3f, 3.0f, "%.2f" ) )
		{
			ApplySuspensionTuning();
		}
		ImGui::TextWrapped( "Axle scales multiply hertz+damping per axle: raise the front to fight the "
							"acceleration-lightened front end, or unbalance them on purpose to feel the difference." );
		ImGui::Separator();

		ImGui::TextUnformatted( "Steering" );
		ImGui::Checkbox( "Invert steering (preference)", &m_invertSteering );
		{
			const char* modes[] = { "Independent servos", "Linked (virtual tie rod)" };
			ImGui::Combo( "Linkage", &m_config.steeringMode, modes, 2 );
		}
		ImGui::Checkbox( "Ackermann geometry (inner wheel steers tighter)", &m_config.ackermannGeometry );
		ImGui::SliderFloat( "Tie rod tolerance", &m_config.tieRodToleranceDegrees, 0.5f, 15.0f, "%.1f deg" );
		ImGui::Checkbox( "Speed-sensitive steering (assist)", &m_config.speedSensitiveSteering );
		if ( m_config.speedSensitiveSteering )
		{
			ImGui::SliderFloat( "Taper start", &m_config.steeringTaperStartSpeed, 0.0f, 30.0f, "%.1f m/s" );
			ImGui::SliderFloat( "Taper end", &m_config.steeringTaperEndSpeed, 1.0f, 60.0f, "%.1f m/s" );
			ImGui::SliderFloat( "Min angle scale", &m_config.steeringTaperMinScale, 0.05f, 1.0f, "%.2f" );
		}
		if ( ImGui::SliderFloat( "Max angle", &m_config.maxSteeringAngleDegrees, 1.0f, 60.0f, "%.0f deg" ) )
		{
			ApplySteeringTuning();
		}
		if ( ImGui::SliderFloat( "Steering torque", &m_config.maxSteeringTorque, 0.0f, 3000.0f, "%.0f" ) )
		{
			ApplySteeringTuning();
		}
		if ( ImGui::SliderFloat( "Steering hertz", &m_config.steeringHertz, 0.5f, 40.0f, "%.2f" ) )
		{
			ApplySteeringTuning();
		}
		if ( ImGui::SliderFloat( "Steering damping ratio", &m_config.steeringDampingRatio, 0.0f, 6.0f, "%.2f" ) )
		{
			ApplySteeringTuning();
		}
		ImGui::TextWrapped( "A stationary tire resists steering with its whole contact patch twisting against "
							"friction (why real cars need power steering); a rolling tire needs far less torque. "
							"If steering feels frozen at low speed again, raise Steering torque first. In Linked "
							"mode a blocked wheel holds its partner back like a real tie rod." );
		ImGui::Separator();

		ImGui::TextUnformatted( "Wheels" );
		if ( ImGui::SliderFloat( "Tire friction", &m_config.wheelFriction, 0.1f, 4.0f, "%.2f" ) )
		{
			ApplyWheelFriction();
		}
		{
			const char* shapes[] = { "Cylinder (faceted, max 32 sides)", "Sphere (smooth)" };
			bool wheelEdited = ImGui::Combo( "Wheel shape (Apply)", &m_editWheelShape, shapes, 2 );
			wheelEdited |= ImGui::SliderInt( "Cylinder sides (Apply)", &m_editWheelCylinderSides, 3, 32 );
			wheelEdited |= ImGui::SliderFloat( "Rolling resistance (Apply)", &m_editWheelRollingResistance, 0.0f, 0.5f, "%.3f" );
			if ( wheelEdited )
			{
				m_structuralSetupDirty = true;
			}
		}
		ImGui::TextWrapped( "Cylinder facets are a built-in washboard: 12 sides = violent hopping, 32 = the "
							"instability from the 2026-07-05 playtest, sphere = smooth reference. The engine hard-caps "
							"hull cylinders at 32 sides." );
		ImGui::Separator();

		ImGui::TextUnformatted( "Contact solver tuning (world level, live)" );
		bool contactEdited = false;
		contactEdited |= ImGui::SliderFloat( "Contact hertz", &m_contactHertz, 5.0f, 240.0f, "%.0f" );
		contactEdited |= ImGui::SliderFloat( "Contact damping ratio", &m_contactDampingRatio, 0.5f, 40.0f, "%.1f" );
		contactEdited |= ImGui::SliderFloat( "Contact push speed", &m_contactSpeed, 0.5f, 20.0f, "%.1f m/s" );
		if ( contactEdited )
		{
			ApplyContactTuning();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Engine defaults##contact" ) )
		{
			m_contactHertz = 30.0f;
			m_contactDampingRatio = 10.0f;
			m_contactSpeed = 3.0f;
			ApplyContactTuning();
		}
		ImGui::Separator();

		if ( ImGui::Checkbox( "Upright assist", &m_config.uprightAssist ) )
		{
			CreateVehicle();
		}
		ImGui::TextWrapped( "Upright assist is the soft parallel joint from the stock Driving sample. Toggling rebuilds the vehicle at spawn." );

		ImGui::Checkbox( "glTF wheel visuals", &m_showWheelVisuals );
		if ( ImGui::Checkbox( "Primitive wheel shapes", &m_showPrimitiveWheelShapes ) )
		{
			UpdateWheelShapeVisibility();
		}
		ImGui::Checkbox( "Axis diagnostics", &m_showAxisDiagnostics );

		if ( ImGui::Button( "Reset vehicle" ) )
		{
			CreateVehicle();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Reset props" ) )
		{
			ResetJozzVehicleM5TestCourseProps( m_testCourse );
		}

		ImGui::Separator();
		ImGui::TextUnformatted( "Structural rig setup (geometry/mass - requires Apply, rebuilds the vehicle)" );
		bool structuralEdited = false;
		structuralEdited |= ImGui::SliderFloat( "Chassis half length", &m_editChassisHalfExtents.x, 0.3f, 4.0f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Chassis half height", &m_editChassisHalfExtents.y, 0.1f, 1.5f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Chassis half width", &m_editChassisHalfExtents.z, 0.2f, 2.0f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Chassis density", &m_editChassisDensity, 20.0f, 2000.0f, "%.0f" );
		structuralEdited |= ImGui::SliderFloat( "CG drop (lower = stabler)", &m_editCgVerticalOffset, -0.3f, 0.8f, "%.2f m" );
		structuralEdited |= ImGui::SliderFloat( "Axle half spacing (wheelbase/2)", &m_editAxleHalfSpacing, 0.4f, 4.0f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Track half width", &m_editTrackHalfWidth, 0.4f, 3.0f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Rest drop", &m_editRestDrop, 0.05f, 2.0f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Wheel density", &m_editWheelDensity, 5.0f, 1000.0f, "%.0f" );
		if ( structuralEdited )
		{
			m_structuralSetupDirty = true;
		}
		if ( m_structuralSetupDirty )
		{
			ImGui::TextWrapped( "Pending structural change. Press Apply to rebuild the vehicle." );
		}
		if ( ImGui::Button( "Apply rig rebuild" ) )
		{
			ApplyPendingStructuralSetup();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Reset rig to asset defaults" ) )
		{
			JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_assetMetadata );
			m_config = JozzVehicleM5DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
			CreateVehicle();
			SyncEditFromConfig();
		}

		ImGui::TextWrapped( "%s", m_wheelVisual.status.c_str() );
		ImGui::TextWrapped( "%s", m_wheelVisual.textureStatus.c_str() );
		ImGui::TextWrapped( "metadata: %s", m_assetMetadata.status.c_str() );

		ImGui::Separator();
		DrawTelemetryPanel();

		return true;
	}

	void DrawTelemetryPanel()
	{
		ImGui::TextUnformatted( "Telemetry (last 10 s)" );

		const char* cornerNames[JOZZ_M5_CORNER_COUNT] = { "FL", "FR", "RL", "RR" };
		JozzVehicleM5WheelTelemetry current[JOZZ_M5_CORNER_COUNT];
		for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
		{
			current[corner] = GetJozzVehicleM5WheelTelemetry( m_vehicle, corner );
		}

		ImGui::Text( "ground contact: FL %s  FR %s  RL %s  RR %s", current[0].groundContact ? "YES" : "air",
					 current[1].groundContact ? "YES" : "air", current[2].groundContact ? "YES" : "air",
					 current[3].groundContact ? "YES" : "air" );
		ImGui::Text( "load N: FL %.0f  FR %.0f  RL %.0f  RR %.0f", current[0].suspensionLoad, current[1].suspensionLoad,
					 current[2].suspensionLoad, current[3].suspensionLoad );

		if ( m_telemetryCount < 2 )
		{
			return;
		}

		// The ring buffer stores monotonically increasing time, so the plot can
		// window on the newest 10 seconds without reordering.
		float latestTime = m_telemetryTime[( m_telemetryHead + TELEMETRY_CAPACITY - 1 ) % TELEMETRY_CAPACITY];
		int count = m_telemetryCount;
		int start = ( m_telemetryHead + TELEMETRY_CAPACITY - count ) % TELEMETRY_CAPACITY;

		// Copy into linear scratch for ImPlot (capacity is small; simple wins).
		static float times[TELEMETRY_CAPACITY];
		static float series[JOZZ_M5_CORNER_COUNT][TELEMETRY_CAPACITY];
		for ( int i = 0; i < count; ++i )
		{
			int index = ( start + i ) % TELEMETRY_CAPACITY;
			times[i] = m_telemetryTime[index];
			for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
			{
				series[corner][i] = m_telemetryTravel[corner][index];
			}
		}

		ImVec2 plotSize( ImGui::GetContentRegionAvail().x, 140.0f );
		if ( ImPlot::BeginPlot( "Suspension travel", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "travel m" );
			ImPlot::SetupAxisLimits( ImAxis_X1, latestTime - 10.0, latestTime, ImPlotCond_Always );
			ImPlot::SetupAxisLimits( ImAxis_Y1, -m_config.reboundTravel * 1.2, m_config.compressionTravel * 1.2 );
			for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
			{
				ImPlot::PlotLine( cornerNames[corner], times, series[corner], count );
			}
			ImPlot::EndPlot();
		}

		for ( int i = 0; i < count; ++i )
		{
			int index = ( start + i ) % TELEMETRY_CAPACITY;
			for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
			{
				series[corner][i] = m_telemetryLoad[corner][index];
			}
		}

		if ( ImPlot::BeginPlot( "Suspension load", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "load N" );
			ImPlot::SetupAxisLimits( ImAxis_X1, latestTime - 10.0, latestTime, ImPlotCond_Always );
			for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
			{
				ImPlot::PlotLine( cornerNames[corner], times, series[corner], count );
			}
			ImPlot::EndPlot();
		}

		ImGui::TextWrapped( "Travel: + is compression, - is rebound; flat lines at the limits mean the suspension is "
							"maxed out. Load: per-corner force through the wheel joint - watch the front unload while "
							"accelerating." );
	}

	void SampleTelemetry()
	{
		float dt = m_context->hertz > 0.0f ? 1.0f / m_context->hertz : 1.0f / 60.0f;
		m_telemetryClock += dt;

		m_telemetryTime[m_telemetryHead] = m_telemetryClock;
		for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
		{
			JozzVehicleM5WheelTelemetry telemetry = GetJozzVehicleM5WheelTelemetry( m_vehicle, corner );
			m_telemetryTravel[corner][m_telemetryHead] = telemetry.suspensionTravel;
			m_telemetryLoad[corner][m_telemetryHead] = telemetry.suspensionLoad;
		}

		m_telemetryHead = ( m_telemetryHead + 1 ) % TELEMETRY_CAPACITY;
		if ( m_telemetryCount < TELEMETRY_CAPACITY )
		{
			m_telemetryCount += 1;
		}
	}

	void Step() override
	{
		JozzVehicleM5DriveInput input = {};
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

		UpdateJozzVehicleM5Drive( m_vehicle, input );

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

	void Render() override
	{
		Sample::Render();
		DrawCentralCampusSkeleton();

		if ( m_vehicle.valid == false )
		{
			return;
		}

		if ( m_showWheelVisuals && m_wheelVisual.IsLoaded() )
		{
			for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
			{
				b3WorldTransform wheelTransform = b3Body_GetTransform( m_vehicle.wheelIds[corner] );
				b3WorldTransform visualTransform = b3MulWorldTransforms( wheelTransform, m_wheelVisualCorrection );
				m_wheelVisual.DrawAtTransform( visualTransform, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
			}
		}

		if ( m_showAxisDiagnostics )
		{
			b3WorldTransform chassisTransform = b3Body_GetTransform( m_vehicle.chassisId );
			DrawAxes( chassisTransform, 1.2f );
			for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
			{
				b3Pos rest = GetJozzVehicleM5RestWheelCenter( m_vehicle, corner );
				b3Pos wheel = b3Body_GetPosition( m_vehicle.wheelIds[corner] );
				DrawCross( rest, 0.09f, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
				DrawLine( rest, wheel, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
			}
		}

		// Etap 2 obstacle-kit station labels (docs/MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md
		// §5/R11 - both labs share the course); distance-culled against the
		// camera so a far-off view doesn't drown the HUD in text.
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

		float steeringLeft = b3WheelJoint_GetSteeringAngle( m_vehicle.wheelJointIds[JOZZ_M5_FRONT_LEFT] );
		float steeringRight = b3WheelJoint_GetSteeringAngle( m_vehicle.wheelJointIds[JOZZ_M5_FRONT_RIGHT] );

		DrawTextLine( "Jozz Vehicle M5 First Drivable" );
		DrawTextLine( "W/S drive, A/D steer, Space brake, T third person, R restart" );
		DrawTextLine( "speed %.1f m/s (%.0f km/h)", GetJozzVehicleM5ForwardSpeed( m_vehicle ),
					  3.6f * GetJozzVehicleM5ForwardSpeed( m_vehicle ) );
		DrawTextLine( "steering %.1f/%.1f deg, %s, %s", 180.0f / B3_PI * steeringLeft, 180.0f / B3_PI * steeringRight,
					  m_config.allWheelDrive ? "AWD" : "RWD", m_config.uprightAssist ? "upright assist on" : "raw" );
		DrawTextLine( "wheels: %s, contact FL:%s FR:%s RL:%s RR:%s",
					  m_config.wheelShape == JOZZ_M5_WHEEL_SPHERE ? "sphere (smooth)" : "cylinder (faceted)",
					  GetJozzVehicleM5WheelTelemetry( m_vehicle, JOZZ_M5_FRONT_LEFT ).groundContact ? "Y" : "-",
					  GetJozzVehicleM5WheelTelemetry( m_vehicle, JOZZ_M5_FRONT_RIGHT ).groundContact ? "Y" : "-",
					  GetJozzVehicleM5WheelTelemetry( m_vehicle, JOZZ_M5_REAR_LEFT ).groundContact ? "Y" : "-",
					  GetJozzVehicleM5WheelTelemetry( m_vehicle, JOZZ_M5_REAR_RIGHT ).groundContact ? "Y" : "-" );
		DrawTextLine( "wheel visuals: %s", ( m_showWheelVisuals && m_wheelVisual.IsLoaded() ) ? "glTF attached (visual-only)"
																							  : "off or not loaded" );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehicleM5FirstDrivable( context );
	}

	b3BodyId m_groundId; // == m_worldGround.plateBodyId
	JozzVehicleWorldGround m_worldGround;
	JozzVehicleM5TestCourse m_testCourse;
	JozzVehicleM5 m_vehicle;
	JozzVehicleM5Config m_config;
	JozzVehicleAuditMetadata m_assetMetadata;
	JozzVehicleVisualMesh m_wheelVisual;
	b3Transform m_wheelVisualCorrection;
	std::string m_wheelVisualSource;
	float m_metersPerBlockbenchUnit;
	bool m_showWheelVisuals;
	bool m_showPrimitiveWheelShapes;
	bool m_showAxisDiagnostics;
	bool m_invertSteering;

	// World-level contact solver tuning (live sliders).
	float m_contactHertz;
	float m_contactDampingRatio;
	float m_contactSpeed;

	// Pending structural rig edits (geometry/mass/wheel shape); require "Apply rig rebuild".
	b3Vec3 m_editChassisHalfExtents;
	float m_editChassisDensity;
	float m_editCgVerticalOffset;
	float m_editAxleHalfSpacing;
	float m_editTrackHalfWidth;
	float m_editRestDrop;
	float m_editWheelDensity;
	int m_editWheelShape;
	int m_editWheelCylinderSides;
	float m_editWheelRollingResistance;
	bool m_structuralSetupDirty;

	// Rolling telemetry ring buffers (10 s at 60 Hz).
	static constexpr int TELEMETRY_CAPACITY = 600;
	float m_telemetryTime[TELEMETRY_CAPACITY];
	float m_telemetryTravel[JOZZ_M5_CORNER_COUNT][TELEMETRY_CAPACITY];
	float m_telemetryLoad[JOZZ_M5_CORNER_COUNT][TELEMETRY_CAPACITY];
	int m_telemetryHead;
	int m_telemetryCount;
	float m_telemetryClock;
};

Sample* CreateJozzVehicleM5FirstDrivable( SampleContext* context )
{
	return JozzVehicleM5FirstDrivable::Create( context );
}
