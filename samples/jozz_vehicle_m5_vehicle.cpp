// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m5_vehicle.h"

#include <cmath>

namespace
{

// Corner offsets in chassis-local space. Forward is +X and LEFT is -Z:
// right = forward x up = (+X) x (+Y) = +Z, so the driver's left is -Z. The
// original M5 pass had this backwards ("left is +Z"), which mirrored the
// corner names and pushed a sign error into the steering path that a
// 2026-07-05 playtest caught as inverted A/D.
b3Vec3 CornerLocalOffset( const JozzVehicleM5Config& config, int corner )
{
	float x = ( corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_FRONT_RIGHT ) ? config.axleHalfSpacing
																				: -config.axleHalfSpacing;
	float z = ( corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_REAR_LEFT ) ? -config.trackHalfWidth
																			  : config.trackHalfWidth;
	return { x, -config.restDrop, z };
}

bool IsFrontCorner( int corner )
{
	return corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_FRONT_RIGHT;
}

bool IsLeftCorner( int corner )
{
	return corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_REAR_LEFT;
}

float CornerSuspensionHertz( const JozzVehicleM5Config& config, int corner )
{
	float scale = IsFrontCorner( corner ) ? config.frontSuspensionScale : config.rearSuspensionScale;
	return config.suspensionHertz * scale;
}

float CornerSuspensionDamping( const JozzVehicleM5Config& config, int corner )
{
	float scale = IsFrontCorner( corner ) ? config.frontSuspensionScale : config.rearSuspensionScale;
	return config.suspensionDampingRatio * scale;
}

int ClampCylinderSides( int sides )
{
	// Hard engine limit: b3CreateCylinder asserts 3 <= sides <= 32. The cap
	// matters: at wheel radius ~0.51m even 32 sides leaves ~2.5mm of radial
	// ripple, a built-in washboard at speed. Going beyond 32 would be an
	// engine modification; the sphere wheel shape is the no-mod alternative.
	if ( sides < 3 )
	{
		return 3;
	}
	if ( sides > 32 )
	{
		return 32;
	}
	return sides;
}

} // namespace

JozzVehicleM5Config JozzVehicleM5DefaultConfig( float wheelRadius, float wheelWidth, float suspensionTravelHint )
{
	JozzVehicleM5Config config = {};

	// A 2026-07-05 playtest found the chassis box visually too wide: at the old
	// half-width 0.80, the tire's inner sidewall (trackHalfWidth 1.05 minus half
	// the wheel width ~0.22 = ~0.83) cleared the box by only ~3cm, so the body
	// looked like it clipped into the tires with no visible fender gap.
	config.chassisHalfExtents = { 1.55f, 0.35f, 0.55f };
	config.chassisDensity = 200.0f;
	config.cgVerticalOffset = 0.15f;

	config.axleHalfSpacing = 1.25f;
	config.trackHalfWidth = 1.05f;
	config.restDrop = 0.55f;

	// Sphere is the default after the 2026-07-05 wheel smoothness probe: at
	// full-throttle cruise the front wheels kept ground contact 100% of
	// sampled steps on spheres vs 31% on the 32-side cylinder (5% at 12
	// sides), with 3x less vertical agitation. The faceted cylinder is the
	// playtest-reported "hopping/teleporting wheels at speed"; it stays
	// available for experiments and for visualizing the facet mechanism.
	config.wheelShape = JOZZ_M5_WHEEL_SPHERE;
	config.wheelCylinderSides = 32;
	config.wheelRadius = wheelRadius;
	config.wheelWidth = wheelWidth;
	config.wheelDensity = 80.0f;
	config.wheelFriction = 1.25f;
	config.wheelRollingResistance = 0.02f;

	// The wheel-joint spring stiffness follows the constraint's effective mass,
	// which the light wheel dominates - not the chassis share it carries. At
	// 2.5 Hz the springs collapsed to the compression limit under the ~700 kg
	// chassis (caught by the headless drive smoke), so the default sits higher
	// than a real car's body frequency.
	config.suspensionHertz = 6.0f;
	config.suspensionDampingRatio = 0.7f;
	config.frontSuspensionScale = 1.0f;
	config.rearSuspensionScale = 1.0f;

	// Split the asset travel hint into more rebound headroom up top; both stay
	// explicit and tunable, the hint only seeds the defaults.
	float travel = suspensionTravelHint > 0.05f ? suspensionTravelHint : 0.70f;
	config.reboundTravel = 0.4f * travel;
	config.compressionTravel = 0.6f * travel;

	config.maxDriveSpeed = 26.0f;
	config.maxDriveTorque = 320.0f;
	config.brakeTorque = 650.0f;
	config.coastTorque = 8.0f;
	config.allWheelDrive = true;

	// A stationary tire resists steering with the friction of its whole contact
	// patch twisting in place (why real cars need power steering); a rolling
	// tire only needs to change its slip angle, which costs far less torque.
	// The scrub radius here is actually zero (the steering pivot passes through
	// the wheel center at rest, same line the wheel travels along), so this
	// static "parking torque" - not scrub geometry - was the missing factor.
	// A 2026-07-05 playtest found low-speed steering nearly frozen; the
	// headless drive smoke's stationary-steer check reproduced it exactly
	// (0 deg of a 32 deg target at the old 80 N*m default) before this fix.
	config.maxSteeringAngleDegrees = 32.0f;
	config.steeringHertz = 14.0f;
	config.steeringDampingRatio = 1.0f;
	config.maxSteeringTorque = 700.0f;

	// Linked-with-Ackermann is the realistic default the 2026-07-05 feedback
	// asked for: one wheel blocked should hold its partner back like a real
	// tie rod, and the inner wheel should steer tighter than the outer one.
	config.steeringMode = JOZZ_M5_STEERING_LINKED;
	config.tieRodToleranceDegrees = 2.5f;
	config.ackermannGeometry = true;
	config.speedSensitiveSteering = false;
	config.steeringTaperStartSpeed = 8.0f;
	config.steeringTaperEndSpeed = 25.0f;
	config.steeringTaperMinScale = 0.35f;

	config.uprightAssist = true;
	config.uprightHertz = 0.4f;
	config.uprightDampingRatio = 1.0f;

	config.filterGroupIndex = -18;

	return config;
}

