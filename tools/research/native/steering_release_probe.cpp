// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// Deterministic native evidence probe for B3WHEEL-STEER-01.
//
// This does not add steering assistance and it does not claim a product fix.
// It exercises the same M6 headless core as the native samples, then records
// the physical state after steering and throttle are released simultaneously.

#include "jozz_vehicle_m6_suspension_rig.h"

#include "box3d/box3d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifndef JV_PROBE_SOURCE_COMMIT
#define JV_PROBE_SOURCE_COMMIT "unknown"
#endif

#ifndef JV_PROBE_SOURCE_TREE
#define JV_PROBE_SOURCE_TREE "unknown"
#endif

#ifndef JV_PROBE_SOURCE_DIRTY
#define JV_PROBE_SOURCE_DIRTY "unknown"
#endif

#ifndef JV_PROBE_BUILD_TYPE
#define JV_PROBE_BUILD_TYPE "unknown"
#endif

namespace
{

constexpr float kTimeStep = 1.0f / 60.0f;
constexpr int kSubStepCount = 4;
constexpr int kAccelerationSteps = 260;
constexpr int kSteeringSteps = 60;
constexpr int kReleaseSteps = 240;
constexpr float kCenterThresholdDeg = 3.0f;
constexpr float kOutwardExcursionThresholdDeg = 1.0f;
constexpr int kSustainedOutwardSteps = 15;
constexpr float kDegreesPerRadian = 180.0f / B3_PI;

struct ContactAxisSample
{
	int manifoldCount = 0;
	int pointCount = 0;
	int loadedPointCount = 0;
	float normalImpulse = 0.0f;
	float frictionImpulse = 0.0f;
	float normalAxisImpulse = 0.0f;
	float frictionAxisImpulse = 0.0f;
	float twistAxisImpulse = 0.0f;
	float rollingAxisImpulse = 0.0f;
	float normalMomentArm = 0.0f;
	float contactTrail = 0.0f;
	float contactScrubOutboard = 0.0f;
	float centroidMomentResidual = 0.0f;
	bool geometryValid = false;
};

struct CornerAxisSample
{
	ContactAxisSample contact;
	float steeringAxisAngularSpeed = 0.0f;
	float slipAngle = 0.0f;
	float suspensionLoad = 0.0f;
	float tieRodAxisTorque = 0.0f;
	float tieRodRackAxisForce = 0.0f;
	float upperBallAxisTorque = 0.0f;
	float lowerBallAxisTorque = 0.0f;
	float spinJointAxisTorqueOnKnuckle = 0.0f;
	float gravityAxisTorque = 0.0f;
	float upperBallTwistMarginDeg = 0.0f;
	float lowerBallTwistMarginDeg = 0.0f;
	float upperBallConeMarginDeg = 0.0f;
	float lowerBallConeMarginDeg = 0.0f;
};

struct JointBodyLoad
{
	b3Vec3 force = b3Vec3_zero;
	b3Vec3 torque = b3Vec3_zero;
	b3Pos applicationPoint = b3Pos_zero;
	bool valid = false;
};

struct ReleaseFrameSample
{
	float leftAngleDeg = 0.0f;
	float rightAngleDeg = 0.0f;
	float speed = 0.0f;
	float alignmentDeg = 0.0f;
	float yawRate = 0.0f;
	float lateralSpeed = 0.0f;
	float rackTranslation = 0.0f;
	float rackSpeed = 0.0f;
	float rackMotorForce = 0.0f;
	float rackMaxMotorForce = 0.0f;
	float tieTransverseForce = 0.0f;
	CornerAxisSample left;
	CornerAxisSample right;
};

struct TransitionSnapshot
{
	float seconds = -1.0f;
	float directedAngleDeg = 0.0f;
	float leftSteeringAxisSpeed = 0.0f;
	float rightSteeringAxisSpeed = 0.0f;
	float leftSlipAngleDeg = 0.0f;
	float rightSlipAngleDeg = 0.0f;
	float leftSuspensionLoad = 0.0f;
	float rightSuspensionLoad = 0.0f;
	float rackMotorForce = 0.0f;
	float leftTieRodRackAxisForce = 0.0f;
	float rightTieRodRackAxisForce = 0.0f;
	float leftTieRodAxisTorque = 0.0f;
	float rightTieRodAxisTorque = 0.0f;
	float leftSpinJointAxisTorqueOnKnuckle = 0.0f;
	float rightSpinJointAxisTorqueOnKnuckle = 0.0f;
	float leftBallAxisTorque = 0.0f;
	float rightBallAxisTorque = 0.0f;
	float leftGravityAxisTorque = 0.0f;
	float rightGravityAxisTorque = 0.0f;
	float leftNormalImpulse = 0.0f;
	float rightNormalImpulse = 0.0f;
	float leftNormalAxisImpulse = 0.0f;
	float rightNormalAxisImpulse = 0.0f;
	float leftNormalMomentArm = 0.0f;
	float rightNormalMomentArm = 0.0f;
	float leftContactTrail = 0.0f;
	float rightContactTrail = 0.0f;
	float leftContactScrubOutboard = 0.0f;
	float rightContactScrubOutboard = 0.0f;
	float leftMinBallTwistMarginDeg = 0.0f;
	float rightMinBallTwistMarginDeg = 0.0f;
};

struct ReleaseResult
{
	const char* envelope = "unknown";
	int mode = -1;
	float steerInput = 0.0f;
	float releaseAngleDeg = 0.0f; // front-left, kept for schema continuity
	float finalAngleDeg = 0.0f;   // front-left, kept for schema continuity
	float releaseLeftDeg = 0.0f;
	float releaseRightDeg = 0.0f;
	float finalLeftDeg = 0.0f;
	float finalRightDeg = 0.0f;
	float peakAbsAngleDeg = 0.0f;
	float releaseSpeed = 0.0f;
	float finalSpeed = 0.0f;
	float releaseAlignmentDeg = 0.0f;
	float finalAlignmentDeg = 0.0f;
	float finalYawRate = 0.0f;
	float finalLateralSpeed = 0.0f;
	float centerTimeSeconds = -1.0f;
	float centerSpeed = 0.0f;
	float rackTranslationAtRelease = 0.0f;
	float rackTranslationFinal = 0.0f;
	float rackSpeedFinal = 0.0f;
	float maxRackMotorForce = 0.0f;
	float contactNormalAxisImpulse = 0.0f;
	float contactFrictionAxisImpulse = 0.0f;
	float contactTwistAxisImpulse = 0.0f;
	float contactRollingAxisImpulse = 0.0f;
	float leftNormalAxisImpulse = 0.0f;
	float rightNormalAxisImpulse = 0.0f;
	float leftFrictionAxisImpulse = 0.0f;
	float rightFrictionAxisImpulse = 0.0f;
	float rackMotorForceTime = 0.0f;
	float tieRodTransverseForceTime = 0.0f;
	float leftNormalImpulse = 0.0f;
	float rightNormalImpulse = 0.0f;
	float leftMeanContactTrail = 0.0f;
	float rightMeanContactTrail = 0.0f;
	float leftMeanContactScrubOutboard = 0.0f;
	float rightMeanContactScrubOutboard = 0.0f;
	float leftTieRodAxisTorqueTime = 0.0f;
	float rightTieRodAxisTorqueTime = 0.0f;
	float leftTieRodRackAxisForceTime = 0.0f;
	float rightTieRodRackAxisForceTime = 0.0f;
	float leftBallAxisTorqueTime = 0.0f;
	float rightBallAxisTorqueTime = 0.0f;
	float leftSpinJointAxisTorqueTime = 0.0f;
	float rightSpinJointAxisTorqueTime = 0.0f;
	float leftGravityAxisTorqueTime = 0.0f;
	float rightGravityAxisTorqueTime = 0.0f;
	float maxAbsSteeringAxisSpeed = 0.0f;
	float firstOutwardExcursionSeconds = -1.0f;
	float firstOutwardExcursionDirectedAngleDeg = 0.0f;
	float maxOutwardExcursionDeg = 0.0f;
	TransitionSnapshot sustainedAntiCenter;
	float leftBallTwistLimitEngagementSeconds = -1.0f;
	float rightBallTwistLimitEngagementSeconds = -1.0f;
	float leftBallTwistLimitEngagementDirectedAngleDeg = 0.0f;
	float rightBallTwistLimitEngagementDirectedAngleDeg = 0.0f;
	float leftMinBallTwistMarginDeg = 0.0f;
	float rightMinBallTwistMarginDeg = 0.0f;
	float leftMinBallConeMarginDeg = 0.0f;
	float rightMinBallConeMarginDeg = 0.0f;
	float meanLoadedContactPoints = 0.0f;
	float meanContactManifolds = 0.0f;
	int contactGeometryInvalidCornerSteps = 0;
	bool finite = true;
	bool traceRequested = false;
	bool traceWritten = true;
};

bool IsFrontCorner( int corner )
{
	return corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
}

const char* EnvelopeName( int mode )
{
	switch ( mode )
	{
		case JOZZ_M6_ENVELOPE_SPHERE:
			return "sphere";
		case JOZZ_M6_ENVELOPE_TORUS:
			return "torus";
		case JOZZ_M6_ENVELOPE_WHEEL:
			return "b3Wheel";
		default:
			return "other";
	}
}

b3BodyId CreateProbeGround( b3WorldId worldId )
{
	// Keep the vehicle world equivalent to the established native probes.
	b3World_EnableContinuous( worldId, false );

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = { 0.0f, -1.0f, 0.0f };
	bodyDef.name = "jv_steering_release_ground";
	b3BodyId groundId = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.8f;
	shapeDef.filter.categoryBits = JOZZ_M6_TERRAIN_CATEGORY;
	b3BoxHull ground = b3MakeBoxHull( 200.0f, 1.0f, 200.0f );
	b3CreateHullShape( groundId, &shapeDef, &ground.base );
	return groundId;
}

bool IsFinite( float value )
{
	return std::isfinite( value );
}

bool TransitionSnapshotIsFinite( const TransitionSnapshot& snapshot )
{
	return IsFinite( snapshot.seconds ) && IsFinite( snapshot.directedAngleDeg ) &&
		   IsFinite( snapshot.leftSteeringAxisSpeed ) && IsFinite( snapshot.rightSteeringAxisSpeed ) &&
		   IsFinite( snapshot.leftSlipAngleDeg ) && IsFinite( snapshot.rightSlipAngleDeg ) &&
		   IsFinite( snapshot.leftSuspensionLoad ) && IsFinite( snapshot.rightSuspensionLoad ) &&
		   IsFinite( snapshot.rackMotorForce ) && IsFinite( snapshot.leftTieRodRackAxisForce ) &&
		   IsFinite( snapshot.rightTieRodRackAxisForce ) && IsFinite( snapshot.leftTieRodAxisTorque ) &&
		   IsFinite( snapshot.rightTieRodAxisTorque ) &&
		   IsFinite( snapshot.leftSpinJointAxisTorqueOnKnuckle ) &&
		   IsFinite( snapshot.rightSpinJointAxisTorqueOnKnuckle ) && IsFinite( snapshot.leftBallAxisTorque ) &&
		   IsFinite( snapshot.rightBallAxisTorque ) && IsFinite( snapshot.leftGravityAxisTorque ) &&
		   IsFinite( snapshot.rightGravityAxisTorque ) && IsFinite( snapshot.leftNormalImpulse ) &&
		   IsFinite( snapshot.rightNormalImpulse ) && IsFinite( snapshot.leftNormalAxisImpulse ) &&
		   IsFinite( snapshot.rightNormalAxisImpulse ) && IsFinite( snapshot.leftNormalMomentArm ) &&
		   IsFinite( snapshot.rightNormalMomentArm ) && IsFinite( snapshot.leftContactTrail ) &&
		   IsFinite( snapshot.rightContactTrail ) && IsFinite( snapshot.leftContactScrubOutboard ) &&
		   IsFinite( snapshot.rightContactScrubOutboard ) && IsFinite( snapshot.leftMinBallTwistMarginDeg ) &&
		   IsFinite( snapshot.rightMinBallTwistMarginDeg );
}

bool ResultIsFinite( const ReleaseResult& result )
{
	return IsFinite( result.releaseAngleDeg ) && IsFinite( result.finalAngleDeg ) &&
		   IsFinite( result.releaseLeftDeg ) && IsFinite( result.releaseRightDeg ) &&
		   IsFinite( result.finalLeftDeg ) && IsFinite( result.finalRightDeg ) &&
		   IsFinite( result.peakAbsAngleDeg ) && IsFinite( result.releaseSpeed ) &&
		   IsFinite( result.finalSpeed ) && IsFinite( result.releaseAlignmentDeg ) &&
		   IsFinite( result.finalAlignmentDeg ) && IsFinite( result.finalYawRate ) &&
		   IsFinite( result.finalLateralSpeed ) && IsFinite( result.rackTranslationAtRelease ) &&
		   IsFinite( result.rackTranslationFinal ) && IsFinite( result.rackSpeedFinal ) &&
		   IsFinite( result.maxRackMotorForce ) && IsFinite( result.contactNormalAxisImpulse ) &&
		   IsFinite( result.contactFrictionAxisImpulse ) && IsFinite( result.contactTwistAxisImpulse ) &&
		   IsFinite( result.contactRollingAxisImpulse ) && IsFinite( result.leftNormalAxisImpulse ) &&
		   IsFinite( result.rightNormalAxisImpulse ) && IsFinite( result.leftFrictionAxisImpulse ) &&
		   IsFinite( result.rightFrictionAxisImpulse ) && IsFinite( result.rackMotorForceTime ) &&
		   IsFinite( result.tieRodTransverseForceTime ) && IsFinite( result.leftNormalImpulse ) &&
		   IsFinite( result.rightNormalImpulse ) && IsFinite( result.leftMeanContactTrail ) &&
		   IsFinite( result.rightMeanContactTrail ) && IsFinite( result.leftMeanContactScrubOutboard ) &&
		   IsFinite( result.rightMeanContactScrubOutboard ) && IsFinite( result.leftTieRodAxisTorqueTime ) &&
		   IsFinite( result.rightTieRodAxisTorqueTime ) && IsFinite( result.leftTieRodRackAxisForceTime ) &&
		   IsFinite( result.rightTieRodRackAxisForceTime ) && IsFinite( result.leftBallAxisTorqueTime ) &&
		   IsFinite( result.rightBallAxisTorqueTime ) && IsFinite( result.leftSpinJointAxisTorqueTime ) &&
		   IsFinite( result.rightSpinJointAxisTorqueTime ) && IsFinite( result.leftGravityAxisTorqueTime ) &&
		   IsFinite( result.rightGravityAxisTorqueTime ) && IsFinite( result.maxAbsSteeringAxisSpeed ) &&
		   IsFinite( result.firstOutwardExcursionSeconds ) &&
		   IsFinite( result.firstOutwardExcursionDirectedAngleDeg ) && IsFinite( result.maxOutwardExcursionDeg ) &&
		   TransitionSnapshotIsFinite( result.sustainedAntiCenter ) &&
		   IsFinite( result.leftBallTwistLimitEngagementSeconds ) &&
		   IsFinite( result.rightBallTwistLimitEngagementSeconds ) &&
		   IsFinite( result.leftBallTwistLimitEngagementDirectedAngleDeg ) &&
		   IsFinite( result.rightBallTwistLimitEngagementDirectedAngleDeg ) &&
		   IsFinite( result.leftMinBallTwistMarginDeg ) && IsFinite( result.rightMinBallTwistMarginDeg ) &&
		   IsFinite( result.leftMinBallConeMarginDeg ) && IsFinite( result.rightMinBallConeMarginDeg ) &&
		   IsFinite( result.meanLoadedContactPoints ) && IsFinite( result.meanContactManifolds );
}

b3Vec3 GetSteeringAxis( const JozzVehicleM6CornerRuntime& runtime, b3Pos* axisPoint )
{
	b3Transform lowerFrame = b3Joint_GetLocalFrameB( runtime.lowerBallId );
	b3Transform upperFrame = b3Joint_GetLocalFrameB( runtime.upperBallId );
	b3Pos lower = b3Body_GetWorldPoint( runtime.knuckleId, lowerFrame.p );
	b3Pos upper = b3Body_GetWorldPoint( runtime.knuckleId, upperFrame.p );
	b3Vec3 axis = b3SubPos( upper, lower );
	float length = b3Length( axis );
	if ( length <= 1.0e-6f )
	{
		*axisPoint = lower;
		return b3Vec3_axisY;
	}
	*axisPoint = lower;
	return b3MulSV( 1.0f / length, axis );
}

float ProjectMomentAboutAxis( b3Pos point, b3Vec3 forceOrImpulse, b3Pos axisPoint, b3Vec3 axis )
{
	// The same r x vector projection is used for instantaneous force (N -> N*m)
	// and accumulated solver impulse (N*s -> N*m*s). Callers retain the unit in
	// their field names; this helper deliberately does not reinterpret it.
	b3Vec3 lever = b3SubPos( point, axisPoint );
	return b3Dot( b3Cross( lever, forceOrImpulse ), axis );
}

ContactAxisSample SampleCornerContactAxisImpulse( const JozzVehicleM6CornerRuntime& runtime, bool isLeft )
{
	ContactAxisSample out = {};
	if ( B3_IS_NULL( runtime.wheelId ) || B3_IS_NULL( runtime.knuckleId ) ||
		 B3_IS_NULL( runtime.lowerBallId ) || B3_IS_NULL( runtime.upperBallId ) )
	{
		return out;
	}

	b3Pos axisPoint = b3Pos_zero;
	b3Vec3 axis = GetSteeringAxis( runtime, &axisPoint );
	b3Pos wheelCom = b3Body_GetWorldCenterOfMass( runtime.wheelId );
	b3Vec3 normalImpulseVector = b3Vec3_zero;
	b3Vec3 weightedAnchorSum = b3Vec3_zero;

	for ( int shapeIndex = 0; shapeIndex < runtime.wheelShapeCount; ++shapeIndex )
	{
		b3ShapeId wheelShapeId = runtime.wheelShapeIds[shapeIndex];
		if ( B3_IS_NULL( wheelShapeId ) || b3Shape_IsValid( wheelShapeId ) == false )
		{
			continue;
		}

		int capacity = b3Shape_GetContactCapacity( wheelShapeId );
		if ( capacity <= 0 )
		{
			continue;
		}
		std::vector<b3ContactData> contacts( static_cast<size_t>( capacity ) );
		int count = b3Shape_GetContactData( wheelShapeId, contacts.data(), capacity );

		for ( int contactIndex = 0; contactIndex < count; ++contactIndex )
		{
			const b3ContactData& contact = contacts[contactIndex];
			bool wheelIsA = B3_ID_EQUALS( contact.shapeIdA, wheelShapeId );
			bool wheelIsB = B3_ID_EQUALS( contact.shapeIdB, wheelShapeId );
			if ( wheelIsA == wheelIsB )
			{
				continue;
			}
			float sign = wheelIsA ? -1.0f : 1.0f;

			for ( int manifoldIndex = 0; manifoldIndex < contact.manifoldCount; ++manifoldIndex )
			{
				const b3Manifold& manifold = contact.manifolds[manifoldIndex];
				out.manifoldCount += 1;
				b3Vec3 normal = b3MulSV( sign, manifold.normal );
				b3Vec3 averageAnchor = b3Vec3_zero;

				for ( int pointIndex = 0; pointIndex < manifold.pointCount; ++pointIndex )
				{
					const b3ManifoldPoint& point = manifold.points[pointIndex];
					out.pointCount += 1;
					b3Vec3 anchor = wheelIsA ? point.anchorA : point.anchorB;
					averageAnchor = b3Add( averageAnchor, anchor );
					if ( point.totalNormalImpulse <= 0.0f )
					{
						continue;
					}
					out.loadedPointCount += 1;
					out.normalImpulse += point.totalNormalImpulse;
					weightedAnchorSum = b3MulAdd( weightedAnchorSum, point.totalNormalImpulse, anchor );
					b3Vec3 impulse = b3MulSV( point.totalNormalImpulse, normal );
					normalImpulseVector = b3Add( normalImpulseVector, impulse );
					b3Pos worldPoint = b3OffsetPos( wheelCom, anchor );
					out.normalAxisImpulse += ProjectMomentAboutAxis( worldPoint, impulse, axisPoint, axis );
				}

				if ( manifold.pointCount > 0 )
				{
					averageAnchor = b3MulSV( 1.0f / static_cast<float>( manifold.pointCount ), averageAnchor );
					b3Pos frictionPoint = b3OffsetPos( wheelCom, averageAnchor );
					b3Vec3 frictionImpulse = b3MulSV( sign, manifold.frictionImpulse );
					out.frictionImpulse += b3Length( frictionImpulse );
					out.frictionAxisImpulse += ProjectMomentAboutAxis( frictionPoint, frictionImpulse, axisPoint, axis );
				}

				b3Vec3 twistImpulse = b3MulSV( sign * manifold.twistImpulse, manifold.normal );
				b3Vec3 rollingImpulse = b3MulSV( sign, manifold.rollingImpulse );
				out.twistAxisImpulse += b3Dot( twistImpulse, axis );
				out.rollingAxisImpulse += b3Dot( rollingImpulse, axis );
			}
		}
	}

	// The normal-impulse centroid and its offset from the live steering axis
	// describe the effective geometric lever in the current contact plane. The
	// names are deliberately geometric diagnostics, not a pneumatic-trail tire
	// model. Positive trail means the loaded centroid is behind the wheel
	// heading. Positive scrub means outboard for the current corner.
	float normalVectorLength = b3Length( normalImpulseVector );
	if ( out.normalImpulse > 1.0e-6f && normalVectorLength > 1.0e-6f )
	{
		b3Vec3 centroidAnchor = b3MulSV( 1.0f / out.normalImpulse, weightedAnchorSum );
		b3Pos centroid = b3OffsetPos( wheelCom, centroidAnchor );
		b3Vec3 contactNormal = b3MulSV( 1.0f / normalVectorLength, normalImpulseVector );
		float axisPlaneDenominator = b3Dot( contactNormal, axis );

		b3Vec3 knuckleForward = b3RotateVector( b3Body_GetRotation( runtime.knuckleId ), b3Vec3_axisX );
		b3Vec3 forwardTangent = b3Sub( knuckleForward, b3MulSV( b3Dot( knuckleForward, contactNormal ), contactNormal ) );
		float forwardLength = b3Length( forwardTangent );
		if ( std::fabs( axisPlaneDenominator ) > 1.0e-5f && forwardLength > 1.0e-5f )
		{
			forwardTangent = b3MulSV( 1.0f / forwardLength, forwardTangent );
			float axisDistance = b3Dot( contactNormal, b3SubPos( centroid, axisPoint ) ) / axisPlaneDenominator;
			b3Pos axisPlaneIntersection = b3OffsetPos( axisPoint, b3MulSV( axisDistance, axis ) );
			b3Vec3 contactOffset = b3SubPos( centroid, axisPlaneIntersection );
			b3Vec3 vehicleLeft = b3Normalize( b3Cross( contactNormal, forwardTangent ) );
			b3Vec3 outboard = isLeft ? vehicleLeft : b3MulSV( -1.0f, vehicleLeft );

			out.normalMomentArm = out.normalAxisImpulse / out.normalImpulse;
			out.contactTrail = -b3Dot( contactOffset, forwardTangent );
			out.contactScrubOutboard = b3Dot( contactOffset, outboard );
			float centroidMoment = ProjectMomentAboutAxis( centroid, normalImpulseVector, axisPoint, axis );
			out.centroidMomentResidual = out.normalAxisImpulse - centroidMoment;
			out.geometryValid = true;
		}
	}

	return out;
}

JointBodyLoad SampleJointLoadOnBody( b3JointId jointId, b3BodyId targetBodyId )
{
	JointBodyLoad out = {};
	if ( B3_IS_NULL( jointId ) || B3_IS_NULL( targetBodyId ) )
	{
		return out;
	}

	b3BodyId bodyA = b3Joint_GetBodyA( jointId );
	b3BodyId bodyB = b3Joint_GetBodyB( jointId );
	// Box3D reports the current joint reaction force and reaction torque applied
	// to body B at its anchor. The equal and opposite reaction acts on body A,
	// so flip both vectors when the requested target is body A.
	b3Vec3 forceOnBodyB = b3Joint_GetConstraintForce( jointId );
	b3Vec3 torqueOnBodyB = b3Joint_GetConstraintTorque( jointId );
	b3Transform frame = b3Transform_identity;

	if ( B3_ID_EQUALS( targetBodyId, bodyB ) )
	{
		out.force = forceOnBodyB;
		out.torque = torqueOnBodyB;
		frame = b3Joint_GetLocalFrameB( jointId );
	}
	else if ( B3_ID_EQUALS( targetBodyId, bodyA ) )
	{
		out.force = b3MulSV( -1.0f, forceOnBodyB );
		out.torque = b3MulSV( -1.0f, torqueOnBodyB );
		frame = b3Joint_GetLocalFrameA( jointId );
	}
	else
	{
		return out;
	}

	out.applicationPoint = b3Body_GetWorldPoint( targetBodyId, frame.p );
	out.valid = true;
	return out;
}

float SampleJointAxisTorqueOnBody( b3JointId jointId, b3BodyId targetBodyId, b3Pos axisPoint, b3Vec3 axis )
{
	JointBodyLoad load = SampleJointLoadOnBody( jointId, targetBodyId );
	if ( load.valid == false )
	{
		return 0.0f;
	}
	return ProjectMomentAboutAxis( load.applicationPoint, load.force, axisPoint, axis ) + b3Dot( load.torque, axis );
}

float SampleGravityAxisTorque( b3WorldId worldId, b3BodyId bodyId, b3Pos axisPoint, b3Vec3 axis )
{
	if ( B3_IS_NULL( bodyId ) )
	{
		return 0.0f;
	}
	float mass = b3Body_GetMass( bodyId );
	b3Vec3 gravityForce = b3MulSV( mass, b3World_GetGravity( worldId ) );
	return ProjectMomentAboutAxis( b3Body_GetWorldCenterOfMass( bodyId ), gravityForce, axisPoint, axis );
}

void SampleSphericalLimitMargins( b3JointId jointId, float* twistMarginDeg, float* coneMarginDeg )
{
	*twistMarginDeg = 0.0f;
	*coneMarginDeg = 0.0f;
	if ( B3_IS_NULL( jointId ) )
	{
		return;
	}
	float twistAngle = b3SphericalJoint_GetTwistAngle( jointId );
	float lowerTwist = b3SphericalJoint_GetLowerTwistLimit( jointId );
	float upperTwist = b3SphericalJoint_GetUpperTwistLimit( jointId );
	float twistMargin = b3MinFloat( twistAngle - lowerTwist, upperTwist - twistAngle );
	float coneMargin = b3SphericalJoint_GetConeLimit( jointId ) - b3SphericalJoint_GetConeAngle( jointId );
	*twistMarginDeg = kDegreesPerRadian * twistMargin;
	*coneMarginDeg = kDegreesPerRadian * coneMargin;
}

CornerAxisSample SampleCornerAxisSources( b3WorldId worldId, const JozzVehicleM6& vehicle, int corner,
	const JozzVehicleM6WheelTelemetry& telemetry )
{
	CornerAxisSample out = {};
	const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
	if ( B3_IS_NULL( runtime.knuckleId ) )
	{
		return out;
	}

	b3Pos axisPoint = b3Pos_zero;
	b3Vec3 axis = GetSteeringAxis( runtime, &axisPoint );
	bool isLeft = corner == JOZZ_M6_FRONT_LEFT;
	out.contact = SampleCornerContactAxisImpulse( runtime, isLeft );

	out.slipAngle = telemetry.slipAngle;
	out.suspensionLoad = telemetry.suspensionLoad;
	b3Vec3 relativeAngularVelocity =
		b3Sub( b3Body_GetAngularVelocity( runtime.knuckleId ), b3Body_GetAngularVelocity( vehicle.chassisId ) );
	out.steeringAxisAngularSpeed = b3Dot( relativeAngularVelocity, axis );

	out.tieRodAxisTorque = SampleJointAxisTorqueOnBody( runtime.steerLinkJointId, runtime.knuckleId, axisPoint, axis );
	JointBodyLoad rackTieLoad = SampleJointLoadOnBody( runtime.steerLinkJointId, vehicle.rackId );
	if ( rackTieLoad.valid )
	{
		b3Vec3 rackAxis = b3RotateVector( b3Body_GetRotation( vehicle.chassisId ), b3Vec3_axisZ );
		out.tieRodRackAxisForce = b3Dot( rackTieLoad.force, rackAxis );
	}
	out.upperBallAxisTorque = SampleJointAxisTorqueOnBody( runtime.upperBallId, runtime.knuckleId, axisPoint, axis );
	out.lowerBallAxisTorque = SampleJointAxisTorqueOnBody( runtime.lowerBallId, runtime.knuckleId, axisPoint, axis );
	out.spinJointAxisTorqueOnKnuckle =
		SampleJointAxisTorqueOnBody( runtime.spinJointId, runtime.knuckleId, axisPoint, axis );
	out.gravityAxisTorque = SampleGravityAxisTorque( worldId, runtime.knuckleId, axisPoint, axis ) +
		SampleGravityAxisTorque( worldId, runtime.wheelId, axisPoint, axis );
	SampleSphericalLimitMargins( runtime.upperBallId, &out.upperBallTwistMarginDeg, &out.upperBallConeMarginDeg );
	SampleSphericalLimitMargins( runtime.lowerBallId, &out.lowerBallTwistMarginDeg, &out.lowerBallConeMarginDeg );
	return out;
}

float SampleTieRodTransverseLoad( const JozzVehicleM6& vehicle )
{
	if ( B3_IS_NULL( vehicle.rackId ) )
	{
		return 0.0f;
	}
	b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 slideAxis = b3RotateVector( chassisRotation, b3Vec3_axisZ );
	float transverseLoad = 0.0f;
	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
		if ( IsFrontCorner( corner ) && B3_IS_NON_NULL( runtime.steerLinkJointId ) )
		{
			b3Vec3 force = b3Joint_GetConstraintForce( runtime.steerLinkJointId );
			b3Vec3 transverse = b3Sub( force, b3MulSV( b3Dot( force, slideAxis ), slideAxis ) );
			transverseLoad += b3Length( transverse );
		}
	}
	return transverseLoad;
}

