// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m8_rig_bench.h"

#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_visual_mesh.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// M8 Suspension Rig Bench: one corner, no driving. Jozz's One_Sided_wheel_mount
// model RIGGED per bone onto a REAL physics suspension so the motion is driven
// by forces, not a slider - a dynamic wheel body hanging from a static chassis
// anchor on a vertical spring/damper (prismatic). The spring target sweeps a
// sine so the wheel bounces continuously and the rig visibly works.
//
// Rig binding: the WheelCenter hub part follows the LIVE wheel body; the chassis
// brackets and the arm stay on the (static) chassis. The hub attaches at the
// wheel's Socket_WheelMount (inboard face), not the tyre centre. The arm+coilover
// is one rigid bone so it cannot telescope yet - that needs the damper bone split
// in Blockbench (Jozz will do it later).
class JozzVehicleM8RigBench : public Sample
{
public:
	explicit JozzVehicleM8RigBench( SampleContext* context )
		: Sample( context )
	{
		m_camera->m_thirdPerson = false;
		if ( context->restart == false )
		{
			m_camera->SetView( -118.0f, 14.0f, 3.9f, { 0.15f, 0.55f, -0.95f } );
		}

		// No continuous collision needed (the suspension bodies carry no shapes),
		// and it keeps the isolated bench off the vehicle-world CCD caveat.
		b3World_EnableContinuous( m_worldId, false );

		AddGroundBox( 20.0f );

		m_restHeight = 0.75f;
		m_trackHalf = 1.05f;
		m_leftSide = true;
		m_yawDeg = -90.0f;
		m_showModel = true;
		m_showWheel = true;
		m_showSockets = true;
		m_armFollowsWheel = false;

		m_animate = true;
		m_animAmplitude = 0.16f;
		m_animFrequency = 0.5f;
		m_manualTarget = 0.0f;
		m_time = 0.0f;

		m_suspensionHertz = 3.5f;
		m_suspensionDamping = 0.55f;
		m_travelLimit = 0.30f;
		m_wheelMass = 40.0f;

		m_assetMetadata = LoadJozzVehicleAuditMetadata();
		JozzVehiclePrimitiveDefaults defaults = GetJozzVehicleM3ADefaults( m_assetMetadata );
		m_metersPerBlockbenchUnit = defaults.metersPerBlockbenchUnit;
		m_wheelRadius = defaults.wheelRadius;

		std::string wheelPath;
		if ( FindJozzVehicleAssetFile( "assets/source/Offroad_Big_Wheels.gltf", &wheelPath ) )
		{
			m_wheelVisual.LoadStaticGltf( wheelPath.c_str(), m_metersPerBlockbenchUnit );
		}
		m_wheelCorrection = ComputeJozzVehicleWheelVisualCorrection( m_wheelVisual, m_assetMetadata, m_metersPerBlockbenchUnit );

		m_contract = LoadJozzVehicleAssetContract( "one_sided_wheel_mount.asset.json" );
		float mountMpbu = m_contract.metersPerBlockbenchUnit > 0.0f ? m_contract.metersPerBlockbenchUnit : m_metersPerBlockbenchUnit;
		std::string mountPath = m_contract.sourcePath;
		if ( mountPath.empty() )
		{
			FindJozzVehicleAssetFile( "assets/source/One_Sided_wheel_mount.gltf", &mountPath );
		}
		if ( mountPath.empty() == false )
		{
			m_riggedMount.LoadSkinnedGltf( mountPath.c_str(), mountMpbu );
		}

		std::string dumperPath;
		if ( FindJozzVehicleAssetFile( "assets/source/Asset_Dumper.gltf", &dumperPath ) )
		{
			m_dumper.LoadSkinnedGltf( dumperPath.c_str(), m_metersPerBlockbenchUnit );
		}

		m_wheelCenterAuthored = { -0.416f, 0.175f, 0.0f };
		const JozzVehicleContractBinding* wc =
			FindJozzVehicleContractBindingByRole( m_contract, "suspension.visual.wheel_center" );
		if ( wc != nullptr && wc->resolved )
		{
			m_wheelCenterAuthored = wc->positionMeters;
		}

		// Env overrides so a --screenshot run can pose an exact state with no UI.
		if ( const char* v = std::getenv( "JOZZ_BENCH_TRAVEL" ) )
		{
			m_animate = false;
			m_manualTarget = (float)atof( v );
		}
		if ( const char* v = std::getenv( "JOZZ_BENCH_WHEEL" ) )
		{
			m_showWheel = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_BENCH_ARMFOLLOW" ) )
		{
			m_armFollowsWheel = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_BENCH_MODEL" ) )
		{
			m_showModel = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_BENCH_DUMPER" ) )
		{
			m_showDumper = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_BENCH_CAM" ) )
		{
			float yaw = -118, pitch = 14, radius = 3.9f, px = 0.15f, py = 0.55f, pz = -0.95f;
			if ( std::sscanf( v, "%f,%f,%f,%f,%f,%f", &yaw, &pitch, &radius, &px, &py, &pz ) >= 3 )
			{
				m_camera->SetView( yaw, pitch, radius, { px, py, pz } );
			}
		}

		CreateSuspension();
	}

	~JozzVehicleM8RigBench() override
	{
		m_wheelVisual.Destroy();
		m_riggedMount.Destroy();
		m_dumper.Destroy();
	}

	b3Pos RestWheelCenterWorld() const
	{
		float z = ( m_leftSide ? -1.0f : 1.0f ) * m_trackHalf;
		return { 0.0f, m_restHeight, z };
	}

	// Static chassis anchor + dynamic wheel on a vertical spring/damper. The
	// wheel is shapeless (mass set explicitly): the bench shows suspension
	// travel, not contact, so no collider is needed and there is no CCD to trip.
	void CreateSuspension()
	{
		b3Pos rest = RestWheelCenterWorld();

		b3BodyDef chassisDef = b3DefaultBodyDef();
		chassisDef.type = b3_staticBody;
		chassisDef.position = rest;
		chassisDef.name = "m8_chassis_anchor";
		m_chassisBodyId = b3CreateBody( m_worldId, &chassisDef );

		b3BodyDef wheelDef = b3DefaultBodyDef();
		wheelDef.type = b3_dynamicBody;
		wheelDef.position = rest;
		wheelDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ ); // local Y = axle onto world Z
		wheelDef.name = "m8_wheel";
		m_wheelBodyId = b3CreateBody( m_worldId, &wheelDef );
		float inertia = 0.4f * m_wheelMass * m_wheelRadius * m_wheelRadius;
		b3MassData mass = {};
		mass.mass = m_wheelMass;
		mass.center = b3Vec3_zero;
		mass.inertia.cx = { inertia, 0.0f, 0.0f };
		mass.inertia.cy = { 0.0f, inertia, 0.0f };
		mass.inertia.cz = { 0.0f, 0.0f, inertia };
		b3Body_SetMassData( m_wheelBodyId, mass );

		b3PrismaticJointDef def = b3DefaultPrismaticJointDef();
		def.base.bodyIdA = m_chassisBodyId;
		def.base.bodyIdB = m_wheelBodyId;
		def.base.localFrameA.p = b3Vec3_zero;
		def.base.localFrameB.p = b3Vec3_zero;
		// Prismatic slides along frame-A local X; map it onto world Y (travel up).
		def.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
		def.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
		def.base.collideConnected = false;
		def.enableSpring = true;
		def.hertz = m_suspensionHertz;
		def.dampingRatio = m_suspensionDamping;
		def.targetTranslation = 0.0f;
		def.enableLimit = true;
		def.lowerTranslation = -m_travelLimit;
		def.upperTranslation = m_travelLimit;
		m_suspensionJointId = b3CreatePrismaticJoint( m_worldId, &def );
	}

	float LiveTravel() const
	{
		return (float)( b3Body_GetPosition( m_wheelBodyId ).y - RestWheelCenterWorld().y );
	}

	b3WorldTransform WheelDrawTransform() const
	{
		return b3MulWorldTransforms( b3Body_GetTransform( m_wheelBodyId ), m_wheelCorrection );
	}

	// The wheel's Socket_WheelMount in world (the hub face the model attaches to),
	// computed from the LIVE wheel body so the hub tracks real suspension travel.
	b3Pos WheelMountAttachLive() const
	{
		b3Vec3 mountBU = JozzVehicleFindPointOrBuiltIn( m_assetMetadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		return b3TransformWorldPoint( WheelDrawTransform(), b3MulSV( m_metersPerBlockbenchUnit, mountBU ) );
	}

	b3Pos WheelMountAttachRest() const
	{
		b3Vec3 mountBU = JozzVehicleFindPointOrBuiltIn( m_assetMetadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		b3WorldTransform restWheel;
		restWheel.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
		restWheel.p = RestWheelCenterWorld();
		return b3TransformWorldPoint( b3MulWorldTransforms( restWheel, m_wheelCorrection ),
									  b3MulSV( m_metersPerBlockbenchUnit, mountBU ) );
	}

	// Chassis-anchored placement (brackets/arm): model rotated onto the car
	// lateral axis, WheelCenter socket on the REST attach point.
	b3WorldTransform ModelPlacementRest() const
	{
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, m_yawDeg * B3_PI / 180.0f );
		b3Vec3 rp = b3RotateVector( yaw, m_wheelCenterAuthored );
		b3Pos target = WheelMountAttachRest();
		b3WorldTransform t;
		t.q = yaw;
		t.p = { target.x - rp.x, target.y - rp.y, target.z - rp.z };
		return t;
	}

	// Hub placement: same orientation, WheelCenter socket on the LIVE attach.
	b3WorldTransform ModelPlacementLive() const
	{
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, m_yawDeg * B3_PI / 180.0f );
		b3Vec3 rp = b3RotateVector( yaw, m_wheelCenterAuthored );
		b3Pos target = WheelMountAttachLive();
		b3WorldTransform t;
		t.q = yaw;
		t.p = { target.x - rp.x, target.y - rp.y, target.z - rp.z };
		return t;
	}

	bool PartFollowsWheel( const std::string& boneName ) const
	{
		if ( boneName.find( "WheelCenter" ) != std::string::npos )
		{
			return true;
		}
		if ( m_armFollowsWheel && boneName.find( "ChassisMount" ) != std::string::npos )
		{
			return true;
		}
		return false;
	}

	bool SocketRoleFollowsWheel( const std::string& role ) const
	{
		return role.find( "wheel_center" ) != std::string::npos || role.find( "cardan_hub" ) != std::string::npos ||
			   role.find( "damper_lower" ) != std::string::npos || role.find( "travel_axis.bottom" ) != std::string::npos;
	}

	void Step() override
	{
		float dt = m_context->hertz > 0.0f ? 1.0f / m_context->hertz : 1.0f / 60.0f;
		m_time += dt;

		float target = m_animate ? m_animAmplitude * std::sin( 2.0f * B3_PI * m_animFrequency * m_time ) : m_manualTarget;
		if ( B3_IS_NON_NULL( m_suspensionJointId ) )
		{
			b3PrismaticJoint_SetTargetTranslation( m_suspensionJointId, target );
			b3Joint_WakeBodies( m_suspensionJointId );
		}

		Sample::Step();
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "M8 Suspension Rig Bench - one corner, real spring, no driving" );
		ImGui::TextWrapped( "A dynamic wheel hangs from a static chassis on a vertical spring. The hub part follows the "
							"live wheel; the brackets stay. The spring target sweeps a sine so it bounces on its own." );
		ImGui::Separator();

		ImGui::Checkbox( "Animate (auto bounce)", &m_animate );
		ImGui::SliderFloat( "Bounce amplitude", &m_animAmplitude, 0.0f, 0.28f, "%.2f m" );
		ImGui::SliderFloat( "Bounce frequency", &m_animFrequency, 0.1f, 2.0f, "%.2f Hz" );
		if ( m_animate == false )
		{
			ImGui::SliderFloat( "Manual target", &m_manualTarget, -m_travelLimit, m_travelLimit, "%.3f m" );
		}
		ImGui::Text( "live travel %.3f m", LiveTravel() );
		ImGui::Separator();

		bool springEdited = false;
		springEdited |= ImGui::SliderFloat( "Spring hertz", &m_suspensionHertz, 0.5f, 10.0f, "%.2f" );
		springEdited |= ImGui::SliderFloat( "Spring damping", &m_suspensionDamping, 0.0f, 2.0f, "%.2f" );
		if ( springEdited && B3_IS_NON_NULL( m_suspensionJointId ) )
		{
			b3PrismaticJoint_SetSpringHertz( m_suspensionJointId, m_suspensionHertz );
			b3PrismaticJoint_SetSpringDampingRatio( m_suspensionJointId, m_suspensionDamping );
		}
		ImGui::Separator();

		ImGui::Checkbox( "Arm (ChassisMount) follows wheel", &m_armFollowsWheel );
		ImGui::TextWrapped( "Leave OFF (arm stays with the chassis). True telescoping needs the damper bone split." );
		ImGui::SliderFloat( "Model yaw (tuning)", &m_yawDeg, -180.0f, 180.0f, "%.1f deg" );
		ImGui::Separator();

		ImGui::Checkbox( "Show mount model", &m_showModel );
		ImGui::Checkbox( "Show wheel", &m_showWheel );
		ImGui::Checkbox( "Show socket overlay", &m_showSockets );
		ImGui::Separator();

		ImGui::TextWrapped( "wheel: %s", m_wheelVisual.status.c_str() );
		ImGui::TextWrapped( "mount: %s", m_riggedMount.status.c_str() );
		for ( const JozzVehicleRiggedPart& part : m_riggedMount.parts )
		{
			ImGui::BulletText( "%s: %d tris%s", part.boneName.c_str(), part.triangleCount,
							   PartFollowsWheel( part.boneName ) ? " (follows wheel)" : " (chassis)" );
		}
		return true;
	}

	void Render() override
	{
		Sample::Render();

		b3WorldTransform restPlacement = ModelPlacementRest();
		b3WorldTransform livePlacement = ModelPlacementLive();

		if ( m_showWheel && m_wheelVisual.IsLoaded() )
		{
			m_wheelVisual.DrawAtTransform( WheelDrawTransform(), MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
		}

		if ( m_showModel && m_riggedMount.IsLoaded() )
		{
			for ( int i = 0; i < m_riggedMount.PartCount(); ++i )
			{
				bool follows = PartFollowsWheel( m_riggedMount.parts[i].boneName );
				m_riggedMount.DrawPart( i, follows ? livePlacement : restPlacement, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
			}
		}

		// Telescoping damper: top pinned above the corner (chassis side), bottom
		// on the live wheel, Part_Stretch scaling to bridge as it bounces.
		if ( m_showDumper && m_dumper.IsLoaded() )
		{
			b3Pos rest = RestWheelCenterWorld();
			b3Pos topWorld = { rest.x, rest.y + m_dumperTopHeight, rest.z };
			b3Pos botWorld = b3Body_GetPosition( m_wheelBodyId );
			m_dumper.DrawTelescopingDamper( topWorld, botWorld, MakeVec4( 0.8f, 0.82f, 0.88f, 1.0f ) );
		}

		if ( m_showSockets )
		{
			b3Pos centre = b3Body_GetPosition( m_wheelBodyId );
			b3Pos attach = WheelMountAttachLive();
			DrawCross( centre, 0.10f, MakeVec4( 0.7f, 0.7f, 0.7f, 1.0f ) );
			DrawCross( attach, 0.12f, MakeVec4( 1.0f, 0.85f, 0.1f, 1.0f ) );

			for ( const JozzVehicleContractBinding& binding : m_contract.bindings )
			{
				if ( binding.resolved == false )
				{
					continue;
				}
				bool isWheelCenter = binding.role.find( "wheel_center" ) != std::string::npos;
				bool follows = SocketRoleFollowsWheel( binding.role );
				b3WorldTransform t = follows ? livePlacement : restPlacement;
				b3Pos world = b3TransformWorldPoint( t, binding.positionMeters );
				Vec4 color = isWheelCenter	 ? MakeVec4( 0.2f, 1.0f, 0.4f, 1.0f )
							 : follows		 ? MakeVec4( 0.2f, 0.9f, 1.0f, 1.0f )
											   : MakeVec4( 0.95f, 0.55f, 0.15f, 1.0f );
				DrawCross( world, isWheelCenter ? 0.09f : 0.05f, color );
			}
		}

		DrawTextLine( "M8 Suspension Rig Bench - rigged per bone on a real spring" );
		DrawTextLine( "%s corner, live travel %.3f m, %s", m_leftSide ? "LEFT" : "RIGHT", LiveTravel(),
					  m_animate ? "auto-bounce" : "manual" );
		DrawTextLine( "green = WheelCenter (hub); cyan = wheel-side sockets; orange = chassis-side sockets" );
		DrawTextLine( "the hub rides the live wheel; the brackets stay - the suspension working under forces" );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehicleM8RigBench( context );
	}

	b3BodyId m_chassisBodyId;
	b3BodyId m_wheelBodyId;
	b3JointId m_suspensionJointId;

	JozzVehicleAuditMetadata m_assetMetadata;
	JozzVehicleVisualMesh m_wheelVisual;
	b3Transform m_wheelCorrection;
	JozzVehicleAssetContract m_contract;
	JozzVehicleRiggedMesh m_riggedMount;
	JozzVehicleRiggedMesh m_dumper;
	bool m_showDumper = true;
	float m_dumperTopHeight = 0.85f;
	b3Vec3 m_wheelCenterAuthored;

	float m_metersPerBlockbenchUnit;
	float m_wheelRadius;
	float m_restHeight;
	float m_trackHalf;
	bool m_leftSide;
	float m_yawDeg;

	bool m_animate;
	float m_animAmplitude;
	float m_animFrequency;
	float m_manualTarget;
	float m_time;

	float m_suspensionHertz;
	float m_suspensionDamping;
	float m_travelLimit;
	float m_wheelMass;

	bool m_armFollowsWheel;
	bool m_showModel;
	bool m_showWheel;
	bool m_showSockets;
};

Sample* CreateJozzVehicleM8RigBench( SampleContext* context )
{
	return JozzVehicleM8RigBench::Create( context );
}
