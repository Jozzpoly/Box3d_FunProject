// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_suspension_rig.h"

#include "jozz_vehicle_m5_vehicle.h"

#include <cmath>

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

	// Control-arm chassis mounts: straight inboard of the ball joints, split
	// fore/aft so each triangular arm becomes two rods sharing the ball joint.
	b3Vec3 upperInboard = b3Add( points.upperBallJoint, { 0.0f, 0.0f, in * geometry.upperArmLength } );
	b3Vec3 lowerInboard = b3Add( points.lowerBallJoint, { 0.0f, 0.0f, in * geometry.lowerArmLength } );
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

	// Real-world upright + hub + brake sits around 20-35 kg. Keeping the
	// knuckle reasonably heavy also keeps the constraint mass ratio against
	// the ~700 kg chassis inside what the iterative solver likes.
	config.knuckleMass = 28.0f;
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

	// Split envelope is the M6 default: smooth sphere for rolling on terrain,
	// true-width cylinder for props/walls/curbs. The 2026-07-05 probe killed
	// the phased-union idea (contact hops between layered hulls lose the
	// solver warm start; it rolled worse than one cylinder), and the plain
	// sphere keeps the ~0.29 m lateral bulge. Requires terrain tagged with
	// terrainCategoryBits; the builder falls back to a sphere when it is 0.
	config.wheelEnvelope.mode = JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL;
	config.wheelEnvelope.cylinderSides = 32;
	config.wheelEnvelope.unionLayerCount = 4;
	config.wheelEnvelope.radius = wheelRadius;
	config.wheelEnvelope.width = wheelWidth;
	config.wheelEnvelope.terrainCategoryBits = JOZZ_M6_TERRAIN_CATEGORY;
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

	config.maxDriveSpeed = 26.0f;
	config.maxDriveTorque = 320.0f;
	config.brakeTorque = 650.0f;
	config.coastTorque = 8.0f;
	config.allWheelDrive = true;

	config.maxSteeringAngleDegrees = 32.0f;
	config.steeringHertz = 14.0f;
	config.steeringDampingRatio = 1.0f;
	config.maxSteeringTorque = 700.0f;
	config.ackermannGeometry = true;
	config.strutCasterDeg = 0.0f; // 0 = exact M5 strut behavior

	// Drift feel: with hands off the wheel the commanded rack angle blends
	// toward the direction the car actually travels. The physical caster above
	// is the hardware half of the same effect.
	config.selfAlignAssist = true;
	config.selfAlignGain = 0.65f;
	config.selfAlignMinSpeed = 3.0f;
	config.selfAlignMaxSlipDeg = 32.0f;

	config.uprightAssist = true;
	config.uprightHertz = 0.4f;
	config.uprightDampingRatio = 1.0f;

	config.filterGroupIndex = -19;

	return config;
}

