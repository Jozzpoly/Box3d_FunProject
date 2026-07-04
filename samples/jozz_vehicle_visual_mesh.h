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
