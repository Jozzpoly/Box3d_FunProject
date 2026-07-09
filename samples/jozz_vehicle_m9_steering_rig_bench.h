// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "sample.h"

// M9 Steering Rig Bench: an isolated two-corner (left+right) visual bench for
// validating Jozz's NEW OneSided_Steering_Suspension_Rig model - a different
// glTF from One_Sided_wheel_mount (M8's model), with a materially different
// node layout (Socket_ChassisMount_b is the upright/knuckle, not a chassis
// bracket; the single damper's lower eye rides the lower wishbone arm; a new
// Socket_SteeringRod part exists). No driving physics - each corner has a
// real 2-DOF suspension (vertical travel + steer yaw) so the rig can be
// exercised and screenshotted before it goes anywhere near the M6 vehicle.
Sample* CreateJozzVehicleM9SteeringRigBench( SampleContext* context );
