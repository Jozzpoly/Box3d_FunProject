// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "jozz_vehicle_m6_suspension_rig.h"

#include <string>

// The M6 setup tabs (Naped / Kierownica / Nadwozie), extracted from the
// suspension workshop so any sample that drives an M6 car gets the SAME editor
// instead of a hand-rolled subset that drifts from it. The workshop's tabs are
// methods on its own class and reach into its terrain, telemetry and preset
// machinery; these are free functions over nothing but a config plus the edit
// state below, which is what makes them portable.
//
// Two kinds of field, and the difference matters:
//   - LIVE: writes straight into the config; the caller re-applies tuning to
//     the running vehicle (or simply lets the next step pick it up).
//   - STRUCTURAL: goes into an edit buffer and sets `structuralDirty`, because
//     changing it means rebuilding the vehicle. Nothing is silently applied.

// Pending structural edits, mirroring the workshop's own m_edit* buffers.
struct JozzVehicleM6SetupEdit
{
	int frontRigType = 0;
	int rearRigType = 0;
	JozzVehicleM6WishboneGeometry wishbone = {};
	JozzVehicleM6TrailingArmGeometry trailingArm = {};
	float knuckleMass = 0.0f;
	float armMass = 0.0f;
	int envelopeMode = 0;
	int envelopeLayers = 0;
	float strutCasterDeg = 0.0f;
	float maxSteeringAngleDegrees = 0.0f;
	float frontToeDeg = 0.0f;
	float rearToeDeg = 0.0f;
	b3Vec3 chassisHalfExtents = b3Vec3_zero;
	float chassisDensity = 0.0f;
	float cgVerticalOffset = 0.0f;
	float axleHalfSpacing = 0.0f;
	float trackHalfWidth = 0.0f;
	float restDrop = 0.0f;
	float wheelDensity = 0.0f;

	bool structuralDirty = false;
	// Set when the body-skin combo picked a different model: purely visual, so
	// it needs a skin reload rather than a vehicle rebuild.
	bool bodyModelChanged = false;
	// Steering preference, not part of the config (the workshop keeps it as a
	// lab-level toggle too).
	bool invertSteering = false;
	std::string clampStatus;

	void SyncFromConfig( const JozzVehicleM6Config& config );
	// Pushes the buffers back and clears `structuralDirty`. The caller then
	// rebuilds the vehicle. Sanitize afterwards - the workshop's safety fence
	// for max-steer vs Ackermann lives in SanitizeJozzVehicleM6Config.
	void ApplyToConfig( JozzVehicleM6Config* config );
};

// Each returns true when a LIVE field changed this frame, so the caller can
// re-apply tuning to the running vehicle immediately.
bool DrawJozzVehicleM6DriveTab( JozzVehicleM6Config* config );
bool DrawJozzVehicleM6SteeringTab( JozzVehicleM6Config* config, JozzVehicleM6SetupEdit* edit );
bool DrawJozzVehicleM6ChassisTab( JozzVehicleM6Config* config, JozzVehicleM6SetupEdit* edit );
bool DrawJozzVehicleM6SuspensionTab( JozzVehicleM6Config* config, JozzVehicleM6SetupEdit* edit );

// Pushes the LIVE-tunable config onto a running vehicle's joints - the same
// work the workshop's ApplySuspensionTuning / ApplySteeringTuning /
// ApplyWheelFriction do. Call it when a tab reports a live change, so sliders
// bite immediately instead of waiting for a rebuild. No-op on an invalid car.
void ApplyJozzVehicleM6LiveTuning( const JozzVehicleM6& vehicle, const JozzVehicleM6Config& config );
