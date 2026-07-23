// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

struct Sample;
struct SampleContext;

// P2B Scan Drive: the first sample that turns the private P2A scan preview pack
// into a STATIC collision mesh and drives the current M6 car on it. Distinct
// from the render-only P2A preview lab (which is physics-excluded); this one
// reads the same pack bytes through the pure jozz_scan_pack_geometry reader and
// creates real b3CreateMesh / b3CreateMeshShape ground.
Sample* CreateJozzVehicleScanDriveLab( SampleContext* context );
