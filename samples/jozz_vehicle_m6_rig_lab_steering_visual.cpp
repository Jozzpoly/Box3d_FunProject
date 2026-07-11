// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_rig_lab_internal.h"

// Rig-editor warm-up, gejt G1 (patrz docs/PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md
// + docs/EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md). Importuje NOWY model
// OneSided_Steering_Suspension_Rig na PRZEDNIE narożniki jeżdżącego pojazdu M6,
// tak by przód (nowy rig sterujący) i tył (obecny mount) dało się testować razem
// (decyzja D1a). Czysto WIZUALNIE: te same żywe ciała, które już napędza fizyka -
// zero nowych ciał/jointów, więc walidator zostaje bez zmian.
//
// Kluczowa reguła Jozza (odkrycie O1 w dokumencie audytu): WheelCenter i
// ChassisMount_b muszą być OSOBNO. Realny narożnik M6 nie ma ciała "carrier"
// (knuckle jeździ I skręca), więc rolę "jeździ-ale-nie-skręca" gra RAMIĘ. Stąd
// trzy piecze tej samej pozy modelu, każda względem innego ciała:
//   - chassisId  : ChassisMount_a, SingleDamper_Mount (nieruchome brackety)
//   - lowerArmId : ChassisMount_b + końce wahaczy (jeżdżą, NIE skręcają)
//   - knuckleId  : WheelCenter (jeździ I skręca z kołem)
// Gejt G3 (2026-07-11): drążek kierowniczy (node 7) rysowany między ŚRODKIEM
// realnego racka (inboard, z=0 - lewy i prawy drążek spotykają się tam) a
// knucklem (outboard) - gdy maglownica jedzie przy skręcie, oba drążki przesuwają
// się z nią razem (kontrakt: zawsze realny rack, nigdy zmyślony punkt). Damper
// rigu: górne oko na chassis, dolne oko na dolnym ramieniu (mapowane przez żywą
// pozę ramienia), ten sam asset i gate co tylne dampery.

void JozzVehicleM6RigLab::LoadSteeringRig()
	{
		m_riggedSteeringL.Destroy();
		m_riggedSteeringR.Destroy();

		m_steeringContract = LoadJozzVehicleAssetContract( "one_sided_steering_suspension.asset.json" );
		float mpbu = m_steeringContract.metersPerBlockbenchUnit > 0.0f ? m_steeringContract.metersPerBlockbenchUnit
																	   : m_metersPerBlockbenchUnit;
		std::string rigPath = m_steeringContract.sourcePath;
		if ( rigPath.empty() )
		{
			FindJozzVehicleAssetFile( "assets/source/OneSided_Steering_Suspension_Rig.gltf", &rigPath );
		}
		if ( rigPath.empty() == false )
		{
			m_riggedSteeringL.LoadSkinnedGltf( rigPath.c_str(), mpbu, false );
			m_riggedSteeringR.LoadSkinnedGltf( rigPath.c_str(), mpbu, true );
		}

		// Fallback authored sockets (meters) = the shipped rig's values, matching
		// the M9 bench; used only if the contract fails to resolve. Only
		// wheelCenter feeds the bake below, but the full set is kept for the later
		// damper/rod/cardan gates.
		m_steeringSockets = {};
		m_steeringSockets.wheelCenter = { -0.4156f, 0.175f, 0.0f };
		m_steeringSockets.chassisMountA = { 0.00546f, 0.56875f, -0.15859f };
		m_steeringSockets.chassisMountB = { -0.27342f, 0.33908f, 0.0f };
		m_steeringSockets.damperMount = { 0.13125f, 0.50313f, 0.0f };
		m_steeringSockets.damperUpper = { 0.01642f, 0.64533f, -0.28438f };
		m_steeringSockets.damperLower = { -0.25158f, 0.01092f, -0.28438f };
		m_steeringSockets.steeringRod = { -0.18046f, 0.25158f, 0.26796f };
		JozzVehicleSteeringSuspensionSockets resolved = ResolveJozzVehicleSteeringSuspensionSockets( m_steeringContract );
		if ( resolved.resolved )
		{
			m_steeringSockets = resolved;
		}
	}

	// Bake the front corners' model placement into three body-local frames (one
	// per kinematic role, see the file header). Same landing math as
	// SetupMountRig - yaw -90, WheelCenter socket lands on the wheel's inboard hub
	// face - but split three ways instead of two. Front corners only; rear keeps
	// the old mount. Called after CreateJozzVehicleM6, bodies at rest.