namespace
{

b3MassData MakeDiagonalMassData( float mass, b3Vec3 inertiaDiagonal )
{
	b3MassData massData = {};
	massData.mass = mass;
	massData.center = b3Vec3_zero;
	massData.inertia.cx = { inertiaDiagonal.x, 0.0f, 0.0f };
	massData.inertia.cy = { 0.0f, inertiaDiagonal.y, 0.0f };
	massData.inertia.cz = { 0.0f, 0.0f, inertiaDiagonal.z };
	return massData;
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

void CreateWishboneCorner( b3WorldId worldId, JozzVehicleM6* vehicle, int corner, b3Vec3 restWheelCenterLocal,
						   b3Pos restWheelCenterWorld )
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
	b3Vec3 upperBallKnuckle = b3Sub( hp.upperBallJoint, restWheelCenterLocal );
	b3Vec3 lowerBallKnuckle = b3Sub( hp.lowerBallJoint, restWheelCenterLocal );
	b3Vec3 steeringArmKnuckle = b3Sub( hp.steeringArm, restWheelCenterLocal );
	b3Vec3 coiloverKnuckle = b3Sub( hp.coiloverKnuckle, restWheelCenterLocal );

	// Wishbones as four rigid rods: a triangular control arm is exactly two
	// two-point links sharing the ball joint.
	runtime.linkJointIds[0] =
		CreateLinkRod( worldId, vehicle->chassisId, runtime.knuckleId, hp.upperFrontChassis, upperBallKnuckle, hp.upperBallJoint );
	runtime.linkJointIds[1] =
		CreateLinkRod( worldId, vehicle->chassisId, runtime.knuckleId, hp.upperRearChassis, upperBallKnuckle, hp.upperBallJoint );
	runtime.linkJointIds[2] =
		CreateLinkRod( worldId, vehicle->chassisId, runtime.knuckleId, hp.lowerFrontChassis, lowerBallKnuckle, hp.lowerBallJoint );
	runtime.linkJointIds[3] =
		CreateLinkRod( worldId, vehicle->chassisId, runtime.knuckleId, hp.lowerRearChassis, lowerBallKnuckle, hp.lowerBallJoint );
	runtime.linkCount = 4;

	// Coilover: the only compliant chassis<->knuckle connection. Spring rest
	// at the authored pose, travel limits mapped onto rod length.
	{
		float scale = CornerSuspensionScale( config, corner );
		float restLength = DistanceBetween( hp.coiloverChassis, hp.coiloverKnuckle );

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
		def.minLength = b3MaxFloat( 0.05f, restLength - config.compressionTravel );
		def.maxLength = restLength + config.reboundTravel;
		runtime.coiloverJointId = b3CreateDistanceJoint( worldId, &def );
	}

	// Fifth link: front corners connect the steering arm to the rack (created
	// before the corners), rear corners get a fixed toe link to the chassis.
	if ( IsFrontCorner( corner ) && B3_IS_NON_NULL( vehicle->rackId ) )
	{
		float rackEndZ = IsLeftCorner( corner ) ? -config.rackHalfWidth : config.rackHalfWidth;
		b3Vec3 rackEndLocal = { 0.0f, 0.0f, rackEndZ };
		// Rack rest position in chassis space, for the rest length.
		b3Vec3 rackRestLocal = { config.axleHalfSpacing - config.wishbone.steeringArmBack, -config.restDrop, 0.0f };
		b3Vec3 rackEndChassisLocal = b3Add( rackRestLocal, rackEndLocal );

		b3DistanceJointDef def = b3DefaultDistanceJointDef();
		def.base.bodyIdA = vehicle->rackId;
		def.base.bodyIdB = runtime.knuckleId;
		def.base.localFrameA.p = rackEndLocal;
		def.base.localFrameB.p = steeringArmKnuckle;
		def.base.collideConnected = false;
		def.length = DistanceBetween( rackEndChassisLocal, hp.steeringArm );
		def.enableSpring = false;
		runtime.steerLinkJointId = b3CreateDistanceJoint( worldId, &def );
	}
	else
	{
		// Toe link: roughly lower-arm length so bump steer stays small.
		float in = IsLeftCorner( corner ) ? 1.0f : -1.0f;
		b3Vec3 toeChassis = b3Add( hp.steeringArm, { 0.0f, 0.0f, in * config.wishbone.lowerArmLength } );

		b3DistanceJointDef def = b3DefaultDistanceJointDef();
		def.base.bodyIdA = vehicle->chassisId;
		def.base.bodyIdB = runtime.knuckleId;
		def.base.localFrameA.p = toeChassis;
		def.base.localFrameB.p = steeringArmKnuckle;
		def.base.collideConnected = false;
		def.length = DistanceBetween( toeChassis, hp.steeringArm );
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
		b3CreateHullShape( vehicle.chassisId, &shapeDef, &box.base );
	}

	// Physical steering rack, only when the front axle is multi-body. The rack
	// slides across Z on a prismatic joint; its position spring is the "power
	// steering" servo and the tie rods do the actual coupling, so a blocked
	// wheel genuinely holds the whole linkage back.
	if ( config.frontRigType == JOZZ_M6_RIG_DOUBLE_WISHBONE )
	{
		b3Vec3 rackRestLocal = { config.axleHalfSpacing - config.wishbone.steeringArmBack, -config.restDrop, 0.0f };
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
		runtime.wheelId = b3_nullBodyId;
		runtime.strutJointId = b3_nullJointId;
		runtime.spinJointId = b3_nullJointId;
		runtime.coiloverJointId = b3_nullJointId;
		runtime.steerLinkJointId = b3_nullJointId;
		for ( int i = 0; i < JOZZ_M6_MAX_WHEEL_SHAPES; ++i )
		{
			runtime.wheelShapeIds[i] = b3_nullShapeId;
		}
		for ( int i = 0; i < 4; ++i )
		{
			runtime.linkJointIds[i] = b3_nullJointId;
		}

		b3Vec3 localOffset = CornerLocalOffset( config, corner );
		runtime.restWheelCenterLocal = localOffset;
		b3Pos restWheelCenterWorld = b3OffsetPos( chassisSpawnPosition, localOffset );

		if ( runtime.rigType == JOZZ_M6_RIG_DOUBLE_WISHBONE )
		{
			CreateWishboneCorner( worldId, &vehicle, corner, localOffset, restWheelCenterWorld );
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

		b3JointId* cornerJointIds[8] = { &runtime.strutJointId,		&runtime.spinJointId,	  &runtime.coiloverJointId,
										 &runtime.steerLinkJointId, &runtime.linkJointIds[0], &runtime.linkJointIds[1],
										 &runtime.linkJointIds[2],	&runtime.linkJointIds[3] };
		for ( b3JointId* jointIdSlot : cornerJointIds )
		{
			b3JointId jointId = *jointIdSlot;
			if ( B3_IS_NON_NULL( jointId ) )
			{
				b3DestroyJoint( jointId, false );
			}
			*jointIdSlot = b3_nullJointId;
		}

		if ( B3_IS_NON_NULL( runtime.wheelId ) )
		{
			b3DestroyBody( runtime.wheelId );
			runtime.wheelId = b3_nullBodyId;
		}
		if ( B3_IS_NON_NULL( runtime.knuckleId ) )
		{
			b3DestroyBody( runtime.knuckleId );
			runtime.knuckleId = b3_nullBodyId;
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

float ComputeJozzVehicleM6RackAngle( const JozzVehicleM6Config& config, float steerInput, b3Vec3 localVelocity )
{
	float maxAngle = config.maxSteeringAngleDegrees * DEGREES_TO_RADIANS;
	float input = b3ClampFloat( steerInput, -1.0f, 1.0f );
	float raw = maxAngle * input;

	if ( config.selfAlignAssist == false )
	{
		return raw;
	}

	// Only meaningful while actually moving forward; atan2 goes wild near
	// standstill and in reverse.
	if ( localVelocity.x < config.selfAlignMinSpeed )
	{
		return raw;
	}

	// Travel direction left of the nose (-Z) means a POSITIVE steering angle
	// aligns the wheels with it, hence the negation of atan2(z, x).
	float maxSlip = config.selfAlignMaxSlipDeg * DEGREES_TO_RADIANS;
	float alignAngle = b3ClampFloat( -std::atan2( localVelocity.z, localVelocity.x ), -maxSlip, maxSlip );

	// The driver's hands outrank the assist: full input = no blending.
	float weight = config.selfAlignGain * ( 1.0f - std::fabs( input ) );
	float rackAngle = raw + weight * ( alignAngle - raw );
	return b3ClampFloat( rackAngle, -maxAngle, maxAngle );
}

void UpdateJozzVehicleM6Drive( const JozzVehicleM6& vehicle, const JozzVehicleM6DriveInput& input )
{
	if ( vehicle.valid == false )
	{
		return;
	}

	const JozzVehicleM6Config& config = vehicle.config;

	b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 localVelocity = b3InvRotateVector( chassisRotation, b3Body_GetLinearVelocity( vehicle.chassisId ) );
	float rackAngle = ComputeJozzVehicleM6RackAngle( config, input.steer, localVelocity );

	// Positive drive = forward (+X). With the axle across +Z forward travel is
	// a negative spin about the joint axis - the validated M5 motor sign.
	float targetSpinSpeed = -config.maxDriveSpeed * input.drive;

	// Wishbone front axle: the rack spring centers on the stroke that
	// corresponds to the commanded angle, and the servo motor slews toward it
	// with a hard force cap - spring for feel, motor for the parking-torque
	// muscle. The Ackermann split then falls out of the physical trapezoid.
	// The angle->stroke mapping goes through the exact linkage solution so the
	// command, the rack limit, and the resulting inner-wheel angle agree.
	if ( B3_IS_NON_NULL( vehicle.rackJointId ) )
	{
		float strokeMagnitude = ComputeJozzVehicleM6RackStroke( config.wishbone, 2.0f * config.axleHalfSpacing,
																config.trackHalfWidth, config.rackHalfWidth,
																std::fabs( rackAngle ) );
		float target = rackAngle >= 0.0f ? strokeMagnitude : -strokeMagnitude;
		target = b3ClampFloat( target, -config.rackTravel, config.rackTravel );
		b3PrismaticJoint_SetTargetTranslation( vehicle.rackJointId, target );

		float error = target - b3PrismaticJoint_GetTranslation( vehicle.rackJointId );
		float servoSpeed = b3ClampFloat( config.rackServoSpeedGain * error, -config.rackServoMaxSpeed,
										 config.rackServoMaxSpeed );
		b3PrismaticJoint_SetMotorSpeed( vehicle.rackJointId, servoSpeed );
		b3PrismaticJoint_SetMaxMotorForce( vehicle.rackJointId, config.rackServoForce );
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
				b3WheelJoint_SetTargetSteeringAngle( runtime.strutJointId,
													 IsLeftCorner( corner ) ? strutTargetLeft : strutTargetRight );
			}

			if ( input.brake )
			{
				b3WheelJoint_SetSpinMotorSpeed( runtime.strutJointId, 0.0f );
				b3WheelJoint_SetMaxSpinTorque( runtime.strutJointId, config.brakeTorque );
			}
			else if ( input.drive != 0.0f && driven )
			{
				b3WheelJoint_SetSpinMotorSpeed( runtime.strutJointId, targetSpinSpeed );
				b3WheelJoint_SetMaxSpinTorque( runtime.strutJointId, config.maxDriveTorque );
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
				b3RevoluteJoint_SetMotorSpeed( runtime.spinJointId, targetSpinSpeed );
				b3RevoluteJoint_SetMaxMotorTorque( runtime.spinJointId, config.maxDriveTorque );
			}
			else
			{
				b3RevoluteJoint_SetMotorSpeed( runtime.spinJointId, 0.0f );
				b3RevoluteJoint_SetMaxMotorTorque( runtime.spinJointId, config.coastTorque );
			}
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
		// The coilover carries the spring load; the rods only guide geometry.
		telemetry.suspensionLoad = b3Dot( b3Joint_GetConstraintForce( runtime.coiloverJointId ), chassisUp );

		// Relative spin of the wheel about the live axle direction.
		b3Vec3 axle = b3RotateVector( b3Body_GetRotation( runtime.wheelId ), b3Vec3_axisY );
		b3Vec3 relativeAngular =
			b3Sub( b3Body_GetAngularVelocity( runtime.wheelId ), b3Body_GetAngularVelocity( runtime.knuckleId ) );
		telemetry.spinSpeed = b3Dot( relativeAngular, axle );

		// Knuckle yaw relative to the chassis; positive = left, matching the
		// steering convention (nose swings toward -Z).
		b3Vec3 knuckleForward =
			b3InvRotateVector( chassisRotation, b3RotateVector( b3Body_GetRotation( runtime.knuckleId ), b3Vec3_axisX ) );
		telemetry.steeringAngle = -std::atan2( knuckleForward.z, knuckleForward.x );

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