ReleaseFrameSample SampleReleaseFrame( b3WorldId worldId, const JozzVehicleM6& vehicle )
{
	ReleaseFrameSample out = {};
	JozzVehicleM6WheelTelemetry leftTelemetry =
		GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT );
	JozzVehicleM6WheelTelemetry rightTelemetry =
		GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT );
	out.leftAngleDeg = kDegreesPerRadian * leftTelemetry.steeringAngle;
	out.rightAngleDeg = kDegreesPerRadian * rightTelemetry.steeringAngle;
	out.speed = GetJozzVehicleM6ForwardSpeed( vehicle );
	out.alignmentDeg = kDegreesPerRadian * GetJozzVehicleM6AlignmentAngle( vehicle );

	b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
	b3Vec3 chassisUp = b3RotateVector( chassisRotation, b3Vec3_axisY );
	b3Vec3 localVelocity = b3InvRotateVector( chassisRotation, b3Body_GetLinearVelocity( vehicle.chassisId ) );
	out.yawRate = b3Dot( b3Body_GetAngularVelocity( vehicle.chassisId ), chassisUp );
	out.lateralSpeed = localVelocity.z;

	if ( B3_IS_NON_NULL( vehicle.rackJointId ) )
	{
		out.rackTranslation = b3PrismaticJoint_GetTranslation( vehicle.rackJointId );
		out.rackSpeed = b3PrismaticJoint_GetSpeed( vehicle.rackJointId );
		out.rackMotorForce = b3PrismaticJoint_GetMotorForce( vehicle.rackJointId );
		out.rackMaxMotorForce = b3PrismaticJoint_GetMaxMotorForce( vehicle.rackJointId );
	}
	out.tieTransverseForce = SampleTieRodTransverseLoad( vehicle );
	out.left = SampleCornerAxisSources( worldId, vehicle, JOZZ_M6_FRONT_LEFT, leftTelemetry );
	out.right = SampleCornerAxisSources( worldId, vehicle, JOZZ_M6_FRONT_RIGHT, rightTelemetry );
	return out;
}

