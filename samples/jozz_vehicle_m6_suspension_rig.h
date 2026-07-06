// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M6 Suspension Rig Foundation: the first multi-body suspension layer.
//
// The M5 vehicle proved the single-joint corner (chassis -> b3WheelJoint ->
// wheel). M6 introduces the real thing: a knuckle (upright) body per corner,
// control arms expressed as rigid link rods between hardpoints, a coilover
// spring/damper, and a physical steering rack with tie rods. The foundation
// is HARDPOINT-BASED on purpose: a suspension type is fully described by
// where its attachment points sit in chassis-local space, which is exactly
// the data a future asset importer can read from Blockbench markers/sockets.
// Swapping geometry (street/drift/offroad, light/heavy) means swapping
// hardpoints and tuning - not code.
//
// Rig types per axle:
//   INTEGRATED_STRUT   - the validated M5 model (one b3WheelJoint per corner).
//                        Cheap and proven; the fallback and the light-vehicle
//                        option. Optionally tilts the strut axis by a caster
//                        angle so even this rig gains a self-centering effect.
//   DOUBLE_WISHBONE    - multi-body: knuckle + wheel bodies; upper/lower
//                        wishbones as 2+2 rigid rods (a triangular arm IS two
//                        rods meeting at a ball joint); coilover as a distance
//                        joint with spring/damping/travel limits; front axle
//                        steers through a rack body on a prismatic joint with
//                        rigid tie rods, rear axle gets a fixed toe link.
//
// Why link rods instead of wishbone bodies: a two-point rigid rod is exactly
// a distance constraint - the cheapest, most stable constraint an iterative
// solver has, and the kinematics are identical. Fewer bodies also keeps the
// mass ratios sane under a ~700 kg chassis. Visual wishbones can later be
// drawn/mounted between the same hardpoints (the M4 endpoint-preview idea).
//
// Physics rules carried over:
//   Wheel collision = primitive shapes, never glTF mesh in v0.
//   Knuckle/rack shapes never collide (maskBits = 0); they exist for mass.
//   Suspension spring rest = the authored rest pose; travel limits explicit.
//
// Direction convention (identical to M5, single source of truth there too):
//   forward = +X, up = +Y, right = forward x up = +Z, LEFT = -Z.
//   Positive steering angle rotates +X toward -Z about +Y = LEFT turn.

#include "box3d/box3d.h"

enum JozzVehicleM6Corner
{
	JOZZ_M6_FRONT_LEFT = 0,
	JOZZ_M6_FRONT_RIGHT = 1,
	JOZZ_M6_REAR_LEFT = 2,
	JOZZ_M6_REAR_RIGHT = 3,
	JOZZ_M6_CORNER_COUNT = 4,
};

enum JozzVehicleM6RigType
{
	JOZZ_M6_RIG_INTEGRATED_STRUT = 0,
	JOZZ_M6_RIG_DOUBLE_WISHBONE = 1,
};

// Collision categories. IMPORTANT: b3DefaultShapeDef() sets categoryBits to
// ALL bits (unlike Box2D's single bit), so a shape left on the default
// category matches EVERY mask, including the rolling sphere's terrain-only
// mask. For the split wheel envelope to work, worlds must tag both sides:
//   drivable surfaces  -> categoryBits = JOZZ_M6_TERRAIN_CATEGORY (only)
//   props/walls/misc   -> categoryBits = JOZZ_M6_OBJECT_CATEGORY (or any
//                         category without the terrain bit)
// A shape that keeps the default all-bits category will be treated as
// terrain by the rolling sphere AND as an obstacle by the sidewall.
#define JOZZ_M6_TERRAIN_CATEGORY 0x2ull
#define JOZZ_M6_OBJECT_CATEGORY 0x1ull

