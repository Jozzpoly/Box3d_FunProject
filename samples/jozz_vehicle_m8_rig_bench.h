// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "sample.h"

// M8 Suspension Rig Bench: an isolated single-corner visual bench for getting
// the One_Sided_wheel_mount model correctly rigged BEFORE it goes into a
// moving four-corner vehicle. No driving physics - the wheel height is a
// slider so the whole travel range can be inspected by eye. This is the
// render-is-the-gate environment the M7 pass was missing.
Sample* CreateJozzVehicleM8RigBench( SampleContext* context );
