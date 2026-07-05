// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"
#include "gfx/geometry_registry.h"
#include "gfx/utility.h"

#include <string>

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
