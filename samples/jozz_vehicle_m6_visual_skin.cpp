// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_visual_skin.h"

#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_body_registry.h"
#include "jozz_vehicle_m6_suspension_rig.h"

namespace
{

// The wheel model the M6 workshop loads. One asset, drawn four times.
constexpr const char* kWheelAsset = "assets/source/Offroad_Big_Wheels.gltf";
constexpr const char* kMountAsset = "assets/source/One_Sided_wheel_mount.gltf";
constexpr const char* kDamperAsset = "assets/source/Asset_Dumper.gltf";
constexpr const char* kMountContract = "one_sided_wheel_mount.asset.json";
constexpr const char* kDefaultBodyKey = "rama_rurowa";

const Vec4 kSkinColor = { 1.0f, 1.0f, 1.0f, 1.0f };
const Vec4 kDamperColor = { 0.82f, 0.84f, 0.9f, 1.0f };

bool CornerIsLeft( int corner )
{
	return corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_REAR_LEFT;
}

// Chassis-end and wheel-end of a wishbone-arm part along its authored X axis.
// The wheel end is the X extreme nearer the wheel centre: authored -X for a
// left (unmirrored) mesh, +X for a right (mirrored) mesh.
void ArmEnds( const JozzVehicleRiggedPart& part, bool wheelNegX, b3Vec3& chassisEnd, b3Vec3& wheelEnd )
{
	float y = 0.5f * ( part.restMin.y + part.restMax.y );
	float z = 0.5f * ( part.restMin.z + part.restMax.z );
	chassisEnd = { wheelNegX ? part.restMax.x : part.restMin.x, y, z };
	wheelEnd = { wheelNegX ? part.restMin.x : part.restMax.x, y, z };
}

} // namespace

bool JozzVehicleM6VisualSkin::Load( const char* bodyModelKey, float unitScale, const JozzVehicleAuditMetadata& metadata )
{
	Destroy();

	if ( unitScale <= 0.0f )
	{
		status = "skin: brak skali modelu (metersPerBlockbenchUnit)";
		return false;
	}

	// AttachToVehicle scales the wheel's mount socket by this, so it MUST be
	// stored, not just used locally - at 0 the suspension pins to the wheel
	// centre instead of the inboard hub face and the whole assembly sits
	// offset along the axle.
	metersPerBlockbenchUnit = unitScale;

	// Body: pose straight from the curated registry row, so the frame lands on
	// the wheelbase/track the same way it does in the workshop.
	const JozzVehicleBodyModelDef* def = nullptr;
	if ( bodyModelKey != nullptr && bodyModelKey[0] != '\0' )
	{
		def = FindJozzVehicleBodyModelByKey( bodyModelKey );
	}
	if ( def == nullptr )
	{
		def = FindJozzVehicleBodyModelByKey( kDefaultBodyKey );
	}
	if ( def != nullptr && def->assetPath != nullptr )
	{
		std::string bodyPath;
		if ( FindJozzVehicleAssetFile( def->assetPath, &bodyPath ) )
		{
			body.LoadStaticGltf( bodyPath.c_str(), metersPerBlockbenchUnit );
		}
		bodyChassisLocal.q = b3MakeQuatFromAxisAngle( b3Vec3_axisY, def->baseYawDeg * B3_PI / 180.0f );
		bodyChassisLocal.p = def->basePos;
	}

	// Wheel: the authored spin axis is +X while the wheel body's axle is local
	// +Y, so the correction transform is mandatory, not cosmetic.
	std::string wheelPath;
	if ( FindJozzVehicleAssetFile( kWheelAsset, &wheelPath ) )
	{
		wheel.LoadStaticGltf( wheelPath.c_str(), metersPerBlockbenchUnit );
	}
	wheelCorrection = ComputeJozzVehicleWheelVisualCorrection( wheel, metadata, metersPerBlockbenchUnit );

	// Suspension: rigidly-skinned, so each bone becomes its own drawable part
	// that can ride a different physics body. The right side is a mirrored copy
	// of the same one-sided model.
	mountContract = LoadJozzVehicleAssetContract( kMountContract );
	float mountUnit =
		mountContract.metersPerBlockbenchUnit > 0.0f ? mountContract.metersPerBlockbenchUnit : metersPerBlockbenchUnit;
	std::string mountPath = mountContract.sourcePath;
	if ( mountPath.empty() )
	{
		FindJozzVehicleAssetFile( kMountAsset, &mountPath );
	}
	if ( mountPath.empty() == false )
	{
		mountLeft.LoadSkinnedGltf( mountPath.c_str(), mountUnit, false );
		mountRight.LoadSkinnedGltf( mountPath.c_str(), mountUnit, true );
	}

	std::string damperPath;
	if ( FindJozzVehicleAssetFile( kDamperAsset, &damperPath ) )
	{
		damper.LoadSkinnedGltf( damperPath.c_str(), metersPerBlockbenchUnit );
	}

	// Attachment sockets from the contract, falling back to the shipped model's
	// measured values if it fails to load.
	auto socket = [this]( const char* role, b3Vec3 fallback ) {
		const JozzVehicleContractBinding* binding = FindJozzVehicleContractBindingByRole( mountContract, role );
		return ( binding != nullptr && binding->resolved ) ? binding->positionMeters : fallback;
	};
	mountWheelCenterAuthored = socket( "suspension.visual.wheel_center", mountWheelCenterAuthored );
	damperUpperLAuthored = socket( "suspension.visual.damper_upper_l", damperUpperLAuthored );
	damperUpperRAuthored = socket( "suspension.visual.damper_upper_r", damperUpperRAuthored );
	damperLowerLAuthored = socket( "suspension.visual.damper_lower_l", damperLowerLAuthored );
	damperLowerRAuthored = socket( "suspension.visual.damper_lower_r", damperLowerRAuthored );

	if ( IsLoaded() == false )
	{
		status = "skin: nie znaleziono modeli w assets/source (rysuje bryly debugowe)";
		return false;
	}

	char message[192];
	std::snprintf( message, sizeof( message ), "skin: nadwozie %s, kola %s, zawieszenie %s, amortyzatory %s",
				   body.IsLoaded() ? "OK" : "BRAK", wheel.IsLoaded() ? "OK" : "BRAK",
				   mountLeft.IsLoaded() ? "OK" : "BRAK", damper.IsLoaded() ? "OK" : "BRAK" );
	status = message;
	return true;
}

