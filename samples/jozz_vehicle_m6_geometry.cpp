// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_geometry.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

// Same one-line constant as the physics side keeps in its own anonymous
// namespace (jozz_vehicle_m6_suspension_rig.cpp); duplicated here rather than
// shared through the header so neither TU's local use has to change. Two
// internal-linkage copies, no ODR concern.
constexpr float DEGREES_TO_RADIANS = B3_PI / 180.0f;

} // namespace

JozzVehicleM6WishboneHardpoints JozzVehicleM6MakeWishboneHardpoints( const JozzVehicleM6WishboneGeometry& geometry,
																	 b3Vec3 restWheelCenter, bool isLeft, float wheelbase,
																	 float track )
{
	JozzVehicleM6WishboneHardpoints points = {};

	// "Inboard" points from the wheel toward the chassis centerline. The left
	// wheel sits at negative Z, so its inboard direction is +Z.
	float in = isLeft ? 1.0f : -1.0f;

	float casterTangent = std::tan( geometry.casterDeg * DEGREES_TO_RADIANS );
	float kpiTangent = std::tan( geometry.kingpinInclinationDeg * DEGREES_TO_RADIANS );
	float h = geometry.uprightHalfHeight;

	// Ball joints rotated around the wheel center: caster tilts the kingpin
	// top rearward (mechanical trail -> physical self-aligning torque),
	// kingpin inclination tilts the top inboard (smaller scrub radius).
	points.upperBallJoint = b3Add( restWheelCenter, { -casterTangent * h, h, in * ( geometry.kingpinOffset + kpiTangent * h ) } );
	points.lowerBallJoint = b3Add( restWheelCenter, { casterTangent * h, -h, in * ( geometry.kingpinOffset - kpiTangent * h ) } );

	// Control-arm chassis mounts: inboard of the ball joints, split fore/aft so
	// each triangular arm becomes two rods sharing the ball joint. The rest droop
	// angle raises the mounts above the ball joints so the arm slopes DOWN to the
	// wheel at the design pose - the wheels hang on drooping arms instead of the
	// arms folding up. The raise is purely vertical (inboard reach stays
	// armLength), so the horizontal kinematics - track, steering-linkage geometry
	// - are unchanged and no toe/camber is induced; only the arm angle changes.
	float droopTan = std::tan( geometry.restArmDroopDeg * DEGREES_TO_RADIANS );
	b3Vec3 upperInboard =
		b3Add( points.upperBallJoint, { 0.0f, geometry.upperArmLength * droopTan, in * geometry.upperArmLength } );
	b3Vec3 lowerInboard =
		b3Add( points.lowerBallJoint, { 0.0f, geometry.lowerArmLength * droopTan, in * geometry.lowerArmLength } );
	points.upperFrontChassis = b3Add( upperInboard, { geometry.armHalfSpread, 0.0f, 0.0f } );
	points.upperRearChassis = b3Add( upperInboard, { -geometry.armHalfSpread, 0.0f, 0.0f } );
	points.lowerFrontChassis = b3Add( lowerInboard, { geometry.armHalfSpread, 0.0f, 0.0f } );
	points.lowerRearChassis = b3Add( lowerInboard, { -geometry.armHalfSpread, 0.0f, 0.0f } );

	// Steering arm reaches rearward from the kingpin line. The Ackermann
	// trapezoid angles it inboard so the tie-rod lines converge toward the
	// rear axle, making the inner wheel steer tighter purely mechanically.
	// The fraction backs the trapezoid off from full geometric Ackermann,
	// which at this scale drives the linkage into its over-center dead point
	// near full lock.
	float armInboard = geometry.kingpinOffset;
	if ( geometry.ackermannTrapezoid && wheelbase > 0.01f )
	{
		armInboard += geometry.ackermannFraction * geometry.steeringArmBack * ( track / wheelbase );
	}
	points.steeringArm = b3Add( restWheelCenter, { -geometry.steeringArmBack, 0.0f, in * armInboard } );

	// Coilover: chassis eye up and inboard, knuckle eye at the lower ball
	// joint (motion ratio ~1, the simplest honest starting point).
	points.coiloverChassis = b3Add( restWheelCenter, { 0.0f, geometry.coiloverTopHeight, in * geometry.coiloverTopInboard } );
	points.coiloverKnuckle = points.lowerBallJoint;

	return points;
}

