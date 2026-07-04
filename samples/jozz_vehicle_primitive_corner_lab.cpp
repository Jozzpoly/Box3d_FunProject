// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_primitive_corner_lab.h"

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_corner_rig.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_debug_preview.h"
#include "jozz_vehicle_visual_asset.h"
#include "jozz_vehicle_visual_mesh.h"

#include "box3d/box3d.h"

class JozzVehiclePrimitiveCornerM2 : public Sample
{
public:
	explicit JozzVehiclePrimitiveCornerM2( SampleContext* context )
		: Sample( context )
	{
		m_chassisId = b3_nullBodyId;
		m_wheelId = b3_nullBodyId;
		m_wheelShapeId = b3_nullShapeId;
		m_jointId = b3_nullJointId;

		if ( context->restart == false )
		{
			m_camera->SetView( 35.0f, 22.0f, 10.0f, { 0.0f, 1.1f, 0.0f } );
		}

		SetDefaults();
		AddGroundBox( 20.0f );
		CreateCorner();
		LoadStaticWheelVisualProof();
		LoadSuspensionVisualProof();
	}

	~JozzVehiclePrimitiveCornerM2() override
	{
		m_staticWheelVisual.Destroy();
		m_suspensionVisual.Destroy();
	}

	void SyncEditSetupFromCommitted()
	{
		m_editWheelRadius = m_wheelRadius;
		m_editWheelWidth = m_wheelWidth;
		m_editRigHeight = m_rigHeight;
		m_editRestDrop = m_restDrop;
		m_editCollideConnected = m_collideConnected;
		m_structuralSetupDirty = false;
	}

	void SetDefaults()
	{
		m_assetMetadata = LoadJozzVehicleAuditMetadata();
		JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_assetMetadata );
		m_assetMetersPerBlockbenchUnit = defaults.metersPerBlockbenchUnit;
		m_assetWheelRadiusDefault = defaults.wheelRadius;
		m_assetWheelWidthDefault = defaults.wheelWidth;
		m_assetSuspensionTravelHint = defaults.assetSuspensionTravelHint;

		m_wheelRadius = defaults.wheelRadius;
		m_wheelWidth = defaults.wheelWidth;

		m_chassisHalfHeight = defaults.chassisHalfHeight;
		m_rigHeight = defaults.rigHeight;
		m_restDrop = defaults.restDrop;
		m_reboundTravel = defaults.reboundTravel;
		m_compressionTravel = defaults.compressionTravel;
		m_liveRootOffset = 0.0f;
		m_liveRootSpeed = 2.5f;
		m_collideConnected = false;
		m_showAxisDiagnostics = true;
		m_showAssetSemanticPreview = true;
		m_showStaticWheelVisualProof = true;
		m_showAttachedWheelVisual = true;
		m_showPrimitiveWheelDebugShape = true;
		m_showSuspensionMountVisual = true;
		m_showSuspensionContractPoints = true;
		m_showSuspensionMovingEndpoints = true;
		m_structuralSetupDirty = false;
		m_suspensionContract = LoadJozzVehicleAssetContract( "one_sided_wheel_mount.asset.json" );

		m_enableSuspension = true;
		m_enableSuspensionLimit = true;
		m_enableSpinMotor = true;
		m_suspensionHertz = 3.0f;
		m_suspensionDampingRatio = 0.75f;
		m_driveSpeed = 14.0f;
		m_maxSpinTorque = 90.0f;
		m_brakeTorque = 180.0f;

