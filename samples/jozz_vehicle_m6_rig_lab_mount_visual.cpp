// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_rig_lab_internal.h"

void JozzVehicleM6RigLab::LoadWheelVisual()
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

	// Jozz's suspension model, rigged per bone onto the LIVE corner bodies: the
	// WheelCenter hub part follows the knuckle (upright: travel + steer, no
	// spin), the chassis brackets and the arm stay on the chassis. Left corners
	// use the model as authored (yaw -90); right corners use the mirrored copy
	// (same yaw) so the asymmetric one-sided model reflects across the car
	// centreline. Per-corner local transforms are baked once in SetupMountRig.
void JozzVehicleM6RigLab::LoadMountVisual()
	{
		m_riggedMountL.Destroy();
		m_riggedMountR.Destroy();

		m_mountContract = LoadJozzVehicleAssetContract( "one_sided_wheel_mount.asset.json" );
		float mpbu = m_mountContract.metersPerBlockbenchUnit > 0.0f ? m_mountContract.metersPerBlockbenchUnit
																	 : m_metersPerBlockbenchUnit;
		std::string mountPath = m_mountContract.sourcePath;
		if ( mountPath.empty() )
		{
			FindJozzVehicleAssetFile( "assets/source/One_Sided_wheel_mount.gltf", &mountPath );
		}
		if ( mountPath.empty() == false )
		{
			m_riggedMountL.LoadSkinnedGltf( mountPath.c_str(), mpbu, false );
			m_riggedMountR.LoadSkinnedGltf( mountPath.c_str(), mpbu, true );
		}

		std::string dumperPath;
		if ( FindJozzVehicleAssetFile( "assets/source/Asset_Dumper.gltf", &dumperPath ) )
		{
			m_dumper.LoadSkinnedGltf( dumperPath.c_str(), m_metersPerBlockbenchUnit );
		}

		m_mountWheelCenterAuthored = { -0.416f, 0.175f, 0.0f };
		const JozzVehicleContractBinding* wc =
			FindJozzVehicleContractBindingByRole( m_mountContract, "suspension.visual.wheel_center" );
		if ( wc != nullptr && wc->resolved )
		{
			m_mountWheelCenterAuthored = wc->positionMeters;
		}

		// Damper sockets: two coilovers per corner, straddling the arm (see the
		// contract - DamperUpper/Lower _L and _R differ only in Z, not X). They
		// are "visual_endpoint"/physicsAuthority:false markers, same status as
		// wheel_center above, so they get the exact same treatment: read once
		// here in the authored (left/unmirrored) frame, mirrored per corner in
		// SetupMountRig-adjacent draw code by negating X only - Z is untouched
		// because the L/R damper pair is not the car's left/right side.
		// Fallbacks below are the shipped One_Sided_wheel_mount.gltf values
		// (BU * 0.35 m/BU) in case the contract fails to load.
		auto readDamperSocket = [this]( const char* role, b3Vec3 fallback ) {
			const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( m_mountContract, role );
			return ( binding != nullptr && binding->resolved ) ? binding->positionMeters : fallback;
		};
		m_damperUpperLAuthored =
			readDamperSocket( "suspension.visual.damper_upper_l", { 0.0164f, 0.6453f, 0.2844f } );
		m_damperUpperRAuthored =
			readDamperSocket( "suspension.visual.damper_upper_r", { 0.0164f, 0.6453f, -0.2844f } );
		m_damperLowerLAuthored =
			readDamperSocket( "suspension.visual.damper_lower_l", { -0.2516f, 0.0109f, 0.2844f } );
		m_damperLowerRAuthored =
			readDamperSocket( "suspension.visual.damper_lower_r", { -0.2516f, 0.0109f, -0.2844f } );
	}

	// Loads (or clears) the body skin the CONFIG names. The ONLY function allowed
	// to (re)load m_bodyVisual - every path that can change
	// m_config.bodyVisualModel (constructor after session load, preset load, the
	// Nadwozie-tab combo, factory reset, env overrides) must funnel through here,
	// or the mesh on screen silently diverges from the config (plan risk R4,
	// docs/FINALIZACJA_ETAP_1_MODEL_I_UI_PL.md §3). Unlike the suspension parts
	// the frame does NOT articulate, so its placement is one constant transform
	// in the chassis body's LOCAL frame - m_bodyVisualOffset (see DrawBodyVisual)
	// is applied at draw time instead of baked in here, so the offset sliders
	// work live without a reload.