// Bake the corner assembly placement while the car is at rest: the model's own
// wheel-mount socket is pinned to the wheel's inboard hub face, then that rest
// pose is expressed relative to BOTH the chassis (brackets/arm roots) and the
// knuckle (hub). Live, each part follows its body, so the assembly articulates
// with the physics instead of being redrawn from formulas.
void JozzVehicleM6VisualSkin::AttachToVehicle( const JozzVehicleM6& vehicle, const JozzVehicleAuditMetadata& metadata )
{
	attached = false;
	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		cornerHasMount[corner] = false;
	}
	if ( vehicle.valid == false || mountLeft.IsLoaded() == false )
	{
		return;
	}

	b3Vec3 mountBU = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
	b3Quat yaw = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
	b3WorldTransform chassisRest = b3Body_GetTransform( vehicle.chassisId );

	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
		cornerHasMount[corner] = runtime.rigType == JOZZ_M6_RIG_DOUBLE_WISHBONE && B3_IS_NON_NULL( runtime.knuckleId );
		if ( cornerHasMount[corner] == false )
		{
			continue;
		}

		bool isLeft = CornerIsLeft( corner );

		// The offset from wheel centre to the inboard hub face runs along the
		// axle; reflect it on the right so the assembly mounts inboard on both.
		b3WorldTransform wheelDraw = b3MulWorldTransforms( b3Body_GetTransform( runtime.wheelId ), wheelCorrection );
		b3Pos wheelCentre = b3Body_GetPosition( runtime.wheelId );
		b3Pos mountWorld = b3TransformWorldPoint( wheelDraw, b3MulSV( metersPerBlockbenchUnit, mountBU ) );
		b3Vec3 offset = b3Sub( mountWorld, wheelCentre );
		if ( isLeft == false )
		{
			offset.z = -offset.z;
		}
		b3Pos attach = b3OffsetPos( wheelCentre, offset );

		b3Vec3 pwc = mountWheelCenterAuthored;
		if ( isLeft == false )
		{
			pwc.x = -pwc.x;
		}
		b3Vec3 rp = b3RotateVector( yaw, pwc );

		b3WorldTransform placementRest;
		placementRest.q = yaw;
		placementRest.p = { attach.x - rp.x, attach.y - rp.y, attach.z - rp.z };

		bracketLocal[corner] = b3InvMulWorldTransforms( chassisRest, placementRest );
		hubLocal[corner] = b3InvMulWorldTransforms( b3Body_GetTransform( runtime.knuckleId ), placementRest );
		attached = true;
	}
}