JozzVehicleM6TrailingArmGeometry JozzVehicleM6DefaultTrailingArmGeometry()
{
	// Built-in fallback when no asset contract fills the struct: a plain
	// rear trailing arm with the pivot ahead of the wheel and a near-vertical
	// coilover. Offsets are chassis-local from the rest wheel center, authored
	// for the LEFT corner (Z mirrored on the right).
	JozzVehicleM6TrailingArmGeometry geometry = {};
	geometry.pivotOffset = { 0.55f, 0.10f, 0.0f };
	geometry.damperArmOffset = { -0.02f, 0.08f, 0.0f };
	geometry.damperChassisOffset = { 0.0f, 0.52f, 0.04f };
	geometry.armMass = 14.0f;
	geometry.loadedFromContract = false;
	return geometry;
}

float ComputeJozzVehicleM6RackStroke( const JozzVehicleM6WishboneGeometry& geometry, float wheelbase, float track,
									  float rackHalfWidth, float steerAngle )
{
	// Horizontal-plane linkage of the LEFT (inner-when-steering-left) wheel,
	// mirrored by symmetry for the other side. All coordinates relative to
	// the front axle X.
	float s = geometry.steeringArmBack;
	float ackermann =
		geometry.ackermannTrapezoid && wheelbase > 0.01f ? geometry.ackermannFraction * s * ( track / wheelbase ) : 0.0f;

	// Kingpin base and steering-arm end at rest (x, z), left wheel at -track.
	float kingpinZ = -track + geometry.kingpinOffset;
	b3Vec3 arm = { -s, 0.0f, ackermann }; // arm end relative to the kingpin base

	// Rotate the arm about +Y by the steer angle (+ = left, +X toward -Z).
	float sinAngle = std::sin( steerAngle );
	float cosAngle = std::cos( steerAngle );
	float armX = arm.x * cosAngle + arm.z * sinAngle;
	float armZ = -arm.x * sinAngle + arm.z * cosAngle;

	// Tie rod runs from the arm end to the rack end at fixed x = -s; its
	// length comes from the rest pose where the rack end sits at -rackHalfWidth.
	float restArmZ = kingpinZ + ackermann;
	float tieRodLength = std::fabs( ( -rackHalfWidth ) - restArmZ );

	// Solve the rack end Z that keeps the tie rod length: closed form since
	// the rack end only moves along Z. Clamp keeps a degenerate geometry from
	// producing NaN; the result is then simply the maximum reachable stroke.
	float deltaX = armX + s;
	float reach = std::sqrt( b3MaxFloat( tieRodLength * tieRodLength - deltaX * deltaX, 1.0e-6f ) );
	float rackEndZ = ( kingpinZ + armZ ) + reach;
	return rackEndZ - ( -rackHalfWidth );
}

float ComputeJozzVehicleM6SteeringDeadPointDeg( const JozzVehicleM6WishboneGeometry& geometry, float wheelbase,
												 float track, float rackHalfWidth )
{
	float deadPointDeg = 90.0f;
	float prevStroke = ComputeJozzVehicleM6RackStroke( geometry, wheelbase, track, rackHalfWidth, 1.0f * DEGREES_TO_RADIANS );
	for ( float deg = 1.5f; deg < 89.0f; deg += 0.5f )
	{
		float stroke = ComputeJozzVehicleM6RackStroke( geometry, wheelbase, track, rackHalfWidth, deg * DEGREES_TO_RADIANS );
		if ( stroke <= prevStroke )
		{
			deadPointDeg = deg;
			break;
		}
		prevStroke = stroke;
	}
	return deadPointDeg;
}

