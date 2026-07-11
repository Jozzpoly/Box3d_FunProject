// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_suspension_rig.h"

#include "jozz_vehicle_m5_vehicle.h"
#include "jozz_vehicle_m6_geometry.h"

#include <cmath>
#include <cstdio>

namespace
{

constexpr float DEGREES_TO_RADIANS = B3_PI / 180.0f;

bool IsFrontCorner( int corner )
{
	return corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
}

bool IsLeftCorner( int corner )
{
	return corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_REAR_LEFT;
}

// Corner offsets in chassis-local space. Forward is +X and LEFT is -Z (the
// M5.2 convention; see the M5 header for the sign-error history).
b3Vec3 CornerLocalOffset( const JozzVehicleM6Config& config, int corner )
{
	float x = IsFrontCorner( corner ) ? config.axleHalfSpacing : -config.axleHalfSpacing;
	float z = IsLeftCorner( corner ) ? -config.trackHalfWidth : config.trackHalfWidth;
	return { x, -config.restDrop, z };
}

float CornerSuspensionScale( const JozzVehicleM6Config& config, int corner )
{
	return IsFrontCorner( corner ) ? config.frontSuspensionScale : config.rearSuspensionScale;
}

int CornerRigType( const JozzVehicleM6Config& config, int corner )
{
	return IsFrontCorner( corner ) ? config.frontRigType : config.rearRigType;
}

float DistanceBetween( b3Vec3 a, b3Vec3 b )
{
	return b3Length( b3Sub( a, b ) );
}

// Rigid two-point rod between a chassis hardpoint and a knuckle hardpoint.
// enableSpring stays false: per the engine docs a non-spring distance joint
// is rigid, which is exactly what a control-arm link is.
b3JointId CreateLinkRod( b3WorldId worldId, b3BodyId chassisId, b3BodyId knuckleId, b3Vec3 chassisPointLocal,
						 b3Vec3 knucklePointLocal, b3Vec3 chassisPointOfKnuckleLocal )
{
	b3DistanceJointDef def = b3DefaultDistanceJointDef();
	def.base.bodyIdA = chassisId;
	def.base.bodyIdB = knuckleId;
	def.base.localFrameA.p = chassisPointLocal;
	def.base.localFrameB.p = knucklePointLocal;
	def.base.collideConnected = false;
	// Both bodies spawn with identity rotation, so the rest length is just the
	// chassis-local distance between the two hardpoints.
	def.length = DistanceBetween( chassisPointLocal, chassisPointOfKnuckleLocal );
	def.enableSpring = false;
	return b3CreateDistanceJoint( worldId, &def );
}

} // namespace

int CreateJozzVehicleM6WheelEnvelope( b3BodyId wheelBodyId, const b3ShapeDef* shapeDef,
									  const JozzVehicleM6WheelEnvelopeDesc* desc,
									  b3ShapeId outShapeIds[JOZZ_M6_MAX_WHEEL_SHAPES] )
{
	for ( int i = 0; i < JOZZ_M6_MAX_WHEEL_SHAPES; ++i )
	{
		outShapeIds[i] = b3_nullShapeId;
	}

	if ( desc->mode == JOZZ_M6_ENVELOPE_SPHERE ||
		 ( desc->mode == JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL && desc->terrainCategoryBits == 0 ) )
	{
		// The split mode degrades to the plain sphere when no terrain category
		// exists: a sphere that ignores the ground would be far worse than a
		// sphere that bulges laterally.
		b3Sphere sphere = { b3Vec3_zero, desc->radius };
		outShapeIds[0] = b3CreateSphereShape( wheelBodyId, shapeDef, &sphere );
		return 1;
	}

	int sides = b3ClampInt( desc->cylinderSides, 3, 32 );

	if ( desc->mode == JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL )
	{
		// Rolling sphere: terrain only, so the ride never feels hull facets.
		b3ShapeDef rollingDef = *shapeDef;
		rollingDef.filter.maskBits = desc->terrainCategoryBits;
		b3Sphere sphere = { b3Vec3_zero, desc->radius };
		outShapeIds[0] = b3CreateSphereShape( wheelBodyId, &rollingDef, &sphere );

		// Sidewall cylinder: everything except terrain, at the true tire
		// width. Density 0 keeps the wheel's mass identical to the sphere
		// wheel; the sphere already provides the mass and inertia.
		b3ShapeDef sidewallDef = *shapeDef;
		sidewallDef.filter.maskBits = ~desc->terrainCategoryBits;
		sidewallDef.density = 0.0f;
		b3HullData* hull = b3CreateCylinder( desc->width, desc->radius, -0.5f * desc->width, sides );
		outShapeIds[1] = b3CreateHullShape( wheelBodyId, &sidewallDef, hull );
		b3DestroyHull( hull );
		return 2;
	}

	if ( desc->mode == JOZZ_M6_ENVELOPE_CYLINDER )
	{
		// Centered on the body origin because the wheel frame is the wheel center.
		b3HullData* hull = b3CreateCylinder( desc->width, desc->radius, -0.5f * desc->width, sides );
		outShapeIds[0] = b3CreateHullShape( wheelBodyId, shapeDef, hull );
		b3DestroyHull( hull );
		return 1;
	}

	// PHASED_UNION: stack co-located cylinder hulls, each rotated by a fraction
	// of one facet angle about the spin axis. The union's rolling surface has
	// layerCount * sides effective facets while each individual hull stays
	// inside the engine's 32-side / 64-vertex hull limits. Density is divided
	// by the layer count so the wheel keeps roughly one cylinder's mass.
	int layerCount = b3ClampInt( desc->unionLayerCount, 2, JOZZ_M6_MAX_WHEEL_SHAPES );
	b3ShapeDef layerDef = *shapeDef;
	layerDef.density = shapeDef->density / (float)layerCount;

	float facetAngle = 2.0f * B3_PI / (float)sides;
	b3Vec3 points[64];

	for ( int layer = 0; layer < layerCount; ++layer )
	{
		float phase = facetAngle * (float)layer / (float)layerCount;
		for ( int i = 0; i < sides; ++i )
		{
			float angle = facetAngle * (float)i + phase;
			float x = desc->radius * std::cos( angle );
			float z = desc->radius * std::sin( angle );
			points[2 * i + 0] = { x, -0.5f * desc->width, z };
			points[2 * i + 1] = { x, 0.5f * desc->width, z };
		}

		b3HullData* hull = b3CreateHull( points, 2 * sides, 2 * sides );
		outShapeIds[layer] = b3CreateHullShape( wheelBodyId, &layerDef, hull );
		b3DestroyHull( hull );
	}

	return layerCount;
}

namespace
{

b3MassData MakeDiagonalMassData( float mass, b3Vec3 inertiaDiagonal, b3Vec3 center = b3Vec3_zero )
{
	b3MassData massData = {};
	massData.mass = mass;
	massData.center = center;
	massData.inertia.cx = { inertiaDiagonal.x, 0.0f, 0.0f };
	massData.inertia.cy = { 0.0f, inertiaDiagonal.y, 0.0f };
	massData.inertia.cz = { 0.0f, 0.0f, inertiaDiagonal.z };
	return massData;
}

// Swing allowance for a suspension hinge whose arm has to cover the wheel
// travel: asin(travel/armLength) plus 25% emergency margin. The coilover
// length limit remains the precise travel stop; the hinge limit is the
// anti-fold guard that makes a mirrored/collapsed configuration impossible.
float HingeSwingLimit( float compressionTravel, float reboundTravel, float armLength )
{
	float travel = b3MaxFloat( compressionTravel, reboundTravel );
	float sine = b3ClampFloat( 1.25f * travel / b3MaxFloat( armLength, 0.05f ), 0.05f, 0.95f );
	return b3MinFloat( std::asin( sine ), 55.0f * DEGREES_TO_RADIANS );
}

// Vertical raise applied to the steering-link inner pivots (front rack, rear toe
// link) so the link stays parallel to the DROOPED lower control arm. Without it,
// drooping the arms leaves the horizontal steering link crossing the arc at an
// angle - that is textbook bump steer (toe changes with travel) and it wrecks the
// static steering ratio. Matching the link's slope to the lower arm keeps toe
// stable through travel at any droop. lowerArmLength is the design link length.
float SteeringLinkDroopLift( const JozzVehicleM6Config& config )
{
	return config.wishbone.lowerArmLength * std::tan( config.wishbone.restArmDroopDeg * DEGREES_TO_RADIANS );
}

b3BodyId CreateKnuckleBody( b3WorldId worldId, const JozzVehicleM6Config& config, b3Pos restWheelCenterWorld )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = restWheelCenterWorld;
	bodyDef.name = "jozz_m6_knuckle";
	b3BodyId knuckleId = b3CreateBody( worldId, &bodyDef );

