// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

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
		if ( context->restart == false )
		{
			m_camera->SetView( 35.0f, 22.0f, 10.0f, { 0.0f, 1.1f, 0.0f } );
		}

		// M2.1: primitive dimensions are now intentionally close to the current
		// audited Offroad_Big_Wheels asset with 0.35 m / Blockbench unit.
		// radius ~= 1.46875 * 0.35 = 0.514 m
		// width  ~= 1.25    * 0.35 = 0.438 m
		m_wheelRadius = 0.52f;
		m_wheelWidth = 0.44f;

		m_enableSuspension = true;
		m_enableSuspensionLimit = true;
		m_enableSpinMotor = true;
		m_suspensionHertz = 3.0f;
		m_suspensionDampingRatio = 0.75f;
		m_lowerTranslation = -0.65f;
		m_upperTranslation = 0.35f;
		m_driveSpeed = 14.0f;
		m_maxSpinTorque = 90.0f;
		m_brakeTorque = 180.0f;

		AddGroundBox( 20.0f );

		// Static chassis rig: this isolates the wheel joint before a full dynamic
		// vehicle body adds mass distribution and roll/pitch problems.
		{
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.position = { 0.0f, 1.70f, 0.0f };
			bodyDef.name = "jozz_m2_static_chassis_rig";
			m_chassisId = b3CreateBody( m_worldId, &bodyDef );

			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.baseMaterial.friction = 0.8f;

			b3BoxHull chassis = b3MakeBoxHull( 1.25f, 0.16f, 0.55f );
			b3CreateHullShape( m_chassisId, &shapeDef, &chassis.base );
		}

		{
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.type = b3_dynamicBody;
			bodyDef.position = { 0.0f, 0.72f, 0.0f };

			// b3CreateCylinder builds along local Y. Rotate local Y onto world Z,
			// matching the stock Box3D wheel-joint sample and giving the wheel a
			// visible axle across Z while it rolls along X.
			bodyDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
			bodyDef.allowFastRotation = true;
			bodyDef.name = "jozz_m2_asset_scaled_primitive_wheel";
			m_wheelId = b3CreateBody( m_worldId, &bodyDef );

			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.density = 20.0f;
			shapeDef.baseMaterial.friction = 1.25f;
			shapeDef.baseMaterial.restitution = 0.02f;
			shapeDef.baseMaterial.rollingResistance = 0.02f;

			// API order is height, radius, yOffset, sides. Height is the wheel width.
			// The old M2 had radius/width effectively swapped, which made the wheel
			// look and spin like a bad offset roller.
			b3HullData* wheelHull = b3CreateCylinder( m_wheelWidth, m_wheelRadius, 0.0f, 24 );
			b3CreateHullShape( m_wheelId, &shapeDef, wheelHull );
			b3DestroyHull( wheelHull );
		}

		b3WheelJointDef jointDef = b3DefaultWheelJointDef();
		jointDef.base.bodyIdA = m_chassisId;
		jointDef.base.bodyIdB = m_wheelId;

		b3Pos anchor = { 0.0f, 1.32f, 0.0f };
		jointDef.base.localFrameA.p = b3Body_GetLocalPoint( m_chassisId, anchor );
		jointDef.base.localFrameB.p = b3Body_GetLocalPoint( m_wheelId, anchor );

		// Matches the known Box3D wheel sample frame pattern. M2 keeps this explicit
		// so later asset-space conversion has a stable primitive reference.
		jointDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
		jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		jointDef.base.collideConnected = false;
		jointDef.base.drawScale = 1.5f;

		jointDef.enableSuspensionSpring = m_enableSuspension;
		jointDef.suspensionHertz = m_suspensionHertz;
		jointDef.suspensionDampingRatio = m_suspensionDampingRatio;
		jointDef.enableSuspensionLimit = m_enableSuspensionLimit;
		jointDef.lowerSuspensionLimit = m_lowerTranslation;
		jointDef.upperSuspensionLimit = m_upperTranslation;
		jointDef.enableSpinMotor = m_enableSpinMotor;
		jointDef.spinSpeed = 0.0f;
		jointDef.maxSpinTorque = 0.0f;

		m_jointId = b3CreateWheelJoint( m_worldId, &jointDef );
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "Jozz Vehicle Lab M2.1" );
		ImGui::Separator();
		ImGui::TextWrapped( "Primitive one-corner wheel-joint lab. M2.1 fixes cylinder radius/width order and uses asset-like primitive dimensions." );
		ImGui::Spacing();
		ImGui::TextUnformatted( "Input: W drive forward, S reverse, Space brake, R restart sample." );
		ImGui::Text( "Primitive wheel radius %.2f m, width %.2f m", m_wheelRadius, m_wheelWidth );
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
			if ( ImGui::SliderFloat( "Lower travel", &m_lowerTranslation, -2.0f, 0.0f, "%.2f" ) )
			{
				if ( m_lowerTranslation > m_upperTranslation )
				{
					m_lowerTranslation = m_upperTranslation;
				}
				b3WheelJoint_SetSuspensionLimits( m_jointId, m_lowerTranslation, m_upperTranslation );
				b3Joint_WakeBodies( m_jointId );
			}

			if ( ImGui::SliderFloat( "Upper travel", &m_upperTranslation, 0.0f, 2.0f, "%.2f" ) )
			{
				if ( m_upperTranslation < m_lowerTranslation )
				{
					m_upperTranslation = m_lowerTranslation;
				}
				b3WheelJoint_SetSuspensionLimits( m_jointId, m_lowerTranslation, m_upperTranslation );
				b3Joint_WakeBodies( m_jointId );
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

		DrawTextLine( "Jozz Vehicle Lab M2.1 Primitive Corner" );
		DrawTextLine( "W/S drive only while held, Space brakes, R restarts." );
		DrawTextLine( "wheel y = %.2f, speed = %.2f m/s", (float)wheelPosition.y, b3Length( wheelVelocity ) );
		DrawTextLine( "wheel radius %.2f m, width %.2f m", m_wheelRadius, m_wheelWidth );
		DrawTextLine( "spring %.2f Hz, damping %.2f, travel %.2f..%.2f", m_suspensionHertz, m_suspensionDampingRatio,
					  m_lowerTranslation, m_upperTranslation );
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
	float m_suspensionHertz;
	float m_suspensionDampingRatio;
	float m_lowerTranslation;
	float m_upperTranslation;
	float m_driveSpeed;
	float m_maxSpinTorque;
	float m_brakeTorque;
	float m_wheelRadius;
	float m_wheelWidth;
};

static int sampleJozzVehiclePrimitiveCornerM2 =
	RegisterSample( "Jozz Vehicle", "Lab M2 Primitive Corner", JozzVehiclePrimitiveCornerM2::Create );