float MinimumBallTwistMarginDeg( const CornerAxisSample& corner )
{
	return b3MinFloat( corner.upperBallTwistMarginDeg, corner.lowerBallTwistMarginDeg );
}

TransitionSnapshot MakeTransitionSnapshot( float seconds, float directedAngleDeg,
	const ReleaseFrameSample& frame )
{
	TransitionSnapshot snapshot = {};
	snapshot.seconds = seconds;
	snapshot.directedAngleDeg = directedAngleDeg;
	snapshot.leftSteeringAxisSpeed = frame.left.steeringAxisAngularSpeed;
	snapshot.rightSteeringAxisSpeed = frame.right.steeringAxisAngularSpeed;
	snapshot.leftSlipAngleDeg = kDegreesPerRadian * frame.left.slipAngle;
	snapshot.rightSlipAngleDeg = kDegreesPerRadian * frame.right.slipAngle;
	snapshot.leftSuspensionLoad = frame.left.suspensionLoad;
	snapshot.rightSuspensionLoad = frame.right.suspensionLoad;
	snapshot.rackMotorForce = frame.rackMotorForce;
	snapshot.leftTieRodRackAxisForce = frame.left.tieRodRackAxisForce;
	snapshot.rightTieRodRackAxisForce = frame.right.tieRodRackAxisForce;
	snapshot.leftTieRodAxisTorque = frame.left.tieRodAxisTorque;
	snapshot.rightTieRodAxisTorque = frame.right.tieRodAxisTorque;
	snapshot.leftSpinJointAxisTorqueOnKnuckle = frame.left.spinJointAxisTorqueOnKnuckle;
	snapshot.rightSpinJointAxisTorqueOnKnuckle = frame.right.spinJointAxisTorqueOnKnuckle;
	snapshot.leftBallAxisTorque = frame.left.upperBallAxisTorque + frame.left.lowerBallAxisTorque;
	snapshot.rightBallAxisTorque = frame.right.upperBallAxisTorque + frame.right.lowerBallAxisTorque;
	snapshot.leftGravityAxisTorque = frame.left.gravityAxisTorque;
	snapshot.rightGravityAxisTorque = frame.right.gravityAxisTorque;
	snapshot.leftNormalImpulse = frame.left.contact.normalImpulse;
	snapshot.rightNormalImpulse = frame.right.contact.normalImpulse;
	snapshot.leftNormalAxisImpulse = frame.left.contact.normalAxisImpulse;
	snapshot.rightNormalAxisImpulse = frame.right.contact.normalAxisImpulse;
	snapshot.leftNormalMomentArm = frame.left.contact.normalMomentArm;
	snapshot.rightNormalMomentArm = frame.right.contact.normalMomentArm;
	snapshot.leftContactTrail = frame.left.contact.contactTrail;
	snapshot.rightContactTrail = frame.right.contact.contactTrail;
	snapshot.leftContactScrubOutboard = frame.left.contact.contactScrubOutboard;
	snapshot.rightContactScrubOutboard = frame.right.contact.contactScrubOutboard;
	snapshot.leftMinBallTwistMarginDeg = MinimumBallTwistMarginDeg( frame.left );
	snapshot.rightMinBallTwistMarginDeg = MinimumBallTwistMarginDeg( frame.right );
	return snapshot;
}

