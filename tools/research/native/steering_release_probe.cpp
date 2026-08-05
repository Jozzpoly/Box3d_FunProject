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
	float meanLoadedContactPoints = 0.0f;
	float meanContactManifolds = 0.0f;
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
		   IsFinite( result.tieRodTransverseForceTime ) && IsFinite( result.meanLoadedContactPoints ) &&
		   IsFinite( result.meanContactManifolds );
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

float ProjectImpulseTorque( b3Pos point, b3Vec3 impulse, b3Pos axisPoint, b3Vec3 axis )
{
	b3Vec3 lever = b3SubPos( point, axisPoint );
	return b3Dot( b3Cross( lever, impulse ), axis );
}

ContactAxisSample SampleCornerContactAxisImpulse( const JozzVehicleM6CornerRuntime& runtime )
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
					b3Vec3 impulse = b3MulSV( point.totalNormalImpulse, normal );
					b3Pos worldPoint = b3OffsetPos( wheelCom, anchor );
					out.normalAxisImpulse += ProjectImpulseTorque( worldPoint, impulse, axisPoint, axis );
				}

				if ( manifold.pointCount > 0 )
				{
					averageAnchor = b3MulSV( 1.0f / static_cast<float>( manifold.pointCount ), averageAnchor );
					b3Pos frictionPoint = b3OffsetPos( wheelCom, averageAnchor );
					b3Vec3 frictionImpulse = b3MulSV( sign, manifold.frictionImpulse );
					out.frictionImpulse += b3Length( frictionImpulse );
					out.frictionAxisImpulse += ProjectImpulseTorque( frictionPoint, frictionImpulse, axisPoint, axis );
				}

				b3Vec3 twistImpulse = b3MulSV( sign * manifold.twistImpulse, manifold.normal );
				b3Vec3 rollingImpulse = b3MulSV( sign, manifold.rollingImpulse );
				out.twistAxisImpulse += b3Dot( twistImpulse, axis );
				out.rollingAxisImpulse += b3Dot( rollingImpulse, axis );
			}
		}
	}

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
	std::fprintf( file, "# schema=jv-steering-release-trace/v1\n" );
	std::fprintf( file, "# commit=%s tree=%s dirty=%s build_type=%s\n", JV_PROBE_SOURCE_COMMIT,
				  JV_PROBE_SOURCE_TREE, JV_PROBE_SOURCE_DIRTY, JV_PROBE_BUILD_TYPE );
	std::fprintf( file, "# envelope=%s mode=%d steer=%+.6f artificial_centering=false\n", envelope, mode, steerInput );
	std::fprintf( file,
		"step,time_s,left_angle_deg,right_angle_deg,forward_speed_mps,alignment_deg,yaw_rate_rps,"
		"lateral_speed_mps,rack_translation_m,rack_speed_mps,rack_motor_force_N,rack_max_force_N,"
		"tie_transverse_force_N,left_normal_axis_angular_impulse_N_m_s,right_normal_axis_angular_impulse_N_m_s,"
		"left_friction_axis_angular_impulse_N_m_s,right_friction_axis_angular_impulse_N_m_s,"
		"left_twist_axis_angular_impulse_N_m_s,right_twist_axis_angular_impulse_N_m_s,"
		"left_rolling_axis_angular_impulse_N_m_s,right_rolling_axis_angular_impulse_N_m_s,"
		"loaded_contact_points,contact_manifolds\n" );
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

	result.releaseLeftDeg =
		kDegreesPerRadian * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
	result.releaseRightDeg =
		kDegreesPerRadian * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT ).steeringAngle;
	result.releaseAngleDeg = result.releaseLeftDeg;
	result.peakAbsAngleDeg = b3MaxFloat( std::fabs( result.releaseLeftDeg ), std::fabs( result.releaseRightDeg ) );
	result.releaseSpeed = GetJozzVehicleM6ForwardSpeed( vehicle );
	result.releaseAlignmentDeg = kDegreesPerRadian * GetJozzVehicleM6AlignmentAngle( vehicle );
	result.rackTranslationAtRelease = B3_IS_NON_NULL( vehicle.rackJointId )
									 ? b3PrismaticJoint_GetTranslation( vehicle.rackJointId )
									 : 0.0f;

	// This is the owner-reported event: both controls are released together.
	input.drive = 0.0f;
	input.steer = 0.0f;

	double loadedPointSum = 0.0;
	double manifoldSum = 0.0;
	for ( int step = 0; step < kReleaseSteps; ++step )
	{
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, kTimeStep, kSubStepCount );

		float leftAngleDeg =
			kDegreesPerRadian * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_LEFT ).steeringAngle;
		float rightAngleDeg =
			kDegreesPerRadian * GetJozzVehicleM6WheelTelemetry( vehicle, JOZZ_M6_FRONT_RIGHT ).steeringAngle;
		float speed = GetJozzVehicleM6ForwardSpeed( vehicle );
		result.finalLeftDeg = leftAngleDeg;
		result.finalRightDeg = rightAngleDeg;
		result.finalAngleDeg = leftAngleDeg;
		result.finalSpeed = speed;
		result.peakAbsAngleDeg = b3MaxFloat( result.peakAbsAngleDeg,
			b3MaxFloat( std::fabs( leftAngleDeg ), std::fabs( rightAngleDeg ) ) );
		if ( result.centerTimeSeconds < 0.0f && std::fabs( leftAngleDeg ) <= kCenterThresholdDeg &&
			 std::fabs( rightAngleDeg ) <= kCenterThresholdDeg )
		{
			result.centerTimeSeconds = static_cast<float>( step + 1 ) * kTimeStep;
			result.centerSpeed = speed;
		}

		ContactAxisSample left = SampleCornerContactAxisImpulse( vehicle.corners[JOZZ_M6_FRONT_LEFT] );
		ContactAxisSample right = SampleCornerContactAxisImpulse( vehicle.corners[JOZZ_M6_FRONT_RIGHT] );
		result.leftNormalAxisImpulse += left.normalAxisImpulse;
		result.rightNormalAxisImpulse += right.normalAxisImpulse;
		result.leftFrictionAxisImpulse += left.frictionAxisImpulse;
		result.rightFrictionAxisImpulse += right.frictionAxisImpulse;
		result.contactNormalAxisImpulse += left.normalAxisImpulse + right.normalAxisImpulse;
		result.contactFrictionAxisImpulse += left.frictionAxisImpulse + right.frictionAxisImpulse;
		result.contactTwistAxisImpulse += left.twistAxisImpulse + right.twistAxisImpulse;
		result.contactRollingAxisImpulse += left.rollingAxisImpulse + right.rollingAxisImpulse;
		int loadedPoints = left.loadedPointCount + right.loadedPointCount;
		int manifolds = left.manifoldCount + right.manifoldCount;
		loadedPointSum += static_cast<double>( loadedPoints );
		manifoldSum += static_cast<double>( manifolds );

		float rackTranslation = 0.0f;
		float rackSpeed = 0.0f;
		float motorForce = 0.0f;
		float maxMotorForce = 0.0f;
		if ( B3_IS_NON_NULL( vehicle.rackJointId ) )
		{
			rackTranslation = b3PrismaticJoint_GetTranslation( vehicle.rackJointId );
			rackSpeed = b3PrismaticJoint_GetSpeed( vehicle.rackJointId );
			motorForce = b3PrismaticJoint_GetMotorForce( vehicle.rackJointId );
			maxMotorForce = b3PrismaticJoint_GetMaxMotorForce( vehicle.rackJointId );
			// The public API reports current motor force. Integrating the sampled
			// value over frame time is a force-time diagnostic, not a direct read
			// of the solver's accumulated motor impulse.
			result.rackMotorForceTime += motorForce * kTimeStep;
			result.maxRackMotorForce = b3MaxFloat( result.maxRackMotorForce, std::fabs( maxMotorForce ) );
		}
		float tieTransverseForce = SampleTieRodTransverseLoad( vehicle );
		result.tieRodTransverseForceTime += tieTransverseForce * kTimeStep;

		if ( trace != nullptr )
		{
			b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
			b3Vec3 chassisUp = b3RotateVector( chassisRotation, b3Vec3_axisY );
			b3Vec3 localVelocity = b3InvRotateVector( chassisRotation, b3Body_GetLinearVelocity( vehicle.chassisId ) );
			float alignmentDeg = kDegreesPerRadian * GetJozzVehicleM6AlignmentAngle( vehicle );
			float yawRate = b3Dot( b3Body_GetAngularVelocity( vehicle.chassisId ), chassisUp );
			std::fprintf( trace,
				"%d,%.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,"
				"%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%+.9f,%d,%d\n",
				step, static_cast<double>( step + 1 ) * kTimeStep, leftAngleDeg, rightAngleDeg, speed, alignmentDeg,
				yawRate, localVelocity.z, rackTranslation, rackSpeed, motorForce, maxMotorForce, tieTransverseForce,
				left.normalAxisImpulse, right.normalAxisImpulse, left.frictionAxisImpulse, right.frictionAxisImpulse,
				left.twistAxisImpulse, right.twistAxisImpulse, left.rollingAxisImpulse, right.rollingAxisImpulse,
				loadedPoints, manifolds );
		}
	}

	if ( B3_IS_NON_NULL( vehicle.rackJointId ) )
	{
		result.rackTranslationFinal = b3PrismaticJoint_GetTranslation( vehicle.rackJointId );
		result.rackSpeedFinal = b3PrismaticJoint_GetSpeed( vehicle.rackJointId );
	}
	result.meanLoadedContactPoints = static_cast<float>( loadedPointSum / static_cast<double>( kReleaseSteps ) );
	result.meanContactManifolds = static_cast<float>( manifoldSum / static_cast<double>( kReleaseSteps ) );
	result.finalAlignmentDeg = kDegreesPerRadian * GetJozzVehicleM6AlignmentAngle( vehicle );
	{
		b3Quat chassisRotation = b3Body_GetRotation( vehicle.chassisId );
		b3Vec3 chassisUp = b3RotateVector( chassisRotation, b3Vec3_axisY );
		b3Vec3 localVelocity = b3InvRotateVector( chassisRotation, b3Body_GetLinearVelocity( vehicle.chassisId ) );
		result.finalYawRate = b3Dot( b3Body_GetAngularVelocity( vehicle.chassisId ), chassisUp );
		result.finalLateralSpeed = localVelocity.z;
	}
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
	std::printf(
		"case envelope=%s mode=%d steer=%+.2f class=%s finite=%s trace_requested=%s trace_written=%s "
		"release_deg=%+.4f final_deg=%+.4f "
		"release_left_deg=%+.4f release_right_deg=%+.4f final_left_deg=%+.4f final_right_deg=%+.4f "
		"peak_abs_deg=%.4f release_speed=%.4f final_speed=%.4f release_alignment_deg=%+.4f "
		"final_alignment_deg=%+.4f final_yaw_rate_rps=%+.6f final_lateral_speed_mps=%+.6f "
		"center_time_s=%.4f center_speed=%.4f "
		"rack_release_m=%+.6f rack_final_m=%+.6f rack_speed_final_mps=%+.6f rack_max_force_N=%.3f "
		"contact_normal_axis_angular_impulse_N_m_s=%+.6f contact_friction_axis_angular_impulse_N_m_s=%+.6f "
		"left_normal_axis_angular_impulse_N_m_s=%+.6f right_normal_axis_angular_impulse_N_m_s=%+.6f "
		"left_friction_axis_angular_impulse_N_m_s=%+.6f right_friction_axis_angular_impulse_N_m_s=%+.6f "
		"contact_twist_axis_angular_impulse_N_m_s=%+.6f contact_rolling_axis_angular_impulse_N_m_s=%+.6f "
		"rack_motor_force_time_N_s=%+.6f tie_transverse_force_time_N_s=%.6f mean_loaded_points=%.4f "
		"mean_manifolds=%.4f\n",
		result.envelope, result.mode, result.steerInput, Classify( result ), result.finite ? "true" : "false",
		result.traceRequested ? "true" : "false", result.traceWritten ? "true" : "false", result.releaseAngleDeg,
		result.finalAngleDeg, result.releaseLeftDeg, result.releaseRightDeg, result.finalLeftDeg,
		result.finalRightDeg, result.peakAbsAngleDeg, result.releaseSpeed, result.finalSpeed, result.releaseAlignmentDeg,
		result.finalAlignmentDeg, result.finalYawRate, result.finalLateralSpeed, result.centerTimeSeconds, result.centerSpeed,
		result.rackTranslationAtRelease, result.rackTranslationFinal, result.rackSpeedFinal, result.maxRackMotorForce,
		result.contactNormalAxisImpulse, result.contactFrictionAxisImpulse, result.leftNormalAxisImpulse,
		result.rightNormalAxisImpulse, result.leftFrictionAxisImpulse, result.rightFrictionAxisImpulse,
		result.contactTwistAxisImpulse, result.contactRollingAxisImpulse,
		result.rackMotorForceTime, result.tieRodTransverseForceTime, result.meanLoadedContactPoints,
		result.meanContactManifolds );
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

} // namespace

