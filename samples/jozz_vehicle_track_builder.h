// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/box3d.h"

// E3.2 neutral base only. This builds the drivable road slabs from the same
// centerline contract that the skeleton renders. It deliberately does not add
// curbs, barriers, obstacles, props or lap sensors.
void BuildJozzTrackBase( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits );

// E3.3 first profile slice only: low bumper rhythms and one complete,
// low-rise articulation. No ramps, jumps, curbs, barriers or decorations.
void BuildJozzTrackProfiles( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits );
