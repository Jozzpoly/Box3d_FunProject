// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// Shared helpers for the jozz_vehicle_validation probes (validation/jozz_probes_*.cpp):
// pass/fail printing, the M6 state/heading readouts, and the M6 smoke-test
// ground body every M6 probe spawns. Moved out of jozz_vehicle_validation.cpp
// verbatim (R1, move-only split) so several probe translation units can share
// them without redefining them per file.

#pragma once

#include "jozz_vehicle_m6_suspension_rig.h"

#include "box3d/box3d.h"

bool CheckTrue( const char* label, bool condition );
bool CheckApprox( const char* label, float actual, float expected, float tolerance );

bool IsM6VehicleStateValid( const JozzVehicleM6& vehicle );
float M6ChassisUpDotWorldUp( const JozzVehicleM6& vehicle );
float M6ChassisHeading( const JozzVehicleM6& vehicle );

b3BodyId CreateM6SmokeGround( b3WorldId worldId, float friction );
