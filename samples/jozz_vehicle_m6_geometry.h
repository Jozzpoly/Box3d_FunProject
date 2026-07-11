// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M6 pure-geometry module (extracted R5, 2026-07-11).
//
// The WORLD-FREE half of the suspension foundation: hardpoint layout,
// rack-stroke / steering dead-point solving, the built-in default geometry,
// and the disk-load config sanitizer. None of these six functions touches
// b3World - they are closed-form math over the geometry/config structs, which
// is exactly what a future rig editor needs to compute hardpoints and the
// over-center dead point WITHOUT building a physics world.
//
// SCOPE NOTE (deliberate, see PLAN_WIELKI_REFACTOR_2026_07_09_PL.md R5):
// only these six already-public functions moved out of suspension_rig.cpp.
// The internal helpers HingeSwingLimit / SteeringLinkDroopLift /
// SteeringArmWithToe stay in suspension_rig.cpp: they are internal-linkage
// implementation details of the physics builders (SteeringArmWithToe even
// depends on the physics-side corner-classification helpers), not part of the
// world-free contract, so pulling them here would trade real linkage/cycle
// risk for no editor value.
//
// This header does NOT yet strip the physics API: it includes
// suspension_rig.h for the shared geometry structs (WishboneGeometry,
// Hardpoints, TrailingArmGeometry, Config), so it is the documented geometry
// SUBSET of that header, not a leaner standalone one. Making a truly
// world-independent header would mean moving those struct definitions - out of
// this move-only pass's scope and speculative-for-editor (plan anti-goal
// section 2.4). The six declarations below are also still present, unchanged,
// in suspension_rig.h, so every existing caller keeps compiling untouched.

#include "jozz_vehicle_m6_suspension_rig.h"

// wheelbase/track feed the Ackermann trapezoid angle; isLeft mirrors Z.
JozzVehicleM6WishboneHardpoints JozzVehicleM6MakeWishboneHardpoints( const JozzVehicleM6WishboneGeometry& geometry,
																	 b3Vec3 restWheelCenter, bool isLeft, float wheelbase,
																	 float track );

// Built-in fallback trailing-arm geometry (chassis-local offsets, LEFT corner).
JozzVehicleM6TrailingArmGeometry JozzVehicleM6DefaultTrailingArmGeometry();

// Exact rack stroke (m) that yaws the inner wheel to steerAngle (rad), closed
// form from the tie-rod/steering-arm linkage. See suspension_rig.h for the
// nonlinearity notes and why default rackTravel and the drive path share it.
float ComputeJozzVehicleM6RackStroke( const JozzVehicleM6WishboneGeometry& geometry, float wheelbase, float track,
									  float rackHalfWidth, float steerAngle );

// The over-center steering dead-point angle (deg) for the given geometry, found
// by walking the stroke curve until it stops climbing. Shared by the P1 twist
// fence, the validator, and the live max-steer clamp - all must agree.
float ComputeJozzVehicleM6SteeringDeadPointDeg( const JozzVehicleM6WishboneGeometry& geometry, float wheelbase,
												 float track, float rackHalfWidth );

// P6 defensive clamp for configs loaded from disk (hand-editable files). Wide
// bounds, one WARNING line per field; re-applies the P5 max-steer dead-point
// clamp. Returns true if anything changed. NOT called inside CreateJozzVehicleM6
// (the validator's probes need extreme configs); guards the file-load boundary.
bool SanitizeJozzVehicleM6Config( JozzVehicleM6Config* config );

// The validated M6 default config for a wheel of the given radius/width.
JozzVehicleM6Config JozzVehicleM6DefaultConfig( float wheelRadius, float wheelWidth, float suspensionTravelHint );