// Wheel collision envelope. The single sphere (M5.2 default) is perfectly
// smooth but bulges past the visual tire laterally by (radius - width/2),
// ~0.29 m at the current offroad wheel - the "invisible wall" next to props.
// A hull cylinder has the correct width but its facets are a built-in
// washboard (engine caps hull cylinders at 32 sides, ~2.5 mm radial ripple
// at r=0.51).
//
// SPLIT_SPHERE_SIDEWALL is the M6 answer: two shapes on the wheel body with
// complementary collision masks. The smooth sphere collides ONLY with shapes
// tagged terrainCategoryBits (ground, ramps, washboard, heightfield), so
// rolling stays facet-free; the full-width cylinder collides with everything
// EXCEPT terrain, so props/walls/curbs meet the true tire width instead of
// the sphere bulge. Requires the world's drivable surfaces to carry the
// terrain category; if terrainCategoryBits is 0 the builder falls back to a
// plain sphere rather than creating a wheel that ignores the ground.
//
// PHASED_UNION stacks rotated 32-side cylinder hulls into an effectively
// 64..128-facet union at the true width. Measured result (2026-07-05 probe):
// it rolls WORSE than a single cylinder - the contact hops between the
// layered hulls thousands of times per second and every hop discards the
// solver's warm-start impulses. It stays available as the measured negative
// result and a facet-mechanism visualizer, not as a recommendation.
//
// Radius/width should come from asset markers (M3A), so future wheel models
// adapt automatically.
enum JozzVehicleM6WheelEnvelopeMode
{
	JOZZ_M6_ENVELOPE_SPHERE = 0,
	JOZZ_M6_ENVELOPE_CYLINDER = 1,
	JOZZ_M6_ENVELOPE_PHASED_UNION = 2,
	JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL = 3,
};

#define JOZZ_M6_MAX_WHEEL_SHAPES 4

struct JozzVehicleM6WheelEnvelopeDesc
{
	int mode;			 // JozzVehicleM6WheelEnvelopeMode
	int cylinderSides;	 // 3..32 per layer (engine hull cap)
	int unionLayerCount; // 2..4 phased layers for PHASED_UNION
	float radius;		 // from asset markers (M3A)
	float width;		 // from asset markers (M3A)
	// Collision category carried by drivable surfaces; used by
	// SPLIT_SPHERE_SIDEWALL to separate rolling from side contact.
	uint64_t terrainCategoryBits;
};

// Creates the wheel collision shapes on an existing wheel body whose local Y
// axis is the wheel spin axis (the body is spawned with local Y rotated onto
// the world axle direction, exactly like M5). Returns the shape count.
int CreateJozzVehicleM6WheelEnvelope( b3BodyId wheelBodyId, const b3ShapeDef* shapeDef,
									  const JozzVehicleM6WheelEnvelopeDesc* desc,
									  b3ShapeId outShapeIds[JOZZ_M6_MAX_WHEEL_SHAPES] );

// Hardpoints of one double-wishbone corner in CHASSIS-LOCAL space at rest.
// This struct is the actual suspension contract: today it is produced by
// JozzVehicleM6MakeWishboneHardpoints from high-level geometry parameters,
// tomorrow it can be filled straight from asset markers/sockets.
struct JozzVehicleM6WishboneHardpoints
{
	// On the knuckle (expressed chassis-local at rest; the knuckle body origin
	// spawns at the rest wheel center, so knuckle-local = these minus wheel center).
	b3Vec3 upperBallJoint;
	b3Vec3 lowerBallJoint;
	b3Vec3 steeringArm; // tie-rod / toe-link end on the knuckle

	// On the chassis.
	b3Vec3 upperFrontChassis;
	b3Vec3 upperRearChassis;
	b3Vec3 lowerFrontChassis;
	b3Vec3 lowerRearChassis;
	b3Vec3 coiloverChassis;

	// On the knuckle: lower coilover eye.
	b3Vec3 coiloverKnuckle;
};

// High-level double-wishbone geometry. All lengths in meters, angles in
// degrees. The generator turns these into hardpoints per corner; the same
// numbers work for a street car (moderate caster, short travel), a drift
// setup (more caster = stronger self-centering), or offroad (long arms).
struct JozzVehicleM6WishboneGeometry
{
	float uprightHalfHeight;	   // ball joints sit +/- this above/below wheel center
	float kingpinOffset;		   // ball joints sit this far inboard of the wheel center
	float casterDeg;			   // kingpin top tilted rearward -> mechanical trail -> physical
								   // self-aligning torque through the tie rod and rack
	float kingpinInclinationDeg;   // kingpin top tilted inboard -> smaller scrub radius
	float upperArmLength;		   // ball joint to chassis mounts, upper
	float lowerArmLength;		   // ball joint to chassis mounts, lower (longer = camber gain)
	float armHalfSpread;		   // chassis mounts sit +/- this along X around the ball joint
	float steeringArmBack;		   // steering arm reaches this far behind the wheel center
	bool ackermannTrapezoid;	   // angle the steering arms inward so the inner wheel steers
								   // tighter (classic steering trapezoid), from wheelbase/track
	float ackermannFraction;	   // 0..1 scale on the trapezoid angle. 1 = full geometric
								   // Ackermann, which at this geometry is aggressive enough to
								   // push the inner-wheel linkage toward its over-center dead
								   // point at full lock (measured: commanded 32 deg -> 46 deg,
								   // arm and tie rod within ~6 deg of collinear, steering
								   // stuck). Real cars run partial Ackermann for the same
								   // reason; default 0.6.
	float coiloverTopHeight;	   // coilover chassis eye this far above the wheel center
	float coiloverTopInboard;	   // ...and this far inboard
};