void WriteTraceRow( FILE* trace, int step, float timeSeconds, const char* phase, const ReleaseFrameSample& sample )
{
	if ( trace == nullptr )
	{
		return;
	}
	const ContactAxisSample& left = sample.left.contact;
	const ContactAxisSample& right = sample.right.contact;
	std::fprintf( trace,
		"%d,%.9f,%s,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f",
		step, timeSeconds, phase, sample.leftAngleDeg, sample.rightAngleDeg, sample.speed, sample.alignmentDeg,
		sample.yawRate, sample.lateralSpeed, sample.rackTranslation, sample.rackSpeed, sample.rackMotorForce,
		sample.rackMaxMotorForce, sample.tieTransverseForce );
	std::fprintf( trace, ",%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f",
		sample.left.steeringAxisAngularSpeed, sample.right.steeringAxisAngularSpeed,
		kDegreesPerRadian * sample.left.slipAngle, kDegreesPerRadian * sample.right.slipAngle,
		sample.left.suspensionLoad, sample.right.suspensionLoad );
	std::fprintf( trace, ",%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f",
		left.normalImpulse, right.normalImpulse, left.normalAxisImpulse, right.normalAxisImpulse,
		left.normalMomentArm, right.normalMomentArm, left.contactTrail, right.contactTrail,
		left.contactScrubOutboard, right.contactScrubOutboard,
		left.centroidMomentResidual, right.centroidMomentResidual );
	std::fprintf( trace, ",%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f",
		left.frictionAxisImpulse, right.frictionAxisImpulse, left.twistAxisImpulse, right.twistAxisImpulse,
		left.rollingAxisImpulse, right.rollingAxisImpulse );
	std::fprintf( trace, ",%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f",
		sample.left.tieRodAxisTorque, sample.right.tieRodAxisTorque,
		sample.left.tieRodRackAxisForce, sample.right.tieRodRackAxisForce,
		sample.left.upperBallAxisTorque, sample.right.upperBallAxisTorque,
		sample.left.lowerBallAxisTorque, sample.right.lowerBallAxisTorque,
		sample.left.spinJointAxisTorqueOnKnuckle, sample.right.spinJointAxisTorqueOnKnuckle,
		sample.left.gravityAxisTorque, sample.right.gravityAxisTorque );
	std::fprintf( trace, ",%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f",
		sample.left.upperBallTwistMarginDeg, sample.right.upperBallTwistMarginDeg,
		sample.left.lowerBallTwistMarginDeg, sample.right.lowerBallTwistMarginDeg,
		sample.left.upperBallConeMarginDeg, sample.right.upperBallConeMarginDeg,
		sample.left.lowerBallConeMarginDeg, sample.right.lowerBallConeMarginDeg );
	std::fprintf( trace, ",%d,%d,%d,%d,%s,%s\n",
		left.loadedPointCount, right.loadedPointCount, left.manifoldCount, right.manifoldCount,
		left.geometryValid ? "true" : "false", right.geometryValid ? "true" : "false" );
}