	// No collision shape AT ALL, mass set explicitly. This is deliberate, not
	// just tidy: the solver flags any dynamic body whose per-step motion
	// exceeds half its smallest shape extent as "fast" and runs continuous
	// collision on it (solver.c). A small helper shape on the knuckle turns
	// ordinary driving speed into permanent CCD against the huge static
	// ground, whose parallel-sweep geometry can starve the TOI root finder
	// (hard engine assert in debug). A shapeless body has no broadphase
	// presence, so none of that machinery ever runs. Inertia: solid sphere,
	// r = 0.2 m.
	float sphereInertia = 0.4f * config.knuckleMass * 0.2f * 0.2f;
	b3Body_SetMassData( knuckleId,
						MakeDiagonalMassData( config.knuckleMass, { sphereInertia, sphereInertia, sphereInertia } ) );

	return knuckleId;
}

b3BodyId CreateWheelBody( b3WorldId worldId, const JozzVehicleM6Config& config, b3Pos restWheelCenterWorld,
						  b3ShapeId outShapeIds[JOZZ_M6_MAX_WHEEL_SHAPES], int* outShapeCount )
{
	// Local Y rotated onto world Z so the axle runs across Z, like M5.
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = restWheelCenterWorld;
	bodyDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
	bodyDef.allowFastRotation = true;
	bodyDef.name = "jozz_m6_wheel";
	b3BodyId wheelId = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = config.wheelDensity;
	shapeDef.baseMaterial.friction = config.wheelFriction;
	shapeDef.baseMaterial.restitution = 0.02f;
	shapeDef.baseMaterial.rollingResistance = config.wheelRollingResistance;
	shapeDef.filter.groupIndex = config.filterGroupIndex;

	*outShapeCount = CreateJozzVehicleM6WheelEnvelope( wheelId, &shapeDef, &config.wheelEnvelope, outShapeIds );
	return wheelId;
}

void CreateStrutCorner( b3WorldId worldId, JozzVehicleM6* vehicle, int corner, b3Pos restWheelCenterWorld )
{
	const JozzVehicleM6Config& config = vehicle->config;
	JozzVehicleM6CornerRuntime& runtime = vehicle->corners[corner];

	runtime.wheelId =
		CreateWheelBody( worldId, config, restWheelCenterWorld, runtime.wheelShapeIds, &runtime.wheelShapeCount );

	// The validated M2.4/M5 rest-anchor model: frame A at the rest wheel
	// center on the chassis, frame B at the wheel origin, spring rest at
	// translation 0. strutCasterDeg tilts the travel/steer axis rearward
	// about chassis Z, which is how a real MacPherson strut gains caster.
	b3WheelJointDef jointDef = b3DefaultWheelJointDef();
	jointDef.base.bodyIdA = vehicle->chassisId;
	jointDef.base.bodyIdB = runtime.wheelId;
	jointDef.base.localFrameA.p = b3Body_GetLocalPoint( vehicle->chassisId, restWheelCenterWorld );
	jointDef.base.localFrameB.p = b3Vec3_zero;

	b3Quat travelUp = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
	if ( config.strutCasterDeg != 0.0f )
	{
		b3Quat casterTilt = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, config.strutCasterDeg * DEGREES_TO_RADIANS );
		travelUp = b3MulQuat( casterTilt, travelUp );
	}
	jointDef.base.localFrameA.q = travelUp;
	jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
	jointDef.base.collideConnected = false;

	float scale = CornerSuspensionScale( config, corner );
	jointDef.enableSuspensionSpring = true;
	jointDef.suspensionHertz = config.suspensionHertz * scale;
	jointDef.suspensionDampingRatio = config.suspensionDampingRatio * scale;
	jointDef.enableSuspensionLimit = true;
	jointDef.lowerSuspensionLimit = -config.reboundTravel;
	jointDef.upperSuspensionLimit = config.compressionTravel;

	jointDef.enableSpinMotor = true;
	jointDef.spinSpeed = 0.0f;
	jointDef.maxSpinTorque = 0.0f;

	if ( IsFrontCorner( corner ) )
	{
		float maxAngle = config.maxSteeringAngleDegrees * DEGREES_TO_RADIANS;
		jointDef.enableSteering = true;
		jointDef.steeringHertz = config.steeringHertz;
		jointDef.steeringDampingRatio = config.steeringDampingRatio;
		jointDef.targetSteeringAngle = 0.0f;
		jointDef.maxSteeringTorque = config.maxSteeringTorque;
		jointDef.enableSteeringLimit = true;
		jointDef.lowerSteeringLimit = -maxAngle;
		jointDef.upperSteeringLimit = maxAngle;
	}

	runtime.strutJointId = b3CreateWheelJoint( worldId, &jointDef );
}

// One control arm as a real hinged body. The hinge axis runs along chassis X
// through the two fore/aft chassis mount hardpoints; the arm body origin sits
// at the hinge midpoint so the revolute frames stay trivial. Angle limits are
// the physical droop/bump stops - the M6 rigid-rod arms had none, and a hard
// landing could snap the corner into the rods' mirrored solution branch.
b3BodyId CreateControlArm( b3WorldId worldId, JozzVehicleM6* vehicle, b3Vec3 frontMountLocal, b3Vec3 rearMountLocal,
						   b3Vec3 ballLocal, b3Vec3 knuckleOriginLocal, b3Quat ballFrameRotation, b3BodyId knuckleId,
						   b3Pos chassisSpawnPosition, float armLength, float twistLimitRadians, b3JointId* outHingeId,
						   b3JointId* outBallId )
{
	const JozzVehicleM6Config& config = vehicle->config;
	b3Vec3 hingeMidLocal = b3MulSV( 0.5f, b3Add( frontMountLocal, rearMountLocal ) );
	b3Vec3 ballFromHinge = b3Sub( ballLocal, hingeMidLocal );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = b3OffsetPos( chassisSpawnPosition, hingeMidLocal );
	bodyDef.name = "jozz_m6_control_arm";
	b3BodyId armId = b3CreateBody( worldId, &bodyDef );

	// Shapeless (the M6 CCD/TOI lesson), mass explicit: a flat plate spanning
	// the arm length across Z and the mount spread across X, COM mid-arm.
	{
		float length = b3MaxFloat( armLength, 0.10f );
		float spread = b3MaxFloat( 2.0f * config.wishbone.armHalfSpread, 0.10f );
		float mass = b3MaxFloat( config.armMass, 0.5f );
		b3Vec3 inertia = { mass * length * length / 12.0f, mass * ( length * length + spread * spread ) / 12.0f,
						   mass * spread * spread / 12.0f };
		b3Body_SetMassData( armId, MakeDiagonalMassData( mass, inertia, b3MulSV( 0.5f, ballFromHinge ) ) );
	}

	// Shared by the hinge limit and the ball-joint cone limit below, so the
	// two fences always agree on how far this arm can legally swing.
	float swing = HingeSwingLimit( config.compressionTravel, config.reboundTravel, armLength );

	// Hinge to the chassis: revolute about local frame Z mapped onto chassis X,
	// symmetric swing limits = droop/bump stops (the coilover length limit is
	// the precise stop; this one forbids folding).
	{
		b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
		def.base.bodyIdA = vehicle->chassisId;
		def.base.bodyIdB = armId;
		def.base.localFrameA.p = hingeMidLocal;
		def.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisX );
		def.base.localFrameB.p = b3Vec3_zero;
		def.base.localFrameB.q = def.base.localFrameA.q;
		def.base.collideConnected = false;
		def.enableLimit = true;
		def.lowerAngle = -swing;
		def.upperAngle = swing;
		*outHingeId = b3CreateRevoluteJoint( worldId, &def );
	}

	// Ball joint to the knuckle: spherical with wide cone/twist limits. These
	// never engage in normal motion (travel swings stay under ~55 deg, steering
	// under 32 deg) - they are pure anti-fold guards, the second fence behind
	// the hinge limits. Frames align local Z with the kingpin so the twist
	// limit bounds steering rotation. twistLimitRadians is set per-corner by
	// the caller (front = max steer + margin, rear = a small fixed fence) -
	// see the P1 fix note at the call site: this used to be a flat hardcoded
	// +-70 deg, well past the tie-rod linkage's own over-center dead point, so
	// a hard steering impact could push the knuckle past the dead point and
	// the rack would clamp it there with no way back (audit
	// AUDIT_PHYSICS_STEERING_2026_07_08_PL.md).
	{
		b3SphericalJointDef def = b3DefaultSphericalJointDef();
		def.base.bodyIdA = armId;
		def.base.bodyIdB = knuckleId;
		def.base.localFrameA.p = ballFromHinge;
		def.base.localFrameA.q = ballFrameRotation;
		def.base.localFrameB.p = b3Sub( ballLocal, knuckleOriginLocal );
		def.base.localFrameB.q = ballFrameRotation;
		def.base.collideConnected = false;
		def.enableConeLimit = true;
		// P6 (audit S5): derived from the hinge swing range + margin instead of
		// a flat 80 deg, so the second fence tracks the geometry the same way
		// the first one does. Suspension travel deflects the cone (the arm
		// rotates about the hinge while the knuckle's kingpin stays near
		// vertical); steering deflects twist, not cone - so swing + margin is
		// the honest bound.
		def.coneAngle = swing + 15.0f * DEGREES_TO_RADIANS;
		def.enableTwistLimit = true;
		def.lowerTwistAngle = -twistLimitRadians;
		def.upperTwistAngle = twistLimitRadians;
		*outBallId = b3CreateSphericalJoint( worldId, &def );
	}

	return armId;
}