JozzVehicleM5 CreateJozzVehicleM5( b3WorldId worldId, b3BodyId groundBodyId, const JozzVehicleM5Config& config,
								   b3Pos chassisSpawnPosition )
{
	JozzVehicleM5 vehicle = {};
	vehicle.config = config;
	vehicle.config.wheelCylinderSides = ClampCylinderSides( config.wheelCylinderSides );
	vehicle.uprightJointId = b3_nullJointId;

	// Chassis: one dynamic box, dropped below the body origin by the CG offset
	// so the center of mass sits lower without moving the suspension anchors.
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = chassisSpawnPosition;
		bodyDef.name = "jozz_m5_chassis";
		vehicle.chassisId = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = config.chassisDensity;
		shapeDef.baseMaterial.friction = 0.6f;
		shapeDef.filter.groupIndex = config.filterGroupIndex;

		b3BoxHull box = b3MakeOffsetBoxHull( config.chassisHalfExtents.x, config.chassisHalfExtents.y,
											 config.chassisHalfExtents.z, { 0.0f, -config.cgVerticalOffset, 0.0f } );
		b3CreateHullShape( vehicle.chassisId, &shapeDef, &box.base );
	}

	for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
	{
		b3Vec3 localOffset = CornerLocalOffset( vehicle.config, corner );
		b3Pos restWheelCenter = b3OffsetPos( chassisSpawnPosition, localOffset );

		// Wheel body: local Y rotated onto world Z so the axle runs across Z,
		// exactly like the validated corner lab.
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = restWheelCenter;
		bodyDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
		bodyDef.allowFastRotation = true;
		bodyDef.name = "jozz_m5_wheel";
		vehicle.wheelIds[corner] = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = config.wheelDensity;
		shapeDef.baseMaterial.friction = config.wheelFriction;
		shapeDef.baseMaterial.restitution = 0.02f;
		shapeDef.baseMaterial.rollingResistance = config.wheelRollingResistance;
		shapeDef.filter.groupIndex = config.filterGroupIndex;

		if ( vehicle.config.wheelShape == JOZZ_M5_WHEEL_SPHERE )
		{
			// Smooth reference wheel (also what the stock Driving sample uses).
			// Note the sphere bulges past the visual tire width laterally; only
			// the ground contact footprint matters for the chatter experiments.
			b3Sphere sphere = { b3Vec3_zero, config.wheelRadius };
			vehicle.wheelShapeIds[corner] = b3CreateSphereShape( vehicle.wheelIds[corner], &shapeDef, &sphere );
		}
		else
		{
			// API order is height, radius, yOffset, sides. Frame B is the wheel
			// center, so the cylinder must stay centered on the body origin.
			b3HullData* wheelHull = b3CreateCylinder( config.wheelWidth, config.wheelRadius, -0.5f * config.wheelWidth,
													  vehicle.config.wheelCylinderSides );
			vehicle.wheelShapeIds[corner] = b3CreateHullShape( vehicle.wheelIds[corner], &shapeDef, wheelHull );
			b3DestroyHull( wheelHull );
		}

		b3WheelJointDef jointDef = b3DefaultWheelJointDef();
		jointDef.base.bodyIdA = vehicle.chassisId;
		jointDef.base.bodyIdB = vehicle.wheelIds[corner];

		// M2.4 rest-anchor model: frame A sits at the rest wheel center on the
		// chassis, frame B at the wheel body origin, so spring rest stays at
		// translation 0 and rest drop remains explicit.
		jointDef.base.localFrameA.p = b3Body_GetLocalPoint( vehicle.chassisId, restWheelCenter );
		jointDef.base.localFrameB.p = b3Vec3_zero;

		// Wheel-joint convention: suspension travels along frame-A local X
		// (mapped onto world Y) and the wheel spins around frame-B local Z
		// (mapped onto world Z through the wheel body rotation).
		jointDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
		jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
		jointDef.base.collideConnected = false;

		jointDef.enableSuspensionSpring = true;
		jointDef.suspensionHertz = CornerSuspensionHertz( vehicle.config, corner );
		jointDef.suspensionDampingRatio = CornerSuspensionDamping( vehicle.config, corner );
		jointDef.enableSuspensionLimit = true;
		jointDef.lowerSuspensionLimit = -config.reboundTravel;
		jointDef.upperSuspensionLimit = config.compressionTravel;

		// Spin motor is enabled on every corner so braking works everywhere;
		// drive torque only goes to driven corners in UpdateJozzVehicleM5Drive.
		jointDef.enableSpinMotor = true;
		jointDef.spinSpeed = 0.0f;
		jointDef.maxSpinTorque = 0.0f;

		if ( IsFrontCorner( corner ) )
		{
			float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
			jointDef.enableSteering = true;
			jointDef.steeringHertz = config.steeringHertz;
			jointDef.steeringDampingRatio = config.steeringDampingRatio;
			jointDef.targetSteeringAngle = 0.0f;
			jointDef.maxSteeringTorque = config.maxSteeringTorque;
			jointDef.enableSteeringLimit = true;
			jointDef.lowerSteeringLimit = -maxAngle;
			jointDef.upperSteeringLimit = maxAngle;
		}

		vehicle.wheelJointIds[corner] = b3CreateWheelJoint( worldId, &jointDef );
	}

	if ( config.uprightAssist && B3_IS_NON_NULL( groundBodyId ) )
	{
		// Soft parallel joint against the ground, borrowed from the stock
		// Driving sample. It resists roll/pitch without pinning the chassis.
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

void DestroyJozzVehicleM5( JozzVehicleM5* vehicle )
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

	for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
	{
		if ( B3_IS_NON_NULL( vehicle->wheelJointIds[corner] ) )
		{
			b3DestroyJoint( vehicle->wheelJointIds[corner], false );
			vehicle->wheelJointIds[corner] = b3_nullJointId;
		}

		if ( B3_IS_NON_NULL( vehicle->wheelIds[corner] ) )
		{
			b3DestroyBody( vehicle->wheelIds[corner] );
			vehicle->wheelIds[corner] = b3_nullBodyId;
			vehicle->wheelShapeIds[corner] = b3_nullShapeId;
		}
	}

	if ( B3_IS_NON_NULL( vehicle->chassisId ) )
	{
		b3DestroyBody( vehicle->chassisId );
		vehicle->chassisId = b3_nullBodyId;
	}

	vehicle->valid = false;
}

