// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

// M6 Suspension Rig Foundation + M7 Real Forces Foundation.
//
// The M5 vehicle proved the single-joint corner (chassis -> b3WheelJoint ->
// wheel). M6 introduced the multi-body corner; M7 rebuilt it so every
// behavior the driver feels comes from a mechanism a real car has, not from
// a script. The foundation is HARDPOINT-BASED on purpose: a suspension type
// is fully described by where its attachment points sit in chassis-local
// space, which is exactly the data a future asset importer can read from
// Blockbench markers/sockets. Swapping geometry (street/drift/offroad,
// light/heavy) means swapping hardpoints and tuning - not code.
//
// Rig types per axle:
//   INTEGRATED_STRUT   - the validated M5 model (one b3WheelJoint per corner).
//                        Cheap and proven; the fallback and the light-vehicle
//                        option. Optionally tilts the strut axis by a caster
//                        angle so even this rig gains a self-centering effect.
//   DOUBLE_WISHBONE    - multi-body: control-arm BODIES on chassis hinges
//                        (revolute, axis through the fore/aft mount hardpoints,
//                        angle limits = physical droop/bump stops) + spherical
//                        ball joints to the knuckle (cone/twist limited);
//                        coilover as a distance joint with spring/damping and
//                        travel limits; front axle steers through a rack body
//                        on a prismatic joint with rigid tie rods, rear axle
//                        gets a fixed toe link.
//   TRAILING_ARM       - one-sided trailing arm (Jozz's One_Sided_wheel_mount
//                        model): arm body hinged to the chassis about the
//                        lateral (Z) axis with angle-limit stops, wheel spins
//                        on the arm, coilover between contract damper points.
//                        No steering DOF - meant for the rear axle; on the
//                        front it builds but does not steer.
//
// M6 LESSON (recorded, do not repeat): the first wishbone build expressed
// each triangular arm as two rigid distance-joint rods. A distance constraint
// has TWO solutions (mirrored about the line through the chassis point), so a
// hard landing that momentarily overwhelmed the solver could snap the corner
// into the mirrored branch - "broken suspension" that never recovers. Arms as
// hinged bodies with angle limits have exactly one motion branch, and the
// limits are the same physical droop/bump stops a real car has.
//
// M7 real-forces rules:
//   Steering is back-drivable: with the driver's hands off (near-zero steer
//   input) the rack spring/servo release and only a small friction force
//   remains, so caster trail physically steers the wheels toward the travel
//   direction (counter-steer in slides, straightening after corners). The M6
//   software blend of the command toward the travel direction is REMOVED -
//   it read as scripted drift, which it was.
//   Drive is torque-based: the spin motor targets the rev limit and throttle
//   scales the available torque (tapering near the limit), so wheelspin,
//   burnouts and power-slides come from torque vs grip, not from a speed servo.
//   Anti-roll bars per axle transfer load between the corner pairs from the
//   travel difference - the real mechanism that replaces the upright-assist
//   crutch (now default OFF).
//   Quadratic aero drag at the chassis makes top speed a force balance.
//
// Physics rules carried over:
//   Wheel collision = primitive shapes, never glTF mesh in v0.
//   Structural bodies (knuckle, rack, arms) carry NO shapes; mass is set
//   explicitly (the M6 CCD/TOI lesson).
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
	JOZZ_M6_RIG_TRAILING_ARM = 2,
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
	float restArmDroopDeg;		   // arm rest angle: chassis mounts raised this many degrees
								   // above the ball joints, so at static ride height the arms
								   // slope DOWN to the wheel (droop) instead of up. 0 = arms
								   // level at the design pose; positive = wheels hang below the
								   // mounts. This is the geometric half of the pose - the spring
								   // rest holds the car at that pose.
};

// wheelbase/track feed the Ackermann trapezoid angle; isLeft mirrors Z.
JozzVehicleM6WishboneHardpoints JozzVehicleM6MakeWishboneHardpoints( const JozzVehicleM6WishboneGeometry& geometry,
																	 b3Vec3 restWheelCenter, bool isLeft, float wheelbase,
																	 float track );

// Trailing-arm corner geometry as chassis-local OFFSETS FROM THE REST WHEEL
// CENTER, authored for the LEFT corner (Z is mirrored for the right side).
// This is the hardpoint contract of the trailing arm rig: today it comes from
// the built-in generator or from the One_Sided_wheel_mount sidecar contract
// (see jozz_vehicle_m7_suspension_import), tomorrow from any asset's markers.
struct JozzVehicleM6TrailingArmGeometry
{
	b3Vec3 pivotOffset;			// hinge point on the chassis; +X = ahead of the wheel (a trailing arm trails)
	b3Vec3 damperArmOffset;		// coilover lower eye on the arm
	b3Vec3 damperChassisOffset; // coilover upper eye on the chassis
	float armMass;				// arm + hub carrier, the unsprung non-wheel mass
	bool loadedFromContract;	// diagnostics: true when an asset contract filled this struct
};