bool SanitizeJozzVehicleM6Config( JozzVehicleM6Config* config )
{
	bool changed = false;

	// One clamp with one printed warning per field. Bounds are deliberately
	// WIDE - this is a "don't hand the solver a NaN or a singular constraint"
	// net for hand-edited files, not a tuning opinion (the UI sliders already
	// express those). A non-finite value clamps to the LOWER bound.
	auto clampField = [&changed]( float* field, const char* name, float lo, float hi ) {
		float value = *field;
		if ( std::isfinite( value ) == false )
		{
			value = lo;
		}
		value = b3ClampFloat( value, lo, hi );
		if ( value != *field )
		{
			std::printf( "jozz m6 WARNING: config sanitized: %s %.4f -> %.4f\n", name, (double)*field, (double)value );
			*field = value;
			changed = true;
		}
	};

	// Structural lengths: zero or negative here means a zero-length arm/link
	// or a degenerate hull - singular joint frames, NaN normals.
	clampField( &config->chassisHalfExtents.x, "chassisHalfExtents.x", 0.10f, 10.0f );
	clampField( &config->chassisHalfExtents.y, "chassisHalfExtents.y", 0.05f, 5.0f );
	clampField( &config->chassisHalfExtents.z, "chassisHalfExtents.z", 0.10f, 5.0f );
	clampField( &config->axleHalfSpacing, "axleHalfSpacing", 0.30f, 6.0f );
	clampField( &config->trackHalfWidth, "trackHalfWidth", 0.30f, 4.0f );
	clampField( &config->restDrop, "restDrop", 0.05f, 3.0f );
	clampField( &config->wishbone.uprightHalfHeight, "uprightHalfHeight", 0.05f, 1.0f );
	clampField( &config->wishbone.upperArmLength, "upperArmLength", 0.10f, 2.0f );
	clampField( &config->wishbone.lowerArmLength, "lowerArmLength", 0.10f, 2.0f );
	clampField( &config->wishbone.armHalfSpread, "armHalfSpread", 0.05f, 1.0f );
	clampField( &config->wishbone.steeringArmBack, "steeringArmBack", 0.05f, 1.0f );
	clampField( &config->wishbone.kingpinOffset, "kingpinOffset", 0.01f, 0.5f );
	clampField( &config->wishbone.ackermannFraction, "ackermannFraction", 0.0f, 1.0f );
	// Measured over-center ceiling for droop is 16 deg (M8 pose work); past it
	// the steering trapezoid goes non-deterministic. The UI slider already
	// stops at 16 - this catches the file/env path.
	clampField( &config->wishbone.restArmDroopDeg, "restArmDroopDeg", 0.0f, 16.0f );
	// A rack as wide as the track means zero-length tie rods.
	clampField( &config->rackHalfWidth, "rackHalfWidth", 0.05f, b3MaxFloat( 0.06f, config->trackHalfWidth - 0.10f ) );

	// Masses and densities: zero mass = infinite inverse mass in the solver.
	clampField( &config->chassisDensity, "chassisDensity", 10.0f, 5000.0f );
	clampField( &config->wheelDensity, "wheelDensity", 5.0f, 2000.0f );
	clampField( &config->knuckleMass, "knuckleMass", 1.0f, 500.0f );
	clampField( &config->armMass, "armMass", 0.5f, 200.0f );
	clampField( &config->rackMass, "rackMass", 0.5f, 200.0f );
	clampField( &config->trailingArm.armMass, "trailingArm.armMass", 1.0f, 500.0f );

	// Suspension: non-positive travel inverts the coilover length limits.
	clampField( &config->compressionTravel, "compressionTravel", 0.02f, 2.0f );
	clampField( &config->reboundTravel, "reboundTravel", 0.02f, 2.0f );
	clampField( &config->suspensionHertz, "suspensionHertz", 0.2f, 60.0f );
	clampField( &config->suspensionDampingRatio, "suspensionDampingRatio", 0.0f, 20.0f );
	clampField( &config->frontSuspensionScale, "frontSuspensionScale", 0.05f, 10.0f );
	clampField( &config->rearSuspensionScale, "rearSuspensionScale", 0.05f, 10.0f );
	clampField( &config->suspensionPreloadFront, "suspensionPreloadFront", -0.30f, 0.50f );
	clampField( &config->suspensionPreloadRear, "suspensionPreloadRear", -0.30f, 0.50f );

	// Steering: toe far past the UI range makes the virtual turnbuckle bend
	// the linkage into geometries the dead-point math never sees.
	clampField( &config->frontToeDeg, "frontToeDeg", -5.0f, 5.0f );
	clampField( &config->rearToeDeg, "rearToeDeg", -5.0f, 5.0f );
	clampField( &config->steeringHertz, "steeringHertz", 0.5f, 60.0f );
	clampField( &config->steeringDampingRatio, "steeringDampingRatio", 0.0f, 20.0f );
	// P4b load-dependent rack friction: negative values would ADD energy
	// (anti-friction); a coeff past 1.0 means more friction than the load
	// itself - both nonsensical, not tuning.
	clampField( &config->rackFrictionBase, "rackFrictionBase", 0.0f, 1000.0f );
	clampField( &config->rackFrictionLoadCoeff, "rackFrictionLoadCoeff", 0.0f, 1.0f );

	// The P5 dead-point clamp, applied AFTER the geometry fields above so it
	// evaluates the sanitized geometry. Mirrors ApplyPendingStructuralSetup's
	// UI clamp (fence margin 10 + 3 deg clearance); without this a hand-edited
	// preset bypassed the fence entirely.
	{
		float wheelbase = 2.0f * config->axleHalfSpacing;
		float deadPointDeg = ComputeJozzVehicleM6SteeringDeadPointDeg( config->wishbone, wheelbase,
																	   config->trackHalfWidth, config->rackHalfWidth );
		float safeMax = b3MaxFloat( 15.0f, deadPointDeg - 13.0f );
		clampField( &config->maxSteeringAngleDegrees, "maxSteeringAngleDegrees", 5.0f, safeMax );
	}

	// Wheel envelope. A torus whose shoulder radius reaches half the tire width
	// has no flat tread left, and one whose ring has fewer capsules than the
	// sealing minimum is a wheel with HOLES in it - the contact would drop into
	// the gaps once per capsule. Both are clamped loudly, like every other
	// field here, because a hand-edited file must not build a wheel that is
	// quietly different from the one it names.
	if ( config->wheelEnvelope.mode == JOZZ_M6_ENVELOPE_TORUS )
	{
		clampField( &config->wheelEnvelope.torusCrownRadius, "wheelEnvelope.torusCrownRadius", 0.005f,
					b3MaxFloat( 0.006f, 0.49f * config->wheelEnvelope.width ) );
		int minSegments = JozzVehicleM6MinTorusSegments( &config->wheelEnvelope );
		if ( config->wheelEnvelope.torusSegments < minSegments ||
			 config->wheelEnvelope.torusSegments > JOZZ_M6_MAX_WHEEL_SHAPES )
		{
			int fixed = b3ClampInt( config->wheelEnvelope.torusSegments, minSegments, JOZZ_M6_MAX_WHEEL_SHAPES );
			std::printf( "jozz m6 WARNING: config sanitized: wheelEnvelope.torusSegments %d -> %d "
						 "(sealed ring needs at least %d at crown %.3f m)\n",
						 config->wheelEnvelope.torusSegments, fixed, minSegments,
						 config->wheelEnvelope.torusCrownRadius );
			config->wheelEnvelope.torusSegments = fixed;
			changed = true;
		}
	}

	// Visual identity strings: this TU (JOZZ_VEHICLE_CORE_FILES) is shared with
	// the headless validator, which must NOT gain a dependency on the lab-only
	// body registry - so bodyVisualModel only gets a structural guard here
	// (NUL-terminated, [a-z0-9_] only); an unknown-but-well-formed key (e.g. one
	// that no longer exists in the registry) is the lab's job in
	// ApplyBodyVisualFromConfig, which DOES see the registry. There is no
	// separate registry for frontSuspensionVisualModel (only these two literal
	// values), so it is fully validated right here.
	auto sanitizeKeyChars = [&changed]( char* buf, size_t cap, const char* name ) {
		buf[cap - 1] = '\0';
		bool bad = false;
		size_t len = std::strlen( buf );
		for ( size_t i = 0; i < len; ++i )
		{
			char c = buf[i];
			bool ok = ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_';
			if ( ok == false )
			{
				bad = true;
				break;
			}
		}
		if ( bad || len == 0 )
		{
			std::printf( "jozz m6 WARNING: config sanitized: %s '%s' -> 'brak' (not a valid registry key)\n", name, buf );
			std::snprintf( buf, cap, "brak" );
			changed = true;
		}
	};
	sanitizeKeyChars( config->bodyVisualModel, sizeof( config->bodyVisualModel ), "bodyVisualModel" );

	config->frontSuspensionVisualModel[sizeof( config->frontSuspensionVisualModel ) - 1] = '\0';
	if ( std::strcmp( config->frontSuspensionVisualModel, "klasyczny" ) != 0 &&
		 std::strcmp( config->frontSuspensionVisualModel, "rig_kierowniczy" ) != 0 )
	{
		std::printf( "jozz m6 WARNING: config sanitized: frontSuspensionVisualModel '%s' -> 'klasyczny'\n",
					 config->frontSuspensionVisualModel );
		std::snprintf( config->frontSuspensionVisualModel, sizeof( config->frontSuspensionVisualModel ), "klasyczny" );
		changed = true;
	}

	clampField( &config->bodyVisualOffset.x, "bodyVisualOffset.x", -2.0f, 2.0f );
	clampField( &config->bodyVisualOffset.y, "bodyVisualOffset.y", -2.0f, 2.0f );
	clampField( &config->bodyVisualOffset.z, "bodyVisualOffset.z", -2.0f, 2.0f );

	return changed;
}