void JozzVehicleM6VisualSkin::Destroy()
{
	body.Destroy();
	wheel.Destroy();
	mountLeft.Destroy();
	mountRight.Destroy();
	damper.Destroy();
	wheelCorrection = b3Transform_identity;
	bodyChassisLocal = b3WorldTransform_identity;
	attached = false;
	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		cornerHasMount[corner] = false;
	}
	status.clear();
}

bool JozzVehicleM6VisualSkin::IsLoaded() const
{
	return body.IsLoaded() || wheel.IsLoaded() || mountLeft.IsLoaded();
}

void JozzVehicleM6VisualSkin::Draw( const JozzVehicleM6& vehicle ) const
{
	if ( vehicle.valid == false )
	{
		return;
	}

	if ( wheel.IsLoaded() )
	{
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			b3WorldTransform wheelTransform = b3Body_GetTransform( vehicle.corners[corner].wheelId );
			wheel.DrawAtTransform( b3MulWorldTransforms( wheelTransform, wheelCorrection ), kSkinColor );
		}
	}

	// Suspension: each part on the live body its kinematic role demands. The
	// arms are pinned BETWEEN their chassis bracket and the hub - the very parts
	// they join in the source model - so they flex as the corner travels
	// instead of being drawn rigidly and tearing away from the assembly.
	if ( attached )
	{
		b3WorldTransform chassisLive = b3Body_GetTransform( vehicle.chassisId );
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			if ( cornerHasMount[corner] == false )
			{
				continue;
			}
			bool isLeft = CornerIsLeft( corner );
			const JozzVehicleRiggedMesh& mesh = isLeft ? mountLeft : mountRight;
			if ( mesh.IsLoaded() == false )
			{
				continue;
			}

			const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
			b3WorldTransform bracketWorld = b3MulWorldTransforms( chassisLive, bracketLocal[corner] );
			b3WorldTransform hubWorld = b3MulWorldTransforms( b3Body_GetTransform( runtime.knuckleId ), hubLocal[corner] );

			for ( int i = 0; i < mesh.PartCount(); ++i )
			{
				const std::string& name = mesh.parts[i].boneName;
				bool isArm = name.find( "Chassis_Top" ) != std::string::npos ||
							 name.find( "Chassis_Bottom" ) != std::string::npos;
				if ( isArm )
				{
					b3Vec3 chassisEnd, wheelEnd;
					ArmEnds( mesh.parts[i], isLeft, chassisEnd, wheelEnd );
					mesh.DrawPartBetween( i, chassisEnd, wheelEnd, b3TransformPoint( bracketWorld, chassisEnd ),
										  b3TransformPoint( hubWorld, wheelEnd ), kSkinColor );
				}
				else if ( name.find( "WheelCenter" ) != std::string::npos )
				{
					mesh.DrawPart( i, hubWorld, kSkinColor );
				}
				else
				{
					mesh.DrawPart( i, bracketWorld, kSkinColor );
				}
			}

			// Two coilovers per corner, straddling the arm: upper eye rides the
			// chassis bracket, lower eye the knuckle hub - the same transforms
			// that pin the arms, so they stay glued to the same live geometry.
			if ( damper.IsLoaded() )
			{
				b3Vec3 upperL = damperUpperLAuthored;
				b3Vec3 upperR = damperUpperRAuthored;
				b3Vec3 lowerL = damperLowerLAuthored;
				b3Vec3 lowerR = damperLowerRAuthored;
				if ( isLeft == false )
				{
					upperL.x = -upperL.x;
					upperR.x = -upperR.x;
					lowerL.x = -lowerL.x;
					lowerR.x = -lowerR.x;
				}
				damper.DrawTelescopingDamper( b3TransformPoint( bracketWorld, upperL ),
											  b3TransformPoint( hubWorld, lowerL ), kDamperColor );
				damper.DrawTelescopingDamper( b3TransformPoint( bracketWorld, upperR ),
											  b3TransformPoint( hubWorld, lowerR ), kDamperColor );
			}
		}
	}

	if ( body.IsLoaded() )
	{
		// The frame is one rigid piece bolted to the chassis; the live offset
		// from the config is applied here rather than baked at load, so the
		// workshop's offset sliders keep meaning the same thing everywhere.
		b3WorldTransform local = bodyChassisLocal;
		local.p = b3Add( local.p, vehicle.config.bodyVisualOffset );
		body.DrawAtTransform( b3MulWorldTransforms( b3Body_GetTransform( vehicle.chassisId ), local ), kSkinColor );
	}
}