// Static toe (P5): a virtual turnbuckle. Shortening/lengthening the tie-rod
// (front) or toe-link (rear) by this delta - WITHOUT moving any hardpoint -
// forces the knuckle to rotate until its (unchanged) steeringArm attachment
// again satisfies the (changed) link length, exactly like screwing a real
// turnbuckle. Moving hp.steeringArm itself instead was tried and rejected:
// both ends of the rear toe-link are defined relative to hp.steeringArm, so a
// hardpoint shift cancels out and induces NO rotation at all (verified by
// derivation, not just assumed - the front tie-rod's chassis end is
// independent of hp.steeringArm so it would have worked there, but using two
// different mechanisms for front/rear invited exactly the kind of asymmetry
// bug this project has been bitten by before).
//
// Magnitude (recalibrated 2026-07-09, audit A4): the first version used the
// small-angle arc length `steeringArmBack * toeRad`, but steeringArmBack is
// NOT the arm's lever about the kingpin (the kingpin is offset and tilted by
// caster/KPI, and the Ackermann trapezoid angles the arm inward), so the dial
// over-steered by ~43% (commanded 1 deg -> measured 1.43 deg). Now computed
// EXACTLY: rotate the steering-arm attachment about the corner's own kingpin
// axis by the commanded toe angle and measure the link rest length to THAT
// point - the knuckle's equilibrium is then the exact commanded rotation (all
// its other constraints - both ball joints - sit ON the axis and don't move).
//
// Sign convention (kept from the calibrated-by-measurement first version):
// positive toeDeg = toe-IN, both noses toward the chassis centerline. With
// positive steer = LEFT (+X toward -Z about +Y), toe-in means the left wheel
// (at -Z) yaws negative and the right wheel positive - the per-side flip here
// defines that target POSE; the mirrored hardpoint geometry then yields the
// mirrored (equal) length change per side, matching the measured behavior of
// the original (same lengthening on both sides converges the noses).
b3Vec3 SteeringArmWithToe( const JozzVehicleM6Config& config, int corner, const JozzVehicleM6WishboneHardpoints& hp )
{
	float toeDeg = IsFrontCorner( corner ) ? config.frontToeDeg : config.rearToeDeg;
	if ( toeDeg == 0.0f )
	{
		return hp.steeringArm;
	}
	float yaw = ( IsLeftCorner( corner ) ? -1.0f : 1.0f ) * toeDeg * DEGREES_TO_RADIANS;
	b3Vec3 axisPoint = hp.lowerBallJoint;
	b3Vec3 axisDirection = b3Normalize( b3Sub( hp.upperBallJoint, hp.lowerBallJoint ) );
	b3Quat rotation = b3MakeQuatFromAxisAngle( axisDirection, yaw );
	return b3Add( axisPoint, b3RotateVector( rotation, b3Sub( hp.steeringArm, axisPoint ) ) );
}

void CreateWishboneCorner( b3WorldId worldId, JozzVehicleM6* vehicle, int corner, b3Vec3 restWheelCenterLocal,
						   b3Pos restWheelCenterWorld, b3Pos chassisSpawnPosition )
{
	const JozzVehicleM6Config& config = vehicle->config;
	JozzVehicleM6CornerRuntime& runtime = vehicle->corners[corner];

	float wheelbase = 2.0f * config.axleHalfSpacing;
	runtime.hardpoints = JozzVehicleM6MakeWishboneHardpoints( config.wishbone, restWheelCenterLocal,
															  IsLeftCorner( corner ), wheelbase, config.trackHalfWidth );
	const JozzVehicleM6WishboneHardpoints& hp = runtime.hardpoints;

	runtime.knuckleId = CreateKnuckleBody( worldId, config, restWheelCenterWorld );
	runtime.wheelId =
		CreateWheelBody( worldId, config, restWheelCenterWorld, runtime.wheelShapeIds, &runtime.wheelShapeCount );

	// The knuckle body origin spawns at the rest wheel center with chassis
	// orientation, so knuckle-local hardpoints are chassis-local minus the
	// wheel center.
	b3Vec3 steeringArmKnuckle = b3Sub( hp.steeringArm, restWheelCenterLocal );
	b3Vec3 coiloverKnuckle = b3Sub( hp.coiloverKnuckle, restWheelCenterLocal );

	// Ball-joint frames share the kingpin direction so the spherical twist
	// limit bounds rotation about the steering axis.
	b3Vec3 kingpinDirection = b3Normalize( b3Sub( hp.upperBallJoint, hp.lowerBallJoint ) );
	b3Quat kingpinFrame = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, kingpinDirection );

	// P1 fix: the ball-joint twist limit must sit just past the tie-rod
	// linkage's own over-center dead point, not at a flat +-70 deg that let a
	// hard steering impact push the knuckle past that dead point with no way
	// back (see AUDIT_PHYSICS_STEERING_2026_07_08_PL.md). Front corners steer;
	// their fence tracks the configured max steering angle plus a margin.
	// Rear corners do not steer at all, so a small fixed fence is enough and
	// stays far from the dead point regardless of front steering config.
	float twistFence = IsFrontCorner( corner ) ? ( config.maxSteeringAngleDegrees + 10.0f ) * DEGREES_TO_RADIANS
											   : 15.0f * DEGREES_TO_RADIANS;

	runtime.upperArmId =
		CreateControlArm( worldId, vehicle, hp.upperFrontChassis, hp.upperRearChassis, hp.upperBallJoint,
						  restWheelCenterLocal, kingpinFrame, runtime.knuckleId, chassisSpawnPosition,
						  config.wishbone.upperArmLength, twistFence, &runtime.upperHingeId, &runtime.upperBallId );
	runtime.lowerArmId =
		CreateControlArm( worldId, vehicle, hp.lowerFrontChassis, hp.lowerRearChassis, hp.lowerBallJoint,
						  restWheelCenterLocal, kingpinFrame, runtime.knuckleId, chassisSpawnPosition,
						  config.wishbone.lowerArmLength, twistFence, &runtime.lowerHingeId, &runtime.lowerBallId );

	// Coilover: the only compliant chassis<->knuckle connection. Spring rest
	// at the authored pose, travel limits mapped onto rod length.
	{
		float scale = CornerSuspensionScale( config, corner );
		// designLength = coilover length at the authored (drooped) pose. The
		// spring rest is preloaded above it so static weight settles back at the
		// design pose; the travel stops stay measured from the design length.
		// Preload is per-axle (front/rear) but deliberately NOT scaled by the
		// stiffness multiplier - see the field comment in the header (P3 fix).
		float designLength = DistanceBetween( hp.coiloverChassis, hp.coiloverKnuckle );
		runtime.coiloverDesignLength = designLength;
		float preload = IsFrontCorner( corner ) ? config.suspensionPreloadFront : config.suspensionPreloadRear;
		float restLength = designLength + preload;

		b3DistanceJointDef def = b3DefaultDistanceJointDef();
		def.base.bodyIdA = vehicle->chassisId;
		def.base.bodyIdB = runtime.knuckleId;
		def.base.localFrameA.p = hp.coiloverChassis;
		def.base.localFrameB.p = coiloverKnuckle;
		def.base.collideConnected = false;
		def.length = restLength;
		def.enableSpring = true;
		def.hertz = config.suspensionHertz * scale;
		def.dampingRatio = config.suspensionDampingRatio * scale;
		// Wheel compression shortens the coilover; rebound stretches it.
		def.enableLimit = true;
		def.minLength = b3MaxFloat( 0.05f, designLength - config.compressionTravel );
		def.maxLength = designLength + config.reboundTravel;
		runtime.coiloverJointId = b3CreateDistanceJoint( worldId, &def );
	}

	// Fifth link: front corners connect the steering arm to the rack (created
	// before the corners), rear corners get a fixed toe link to the chassis.
	if ( IsFrontCorner( corner ) && B3_IS_NON_NULL( vehicle->rackId ) )
	{
		float rackEndZ = IsLeftCorner( corner ) ? -config.rackHalfWidth : config.rackHalfWidth;
		b3Vec3 rackEndLocal = { 0.0f, 0.0f, rackEndZ };
		// Rack rest position in chassis space, for the rest length.
		b3Vec3 rackRestLocal = {
			config.axleHalfSpacing - config.wishbone.steeringArmBack, -config.restDrop + SteeringLinkDroopLift( config ), 0.0f };
		b3Vec3 rackEndChassisLocal = b3Add( rackRestLocal, rackEndLocal );

		b3DistanceJointDef def = b3DefaultDistanceJointDef();
		def.base.bodyIdA = vehicle->rackId;
		def.base.bodyIdB = runtime.knuckleId;
		def.base.localFrameA.p = rackEndLocal;
		def.base.localFrameB.p = steeringArmKnuckle;
		def.base.collideConnected = false;
		def.length = DistanceBetween( rackEndChassisLocal, SteeringArmWithToe( config, corner, hp ) );
		def.enableSpring = false;
		runtime.steerLinkJointId = b3CreateDistanceJoint( worldId, &def );
	}
	else
	{
		// Toe link: lower-arm length AND lifted by the same droop as the lower arm,
		// so it stays parallel to it and bump steer stays small at any droop angle.
		float in = IsLeftCorner( corner ) ? 1.0f : -1.0f;
		b3Vec3 toeChassis =
			b3Add( hp.steeringArm, { 0.0f, SteeringLinkDroopLift( config ), in * config.wishbone.lowerArmLength } );

		b3DistanceJointDef def = b3DefaultDistanceJointDef();
		def.base.bodyIdA = vehicle->chassisId;
		def.base.bodyIdB = runtime.knuckleId;
		def.base.localFrameA.p = toeChassis;
		def.base.localFrameB.p = steeringArmKnuckle;
		def.base.collideConnected = false;
		def.length = DistanceBetween( toeChassis, SteeringArmWithToe( config, corner, hp ) );
		def.enableSpring = false;
		runtime.steerLinkJointId = b3CreateDistanceJoint( worldId, &def );
	}

	// Wheel spins on the knuckle. Frame A z-axis is the knuckle's Z (the axle
	// direction at rest), frame B maps the wheel body's local Y spin axis onto
	// it, mirroring the M5 wheel-joint frame convention.
	{
		b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
		def.base.bodyIdA = runtime.knuckleId;
		def.base.bodyIdB = runtime.wheelId;
		def.base.localFrameA.p = b3Vec3_zero;
		def.base.localFrameA.q = b3Quat_identity;
		def.base.localFrameB.p = b3Vec3_zero;
		def.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		def.base.collideConnected = false;
		def.enableMotor = true;
		def.maxMotorTorque = 0.0f;
		def.motorSpeed = 0.0f;
		runtime.spinJointId = b3CreateRevoluteJoint( worldId, &def );
	}
}