void JozzVehicleM6RigLab::SetupSteeringRig()
	{
		b3Vec3 mountBU = JozzVehicleFindPointOrBuiltIn( m_assetMetadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
		b3WorldTransform chassisRest = b3Body_GetTransform( m_vehicle.chassisId );

		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			m_steeringCornerActive[corner] = false;

			bool isFront = corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
			if ( isFront == false )
			{
				continue;
			}

			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			if ( runtime.rigType != JOZZ_M6_RIG_DOUBLE_WISHBONE || B3_IS_NULL( runtime.knuckleId ) ||
				 B3_IS_NULL( runtime.lowerArmId ) )
			{
				continue;
			}
			m_steeringCornerActive[corner] = true;

			bool isLeft = CornerIsLeft( corner );

			// Attach = the wheel's Socket_WheelMount (inboard hub face), reflected
			// for the right side. Identical to SetupMountRig so the new model lands
			// exactly where the old mount does (its WheelCenter socket ~matches).
			b3WorldTransform wheelDraw =
				b3MulWorldTransforms( b3Body_GetTransform( runtime.wheelId ), m_wheelVisualCorrection );
			b3Pos wheelCentre = b3Body_GetPosition( runtime.wheelId );
			b3Pos mountWorld = b3TransformWorldPoint( wheelDraw, b3MulSV( m_metersPerBlockbenchUnit, mountBU ) );
			b3Vec3 offset = b3Sub( mountWorld, wheelCentre );
			if ( isLeft == false )
			{
				offset.z = -offset.z;
			}
			b3Pos attach = b3OffsetPos( wheelCentre, offset );

			b3Vec3 pwc = m_steeringSockets.wheelCenter;
			if ( isLeft == false )
			{
				pwc.x = -pwc.x;
			}
			b3Vec3 rp = b3RotateVector( yaw, pwc );

			b3WorldTransform placementRest;
			placementRest.q = yaw;
			placementRest.p = { attach.x - rp.x, attach.y - rp.y, attach.z - rp.z };

			m_steeringChassisLocal[corner] = b3InvMulWorldTransforms( chassisRest, placementRest );
			m_steeringArmLocal[corner] = b3InvMulWorldTransforms( b3Body_GetTransform( runtime.lowerArmId ), placementRest );
			m_steeringKnuckleLocal[corner] = b3InvMulWorldTransforms( b3Body_GetTransform( runtime.knuckleId ), placementRest );
		}
	}

	// Draw the new rig on the active (front) corners, each part on the body its
	// kinematic role demands. Left corners use the authored mesh, right the
	// mirror. Steering it live, only WheelCenter + the wheel yaw; ChassisMount_b
	// and the arm ends hold still on the (non-steering) lower arm - the split
	// Jozz required, now on real vehicle bodies.