JozzVehicleM6Config JozzVehicleM6DefaultConfig( float wheelRadius, float wheelWidth, float suspensionTravelHint )
{
	JozzVehicleM6Config config = {};

	// Chassis identical to the validated M5 defaults.
	config.chassisHalfExtents = { 1.55f, 0.35f, 0.55f };
	config.chassisDensity = 200.0f;
	config.cgVerticalOffset = 0.15f;
	config.axleHalfSpacing = 1.25f;
	config.trackHalfWidth = 1.05f;
	config.restDrop = 0.55f;

	// The M6 lab exists to exercise the multi-body rig, so both axles default
	// to it; INTEGRATED_STRUT stays selectable as the cheap/validated option.
	config.frontRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;
	config.rearRigType = JOZZ_M6_RIG_DOUBLE_WISHBONE;

	config.wishbone.uprightHalfHeight = 0.18f;
	config.wishbone.kingpinOffset = 0.14f;
	config.wishbone.casterDeg = 5.0f;
	config.wishbone.kingpinInclinationDeg = 7.0f;
	config.wishbone.upperArmLength = 0.34f;
	config.wishbone.lowerArmLength = 0.46f;
	config.wishbone.armHalfSpread = 0.24f;
	config.wishbone.steeringArmBack = 0.17f;
	config.wishbone.ackermannTrapezoid = true;
	config.wishbone.ackermannFraction = 0.6f;
	config.wishbone.coiloverTopHeight = 0.42f;
	config.wishbone.coiloverTopInboard = 0.12f;
	config.wishbone.restArmDroopDeg = 15.0f; // wheels hang on drooping arms at rest

	config.trailingArm = JozzVehicleM6DefaultTrailingArmGeometry();

	// Real-world upright + hub + brake sits around 20-35 kg. Keeping the
	// knuckle reasonably heavy also keeps the constraint mass ratio against
	// the ~700 kg chassis inside what the iterative solver likes.
	config.knuckleMass = 28.0f;
	config.armMass = 5.0f;
	config.rackMass = 5.0f;
	config.rackHalfWidth = 0.45f;

	// Full steering stroke solved from the actual linkage geometry, so the
	// rack limit IS the steering-angle limit for the inner wheel.
	float maxAngleRadians = 32.0f * DEGREES_TO_RADIANS;
	config.rackTravel = ComputeJozzVehicleM6RackStroke( config.wishbone, 2.0f * config.axleHalfSpacing,
														config.trackHalfWidth, config.rackHalfWidth, maxAngleRadians );

	// Power-steering servo sizing: stationary loaded tires need ~700 N*m per
	// kingpin (the measured M5.1 parking-torque number); through the 0.17 m
	// steering arms on two wheels that is ~8.2 kN at the rack, plus headroom.
	config.rackServoForce = 12000.0f;
	config.rackServoSpeedGain = 12.0f;
	config.rackServoMaxSpeed = 1.2f;

	// The split envelope STAYS the default, and the reason is measured, not
	// cautious. TORUS is available and is the better rolling shape by every
	// bench number plus the owner's own verdict (2026-08-03: the hull wheels
	// "hop on flat ground - I want cars, not vibrators"; the torus is "smooth
	// in every condition, bumps included"). But switching the DEFAULT to it
	// takes the product validator from 18/18 to 15/18, and the three that fall
	// over are steering probes: the front end shimmies (4.66 deg sustained
	// oscillation after release, settles 3.96 deg off straight).
	//
	// The control run says this is NOT the capsule ring's fault. Same probe,
	// four envelopes:
	//   sphere          1 shape, point contact       0 red,  0.31 deg
	//   split (default) sphere on terrain            0 red,  0.31 deg
	//   torus-64        64 shapes, true width        3 red,  4.66 deg
	//   cylinder        ONE shape, true width        8 red,  6.71 deg
	// The single-shape cylinder is WORSE than the 64-capsule ring, and the
	// shimmy tracks the width of the flat tread (337/238/137/38 mm of tread
	// gives 6/6/5/3 failed probes). So the trigger is ground contact across a
	// REAL WIDTH, which the split envelope has never had - it meets terrain
	// with a sphere, i.e. a single point on the wheel's center plane.
	//
	// In other words the steering geometry was tuned against a point contact
	// and has never met a tire. Fixing that is steering work (caster, scrub
	// radius, steering damping, rack friction) and it changes how the car
	// feels, so it is the owner's call and a separate change - not something
	// to slip in underneath a wheel-shape swap (hard rule 3: one variable).
	config.wheelEnvelope.mode = JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL;
	config.wheelEnvelope.cylinderSides = 32;
	config.wheelEnvelope.unionLayerCount = 4;
	config.wheelEnvelope.radius = wheelRadius;
	config.wheelEnvelope.width = wheelWidth;
	config.wheelEnvelope.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;
	config.wheelEnvelope.torusSegments = 64;
	// Scaled from the width, not hard-coded in meters, so a different wheel
	// asset keeps the same cross-section proportions instead of silently
	// losing its flat tread. 0.914 * (width/2) is the bench's 0.20 m at the
	// current 0.4375 m wheel.
	config.wheelEnvelope.torusCrownRadius = 0.914f * 0.5f * wheelWidth;
	// Flat tread by default: a crown narrows the contact patch, and how the car
	// should drive is the owner's call, not a default I get to change quietly.
	config.wheelEnvelope.wheelCrownDrop = 0.0f;
	config.wheelEnvelope.wheelProfilePoints = 5;
	config.wheelDensity = 80.0f;
	config.wheelFriction = 1.25f;
	config.wheelRollingResistance = 0.02f;

	// Same reasoning as M5: the joint spring stiffness follows the constraint's
	// effective mass (dominated by the light unsprung side), so the hertz sits
	// above a real car's body frequency.
	config.suspensionHertz = 6.0f;
	config.suspensionDampingRatio = 0.7f;
	config.frontSuspensionScale = 1.0f;
	config.rearSuspensionScale = 1.0f;

	float travel = suspensionTravelHint > 0.05f ? suspensionTravelHint : 0.70f;
	config.reboundTravel = 0.4f * travel;
	config.compressionTravel = 0.6f * travel;
	config.suspensionPreloadFront = 0.07f; // holds the drooped design pose under static weight (toe ~0)
	config.suspensionPreloadRear = 0.07f;

	// Anti-roll bars replace the upright-assist crutch. Sizing sanity: ~0.8 g
	// lateral on this chassis transfers ~1.5 kN across an axle; a 0.1 m travel
	// split at these numbers contributes a comparable couple.
	config.arbFrontStiffness = 16000.0f;
	config.arbRearStiffness = 10000.0f;

	// Cd*A of a boxy off-roader. Makes top speed a drag-vs-torque balance.
	config.aeroDragArea = 0.9f;

	// Torque-based drive: the motor targets the rev limit and the throttle
	// scales available torque (taper above 60% revs). 320 N*m per wheel does
	// NOT break these tires loose (that takes ~1.2 kN*m at this load) - the
	// lab slider goes far enough for burnouts on purpose.
	config.maxDriveSpeed = 40.0f;
	config.maxDriveTorque = 320.0f;
	config.driveTaperStart = 0.6f;
	config.brakeTorque = 650.0f;
	config.coastTorque = 8.0f;
	config.allWheelDrive = true;

	config.maxSteeringAngleDegrees = 32.0f;
	config.frontToeDeg = 0.0f;
	config.rearToeDeg = 0.0f;
	config.steeringHertz = 14.0f;
	config.steeringDampingRatio = 1.0f;
	config.maxSteeringTorque = 700.0f;
	// Hands-off resistance: load-dependent since P4b (see the header field
	// comment for the model and why it replaced the flat static/kinetic
	// pair). Historical context kept for the record: the flat model needed
	// kinetic >= ~200 N to keep the 3.5 m landing stable (sharp cliff below
	// ~140 N: tie-rod branch-snap camber, plus a separate yaw-drift threshold
	// ~200 N), and that same floor parked the rack ~1 mm off-center in
	// ordinary driving (the diagnosed left-pull). The load-proportional term
	// resolves that tension physically: a landing loads the tie rods with kN
	// so friction spikes right when stability needs it, while near-straight
	// cruising leaves the rack nearly free. Defaults re-validated against the
	// same landing probe (see RunP4SteeringReturnProbe's comment).
	config.rackFrictionBase = 40.0f;
	config.rackFrictionLoadCoeff = 0.10f;
	config.steeringFrictionTorque = 40.0f;
	config.steerInputDeadzone = 0.02f;
	config.rackCenteringHertz = 0.0f; // OFF = realistic (no self-centering at rest); opt-in arcade assist
	config.ackermannGeometry = true;
	config.strutCasterDeg = 0.0f; // 0 = exact M5 strut behavior

	// M7: the honest mechanisms (ARB + geometry) carry the car; the world-
	// anchored upright spring stays available as a rescue toggle only.
	config.uprightAssist = false;
	config.uprightHertz = 0.4f;
	config.uprightDampingRatio = 1.0f;

	// Visual identity defaults - the validated game state (Etap 3, decyzje
	// Jozza D1/D2 2026-07-11): Jozz's tube frame on the chassis and the new
	// steering rig on the front axle. "brak"/"klasyczny" stay selectable in
	// the Nadwozie tab / Debug checkbox for bare-suspension work.
	std::snprintf( config.bodyVisualModel, sizeof( config.bodyVisualModel ), "rama_rurowa" );
	config.bodyVisualOffset = { 0.0f, 0.0f, 0.0f };
	std::snprintf( config.frontSuspensionVisualModel, sizeof( config.frontSuspensionVisualModel ), "rig_kierowniczy" );

	config.filterGroupIndex = -19;

	return config;
}
