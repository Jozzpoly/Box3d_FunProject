// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m9_steering_rig_bench.h"

#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_steering_suspension_contract.h"
#include "jozz_vehicle_visual_mesh.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// M9 Steering Rig Bench: two corners (left + right) of Jozz's NEW
// OneSided_Steering_Suspension_Rig model, each on a real 2-DOF suspension -
// vertical travel (prismatic spring, chassis->carrier) and steer yaw
// (revolute spring, carrier->knuckle). No driving physics; the point is to
// see the RIG (which part rides which body) work under real motion before
// this model goes anywhere near the M6 vehicle.
//
// This model's node layout is NOT a copy of One_Sided_wheel_mount (M8's
// model) - see assets/contracts/one_sided_steering_suspension.asset.json's
// "notes" block. Two bindings are the ones most likely to be gotten wrong by
// habit from the old model:
//   - Socket_ChassisMount_b is the upright/knuckle (rides the KNUCKLE body),
//     despite the name suggesting a chassis bracket.
//   - Socket_SingleDamperLower is a child of the LOWER WISHBONE ARM in the
//     source glTF, not of the knuckle - its live position is derived from the
//     arm's own live placement (JozzVehicleComputeArmPlacement /
//     JozzVehicleMapAuthoredPoint), the same "pin two ends, stretch along the
//     axis" math that draws the arm mesh itself, because this bench has no
//     dedicated physics body for the arm (a simplification acceptable for a
//     visual-rig bench; the real M6 corner does have a lowerArmId body and
//     should pin the damper socket to it directly instead).
//   - Socket_SteeringRod's inboard (rack-side) end has no real rack in this
//     isolated bench. It is pinned to a FIXED point (the rod's own rest
//     placement) rather than inventing rack kinematics: the rod visually
//     stretches/compresses through steering sweeps here, the same way the
//     wishbone arms already tolerate a live span different from their
//     authored length. The real vehicle integration must instead read the
//     actual rack body / steerLinkJoint - never a new mechanism.
class JozzVehicleM9SteeringRigBench : public Sample
{
public:
	struct Corner
	{
		bool mirror = false; // true = right corner (mirrored mesh + negated authored X)
		b3Pos restWorld = b3Vec3_zero;

		b3BodyId chassisBodyId;
		b3BodyId carrierBodyId;
		b3BodyId knuckleBodyId;
		b3JointId travelJointId;
		b3JointId steerJointId;

		b3WorldTransform bracketWorld; // fixed chassis-side placement (chassis never moves in this bench)
		b3Transform hubLocal;			// baked knuckle-body-local placement offset
		b3Pos rackRestWorld;			// fixed rack-side reference for the steering rod's inboard end
	};