void GetJozzVehicleM5SteeringTargets( const JozzVehicleM5Config& config, float rackAngle, float* outLeftTarget,
									  float* outRightTarget )
{
	float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
	float left = rackAngle;
	float right = rackAngle;

	if ( config.ackermannGeometry && std::fabs( rackAngle ) > 0.01f )
	{
		// Classic Ackermann from a bicycle-model turn radius: the inner wheel
		// follows a tighter circle so it needs a larger angle than the outer.
		//   R = wheelbase / tan(|rack|)  (radius of the mean wheel's circle)
		//   inner = atan(wheelbase / (R - track/2))
		//   outer = atan(wheelbase / (R + track/2))
		float wheelbase = 2.0f * config.axleHalfSpacing;
		float halfTrack = config.trackHalfWidth;
		float magnitude = std::fabs( rackAngle );
		float radius = wheelbase / std::tan( magnitude );

		float innerDenominator = radius - halfTrack;
		float inner = innerDenominator > 0.05f ? std::atan( wheelbase / innerDenominator ) : maxAngle;
		float outer = std::atan( wheelbase / ( radius + halfTrack ) );

		if ( rackAngle > 0.0f )
		{
			// Turning left: the left wheel is the inner wheel.
			left = inner;
			right = outer;
		}
		else
		{
			left = -outer;
			right = -inner;
		}
	}

	// Respect the joint's steering limit on the commanded targets themselves so
	// the tie-rod coupling below never chases an unreachable angle.
	left = b3ClampFloat( left, -maxAngle, maxAngle );
	right = b3ClampFloat( right, -maxAngle, maxAngle );

	*outLeftTarget = left;
	*outRightTarget = right;
}