// wheelbase/track feed the Ackermann trapezoid angle; isLeft mirrors Z.
JozzVehicleM6WishboneHardpoints JozzVehicleM6MakeWishboneHardpoints( const JozzVehicleM6WishboneGeometry& geometry,
																	 b3Vec3 restWheelCenter, bool isLeft, float wheelbase,
																	 float track );

// Exact rack stroke (meters) that yaws the INNER wheel to steerAngle radians,
// solved from the tie-rod/steering-arm geometry in the horizontal plane
// (closed form, no iteration). The linkage is nonlinear: at the current
// default geometry 32 degrees needs ~0.075 m, noticeably less than the
// small-angle steeringArmBack * angle estimate - using the estimate as the
// rack limit let the inner wheel overshoot to ~45 degrees. Both the default
// rackTravel and the drive-path target mapping go through this function so
// the commanded angle, the physical limit, and the trapezoid always agree.
float ComputeJozzVehicleM6RackStroke( const JozzVehicleM6WishboneGeometry& geometry, float wheelbase, float track,
									  float rackHalfWidth, float steerAngle );

struct JozzVehicleM6Config
{
	// Chassis (same conventions as M5).
	b3Vec3 chassisHalfExtents;
	float chassisDensity;
	float cgVerticalOffset;

	// Corner layout in chassis-local space. Forward is +X, left is -Z.
	float axleHalfSpacing;
	float trackHalfWidth;
	float restDrop;

	// Rig type per axle (JozzVehicleM6RigType).
	int frontRigType;
	int rearRigType;

	// Multi-body pieces.
	JozzVehicleM6WishboneGeometry wishbone;
	float knuckleMass; // upright + hub + brake, the unsprung non-wheel mass
	float rackMass;
	float rackHalfWidth;   // rack body half length across Z
	float rackTravel;	   // prismatic limit; full steering maps onto this stroke
	// Power-steering servo: a velocity motor on the rack prismatic joint with
	// a hard force cap. The position spring alone tops out around the rack's
	// effective mass times hertz^2, which is an order of magnitude short of
	// twisting stationary loaded tires (the same parking-torque lesson the M5.1
	// playtest taught about maxSteeringTorque).
	float rackServoForce;	  // N; cap on the servo force
	float rackServoSpeedGain; // 1/s; rack speed commanded per meter of target error
	float rackServoMaxSpeed;  // m/s; rack slew rate limit

	// Wheel envelope + material.
	JozzVehicleM6WheelEnvelopeDesc wheelEnvelope;
	float wheelDensity;
	float wheelFriction;
	float wheelRollingResistance;

	// Suspension (per-axle scales multiply the base values, like M5).
	float suspensionHertz;
	float suspensionDampingRatio;
	float frontSuspensionScale;
	float rearSuspensionScale;
	float reboundTravel;
	float compressionTravel;

	// Drive.
	float maxDriveSpeed;
	float maxDriveTorque;
	float brakeTorque;
	float coastTorque;
	bool allWheelDrive;

	// Steering. Positive angle = left (see header comment). On a wishbone
	// front axle the rack spring chases rackTravel-scaled targets and the
	// Ackermann split comes from the physical trapezoid; on a strut axle the
	// targets go through the same math the M5 module validated.
	float maxSteeringAngleDegrees;
	float steeringHertz;
	float steeringDampingRatio;
	float maxSteeringTorque; // strut axle only; the rack spring is limited by hertz
	bool ackermannGeometry;	 // strut: computed targets; wishbone: trapezoid arms
	float strutCasterDeg;	 // tilts the strut travel/steer axis rearward (0 = exact M5)