	explicit JozzVehicleM9SteeringRigBench( SampleContext* context )
		: Sample( context )
	{
		m_camera->m_thirdPerson = false;
		if ( context->restart == false )
		{
			m_camera->SetView( -140.0f, 12.0f, 5.2f, { 0.0f, 0.55f, 0.0f } );
		}

		// Suspension bodies carry no shapes; keep this bench off the vehicle
		// world's CCD caveat (see README_FOR_AGENTS.md rule).
		b3World_EnableContinuous( m_worldId, false );

		AddGroundBox( 20.0f );

		m_restHeight = 0.75f;
		m_trackHalf = 1.05f;
		m_showModel = true;
		m_showWheel = true;
		m_showSockets = true;
		m_showDumper = true;

		m_animateTravel = true;
		m_travelAmplitude = 0.16f;
		m_travelFrequency = 0.5f;
		m_manualTravel = 0.0f;
		m_steerAngleDeg = 0.0f;
		m_time = 0.0f;

		m_suspensionHertz = 3.5f;
		m_suspensionDamping = 0.55f;
		m_travelLimit = 0.30f;
		m_steerHertz = 4.0f;
		m_steerDamping = 0.7f;
		m_carrierMass = 15.0f;
		m_knuckleMass = 40.0f;

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

		m_contract = LoadJozzVehicleAssetContract( "one_sided_steering_suspension.asset.json" );
		float rigMpbu = m_contract.metersPerBlockbenchUnit > 0.0f ? m_contract.metersPerBlockbenchUnit : m_metersPerBlockbenchUnit;
		std::string rigPath = m_contract.sourcePath;
		if ( rigPath.empty() )
		{
			FindJozzVehicleAssetFile( "assets/source/OneSided_Steering_Suspension_Rig.gltf", &rigPath );
		}
		if ( rigPath.empty() == false )
		{
			m_riggedRigL.LoadSkinnedGltf( rigPath.c_str(), rigMpbu, false );
			m_riggedRigR.LoadSkinnedGltf( rigPath.c_str(), rigMpbu, true );
		}

		std::string dumperPath;
		if ( FindJozzVehicleAssetFile( "assets/source/Asset_Dumper.gltf", &dumperPath ) )
		{
			m_dumper.LoadSkinnedGltf( dumperPath.c_str(), m_metersPerBlockbenchUnit );
		}

		// Fallback authored sockets (meters) match the contract's authored glTF
		// exactly - used only if the contract fails to load/resolve.
		m_sockets.wheelCenter = { -0.4156f, 0.175f, 0.0f };
		m_sockets.chassisMountA = { 0.00546f, 0.56875f, -0.15859f };
		m_sockets.chassisMountB = { -0.27342f, 0.33908f, 0.0f };
		m_sockets.damperMount = { 0.13125f, 0.50313f, 0.0f };
		m_sockets.damperUpper = { 0.01642f, 0.64533f, -0.28438f };
		m_sockets.damperLower = { -0.25158f, 0.01092f, -0.28438f };
		m_sockets.steeringRod = { -0.18046f, 0.25158f, 0.26796f };
		m_sockets.travelAxisTop = { -0.4156f, 0.525f, 0.0f };
		m_sockets.travelAxisBottom = { -0.4156f, -0.175f, 0.0f };
		JozzVehicleSteeringSuspensionSockets resolved = ResolveJozzVehicleSteeringSuspensionSockets( m_contract );
		if ( resolved.resolved )
		{
			m_sockets = resolved;
		}

		// Env overrides so a --screenshot run can pose an exact state with no UI.
		if ( const char* v = std::getenv( "JOZZ_M9_TRAVEL" ) )
		{
			m_animateTravel = false;
			m_manualTravel = (float)atof( v );
		}
		if ( const char* v = std::getenv( "JOZZ_M9_STEER" ) )
		{
			m_steerAngleDeg = (float)atof( v );
		}
		if ( const char* v = std::getenv( "JOZZ_M9_WHEEL" ) )
		{
			m_showWheel = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M9_MODEL" ) )
		{
			m_showModel = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M9_DUMPER" ) )
		{
			m_showDumper = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M9_SOCKETS" ) )
		{
			m_showSockets = atoi( v ) != 0;
		}
		if ( const char* v = std::getenv( "JOZZ_M9_CAM" ) )
		{
			float yaw = -140, pitch = 12, radius = 5.2f, px = 0.0f, py = 0.55f, pz = 0.0f;
			if ( std::sscanf( v, "%f,%f,%f,%f,%f,%f", &yaw, &pitch, &radius, &px, &py, &pz ) >= 3 )
			{
				m_camera->SetView( yaw, pitch, radius, { px, py, pz } );
			}
		}

		CreateCorner( false );
		CreateCorner( true );

		if ( std::getenv( "JOZZ_M9_DUMP" ) )
		{
			m_dumpOnce = true;
		}
	}

	~JozzVehicleM9SteeringRigBench() override
	{
		m_wheelVisual.Destroy();
		m_riggedRigL.Destroy();
		m_riggedRigR.Destroy();
		m_dumper.Destroy();
	}

	static b3Vec3 MirrorX( b3Vec3 v, bool mirror )
	{
		if ( mirror )
		{
			v.x = -v.x;
		}
		return v;
	}

	// The wheel tire mesh's authored spin axis is +X, and the primitive wheel
	// body convention used everywhere else in this codebase (M6, M8) puts the
	// axle on body-local +Y. M8's bench bakes local-Y -> world-Z directly into
	// its wheel body's spawn rotation, which works there because that body has
	// no other rotational DOF (only a prismatic, which fixes all rotation).
	// The M9 knuckle body DOES have a real rotational DOF (the steer revolute)
	// - baking this fix into the body's rest rotation would fight the joint
	// solver, which forces the knuckle toward whatever rotation the revolute's
	// localFrameA/B + targetAngle demand (pure yaw about world Y at rest),
	// snapping any other baked-in rotation away. So this stays a render-only
	// correction, composed onto the live knuckle transform at draw time
	// instead of being part of the physics body.
	static b3Quat WheelAxleFix()
	{
		return b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
	}

	// Chassis-end and rack/wheel-extreme end of a part along its authored X
	// axis, at the vertical/lateral midpoint of its bbox. Mirrors the M6 rig
	// lab's ArmEnds helper (same math: rigid-skin parts in this family of
	// models extend along authored X). `negXIsOutboard` selects which extreme
	// is the "outboard" (wheel/knuckle-side) one - true for the left
	// (unmirrored) mesh, false for the mirrored copy, exactly like
	// JozzVehicleRiggedMesh's own mirrorX convention flips restMin/restMax.
	static void PartXEnds( const JozzVehicleRiggedPart& part, bool negXIsOutboard, b3Vec3& inboardEnd, b3Vec3& outboardEnd )
	{
		float y = 0.5f * ( part.restMin.y + part.restMax.y );
		float z = 0.5f * ( part.restMin.z + part.restMax.z );
		inboardEnd = { negXIsOutboard ? part.restMax.x : part.restMin.x, y, z };
		outboardEnd = { negXIsOutboard ? part.restMin.x : part.restMax.x, y, z };
	}

	void CreateCorner( bool mirror )
	{
		Corner& c = mirror ? m_right : m_left;
		c.mirror = mirror;
		c.restWorld = { 0.0f, m_restHeight, mirror ? m_trackHalf : -m_trackHalf };

		b3BodyDef chassisDef = b3DefaultBodyDef();
		chassisDef.type = b3_staticBody;
		chassisDef.position = c.restWorld;
		chassisDef.name = mirror ? "m9_chassis_R" : "m9_chassis_L";
		c.chassisBodyId = b3CreateBody( m_worldId, &chassisDef );

		b3BodyDef carrierDef = b3DefaultBodyDef();
		carrierDef.type = b3_dynamicBody;
		carrierDef.position = c.restWorld;
		carrierDef.name = mirror ? "m9_carrier_R" : "m9_carrier_L";
		c.carrierBodyId = b3CreateBody( m_worldId, &carrierDef );
		SetShapelessMass( c.carrierBodyId, m_carrierMass, 0.06f );

		b3BodyDef knuckleDef = b3DefaultBodyDef();
		knuckleDef.type = b3_dynamicBody;
		knuckleDef.position = c.restWorld;
		knuckleDef.name = mirror ? "m9_knuckle_R" : "m9_knuckle_L";
		c.knuckleBodyId = b3CreateBody( m_worldId, &knuckleDef );
		SetShapelessMass( c.knuckleBodyId, m_knuckleMass, m_wheelRadius );

		// Vertical travel: chassis -> carrier, prismatic local X mapped onto
		// world Y (same trick as the M8 bench).
		b3PrismaticJointDef travelDef = b3DefaultPrismaticJointDef();
		travelDef.base.bodyIdA = c.chassisBodyId;
		travelDef.base.bodyIdB = c.carrierBodyId;
		travelDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
		travelDef.base.localFrameB.q = travelDef.base.localFrameA.q;
		travelDef.base.collideConnected = false;
		travelDef.enableSpring = true;
		travelDef.hertz = m_suspensionHertz;
		travelDef.dampingRatio = m_suspensionDamping;
		travelDef.targetTranslation = 0.0f;
		travelDef.enableLimit = true;
		travelDef.lowerTranslation = -m_travelLimit;
		travelDef.upperTranslation = m_travelLimit;
		c.travelJointId = b3CreatePrismaticJoint( m_worldId, &travelDef );

		// Steer yaw: carrier -> knuckle, revolute local Z (its natural hinge
		// axis) mapped onto world Y (kingpin/steer axis, simplified vertical -
		// this bench is not modeling caster/kingpin lean).
		b3RevoluteJointDef steerDef = b3DefaultRevoluteJointDef();
		steerDef.base.bodyIdA = c.carrierBodyId;
		steerDef.base.bodyIdB = c.knuckleBodyId;
		steerDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		steerDef.base.localFrameB.q = steerDef.base.localFrameA.q;
		steerDef.base.collideConnected = false;
		steerDef.enableSpring = true;
		steerDef.hertz = m_steerHertz;
		steerDef.dampingRatio = m_steerDamping;
		steerDef.targetAngle = 0.0f;
		c.steerJointId = b3CreateRevoluteJoint( m_worldId, &steerDef );

		// Rest placement: model yawed -90 (matches the established
		// One_Sided_wheel_mount / M6 convention), with the (mirrored-as-needed)
		// wheel_center socket landing on the corner's rest world point.
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
		b3Vec3 wc = MirrorX( m_sockets.wheelCenter, mirror );
		b3Vec3 rp = b3RotateVector( yaw, wc );
		b3WorldTransform placementRest;
		placementRest.q = yaw;
		placementRest.p = { c.restWorld.x - rp.x, c.restWorld.y - rp.y, c.restWorld.z - rp.z };

		c.bracketWorld = placementRest; // chassis never moves in this bench
		c.hubLocal = b3InvMulWorldTransforms( b3Body_GetTransform( c.knuckleBodyId ), placementRest );

		// Fixed rack-side reference for the steering rod's inboard end: the
		// rod's own rest placement, so it starts with zero stretch (see the
		// class comment for why this is a bench-only simplification).
		c.rackRestWorld = ResolveRodInboardRest( c, mirror );
	}

	// Finds the SteeringRod part's own inboard authored point (rack side) and
	// transforms it through the rest placement, so the rack reference starts
	// exactly where the model was authored (zero stretch at rest).
	b3Pos ResolveRodInboardRest( const Corner& c, bool mirror ) const
	{
		const JozzVehicleRiggedMesh& mesh = mirror ? m_riggedRigR : m_riggedRigL;
		int rodIndex = FindPartByBoneNodeIndex( mesh, 7 );
		if ( rodIndex < 0 )
		{
			return c.restWorld;
		}
		b3Vec3 inboardAuthored, outboardAuthored;
		PartXEnds( mesh.parts[rodIndex], mirror == false, inboardAuthored, outboardAuthored );
		return b3TransformPoint( c.bracketWorld, inboardAuthored );
	}

	static int FindPartByBoneNodeIndex( const JozzVehicleRiggedMesh& mesh, int nodeIndex )
	{
		for ( int i = 0; i < mesh.PartCount(); ++i )
		{
			if ( mesh.parts[i].boneNodeIndex == nodeIndex )
			{
				return i;
			}
		}
		return -1;
	}

	static void SetShapelessMass( b3BodyId bodyId, float mass, float radius )
	{
		float inertia = 0.4f * mass * radius * radius;
		b3MassData massData = {};
		massData.mass = mass;
		massData.center = b3Vec3_zero;
		massData.inertia.cx = { inertia, 0.0f, 0.0f };
		massData.inertia.cy = { 0.0f, inertia, 0.0f };
		massData.inertia.cz = { 0.0f, 0.0f, inertia };
		b3Body_SetMassData( bodyId, massData );
	}

	void Step() override
	{
		float dt = m_context->hertz > 0.0f ? 1.0f / m_context->hertz : 1.0f / 60.0f;
		m_time += dt;

		float travel = m_animateTravel ? m_travelAmplitude * std::sin( 2.0f * B3_PI * m_travelFrequency * m_time ) : m_manualTravel;
		float steerRad = m_steerAngleDeg * B3_PI / 180.0f;

		for ( Corner* c : { &m_left, &m_right } )
		{
			if ( B3_IS_NON_NULL( c->travelJointId ) )
			{
				b3PrismaticJoint_SetTargetTranslation( c->travelJointId, travel );
				b3Joint_WakeBodies( c->travelJointId );
			}
			if ( B3_IS_NON_NULL( c->steerJointId ) )
			{
				b3RevoluteJoint_SetTargetAngle( c->steerJointId, steerRad );
				b3Joint_WakeBodies( c->steerJointId );
			}
		}

		Sample::Step();

		if ( m_dumpOnce && m_stepCount == 60 )
		{
			DumpCornerNumbers( m_left );
			DumpCornerNumbers( m_right );
			m_dumpOnce = false;
		}
	}

	void DumpCornerNumbers( const Corner& c ) const
	{
		b3WorldTransform hubWorld = ComputeHubWorld( c );
		const JozzVehicleRiggedMesh& mesh = c.mirror ? m_riggedRigR : m_riggedRigL;
		b3Vec3 wc = MirrorX( m_sockets.wheelCenter, c.mirror );
		b3Vec3 cmb = MirrorX( m_sockets.chassisMountB, c.mirror );
		b3Pos wheelCenterLive = b3TransformPoint( hubWorld, wc );
		b3Pos chassisMountBLive = b3TransformPoint( hubWorld, cmb );
		std::printf( "m9 corner mirror=%d wheelCenterLive=(%.4f,%.4f,%.4f) chassisMountBLive=(%.4f,%.4f,%.4f) span=%.4f\n",
					 c.mirror ? 1 : 0, wheelCenterLive.x, wheelCenterLive.y, wheelCenterLive.z, chassisMountBLive.x,
					 chassisMountBLive.y, chassisMountBLive.z, b3Distance( wheelCenterLive, chassisMountBLive ) );

		int rodIndex = FindPartByBoneNodeIndex( mesh, 7 );
		if ( rodIndex >= 0 )
		{
			b3Vec3 inboardA, outboardA;
			PartXEnds( mesh.parts[rodIndex], c.mirror == false, inboardA, outboardA );
			b3Pos outboardLive = b3TransformPoint( hubWorld, outboardA );
			float restLen = b3Distance( inboardA, outboardA );
			float liveLen = b3Distance( c.rackRestWorld, outboardLive );
			std::printf( "m9 corner mirror=%d steeringRod restLen=%.4f liveLen=%.4f (stretch factor %.3f)\n", c.mirror ? 1 : 0,
						 restLen, liveLen, restLen > 1.0e-5f ? liveLen / restLen : 0.0f );
		}
	}

	b3WorldTransform ComputeHubWorld( const Corner& c ) const
	{
		return b3MulWorldTransforms( b3Body_GetTransform( c.knuckleBodyId ), c.hubLocal );
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "M9 Steering Rig Bench - two corners, real 2-DOF suspension, no driving" );
		ImGui::TextWrapped( "Each corner: vertical travel (spring) + steer yaw (spring), both on a static chassis anchor. "
							"Validates OneSided_Steering_Suspension_Rig's rig binding before it goes near the vehicle." );
		ImGui::Separator();

		ImGui::Checkbox( "Animate travel (auto bounce)", &m_animateTravel );
		ImGui::SliderFloat( "Bounce amplitude", &m_travelAmplitude, 0.0f, 0.28f, "%.2f m" );
		ImGui::SliderFloat( "Bounce frequency", &m_travelFrequency, 0.1f, 2.0f, "%.2f Hz" );
		if ( m_animateTravel == false )
		{
			ImGui::SliderFloat( "Manual travel", &m_manualTravel, -m_travelLimit, m_travelLimit, "%.3f m" );
		}
		ImGui::SliderFloat( "Steer angle", &m_steerAngleDeg, -35.0f, 35.0f, "%.1f deg" );
		ImGui::Separator();

		ImGui::Checkbox( "Show mount model", &m_showModel );
		ImGui::Checkbox( "Show wheel", &m_showWheel );
		ImGui::Checkbox( "Show damper", &m_showDumper );
		ImGui::Checkbox( "Show socket overlay", &m_showSockets );
		ImGui::Separator();

		ImGui::TextWrapped( "contract: %s", m_contract.status.c_str() );
		for ( const std::string& warning : m_contract.warnings )
		{
			ImGui::BulletText( "warning: %s", warning.c_str() );
		}
		ImGui::TextWrapped( "rig L: %s", m_riggedRigL.status.c_str() );
		for ( const JozzVehicleRiggedPart& part : m_riggedRigL.parts )
		{
			ImGui::BulletText( "%s: %d tris (node %d)", part.boneName.c_str(), part.triangleCount, part.boneNodeIndex );
		}
		return true;
	}

	void RenderCorner( const Corner& c )
	{
		const JozzVehicleRiggedMesh& mesh = c.mirror ? m_riggedRigR : m_riggedRigL;
		b3WorldTransform hubWorld = ComputeHubWorld( c );
		b3WorldTransform bracketWorld = c.bracketWorld;
		bool outboardIsNegX = ( c.mirror == false );

		if ( m_showWheel && m_wheelVisual.IsLoaded() )
		{
			b3WorldTransform knuckleLive = b3Body_GetTransform( c.knuckleBodyId );
			b3Transform axleFix = { b3Vec3_zero, WheelAxleFix() };
			b3WorldTransform wheelDraw = b3MulWorldTransforms( b3MulWorldTransforms( knuckleLive, axleFix ), m_wheelCorrection );
			m_wheelVisual.DrawAtTransform( wheelDraw, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
		}

		b3Pos damperLowerLive = c.restWorld;
		if ( m_showModel && mesh.IsLoaded() )
		{
			const Vec4 white = MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f );
			const Vec4 rodColor = MakeVec4( 0.95f, 0.75f, 0.2f, 1.0f );
			for ( int i = 0; i < mesh.PartCount(); ++i )
			{
				const JozzVehicleRiggedPart& part = mesh.parts[i];
				if ( part.boneNodeIndex == 3 || part.boneNodeIndex == 5 ) // Chassis_Top / Chassis_Bottom (wishbone arms)
				{
					b3Vec3 chassisEndA, wheelEndA;
					PartXEnds( part, outboardIsNegX, chassisEndA, wheelEndA );
					b3Pos chassisEndLive = b3TransformPoint( bracketWorld, chassisEndA );
					b3Pos wheelEndLive = b3TransformPoint( hubWorld, wheelEndA );
					mesh.DrawPartBetween( i, chassisEndA, wheelEndA, chassisEndLive, wheelEndLive, white );

					if ( part.boneNodeIndex == 5 ) // lower arm carries the damper's lower eye
					{
						JozzVehicleArmPlacement placement =
							JozzVehicleComputeArmPlacement( chassisEndA, wheelEndA, chassisEndLive, wheelEndLive );
						damperLowerLive = JozzVehicleMapAuthoredPoint( placement, MirrorX( m_sockets.damperLower, c.mirror ) );
					}
				}
				else if ( part.boneNodeIndex == 7 ) // Socket_SteeringRod
				{
					b3Vec3 inboardA, outboardA;
					PartXEnds( part, outboardIsNegX, inboardA, outboardA );
					b3Pos outboardLive = b3TransformPoint( hubWorld, outboardA );
					mesh.DrawPartBetween( i, inboardA, outboardA, c.rackRestWorld, outboardLive, rodColor );
				}
				else if ( part.boneNodeIndex == 8 ) // Socket_WheelCenter -> knuckle
				{
					// Tinted (not white) so its boundary against ChassisMount_b is
					// unambiguous in a screenshot - both ride the same rigid knuckle
					// and sit close together by design, but must read as two
					// distinct parts, never as one fused blob.
					mesh.DrawPart( i, hubWorld, MakeVec4( 0.2f, 1.0f, 0.4f, 1.0f ) );
				}
				else if ( part.boneNodeIndex == 6 ) // Socket_ChassisMount_b (upright/knuckle carrier) -> knuckle
				{
					mesh.DrawPart( i, hubWorld, MakeVec4( 0.2f, 0.9f, 1.0f, 1.0f ) );
				}
				else // ChassisMount_a, SingleDamper_Mount -> chassis
				{
					mesh.DrawPart( i, bracketWorld, white );
				}
			}
		}

		if ( m_showDumper && m_dumper.IsLoaded() )
		{
			b3Pos topWorld = b3TransformPoint( bracketWorld, MirrorX( m_sockets.damperUpper, c.mirror ) );
			m_dumper.DrawTelescopingDamper( topWorld, damperLowerLive, MakeVec4( 0.8f, 0.82f, 0.88f, 1.0f ) );
		}

		if ( m_showSockets )
		{
			b3Pos wheelCenterLive = b3TransformPoint( hubWorld, MirrorX( m_sockets.wheelCenter, c.mirror ) );
			b3Pos chassisMountBLive = b3TransformPoint( hubWorld, MirrorX( m_sockets.chassisMountB, c.mirror ) );
			b3Pos chassisMountALive = b3TransformPoint( bracketWorld, MirrorX( m_sockets.chassisMountA, c.mirror ) );
			DrawCross( wheelCenterLive, 0.09f, MakeVec4( 0.2f, 1.0f, 0.4f, 1.0f ) );
			DrawCross( chassisMountBLive, 0.08f, MakeVec4( 0.2f, 0.9f, 1.0f, 1.0f ) );
			DrawCross( chassisMountALive, 0.06f, MakeVec4( 0.95f, 0.55f, 0.15f, 1.0f ) );
			DrawCross( damperLowerLive, 0.06f, MakeVec4( 0.9f, 0.3f, 0.9f, 1.0f ) );
			DrawCross( c.rackRestWorld, 0.07f, MakeVec4( 0.95f, 0.75f, 0.2f, 1.0f ) );
		}
	}

	void Render() override
	{
		Sample::Render();

		RenderCorner( m_left );
		RenderCorner( m_right );

		DrawTextLine( "M9 Steering Rig Bench - OneSided_Steering_Suspension_Rig, two corners" );
		DrawTextLine( "green = WheelCenter (knuckle); cyan = ChassisMount_b (knuckle, upright); orange = ChassisMount_a (chassis)" );
		DrawTextLine( "magenta = damper lower eye (lower arm); yellow = steering rod + fixed rack reference" );
		DrawTextLine( "steer %.1f deg, travel %.3f m", m_steerAngleDeg, m_animateTravel ? 0.0f : m_manualTravel );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehicleM9SteeringRigBench( context );
	}

	JozzVehicleAuditMetadata m_assetMetadata;
	JozzVehicleVisualMesh m_wheelVisual;
	b3Transform m_wheelCorrection;
	JozzVehicleAssetContract m_contract;
	JozzVehicleSteeringSuspensionSockets m_sockets;
	JozzVehicleRiggedMesh m_riggedRigL;
	JozzVehicleRiggedMesh m_riggedRigR;
	JozzVehicleRiggedMesh m_dumper;

	Corner m_left;
	Corner m_right;

	float m_metersPerBlockbenchUnit;
	float m_wheelRadius;
	float m_restHeight;
	float m_trackHalf;

	bool m_animateTravel;
	float m_travelAmplitude;
	float m_travelFrequency;
	float m_manualTravel;
	float m_steerAngleDeg;
	float m_time;

	float m_suspensionHertz;
	float m_suspensionDamping;
	float m_travelLimit;
	float m_steerHertz;
	float m_steerDamping;
	float m_carrierMass;
	float m_knuckleMass;

	bool m_showModel;
	bool m_showWheel;
	bool m_showDumper;
	bool m_showSockets;
	bool m_dumpOnce = false;
};

Sample* CreateJozzVehicleM9SteeringRigBench( SampleContext* context )
{
	return JozzVehicleM9SteeringRigBench::Create( context );
}