// Trailing-arm corner (Jozz's One_Sided_wheel_mount): one arm body hinged to
// the chassis about the lateral Z axis, the wheel spinning on the arm, and a
// coilover between the contract damper points. No steering DOF.
void CreateTrailingArmCorner( b3WorldId worldId, JozzVehicleM6* vehicle, int corner, b3Vec3 restWheelCenterLocal,
							  b3Pos restWheelCenterWorld, b3Pos chassisSpawnPosition )
{
	const JozzVehicleM6Config& config = vehicle->config;
	JozzVehicleM6CornerRuntime& runtime = vehicle->corners[corner];
	const JozzVehicleM6TrailingArmGeometry& geometry = config.trailingArm;

	// Geometry offsets are authored for the LEFT corner; mirror Z on the right.
	float zMirror = IsLeftCorner( corner ) ? 1.0f : -1.0f;
	b3Vec3 pivotOffset = { geometry.pivotOffset.x, geometry.pivotOffset.y, zMirror * geometry.pivotOffset.z };
	b3Vec3 damperArmOffset = { geometry.damperArmOffset.x, geometry.damperArmOffset.y,
							   zMirror * geometry.damperArmOffset.z };
	b3Vec3 damperChassisOffset = { geometry.damperChassisOffset.x, geometry.damperChassisOffset.y,
								   zMirror * geometry.damperChassisOffset.z };

	runtime.trailingPivotLocal = b3Add( restWheelCenterLocal, pivotOffset );
	runtime.trailingDamperArmLocal = b3Add( restWheelCenterLocal, damperArmOffset );
	runtime.trailingDamperChassisLocal = b3Add( restWheelCenterLocal, damperChassisOffset );

	b3Vec3 wheelFromPivot = b3Sub( restWheelCenterLocal, runtime.trailingPivotLocal );
	float armLength = b3MaxFloat( b3Length( wheelFromPivot ), 0.15f );

	// Arm body, origin at the hinge. Shapeless + explicit mass (CCD lesson):
	// a slender rod from the pivot to the wheel center, COM mid-arm.
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = b3OffsetPos( chassisSpawnPosition, runtime.trailingPivotLocal );
		bodyDef.name = "jozz_m6_trailing_arm";
		b3BodyId armId = b3CreateBody( worldId, &bodyDef );

		float mass = b3MaxFloat( geometry.armMass, 1.0f );
		float rodInertia = mass * armLength * armLength / 12.0f;
		b3Vec3 inertia = { 0.2f * rodInertia + 0.001f * mass, rodInertia, rodInertia };
		b3Body_SetMassData( armId, MakeDiagonalMassData( mass, inertia, b3MulSV( 0.5f, wheelFromPivot ) ) );
		runtime.trailingArmId = armId;
	}

	runtime.wheelId =
		CreateWheelBody( worldId, config, restWheelCenterWorld, runtime.wheelShapeIds, &runtime.wheelShapeCount );

	// Hinge about chassis Z (revolute already rotates about local frame Z),
	// symmetric swing limits from the wheel travel = physical stops.
	{
		float planarArm = b3MaxFloat( std::sqrt( wheelFromPivot.x * wheelFromPivot.x + wheelFromPivot.y * wheelFromPivot.y ),
									  0.10f );
		float swing = HingeSwingLimit( config.compressionTravel, config.reboundTravel, planarArm );

		b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
		def.base.bodyIdA = vehicle->chassisId;
		def.base.bodyIdB = runtime.trailingArmId;
		def.base.localFrameA.p = runtime.trailingPivotLocal;
		def.base.localFrameA.q = b3Quat_identity;
		def.base.localFrameB.p = b3Vec3_zero;
		def.base.localFrameB.q = b3Quat_identity;
		def.base.collideConnected = false;
		def.enableLimit = true;
		def.lowerAngle = -swing;
		def.upperAngle = swing;
		runtime.armHingeId = b3CreateRevoluteJoint( worldId, &def );
	}

	// Coilover between the contract damper points, spring rest at the authored
	// pose. Two rig-specific corrections make the config hertz MEAN the same
	// wheel rate it means on the other rig types (the M5 lesson: a joint
	// spring's stiffness follows the constraint's effective mass, and here the
	// constraint grabs a slender rotating arm whose effective mass at the
	// damper eye is a few kilograms - naively reusing the config hertz left
	// the rear parked ON its bump stops):
	//   1. motion ratio: damper length change per meter of vertical wheel
	//      travel, from the rest geometry; scales the travel limits so they
	//      stop the WHEEL at the configured travel, and enters the stiffness.
	//   2. effective-mass compensation: the damper hertz is recomputed so the
	//      resulting WHEEL rate equals unsprungMass * (2*pi*configHertz)^2,
	//      the same definition the strut and wishbone corners effectively use.
	{
		float scale = CornerSuspensionScale( config, corner );
		float restLength = DistanceBetween( runtime.trailingDamperChassisLocal, runtime.trailingDamperArmLocal );
		if ( restLength < 0.08f )
		{
			// Degenerate contract data: fall back to a vertical strut above the
			// wheel center so the corner still carries the car.
			runtime.trailingDamperChassisLocal = b3Add( restWheelCenterLocal, { 0.0f, 0.5f, 0.0f } );
			runtime.trailingDamperArmLocal = restWheelCenterLocal;
			restLength = 0.5f;
		}

		// Motion ratio at rest: rotate the arm virtually about the hinge (Z)
		// and compare the damper-eye speed along the damper axis with the
		// wheel center's vertical speed.
		b3Vec3 damperRadius = b3Sub( runtime.trailingDamperArmLocal, runtime.trailingPivotLocal );
		b3Vec3 damperEyeVelocity = { -damperRadius.y, damperRadius.x, 0.0f }; // z-hat cross r
		b3Vec3 damperAxis = b3Normalize( b3Sub( runtime.trailingDamperChassisLocal, runtime.trailingDamperArmLocal ) );
		float damperRate = std::fabs( b3Dot( damperEyeVelocity, damperAxis ) );
		float wheelVerticalRate = b3MaxFloat( std::fabs( wheelFromPivot.x ), 0.05f );
		float motionRatio = b3ClampFloat( damperRate / wheelVerticalRate, 0.05f, 5.0f );

		// Effective mass of the distance constraint on the arm side, from the
		// exact mass data this function just set: 1/m + (r x u)^T I^-1 (r x u).
		float armMass = b3MaxFloat( geometry.armMass, 1.0f );
		float rodInertia = armMass * armLength * armLength / 12.0f;
		b3Vec3 inertiaDiagonal = { 0.2f * rodInertia + 0.001f * armMass, rodInertia, rodInertia };
		b3Vec3 centerOffset = b3MulSV( 0.5f, wheelFromPivot );
		b3Vec3 anchorFromCom = b3Sub( damperRadius, centerOffset );
		b3Vec3 rCrossU = b3Cross( anchorFromCom, damperAxis );
		float invEffectiveMass = 1.0f / armMass + rCrossU.x * rCrossU.x / inertiaDiagonal.x +
								 rCrossU.y * rCrossU.y / inertiaDiagonal.y + rCrossU.z * rCrossU.z / inertiaDiagonal.z;

		// Wheel-rate target on the same terms as the other rigs, mapped to the
		// damper through the motion ratio, then expressed as the hertz the
		// solver needs on THIS constraint's effective mass.
		float configOmega = 2.0f * B3_PI * config.suspensionHertz * scale;
		float unsprungMass = b3Body_GetMass( runtime.wheelId ) + armMass;
		float wheelRateTarget = unsprungMass * configOmega * configOmega;
		float damperRateTarget = wheelRateTarget / ( motionRatio * motionRatio );
		float damperOmega = std::sqrt( damperRateTarget * invEffectiveMass );
		float damperHertz = b3ClampFloat( damperOmega / ( 2.0f * B3_PI ), 0.5f, 60.0f );
		runtime.trailingCoiloverHertzScale = damperHertz / b3MaxFloat( config.suspensionHertz * scale, 0.01f );
		runtime.trailingMotionRatio = motionRatio;
		runtime.coiloverDesignLength = restLength;

		// Ride height preload is a wheel-space (vertical) rise; the motion ratio
		// maps it onto the damper's own axis, same as the travel stops below.
		// Per-axle, NOT scaled by stiffness (P3 fix - see header field comment).
		float preload = IsFrontCorner( corner ) ? config.suspensionPreloadFront : config.suspensionPreloadRear;
		float preloadedLength = restLength + preload * motionRatio;

		b3DistanceJointDef def = b3DefaultDistanceJointDef();
		def.base.bodyIdA = vehicle->chassisId;
		def.base.bodyIdB = runtime.trailingArmId;
		def.base.localFrameA.p = runtime.trailingDamperChassisLocal;
		def.base.localFrameB.p = b3Sub( runtime.trailingDamperArmLocal, runtime.trailingPivotLocal );
		def.base.collideConnected = false;
		def.length = preloadedLength;
		def.enableSpring = true;
		def.hertz = damperHertz;
		def.dampingRatio = config.suspensionDampingRatio * scale;
		def.enableLimit = true;
		def.minLength = b3MaxFloat( 0.05f, restLength - config.compressionTravel * motionRatio );
		def.maxLength = restLength + config.reboundTravel * motionRatio;
		runtime.coiloverJointId = b3CreateDistanceJoint( worldId, &def );
	}

	// Wheel spins on the arm; same frame convention as the knuckle spin joint.
	{
		b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
		def.base.bodyIdA = runtime.trailingArmId;
		def.base.bodyIdB = runtime.wheelId;
		def.base.localFrameA.p = wheelFromPivot;
		def.base.localFrameA.q = b3Quat_identity;
		def.base.localFrameB.p = b3Vec3_zero;
		def.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		def.base.collideConnected = false;
		def.enableMotor = true;
		def.maxMotorTorque = 0.0f;
		def.motorSpeed = 0.0f;
		runtime.spinJointId = b3CreateRevoluteJoint( worldId, &def );
	}
}

} // namespace

