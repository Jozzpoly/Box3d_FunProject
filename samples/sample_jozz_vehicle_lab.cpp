// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "sample.h"

#include "box3d/box3d.h"

class JozzVehicleLabM1 : public Sample
{
public:
	explicit JozzVehicleLabM1( SampleContext* context )
		: Sample( context )
	{
		if ( context->restart == false )
		{
			m_camera->SetView( 35.0f, 25.0f, 12.0f, { 0.0f, 1.5f, 0.0f } );
		}

		AddGroundBox( 20.0f );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = { 0.0f, 4.0f, 0.0f };
		bodyDef.name = "jozz_m1_smoke_box";
		m_smokeBoxId = b3CreateBody( m_worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = 1.0f;
		shapeDef.baseMaterial.friction = 0.7f;
		shapeDef.baseMaterial.restitution = 0.1f;

		b3BoxHull box = b3MakeBoxHull( 0.5f, 0.5f, 0.5f );
		b3CreateHullShape( m_smokeBoxId, &shapeDef, &box.base );
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "Jozz Vehicle Lab M1" );
		ImGui::Separator();
		ImGui::BulletText( "Native Box3D sample host is alive." );
		ImGui::BulletText( "Camera, ImGui, debug draw, and physics are wired." );
		ImGui::BulletText( "This smoke test intentionally uses primitives only." );
		ImGui::Spacing();
		ImGui::TextWrapped( "Next target: replace this box-only scene with a primitive one-corner wheel-joint lab before touching glTF rendering." );
		return true;
	}

	void Render() override
	{
		Sample::Render();

		DrawTextLine( "Jozz Vehicle Lab M1 Smoke" );
		DrawTextLine( "Expected: one dynamic cube falls onto the ground." );
		DrawTextLine( "No glTF rendering yet; assets are validated offline through tools/asset_audit.py." );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehicleLabM1( context );
	}

	b3BodyId m_smokeBoxId;
};

static int sampleJozzVehicleLabM1 = RegisterSample( "Jozz Vehicle", "Lab M1 Smoke", JozzVehicleLabM1::Create );

class JozzVehiclePrimitiveCornerM2 : public Sample
{
public:
	explicit JozzVehiclePrimitiveCornerM2( SampleContext* context )
		: Sample( context )
	{
		m_chassisId = b3_nullBodyId;
		m_wheelId = b3_nullBodyId;
		m_jointId = b3_nullJointId;

		if ( context->restart == false )
		{
			m_camera->SetView( 35.0f, 22.0f, 10.0f, { 0.0f, 1.1f, 0.0f } );
		}

		SetDefaults();
		AddGroundBox( 20.0f );
		CreateCorner();
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
		// Primitive dimensions are intentionally close to the current audited
		// Offroad_Big_Wheels asset at 0.35 m / Blockbench unit.
		// radius ~= 1.46875 * 0.35 = 0.514 m
		// width  ~= 1.25    * 0.35 = 0.438 m
		m_wheelRadius = 0.52f;
		m_wheelWidth = 0.44f;

		m_chassisHalfHeight = 0.16f;
		m_rigHeight = 1.70f;
		m_restDrop = 0.82f;
		m_reboundTravel = 0.42f;
		m_compressionTravel = 0.32f;
		m_liveRootOffset = 0.0f;
		m_liveRootSpeed = 2.5f;
		m_collideConnected = false;
		m_showAxisDiagnostics = true;
		m_structuralSetupDirty = false;

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

			// API order is height, radius, yOffset, sides. Height is the wheel width.
			b3HullData* wheelHull = b3CreateCylinder( m_wheelWidth, m_wheelRadius, 0.0f, 32 );
			b3CreateHullShape( m_wheelId, &shapeDef, wheelHull );
			b3DestroyHull( wheelHull );
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
		ImGui::TextUnformatted( "Jozz Vehicle Lab M2.5" );
		ImGui::Separator();
		ImGui::TextWrapped( "Primitive one-corner wheel-joint lab. M2.5 adds a realtime live root mover for suspension stress testing without rebuilding the joint." );
		ImGui::Spacing();
		ImGui::TextUnformatted( "Input: W drive forward, S reverse, Space brake, [ lower root, ] raise root, R restart sample." );
		ImGui::Text( "wheel radius %.2f m, width %.2f m", m_wheelRadius, m_wheelWidth );
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

		if ( m_structuralSetupDirty )
		{
			ImGui::TextWrapped( "Pending structural change. Press Apply to rebuild bodies/joint cleanly. Live root uses the committed setup until Apply." );
		}

		if ( ImGui::Button( "Apply rig rebuild" ) )
		{
			ApplyPendingStructuralSetup();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Reset M2.5 setup" ) )
		{
			SetDefaults();
			CreateCorner();
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
		if ( IsKeyDown( KEY_LEFT_BRACKET ) )
		{
			rootDirection -= 1.0f;
		}
		if ( IsKeyDown( KEY_RIGHT_BRACKET ) )
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
		Sample::Render();

		b3Pos wheelPosition = b3Body_GetPosition( m_wheelId );
		b3Vec3 wheelVelocity = b3Body_GetLinearVelocity( m_wheelId );
		b3Pos liveChassisMount = { 0.0f, GetLiveChassisMountY(), 0.0f };
		b3Pos liveRestWheelCenter = { 0.0f, GetLiveRestWheelCenterY(), 0.0f };
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

		DrawTextLine( "Jozz Vehicle Lab M2.5 Primitive Corner" );
		DrawTextLine( "W/S drive, Space brakes, [/] live root down/up, R restarts." );
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
	b3JointId m_jointId;

	bool m_enableSuspension;
	bool m_enableSuspensionLimit;
	bool m_enableSpinMotor;
	bool m_collideConnected;
	bool m_editCollideConnected;
	bool m_showAxisDiagnostics;
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
};

static int sampleJozzVehiclePrimitiveCornerM2 =
	RegisterSample( "Jozz Vehicle", "Lab M2 Primitive Corner", JozzVehiclePrimitiveCornerM2::Create );
