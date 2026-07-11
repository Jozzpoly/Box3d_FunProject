// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/math_functions.h"

// One row = one selectable body skin. Curated BY HAND on purpose - a folder
// scan would happily offer Cardan_shaft.gltf as a "body". Adding a body =
// adding a row (+ the asset under assets/source/). Lab-only (GUI target);
// the headless validator does not link this file - see plan risk R4/R7 in
// docs/PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md.
struct JozzVehicleBodyModelDef
{
	const char* key;	   // stable ID stored in configs/presets ([a-z0-9_])
	const char* label;	   // Polish UI label for the combo
	const char* assetPath; // repo-relative; nullptr = no mesh (the "brak" row)
	float baseYawDeg;	   // authored base rotation around chassis Y
	b3Vec3 basePos;		   // authored base position in the chassis body's local frame
};

const JozzVehicleBodyModelDef* GetJozzVehicleBodyModels( int* outCount );
// nullptr when the key is unknown - caller decides the fallback.
const JozzVehicleBodyModelDef* FindJozzVehicleBodyModelByKey( const char* key );