		SyncEditSetupFromCommitted();
	}

	void LoadStaticWheelVisualProof()
	{
		m_staticWheelVisual.Destroy();
		const char* candidates[] = {
			"assets/source/Offroad_Big_Wheels.gltf",
			"../assets/source/Offroad_Big_Wheels.gltf",
			"../../assets/source/Offroad_Big_Wheels.gltf",
			"../../../assets/source/Offroad_Big_Wheels.gltf",
			"../../../../assets/source/Offroad_Big_Wheels.gltf",
		};

		m_staticWheelVisualSource.clear();
		for ( const char* path : candidates )
		{
			if ( m_staticWheelVisual.LoadStaticGltf( path, m_assetMetersPerBlockbenchUnit ) )
			{
				m_staticWheelVisualSource = path;
				return;
			}
		}
	}

	void LoadSuspensionVisualProof()
	{
		m_suspensionVisual.LoadFromContract( m_suspensionContract );
	}

	float GetChassisMountY() const
	{
		return m_rigHeight - m_chassisHalfHeight;
	}

	float GetRestWheelCenterY() const
	{
		return GetChassisMountY() - m_restDrop;
	}

	float GetLiveRigHeight() const
	{
		return m_rigHeight + m_liveRootOffset;
	}

	float GetLiveChassisMountY() const
	{
		return GetChassisMountY() + m_liveRootOffset;
	}

	float GetLiveRestWheelCenterY() const
	{
		return GetRestWheelCenterY() + m_liveRootOffset;
	}

	float GetLowerSuspensionLimit() const
	{
		return -m_reboundTravel;
	}

	float GetUpperSuspensionLimit() const
	{
		return m_compressionTravel;
	}

	b3Pos GetWheelAuditPreviewOrigin( b3Pos wheelPosition ) const
	{
		return { wheelPosition.x - 1.35f, wheelPosition.y, wheelPosition.z + 1.45f };
	}

	b3Pos GetSuspensionAuditPreviewOrigin() const
	{
		// The suspension schematic is chassis/root anchored. Rest drop must not
		// drag this whole preview because that would imply suspension semantics are
		// owned by the wheel rest center.
		return { 1.35f, GetLiveChassisMountY(), 1.45f };
	}

	b3Vec3 GetWheelVisualCenterMeters() const
	{
		if ( m_staticWheelVisual.IsLoaded() )
		{
			return {
				0.5f * ( m_staticWheelVisual.boundsMin.x + m_staticWheelVisual.boundsMax.x ),
				0.5f * ( m_staticWheelVisual.boundsMin.y + m_staticWheelVisual.boundsMax.y ),
				0.5f * ( m_staticWheelVisual.boundsMin.z + m_staticWheelVisual.boundsMax.z ),
			};
		}

		b3Vec3 wheelMountBU =
			JozzVehicleFindPointOrFallback( m_assetMetadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount", { 0.25f, 0.5f, 0.0f } );
		b3Vec3 radiusOuterBU = JozzVehicleFindPointOrFallback( m_assetMetadata, "Offroad_Big_Wheels.gltf",
															   "Marker_TireRadiusOuter", { -0.125f, 1.96875f, 0.0f } );
		b3Vec3 wheelCenterBU = { radiusOuterBU.x, wheelMountBU.y, wheelMountBU.z };
		return b3MulSV( m_assetMetersPerBlockbenchUnit, wheelCenterBU );
	}

	b3Transform GetAttachedWheelVisualCorrection() const
	{
		// Visual-only correction for the current Offroad_Big_Wheels.gltf proof.
		// The authored wheel spin/width axis is +X, while the primitive wheel
		// body uses local +Y as its axle. This maps authored +X to body +Y and
		// centers the audited wheel center on the primitive body origin.
		b3Quat visualXToBodyY = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 0.5f * B3_PI );
		b3Quat visualUpToBodyRadial = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
		b3Quat correctionRotation = b3MulQuat( visualUpToBodyRadial, visualXToBodyY );
		b3Vec3 visualCenter = GetWheelVisualCenterMeters();
		return { b3Neg( b3RotateVector( correctionRotation, visualCenter ) ), correctionRotation };
	}

	JozzVehicleCornerRigState GetCornerRigState() const
	{
		b3Pos liveChassisMount = { 0.0f, GetLiveChassisMountY(), 0.0f };
		b3Pos liveRestWheelCenter = { 0.0f, GetLiveRestWheelCenterY(), 0.0f };
		return MakeJozzVehicleCornerRigState( liveChassisMount, liveRestWheelCenter, b3Body_GetTransform( m_wheelId ) );
	}

	bool TryGetSuspensionContractWorldPoint( b3WorldTransform transform, const char* role, b3Pos* out ) const
	{
		const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( m_suspensionContract, role );
		if ( binding == nullptr || binding->resolved == false )
		{
			return false;
		}

		*out = TransformJozzVehicleContractPoint( transform, *binding );
		return true;
	}

	void UpdatePrimitiveWheelDebugVisibility()
	{
		if ( B3_IS_NULL( m_wheelShapeId ) )
		{
			return;
		}

		SetShapeHidden( m_wheelShapeId, m_showPrimitiveWheelDebugShape == false );
	}

	void ClampLiveRootOffset()
	{
		if ( m_liveRootOffset < -1.50f )
		{
			m_liveRootOffset = -1.50f;
		}
		else if ( m_liveRootOffset > 2.50f )
		{
			m_liveRootOffset = 2.50f;
		}
	}

	void ApplyLiveRootTransform()
	{
		ClampLiveRootOffset();

		if ( B3_IS_NON_NULL( m_chassisId ) )
		{
			b3Body_SetTransform( m_chassisId, { 0.0f, GetLiveRigHeight(), 0.0f }, b3Quat_identity );
		}

		if ( B3_IS_NON_NULL( m_jointId ) )
		{
			b3Joint_WakeBodies( m_jointId );
		}
	}

	void ApplyPendingStructuralSetup()
	{
		m_wheelRadius = m_editWheelRadius;
		m_wheelWidth = m_editWheelWidth;
		m_rigHeight = m_editRigHeight;
		m_restDrop = m_editRestDrop;
		m_collideConnected = m_editCollideConnected;
		CreateCorner();
		SyncEditSetupFromCommitted();
	}

	void ApplyTravelLimits()
	{
		if ( B3_IS_NON_NULL( m_jointId ) )
		{
			b3WheelJoint_SetSuspensionLimits( m_jointId, GetLowerSuspensionLimit(), GetUpperSuspensionLimit() );
			b3Joint_WakeBodies( m_jointId );
		}
	}

	void DestroyCorner()
	{
		if ( B3_IS_NON_NULL( m_jointId ) )
		{
			b3DestroyJoint( m_jointId, false );
			m_jointId = b3_nullJointId;
		}

		if ( B3_IS_NON_NULL( m_wheelId ) )
		{
			if ( B3_IS_NULL( m_wheelShapeId ) == false )
			{
				SetShapeHidden( m_wheelShapeId, false );
				m_wheelShapeId = b3_nullShapeId;
			}

			b3DestroyBody( m_wheelId );
			m_wheelId = b3_nullBodyId;
		}

		if ( B3_IS_NON_NULL( m_chassisId ) )
		{
			b3DestroyBody( m_chassisId );
			m_chassisId = b3_nullBodyId;
		}
	}

	void CreateCorner()
	{
		DestroyCorner();

		const float liveRestWheelCenterY = GetLiveRestWheelCenterY();

		// Static chassis rig: this isolates the wheel joint before a full dynamic
		// vehicle body adds mass distribution and roll/pitch problems.
		{
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.position = { 0.0f, GetLiveRigHeight(), 0.0f };
			bodyDef.name = "jozz_m2_static_chassis_rig";
			m_chassisId = b3CreateBody( m_worldId, &bodyDef );

			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.baseMaterial.friction = 0.8f;
			if ( m_collideConnected == false )
			{
				// Belt-and-suspenders with joint collideConnected=false. This makes the
				// lab unambiguous when testing clearance against the chassis block.
				shapeDef.filter.groupIndex = -17;
			}

			b3BoxHull chassis = b3MakeBoxHull( 1.25f, m_chassisHalfHeight, 0.55f );
			b3CreateHullShape( m_chassisId, &shapeDef, &chassis.base );
		}

		{
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.type = b3_dynamicBody;
			bodyDef.position = { 0.0f, liveRestWheelCenterY, 0.0f };

			// b3CreateCylinder builds along local Y. Rotate local Y onto world Z,
			// matching the stock Box3D wheel-joint sample and giving the wheel a
			// visible axle across Z while it rolls along X.
			bodyDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
			bodyDef.allowFastRotation = true;
			bodyDef.name = "jozz_m2_centered_primitive_wheel";
			m_wheelId = b3CreateBody( m_worldId, &bodyDef );

			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.density = 20.0f;
			shapeDef.baseMaterial.friction = 1.25f;
			shapeDef.baseMaterial.restitution = 0.02f;
			shapeDef.baseMaterial.rollingResistance = 0.02f;
			if ( m_collideConnected == false )
			{
				shapeDef.filter.groupIndex = -17;
			}

			// API order is height, radius, yOffset, sides. The cylinder must be
			// centered on body origin because frame B is the wheel body center.
			b3HullData* wheelHull = b3CreateCylinder( m_wheelWidth, m_wheelRadius, -0.5f * m_wheelWidth, 32 );
			m_wheelShapeId = b3CreateHullShape( m_wheelId, &shapeDef, wheelHull );
			b3DestroyHull( wheelHull );
			UpdatePrimitiveWheelDebugVisibility();
		}

		b3WheelJointDef jointDef = b3DefaultWheelJointDef();
		jointDef.base.bodyIdA = m_chassisId;
		jointDef.base.bodyIdB = m_wheelId;

		// M2.5 keeps the M2.4 rest-anchor model. The live root offset moves the
		// chassis body itself; because frame A is local to the chassis, the rest
		// wheel center follows the root without changing rest drop or travel setup.
		b3Pos liveRestWheelCenter = { 0.0f, liveRestWheelCenterY, 0.0f };
		jointDef.base.localFrameA.p = b3Body_GetLocalPoint( m_chassisId, liveRestWheelCenter );
		jointDef.base.localFrameB.p = b3Vec3_zero;

		// Wheel-joint convention: wheel translates along local X in frame A and
		// rotates around local Z in frame B. We map frame-A X onto world Y.
		jointDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
		jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		jointDef.base.collideConnected = m_collideConnected;
		jointDef.base.drawScale = 1.5f;

		jointDef.enableSuspensionSpring = m_enableSuspension;
		jointDef.suspensionHertz = m_suspensionHertz;
		jointDef.suspensionDampingRatio = m_suspensionDampingRatio;
		jointDef.enableSuspensionLimit = m_enableSuspensionLimit;
		jointDef.lowerSuspensionLimit = GetLowerSuspensionLimit();
		jointDef.upperSuspensionLimit = GetUpperSuspensionLimit();
		jointDef.enableSpinMotor = m_enableSpinMotor;
		jointDef.spinSpeed = 0.0f;
		jointDef.maxSpinTorque = 0.0f;

		m_jointId = b3CreateWheelJoint( m_worldId, &jointDef );
		m_structuralSetupDirty = false;
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug" );
		ImGui::Separator();
		ImGui::TextWrapped( "Primitive one-corner wheel-joint lab. M3A centralizes wheel primitive defaults; M3B shows semantic metadata; M3B.2.1 keeps a static textured visual proof; M3B.3 attaches the same visual-only wheel mesh to the primitive wheel body." );
		ImGui::Spacing();
		ImGui::TextUnformatted( "Input: W drive forward, S reverse, Space brake, Q lower root, E raise root, R restart sample." );
		ImGui::Text( "wheel radius %.2f m, width %.2f m", m_wheelRadius, m_wheelWidth );
		ImGui::Text( "asset defaults: scale %.2f m/BU, wheel r %.2f, width %.2f", m_assetMetersPerBlockbenchUnit,
					 m_assetWheelRadiusDefault, m_assetWheelWidthDefault );
		ImGui::Text( "asset travel hint %.2f m; rest drop %.2f m is explicit/tuned", m_assetSuspensionTravelHint, m_restDrop );
		ImGui::TextWrapped( "metadata: %s", m_assetMetadata.status.c_str() );
		if ( m_assetMetadata.loadedFromRuntimeReport )
		{
			ImGui::TextWrapped( "source: %s", m_assetMetadata.sourcePath.c_str() );
		}
		ImGui::Text( "live root %.2f, live rest center y %.2f", m_liveRootOffset, GetLiveRestWheelCenterY() );
		ImGui::Text( "relative travel: rebound %.2f down, compression %.2f up", m_reboundTravel, m_compressionTravel );
		ImGui::Separator();

		ImGui::TextUnformatted( "Live root stress test" );
		if ( ImGui::SliderFloat( "Live root offset", &m_liveRootOffset, -1.50f, 2.50f, "%.2f" ) )
		{
			ApplyLiveRootTransform();
		}
		if ( ImGui::SliderFloat( "Live root key speed", &m_liveRootSpeed, 0.10f, 8.00f, "%.2f m/s" ) )
		{
			if ( m_liveRootSpeed < 0.10f )
			{
				m_liveRootSpeed = 0.10f;
			}
		}
		if ( ImGui::Button( "Reset live root" ) )
		{
			m_liveRootOffset = 0.0f;
			ApplyLiveRootTransform();
		}
		ImGui::TextWrapped( "This moves only the chassis/root in realtime. The wheel is not teleported, so the suspension and ground contact are stressed." );
		ImGui::Separator();

		ImGui::TextUnformatted( "Structural rig setup" );
		bool structuralEdited = false;
		structuralEdited |= ImGui::SliderFloat( "Rig height", &m_editRigHeight, 0.75f, 3.50f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Rest drop", &m_editRestDrop, 0.25f, 2.00f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Wheel radius", &m_editWheelRadius, 0.20f, 0.90f, "%.2f" );
		structuralEdited |= ImGui::SliderFloat( "Wheel width", &m_editWheelWidth, 0.12f, 0.90f, "%.2f" );
		structuralEdited |= ImGui::Checkbox( "Wheel collides with chassis", &m_editCollideConnected );
		if ( structuralEdited )
		{
			m_structuralSetupDirty = true;
		}
		ImGui::Checkbox( "Axis diagnostics", &m_showAxisDiagnostics );
		ImGui::Checkbox( "M3B semantic preview", &m_showAssetSemanticPreview );
		ImGui::TextWrapped( "M3B preview: wheel schematic follows the wheel body; suspension schematic follows chassis/root. It is not final glTF transform and does not drive physics." );
		ImGui::Checkbox( "M3B.2.1 static textured wheel proof", &m_showStaticWheelVisualProof );
		ImGui::Checkbox( "M3B.3 attached textured wheel visual", &m_showAttachedWheelVisual );
		if ( ImGui::Checkbox( "Primitive wheel debug shape", &m_showPrimitiveWheelDebugShape ) )
		{
			UpdatePrimitiveWheelDebugVisibility();
		}
		ImGui::TextWrapped( "Primitive debug is the orange collision wheel only. Hiding it does not disable the physics body, collision, or wheel joint." );
		ImGui::TextWrapped( "%s", m_staticWheelVisual.status.c_str() );
		ImGui::TextWrapped( "%s", m_staticWheelVisual.textureStatus.c_str() );
		if ( m_staticWheelVisual.IsLoaded() )
		{
			ImGui::TextWrapped( "visual source: %s", m_staticWheelVisualSource.c_str() );
			ImGui::Text( "visual verts %d, tris %d", m_staticWheelVisual.vertexCount, m_staticWheelVisual.triangleCount );
			if ( m_staticWheelVisual.textureLoaded )
			{
				ImGui::Text( "texture %dx%d", m_staticWheelVisual.textureWidth, m_staticWheelVisual.textureHeight );
			}
		}
		ImGui::TextWrapped( "M3A note: visual sockets are not physics frame A; rest drop remains explicit until a dedicated physics rest-anchor contract exists." );
		ImGui::Separator();
		ImGui::TextUnformatted( "M4 suspension foundation" );
		ImGui::Checkbox( "M4A suspension mount visual", &m_showSuspensionMountVisual );
		ImGui::Checkbox( "M4A suspension contract points", &m_showSuspensionContractPoints );
		ImGui::Checkbox( "M4B moving endpoint preview", &m_showSuspensionMovingEndpoints );
		ImGui::TextWrapped( "contract: %s", m_suspensionContract.status.c_str() );
		if ( m_suspensionContract.loaded )
		{
			ImGui::TextWrapped( "contract source: %s", m_suspensionContract.sourcePath.c_str() );
			ImGui::Text( "contract bindings %d, warnings %d", (int)m_suspensionContract.bindings.size(),
						 (int)m_suspensionContract.warnings.size() );
		}
		ImGui::TextWrapped( "%s", m_suspensionVisual.status.c_str() );
		ImGui::TextWrapped( "%s", m_suspensionVisual.mesh.status.c_str() );
		ImGui::TextWrapped( "%s", m_suspensionVisual.mesh.textureStatus.c_str() );
		ImGui::TextWrapped( "M4 note: suspension sockets are visual/debug bindings only. They do not define b3WheelJoint frames or restDrop." );

		if ( m_structuralSetupDirty )
		{
			ImGui::TextWrapped( "Pending structural change. Press Apply to rebuild bodies/joint cleanly. Live root uses the committed setup until Apply." );
		}

		if ( ImGui::Button( "Apply rig rebuild" ) )
		{
			ApplyPendingStructuralSetup();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Reload metadata + reset defaults" ) )
		{
			SetDefaults();
			CreateCorner();
			LoadStaticWheelVisualProof();
			LoadSuspensionVisualProof();
		}

		ImGui::Separator();

		if ( ImGui::Checkbox( "Suspension spring", &m_enableSuspension ) )
		{
			b3WheelJoint_EnableSuspension( m_jointId, m_enableSuspension );
			b3Joint_WakeBodies( m_jointId );
		}

		if ( m_enableSuspension )
		{
			if ( ImGui::SliderFloat( "Spring hertz", &m_suspensionHertz, 0.0f, 12.0f, "%.2f" ) )
			{
				b3WheelJoint_SetSuspensionHertz( m_jointId, m_suspensionHertz );
				b3Joint_WakeBodies( m_jointId );
			}

			if ( ImGui::SliderFloat( "Damping ratio", &m_suspensionDampingRatio, 0.0f, 3.0f, "%.2f" ) )
			{
				b3WheelJoint_SetSuspensionDampingRatio( m_jointId, m_suspensionDampingRatio );
				b3Joint_WakeBodies( m_jointId );
			}
		}

		ImGui::Separator();

		if ( ImGui::Checkbox( "Suspension limit", &m_enableSuspensionLimit ) )
		{
			b3WheelJoint_EnableSuspensionLimit( m_jointId, m_enableSuspensionLimit );
			b3Joint_WakeBodies( m_jointId );
		}

		if ( m_enableSuspensionLimit )
		{
			if ( ImGui::SliderFloat( "Rebound travel down", &m_reboundTravel, 0.0f, 1.50f, "%.2f" ) )
			{
				ApplyTravelLimits();
			}

			if ( ImGui::SliderFloat( "Compression travel up", &m_compressionTravel, 0.0f, 1.50f, "%.2f" ) )
			{
				ApplyTravelLimits();
			}
		}

		ImGui::Separator();

		if ( ImGui::Checkbox( "Spin motor", &m_enableSpinMotor ) )
		{
			b3WheelJoint_EnableSpinMotor( m_jointId, m_enableSpinMotor );
			b3Joint_WakeBodies( m_jointId );
		}

		if ( m_enableSpinMotor )
		{
			if ( ImGui::SliderFloat( "Drive speed", &m_driveSpeed, 0.0f, 40.0f, "%.1f" ) )
			{
				b3Joint_WakeBodies( m_jointId );
			}

			if ( ImGui::SliderFloat( "Drive torque", &m_maxSpinTorque, 0.0f, 400.0f, "%.0f" ) )
			{
				b3Joint_WakeBodies( m_jointId );
			}

			if ( ImGui::SliderFloat( "Brake torque", &m_brakeTorque, 0.0f, 800.0f, "%.0f" ) )
			{
				b3Joint_WakeBodies( m_jointId );
			}
		}

		return true;
	}

	void Step() override
	{
		float rootDirection = 0.0f;
		if ( IsKeyDown( KEY_Q ) )
		{
			rootDirection -= 1.0f;
		}
		if ( IsKeyDown( KEY_E ) )
		{
			rootDirection += 1.0f;
		}

		if ( rootDirection != 0.0f )
		{
			float dt = m_context->hertz > 0.0f ? 1.0f / m_context->hertz : 1.0f / 60.0f;
			m_liveRootOffset += rootDirection * m_liveRootSpeed * dt;
			ApplyLiveRootTransform();
		}

		if ( m_enableSpinMotor )
		{
			bool driveForward = IsKeyDown( KEY_W );
			bool driveReverse = IsKeyDown( KEY_S );
			bool braking = IsKeyDown( KEY_SPACE );

			float targetSpeed = 0.0f;
			float torque = 0.0f;

			if ( driveForward && !driveReverse )
			{
				targetSpeed = -m_driveSpeed;
				torque = m_maxSpinTorque;
			}
			else if ( driveReverse && !driveForward )
			{
				targetSpeed = m_driveSpeed;
				torque = m_maxSpinTorque;
			}

			if ( braking )
			{
				targetSpeed = 0.0f;
				torque = m_brakeTorque;
			}

			b3WheelJoint_SetSpinMotorSpeed( m_jointId, targetSpeed );
			b3WheelJoint_SetMaxSpinTorque( m_jointId, torque );

			if ( torque > 0.0f )
			{
				b3Joint_WakeBodies( m_jointId );
			}
		}

		Sample::Step();
	}

	void Render() override
	{
		UpdatePrimitiveWheelDebugVisibility();
		Sample::Render();

		b3Pos wheelPosition = b3Body_GetPosition( m_wheelId );
		b3Vec3 wheelVelocity = b3Body_GetLinearVelocity( m_wheelId );
		JozzVehicleCornerRigState cornerRig = GetCornerRigState();
		b3Pos liveChassisMount = cornerRig.liveChassisMount;
		b3Pos liveRestWheelCenter = cornerRig.liveRestWheelCenter;
		float actualTranslation = (float)( wheelPosition.y - liveRestWheelCenter.y );

		if ( m_showAxisDiagnostics )
		{
			b3WorldTransform wheelTransform = b3Body_GetTransform( m_wheelId );
			DrawAxes( wheelTransform, m_wheelRadius + 0.25f );
			DrawCross( wheelPosition, 0.12f, MakeVec4( 1.0f, 1.0f, 0.0f, 1.0f ) );
			DrawCross( liveChassisMount, 0.12f, MakeVec4( 0.1f, 0.8f, 1.0f, 1.0f ) );
			DrawCross( liveRestWheelCenter, 0.10f, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
			DrawLine( liveChassisMount, liveRestWheelCenter, MakeVec4( 0.1f, 0.8f, 1.0f, 1.0f ) );
			DrawLine( liveRestWheelCenter, wheelPosition, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );

			b3Vec3 axle = b3Body_GetWorldVector( m_wheelId, b3Vec3_axisY );
			float halfAxle = 0.5f * m_wheelWidth + 0.25f;
			b3Pos axleA = { wheelPosition.x - halfAxle * axle.x, wheelPosition.y - halfAxle * axle.y,
						  wheelPosition.z - halfAxle * axle.z };
			b3Pos axleB = { wheelPosition.x + halfAxle * axle.x, wheelPosition.y + halfAxle * axle.y,
						  wheelPosition.z + halfAxle * axle.z };
			DrawLine( axleA, axleB, MakeVec4( 1.0f, 0.85f, 0.1f, 1.0f ) );
		}

		if ( m_showAssetSemanticPreview )
		{
			JozzVehicleSemanticPreviewConfig preview = {};
			preview.metadata = &m_assetMetadata;
			preview.metersPerBlockbenchUnit = m_assetMetersPerBlockbenchUnit;
			preview.wheelPosition = wheelPosition;
			preview.wheelPreviewOrigin = GetWheelAuditPreviewOrigin( wheelPosition );
			preview.suspensionPreviewOrigin = GetSuspensionAuditPreviewOrigin();
			DrawJozzVehicleSemanticPreview( preview );
		}

		if ( m_showStaticWheelVisualProof && m_staticWheelVisual.IsLoaded() )
		{
			b3Pos visualOrigin = { -2.75f, 0.65f, -1.65f };
			m_staticWheelVisual.Draw( visualOrigin, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
			DrawAxes( { visualOrigin, b3Quat_identity }, 0.55f );
			DrawString3D( { visualOrigin.x, visualOrigin.y + 1.15f, visualOrigin.z }, MakeVec4( 0.95f, 0.95f, 0.95f, 1.0f ),
						  "M3B.2.1 static textured glTF visual proof" );
		}

		if ( m_showAttachedWheelVisual && m_staticWheelVisual.IsLoaded() )
		{
			b3WorldTransform wheelTransform = b3Body_GetTransform( m_wheelId );
			b3WorldTransform visualTransform = b3MulWorldTransforms( wheelTransform, GetAttachedWheelVisualCorrection() );
			m_staticWheelVisual.DrawAtTransform( visualTransform, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
			DrawAxes( visualTransform, 0.45f );
		}

		JozzVehicleSuspensionVisualFrame suspensionFrame =
			MakeJozzVehicleSuspensionVisualFrame( m_suspensionContract, cornerRig );
		if ( suspensionFrame.valid )
		{
			if ( m_showSuspensionMountVisual && m_suspensionVisual.IsLoaded() )
			{
				m_suspensionVisual.DrawAtTransform( suspensionFrame.mountTransform, MakeVec4( 1.0f, 1.0f, 1.0f, 0.72f ) );
			}

			if ( m_showSuspensionContractPoints )
			{
				DrawCross( suspensionFrame.contractWheelCenterWorld, 0.10f, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
				DrawCross( suspensionFrame.contractChassisMountWorld, 0.10f, MakeVec4( 0.1f, 0.8f, 1.0f, 1.0f ) );
				DrawCross( suspensionFrame.contractTravelTopWorld, 0.08f, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
				DrawCross( suspensionFrame.contractTravelBottomWorld, 0.08f, MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
				DrawLine( suspensionFrame.contractTravelBottomWorld, suspensionFrame.contractTravelTopWorld,
						  MakeVec4( 0.9f, 0.2f, 1.0f, 1.0f ) );
				DrawLine( suspensionFrame.contractChassisMountWorld, liveChassisMount, MakeVec4( 0.1f, 0.8f, 1.0f, 0.85f ) );
				DrawLine( suspensionFrame.contractWheelCenterWorld, liveRestWheelCenter, MakeVec4( 1.0f, 1.0f, 1.0f, 0.85f ) );
				DrawString3D( { suspensionFrame.contractChassisMountWorld.x, suspensionFrame.contractChassisMountWorld.y + 0.15f,
								suspensionFrame.contractChassisMountWorld.z },
							  MakeVec4( 0.1f, 0.8f, 1.0f, 1.0f ), "M4 contract chassis visual socket" );
				DrawString3D( { suspensionFrame.contractWheelCenterWorld.x, suspensionFrame.contractWheelCenterWorld.y - 0.18f,
								suspensionFrame.contractWheelCenterWorld.z },
							  MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ), "M4 contract wheel center at rest" );
			}

			if ( m_showSuspensionMovingEndpoints )
			{
				b3Vec3 wheelTravelDelta = b3SubPos( cornerRig.liveWheelBody.p, cornerRig.liveRestWheelCenter );
				b3Pos damperUpperR = b3Pos_zero;
				b3Pos damperUpperL = b3Pos_zero;
				b3Pos damperLowerR = b3Pos_zero;
				b3Pos damperLowerL = b3Pos_zero;
				b3Pos cardanDrive = b3Pos_zero;
				b3Pos cardanHub = b3Pos_zero;
				if ( TryGetSuspensionContractWorldPoint( suspensionFrame.mountTransform, "suspension.visual.damper_upper_r",
														 &damperUpperR ) &&
					 TryGetSuspensionContractWorldPoint( suspensionFrame.mountTransform, "suspension.visual.damper_upper_l",
														 &damperUpperL ) &&
					 TryGetSuspensionContractWorldPoint( suspensionFrame.mountTransform, "suspension.visual.damper_lower_r",
														 &damperLowerR ) &&
					 TryGetSuspensionContractWorldPoint( suspensionFrame.mountTransform, "suspension.visual.damper_lower_l",
														 &damperLowerL ) &&
					 TryGetSuspensionContractWorldPoint( suspensionFrame.mountTransform, "suspension.visual.cardan_drive", &cardanDrive ) &&
					 TryGetSuspensionContractWorldPoint( suspensionFrame.mountTransform, "suspension.visual.cardan_hub", &cardanHub ) )
				{
					b3Pos liveDamperLowerR = b3OffsetPos( damperLowerR, wheelTravelDelta );
					b3Pos liveDamperLowerL = b3OffsetPos( damperLowerL, wheelTravelDelta );
					b3Pos liveCardanHub = b3OffsetPos( cardanHub, wheelTravelDelta );
					DrawLine( damperUpperR, liveDamperLowerR, MakeVec4( 0.1f, 1.0f, 0.2f, 1.0f ) );
					DrawLine( damperUpperL, liveDamperLowerL, MakeVec4( 0.1f, 1.0f, 0.2f, 1.0f ) );
					DrawLine( cardanDrive, liveCardanHub, MakeVec4( 1.0f, 0.65f, 0.1f, 1.0f ) );
					DrawCross( liveDamperLowerR, 0.07f, MakeVec4( 0.1f, 1.0f, 0.2f, 1.0f ) );
					DrawCross( liveDamperLowerL, 0.07f, MakeVec4( 0.1f, 1.0f, 0.2f, 1.0f ) );
					DrawCross( liveCardanHub, 0.07f, MakeVec4( 1.0f, 0.65f, 0.1f, 1.0f ) );
				}
			}
		}

		DrawTextLine( "Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 Foundation Debug Corner" );
		DrawTextLine( "W/S drive, Space brakes, Q/E live root down/up, R restarts." );
		DrawTextLine( "M3A asset defaults: scale %.2f m/BU, wheel r %.2f, width %.2f", m_assetMetersPerBlockbenchUnit,
					  m_assetWheelRadiusDefault, m_assetWheelWidthDefault );
		DrawTextLine( "M3B metadata: %s", m_assetMetadata.loadedFromRuntimeReport ? "runtime audit" : "built-in fallback" );
		DrawTextLine( "M3B preview: %s, wheel->body, suspension->chassis/root", m_showAssetSemanticPreview ? "on" : "off" );
		DrawTextLine( "M3B.2.1 static wheel mesh: %s", m_staticWheelVisual.IsLoaded() ? "loaded, not attached" : "not loaded" );
		DrawTextLine( "M3B.3 attached wheel mesh: %s",
					  ( m_showAttachedWheelVisual && m_staticWheelVisual.IsLoaded() ) ? "loaded, follows primitive wheel body" :
																					   "off or not loaded" );
		DrawTextLine( "primitive wheel debug shape: %s", m_showPrimitiveWheelDebugShape ? "shown" : "hidden, physics still active" );
		DrawTextLine( "M3B.2.1 texture: %s", m_staticWheelVisual.textureLoaded ? "loaded baseColor" : "solid fallback" );
		DrawTextLine( "M4 contract runtime: %s", m_suspensionContract.loaded ? "loaded sidecar + glTF" : "not loaded" );
		DrawTextLine( "M4A suspension mount visual: %s",
					  ( m_showSuspensionMountVisual && m_suspensionVisual.IsLoaded() && suspensionFrame.valid ) ?
						  "loaded, visual-only at rest wheel center" :
						  "off or not loaded" );
		DrawTextLine( "M4B moving endpoints: %s", m_showSuspensionMovingEndpoints ? "debug-only wheel-side endpoints follow body travel" :
																					 "off" );
		DrawTextLine( "root %.2f, wheel y %.2f, speed %.2f m/s, translation %.2f", m_liveRootOffset, (float)wheelPosition.y,
					  b3Length( wheelVelocity ), actualTranslation );
		DrawTextLine( "live mount %.2f, live rest %.2f, wheel bottom %.2f", GetLiveChassisMountY(), GetLiveRestWheelCenterY(),
					  (float)wheelPosition.y - m_wheelRadius );
		DrawTextLine( "travel limit %.2f..%.2f, spring %.2f Hz, damping %.2f", GetLowerSuspensionLimit(), GetUpperSuspensionLimit(),
					  m_suspensionHertz, m_suspensionDampingRatio );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehiclePrimitiveCornerM2( context );
	}

	b3BodyId m_chassisId;
	b3BodyId m_wheelId;
	b3ShapeId m_wheelShapeId;
	b3JointId m_jointId;

	JozzVehicleAuditMetadata m_assetMetadata;
	JozzVehicleAssetContract m_suspensionContract;
	JozzVehicleVisualMesh m_staticWheelVisual;
	JozzVehicleVisualAsset m_suspensionVisual;
	std::string m_staticWheelVisualSource;
	bool m_enableSuspension;
	bool m_enableSuspensionLimit;
	bool m_enableSpinMotor;
	bool m_collideConnected;
	bool m_editCollideConnected;
	bool m_showAxisDiagnostics;
	bool m_showAssetSemanticPreview;
	bool m_showStaticWheelVisualProof;
	bool m_showAttachedWheelVisual;
	bool m_showPrimitiveWheelDebugShape;
	bool m_showSuspensionMountVisual;
	bool m_showSuspensionContractPoints;
	bool m_showSuspensionMovingEndpoints;
	bool m_structuralSetupDirty;
	float m_suspensionHertz;
	float m_suspensionDampingRatio;
	float m_reboundTravel;
	float m_compressionTravel;
	float m_driveSpeed;
	float m_maxSpinTorque;
	float m_brakeTorque;
	float m_wheelRadius;
	float m_wheelWidth;
	float m_chassisHalfHeight;
	float m_rigHeight;
	float m_restDrop;
	float m_liveRootOffset;
	float m_liveRootSpeed;
	float m_editWheelRadius;
	float m_editWheelWidth;
	float m_editRigHeight;
	float m_editRestDrop;
	float m_assetMetersPerBlockbenchUnit;
	float m_assetWheelRadiusDefault;
	float m_assetWheelWidthDefault;
	float m_assetSuspensionTravelHint;
};

Sample* CreateJozzVehiclePrimitiveCornerM2( SampleContext* context )
{
	return JozzVehiclePrimitiveCornerM2::Create( context );
}