JozzVehicleM6 CreateJozzVehicleM6( b3WorldId worldId, b3BodyId groundBodyId, const JozzVehicleM6Config& config,
								   b3Pos chassisSpawnPosition )
{
	JozzVehicleM6 vehicle = {};
	vehicle.config = config;
	vehicle.rackId = b3_nullBodyId;
	vehicle.rackJointId = b3_nullJointId;
	vehicle.uprightJointId = b3_nullJointId;

	// Chassis: one dynamic box with the CG dropped below the origin, same as M5.
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = chassisSpawnPosition;
		bodyDef.name = "jozz_m6_chassis";
		vehicle.chassisId = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = config.chassisDensity;
		shapeDef.baseMaterial.friction = 0.6f;
		shapeDef.filter.groupIndex = config.filterGroupIndex;

		b3BoxHull box = b3MakeOffsetBoxHull( config.chassisHalfExtents.x, config.chassisHalfExtents.y,
											 config.chassisHalfExtents.z, { 0.0f, -config.cgVerticalOffset, 0.0f } );
		vehicle.chassisShapeId = b3CreateHullShape( vehicle.chassisId, &shapeDef, &box.base );
	}

	// Physical steering rack, only when the front axle is multi-body. The rack
	// slides across Z on a prismatic joint; its position spring is the "power
	// steering" servo and the tie rods do the actual coupling, so a blocked
	// wheel genuinely holds the whole linkage back.
	if ( config.frontRigType == JOZZ_M6_RIG_DOUBLE_WISHBONE )
	{
		b3Vec3 rackRestLocal = {
			config.axleHalfSpacing - config.wishbone.steeringArmBack, -config.restDrop + SteeringLinkDroopLift( config ), 0.0f };
		b3Pos rackRestWorld = b3Body_GetWorldPoint( vehicle.chassisId, rackRestLocal );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = rackRestWorld;
		bodyDef.name = "jozz_m6_rack";
		vehicle.rackId = b3CreateBody( worldId, &bodyDef );

		// Shapeless for the same CCD reason as the knuckle (see
		// CreateKnuckleBody): a 3.5 cm box moving at driving speed would be
		// flagged "fast" every step and swept against the world. Inertia: a
		// slender rod across Z.
		float rodLength = 2.0f * config.rackHalfWidth;
		float rodInertia = config.rackMass * rodLength * rodLength / 12.0f;
		b3Body_SetMassData( vehicle.rackId,
							MakeDiagonalMassData( config.rackMass, { rodInertia, rodInertia, 0.002f * config.rackMass } ) );

		b3PrismaticJointDef jointDef = b3DefaultPrismaticJointDef();
		jointDef.base.bodyIdA = vehicle.chassisId;
		jointDef.base.bodyIdB = vehicle.rackId;
		jointDef.base.localFrameA.p = rackRestLocal;
		jointDef.base.localFrameB.p = b3Vec3_zero;
		// Prismatic slides along frame A local X; map it onto chassis Z.
		jointDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisZ );
		jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisZ );
		jointDef.base.collideConnected = false;
		jointDef.enableSpring = true;
		jointDef.hertz = config.steeringHertz;
		jointDef.dampingRatio = config.steeringDampingRatio;
		jointDef.targetTranslation = 0.0f;
		jointDef.enableLimit = true;
		jointDef.lowerTranslation = -config.rackTravel;
		jointDef.upperTranslation = config.rackTravel;

		// Tripwire: rackTravel is derived from steering geometry and must be
		// recomputed by the caller (RecomputeRackTravel) whenever that geometry
		// changes. This catches any future load/apply path that forgets to, by
		// comparing against a fresh computation from the config actually being
		// built here. Print-only (not an assert) - the validator reads stdout.
		{
			float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
			float freshRackTravel = ComputeJozzVehicleM6RackStroke( config.wishbone, 2.0f * config.axleHalfSpacing,
																	 config.trackHalfWidth, config.rackHalfWidth, maxAngle );
			if ( std::fabs( freshRackTravel - config.rackTravel ) > 1.0e-4f )
			{
				std::printf( "jozz m6 WARNING: stale rackTravel %.4f vs %.4f\n", config.rackTravel, freshRackTravel );
			}
		}
		// The servo motor supplies the parking-torque muscle the position
		// spring cannot (see rackServoForce in the header); the drive update
		// steers its speed toward the target every step.
		jointDef.enableMotor = true;
		jointDef.motorSpeed = 0.0f;
		jointDef.maxMotorForce = config.rackServoForce;
		vehicle.rackJointId = b3CreatePrismaticJoint( worldId, &jointDef );
	}

	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
		runtime = {};
		runtime.rigType = CornerRigType( config, corner );
		runtime.knuckleId = b3_nullBodyId;
		runtime.upperArmId = b3_nullBodyId;
		runtime.lowerArmId = b3_nullBodyId;
		runtime.trailingArmId = b3_nullBodyId;
		runtime.wheelId = b3_nullBodyId;
		runtime.strutJointId = b3_nullJointId;
		runtime.spinJointId = b3_nullJointId;
		runtime.upperHingeId = b3_nullJointId;
		runtime.lowerHingeId = b3_nullJointId;
		runtime.upperBallId = b3_nullJointId;
		runtime.lowerBallId = b3_nullJointId;
		runtime.armHingeId = b3_nullJointId;
		runtime.coiloverJointId = b3_nullJointId;
		runtime.steerLinkJointId = b3_nullJointId;
		runtime.trailingCoiloverHertzScale = 1.0f;
		runtime.trailingMotionRatio = 1.0f;
		for ( int i = 0; i < JOZZ_M6_MAX_WHEEL_SHAPES; ++i )
		{
			runtime.wheelShapeIds[i] = b3_nullShapeId;
		}

		b3Vec3 localOffset = CornerLocalOffset( config, corner );
		runtime.restWheelCenterLocal = localOffset;
		b3Pos restWheelCenterWorld = b3OffsetPos( chassisSpawnPosition, localOffset );

		if ( runtime.rigType == JOZZ_M6_RIG_DOUBLE_WISHBONE )
		{
			CreateWishboneCorner( worldId, &vehicle, corner, localOffset, restWheelCenterWorld, chassisSpawnPosition );
		}
		else if ( runtime.rigType == JOZZ_M6_RIG_TRAILING_ARM )
		{
			CreateTrailingArmCorner( worldId, &vehicle, corner, localOffset, restWheelCenterWorld, chassisSpawnPosition );
		}
		else
		{
			CreateStrutCorner( worldId, &vehicle, corner, restWheelCenterWorld );
		}
	}

	if ( config.uprightAssist && B3_IS_NON_NULL( groundBodyId ) )
	{
		b3ParallelJointDef parallelDef = b3DefaultParallelJointDef();
		parallelDef.base.bodyIdA = groundBodyId;
		parallelDef.base.bodyIdB = vehicle.chassisId;
		parallelDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		parallelDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		parallelDef.base.collideConnected = true;
		parallelDef.hertz = config.uprightHertz;
		parallelDef.dampingRatio = config.uprightDampingRatio;
		vehicle.uprightJointId = b3CreateParallelJoint( worldId, &parallelDef );
	}

	vehicle.valid = true;
	return vehicle;
}