void JozzVehicleM6RigLab::DrawSteeringRig()
	{
		b3WorldTransform chassisLive = b3Body_GetTransform( m_vehicle.chassisId );
		const Vec4 white = MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f );
		const Vec4 rodColor = MakeVec4( 0.95f, 0.75f, 0.2f, 1.0f );	 // steering rod (yellow)
		const Vec4 damperColor = MakeVec4( 0.82f, 0.84f, 0.9f, 1.0f ); // telescoping shock

		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			if ( m_steeringCornerActive[corner] == false )
			{
				continue;
			}
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			const JozzVehicleRiggedMesh& mesh = CornerIsLeft( corner ) ? m_riggedSteeringL : m_riggedSteeringR;
			if ( mesh.IsLoaded() == false )
			{
				continue;
			}

			b3WorldTransform chassisWorld = b3MulWorldTransforms( chassisLive, m_steeringChassisLocal[corner] );
			b3WorldTransform armWorld =
				b3MulWorldTransforms( b3Body_GetTransform( runtime.lowerArmId ), m_steeringArmLocal[corner] );
			b3WorldTransform knuckleWorld =
				b3MulWorldTransforms( b3Body_GetTransform( runtime.knuckleId ), m_steeringKnuckleLocal[corner] );

			bool isLeft = CornerIsLeft( corner );
			bool wheelNegX = isLeft;

			// Live inboard reference for the steering rod = the REAL rack body's
			// CENTRE (z=0). Both the left and right rods meet there (Jozz: "prawy z
			// lewym się łączył") and slide together as the rack translates under
			// steering. Still the real rack body (never a guessed point) - just its
			// centre rather than its ±halfWidth ends, which sit right at the knuckle
			// and left the rod a short stub. Front wishbone always has a rack; the
			// guard is defensive.
			bool haveRack = B3_IS_NON_NULL( m_vehicle.rackId );
			b3Pos rackCenterLive = {};
			if ( haveRack )
			{
				rackCenterLive = b3Body_GetWorldPoint( m_vehicle.rackId, { 0.0f, 0.0f, 0.0f } );
			}

			// Damper lower eye rides the lower arm. Filled precisely when the lower-
			// arm part is drawn (mapped through its live placement, like the M9
			// bench); the arm-frame transform is the fallback until then.
			b3Vec3 damperLowerAuthored = m_steeringSockets.damperLower;
			if ( isLeft == false )
			{
				damperLowerAuthored.x = -damperLowerAuthored.x;
			}
			b3Pos damperLowerLive = b3TransformPoint( armWorld, damperLowerAuthored );

			for ( int i = 0; i < mesh.PartCount(); ++i )
			{
				int node = mesh.parts[i].boneNodeIndex;
				if ( node == 3 || node == 5 ) // Chassis_Top / Chassis_Bottom = wishbone arms
				{
					// Chassis end rides the chassis bracket, wheel end rides the
					// lower arm (carrier: travels, does NOT steer). The arm flexes
					// between them exactly as the physics arm swings.
					b3Vec3 chassisEnd, wheelEnd;
					ArmEnds( mesh.parts[i], wheelNegX, chassisEnd, wheelEnd );
					b3Pos chassisEndLive = b3TransformPoint( chassisWorld, chassisEnd );
					b3Pos wheelEndLive = b3TransformPoint( armWorld, wheelEnd );
					mesh.DrawPartBetween( i, chassisEnd, wheelEnd, chassisEndLive, wheelEndLive, white );

					if ( node == 5 ) // lower arm carries the damper's lower eye
					{
						JozzVehicleArmPlacement placement =
							JozzVehicleComputeArmPlacement( chassisEnd, wheelEnd, chassisEndLive, wheelEndLive );
						damperLowerLive = JozzVehicleMapAuthoredPoint( placement, damperLowerAuthored );
					}
				}
				else if ( node == 8 ) // Socket_WheelCenter -> knuckle (STEERS with the wheel)
				{
					mesh.DrawPart( i, knuckleWorld, white );
				}
				else if ( node == 6 ) // Socket_ChassisMount_b -> lower arm (travels, does NOT steer)
				{
					mesh.DrawPart( i, armWorld, white );
				}
				else if ( node == 7 ) // Socket_SteeringRod -> inboard on the rack centre, outboard on the knuckle
				{
					if ( haveRack )
					{
						b3Vec3 inboardEnd, outboardEnd;
						ArmEnds( mesh.parts[i], wheelNegX, inboardEnd, outboardEnd );
						b3Pos outboardLive = b3TransformPoint( knuckleWorld, outboardEnd );
						mesh.DrawPartBetween( i, inboardEnd, outboardEnd, rackCenterLive, outboardLive, rodColor );
					}
				}
				else // ChassisMount_a, SingleDamper_Mount, anything else -> chassis
				{
					mesh.DrawPart( i, chassisWorld, white );
				}
			}

			// The rig's own telescoping shock: upper eye on the chassis bracket,
			// lower eye on the lower arm. Same asset + gate (m_showDumper) as the
			// rear mount's dampers, which skip the front while this rig is on.
			if ( m_showDumper && m_dumper.IsLoaded() )
			{
				b3Vec3 damperUpperAuthored = m_steeringSockets.damperUpper;
				if ( isLeft == false )
				{
					damperUpperAuthored.x = -damperUpperAuthored.x;
				}
				b3Pos damperTopLive = b3TransformPoint( chassisWorld, damperUpperAuthored );
				m_dumper.DrawTelescopingDamper( damperTopLive, damperLowerLive, damperColor );
			}
		}
	}