JozzVehicleM6TrailingArmGeometry JozzVehicleM6DefaultTrailingArmGeometry();

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

// The tie-rod/rack-and-arm linkage has an over-center "dead point" - a steer
// angle past which the rack stroke stops increasing with angle (P1, see
// AUDIT_PHYSICS_STEERING_2026_07_08_PL.md K1). Returns that angle in degrees
// for the given geometry, found by walking the stroke curve until it stops
// climbing. Shared by the P1 ball-joint twist fence, the validator, and the
// live UI clamp on "Maksymalny skręt kół" (P5) - all three must agree on
// where this line is, since ackermannFraction alone can move it from ~75 deg
// (fraction 0) down to ~50 deg (fraction 1.0).
float ComputeJozzVehicleM6SteeringDeadPointDeg( const JozzVehicleM6WishboneGeometry& geometry, float wheelbase,
												 float track, float rackHalfWidth );

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
	JozzVehicleM6TrailingArmGeometry trailingArm;
	float knuckleMass; // upright + hub + brake, the unsprung non-wheel mass
	float armMass;	   // one control-arm body (wishbone corners)
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
	// Spring preload: the coilover rest length is set this far ABOVE the design
	// length, so under the car's static weight it settles back at the design pose
	// (arms drooped as authored, correct track, zero toe) instead of sagging below
	// it. Raising it lifts ride height; travel limits stay measured from the design
	// pose. This is what makes the pose a deliberate setting, not an accident of
	// spring rate vs weight. Split per axle (P3, 2026-07-08) and deliberately NOT
	// scaled by frontSuspensionScale/rearSuspensionScale: it used to be, on the
	// theory that a stiffer-scaled axle sags less so needs less preload to reach
	// the same pose - but static deflection scales like 1/stiffness while the old
	// code multiplied preload BY the stiffness scale, coupling ride height to
	// spring rate in the wrong direction (a stiffness change silently moved ride
	// height, and there was no way to set height independent of stiffness or
	// independent per axle - see AUDIT_PHYSICS_STEERING_2026_07_08_PL.md K3).
	// Ride height and stiffness are now two independent dials.
	float suspensionPreloadFront;
	float suspensionPreloadRear;

	// Anti-roll bars: load transfer per axle from the left/right travel
	// difference, F = stiffness * (travelLeft - travelRight), pushing the
	// more-compressed corner down and its partner up, with equal/opposite
	// reactions on the chassis - a pure anti-roll couple, zero net force.
	// This is the real mechanism that lets the upright-assist crutch stay off.
	float arbFrontStiffness; // N per meter of travel difference; 0 disables
	float arbRearStiffness;

	// Quadratic aero drag applied at the chassis center of mass:
	// F = -0.5 * airDensity * dragArea * |v| * v. Top speed becomes a force
	// balance instead of a motor speed cap.
	float aeroDragArea; // Cd * A in m^2; 0 disables

	// Drive: torque-based. The spin motors always target the rev limit
	// (maxDriveSpeed) and the throttle scales the available torque, tapering
	// linearly to zero between driveTaperStart * maxDriveSpeed and
	// maxDriveSpeed - a minimal engine curve. Whether a wheel spins up (or
	// breaks loose) is decided by torque against contact grip, not by a
	// speed servo chasing a target.
	float maxDriveSpeed; // rad/s rev limit at the wheel
	float maxDriveTorque;
	float driveTaperStart; // 0..1 fraction of maxDriveSpeed where torque starts tapering
	float brakeTorque;
	float coastTorque;
	bool allWheelDrive;

	// Steering. Positive angle = left (see header comment). On a wishbone
	// front axle the rack spring chases rackTravel-scaled targets and the
	// Ackermann split comes from the physical trapezoid; on a strut axle the
	// targets go through the same math the M5 module validated.
	//
	// Back-drivable hands model (M7): while |steer input| > deadzone the rack
	// spring + servo act as the driver's grip and the power steering. With the
	// input released the spring/servo let go and only the friction force below
	// resists rack motion, so the caster trail in the geometry physically
	// steers the wheels toward the travel direction. The M6 software blend of
	// the command toward the travel direction is gone (documented negative
	// result: it read as scripted drift).
	float maxSteeringAngleDegrees;
	// Static toe (P5), wishbone corners only (strut/trailing not wired up -
	// out of scope for this pass). Positive = toe-IN (both noses point toward
	// the chassis centerline - the alignment-sheet convention). Implemented by
	// lengthening/shortening the tie-rod (front) / toe-link (rear) rest length
	// - a virtual turnbuckle - rather than moving any hardpoint, so it can't
	// perturb the P1 dead-point/P2 rackTravel calibration (those only look at
	// hp.steeringArm, which stays exactly as authored). See
	// CreateWishboneCorner's SteeringToeLengthDelta for the derivation and the
	// sign, verified against the validator's toe probe.
	float frontToeDeg;
	float rearToeDeg;
	float steeringHertz;
	float steeringDampingRatio;
	float maxSteeringTorque;	  // strut axle only; the rack spring is limited by hertz
	// Hands-off rack resistance, split into a Coulomb static/kinetic pair (P4,
	// 2026-07-08) instead of one flat force. A single flat cap made the rack
	// stop dead exactly where the caster centering force first dropped below
	// it - zero velocity dependence, so it could never overshoot center even
	// after a hard lock at speed (see AUDIT_PHYSICS_STEERING_2026_07_08_PL.md
	// S1). Static applies while the rack is (near) stationary - parking hold,
	// getting it moving at all; kinetic (lower) applies once it's actually
	// sliding, so the knuckle/wheel inertia can carry it a little past center
	// before it settles, the way a real column does.
	float rackStaticFrictionForce;
	float rackKineticFrictionForce;
	float steeringFrictionTorque; // N*m; hands-off resistance of a strut corner
	float steerInputDeadzone;	  // |input| below this = hands off the wheel
	// OPT-IN arcade return-to-center assist (default 0 = OFF = realistic). When
	// > 0, the hands-off rack also gets a WEAK spring pulling it toward center,
	// so the wheels straighten even at a standstill. This is deliberately off by
	// default and clearly an assist, NOT the honest mechanism: at rest a real
	// back-drivable steering has NO centering force (caster trail needs forward
	// rolling), which is exactly why M7 removed the old software self-align.
	// Verified empirically (2026-07-08): with this at 0, a wheel knocked to full
	// lock on a stationary car stays there and self-centers only once driving
	// (-29 deg -> 1.4 deg at 12.7 m/s) - correct physics. This slider exists for
	// players who want arcade auto-centering; it will lightly fight the caster
	// counter-steer in slides, so keep it low. Sibling of uprightAssist below.
	float rackCenteringHertz;
	bool ackermannGeometry;		  // strut: computed targets; wishbone: trapezoid arms
	float strutCasterDeg;		  // tilts the strut travel/steer axis rearward (0 = exact M5)

	// Soft keep-upright helper (same as M5/stock Driving). M7 default: OFF -
	// the anti-roll bars are the honest mechanism; this stays as a rescue
	// toggle for experiments.
	bool uprightAssist;
	float uprightHertz;
	float uprightDampingRatio;

	int filterGroupIndex;
};