void JozzVehicleM6RigLab::ApplyBodyVisualFromConfig()
	{
		m_bodyVisual.Destroy();

		const JozzVehicleBodyModelDef* def = FindJozzVehicleBodyModelByKey( m_config.bodyVisualModel );
		if ( def == nullptr || def->assetPath == nullptr )
		{
			// "brak" (or an unknown key): no skin, so the collision box must show.
			UpdateChassisShapeVisibility();
			return;
		}

		std::string path;
		if ( FindJozzVehicleAssetFile( def->assetPath, &path ) )
		{
			m_bodyVisual.LoadStaticGltf( path.c_str(), m_metersPerBlockbenchUnit );
		}

		// Base pose from the registry row (measured geometry, see
		// jozz_vehicle_body_registry.cpp): e.g. for "rama_rurowa", model bounds
		// 3.28 m long (Z) x 2.73 m wide (X) x 1.23 m tall (Y), centred in X/Z with
		// its floor near y=0; the car's wheelbase is 2.50 m (front +X), track
		// 2.10 m, chassis origin 0.60 m above the axle line. Yaw -90 maps model +Z
		// (rear) onto -X (rear) and model X (width) onto Z (track).
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, def->baseYawDeg * B3_PI / 180.0f );
		m_bodyChassisLocal.q = yaw;
		m_bodyChassisLocal.p = def->basePos;

		// The chassis collision box hides behind the skin that just loaded (and
		// reappears if the load failed) - same SetShapeHidden pattern as the
		// wheel shapes, see UpdateChassisShapeVisibility.
		UpdateChassisShapeVisibility();
	}

	// Draws the frame rigidly on the live chassis: worldTransform = chassisLive o
	// (m_bodyChassisLocal + live offset slider). Whole-mesh (not per-bone)
	// because the frame is one rigid piece. Gated by m_showBodyVisual (JOZZ_M6_BODY
	// + Debug checkbox "Pokaż nadwozie 3D" - a view toggle, independent of WHICH
	// body is selected).