void UpdateJozzVehicleM5Drive( const JozzVehicleM5& vehicle, const JozzVehicleM5DriveInput& input )
{
	if ( vehicle.valid == false )
	{
		return;
	}

	const JozzVehicleM5Config& config = vehicle.config;

	// Positive drive means forward (+X). With the axle across +Z, forward
	// travel is a negative spin about the joint axis, matching the validated
	// corner-lab motor convention.
	float targetSpinSpeed = -config.maxDriveSpeed * input.drive;

	// Positive steering angle = left turn (+X rotates toward -Z about the +Y
	// steering axis), so the driver input maps straight through with no sign
	// flip. History: the first M5 pass negated here based on a wrong "left is
	// +Z" note, a playtest caught A/D inverted, and an intermediate fix blamed
	// the camera. The convention at the top of the header is now the single
	// source of truth and the validation smoke asserts the signed direction.
	float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
	float steerScale = 1.0f;
	if ( config.speedSensitiveSteering )
	{
		float speed = std::fabs( GetJozzVehicleM5ForwardSpeed( vehicle ) );
		float taperStart = config.steeringTaperStartSpeed;
		float taperEnd = config.steeringTaperEndSpeed > taperStart + 0.01f ? config.steeringTaperEndSpeed : taperStart + 0.01f;
		float t = b3ClampFloat( ( speed - taperStart ) / ( taperEnd - taperStart ), 0.0f, 1.0f );
		steerScale = 1.0f + t * ( config.steeringTaperMinScale - 1.0f );
	}

	float rackAngle = maxAngle * steerScale * input.steer;
	float targetLeft = 0.0f;
	float targetRight = 0.0f;
	GetJozzVehicleM5SteeringTargets( config, rackAngle, &targetLeft, &targetRight );

	if ( config.steeringMode == JOZZ_M5_STEERING_LINKED )
	{
		// Virtual tie rod: each wheel may only be commanded to within the rod
		// tolerance of where its partner actually is (plus the Ackermann offset
		// between the two targets). If one wheel is physically blocked, the
		// other's command gets pinned next to it instead of steering on alone.
		float ackermannOffset = targetLeft - targetRight;
		float tolerance = config.tieRodToleranceDegrees * B3_PI / 180.0f;
		float actualLeft = b3WheelJoint_GetSteeringAngle( vehicle.wheelJointIds[JOZZ_M5_FRONT_LEFT] );
		float actualRight = b3WheelJoint_GetSteeringAngle( vehicle.wheelJointIds[JOZZ_M5_FRONT_RIGHT] );

		targetLeft = b3ClampFloat( targetLeft, actualRight + ackermannOffset - tolerance,
								   actualRight + ackermannOffset + tolerance );
		targetRight = b3ClampFloat( targetRight, actualLeft - ackermannOffset - tolerance,
									actualLeft - ackermannOffset + tolerance );
	}

	bool anyInput = input.brake || input.drive != 0.0f || input.steer != 0.0f;

	for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
	{
		b3JointId jointId = vehicle.wheelJointIds[corner];

		if ( IsFrontCorner( corner ) )
		{
			b3WheelJoint_SetTargetSteeringAngle( jointId, IsLeftCorner( corner ) ? targetLeft : targetRight );
		}

		bool driven = config.allWheelDrive || IsFrontCorner( corner ) == false;

		if ( input.brake )
		{
			b3WheelJoint_SetSpinMotorSpeed( jointId, 0.0f );
			b3WheelJoint_SetMaxSpinTorque( jointId, config.brakeTorque );
		}
		else if ( input.drive != 0.0f && driven )
		{
			b3WheelJoint_SetSpinMotorSpeed( jointId, targetSpinSpeed );
			b3WheelJoint_SetMaxSpinTorque( jointId, config.maxDriveTorque );
		}
		else
		{
			// Freewheel with light drag so the vehicle coasts down naturally.
			b3WheelJoint_SetSpinMotorSpeed( jointId, 0.0f );
			b3WheelJoint_SetMaxSpinTorque( jointId, config.coastTorque );
		}
	}

	if ( anyInput )
	{
		b3Body_SetAwake( vehicle.chassisId, true );
	}
}