	// Self-aligning assist (the software half of drift feel; the hardware
	// half is casterDeg above). When the driver eases off the steer input
	// during a slide, the commanded rack angle blends toward the direction
	// the car is actually moving, like a real wheel finding center.
	bool selfAlignAssist;
	float selfAlignGain;	 // 0..1 blend weight at zero steer input
	float selfAlignMinSpeed; // m/s of planar speed before the assist wakes up
	float selfAlignMaxSlipDeg; // clamp on the alignment target angle

	// Soft keep-upright helper (same as M5/stock Driving).
	bool uprightAssist;
	float uprightHertz;
	float uprightDampingRatio;

	int filterGroupIndex;
};

JozzVehicleM6Config JozzVehicleM6DefaultConfig( float wheelRadius, float wheelWidth, float suspensionTravelHint );

struct JozzVehicleM6CornerRuntime
{
	int rigType;
	b3BodyId knuckleId; // null on strut corners
	b3BodyId wheelId;
	b3ShapeId wheelShapeIds[JOZZ_M6_MAX_WHEEL_SHAPES];
	int wheelShapeCount;
	b3JointId strutJointId;	   // b3WheelJoint (strut corners)
	b3JointId spinJointId;	   // revolute knuckle->wheel (wishbone corners)
	b3JointId linkJointIds[4]; // upper front/rear, lower front/rear rods
	int linkCount;
	b3JointId coiloverJointId;
	b3JointId steerLinkJointId; // tie rod (front) / toe link (rear)
	// Cached rest geometry for telemetry/debug (chassis-local).
	b3Vec3 restWheelCenterLocal;
	JozzVehicleM6WishboneHardpoints hardpoints;
};

struct JozzVehicleM6
{
	bool valid;
	JozzVehicleM6Config config;
	b3BodyId chassisId;
	JozzVehicleM6CornerRuntime corners[JOZZ_M6_CORNER_COUNT];
	b3BodyId rackId;	  // null unless the front axle is a wishbone rig
	b3JointId rackJointId;
	b3JointId uprightJointId;
};

JozzVehicleM6 CreateJozzVehicleM6( b3WorldId worldId, b3BodyId groundBodyId, const JozzVehicleM6Config& config,
								   b3Pos chassisSpawnPosition );

void DestroyJozzVehicleM6( JozzVehicleM6* vehicle );

struct JozzVehicleM6DriveInput
{
	float drive; // -1..1, +1 = forward (+X at spawn orientation)
	float steer; // -1..1, +1 = steer left
	bool brake;
};

void UpdateJozzVehicleM6Drive( const JozzVehicleM6& vehicle, const JozzVehicleM6DriveInput& input );

// The commanded rack angle after the self-align assist, exposed so the
// validation CLI asserts the exact math the drive path uses. localVelocity is
// the chassis velocity rotated into chassis space.
float ComputeJozzVehicleM6RackAngle( const JozzVehicleM6Config& config, float steerInput, b3Vec3 localVelocity );

float GetJozzVehicleM6ForwardSpeed( const JozzVehicleM6& vehicle );

// Chassis-frame sideslip angle beta in radians: the angle between the nose
// and the actual travel direction. Positive input convention matches
// steering: a travel direction left of the nose (-Z) needs a POSITIVE
// steering angle to align, and this returns that aligning angle.
float GetJozzVehicleM6AlignmentAngle( const JozzVehicleM6& vehicle );

b3Pos GetJozzVehicleM6RestWheelCenter( const JozzVehicleM6& vehicle, int corner );

struct JozzVehicleM6WheelTelemetry
{
	float suspensionTravel; // m along chassis up; positive = compressed
	float suspensionLoad;	// N through the strut joint / coilover, on chassis up
	float spinSpeed;		// rad/s
	float steeringAngle;	// rad, +left; wishbone corners measure the knuckle yaw
	float slipAngle;		// rad; wheel velocity direction vs wheel heading (drift telemetry)
	float camberAngle;		// rad; wishbone corners show the geometric camber gain
	bool groundContact;
};

JozzVehicleM6WheelTelemetry GetJozzVehicleM6WheelTelemetry( const JozzVehicleM6& vehicle, int corner );