void JozzVehicleM6RigLab::DrawBodyVisual()
	{
		if ( m_bodyVisual.IsLoaded() == false )
		{
			return;
		}
		b3WorldTransform local = m_bodyChassisLocal;
		local.p = b3Add( local.p, m_config.bodyVisualOffset );
		b3WorldTransform chassisLive = b3Body_GetTransform( m_vehicle.chassisId );
		b3WorldTransform world = b3MulWorldTransforms( chassisLive, local );
		m_bodyVisual.DrawAtTransform( world, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
	}

bool JozzVehicleM6RigLab::CornerIsLeft( int corner )
	{
		return corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_REAR_LEFT;
	}

	// Chassis-end and wheel-end of a wishbone-arm part along its authored X axis.
	// The wheel end is the X extreme nearer the wheel centre: authored -X for a
	// left (unmirrored) mesh, +X for a right (mirrored) mesh.
void JozzVehicleM6RigLab::ArmEnds( const JozzVehicleRiggedPart& part, bool wheelNegX, b3Vec3& chassisEnd, b3Vec3& wheelEnd )
	{
		float y = 0.5f * ( part.restMin.y + part.restMax.y );
		float z = 0.5f * ( part.restMin.z + part.restMax.z );
		chassisEnd = { wheelNegX ? part.restMax.x : part.restMin.x, y, z };
		wheelEnd = { wheelNegX ? part.restMin.x : part.restMax.x, y, z };
	}

	// Bake each corner's model placement into two body-local transforms: the
	// brackets/arm relative to the chassis, the hub relative to the knuckle.
	// Live, the parts follow those bodies, so the model articulates exactly as
	// the physics does. Called after CreateJozzVehicleM6, when bodies sit at
	// rest (no spin/steer yet).
void JozzVehicleM6RigLab::SetupMountRig()
	{
		b3Vec3 mountBU = JozzVehicleFindPointOrBuiltIn( m_assetMetadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
		b3WorldTransform chassisRest = b3Body_GetTransform( m_vehicle.chassisId );

		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			m_cornerHasMount[corner] = runtime.rigType == JOZZ_M6_RIG_DOUBLE_WISHBONE && B3_IS_NON_NULL( runtime.knuckleId );
			if ( m_cornerHasMount[corner] == false )
			{
				continue;
			}

			bool isLeft = CornerIsLeft( corner );

			// Attach point: the wheel's Socket_WheelMount (inboard hub face). The
			// offset from the wheel centre is along the axle; reflect it for the
			// right side so the model attaches inboard on both.
			b3WorldTransform wheelDraw = b3MulWorldTransforms( b3Body_GetTransform( runtime.wheelId ), m_wheelVisualCorrection );
			b3Pos wheelCentre = b3Body_GetPosition( runtime.wheelId );
			b3Pos mountWorld = b3TransformWorldPoint( wheelDraw, b3MulSV( m_metersPerBlockbenchUnit, mountBU ) );
			b3Vec3 offset = b3Sub( mountWorld, wheelCentre );
			if ( isLeft == false )
			{
				offset.z = -offset.z;
			}
			b3Pos attach = b3OffsetPos( wheelCentre, offset );

			// Model wheel-centre socket (mirrored copy has authored X negated).
			b3Vec3 pwc = m_mountWheelCenterAuthored;
			if ( isLeft == false )
			{
				pwc.x = -pwc.x;
			}
			b3Vec3 rp = b3RotateVector( yaw, pwc );

			b3WorldTransform placementRest;
			placementRest.q = yaw;
			placementRest.p = { attach.x - rp.x, attach.y - rp.y, attach.z - rp.z };

			m_bracketLocal[corner] = b3InvMulWorldTransforms( chassisRest, placementRest );
			m_hubLocal[corner] = b3InvMulWorldTransforms( b3Body_GetTransform( runtime.knuckleId ), placementRest );
		}
	}

void JozzVehicleM6RigLab::DrawRigDiagnostics()
	{
		// Live rig geometry without any suspension model mounted: yellow =
		// upper arms, orange = lower arms, green = coilover, cyan = tie/toe
		// link, magenta = kingpin axis / strut, white = trailing arm.
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];

			if ( runtime.rigType == JOZZ_M6_RIG_TRAILING_ARM && B3_IS_NON_NULL( runtime.trailingArmId ) )
			{
				b3Pos pivot = b3Body_GetPosition( runtime.trailingArmId );
				b3Pos wheel = b3Body_GetPosition( runtime.wheelId );
				b3Pos damperChassis = b3Body_GetWorldPoint( m_vehicle.chassisId, runtime.trailingDamperChassisLocal );
				b3Pos damperArm = b3Body_GetWorldPoint(
					runtime.trailingArmId, b3Sub( runtime.trailingDamperArmLocal, runtime.trailingPivotLocal ) );

				DrawCross( pivot, 0.07f, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
				DrawLine( pivot, wheel, MakeVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
				DrawLine( damperChassis, damperArm, MakeVec4( 0.2f, 1.0f, 0.3f, 1.0f ) );
				continue;
			}

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

			// Ball joints from the LIVE arm bodies (origin = hinge midpoint),
			// so the drawn arms are the actual constraint geometry, stops and
			// all - not a schematic.
			b3Vec3 upperHingeMid = b3MulSV( 0.5f, b3Add( hp.upperFrontChassis, hp.upperRearChassis ) );
			b3Vec3 lowerHingeMid = b3MulSV( 0.5f, b3Add( hp.lowerFrontChassis, hp.lowerRearChassis ) );
			b3Pos upperBall = b3Body_GetWorldPoint( runtime.upperArmId, b3Sub( hp.upperBallJoint, upperHingeMid ) );
			b3Pos lowerBall = b3Body_GetWorldPoint( runtime.lowerArmId, b3Sub( hp.lowerBallJoint, lowerHingeMid ) );
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

			if ( B3_IS_NON_NULL( m_vehicle.rackId ) && ( corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT ) )
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

	// Prints exact world geometry for every wishbone corner once the car has
	// settled, so symmetry (FL vs FR should be a clean Z-mirror) and tire reach
	// (ball-joint Z vs the tire Z-band) can be judged from numbers, not pixels.
void JozzVehicleM6RigLab::DumpCornerGeometry()
	{
		const char* names[JOZZ_M6_CORNER_COUNT] = { "FL", "FR", "RL", "RR" };
		b3WorldTransform chassisLive = b3Body_GetTransform( m_vehicle.chassisId );
		float halfW = 0.5f * m_config.wheelEnvelope.width;
		b3Pos chassisP = b3Body_GetPosition( m_vehicle.chassisId );
		printf( "[DUMP] hertz=%.2f chassisY=%.3f tire R=%.3f W=%.3f trackHalf=%.3f\n", m_config.suspensionHertz,
				chassisP.y, m_config.wheelEnvelope.radius, m_config.wheelEnvelope.width, m_config.trackHalfWidth );
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			const JozzVehicleM6CornerRuntime& runtime = m_vehicle.corners[corner];
			if ( runtime.rigType != JOZZ_M6_RIG_DOUBLE_WISHBONE || B3_IS_NULL( runtime.knuckleId ) )
			{
				continue;
			}
			const JozzVehicleM6WishboneHardpoints& hp = runtime.hardpoints;
			b3Vec3 wl = runtime.restWheelCenterLocal;
			b3WorldTransform knuckleLive = b3Body_GetTransform( runtime.knuckleId );
			b3Pos wheelC = b3Body_GetPosition( runtime.wheelId );
			b3Pos upBall = b3TransformPoint( knuckleLive, b3Sub( hp.upperBallJoint, wl ) );
			b3Pos loBall = b3TransformPoint( knuckleLive, b3Sub( hp.lowerBallJoint, wl ) );
			b3Pos upChM =
				b3TransformPoint( chassisLive, b3MulSV( 0.5f, b3Add( hp.upperFrontChassis, hp.upperRearChassis ) ) );
			b3Pos loChM =
				b3TransformPoint( chassisLive, b3MulSV( 0.5f, b3Add( hp.lowerFrontChassis, hp.lowerRearChassis ) ) );
			printf( "[DUMP] %s isLeft=%d wheelC=(% .3f,% .3f,% .3f)\n", names[corner], CornerIsLeft( corner ) ? 1 : 0,
					wheelC.x, wheelC.y, wheelC.z );
			printf( "   upBall=(% .3f,% .3f,% .3f) loBall=(% .3f,% .3f,% .3f)\n", upBall.x, upBall.y, upBall.z,
					loBall.x, loBall.y, loBall.z );
			printf( "   upChM =(% .3f,% .3f,% .3f) loChM =(% .3f,% .3f,% .3f)\n", upChM.x, upChM.y, upChM.z, loChM.x,
					loChM.y, loChM.z );
			printf( "   tireZband=[% .3f,% .3f] upBallZ=% .3f loBallZ=% .3f (inboard=|z|<|%.3f|)\n", wheelC.z - halfW,
					wheelC.z + halfW, upBall.z, loBall.z, wheelC.z );
		}
		fflush( stdout );
	}

