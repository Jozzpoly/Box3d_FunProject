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
// OneSided_Steering_Suspension_Rig model. Each corner has TWO physics DOF -
// vertical travel (prismatic spring, chassis->carrier) and steer yaw
// (revolute spring, carrier->knuckle/upright). The wheel's rolling spin is
// NOT a physics DOF: it is a purely VISUAL rotation applied to the tire mesh
// only. This is deliberate, and it is the crux of the two rig-binding rules
// Jozz spelled out:
//
//   RULE 1 - Socket_WheelCenter must NOT roll with the wheel. It is the
//   wheel-centre reference: it positions the wheel, it steers left/right with
//   the knuckle, but it does not spin about the axle. So its mesh part rides
//   the KNUCKLE body (steer only), exactly like Socket_ChassisMount_b. Only
//   the tire mesh gets the rolling rotation. (An earlier version wrongly put
//   the WheelCenter geometry on a spinning wheel body - fixed here.)
//
//   RULE 2 - the wheel's roll axis passes THROUGH Socket_WheelCenter, and the
//   tire is positioned by Socket_WheelCenter (NOT Socket_ChassisMount_b). The
//   tire's inboard hub face (the wheel model's Socket_WheelMount) is set onto
//   Socket_WheelCenter, so the tire extends OUTBOARD from there and does not
//   sink into the suspension (the reused M8 attach logic).
//
// The single most important rule (Jozz repeated it many times): WheelCenter
// and ChassisMount_b are SEPARATE and must NOT move together. When you steer,
// only WheelCenter and the wheel turn; ChassisMount_b stays put. They ride
// DIFFERENT bodies:
//   - WheelCenter, the steering-rod outboard end, and the tire ride the
//     KNUCKLE (carrier -> knuckle revolute): they steer left/right.
//   - ChassisMount_b and the wishbone arm wheel-ends ride the CARRIER
//     (chassis -> carrier prismatic): they travel up/down but do NOT steer.
//     ChassisMount_b connects to the wishbone arms, and the arms do not steer,
//     so ChassisMount_b holds still while the wheel yaws away from it.
//
// So the motions are cleanly separated:
//   - travel (up/down): carrier prismatic - carries BOTH WheelCenter (via the
//     knuckle stacked on the carrier) and ChassisMount_b, together.
//   - steer (left/right yaw): knuckle revolute - turns ONLY WheelCenter + the
//     steering rod + the tire. ChassisMount_b + arms stay. THIS is the split.
//   - roll (drive spin): visual-only, tire mesh only, about the axle through
//     WheelCenter. Nothing structural rolls with it.
//
// This model's node layout is NOT a copy of One_Sided_wheel_mount (M8's
// model) - see assets/contracts/one_sided_steering_suspension.asset.json's
// "notes" block. The trap that took several rounds to get right: putting
// ChassisMount_b on the steering body fuses it to WheelCenter so both yaw
// together - wrong. ChassisMount_b belongs on the non-steering carrier.
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
		b3BodyId knuckleBodyId; // upright: ChassisMount_b, WheelCenter, arm wheel-ends, steering rod outboard end
		b3JointId travelJointId;
		b3JointId steerJointId;

		b3WorldTransform bracketWorld; // fixed chassis-side placement (chassis never moves in this bench)
		b3Transform knuckleHubLocal;	// baked knuckle-local offset: parts that STEER (WheelCenter, steering-rod outer end)
		b3Transform carrierHubLocal;	// baked carrier-local offset: parts that TRAVEL but do NOT steer (ChassisMount_b, arm wheel-ends)
		b3Transform tireDrawLocal;		// baked knuckle-local tire draw frame (tire centre offset OUTBOARD from WheelCenter,
										// axle along the steered lateral axis; spin is applied on top of this, visual only)
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
		m_knuckleMass = 40.0f; // upright + wheel unsprung mass (no separate wheel body)
		m_wheelSpinRadPerSec = 0.0f;
		m_wheelSpinAngle = 0.0f;

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
		if ( const char* v = std::getenv( "JOZZ_M9_SPIN" ) )
		{
			m_wheelSpinRadPerSec = (float)atof( v );
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

	// Orientation of the tire draw frame: the wheel mesh + wheelCorrection put
	// the tire's axle on the frame's local +Y; this maps that axle onto world
	// +Z (the lateral axis) at rest. The tire is drawn (not simulated) so this
	// is a pure render transform, composed onto the live knuckle transform -
	// the tire therefore steers with the knuckle but never inherits a rolling
	// rotation from physics. The rolling spin is added separately as a visual
	// rotation about this same local +Y (WheelSpinAxisLocal), tire mesh only.
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

		// Knuckle/upright: carries every steering-but-not-rolling part
		// (ChassisMount_b, WheelCenter, arm wheel-ends, steering-rod outboard
		// end) and defines the wheel-centre + roll axis. Mass includes the
		// wheel's unsprung mass since there is no separate wheel body anymore.
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
		// wheel_center socket landing on the corner's rest world point. The whole
		// mount model, including the WheelCenter part, rides the knuckle.
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
		b3Vec3 wc = MirrorX( m_sockets.wheelCenter, mirror );
		b3Vec3 rp = b3RotateVector( yaw, wc );
		b3WorldTransform placementRest;
		placementRest.q = yaw;
		placementRest.p = { c.restWorld.x - rp.x, c.restWorld.y - rp.y, c.restWorld.z - rp.z };

		c.bracketWorld = placementRest; // chassis never moves in this bench
		// Two bakes from the SAME rest placement, against two DIFFERENT bodies.
		// At rest they coincide; live they diverge - the knuckle steers, the
		// carrier does not. This is what keeps WheelCenter (knuckle) and
		// ChassisMount_b (carrier) SEPARATE: steering turns only the knuckle.
		c.knuckleHubLocal = b3InvMulWorldTransforms( b3Body_GetTransform( c.knuckleBodyId ), placementRest );
		c.carrierHubLocal = b3InvMulWorldTransforms( b3Body_GetTransform( c.carrierBodyId ), placementRest );

		// Tire draw frame (RULE 2): the tire centre is offset OUTBOARD from
		// WheelCenter so the wheel model's inboard hub face (Socket_WheelMount)
		// sits ON WheelCenter and the tire extends away from the suspension
		// instead of sinking into it. The offset is taken purely along the axle
		// (the roll axis), so the axle line passes exactly through WheelCenter.
		b3Vec3 mountBU = JozzVehicleFindPointOrBuiltIn( m_assetMetadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		// Socket_WheelMount expressed in the tire's draw-local frame (before the
		// {centre, WheelAxleFix} body transform): the wheelCorrection maps the
		// authored mesh so its axle is local +Y, so this vector's Y component is
		// the inboard-face-to-tire-centre distance along the axle.
		b3Vec3 mountFaceLocal = b3TransformPoint( m_wheelCorrection, b3MulSV( m_metersPerBlockbenchUnit, mountBU ) );
		float axleOffset = mountFaceLocal.y; // signed distance along the axle from tire centre to hub face
		// Outboard is -Z for the left corner, +Z for the mirrored right corner.
		float outboardSign = mirror ? 1.0f : -1.0f;
		b3Vec3 tireCentreRestWorld = { c.restWorld.x, c.restWorld.y, c.restWorld.z + outboardSign * std::fabs( axleOffset ) };
		b3WorldTransform tireRest;
		tireRest.q = WheelAxleFix();
		tireRest.p = tireCentreRestWorld;
		c.tireDrawLocal = b3InvMulWorldTransforms( b3Body_GetTransform( c.knuckleBodyId ), tireRest );

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

		// Wheel roll is VISUAL ONLY (RULE 1): accumulate an angle here and apply
		// it to the tire mesh at draw time. It never touches any physics body,
		// so it can never leak into WheelCenter, the knuckle, or any socket.
		m_wheelSpinAngle += m_wheelSpinRadPerSec * dt;

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
		b3WorldTransform knuckleHubWorld = ComputeKnuckleHubWorld( c );
		b3WorldTransform carrierHubWorld = ComputeCarrierHubWorld( c );
		b3Vec3 wc = MirrorX( m_sockets.wheelCenter, c.mirror );
		b3Vec3 cmb = MirrorX( m_sockets.chassisMountB, c.mirror );
		b3Pos wheelCenterLive = b3TransformPoint( knuckleHubWorld, wc );	 // steers
		b3Pos chassisMountBLive = b3TransformPoint( carrierHubWorld, cmb ); // does NOT steer

		// Tire centre (no spin) and its inboard/outboard faces along the axle, so
		// the "does the tire clear the suspension" check is a hard number.
		b3WorldTransform tireWorld = ComputeTireDrawWorld( c, false );
		b3Pos tireCentre = tireWorld.p;
		b3Vec3 axleDir = b3RotateVector( tireWorld.q, b3Vec3_axisY ); // roll axis in world
		float halfWidth = 0.5f * m_wheelRadius * 0.85f;				 // rough tire half-width for the face print
		b3Pos faceInboard = b3OffsetPos( tireCentre, b3MulSV( c.mirror ? halfWidth : -halfWidth, axleDir ) );
		// Distance from the axle line to WheelCenter (RULE 2: must be ~0).
		b3Vec3 toWc = b3Sub( wheelCenterLive, tireCentre );
		float alongAxle = b3Dot( toWc, axleDir );
		float axleMiss = b3Length( b3Sub( toWc, b3MulSV( alongAxle, axleDir ) ) );

		std::printf( "m9 corner mirror=%d wheelCenterLive=(%.4f,%.4f,%.4f) chassisMountB=(%.4f,%.4f,%.4f) span=%.4f\n",
					 c.mirror ? 1 : 0, wheelCenterLive.x, wheelCenterLive.y, wheelCenterLive.z, chassisMountBLive.x,
					 chassisMountBLive.y, chassisMountBLive.z, b3Distance( wheelCenterLive, chassisMountBLive ) );
		std::printf( "m9 corner mirror=%d tireCentre=(%.4f,%.4f,%.4f) tireInboardFaceZ=%.4f axleMissFromWheelCenter=%.4f m "
					 "(RULE2 ok iff ~0)\n",
					 c.mirror ? 1 : 0, tireCentre.x, tireCentre.y, tireCentre.z, faceInboard.z, axleMiss );

		const JozzVehicleRiggedMesh& mesh = c.mirror ? m_riggedRigR : m_riggedRigL;
		int rodIndex = FindPartByBoneNodeIndex( mesh, 7 );
		if ( rodIndex >= 0 )
		{
			b3Vec3 inboardA, outboardA;
			PartXEnds( mesh.parts[rodIndex], c.mirror == false, inboardA, outboardA );
			b3Pos outboardLive = b3TransformPoint( knuckleHubWorld, outboardA );
			float restLen = b3Distance( inboardA, outboardA );
			float liveLen = b3Distance( c.rackRestWorld, outboardLive );
			std::printf( "m9 corner mirror=%d steeringRod restLen=%.4f liveLen=%.4f (stretch factor %.3f)\n", c.mirror ? 1 : 0,
						 restLen, liveLen, restLen > 1.0e-5f ? liveLen / restLen : 0.0f );
		}
	}

	b3WorldTransform ComputeKnuckleHubWorld( const Corner& c ) const
	{
		return b3MulWorldTransforms( b3Body_GetTransform( c.knuckleBodyId ), c.knuckleHubLocal );
	}

	// Carrier placement: travels up/down with the suspension but never steers.
	// Carries ChassisMount_b and the wishbone arm wheel-ends, so they hold
	// still while WheelCenter + the wheel yaw away from them under steering.
	b3WorldTransform ComputeCarrierHubWorld( const Corner& c ) const
	{
		return b3MulWorldTransforms( b3Body_GetTransform( c.carrierBodyId ), c.carrierHubLocal );
	}

	// Tire draw transform: the knuckle (steer + travel) carries the baked tire
	// frame; the visual roll spin is applied on top about the tire's own axle
	// (local +Y) so it never touches any physics body. `withSpin` lets callers
	// (dumps) ask for the un-spun frame when they only care about placement.
	b3WorldTransform ComputeTireDrawWorld( const Corner& c, bool withSpin ) const
	{
		b3WorldTransform base = b3MulWorldTransforms( b3Body_GetTransform( c.knuckleBodyId ), c.tireDrawLocal );
		if ( withSpin == false )
		{
			return base;
		}
		b3Transform spin = { b3Vec3_zero, b3MakeQuatFromAxisAngle( b3Vec3_axisY, m_wheelSpinAngle ) };
		return b3MulWorldTransforms( base, spin );
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
		ImGui::SliderFloat( "Wheel roll (test)", &m_wheelSpinRadPerSec, -10.0f, 10.0f, "%.1f rad/s" );
		ImGui::TextWrapped( "Roll is VISUAL, tire mesh only, about the axle through WheelCenter. WheelCenter and the whole "
							"rig steer but never roll - drag this and watch only the tire turn." );
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
		b3WorldTransform knuckleHubWorld = ComputeKnuckleHubWorld( c );	// STEERS (WheelCenter, steering-rod outer, tire)
		b3WorldTransform carrierHubWorld = ComputeCarrierHubWorld( c ); // travels, does NOT steer (ChassisMount_b, arm ends)
		b3WorldTransform bracketWorld = c.bracketWorld;
		bool outboardIsNegX = ( c.mirror == false );

		if ( m_showWheel && m_wheelVisual.IsLoaded() )
		{
			// Tire = knuckle placement (steer + travel) + visual roll spin, then
			// the mesh-centering wheelCorrection. The roll never touches physics
			// or the mount model, so only the tire turns when rolling.
			b3WorldTransform wheelDraw = b3MulWorldTransforms( ComputeTireDrawWorld( c, true ), m_wheelCorrection );
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
					// Arms bridge chassis -> CARRIER (not the knuckle): they travel
					// but do not steer, and their wheel-end meets ChassisMount_b.
					b3Vec3 chassisEndA, wheelEndA;
					PartXEnds( part, outboardIsNegX, chassisEndA, wheelEndA );
					b3Pos chassisEndLive = b3TransformPoint( bracketWorld, chassisEndA );
					b3Pos wheelEndLive = b3TransformPoint( carrierHubWorld, wheelEndA );
					mesh.DrawPartBetween( i, chassisEndA, wheelEndA, chassisEndLive, wheelEndLive, white );

					if ( part.boneNodeIndex == 5 ) // lower arm carries the damper's lower eye
					{
						JozzVehicleArmPlacement placement =
							JozzVehicleComputeArmPlacement( chassisEndA, wheelEndA, chassisEndLive, wheelEndLive );
						damperLowerLive = JozzVehicleMapAuthoredPoint( placement, MirrorX( m_sockets.damperLower, c.mirror ) );
					}
				}
				else if ( part.boneNodeIndex == 7 ) // Socket_SteeringRod - outboard end on the knuckle (steering arm)
				{
					b3Vec3 inboardA, outboardA;
					PartXEnds( part, outboardIsNegX, inboardA, outboardA );
					b3Pos outboardLive = b3TransformPoint( knuckleHubWorld, outboardA );
					mesh.DrawPartBetween( i, inboardA, outboardA, c.rackRestWorld, outboardLive, rodColor );
				}
				else if ( part.boneNodeIndex == 8 ) // Socket_WheelCenter -> the KNUCKLE (STEERS with the wheel, does NOT roll)
				{
					// Green. Rides the knuckle: steers left/right with the wheel,
					// never rolls with the tire. SEPARATE from ChassisMount_b -
					// under steering this turns while ChassisMount_b stays.
					mesh.DrawPart( i, knuckleHubWorld, MakeVec4( 0.2f, 1.0f, 0.4f, 1.0f ) );
				}
				else if ( part.boneNodeIndex == 6 ) // Socket_ChassisMount_b -> the CARRIER (travels, does NOT steer)
				{
					// Cyan. Rides the carrier with the wishbone arms: it moves up/
					// down with travel but holds still when steering. This is the
					// separation Jozz required - it must NOT turn with WheelCenter.
					mesh.DrawPart( i, carrierHubWorld, MakeVec4( 0.2f, 0.9f, 1.0f, 1.0f ) );
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
			b3Pos wheelCenterLive = b3TransformPoint( knuckleHubWorld, MirrorX( m_sockets.wheelCenter, c.mirror ) );
			b3Pos chassisMountBLive = b3TransformPoint( carrierHubWorld, MirrorX( m_sockets.chassisMountB, c.mirror ) );
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
		DrawTextLine( "green = WheelCenter: STEERS with the wheel, no roll. cyan = ChassisMount_b: travels, does NOT steer (SEPARATE - stays when you steer)" );
		DrawTextLine( "orange = ChassisMount_a (chassis); magenta = damper lower (lower arm); yellow = steering rod; tire rolls visual-only about WheelCenter" );
		DrawTextLine( "steer %.1f deg, travel %.3f m, roll %.1f rad/s (tire only)", m_steerAngleDeg,
					  m_animateTravel ? 0.0f : m_manualTravel, m_wheelSpinRadPerSec );
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
	float m_wheelSpinRadPerSec; // visual roll rate (rad/s) applied to the tire mesh only
	float m_wheelSpinAngle;		 // accumulated visual roll angle (rad)

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