float GetJozzVehicleM5ForwardSpeed( const JozzVehicleM5& vehicle )
{
	if ( vehicle.valid == false )
	{
		return 0.0f;
	}

	b3Vec3 velocity = b3Body_GetLinearVelocity( vehicle.chassisId );
	b3Quat rotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 forward = b3RotateVector( rotation, b3Vec3_axisX );
	return b3Dot( velocity, forward );
}

b3Pos GetJozzVehicleM5RestWheelCenter( const JozzVehicleM5& vehicle, int corner )
{
	if ( vehicle.valid == false || corner < 0 || corner >= JOZZ_M5_CORNER_COUNT )
	{
		return b3Pos_zero;
	}

	b3Vec3 localOffset = CornerLocalOffset( vehicle.config, corner );
	return b3Body_GetWorldPoint( vehicle.chassisId, localOffset );
}

JozzVehicleM5WheelTelemetry GetJozzVehicleM5WheelTelemetry( const JozzVehicleM5& vehicle, int corner )
{
	JozzVehicleM5WheelTelemetry telemetry = {};
	if ( vehicle.valid == false || corner < 0 || corner >= JOZZ_M5_CORNER_COUNT )
	{
		return telemetry;
	}

	b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 chassisUp = b3RotateVector( chassisRotation, b3Vec3_axisY );

	b3Pos restCenter = GetJozzVehicleM5RestWheelCenter( vehicle, corner );
	b3Pos wheelPosition = b3Body_GetPosition( vehicle.wheelIds[corner] );
	telemetry.suspensionTravel = b3Dot( b3SubPos( wheelPosition, restCenter ), chassisUp );

	// The wheel joint's constraint force carries the suspension spring/limit
	// load; its projection on the chassis up axis is the per-corner load.
	b3Vec3 constraintForce = b3Joint_GetConstraintForce( vehicle.wheelJointIds[corner] );
	telemetry.suspensionLoad = b3Dot( constraintForce, chassisUp );

	telemetry.spinSpeed = b3WheelJoint_GetSpinSpeed( vehicle.wheelJointIds[corner] );
	telemetry.steeringAngle = IsFrontCorner( corner ) ? b3WheelJoint_GetSteeringAngle( vehicle.wheelJointIds[corner] ) : 0.0f;

	b3ContactData contacts[8];
	int contactCount = b3Shape_GetContactData( vehicle.wheelShapeIds[corner], contacts, 8 );
	for ( int i = 0; i < contactCount; ++i )
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

	return telemetry;
}
