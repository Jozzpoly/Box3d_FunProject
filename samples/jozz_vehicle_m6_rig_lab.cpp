// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_rig_lab.h"

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "implot.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_m5_test_course.h"
#include "jozz_vehicle_m6_suspension_rig.h"
#include "jozz_vehicle_visual_mesh.h"

#include "box3d/box3d.h"

// M6 Suspension Rig Lab: the first multi-body suspension vehicle. The physics
// rig lives in jozz_vehicle_m6_suspension_rig (shared with the headless
// validation smoke); this sample adds input, camera, the M5 test course, live
// tuning, hardpoint/link debug drawing, and the visual-only glTF wheels.
// The M5 First Drivable sample stays untouched as the strut baseline.
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

		m_showWheelVisuals = true;
		m_showPrimitiveWheelShapes = false;
		m_showRigDiagnostics = true;
		m_invertSteering = false;

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

	~JozzVehicleM6RigLab() override
	{
		DestroyVehicle();
		m_wheelVisual.Destroy();
		DestroyJozzVehicleM5TestCourse( &m_testCourse );
	}

	void SyncEditFromConfig()
	{
		m_editFrontRigType = m_config.frontRigType;
		m_editRearRigType = m_config.rearRigType;
		m_editWishbone = m_config.wishbone;
		m_editKnuckleMass = m_config.knuckleMass;
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
		m_config.knuckleMass = m_editKnuckleMass;
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

	float GetSpawnHeight() const
	{
		return m_config.restDrop + m_config.wheelEnvelope.radius + 0.05f;
	}

	void CreateVehicle()
	{
		DestroyVehicle();
		m_vehicle = CreateJozzVehicleM6( m_worldId, m_groundId, m_config, { 0.0f, GetSpawnHeight(), 0.0f } );
		UpdateWheelShapeVisibility();
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
				b3DistanceJoint_SetSpringHertz( runtime.coiloverJointId, m_config.suspensionHertz * scale );
				b3DistanceJoint_SetSpringDampingRatio( runtime.coiloverJointId, m_config.suspensionDampingRatio * scale );
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

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "Jozz Vehicle M6 Suspension Rig Lab" );
		ImGui::Separator();
		ImGui::TextWrapped( "First multi-body suspension: knuckle + wheel bodies per corner, wishbones as rigid link "
							"rods between hardpoints, coilover spring, physical steering rack with tie rods. Rig type "
							"selectable per axle; Integrated strut = the validated M5 wheel-joint model." );
		ImGui::TextUnformatted( "Input: W/S drive, A/D steer, Space brake, T third-person camera, R restart." );
		ImGui::Text( "speed %.1f m/s (%.0f km/h)", GetJozzVehicleM6ForwardSpeed( m_vehicle ),
					 3.6f * GetJozzVehicleM6ForwardSpeed( m_vehicle ) );
		ImGui::Separator();

		ImGui::TextUnformatted( "Rig type per axle (Apply)" );
		{
			const char* rigTypes[] = { "Integrated strut (M5 wheel joint)", "Double wishbone (multi-body)" };
			bool rigEdited = ImGui::Combo( "Front axle", &m_editFrontRigType, rigTypes, 2 );
			rigEdited |= ImGui::Combo( "Rear axle", &m_editRearRigType, rigTypes, 2 );
			if ( rigEdited )
			{
				m_structuralSetupDirty = true;
			}
		}
		ImGui::Separator();

		ImGui::TextUnformatted( "Drive" );
		ImGui::SliderFloat( "Drive torque", &m_config.maxDriveTorque, 0.0f, 6000.0f, "%.0f" );
		ImGui::SliderFloat( "Drive speed", &m_config.maxDriveSpeed, 0.0f, 150.0f, "%.0f rad/s" );
		ImGui::SliderFloat( "Brake torque", &m_config.brakeTorque, 0.0f, 6000.0f, "%.0f" );
		ImGui::Checkbox( "All wheel drive", &m_config.allWheelDrive );
		ImGui::Separator();

		ImGui::TextUnformatted( "Suspension (live)" );
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
		ImGui::Separator();

		ImGui::TextUnformatted( "Steering + drift feel" );
		ImGui::Checkbox( "Invert steering (preference)", &m_invertSteering );
		if ( ImGui::SliderFloat( "Steering hertz (rack/servo)", &m_config.steeringHertz, 0.5f, 40.0f, "%.2f" ) )
		{
			ApplySteeringTuning();
		}
		if ( ImGui::SliderFloat( "Steering damping ratio", &m_config.steeringDampingRatio, 0.0f, 6.0f, "%.2f" ) )
		{
			ApplySteeringTuning();
		}
		// Live: the drive update pushes this to the joint every step anyway.
		ImGui::SliderFloat( "Rack servo force", &m_config.rackServoForce, 0.0f, 30000.0f, "%.0f N" );
		ImGui::TextWrapped( "The rack servo is the power-steering muscle: it covers the parking torque a stationary "
							"loaded tire demands. Drop it low to feel an under-assisted rack that only steers while "
							"rolling." );
		ImGui::Checkbox( "Self-align assist (wheels follow the slide)", &m_config.selfAlignAssist );
		if ( m_config.selfAlignAssist )
		{
			ImGui::SliderFloat( "Self-align gain", &m_config.selfAlignGain, 0.0f, 1.0f, "%.2f" );
			ImGui::SliderFloat( "Self-align min speed", &m_config.selfAlignMinSpeed, 0.5f, 15.0f, "%.1f m/s" );
			ImGui::SliderFloat( "Self-align max angle", &m_config.selfAlignMaxSlipDeg, 5.0f, 45.0f, "%.0f deg" );
		}
		ImGui::TextWrapped( "Two layers of self-aligning torque: the wishbone geometry carries a physical caster "
							"angle (mechanical trail through the rack), and this assist blends the commanded rack "
							"angle toward the real travel direction when you ease off A/D during a slide." );
		ImGui::Separator();

		ImGui::TextUnformatted( "Wishbone geometry (Apply)" );
		{
			bool geometryEdited = false;
			geometryEdited |= ImGui::SliderFloat( "Caster", &m_editWishbone.casterDeg, -5.0f, 15.0f, "%.1f deg" );
			geometryEdited |= ImGui::SliderFloat( "Kingpin inclination", &m_editWishbone.kingpinInclinationDeg, -5.0f, 20.0f, "%.1f deg" );
			geometryEdited |= ImGui::SliderFloat( "Kingpin offset", &m_editWishbone.kingpinOffset, 0.05f, 0.40f, "%.2f m" );
			geometryEdited |= ImGui::SliderFloat( "Upright half height", &m_editWishbone.uprightHalfHeight, 0.08f, 0.40f, "%.2f m" );
			geometryEdited |= ImGui::SliderFloat( "Upper arm length", &m_editWishbone.upperArmLength, 0.15f, 0.90f, "%.2f m" );
			geometryEdited |= ImGui::SliderFloat( "Lower arm length", &m_editWishbone.lowerArmLength, 0.15f, 1.00f, "%.2f m" );
			geometryEdited |= ImGui::SliderFloat( "Arm half spread", &m_editWishbone.armHalfSpread, 0.10f, 0.50f, "%.2f m" );
			geometryEdited |= ImGui::SliderFloat( "Steering arm back", &m_editWishbone.steeringArmBack, 0.08f, 0.35f, "%.2f m" );
			geometryEdited |= ImGui::Checkbox( "Ackermann trapezoid (mechanical)", &m_editWishbone.ackermannTrapezoid );
			if ( m_editWishbone.ackermannTrapezoid )
			{
				geometryEdited |= ImGui::SliderFloat( "Ackermann fraction", &m_editWishbone.ackermannFraction, 0.0f, 1.0f, "%.2f" );
			}
			geometryEdited |= ImGui::SliderFloat( "Coilover top height", &m_editWishbone.coiloverTopHeight, 0.15f, 0.80f, "%.2f m" );
			geometryEdited |= ImGui::SliderFloat( "Knuckle mass", &m_editKnuckleMass, 8.0f, 80.0f, "%.0f kg" );
			geometryEdited |= ImGui::SliderFloat( "Strut caster (strut axles)", &m_editStrutCasterDeg, -5.0f, 15.0f, "%.1f deg" );
			if ( geometryEdited )
			{
				m_structuralSetupDirty = true;
			}
		}
		ImGui::TextWrapped( "More caster = stronger physical self-centering (drift setups run 7-10 deg). Longer lower "
							"arms = gentler camber gain (offroad); short arms = aggressive street geometry." );
		ImGui::Separator();

		ImGui::TextUnformatted( "Wheels" );
		if ( ImGui::SliderFloat( "Tire friction", &m_config.wheelFriction, 0.1f, 4.0f, "%.2f" ) )
		{
			ApplyWheelFriction();
		}
		{
			const char* envelopes[] = { "Sphere (smooth, bulges laterally)", "Cylinder 32 (true width, faceted)",
										"Phased union (measured dead end)",
										"Split: sphere rolls, true-width sidewall (default)" };
			bool envelopeEdited = ImGui::Combo( "Collision envelope (Apply)", &m_editEnvelopeMode, envelopes, 4 );
			if ( m_editEnvelopeMode == JOZZ_M6_ENVELOPE_PHASED_UNION )
			{
				envelopeEdited |= ImGui::SliderInt( "Union layers (Apply)", &m_editEnvelopeLayers, 2, 4 );
			}
			if ( envelopeEdited )
			{
				m_structuralSetupDirty = true;
			}
		}
		ImGui::TextWrapped( "Envelope width follows the tire markers from the asset audit (M3A), so imported wheel "
							"models get their collision size automatically. Split = the M6 default: the smooth sphere "
							"only touches terrain-tagged surfaces (no facet washboard), while props/walls/curbs meet a "
							"cylinder at the true tire width - no more invisible side bulge. Phased union stays as the "
							"measured negative result: contact hopping between stacked hulls rolls WORSE than one "
							"cylinder." );
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

		ImGui::Checkbox( "glTF wheel visuals", &m_showWheelVisuals );
		if ( ImGui::Checkbox( "Primitive wheel shapes", &m_showPrimitiveWheelShapes ) )
		{
			UpdateWheelShapeVisibility();
		}
		ImGui::Checkbox( "Rig diagnostics (links/hardpoints)", &m_showRigDiagnostics );

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
		ImGui::TextUnformatted( "Structural setup (Apply, rebuilds the vehicle)" );
		bool structuralEdited = false;
		structuralEdited |= ImGui::SliderFloat( "Chassis half length", &m_editChassisHalfExtents.x, 0.3f, 4.0f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Chassis half height", &m_editChassisHalfExtents.y, 0.1f, 1.5f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Chassis half width", &m_editChassisHalfExtents.z, 0.2f, 2.0f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Chassis density", &m_editChassisDensity, 20.0f, 2000.0f, "%.0f" );
		structuralEdited |= ImGui::SliderFloat( "CG drop", &m_editCgVerticalOffset, -0.3f, 0.8f, "%.2f m" );
		structuralEdited |= ImGui::SliderFloat( "Axle half spacing", &m_editAxleHalfSpacing, 0.4f, 4.0f, "%.2f" );
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
			m_config =
				JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
			CreateVehicle();
			SyncEditFromConfig();
		}

		ImGui::TextWrapped( "%s", m_wheelVisual.status.c_str() );
		ImGui::TextWrapped( "metadata: %s", m_assetMetadata.status.c_str() );

		ImGui::Separator();
		DrawTelemetryPanel();

		return true;
	}

	void DrawTelemetryPanel()
	{
		ImGui::TextUnformatted( "Telemetry (last 10 s)" );

		const char* cornerNames[JOZZ_M6_CORNER_COUNT] = { "FL", "FR", "RL", "RR" };
		JozzVehicleM6WheelTelemetry current[JOZZ_M6_CORNER_COUNT];
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			current[corner] = GetJozzVehicleM6WheelTelemetry( m_vehicle, corner );
		}

		ImGui::Text( "ground contact: FL %s  FR %s  RL %s  RR %s", current[0].groundContact ? "YES" : "air",
					 current[1].groundContact ? "YES" : "air", current[2].groundContact ? "YES" : "air",
					 current[3].groundContact ? "YES" : "air" );
		ImGui::Text( "load N: FL %.0f  FR %.0f  RL %.0f  RR %.0f", current[0].suspensionLoad, current[1].suspensionLoad,
					 current[2].suspensionLoad, current[3].suspensionLoad );
		ImGui::Text( "slip deg: FL %.1f  FR %.1f  RL %.1f  RR %.1f", 180.0f / B3_PI * current[0].slipAngle,
					 180.0f / B3_PI * current[1].slipAngle, 180.0f / B3_PI * current[2].slipAngle,
					 180.0f / B3_PI * current[3].slipAngle );
		ImGui::Text( "camber deg: FL %.1f  FR %.1f  RL %.1f  RR %.1f", 180.0f / B3_PI * current[0].camberAngle,
					 180.0f / B3_PI * current[1].camberAngle, 180.0f / B3_PI * current[2].camberAngle,
					 180.0f / B3_PI * current[3].camberAngle );

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
		if ( ImPlot::BeginPlot( "Suspension travel", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "travel m" );
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

		if ( ImPlot::BeginPlot( "Slip angle (drift telemetry)", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "slip deg" );
			ImPlot::SetupAxisLimits( ImAxis_X1, latestTime - 10.0, latestTime, ImPlotCond_Always );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				ImPlot::PlotLine( cornerNames[corner], times, series[corner], count );
			}
			ImPlot::EndPlot();
		}
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
		// Draw the link rods between their live endpoints so the multi-body
		// geometry is visible without any suspension model mounted yet:
		// yellow = upper arms, orange = lower arms, green = coilover,
		// cyan = tie/toe link, magenta = kingpin axis.
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
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

			b3Pos upperBall = b3Body_GetWorldPoint( runtime.knuckleId, b3Sub( hp.upperBallJoint, wheelCenterLocal ) );
			b3Pos lowerBall = b3Body_GetWorldPoint( runtime.knuckleId, b3Sub( hp.lowerBallJoint, wheelCenterLocal ) );
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

			if ( B3_IS_NON_NULL( m_vehicle.rackId ) &&
				 ( corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT ) )
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

	void Render() override
	{
		Sample::Render();

		if ( m_vehicle.valid == false )
		{
			return;
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

		if ( m_showRigDiagnostics )
		{
			DrawRigDiagnostics();
		}

		JozzVehicleM6WheelTelemetry frontLeft = GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_FRONT_LEFT );
		JozzVehicleM6WheelTelemetry frontRight = GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_FRONT_RIGHT );

		DrawTextLine( "Jozz Vehicle M6 Suspension Rig Lab" );
		DrawTextLine( "W/S drive, A/D steer, Space brake, T third person, R restart" );
		DrawTextLine( "speed %.1f m/s (%.0f km/h)  align %.1f deg", GetJozzVehicleM6ForwardSpeed( m_vehicle ),
					  3.6f * GetJozzVehicleM6ForwardSpeed( m_vehicle ),
					  180.0f / B3_PI * GetJozzVehicleM6AlignmentAngle( m_vehicle ) );
		DrawTextLine( "front rig: %s, rear rig: %s",
					  m_config.frontRigType == JOZZ_M6_RIG_DOUBLE_WISHBONE ? "double wishbone" : "strut",
					  m_config.rearRigType == JOZZ_M6_RIG_DOUBLE_WISHBONE ? "double wishbone" : "strut" );
		DrawTextLine( "steering %.1f/%.1f deg, slip %.1f/%.1f deg", 180.0f / B3_PI * frontLeft.steeringAngle,
					  180.0f / B3_PI * frontRight.steeringAngle, 180.0f / B3_PI * frontLeft.slipAngle,
					  180.0f / B3_PI * frontRight.slipAngle );
		DrawTextLine( "contact FL:%s FR:%s RL:%s RR:%s, self-align %s",
					  frontLeft.groundContact ? "Y" : "-", frontRight.groundContact ? "Y" : "-",
					  GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_REAR_LEFT ).groundContact ? "Y" : "-",
					  GetJozzVehicleM6WheelTelemetry( m_vehicle, JOZZ_M6_REAR_RIGHT ).groundContact ? "Y" : "-",
					  m_config.selfAlignAssist ? "on" : "off" );
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
	float m_metersPerBlockbenchUnit;
	bool m_showWheelVisuals;
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
	float m_editKnuckleMass;
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
