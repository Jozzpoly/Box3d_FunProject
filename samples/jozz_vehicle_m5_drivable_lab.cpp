// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m5_drivable_lab.h"

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_m5_test_course.h"
#include "jozz_vehicle_m5_vehicle.h"
#include "jozz_vehicle_visual_mesh.h"

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
		// Rear three-quarter chase view: eye behind and above the spawn point,
		// looking toward +X (the chassis' forward axis). A 2026-07-05 playtest
		// reported A/D steering as inverted; the steering math itself checks out
		// against this codebase's own right = up x forward convention (see
		// jozz_vehicle_m5_vehicle.cpp), so the likely cause was the previous
		// default camera watching the car mostly from the front, which mirrors
		// screen-left/right relative to the driver's own left/right. This view
		// puts the car driving away from the camera instead, matching normal
		// chase-cam expectations. m_invertSteering below is a one-click escape
		// hatch in case this still is not enough.
		m_camera->m_thirdPerson = false;
		if ( context->restart == false )
		{
			m_camera->SetView( -135.0f, 14.0f, 13.0f, { 0.0f, 1.2f, 0.0f } );
		}

		// 2x the prior half-extent (60 -> 120) per playtest feedback wanting more
		// room to build speed.
		m_groundId = AddGroundBox( 120.0f );
		// AddGroundBox always places the box at y=-1 with half-height 1, so the
		// top surface is at y=0 regardless of the extent argument.
		m_testCourse = CreateJozzVehicleM5TestCourse( m_worldId, 0.0f );

		m_assetMetadata = LoadJozzVehicleAuditMetadata();
		JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_assetMetadata );
		m_metersPerBlockbenchUnit = defaults.metersPerBlockbenchUnit;
		m_config = JozzVehicleM5DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );

		m_showWheelVisuals = true;
		m_showPrimitiveWheelShapes = false;
		m_showAxisDiagnostics = false;
		m_invertSteering = false;

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
	}

	void SyncEditFromConfig()
	{
		m_editChassisHalfExtents = m_config.chassisHalfExtents;
		m_editChassisDensity = m_config.chassisDensity;
		m_editAxleHalfSpacing = m_config.axleHalfSpacing;
		m_editTrackHalfWidth = m_config.trackHalfWidth;
		m_editRestDrop = m_config.restDrop;
		m_editWheelDensity = m_config.wheelDensity;
		m_structuralSetupDirty = false;
	}

	void ApplyPendingStructuralSetup()
	{
		m_config.chassisHalfExtents = m_editChassisHalfExtents;
		m_config.chassisDensity = m_editChassisDensity;
		m_config.axleHalfSpacing = m_editAxleHalfSpacing;
		m_config.trackHalfWidth = m_editTrackHalfWidth;
		m_config.restDrop = m_editRestDrop;
		m_config.wheelDensity = m_editWheelDensity;
		CreateVehicle();
		m_structuralSetupDirty = false;
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
			b3WheelJoint_SetSuspensionHertz( m_vehicle.wheelJointIds[corner], m_config.suspensionHertz );
			b3WheelJoint_SetSuspensionDampingRatio( m_vehicle.wheelJointIds[corner], m_config.suspensionDampingRatio );
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
		ImGui::TextWrapped( "M5.1 note: instability at speed may be physics or a render-interpolation artifact of "
							"reading the body transform at draw time. Use the Solver panel below (Sub-steps) to help "
							"tell them apart: if raising sub-steps removes it, it was a solver convergence issue." );
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
		ImGui::Separator();

		ImGui::TextUnformatted( "Steering" );
		ImGui::Checkbox( "Invert steering (if A/D feels backwards)", &m_invertSteering );
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
							"If steering feels frozen at low speed again, raise Steering torque first." );
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

		return true;
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
	}

	void Render() override
	{
		Sample::Render();

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

		float steeringLeft = b3WheelJoint_GetSteeringAngle( m_vehicle.wheelJointIds[JOZZ_M5_FRONT_LEFT] );
		float steeringRight = b3WheelJoint_GetSteeringAngle( m_vehicle.wheelJointIds[JOZZ_M5_FRONT_RIGHT] );

		DrawTextLine( "Jozz Vehicle M5 First Drivable" );
		DrawTextLine( "W/S drive, A/D steer, Space brake, T third person, R restart" );
		DrawTextLine( "speed %.1f m/s (%.0f km/h)", GetJozzVehicleM5ForwardSpeed( m_vehicle ),
					  3.6f * GetJozzVehicleM5ForwardSpeed( m_vehicle ) );
		DrawTextLine( "steering %.1f/%.1f deg, %s, %s", 180.0f / B3_PI * steeringLeft, 180.0f / B3_PI * steeringRight,
					  m_config.allWheelDrive ? "AWD" : "RWD", m_config.uprightAssist ? "upright assist on" : "raw" );
		DrawTextLine( "wheel visuals: %s", ( m_showWheelVisuals && m_wheelVisual.IsLoaded() ) ? "glTF attached (visual-only)"
																							  : "off or not loaded" );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehicleM5FirstDrivable( context );
	}

	b3BodyId m_groundId;
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

	// Pending structural rig edits (geometry/mass); require "Apply rig rebuild".
	b3Vec3 m_editChassisHalfExtents;
	float m_editChassisDensity;
	float m_editAxleHalfSpacing;
	float m_editTrackHalfWidth;
	float m_editRestDrop;
	float m_editWheelDensity;
	bool m_structuralSetupDirty;
};

Sample* CreateJozzVehicleM5FirstDrivable( SampleContext* context )
{
	return JozzVehicleM5FirstDrivable::Create( context );
}
