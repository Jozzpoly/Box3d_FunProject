// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m5_vehicle.h"

namespace
{

// Corner offsets in chassis-local space. Forward is +X, left is +Z.
b3Vec3 CornerLocalOffset( const JozzVehicleM5Config& config, int corner )
{
	float x = ( corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_FRONT_RIGHT ) ? config.axleHalfSpacing
																				: -config.axleHalfSpacing;
	float z = ( corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_REAR_LEFT ) ? config.trackHalfWidth
																			  : -config.trackHalfWidth;
	return { x, -config.restDrop, z };
}

bool IsFrontCorner( int corner )
{
	return corner == JOZZ_M5_FRONT_LEFT || corner == JOZZ_M5_FRONT_RIGHT;
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

	config.axleHalfSpacing = 1.25f;
	config.trackHalfWidth = 1.05f;
	config.restDrop = 0.55f;

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
	vehicle.uprightJointId = b3_nullJointId;

	// Chassis: one dynamic box. Mass distribution stays simple until feel says
	// otherwise.
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

		b3BoxHull box = b3MakeBoxHull( config.chassisHalfExtents.x, config.chassisHalfExtents.y, config.chassisHalfExtents.z );
		b3CreateHullShape( vehicle.chassisId, &shapeDef, &box.base );
	}

	for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
	{
		b3Vec3 localOffset = CornerLocalOffset( config, corner );
		b3Pos restWheelCenter = b3OffsetPos( chassisSpawnPosition, localOffset );

		// Wheel body: centered primitive cylinder, local Y rotated onto world Z
		// so the axle runs across Z, exactly like the validated corner lab.
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

		// API order is height, radius, yOffset, sides. Frame B is the wheel
		// center, so the cylinder must stay centered on the body origin.
		b3HullData* wheelHull = b3CreateCylinder( config.wheelWidth, config.wheelRadius, -0.5f * config.wheelWidth, 32 );
		vehicle.wheelShapeIds[corner] = b3CreateHullShape( vehicle.wheelIds[corner], &shapeDef, wheelHull );
		b3DestroyHull( wheelHull );

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
		jointDef.suspensionHertz = config.suspensionHertz;
		jointDef.suspensionDampingRatio = config.suspensionDampingRatio;
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

	// Steering rotates about the frame-A suspension axis (world +Y); a positive
	// joint angle swings +X forward toward -Z, which is a right turn. Negate so
	// positive steer input means left, matching the header contract. Verified
	// empirically by the headless drive smoke.
	float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
	float targetSteering = -maxAngle * input.steer;

	bool anyInput = input.brake || input.drive != 0.0f || input.steer != 0.0f;

	for ( int corner = 0; corner < JOZZ_M5_CORNER_COUNT; ++corner )
	{
		b3JointId jointId = vehicle.wheelJointIds[corner];

		if ( IsFrontCorner( corner ) )
		{
			b3WheelJoint_SetTargetSteeringAngle( jointId, targetSteering );
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