JozzVehicleM6Config JozzVehicleM6DefaultConfig( float wheelRadius, float wheelWidth, float suspensionTravelHint );

struct JozzVehicleM6CornerRuntime
{
	int rigType;
	b3BodyId knuckleId;	 // wishbone corners
	b3BodyId upperArmId; // wishbone corners: control-arm bodies on chassis hinges
	b3BodyId lowerArmId;
	b3BodyId trailingArmId; // trailing-arm corners
	b3BodyId wheelId;
	b3ShapeId wheelShapeIds[JOZZ_M6_MAX_WHEEL_SHAPES];
	int wheelShapeCount;
	b3JointId strutJointId;		// b3WheelJoint (strut corners)
	b3JointId spinJointId;		// revolute (arm|knuckle)->wheel (multi-body corners)
	b3JointId upperHingeId;		// revolute chassis->arm, limits = droop/bump stops
	b3JointId lowerHingeId;
	b3JointId upperBallId;		// spherical arm->knuckle, cone/twist limited
	b3JointId lowerBallId;
	b3JointId armHingeId;		// revolute chassis->trailing arm
	b3JointId coiloverJointId;
	b3JointId steerLinkJointId; // tie rod (front) / toe link (rear), wishbone only
	// Cached rest geometry for telemetry/debug (chassis-local).
	b3Vec3 restWheelCenterLocal;
	JozzVehicleM6WishboneHardpoints hardpoints;
	// Trailing corners: resolved chassis-local points (mirrored per side) and
	// the coilover compensation the corner was built with. The hertz scale
	// maps the config suspension hertz onto the damper joint so live tuning
	// keeps the compensated wheel rate (see CreateTrailingArmCorner).
	b3Vec3 trailingPivotLocal;
	b3Vec3 trailingDamperArmLocal;
	b3Vec3 trailingDamperChassisLocal;
	float trailingCoiloverHertzScale;
	float trailingMotionRatio;
	// Coilover length at the authored design pose (arms at restArmDroopDeg, no
	// preload). Cached so ride height (preload) and the bump/droop travel stops
	// can be pushed onto the live joint (b3DistanceJoint_SetLength/SetLengthRange)
	// whenever the pose config changes, without rebuilding the vehicle.
	float coiloverDesignLength;
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

// The commanded rack angle from the raw input (clamped input * max angle).
// No velocity term: self-alignment is physical (caster trail back-driving the
// released rack), not a command blend.
float ComputeJozzVehicleM6RackAngle( const JozzVehicleM6Config& config, float steerInput );

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
