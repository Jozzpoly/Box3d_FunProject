// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"
#include "gfx/geometry_registry.h"
#include "gfx/utility.h"

#include <string>
#include <vector>

struct JozzVehicleVisualMesh
{
	MeshHandle handle = InvalidMeshHandle();
	std::string status;
	std::string textureStatus;
	int vertexCount = 0;
	int triangleCount = 0;
	bool textureLoaded = false;
	int textureWidth = 0;
	int textureHeight = 0;
	float textureAlphaCutoff = 0.0f;
	b3Vec3 boundsMin = b3Vec3_zero;
	b3Vec3 boundsMax = b3Vec3_zero;

	bool LoadStaticGltf( const char* path, float metersPerBlockbenchUnit );
	void Destroy();
	bool IsLoaded() const;
	void Draw( b3Pos origin, Vec4 color ) const;
	void DrawAtTransform( b3WorldTransform worldTransform, Vec4 color ) const;
};

// One rigidly-skinned part of a multi-bone model: the triangles whose vertices
// bind to a single bone. Geometry is baked to authored-world-meters (same space
// as JozzVehicleVisualMesh), so DrawPart(transform) applies transform to the
// authored rest positions exactly like the whole-mesh path.
struct JozzVehicleRiggedPart
{
	MeshHandle handle = InvalidMeshHandle();
	std::string boneName;
	int boneNodeIndex = -1;
	int vertexCount = 0;
	int triangleCount = 0;
	b3Vec3 restCenter = b3Vec3_zero;   // authored-world-meters bbox centre of this part at rest
	b3Vec3 boneRestWorld = b3Vec3_zero; // authored-world-meters position of this part's bone at rest
	b3Vec3 restMin = b3Vec3_zero;	   // authored-world-meters bbox min/max at rest
	b3Vec3 restMax = b3Vec3_zero;
};

// A model split into rigid per-bone parts. Requires the glTF to be rigidly
// skinned (each vertex weighted 1.0 to one bone), which the audit confirms for
// One_Sided_wheel_mount. Each part can then be driven by its own physics body:
// the hub part follows the wheel/upright while the bracket parts stay with the
// chassis. This is what actually "rigs" the model, as opposed to drawing the
// whole mesh rigidly on one body.
struct JozzVehicleRiggedMesh
{
	std::vector<JozzVehicleRiggedPart> parts;
	std::string status;
	bool textureLoaded = false;
	int textureWidth = 0;
	int textureHeight = 0;
	float textureAlphaCutoff = 0.0f;
	b3Vec3 boundsMin = b3Vec3_zero;
	b3Vec3 boundsMax = b3Vec3_zero;

	// mirrorX builds the opposite-hand copy: authored X (the arm axis) and the
	// normals' X are negated and triangle winding is reversed, so the same
	// yaw -90 placement that serves a left corner produces the correct mirror
	// image for a right corner (a reflection across the car centreline).
	bool LoadSkinnedGltf( const char* path, float metersPerBlockbenchUnit, bool mirrorX = false );
	void Destroy();
	bool IsLoaded() const;
	int PartCount() const;
	// Case-sensitive substring match on the bone name; -1 if none.
	int FindPart( const char* boneNameSubstring ) const;
	void DrawPart( int index, b3WorldTransform worldTransform, Vec4 color ) const;

	// Draws one part rotated by `rotation` and non-uniformly scaled by `scale`,
	// with the authored point `pivotAuthored` landing on `targetWorld`. The scale
	// is around that pivot (so a part can stretch without drifting). Used for the
	// telescoping damper's stretch section.
	void DrawPartScaled( int index, b3Quat rotation, b3Vec3 scale, b3Vec3 pivotAuthored, b3Pos targetWorld, Vec4 color ) const;

	// Pins a part's authored endpoints A and B onto two live world points: the
	// part rotates so its A->B axis aligns with liveA->liveB and stretches along
	// that axis to span the gap. Used for a wishbone arm (chassis mount -> ball
	// joint): the arm pivots and its ends stay glued to both bodies.
	void DrawPartBetween( int index, b3Vec3 authoredA, b3Vec3 authoredB, b3Pos liveA, b3Pos liveB, Vec4 color ) const;

	// A point that is a child of some other bone but is not itself one of the
	// mesh's own parts (e.g. a socket bolted to a wishbone arm) can still be
	// carried along the arm's live placement via JozzVehicleComputeArmPlacement
	// + JozzVehicleMapAuthoredPoint below, without a dedicated physics body.

	// Draws an Asset_Dumper-style vertical damper stretched between two live
	// mount points: Part_Upper pinned rigidly at topWorld, Part_Lower at
	// botWorld, Part_Stretch scaled along the damper axis to bridge them. Part
	// names are matched by substring ("Upper"/"Lower"/"Stretch").
	void DrawTelescopingDamper( b3Pos topWorld, b3Pos botWorld, Vec4 color ) const;
};

// The rigid+non-uniform-scale placement that pins authored point A onto liveA
// and authored point B onto liveB, preserving world up/forward as much as
// possible (see DrawPartBetween's comment for why a minimal rotation is not
// used). `valid` is false when authoredA/authoredB coincide (degenerate arm
// axis) - callers should skip drawing/mapping in that case, same as
// DrawPartBetween does internally.
struct JozzVehicleArmPlacement
{
	bool valid = false;
	b3Quat rotation = b3Quat_identity;
	b3Vec3 scale = b3Vec3_one;
	b3Vec3 pivotAuthored = b3Vec3_zero;
	b3Pos pivotWorld = b3Vec3_zero;
};

JozzVehicleArmPlacement JozzVehicleComputeArmPlacement( b3Vec3 authoredA, b3Vec3 authoredB, b3Pos liveA, b3Pos liveB );

// Maps an arbitrary authored-world-meters point through a placement computed
// by JozzVehicleComputeArmPlacement - for a socket that hangs off the arm bone
// itself but is not one of the two pinned endpoints (e.g. a damper eye bolted
// to the lower wishbone arm in a rig with no dedicated physics body for that
// arm, such as an isolated bench).
b3Pos JozzVehicleMapAuthoredPoint( const JozzVehicleArmPlacement& placement, b3Vec3 authoredPoint );

struct JozzVehicleAuditMetadata;

// Render-only correction for the current Offroad_Big_Wheels.gltf proof. The
// authored wheel spin/width axis is +X while the primitive wheel body uses
// local +Y as its axle; this maps authored +X onto body +Y and centers the
// authored wheel center on the body origin. Centers against loaded mesh bounds
// when available, with audited semantic points as fallback. Shared by the
// corner lab and the M5 drivable sample; it stays visual-only and must never
// feed physics frames.
b3Transform ComputeJozzVehicleWheelVisualCorrection( const JozzVehicleVisualMesh& mesh,
													 const JozzVehicleAuditMetadata& metadata,
													 float metersPerBlockbenchUnit );
