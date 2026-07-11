// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_visual_mesh.h"

#include "box3d/base.h"
#include "gfx/draw.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_image_decode.h"
#include "jozz_vehicle_json.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <vector>

using namespace jozz;

void JozzVehicleVisualMesh::Draw( b3Pos origin, Vec4 color ) const
{
	DrawAtTransform( { origin, b3Quat_identity }, color );
}

void JozzVehicleVisualMesh::DrawAtTransform( b3WorldTransform worldTransform, Vec4 color ) const
{
	if ( IsLoaded() == false )
	{
		return;
	}

	b3Transform relativeTransform = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	AppendMesh( handle, relativeTransform, b3Vec3_one, color, 0.0f, 0.58f,
				textureLoaded ? MESH_MATERIAL_MODE_TEXTURED : MESH_MATERIAL_MODE_SOLID, textureAlphaCutoff,
				TRANSPARENT_SHADOW_FULL );
}

int JozzVehicleRiggedMesh::FindPart( const char* boneNameSubstring ) const
{
	for ( int i = 0; i < (int)parts.size(); ++i )
	{
		if ( parts[i].boneName.find( boneNameSubstring ) != std::string::npos )
		{
			return i;
		}
	}
	return -1;
}

void JozzVehicleRiggedMesh::DrawPart( int index, b3WorldTransform worldTransform, Vec4 color ) const
{
	if ( index < 0 || index >= (int)parts.size() || IsMeshHandleValid( parts[index].handle ) == false )
	{
		return;
	}
	b3Transform relativeTransform = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	AppendMesh( parts[index].handle, relativeTransform, b3Vec3_one, color, 0.0f, 0.58f,
				textureLoaded ? MESH_MATERIAL_MODE_TEXTURED : MESH_MATERIAL_MODE_SOLID, textureAlphaCutoff,
				TRANSPARENT_SHADOW_FULL );
}

void JozzVehicleRiggedMesh::DrawPartScaled( int index, b3Quat rotation, b3Vec3 scale, b3Vec3 pivotAuthored,
										   b3Pos targetWorld, Vec4 color ) const
{
	if ( index < 0 || index >= (int)parts.size() || IsMeshHandleValid( parts[index].handle ) == false )
	{
		return;
	}
	// worldVert = rotation * (scale . vert) + T. Choose T so the authored pivot
	// lands on targetWorld: T = targetWorld - rotation * (scale . pivot).
	b3Vec3 scaledPivot = { scale.x * pivotAuthored.x, scale.y * pivotAuthored.y, scale.z * pivotAuthored.z };
	b3Vec3 rotatedPivot = b3RotateVector( rotation, scaledPivot );
	b3WorldTransform worldTransform;
	worldTransform.q = rotation;
	worldTransform.p = { targetWorld.x - rotatedPivot.x, targetWorld.y - rotatedPivot.y, targetWorld.z - rotatedPivot.z };

	b3Transform relativeTransform = b3ToRelativeTransform( worldTransform, GetDrawOrigin() );
	AppendMesh( parts[index].handle, relativeTransform, scale, color, 0.0f, 0.58f,
				textureLoaded ? MESH_MATERIAL_MODE_TEXTURED : MESH_MATERIAL_MODE_SOLID, textureAlphaCutoff,
				TRANSPARENT_SHADOW_FULL );
}