std::string TraceFileName( const char* envelope, float steerInput )
{
	char buffer[128];
	int magnitude = static_cast<int>( std::lround( 100.0f * std::fabs( steerInput ) ) );
	std::snprintf( buffer, sizeof( buffer ), "%s_%c%03d.csv", envelope, steerInput >= 0.0f ? 'p' : 'm', magnitude );
	return buffer;
}

FILE* OpenTrace( const char* traceDirectory, const char* envelope, int mode, float steerInput )
{
	if ( traceDirectory == nullptr )
	{
		return nullptr;
	}
	std::filesystem::path path = std::filesystem::path( traceDirectory ) / TraceFileName( envelope, steerInput );
#if defined( _WIN32 )
	FILE* file = _wfopen( path.c_str(), L"wb" );
#else
	FILE* file = std::fopen( path.string().c_str(), "wb" );
#endif
	if ( file == nullptr )
	{
		std::fprintf( stderr, "trace_open_failed path=%s\n", path.string().c_str() );
		return nullptr;
	}
	std::fprintf( file, "# schema=jv-steering-release-trace/v2\n" );
	std::fprintf( file, "# commit=%s tree=%s dirty=%s build_type=%s\n", JV_PROBE_SOURCE_COMMIT,
				  JV_PROBE_SOURCE_TREE, JV_PROBE_SOURCE_DIRTY, JV_PROBE_BUILD_TYPE );
	std::fprintf( file, "# envelope=%s mode=%d steer=%+.6f artificial_centering=false\n", envelope, mode, steerInput );
	std::fprintf( file,
		"step,time_s,phase,left_angle_deg,right_angle_deg,forward_speed_mps,alignment_deg,yaw_rate_rps,"
		"lateral_speed_mps,rack_translation_m,rack_speed_mps,rack_motor_force_N,rack_max_force_N,"
		"tie_transverse_force_N,left_steering_axis_speed_rps,right_steering_axis_speed_rps,"
		"left_slip_angle_deg,right_slip_angle_deg,left_suspension_load_N,right_suspension_load_N,"
		"left_normal_impulse_N_s,right_normal_impulse_N_s,"
		"left_normal_axis_angular_impulse_N_m_s,right_normal_axis_angular_impulse_N_m_s,"
		"left_normal_moment_arm_m,right_normal_moment_arm_m,"
		"left_contact_trail_positive_behind_m,right_contact_trail_positive_behind_m,"
		"left_contact_scrub_positive_outboard_m,right_contact_scrub_positive_outboard_m,"
		"left_contact_centroid_moment_residual_N_m_s,right_contact_centroid_moment_residual_N_m_s,"
		"left_friction_axis_angular_impulse_N_m_s,right_friction_axis_angular_impulse_N_m_s,"
		"left_twist_axis_angular_impulse_N_m_s,right_twist_axis_angular_impulse_N_m_s,"
		"left_rolling_axis_angular_impulse_N_m_s,right_rolling_axis_angular_impulse_N_m_s,"
		"left_tie_rod_axis_torque_N_m,right_tie_rod_axis_torque_N_m,"
		"left_tie_rod_rack_axis_force_N,right_tie_rod_rack_axis_force_N,"
		"left_upper_ball_axis_torque_N_m,right_upper_ball_axis_torque_N_m,"
		"left_lower_ball_axis_torque_N_m,right_lower_ball_axis_torque_N_m,"
		"left_spin_joint_axis_torque_on_knuckle_N_m,right_spin_joint_axis_torque_on_knuckle_N_m,"
		"left_gravity_axis_torque_N_m,right_gravity_axis_torque_N_m,"
		"left_upper_ball_twist_margin_deg,right_upper_ball_twist_margin_deg,"
		"left_lower_ball_twist_margin_deg,right_lower_ball_twist_margin_deg,"
		"left_upper_ball_cone_margin_deg,right_upper_ball_cone_margin_deg,"
		"left_lower_ball_cone_margin_deg,right_lower_ball_cone_margin_deg,"
		"left_loaded_contact_points,right_loaded_contact_points,left_contact_manifolds,right_contact_manifolds,"
		"left_contact_geometry_valid,right_contact_geometry_valid\n" );
	return file;
}