int main( int argc, char** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const char* traceDirectory = nullptr;
	for ( int i = 1; i < argc; ++i )
	{
		if ( std::strcmp( argv[i], "--trace-dir" ) == 0 && i + 1 < argc )
		{
			traceDirectory = argv[++i];
		}
		else
		{
			std::fprintf( stderr, "usage: %s [--trace-dir DIRECTORY]\n", argv[0] );
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
	std::printf( "jv_steering_release_probe schema=jv-steering-release/v1\n" );
	std::printf( "provenance commit=%s tree=%s dirty=%s build_type=%s\n", JV_PROBE_SOURCE_COMMIT,
				 JV_PROBE_SOURCE_TREE, JV_PROBE_SOURCE_DIRTY, JV_PROBE_BUILD_TYPE );
	std::printf( "receipt wheel_radius_m=0.514000 wheel_width_m=0.437500 suspension_travel_m=0.700000 "
				 "dt=0.016666667 substeps=4 accel_steps=%d steer_steps=%d release_steps=%d "
				 "simultaneous_throttle_and_steering_release=true artificial_centering=false\n",
				 kAccelerationSteps, kSteeringSteps, kReleaseSteps );
	std::printf( "note contact_axis_impulses_are_solver_impulse_diagnostics_not_a_tire_model=true\n" );

	const struct
	{
		int mode;
		float steer;
	} cases[] = {
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

	std::vector<ReleaseResult> results;
	results.reserve( sizeof( cases ) / sizeof( cases[0] ) );
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

	std::printf( "evidence finite=%s trace_output_ok=%s sphere_directional_asymmetry=%s torus_bidirectional_return=%s "
				 "b3wheel_transition_bracket_0_4_to_0_5=%s b3wheel_full_lock_known_defect=%s product_acceptance=false\n",
				 finite ? "true" : "false", traceOutputOk ? "true" : "false",
				 sphereDirectionalAsymmetry ? "true" : "false",
				 torusReferencesReturn ? "true" : "false", wheelTransitionBracketObserved ? "true" : "false",
				 wheelFullLockDefect ? "true" : "false" );

	if ( finite && traceOutputOk && sphereDirectionalAsymmetry && torusReferencesReturn && wheelTransitionBracketObserved &&
		 wheelFullLockDefect )
	{
		std::printf( "JV STEERING RELEASE EVIDENCE LOCK: PASS (known defect reproduced; no fix claimed)\n" );
		return 0;
	}

	std::printf( "JV STEERING RELEASE EVIDENCE LOCK: FAILED OR INCONCLUSIVE\n" );
	return 1;
}