JozzVehicleArmPlacement JozzVehicleComputeArmPlacement( b3Vec3 authoredA, b3Vec3 authoredB, b3Pos liveA, b3Pos liveB )
{
	JozzVehicleArmPlacement placement;
	placement.pivotAuthored = authoredA;
	placement.pivotWorld = liveA;

	b3Vec3 authoredDir = b3Sub( authoredB, authoredA );
	float authoredLen = b3Length( authoredDir );
	if ( authoredLen < 1.0e-5f )
	{
		return placement;
	}
	b3Vec3 ua = b3MulSV( 1.0f / authoredLen, authoredDir );
	b3Vec3 liveDir = b3Sub( liveB, liveA );
	float liveLen = b3Length( liveDir );
	b3Vec3 ul = liveLen > 1.0e-5f ? b3MulSV( 1.0f / liveLen, liveDir ) : ua;

	// A minimal rotation (b3ComputeQuatBetweenUnitVectors) leaves the roll about
	// the arm axis free, and it resolves differently for a left vs a mirrored
	// right corner - so the two arms twist unequally and the rig looks crooked.
	// Build the full orientation instead: keep the part's face up and its width
	// along the car's fore/aft, pinning the roll. Because it is derived the same
	// way from ua/ul on both sides, mirrored inputs give a mirrored result.
	const b3Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
	const b3Vec3 worldFwd = { 1.0f, 0.0f, 0.0f };
	// Authored frame: ua (long), upA (authored up made perpendicular to ua), wA.
	b3Vec3 upA = b3Sub( worldUp, b3MulSV( b3Dot( worldUp, ua ), ua ) );
	upA = b3Length( upA ) > 1.0e-4f ? b3Normalize( upA ) : b3Sub( worldFwd, b3MulSV( b3Dot( worldFwd, ua ), ua ) );
	upA = b3Normalize( upA );
	b3Vec3 wA = b3Cross( ua, upA );
	// Target frame: ul (long), upT (world up made perpendicular to ul), wT.
	b3Vec3 upT = b3Sub( worldUp, b3MulSV( b3Dot( worldUp, ul ), ul ) );
	upT = b3Length( upT ) > 1.0e-4f ? b3Normalize( upT ) : b3Sub( worldFwd, b3MulSV( b3Dot( worldFwd, ul ), ul ) );
	upT = b3Normalize( upT );
	b3Vec3 wT = b3Cross( ul, upT );
	// R maps the authored frame (ua, upA, wA) onto the target frame (ul, upT, wT):
	// column j = ul*ua[j] + upT*upA[j] + wT*wA[j].
	b3Matrix3 r;
	r.cx = b3Add( b3Add( b3MulSV( ua.x, ul ), b3MulSV( upA.x, upT ) ), b3MulSV( wA.x, wT ) );
	r.cy = b3Add( b3Add( b3MulSV( ua.y, ul ), b3MulSV( upA.y, upT ) ), b3MulSV( wA.y, wT ) );
	r.cz = b3Add( b3Add( b3MulSV( ua.z, ul ), b3MulSV( upA.z, upT ) ), b3MulSV( wA.z, wT ) );
	placement.rotation = b3MakeQuatFromMatrix( &r );

	// Stretch along whichever authored axis the endpoints run along (arms are on
	// authored X, the damper on Y), so the part exactly spans liveA..liveB.
	float s = liveLen / authoredLen;
	placement.scale = { 1.0f + ( s - 1.0f ) * std::fabs( ua.x ), 1.0f + ( s - 1.0f ) * std::fabs( ua.y ),
						 1.0f + ( s - 1.0f ) * std::fabs( ua.z ) };
	placement.valid = true;
	return placement;
}

b3Pos JozzVehicleMapAuthoredPoint( const JozzVehicleArmPlacement& placement, b3Vec3 authoredPoint )
{
	if ( placement.valid == false )
	{
		return placement.pivotWorld;
	}
	b3Vec3 local = b3Sub( authoredPoint, placement.pivotAuthored );
	b3Vec3 scaled = { placement.scale.x * local.x, placement.scale.y * local.y, placement.scale.z * local.z };
	b3Vec3 rotated = b3RotateVector( placement.rotation, scaled );
	return b3Add( placement.pivotWorld, rotated );
}

void JozzVehicleRiggedMesh::DrawPartBetween( int index, b3Vec3 authoredA, b3Vec3 authoredB, b3Pos liveA, b3Pos liveB,
											Vec4 color ) const
{
	if ( index < 0 || index >= (int)parts.size() || IsMeshHandleValid( parts[index].handle ) == false )
	{
		return;
	}
	JozzVehicleArmPlacement placement = JozzVehicleComputeArmPlacement( authoredA, authoredB, liveA, liveB );
	if ( placement.valid == false )
	{
		return;
	}
	DrawPartScaled( index, placement.rotation, placement.scale, authoredA, liveA, color );
}

