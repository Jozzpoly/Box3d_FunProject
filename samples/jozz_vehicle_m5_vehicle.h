// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M5 First Drivable: a four-corner primitive vehicle built from the validated
// M2.4/M2.5 one-corner model. This module is the first piece of the "physics
// prefab" layer from PROJECT_DIRECTION: bodies + shapes + joints + tuning
// parameters, with no rendering or input dependencies, so the samples host and
// the headless validation CLI share the exact same vehicle.
//
// Physics rules carried over from the corner lab:
//   b3WheelJoint spring rest = translation 0
//   Frame A = rest wheel-center anchor on the chassis (chassis-local)
//   Frame B = wheel center / wheel body origin
//   Rest drop = explicit chassis-center-to-rest-wheel-center offset
//   Wheel collision = centered primitive cylinder, never glTF mesh in v0

#include "box3d/box3d.h"

enum JozzVehicleM5Corner
{
	JOZZ_M5_FRONT_LEFT = 0,
	JOZZ_M5_FRONT_RIGHT = 1,
	JOZZ_M5_REAR_LEFT = 2,
	JOZZ_M5_REAR_RIGHT = 3,
	JOZZ_M5_CORNER_COUNT = 4,
};

struct JozzVehicleM5Config
{
	// Chassis box half extents: x = half length, y = half height, z = half width.
	b3Vec3 chassisHalfExtents;
	float chassisDensity;

	// Corner layout in chassis-local space. Forward is +X.
	float axleHalfSpacing; // chassis center to front/rear axle along x
	float trackHalfWidth;  // chassis center to wheel center along z
	float restDrop;		   // chassis center down to rest wheel center

	// Wheel primitive.
	float wheelRadius;
	float wheelWidth;
	float wheelDensity;
	float wheelFriction;
	float wheelRollingResistance;

	// Suspension per corner.
	float suspensionHertz;
	float suspensionDampingRatio;
	float reboundTravel;	 // lower limit magnitude (wheel drops)
	float compressionTravel; // upper limit magnitude (wheel compresses)

	// Drive.
	float maxDriveSpeed; // wheel spin speed in rad/s at full throttle
	float maxDriveTorque;
	float brakeTorque;
	float coastTorque; // small drag torque toward zero spin with no input
	bool allWheelDrive;

	// Steering on the front axle.
	float maxSteeringAngleDegrees;
	float steeringHertz;
	float steeringDampingRatio;
	float maxSteeringTorque;

	// Soft keep-upright helper joint against the ground body, same idea as the
	// stock Box3D Driving sample. Off = raw vehicle behavior.
	bool uprightAssist;
	float uprightHertz;
	float uprightDampingRatio;

	// Same-vehicle collision filter group (negative = never self-collide).
	int filterGroupIndex;
};

// Defaults tuned for the current Offroad_Big_Wheels prototype scale. Wheel
// radius/width come from the caller so asset-derived M3A defaults stay the
// single source for wheel dimensions.
JozzVehicleM5Config JozzVehicleM5DefaultConfig( float wheelRadius, float wheelWidth, float suspensionTravelHint );

struct JozzVehicleM5
{
	bool valid;
	JozzVehicleM5Config config;
	b3BodyId chassisId;
	b3BodyId wheelIds[JOZZ_M5_CORNER_COUNT];
	b3ShapeId wheelShapeIds[JOZZ_M5_CORNER_COUNT];
	b3JointId wheelJointIds[JOZZ_M5_CORNER_COUNT];
	b3JointId uprightJointId;
};

// groundBodyId is only used for the optional upright assist joint.
JozzVehicleM5 CreateJozzVehicleM5( b3WorldId worldId, b3BodyId groundBodyId, const JozzVehicleM5Config& config,
								   b3Pos chassisSpawnPosition );

void DestroyJozzVehicleM5( JozzVehicleM5* vehicle );

struct JozzVehicleM5DriveInput
{
	float drive; // -1..1, +1 = forward (+X at spawn orientation)
	float steer; // -1..1, +1 = steer left (positive steering angle)
	bool brake;
};

void UpdateJozzVehicleM5Drive( const JozzVehicleM5& vehicle, const JozzVehicleM5DriveInput& input );

// Signed speed along the chassis forward (+X) axis in m/s.
float GetJozzVehicleM5ForwardSpeed( const JozzVehicleM5& vehicle );

// Rest wheel-center world position for a corner, following the live chassis
// transform. Debug/visual helpers use this; physics owns its own frames.
b3Pos GetJozzVehicleM5RestWheelCenter( const JozzVehicleM5& vehicle, int corner );