ReleaseResult RunReleaseCase( int mode, float steerInput, const char* traceDirectory )
{
	ReleaseResult result = {};
	result.envelope = EnvelopeName( mode );
	result.mode = mode;
	result.steerInput = steerInput;
	result.traceRequested = traceDirectory != nullptr;

	// These dimensions are the pinned M3A values used by the current native
	// owner session. They are printed below as part of the evidence receipt.
	constexpr float wheelRadius = 0.514f;
	constexpr float wheelWidth = 0.4375f;
	constexpr float suspensionTravelHint = 0.700f;

	JozzVehicleM6Config config = JozzVehicleM6DefaultConfig( wheelRadius, wheelWidth, suspensionTravelHint );
	config.wheelEnvelope.mode = mode;
	config.rackCenteringHertz = 0.0f;

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3BodyId groundId = CreateProbeGround( worldId );
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
	if ( vehicle.valid == false )
	{
		result.finite = false;
		b3DestroyWorld( worldId );
		return result;
	}

	FILE* trace = OpenTrace( traceDirectory, result.envelope, mode, steerInput );
	if ( traceDirectory != nullptr && trace == nullptr )
	{
		result.traceWritten = false;
	}

	JozzVehicleM6DriveInput input = {};
	input.drive = 1.0f;
	for ( int step = 0; step < kAccelerationSteps; ++step )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, kTimeStep, kSubStepCount );
	}

	input.steer = steerInput;
	for ( int step = 0; step < kSteeringSteps; ++step )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, kTimeStep, kSubStepCount );
	}

	ReleaseFrameSample releaseFrame = SampleReleaseFrame( worldId, vehicle );
	result.releaseLeftDeg = releaseFrame.leftAngleDeg;
	result.releaseRightDeg = releaseFrame.rightAngleDeg;
	result.releaseAngleDeg = result.releaseLeftDeg;
	result.peakAbsAngleDeg = b3MaxFloat( std::fabs( result.releaseLeftDeg ), std::fabs( result.releaseRightDeg ) );
	result.releaseSpeed = releaseFrame.speed;
	result.releaseAlignmentDeg = releaseFrame.alignmentDeg;
	result.rackTranslationAtRelease = releaseFrame.rackTranslation;
	WriteTraceRow( trace, -1, 0.0f, "pre_release", releaseFrame );

	// This is the owner-reported event: both controls are released together.
	input.drive = 0.0f;
	input.steer = 0.0f;

	double loadedPointSum = 0.0;
	double manifoldSum = 0.0;
	double leftTrailImpulseSum = 0.0;
	double rightTrailImpulseSum = 0.0;
	double leftScrubImpulseSum = 0.0;
	double rightScrubImpulseSum = 0.0;
	float leftMinTwistMarginDeg = 1.0e30f;
	float rightMinTwistMarginDeg = 1.0e30f;
	float leftMinConeMarginDeg = 1.0e30f;
	float rightMinConeMarginDeg = 1.0e30f;
	float steeringSign = steerInput >= 0.0f ? 1.0f : -1.0f;
	float releaseDirectedAngle = steeringSign * 0.5f * ( result.releaseLeftDeg + result.releaseRightDeg );
	int sustainedOutwardStepCount = 0;
	TransitionSnapshot sustainedOutwardCandidate = {};

	for ( int step = 0; step < kReleaseSteps; ++step )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, kTimeStep, kSubStepCount );
		ReleaseFrameSample frame = SampleReleaseFrame( worldId, vehicle );
		const ContactAxisSample& left = frame.left.contact;
		const ContactAxisSample& right = frame.right.contact;

		result.finalLeftDeg = frame.leftAngleDeg;
		result.finalRightDeg = frame.rightAngleDeg;
		result.finalAngleDeg = frame.leftAngleDeg;
		result.finalSpeed = frame.speed;
		result.finalAlignmentDeg = frame.alignmentDeg;
		result.finalYawRate = frame.yawRate;
		result.finalLateralSpeed = frame.lateralSpeed;
		result.rackTranslationFinal = frame.rackTranslation;
		result.rackSpeedFinal = frame.rackSpeed;
		result.peakAbsAngleDeg = b3MaxFloat( result.peakAbsAngleDeg,
			b3MaxFloat( std::fabs( frame.leftAngleDeg ), std::fabs( frame.rightAngleDeg ) ) );
		if ( result.centerTimeSeconds < 0.0f && std::fabs( frame.leftAngleDeg ) <= kCenterThresholdDeg &&
			 std::fabs( frame.rightAngleDeg ) <= kCenterThresholdDeg )
		{
			result.centerTimeSeconds = static_cast<float>( step + 1 ) * kTimeStep;
			result.centerSpeed = frame.speed;
		}

		float directedAngle = steeringSign * 0.5f * ( frame.leftAngleDeg + frame.rightAngleDeg );
		float directedAxisSpeed =
			steeringSign * 0.5f * ( frame.left.steeringAxisAngularSpeed + frame.right.steeringAxisAngularSpeed );
		float outwardExcursionDeg = directedAngle - releaseDirectedAngle;
		result.maxOutwardExcursionDeg = b3MaxFloat( result.maxOutwardExcursionDeg, outwardExcursionDeg );
		if ( result.firstOutwardExcursionSeconds < 0.0f &&
			 outwardExcursionDeg >= kOutwardExcursionThresholdDeg && directedAxisSpeed > 0.0f )
		{
			result.firstOutwardExcursionSeconds = static_cast<float>( step + 1 ) * kTimeStep;
			result.firstOutwardExcursionDirectedAngleDeg = directedAngle;
		}

		// A brief outward overshoot can occur in a case that subsequently returns.
		// Keep it separate from a sustained anti-centering transition. The latter
		// requires the directed angle to remain at least one degree beyond release
		// for 0.25 s. The recorded snapshot is the first frame of that confirmed run,
		// not the later frame at which confirmation becomes available.
		if ( outwardExcursionDeg >= kOutwardExcursionThresholdDeg )
		{
			if ( sustainedOutwardStepCount == 0 )
			{
				sustainedOutwardCandidate = MakeTransitionSnapshot(
					static_cast<float>( step + 1 ) * kTimeStep, directedAngle, frame );
			}
			sustainedOutwardStepCount += 1;
			if ( result.sustainedAntiCenter.seconds < 0.0f &&
				 sustainedOutwardStepCount >= kSustainedOutwardSteps )
			{
				result.sustainedAntiCenter = sustainedOutwardCandidate;
			}
		}
		else
		{
			sustainedOutwardStepCount = 0;
		}

		float leftTwistMarginDeg = MinimumBallTwistMarginDeg( frame.left );
		float rightTwistMarginDeg = MinimumBallTwistMarginDeg( frame.right );
		if ( result.leftBallTwistLimitEngagementSeconds < 0.0f && leftTwistMarginDeg <= 0.0f )
		{
			result.leftBallTwistLimitEngagementSeconds = static_cast<float>( step + 1 ) * kTimeStep;
			result.leftBallTwistLimitEngagementDirectedAngleDeg = directedAngle;
		}
		if ( result.rightBallTwistLimitEngagementSeconds < 0.0f && rightTwistMarginDeg <= 0.0f )
		{
			result.rightBallTwistLimitEngagementSeconds = static_cast<float>( step + 1 ) * kTimeStep;
			result.rightBallTwistLimitEngagementDirectedAngleDeg = directedAngle;
		}

		result.leftNormalAxisImpulse += left.normalAxisImpulse;
		result.rightNormalAxisImpulse += right.normalAxisImpulse;
		result.leftFrictionAxisImpulse += left.frictionAxisImpulse;
		result.rightFrictionAxisImpulse += right.frictionAxisImpulse;
		result.contactNormalAxisImpulse += left.normalAxisImpulse + right.normalAxisImpulse;
		result.contactFrictionAxisImpulse += left.frictionAxisImpulse + right.frictionAxisImpulse;
		result.contactTwistAxisImpulse += left.twistAxisImpulse + right.twistAxisImpulse;
		result.contactRollingAxisImpulse += left.rollingAxisImpulse + right.rollingAxisImpulse;
		result.leftNormalImpulse += left.normalImpulse;
		result.rightNormalImpulse += right.normalImpulse;
		if ( left.geometryValid )
		{
			leftTrailImpulseSum += static_cast<double>( left.normalImpulse ) * left.contactTrail;
			leftScrubImpulseSum += static_cast<double>( left.normalImpulse ) * left.contactScrubOutboard;
		}
		else if ( left.loadedPointCount > 0 )
		{
			result.contactGeometryInvalidCornerSteps += 1;
		}
		if ( right.geometryValid )
		{
			rightTrailImpulseSum += static_cast<double>( right.normalImpulse ) * right.contactTrail;
			rightScrubImpulseSum += static_cast<double>( right.normalImpulse ) * right.contactScrubOutboard;
		}
		else if ( right.loadedPointCount > 0 )
		{
			result.contactGeometryInvalidCornerSteps += 1;
		}

		result.leftTieRodAxisTorqueTime += frame.left.tieRodAxisTorque * kTimeStep;
		result.rightTieRodAxisTorqueTime += frame.right.tieRodAxisTorque * kTimeStep;
		result.leftTieRodRackAxisForceTime += frame.left.tieRodRackAxisForce * kTimeStep;
		result.rightTieRodRackAxisForceTime += frame.right.tieRodRackAxisForce * kTimeStep;
		result.leftBallAxisTorqueTime +=
			( frame.left.upperBallAxisTorque + frame.left.lowerBallAxisTorque ) * kTimeStep;
		result.rightBallAxisTorqueTime +=
			( frame.right.upperBallAxisTorque + frame.right.lowerBallAxisTorque ) * kTimeStep;
		result.leftSpinJointAxisTorqueTime += frame.left.spinJointAxisTorqueOnKnuckle * kTimeStep;
		result.rightSpinJointAxisTorqueTime += frame.right.spinJointAxisTorqueOnKnuckle * kTimeStep;
		result.leftGravityAxisTorqueTime += frame.left.gravityAxisTorque * kTimeStep;
		result.rightGravityAxisTorqueTime += frame.right.gravityAxisTorque * kTimeStep;
		leftMinTwistMarginDeg = b3MinFloat( leftMinTwistMarginDeg,
			b3MinFloat( frame.left.upperBallTwistMarginDeg, frame.left.lowerBallTwistMarginDeg ) );
		rightMinTwistMarginDeg = b3MinFloat( rightMinTwistMarginDeg,
			b3MinFloat( frame.right.upperBallTwistMarginDeg, frame.right.lowerBallTwistMarginDeg ) );
		leftMinConeMarginDeg = b3MinFloat( leftMinConeMarginDeg,
			b3MinFloat( frame.left.upperBallConeMarginDeg, frame.left.lowerBallConeMarginDeg ) );
		rightMinConeMarginDeg = b3MinFloat( rightMinConeMarginDeg,
			b3MinFloat( frame.right.upperBallConeMarginDeg, frame.right.lowerBallConeMarginDeg ) );
		result.maxAbsSteeringAxisSpeed = b3MaxFloat( result.maxAbsSteeringAxisSpeed,
			b3MaxFloat( std::fabs( frame.left.steeringAxisAngularSpeed ),
				std::fabs( frame.right.steeringAxisAngularSpeed ) ) );

		// The public API reports current motor force. Integrating the sampled
		// value over frame time is a force-time diagnostic, not a direct read of
		// the solver's accumulated motor impulse.
		result.rackMotorForceTime += frame.rackMotorForce * kTimeStep;
		result.maxRackMotorForce = b3MaxFloat( result.maxRackMotorForce, std::fabs( frame.rackMaxMotorForce ) );
		result.tieRodTransverseForceTime += frame.tieTransverseForce * kTimeStep;

		int loadedPoints = left.loadedPointCount + right.loadedPointCount;
		int manifolds = left.manifoldCount + right.manifoldCount;
		loadedPointSum += static_cast<double>( loadedPoints );
		manifoldSum += static_cast<double>( manifolds );
		WriteTraceRow( trace, step, static_cast<float>( step + 1 ) * kTimeStep, "released", frame );
	}

	if ( result.leftNormalImpulse > 1.0e-6f )
	{
		result.leftMeanContactTrail = static_cast<float>( leftTrailImpulseSum / result.leftNormalImpulse );
		result.leftMeanContactScrubOutboard = static_cast<float>( leftScrubImpulseSum / result.leftNormalImpulse );
	}
	if ( result.rightNormalImpulse > 1.0e-6f )
	{
		result.rightMeanContactTrail = static_cast<float>( rightTrailImpulseSum / result.rightNormalImpulse );
		result.rightMeanContactScrubOutboard = static_cast<float>( rightScrubImpulseSum / result.rightNormalImpulse );
	}
	result.leftMinBallTwistMarginDeg = leftMinTwistMarginDeg;
	result.rightMinBallTwistMarginDeg = rightMinTwistMarginDeg;
	result.leftMinBallConeMarginDeg = leftMinConeMarginDeg;
	result.rightMinBallConeMarginDeg = rightMinConeMarginDeg;
	result.meanLoadedContactPoints = static_cast<float>( loadedPointSum / static_cast<double>( kReleaseSteps ) );
	result.meanContactManifolds = static_cast<float>( manifoldSum / static_cast<double>( kReleaseSteps ) );
	if ( trace != nullptr && std::fclose( trace ) != 0 )
	{
		result.traceWritten = false;
	}
	result.finite = ResultIsFinite( result );

	DestroyJozzVehicleM6( &vehicle );
	b3DestroyWorld( worldId );
	return result;
}

const char* Classify( const ReleaseResult& result )
{
	if ( result.finite == false )
	{
		return "NON_FINITE";
	}
	if ( result.centerTimeSeconds >= 0.0f && result.centerSpeed > 0.5f )
	{
		return "ROLLING_RETURN_OBSERVED";
	}
	if ( b3MaxFloat( std::fabs( result.finalLeftDeg ), std::fabs( result.finalRightDeg ) ) > 10.0f &&
		 result.finalSpeed > 0.5f )
	{
		return "ANTI_CENTERING_OR_HOLD";
	}
	return "INCONCLUSIVE";
}

