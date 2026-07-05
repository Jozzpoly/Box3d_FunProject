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
//   Wheel collision = primitive shape (cylinder/sphere), never glTF mesh in v0
//
// Direction convention (settled for good in M5.2 after two wrong guesses):
//   forward = +X, up = +Y, right = forward x up = +Z, therefore LEFT = -Z.
//   A positive steering angle rotates +X toward -Z about the +Y steering
//   axis, so positive steering = LEFT turn.

#include "box3d/box3d.h"

enum JozzVehicleM5Corner
{
	JOZZ_M5_FRONT_LEFT = 0,
	JOZZ_M5_FRONT_RIGHT = 1,
	JOZZ_M5_REAR_LEFT = 2,
	JOZZ_M5_REAR_RIGHT = 3,
	JOZZ_M5_CORNER_COUNT = 4,
};

// Wheel collision primitive. The faceted cylinder matches the visual tire
// but its flats excite a built-in washboard at speed (engine caps hull
// cylinders at 32 sides, ~2.5mm radial ripple at the current wheel radius).
// The sphere is perfectly smooth in the rolling direction - the reference
// shape for isolating facet chatter - at the cost of a point-like contact
// and a lateral bulge past the visual tire width (the stock Driving sample
// also uses sphere wheels).
enum JozzVehicleM5WheelShape
{
	JOZZ_M5_WHEEL_CYLINDER = 0,
	JOZZ_M5_WHEEL_SPHERE = 1,
};

// Steering linkage model.
//   INDEPENDENT: each front wheel has its own servo chasing the same target;
//     if one wheel is blocked the other keeps steering (broken-tie-rod look).
//   LINKED: a virtual tie rod - each wheel's commanded target is clamped to
//     stay within tieRodToleranceDegrees of the OTHER wheel's actual angle
//     (plus the Ackermann offset), so a blocked wheel holds its partner back,
//     like a real steering linkage.
enum JozzVehicleM5SteeringMode
{
	JOZZ_M5_STEERING_INDEPENDENT = 0,
	JOZZ_M5_STEERING_LINKED = 1,
};

struct JozzVehicleM5Config
{
	// Chassis box half extents: x = half length, y = half height, z = half width.
	b3Vec3 chassisHalfExtents;
	float chassisDensity;
	// Drops the chassis collision box (and so the center of mass) below the
	// body origin without moving the suspension anchors. Higher values = lower
	// CG = less pitch under acceleration and less roll in corners.
	float cgVerticalOffset;

	// Corner layout in chassis-local space. Forward is +X, left is -Z.
	float axleHalfSpacing; // chassis center to front/rear axle along x
	float trackHalfWidth;  // chassis center to wheel center along |z|
	float restDrop;		   // chassis center down to rest wheel center

	// Wheel primitive.
	int wheelShape;			// JozzVehicleM5WheelShape
	int wheelCylinderSides; // 3..32 (hard engine cap); low values exaggerate facet chatter for experiments
	float wheelRadius;
	float wheelWidth;
	float wheelDensity;
	float wheelFriction;
	float wheelRollingResistance;

	// Suspension per corner. Front/rear scales multiply the base values so a
	// single pair of sliders can stay authoritative while axles get balanced
	// (e.g. stiffer rear under acceleration squat).
	float suspensionHertz;
	float suspensionDampingRatio;
	float frontSuspensionScale;
	float rearSuspensionScale;
	float reboundTravel;	 // lower limit magnitude (wheel drops)
	float compressionTravel; // upper limit magnitude (wheel compresses)

	// Drive.
	float maxDriveSpeed; // wheel spin speed in rad/s at full throttle
	float maxDriveTorque;
	float brakeTorque;
	float coastTorque; // small drag torque toward zero spin with no input
	bool allWheelDrive;

	// Steering on the front axle. Positive angle = left (see header comment).
	float maxSteeringAngleDegrees;
	float steeringHertz;
	float steeringDampingRatio;
	float maxSteeringTorque;
	int steeringMode;			   // JozzVehicleM5SteeringMode
	float tieRodToleranceDegrees;  // linked mode: max commanded divergence from the partner wheel's actual angle
	bool ackermannGeometry;		   // inner wheel steers more, from wheelbase/track geometry
	bool speedSensitiveSteering;   // shrink the usable steering angle with speed (common driving assist)
	float steeringTaperStartSpeed; // m/s where the taper begins
	float steeringTaperEndSpeed;   // m/s where the taper bottoms out
	float steeringTaperMinScale;   // fraction of max angle left at/above taper end

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
	float steer; // -1..1, +1 = steer left (positive steering angle, turn toward -Z)
	bool brake;
};

void UpdateJozzVehicleM5Drive( const JozzVehicleM5& vehicle, const JozzVehicleM5DriveInput& input );

// Per-front-wheel steering targets for a given rack angle (radians, +left),
// applying the Ackermann inner/outer split when enabled and clamping to the
// steering limit. Exposed so validation can assert the exact same math the
// drive path uses.
void GetJozzVehicleM5SteeringTargets( const JozzVehicleM5Config& config, float rackAngle, float* outLeftTarget,
									  float* outRightTarget );

// Signed speed along the chassis forward (+X) axis in m/s.
float GetJozzVehicleM5ForwardSpeed( const JozzVehicleM5& vehicle );

// Rest wheel-center world position for a corner, following the live chassis
// transform. Debug/visual helpers use this; physics owns its own frames.
b3Pos GetJozzVehicleM5RestWheelCenter( const JozzVehicleM5& vehicle, int corner );

// Per-wheel telemetry snapshot for tuning UI, plots, and headless probes.
struct JozzVehicleM5WheelTelemetry
{
	float suspensionTravel; // m along chassis up; positive = compressed above rest
	float suspensionLoad;	// N, wheel-joint constraint force projected on chassis up
	float spinSpeed;		// rad/s
	float steeringAngle;	// rad, +left; 0 on the rear axle
	bool groundContact;		// any live contact manifold on the wheel shape
};

JozzVehicleM5WheelTelemetry GetJozzVehicleM5WheelTelemetry( const JozzVehicleM5& vehicle, int corner );