void DestroyJozzVehicleM6( JozzVehicleM6* vehicle )
{
	if ( vehicle->valid == false )
	{
		return;
	}

	if ( B3_IS_NON_NULL( vehicle->uprightJointId ) )
	{
		b3DestroyJoint( vehicle->uprightJointId, false );
		vehicle->uprightJointId = b3_nullJointId;
	}

	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		JozzVehicleM6CornerRuntime& runtime = vehicle->corners[corner];

		b3JointId* cornerJointIds[9] = { &runtime.strutJointId, &runtime.spinJointId,  &runtime.coiloverJointId,
										 &runtime.steerLinkJointId, &runtime.upperHingeId, &runtime.lowerHingeId,
										 &runtime.upperBallId,		&runtime.lowerBallId,  &runtime.armHingeId };
		for ( b3JointId* jointIdSlot : cornerJointIds )
		{
			b3JointId jointId = *jointIdSlot;
			if ( B3_IS_NON_NULL( jointId ) )
			{
				b3DestroyJoint( jointId, false );
			}
			*jointIdSlot = b3_nullJointId;
		}

		b3BodyId* cornerBodyIds[5] = { &runtime.wheelId, &runtime.knuckleId, &runtime.upperArmId, &runtime.lowerArmId,
									   &runtime.trailingArmId };
		for ( b3BodyId* bodyIdSlot : cornerBodyIds )
		{
			b3BodyId bodyId = *bodyIdSlot;
			if ( B3_IS_NON_NULL( bodyId ) )
			{
				b3DestroyBody( bodyId );
			}
			*bodyIdSlot = b3_nullBodyId;
		}
		for ( int i = 0; i < JOZZ_M6_MAX_WHEEL_SHAPES; ++i )
		{
			runtime.wheelShapeIds[i] = b3_nullShapeId;
		}
		runtime.wheelShapeCount = 0;
	}

	if ( B3_IS_NON_NULL( vehicle->rackJointId ) )
	{
		b3DestroyJoint( vehicle->rackJointId, false );
		vehicle->rackJointId = b3_nullJointId;
	}
	if ( B3_IS_NON_NULL( vehicle->rackId ) )
	{
		b3DestroyBody( vehicle->rackId );
		vehicle->rackId = b3_nullBodyId;
	}

	if ( B3_IS_NON_NULL( vehicle->chassisId ) )
	{
		b3DestroyBody( vehicle->chassisId );
		vehicle->chassisId = b3_nullBodyId;
	}

	vehicle->valid = false;
}

float ComputeJozzVehicleM6RackAngle( const JozzVehicleM6Config& config, float steerInput )
{
	// Pure input mapping. The M6 version blended this command toward the
	// travel direction during slides; that software alignment is gone (it
	// read as scripted drift). Self-alignment now happens physically: with
	// the input released the rack spring/servo let go (see the drive update)
	// and the caster trail back-drives the steering through the tie rods.
	float maxAngle = config.maxSteeringAngleDegrees * DEGREES_TO_RADIANS;
	return maxAngle * b3ClampFloat( steerInput, -1.0f, 1.0f );
}

namespace
{

// Available drive torque at the current wheel speed: full torque up to
// driveTaperStart * maxDriveSpeed, then a linear taper to zero at the rev
// limit. Only spin IN the commanded direction counts against the taper, so
// torque stays available to slow a wheel spinning the wrong way.
float TaperedDriveTorque( const JozzVehicleM6Config& config, float spinSpeed, float commandedSpinSpeed, float driveInput )
{
	float commandSign = commandedSpinSpeed >= 0.0f ? 1.0f : -1.0f;
	float forwardSpin = b3MaxFloat( spinSpeed * commandSign, 0.0f );

	float taperStart = b3ClampFloat( config.driveTaperStart, 0.0f, 0.99f ) * config.maxDriveSpeed;
	float taper = 1.0f;
	if ( config.maxDriveSpeed > taperStart + 0.001f )
	{
		taper = b3ClampFloat( ( config.maxDriveSpeed - forwardSpin ) / ( config.maxDriveSpeed - taperStart ), 0.0f, 1.0f );
	}

	return std::fabs( driveInput ) * config.maxDriveTorque * taper;
}

// Anti-roll bar: a couple built from the left/right travel difference,
// pushing the more-compressed corner's unsprung mass down and its partner up,
// with equal and opposite reactions on the chassis at the rest anchors. Net
// force zero, net moment = anti-roll - the same load transfer a torsion bar
// delivers, computed from the same displacement it physically reads.
void ApplyAxleAntiRollBar( const JozzVehicleM6& vehicle, int leftCorner, int rightCorner, float stiffness,
						   b3Vec3 chassisUp )
{
	if ( stiffness <= 0.0f )
	{
		return;
	}

	const JozzVehicleM6CornerRuntime& left = vehicle.corners[leftCorner];
	const JozzVehicleM6CornerRuntime& right = vehicle.corners[rightCorner];
	if ( B3_IS_NULL( left.wheelId ) || B3_IS_NULL( right.wheelId ) )
	{
		return;
	}

	b3Pos leftRest = b3Body_GetWorldPoint( vehicle.chassisId, left.restWheelCenterLocal );
	b3Pos rightRest = b3Body_GetWorldPoint( vehicle.chassisId, right.restWheelCenterLocal );
	float travelLeft = b3Dot( b3SubPos( b3Body_GetPosition( left.wheelId ), leftRest ), chassisUp );
	float travelRight = b3Dot( b3SubPos( b3Body_GetPosition( right.wheelId ), rightRest ), chassisUp );

	float force = stiffness * ( travelLeft - travelRight );
	if ( std::fabs( force ) < 1.0f )
	{
		return;
	}

	// The bar pushes on the unsprung side: knuckle when there is one,
	// otherwise the arm or the wheel body itself (strut corners).
	auto unsprungBody = []( const JozzVehicleM6CornerRuntime& corner ) -> b3BodyId {
		if ( B3_IS_NON_NULL( corner.knuckleId ) )
		{
			return corner.knuckleId;
		}
		if ( B3_IS_NON_NULL( corner.trailingArmId ) )
		{
			return corner.trailingArmId;
		}
		return corner.wheelId;
	};

	b3BodyId leftBody = unsprungBody( left );
	b3BodyId rightBody = unsprungBody( right );
	b3Vec3 down = b3MulSV( -force, chassisUp );
	b3Vec3 up = b3MulSV( force, chassisUp );
	b3Body_ApplyForce( leftBody, down, b3Body_GetPosition( leftBody ), false );
	b3Body_ApplyForce( rightBody, up, b3Body_GetPosition( rightBody ), false );
	// Reactions on the chassis at the rest anchors keep the couple internal.
	b3Body_ApplyForce( vehicle.chassisId, b3MulSV( force, chassisUp ), leftRest, false );
	b3Body_ApplyForce( vehicle.chassisId, b3MulSV( -force, chassisUp ), rightRest, false );
}

} // namespace

