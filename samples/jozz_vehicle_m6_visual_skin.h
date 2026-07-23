// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_visual_mesh.h"

#include <string>

struct JozzVehicleM6;
struct JozzVehicleAuditMetadata;

// Puts Jozz's real 3D model on a live M6 car: the tube-frame body on the
// chassis, the offroad wheel on each wheel body, and the wishbone suspension
// rigged per bone onto the LIVE corner bodies - arms flexing between their
// chassis bracket and the knuckle, coilovers stretching as the corner travels.
// What you see moves exactly as the physics does; nothing here is a schematic.
//
// This is the same draw model the M6 suspension workshop uses, packaged so any
// sample gets it in three calls (Load -> AttachToVehicle -> Draw) instead of
// carrying the workshop's per-corner rig state itself. It is a separate,
// smaller module rather than a refactor of that 870-line lab, which stays
// untouched.
//
// Nothing is guessed: the body pose comes from the curated body registry
// (jozz_vehicle_body_registry.h), the wheel pose from
// ComputeJozzVehicleWheelVisualCorrection, and the mount/damper attachment
// points from the asset contract's own sockets. Visual only - it never feeds
// physics.
struct JozzVehicleM6VisualSkin
{
	JozzVehicleVisualMesh body;
	JozzVehicleVisualMesh wheel;
	JozzVehicleRiggedMesh mountLeft;  // authored (left corners)
	JozzVehicleRiggedMesh mountRight; // mirrored copy (right corners)
	JozzVehicleRiggedMesh damper;
	JozzVehicleAssetContract mountContract;

	b3Transform wheelCorrection = b3Transform_identity;
	b3WorldTransform bodyChassisLocal = b3WorldTransform_identity;
	float metersPerBlockbenchUnit = 0.0f;
	std::string status;

	// Authored (left-hand, unmirrored) sockets read from the contract; right
	// corners negate X only - the damper L/R pair straddles the arm and is not
	// the car's left/right side, so Z is left alone.
	b3Vec3 mountWheelCenterAuthored = { -0.416f, 0.175f, 0.0f };
	b3Vec3 damperUpperLAuthored = { 0.0164f, 0.6453f, 0.2844f };
	b3Vec3 damperUpperRAuthored = { 0.0164f, 0.6453f, -0.2844f };
	b3Vec3 damperLowerLAuthored = { -0.2516f, 0.0109f, 0.2844f };
	b3Vec3 damperLowerRAuthored = { -0.2516f, 0.0109f, -0.2844f };

	// Baked by AttachToVehicle: the corner assembly's placement relative to the
	// chassis (brackets, arm roots) and to the knuckle (hub).
	b3Transform bracketLocal[4] = {};
	b3Transform hubLocal[4] = {};
	bool cornerHasMount[4] = {};
	bool attached = false;

	// bodyModelKey is a registry key ("rama_rurowa", "brak", ...); an empty or
	// unknown key falls back to the default body rather than drawing nothing.
	// Returns true when at least one mesh loaded; `status` always explains.
	bool Load( const char* bodyModelKey, float unitScale, const JozzVehicleAuditMetadata& metadata );
	void Destroy();
	bool IsLoaded() const;

	// Bake each corner's model placement against the car AT REST - call once
	// right after CreateJozzVehicleM6, before anything spins or steers, and
	// again after any rebuild. Without it the suspension simply is not drawn.
	void AttachToVehicle( const JozzVehicleM6& vehicle, const JozzVehicleAuditMetadata& metadata );

	// Draws whatever loaded, at the vehicle's live pose. Safe to call with an
	// invalid vehicle or a failed load - it simply draws nothing.
	void Draw( const JozzVehicleM6& vehicle ) const;
};