void PrintResult( const ReleaseResult& result )
{
	float leftEffectiveNormalMomentArm =
		result.leftNormalImpulse > 1.0e-6f ? result.leftNormalAxisImpulse / result.leftNormalImpulse : 0.0f;
	float rightEffectiveNormalMomentArm =
		result.rightNormalImpulse > 1.0e-6f ? result.rightNormalAxisImpulse / result.rightNormalImpulse : 0.0f;
	const TransitionSnapshot& transition = result.sustainedAntiCenter;

	std::printf(
		"case envelope=%s mode=%d steer=%+.2f class=%s finite=%s trace_requested=%s trace_written=%s "
		"release_deg=%+.4f final_deg=%+.4f "
		"release_left_deg=%+.4f release_right_deg=%+.4f final_left_deg=%+.4f final_right_deg=%+.4f "
		"peak_abs_deg=%.4f release_speed=%.4f final_speed=%.4f release_alignment_deg=%+.4f "
		"final_alignment_deg=%+.4f final_yaw_rate_rps=%+.6f final_lateral_speed_mps=%+.6f "
		"center_time_s=%.4f center_speed=%.4f ",
		result.envelope, result.mode, result.steerInput, Classify( result ), result.finite ? "true" : "false",
		result.traceRequested ? "true" : "false", result.traceWritten ? "true" : "false", result.releaseAngleDeg,
		result.finalAngleDeg, result.releaseLeftDeg, result.releaseRightDeg, result.finalLeftDeg,
		result.finalRightDeg, result.peakAbsAngleDeg, result.releaseSpeed, result.finalSpeed, result.releaseAlignmentDeg,
		result.finalAlignmentDeg, result.finalYawRate, result.finalLateralSpeed, result.centerTimeSeconds,
		result.centerSpeed );

	std::printf(
		"first_outward_excursion_s=%.4f first_outward_excursion_directed_angle_deg=%+.4f "
		"max_outward_excursion_deg=%+.4f sustained_anti_center_onset_s=%.4f "
		"sustained_anti_center_directed_angle_deg=%+.4f "
		"sustained_left_steering_axis_speed_rps=%+.6f sustained_right_steering_axis_speed_rps=%+.6f "
		"sustained_left_slip_angle_deg=%+.6f sustained_right_slip_angle_deg=%+.6f "
		"sustained_left_suspension_load_N=%+.6f sustained_right_suspension_load_N=%+.6f "
		"sustained_rack_motor_force_N=%+.6f sustained_left_tie_rod_rack_axis_force_N=%+.6f "
		"sustained_right_tie_rod_rack_axis_force_N=%+.6f "
		"sustained_left_tie_rod_axis_torque_N_m=%+.6f sustained_right_tie_rod_axis_torque_N_m=%+.6f "
		"sustained_left_spin_joint_axis_torque_on_knuckle_N_m=%+.6f "
		"sustained_right_spin_joint_axis_torque_on_knuckle_N_m=%+.6f "
		"sustained_left_ball_axis_torque_N_m=%+.6f sustained_right_ball_axis_torque_N_m=%+.6f "
		"sustained_left_gravity_axis_torque_N_m=%+.6f sustained_right_gravity_axis_torque_N_m=%+.6f "
		"sustained_left_normal_impulse_N_s=%+.6f sustained_right_normal_impulse_N_s=%+.6f "
		"sustained_left_normal_axis_angular_impulse_N_m_s=%+.6f "
		"sustained_right_normal_axis_angular_impulse_N_m_s=%+.6f "
		"sustained_left_normal_moment_arm_m=%+.6f sustained_right_normal_moment_arm_m=%+.6f "
		"sustained_left_contact_trail_positive_behind_m=%+.6f "
		"sustained_right_contact_trail_positive_behind_m=%+.6f "
		"sustained_left_contact_scrub_positive_outboard_m=%+.6f "
		"sustained_right_contact_scrub_positive_outboard_m=%+.6f "
		"sustained_left_min_ball_twist_margin_deg=%+.6f "
		"sustained_right_min_ball_twist_margin_deg=%+.6f "
		"left_ball_twist_limit_engagement_s=%.4f right_ball_twist_limit_engagement_s=%.4f "
		"left_ball_twist_limit_engagement_directed_angle_deg=%+.4f "
		"right_ball_twist_limit_engagement_directed_angle_deg=%+.4f "
		"max_abs_steering_axis_speed_rps=%.6f ",
		result.firstOutwardExcursionSeconds, result.firstOutwardExcursionDirectedAngleDeg,
		result.maxOutwardExcursionDeg, transition.seconds, transition.directedAngleDeg,
		transition.leftSteeringAxisSpeed, transition.rightSteeringAxisSpeed,
		transition.leftSlipAngleDeg, transition.rightSlipAngleDeg,
		transition.leftSuspensionLoad, transition.rightSuspensionLoad,
		transition.rackMotorForce,
		transition.leftTieRodRackAxisForce, transition.rightTieRodRackAxisForce,
		transition.leftTieRodAxisTorque, transition.rightTieRodAxisTorque,
		transition.leftSpinJointAxisTorqueOnKnuckle, transition.rightSpinJointAxisTorqueOnKnuckle,
		transition.leftBallAxisTorque, transition.rightBallAxisTorque,
		transition.leftGravityAxisTorque, transition.rightGravityAxisTorque,
		transition.leftNormalImpulse, transition.rightNormalImpulse,
		transition.leftNormalAxisImpulse, transition.rightNormalAxisImpulse,
		transition.leftNormalMomentArm, transition.rightNormalMomentArm,
		transition.leftContactTrail, transition.rightContactTrail,
		transition.leftContactScrubOutboard, transition.rightContactScrubOutboard,
		transition.leftMinBallTwistMarginDeg, transition.rightMinBallTwistMarginDeg,
		result.leftBallTwistLimitEngagementSeconds, result.rightBallTwistLimitEngagementSeconds,
		result.leftBallTwistLimitEngagementDirectedAngleDeg,
		result.rightBallTwistLimitEngagementDirectedAngleDeg, result.maxAbsSteeringAxisSpeed );

	std::printf(
		"rack_release_m=%+.6f rack_final_m=%+.6f rack_speed_final_mps=%+.6f rack_max_force_N=%.3f "
		"contact_normal_axis_angular_impulse_N_m_s=%+.6f contact_friction_axis_angular_impulse_N_m_s=%+.6f "
		"left_normal_axis_angular_impulse_N_m_s=%+.6f right_normal_axis_angular_impulse_N_m_s=%+.6f "
		"left_normal_impulse_N_s=%.6f right_normal_impulse_N_s=%.6f "
		"left_effective_normal_moment_arm_m=%+.6f right_effective_normal_moment_arm_m=%+.6f "
		"left_mean_contact_trail_positive_behind_m=%+.6f right_mean_contact_trail_positive_behind_m=%+.6f "
		"left_mean_contact_scrub_positive_outboard_m=%+.6f right_mean_contact_scrub_positive_outboard_m=%+.6f "
		"left_friction_axis_angular_impulse_N_m_s=%+.6f right_friction_axis_angular_impulse_N_m_s=%+.6f "
		"contact_twist_axis_angular_impulse_N_m_s=%+.6f contact_rolling_axis_angular_impulse_N_m_s=%+.6f ",
		result.rackTranslationAtRelease, result.rackTranslationFinal, result.rackSpeedFinal,
		result.maxRackMotorForce, result.contactNormalAxisImpulse, result.contactFrictionAxisImpulse,
		result.leftNormalAxisImpulse, result.rightNormalAxisImpulse, result.leftNormalImpulse,
		result.rightNormalImpulse, leftEffectiveNormalMomentArm, rightEffectiveNormalMomentArm,
		result.leftMeanContactTrail, result.rightMeanContactTrail,
		result.leftMeanContactScrubOutboard, result.rightMeanContactScrubOutboard,
		result.leftFrictionAxisImpulse, result.rightFrictionAxisImpulse,
		result.contactTwistAxisImpulse, result.contactRollingAxisImpulse );

	std::printf(
		"left_tie_rod_axis_torque_time_N_m_s=%+.6f right_tie_rod_axis_torque_time_N_m_s=%+.6f "
		"left_tie_rod_rack_axis_force_time_N_s=%+.6f right_tie_rod_rack_axis_force_time_N_s=%+.6f "
		"left_ball_axis_torque_time_N_m_s=%+.6f right_ball_axis_torque_time_N_m_s=%+.6f "
		"left_spin_joint_axis_torque_time_N_m_s=%+.6f right_spin_joint_axis_torque_time_N_m_s=%+.6f "
		"left_gravity_axis_torque_time_N_m_s=%+.6f right_gravity_axis_torque_time_N_m_s=%+.6f "
		"left_min_ball_twist_margin_deg=%+.6f right_min_ball_twist_margin_deg=%+.6f "
		"left_min_ball_cone_margin_deg=%+.6f right_min_ball_cone_margin_deg=%+.6f "
		"rack_motor_force_time_N_s=%+.6f tie_transverse_force_time_N_s=%.6f "
		"mean_loaded_points=%.4f mean_manifolds=%.4f contact_geometry_invalid_corner_steps=%d\n",
		result.leftTieRodAxisTorqueTime, result.rightTieRodAxisTorqueTime,
		result.leftTieRodRackAxisForceTime, result.rightTieRodRackAxisForceTime,
		result.leftBallAxisTorqueTime, result.rightBallAxisTorqueTime,
		result.leftSpinJointAxisTorqueTime, result.rightSpinJointAxisTorqueTime,
		result.leftGravityAxisTorqueTime, result.rightGravityAxisTorqueTime,
		result.leftMinBallTwistMarginDeg, result.rightMinBallTwistMarginDeg,
		result.leftMinBallConeMarginDeg, result.rightMinBallConeMarginDeg,
		result.rackMotorForceTime, result.tieRodTransverseForceTime, result.meanLoadedContactPoints,
		result.meanContactManifolds, result.contactGeometryInvalidCornerSteps );
}

bool ReturnedWhileRolling( const ReleaseResult& result )
{
	return result.finite && result.centerTimeSeconds >= 0.0f && result.centerTimeSeconds <= 2.0f &&
		   result.centerSpeed > 0.5f;
}

bool ReproducesFullLockDefect( const ReleaseResult& result )
{
	return result.finite && result.centerTimeSeconds < 0.0f &&
		   b3MaxFloat( std::fabs( result.finalLeftDeg ), std::fabs( result.finalRightDeg ) ) > 10.0f &&
		   result.finalSpeed > 0.5f;
}