void JozzVehicleRiggedMesh::DrawTelescopingDamper( b3Pos topWorld, b3Pos botWorld, Vec4 color ) const
{
	int upper = FindPart( "Upper" );
	int stretch = FindPart( "Stretch" );
	int lower = FindPart( "Lower" );
	if ( upper < 0 || lower < 0 )
	{
		return;
	}

	b3Vec3 restSpan = b3Sub( parts[upper].boneRestWorld, parts[lower].boneRestWorld );
	float restGap = b3Length( restSpan );
	b3Vec3 authoredAxis = restGap > 1.0e-5f ? b3MulSV( 1.0f / restGap, restSpan ) : b3Vec3_axisY;

	b3Vec3 liveSpan = b3Sub( topWorld, botWorld );
	float liveGap = b3Length( liveSpan );
	b3Vec3 liveAxis = liveGap > 1.0e-5f ? b3MulSV( 1.0f / liveGap, liveSpan ) : b3Vec3_axisY;

	b3Quat rotation = b3ComputeQuatBetweenUnitVectors( authoredAxis, liveAxis );

	// Rigid tubes pinned to the two mount points.
	DrawPartScaled( upper, rotation, b3Vec3_one, parts[upper].boneRestWorld, topWorld, color );
	DrawPartScaled( lower, rotation, b3Vec3_one, parts[lower].boneRestWorld, botWorld, color );

	// Stretch section: scaled along the damper axis, placed at the same fraction
	// of the segment it occupies at rest so it stays exact when uncompressed.
	if ( stretch >= 0 && restGap > 1.0e-4f )
	{
		float f = b3ClampFloat( b3Dot( b3Sub( parts[upper].boneRestWorld, parts[stretch].boneRestWorld ), authoredAxis ) / restGap,
								0.0f, 1.0f );
		b3Vec3 target = b3Add( topWorld, b3MulSV( f, b3Sub( botWorld, topWorld ) ) );
		float scaleY = liveGap / restGap;
		DrawPartScaled( stretch, rotation, { 1.0f, scaleY, 1.0f }, parts[stretch].boneRestWorld, target, color );
	}
}

b3Transform ComputeJozzVehicleWheelVisualCorrection( const JozzVehicleVisualMesh& mesh,
													 const JozzVehicleAuditMetadata& metadata,
													 float metersPerBlockbenchUnit )
{
	b3Vec3 visualCenter;
	if ( mesh.IsLoaded() )
	{
		visualCenter = {
			0.5f * ( mesh.boundsMin.x + mesh.boundsMax.x ),
			0.5f * ( mesh.boundsMin.y + mesh.boundsMax.y ),
			0.5f * ( mesh.boundsMin.z + mesh.boundsMax.z ),
		};
	}
	else
	{
		b3Vec3 wheelMountBU = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Socket_WheelMount" );
		b3Vec3 radiusOuterBU = JozzVehicleFindPointOrBuiltIn( metadata, "Offroad_Big_Wheels.gltf", "Marker_TireRadiusOuter" );
		b3Vec3 wheelCenterBU = { radiusOuterBU.x, wheelMountBU.y, wheelMountBU.z };
		visualCenter = b3MulSV( metersPerBlockbenchUnit, wheelCenterBU );
	}

	// The authored wheel spin/width axis is +X, while the primitive wheel body
	// uses local +Y as its axle. Map authored +X to body +Y and center the
	// authored wheel center on the primitive body origin.
	b3Quat visualXToBodyY = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 0.5f * B3_PI );
	b3Quat visualUpToBodyRadial = b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.5f * B3_PI );
	b3Quat correctionRotation = b3MulQuat( visualUpToBodyRadial, visualXToBodyY );
	return { b3Neg( b3RotateVector( correctionRotation, visualCenter ) ), correctionRotation };
}