void UpdateJozzVehicleM6Drive( const JozzVehicleM6& vehicle, const JozzVehicleM6DriveInput& input )
{
	if ( vehicle.valid == false )
	{
		return;
	}

	const JozzVehicleM6Config& config = vehicle.config;

	b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 chassisUp = b3RotateVector( chassisRotation, b3Vec3_axisY );
	float rackAngle = ComputeJozzVehicleM6RackAngle( config, input.steer );
	bool handsOn = std::fabs( input.steer ) > config.steerInputDeadzone;

	// Positive drive = forward (+X). With the axle across +Z forward travel is
	// a negative spin about the joint axis - the validated M5 motor sign. The
	// motor always targets the rev limit; the throttle decides TORQUE, so
	// whether a wheel grips or spins up is a fight between engine torque and
	// contact friction, like it should be.
	float commandedSpinSpeed = input.drive >= 0.0f ? -config.maxDriveSpeed : config.maxDriveSpeed;

	// Wishbone front axle, hands ON: the rack spring centers on the stroke for
	// the commanded angle and the servo slews toward it with a hard force cap
	// (power steering + parking-torque muscle); Ackermann falls out of the
	// physical trapezoid. Hands OFF: spring and servo release and only the
	// rack friction resists, so the caster trail in the geometry back-drives
	// the steering through the tie rods - counter-steer in slides and
	// straightening after corners come from forces, not from a command blend.
	if ( B3_IS_NON_NULL( vehicle.rackJointId ) )
	{
		if ( handsOn )
		{
			float strokeMagnitude = ComputeJozzVehicleM6RackStroke( config.wishbone, 2.0f * config.axleHalfSpacing,
																	config.trackHalfWidth, config.rackHalfWidth,
																	std::fabs( rackAngle ) );
			float target = rackAngle >= 0.0f ? strokeMagnitude : -strokeMagnitude;
			target = b3ClampFloat( target, -config.rackTravel, config.rackTravel );
			b3PrismaticJoint_EnableSpring( vehicle.rackJointId, true );
			// Reassert the full steering hertz AND damping every step: the
			// opt-in centering assist below can leave the spring on a weak
			// hertz with damping forced to 1.0, and the servo must snap back
			// to the configured stiffness/damping the instant the driver
			// grabs the wheel again (audit A3: hertz alone left the user's
			// steeringDampingRatio silently replaced until the next slider
			// touch).
			b3PrismaticJoint_SetSpringHertz( vehicle.rackJointId, config.steeringHertz );
			b3PrismaticJoint_SetSpringDampingRatio( vehicle.rackJointId, config.steeringDampingRatio );
			b3PrismaticJoint_SetTargetTranslation( vehicle.rackJointId, target );

			float error = target - b3PrismaticJoint_GetTranslation( vehicle.rackJointId );
			float servoSpeed = b3ClampFloat( config.rackServoSpeedGain * error, -config.rackServoMaxSpeed,
											 config.rackServoMaxSpeed );
			b3PrismaticJoint_SetMotorSpeed( vehicle.rackJointId, servoSpeed );
			b3PrismaticJoint_SetMaxMotorForce( vehicle.rackJointId, config.rackServoForce );
		}
		else
		{
			// Coulomb friction with a static/kinetic split (P4): motor still
			// targets zero speed, but the force cap depends on whether the rack
			// is currently moving. A single flat cap made the rack stop dead the
			// instant the (speed-independent) caster force dropped below it -
			// this lets a released, still-sliding rack overshoot center a little
			// before the higher static cap catches it at rest.
			//
			// Realistic default (rackCenteringHertz == 0): spring OFF, so the
			// ONLY thing that centers the wheels hands-off is the caster trail,
			// which needs forward rolling - at a standstill the wheels stay where
			// they are, exactly like a real car (verified 2026-07-08). Opt-in
			// arcade assist (> 0): a WEAK spring toward center is left on even
			// hands-off, so the wheels straighten at any speed including rest.
			// This is the old "software self-align" M7 removed - hence off by
			// default and kept weak; it lightly biases the honest caster
			// counter-steer in slides.
			b3PrismaticJoint_SetMotorSpeed( vehicle.rackJointId, 0.0f );
			if ( config.rackCenteringHertz > 0.0f )
			{
				// Arcade assist ON: weak centering spring + damping. The static
				// friction hold is deliberately dropped to a light value here -
				// keeping the full 250 N static cap would just pin the rack
				// wherever it is and swallow the spring (measured: hz=2 did
				// nothing against 250 N). Turning this on means "I want the
				// wheels to recenter", which is the opposite of "hold them
				// parked off-center", so the hold gives way to the spring.
				b3PrismaticJoint_EnableSpring( vehicle.rackJointId, true );
				b3PrismaticJoint_SetSpringHertz( vehicle.rackJointId, config.rackCenteringHertz );
				b3PrismaticJoint_SetSpringDampingRatio( vehicle.rackJointId, 1.0f );
				b3PrismaticJoint_SetTargetTranslation( vehicle.rackJointId, 0.0f );
				b3PrismaticJoint_SetMaxMotorForce( vehicle.rackJointId, 20.0f );
			}
			else
			{
				// Realistic default: spring OFF, LOAD-DEPENDENT Coulomb friction
				// (P4b - see the rackFrictionBase header comment for the model).
				// The transverse components of the front tie-rod constraint
				// forces press the rack into its guides; friction follows them
				// (mu * N) on top of a small constant seal/bearing drag. The
				// constraint force is last step's - a one-step lag is fine for
				// a friction magnitude.
				b3PrismaticJoint_EnableSpring( vehicle.rackJointId, false );

				b3Vec3 slideAxis = b3RotateVector( chassisRotation, b3Vec3_axisZ );
				float transverseLoad = 0.0f;
				for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
				{
					const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
					if ( IsFrontCorner( corner ) && B3_IS_NON_NULL( runtime.steerLinkJointId ) )
					{
						b3Vec3 tieForce = b3Joint_GetConstraintForce( runtime.steerLinkJointId );
						b3Vec3 transverse = b3Sub( tieForce, b3MulSV( b3Dot( tieForce, slideAxis ), slideAxis ) );
						transverseLoad += b3Length( transverse );
					}
				}

				float rackSpeed = b3PrismaticJoint_GetSpeed( vehicle.rackJointId );
				float stiction = std::fabs( rackSpeed ) < 0.01f ? JOZZ_M6_RACK_STICTION_RATIO : 1.0f;
				float frictionCap =
					stiction * ( config.rackFrictionBase + config.rackFrictionLoadCoeff * transverseLoad );
				b3PrismaticJoint_SetMaxMotorForce( vehicle.rackJointId, frictionCap );
			}
		}
		b3Joint_WakeBodies( vehicle.rackJointId );
	}

	// Strut front corners get their targets from the exact math M5 validated
	// (Ackermann split + limit clamp), reusing the shared helper.
	float strutTargetLeft = 0.0f;
	float strutTargetRight = 0.0f;
	{
		JozzVehicleM5Config steeringConfig = {};
		steeringConfig.axleHalfSpacing = config.axleHalfSpacing;
		steeringConfig.trackHalfWidth = config.trackHalfWidth;
		steeringConfig.maxSteeringAngleDegrees = config.maxSteeringAngleDegrees;
		steeringConfig.ackermannGeometry = config.ackermannGeometry;
		GetJozzVehicleM5SteeringTargets( steeringConfig, rackAngle, &strutTargetLeft, &strutTargetRight );
	}

	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
		bool driven = config.allWheelDrive || IsFrontCorner( corner ) == false;

		if ( runtime.rigType == JOZZ_M6_RIG_INTEGRATED_STRUT )
		{
			if ( IsFrontCorner( corner ) )
			{
				if ( handsOn )
				{
					b3WheelJoint_SetTargetSteeringAngle( runtime.strutJointId,
														 IsLeftCorner( corner ) ? strutTargetLeft : strutTargetRight );
					b3WheelJoint_SetMaxSteeringTorque( runtime.strutJointId, config.maxSteeringTorque );
				}
				else
				{
					// Hands off: the steering spring holds the CURRENT angle
					// with only friction-level torque, so with a caster-tilted
					// strut axis the contact forces can back-drive the wheel.
					b3WheelJoint_SetTargetSteeringAngle( runtime.strutJointId,
														 b3WheelJoint_GetSteeringAngle( runtime.strutJointId ) );
					b3WheelJoint_SetMaxSteeringTorque( runtime.strutJointId, config.steeringFrictionTorque );
				}
			}

			if ( input.brake )
			{
				b3WheelJoint_SetSpinMotorSpeed( runtime.strutJointId, 0.0f );
				b3WheelJoint_SetMaxSpinTorque( runtime.strutJointId, config.brakeTorque );
			}
			else if ( input.drive != 0.0f && driven )
			{
				float spinSpeed = b3WheelJoint_GetSpinSpeed( runtime.strutJointId );
				b3WheelJoint_SetSpinMotorSpeed( runtime.strutJointId, commandedSpinSpeed );
				b3WheelJoint_SetMaxSpinTorque( runtime.strutJointId,
											   TaperedDriveTorque( config, spinSpeed, commandedSpinSpeed, input.drive ) );
			}
			else
			{
				b3WheelJoint_SetSpinMotorSpeed( runtime.strutJointId, 0.0f );
				b3WheelJoint_SetMaxSpinTorque( runtime.strutJointId, config.coastTorque );
			}
		}
		else
		{
			if ( input.brake )
			{
				b3RevoluteJoint_SetMotorSpeed( runtime.spinJointId, 0.0f );
				b3RevoluteJoint_SetMaxMotorTorque( runtime.spinJointId, config.brakeTorque );
			}
			else if ( input.drive != 0.0f && driven )
			{
				// Relative wheel spin about the live axle, same math as the
				// telemetry: the motor speed is relative A->B.
				b3BodyId carrierId = B3_IS_NON_NULL( runtime.knuckleId ) ? runtime.knuckleId : runtime.trailingArmId;
				b3Vec3 axle = b3RotateVector( b3Body_GetRotation( runtime.wheelId ), b3Vec3_axisY );
				b3Vec3 relativeAngular =
					b3Sub( b3Body_GetAngularVelocity( runtime.wheelId ), b3Body_GetAngularVelocity( carrierId ) );
				float spinSpeed = b3Dot( relativeAngular, axle );

				b3RevoluteJoint_SetMotorSpeed( runtime.spinJointId, commandedSpinSpeed );
				b3RevoluteJoint_SetMaxMotorTorque( runtime.spinJointId,
												   TaperedDriveTorque( config, spinSpeed, commandedSpinSpeed, input.drive ) );
			}
			else
			{
				b3RevoluteJoint_SetMotorSpeed( runtime.spinJointId, 0.0f );
				b3RevoluteJoint_SetMaxMotorTorque( runtime.spinJointId, config.coastTorque );
			}
		}
	}

	// Real forces that need no input: anti-roll couples per axle and quadratic
	// aero drag. Forces (not impulses), so they are frame-rate honest.
	ApplyAxleAntiRollBar( vehicle, JOZZ_M6_FRONT_LEFT, JOZZ_M6_FRONT_RIGHT, config.arbFrontStiffness, chassisUp );
	ApplyAxleAntiRollBar( vehicle, JOZZ_M6_REAR_LEFT, JOZZ_M6_REAR_RIGHT, config.arbRearStiffness, chassisUp );

	if ( config.aeroDragArea > 0.0f )
	{
		b3Vec3 velocity = b3Body_GetLinearVelocity( vehicle.chassisId );
		float speed = b3Length( velocity );
		if ( speed > 1.0f )
		{
			const float airDensity = 1.225f;
			b3Vec3 drag = b3MulSV( -0.5f * airDensity * config.aeroDragArea * speed, velocity );
			b3Body_ApplyForceToCenter( vehicle.chassisId, drag, false );
		}
	}

	if ( input.brake || input.drive != 0.0f || input.steer != 0.0f )
	{
		b3Body_SetAwake( vehicle.chassisId, true );
	}
}