struct DenseSweepSideSummary
{
	float lastReturnAmplitude = -1.0f;
	float firstDefectAmplitude = -1.0f;
	bool monotonic = true;
	bool allClassified = true;
};

DenseSweepSideSummary SummarizeDenseSweepSide( const std::vector<ReleaseResult>& results, bool positive )
{
	struct ClassifiedAmplitude
	{
		float amplitude;
		int state; // 0 = rolling return, 1 = known defect, 2 = inconclusive
	};

	std::vector<ClassifiedAmplitude> samples;
	for ( const ReleaseResult& result : results )
	{
		if ( result.mode != JOZZ_M6_ENVELOPE_WHEEL || ( result.steerInput >= 0.0f ) != positive )
		{
			continue;
		}
		float amplitude = std::fabs( result.steerInput );
		if ( amplitude < 0.399f || amplitude > 0.501f )
		{
			continue;
		}
		int state = ReturnedWhileRolling( result ) ? 0 : ( ReproducesFullLockDefect( result ) ? 1 : 2 );
		samples.push_back( { amplitude, state } );
	}

	std::sort( samples.begin(), samples.end(), []( const ClassifiedAmplitude& a, const ClassifiedAmplitude& b ) {
		return a.amplitude < b.amplitude;
	} );

	DenseSweepSideSummary out = {};
	bool defectSeen = false;
	for ( const ClassifiedAmplitude& sample : samples )
	{
		if ( sample.state == 0 )
		{
			out.lastReturnAmplitude = sample.amplitude;
			if ( defectSeen )
			{
				out.monotonic = false;
			}
		}
		else if ( sample.state == 1 )
		{
			if ( defectSeen == false )
			{
				out.firstDefectAmplitude = sample.amplitude;
			}
			defectSeen = true;
		}
		else
		{
			out.allClassified = false;
		}
	}
	return out;
}

bool DenseSweepSideObserved( const DenseSweepSideSummary& side )
{
	return side.allClassified && side.monotonic && side.lastReturnAmplitude > 0.0f &&
		   side.firstDefectAmplitude > side.lastReturnAmplitude;
}

} // namespace

int main( int argc, char** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const char* traceDirectory = nullptr;
	bool denseSweep = false;
	for ( int i = 1; i < argc; ++i )
	{
		if ( std::strcmp( argv[i], "--trace-dir" ) == 0 && i + 1 < argc )
		{
			traceDirectory = argv[++i];
		}
		else if ( std::strcmp( argv[i], "--dense-sweep" ) == 0 )
		{
			denseSweep = true;
		}
		else
		{
			std::fprintf( stderr, "usage: %s [--trace-dir DIRECTORY] [--dense-sweep]\n", argv[0] );
			return 2;
		}
	}
	if ( traceDirectory != nullptr )
	{
		std::error_code error;
		std::filesystem::path tracePath( traceDirectory );
		if ( std::filesystem::exists( tracePath, error ) )
		{
			if ( error || std::filesystem::is_directory( tracePath, error ) == false )
			{
				std::fprintf( stderr, "trace_directory_invalid path=%s\n", traceDirectory );
				return 2;
			}
			if ( std::filesystem::directory_iterator( tracePath, error ) != std::filesystem::directory_iterator() )
			{
				std::fprintf( stderr, "trace_directory_not_empty path=%s\n", traceDirectory );
				return 2;
			}
		}
		else
		{
			std::filesystem::create_directories( tracePath, error );
		}
		if ( error )
		{
			std::fprintf( stderr, "trace_directory_failed path=%s error=%s\n", traceDirectory, error.message().c_str() );
			return 2;
		}
	}
	std::printf( "jv_steering_release_probe schema=jv-steering-release/v2\n" );
	std::printf( "provenance commit=%s tree=%s dirty=%s build_type=%s\n", JV_PROBE_SOURCE_COMMIT,
				 JV_PROBE_SOURCE_TREE, JV_PROBE_SOURCE_DIRTY, JV_PROBE_BUILD_TYPE );
	std::printf( "receipt wheel_radius_m=0.514000 wheel_width_m=0.437500 suspension_travel_m=0.700000 "
				 "dt=0.016666667 substeps=4 accel_steps=%d steer_steps=%d release_steps=%d "
				 "outward_excursion_threshold_deg=%.3f sustained_outward_steps=%d dense_sweep=%s "
				 "simultaneous_throttle_and_steering_release=true artificial_centering=false\n",
				 kAccelerationSteps, kSteeringSteps, kReleaseSteps, kOutwardExcursionThresholdDeg,
				 kSustainedOutwardSteps, denseSweep ? "true" : "false" );
	std::printf( "note contact_axis_impulses_are_solver_impulse_diagnostics_not_a_tire_model=true\n" );
	std::printf( "note contact_trail_and_scrub_are_normal_impulse_centroid_geometry_not_pneumatic_tire_parameters=true\n" );
	std::printf( "note joint_axis_torques_are_public_api_constraint_force_torque_diagnostics=true\n" );
	std::printf( "note source_channels_are_not_independent_or_directly_additive=true\n" );

	struct ProbeCase
	{
		int mode;
		float steer;
	};
	std::vector<ProbeCase> cases = {
		{ JOZZ_M6_ENVELOPE_SPHERE, 1.0f },
		{ JOZZ_M6_ENVELOPE_SPHERE, -1.0f },
		{ JOZZ_M6_ENVELOPE_TORUS, 1.0f },
		{ JOZZ_M6_ENVELOPE_TORUS, -1.0f },
		{ JOZZ_M6_ENVELOPE_WHEEL, 0.4f },
		{ JOZZ_M6_ENVELOPE_WHEEL, -0.4f },
		{ JOZZ_M6_ENVELOPE_WHEEL, 0.5f },
		{ JOZZ_M6_ENVELOPE_WHEEL, -0.5f },
		{ JOZZ_M6_ENVELOPE_WHEEL, 1.0f },
		{ JOZZ_M6_ENVELOPE_WHEEL, -1.0f },
	};
	if ( denseSweep )
	{
		for ( float amplitude : { 0.41f, 0.42f, 0.43f, 0.44f, 0.45f, 0.46f, 0.47f, 0.48f, 0.49f } )
		{
			cases.push_back( { JOZZ_M6_ENVELOPE_WHEEL, amplitude } );
			cases.push_back( { JOZZ_M6_ENVELOPE_WHEEL, -amplitude } );
		}
	}

	std::vector<ReleaseResult> results;
	results.reserve( cases.size() );
	for ( const auto& testCase : cases )
	{
		ReleaseResult result = RunReleaseCase( testCase.mode, testCase.steer, traceDirectory );
		PrintResult( result );
		results.push_back( result );
	}

	bool finite = true;
	bool traceOutputOk = true;
	for ( const ReleaseResult& result : results )
	{
		finite = finite && result.finite;
		traceOutputOk = traceOutputOk && ( result.traceRequested == false || result.traceWritten );
	}

	bool spherePositiveReturns = ReturnedWhileRolling( results[0] );
	bool sphereNegativeDefect = ReproducesFullLockDefect( results[1] );
	bool sphereDirectionalAsymmetry = spherePositiveReturns && sphereNegativeDefect;
	bool torusReferencesReturn = ReturnedWhileRolling( results[2] ) && ReturnedWhileRolling( results[3] );
	bool wheelTransitionBracketObserved = ReturnedWhileRolling( results[4] ) && ReturnedWhileRolling( results[5] ) &&
		ReproducesFullLockDefect( results[6] ) && ReproducesFullLockDefect( results[7] );
	bool wheelFullLockDefect = ReproducesFullLockDefect( results[8] ) && ReproducesFullLockDefect( results[9] );
	bool denseSweepObserved = true;
	if ( denseSweep )
	{
		DenseSweepSideSummary positive = SummarizeDenseSweepSide( results, true );
		DenseSweepSideSummary negative = SummarizeDenseSweepSide( results, false );
		denseSweepObserved = DenseSweepSideObserved( positive ) && DenseSweepSideObserved( negative );
		std::printf(
			"dense_transition positive_last_return=%.2f positive_first_defect=%.2f positive_monotonic=%s "
			"negative_last_return=%.2f negative_first_defect=%.2f negative_monotonic=%s "
			"directionally_asymmetric=%s observed=%s\n",
			positive.lastReturnAmplitude, positive.firstDefectAmplitude, positive.monotonic ? "true" : "false",
			negative.lastReturnAmplitude, negative.firstDefectAmplitude, negative.monotonic ? "true" : "false",
			std::fabs( positive.firstDefectAmplitude - negative.firstDefectAmplitude ) > 0.001f ? "true" : "false",
			denseSweepObserved ? "true" : "false" );
	}

	std::printf( "evidence finite=%s trace_output_ok=%s sphere_directional_asymmetry=%s torus_bidirectional_return=%s "
				 "b3wheel_transition_bracket_0_4_to_0_5=%s b3wheel_full_lock_known_defect=%s product_acceptance=false\n",
				 finite ? "true" : "false", traceOutputOk ? "true" : "false",
				 sphereDirectionalAsymmetry ? "true" : "false",
				 torusReferencesReturn ? "true" : "false", wheelTransitionBracketObserved ? "true" : "false",
				 wheelFullLockDefect ? "true" : "false" );

	if ( finite && traceOutputOk && sphereDirectionalAsymmetry && torusReferencesReturn && wheelTransitionBracketObserved &&
		 wheelFullLockDefect && denseSweepObserved )
	{
		std::printf( "JV STEERING RELEASE EVIDENCE LOCK: PASS (known defect reproduced; no fix claimed)\n" );
		return 0;
	}

	std::printf( "JV STEERING RELEASE EVIDENCE LOCK: FAILED OR INCONCLUSIVE\n" );
	return 1;
}