float GetJozzVehicleM6ForwardSpeed( const JozzVehicleM6& vehicle )
{
	if ( vehicle.valid == false )
	{
		return 0.0f;
	}

	b3Vec3 velocity = b3Body_GetLinearVelocity( vehicle.chassisId );
	b3Vec3 forward = b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisX );
	return b3Dot( velocity, forward );
}

float GetJozzVehicleM6AlignmentAngle( const JozzVehicleM6& vehicle )
{
	if ( vehicle.valid == false )
	{
		return 0.0f;
	}

	b3Quat rotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 localVelocity = b3InvRotateVector( rotation, b3Body_GetLinearVelocity( vehicle.chassisId ) );
	float planarSpeed = std::sqrt( localVelocity.x * localVelocity.x + localVelocity.z * localVelocity.z );
	if ( planarSpeed < 0.5f || localVelocity.x < 0.1f )
	{
		return 0.0f;
	}

	return -std::atan2( localVelocity.z, localVelocity.x );
}

b3Pos GetJozzVehicleM6RestWheelCenter( const JozzVehicleM6& vehicle, int corner )
{
	if ( vehicle.valid == false || corner < 0 || corner >= JOZZ_M6_CORNER_COUNT )
	{
		return b3Pos_zero;
	}

	return b3Body_GetWorldPoint( vehicle.chassisId, vehicle.corners[corner].restWheelCenterLocal );
}

JozzVehicleM6WheelTelemetry GetJozzVehicleM6WheelTelemetry( const JozzVehicleM6& vehicle, int corner )
{
	JozzVehicleM6WheelTelemetry telemetry = {};
	if ( vehicle.valid == false || corner < 0 || corner >= JOZZ_M6_CORNER_COUNT )
	{
		return telemetry;
	}

	const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
	b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 chassisUp = b3RotateVector( chassisRotation, b3Vec3_axisY );

	b3Pos restCenter = GetJozzVehicleM6RestWheelCenter( vehicle, corner );
	b3Pos wheelPosition = b3Body_GetPosition( runtime.wheelId );
	telemetry.suspensionTravel = b3Dot( b3SubPos( wheelPosition, restCenter ), chassisUp );

	if ( runtime.rigType == JOZZ_M6_RIG_INTEGRATED_STRUT )
	{
		telemetry.suspensionLoad = b3Dot( b3Joint_GetConstraintForce( runtime.strutJointId ), chassisUp );
		telemetry.spinSpeed = b3WheelJoint_GetSpinSpeed( runtime.strutJointId );
		telemetry.steeringAngle =
			IsFrontCorner( corner ) ? b3WheelJoint_GetSteeringAngle( runtime.strutJointId ) : 0.0f;
	}
	else
	{
		// The coilover carries the spring load; the arms only guide geometry.
		telemetry.suspensionLoad = b3Dot( b3Joint_GetConstraintForce( runtime.coiloverJointId ), chassisUp );

		// Relative spin of the wheel about the live axle direction, against
		// whatever body carries the wheel (knuckle or trailing arm).
		b3BodyId carrierId = B3_IS_NON_NULL( runtime.knuckleId ) ? runtime.knuckleId : runtime.trailingArmId;
		b3Vec3 axle = b3RotateVector( b3Body_GetRotation( runtime.wheelId ), b3Vec3_axisY );
		b3Vec3 relativeAngular =
			b3Sub( b3Body_GetAngularVelocity( runtime.wheelId ), b3Body_GetAngularVelocity( carrierId ) );
		telemetry.spinSpeed = b3Dot( relativeAngular, axle );

		if ( B3_IS_NON_NULL( runtime.knuckleId ) )
		{
			// Knuckle yaw relative to the chassis; positive = left, matching
			// the steering convention (nose swings toward -Z).
			b3Vec3 knuckleForward =
				b3InvRotateVector( chassisRotation, b3RotateVector( b3Body_GetRotation( runtime.knuckleId ), b3Vec3_axisX ) );
			telemetry.steeringAngle = -std::atan2( knuckleForward.z, knuckleForward.x );
		}
		else
		{
			// Trailing arms do not steer.
			telemetry.steeringAngle = 0.0f;
		}

		// Camber: how far the axle tilts out of the chassis horizontal plane.
		// Sign flipped per side so "top of the wheel leaning inboard" is
		// negative camber on both sides, like an alignment sheet.
		b3Vec3 axleLocal = b3InvRotateVector( chassisRotation, axle );
		float lift = b3ClampFloat( axleLocal.y, -1.0f, 1.0f );
		telemetry.camberAngle = std::asin( lift ) * ( IsLeftCorner( corner ) ? 1.0f : -1.0f );
	}

	// Slip angle: wheel-center velocity direction vs wheel heading in the
	// chassis horizontal plane. Positive = the wheel travels right of where
	// it points. Near-standstill values are meaningless, so gate on speed.
	{
		b3Vec3 wheelVelocityLocal = b3InvRotateVector( chassisRotation, b3Body_GetLinearVelocity( runtime.wheelId ) );
		float planarSpeed = std::sqrt( wheelVelocityLocal.x * wheelVelocityLocal.x + wheelVelocityLocal.z * wheelVelocityLocal.z );
		if ( planarSpeed > 0.5f )
		{
			float steer = telemetry.steeringAngle;
			float headingX = std::cos( steer );
			float headingZ = -std::sin( steer );
			float cross = headingX * wheelVelocityLocal.z - headingZ * wheelVelocityLocal.x;
			float dot = headingX * wheelVelocityLocal.x + headingZ * wheelVelocityLocal.z;
			telemetry.slipAngle = std::atan2( cross, dot );
		}
	}

	b3ContactData contacts[8];
	for ( int shapeIndex = 0; shapeIndex < runtime.wheelShapeCount && telemetry.groundContact == false; ++shapeIndex )
	{
		int contactCount = b3Shape_GetContactData( runtime.wheelShapeIds[shapeIndex], contacts, 8 );
		for ( int i = 0; i < contactCount && telemetry.groundContact == false; ++i )
		{
			for ( int m = 0; m < contacts[i].manifoldCount; ++m )
			{
				if ( contacts[i].manifolds[m].pointCount > 0 )
				{
					telemetry.groundContact = true;
					break;
				}
			}
		}
	}

	return telemetry;
}
