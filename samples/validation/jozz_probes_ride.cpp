// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// Ride-quality diagnosis: what the driver feels at speed, on FLAT ground.
//
// Why this file exists. The owner drove the car on the wheel envelopes and
// reported that they shake and hop at speed, worst under drift. Neither
// instrument in the program could have caught that:
//   - the wheel bench (Q3) rolls ONE wheel over a plate at walking pace, with
//     no steering axis, no chassis and no sideslip;
//   - the validator's stress matrix measures STANDSTILL jitter after the abuse
//     script - the shake DURING the drive is never sampled.
// So the program had no number for the one thing that decides whether a wheel
// is usable. This probe is that number.
//
// Method. Perfectly flat box ground, so every bit of vertical acceleration the
// chassis sees is manufactured by the WHEEL MODEL - a perfectly round wheel on
// flat ground must read zero. Speed is held by a proportional throttle so the
// envelopes are compared at the SAME speed instead of at whatever top speed
// each happens to reach.
//
// Every envelope in the table now carries the same frozen wheel mass (U-32 was
// paid to get CYLINDER and PHASED_UNION onto that footing), so the rows differ
// by SHAPE alone. That matters here more than anywhere: an envelope that
// happened to be lighter would ride differently for a reason that has nothing
// to do with its surface.

#include "jozz_validation_helpers.h"

#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_m6_suspension_rig.h"

#include "box3d/box3d.h"

#include <cmath>
#include <cstdio>

namespace
{

struct RideEnvelope
{
	const char* name;
	int mode;
	int torusSegments;
	int cylinderSides; // 0 = leave the configured default
};

struct RideSample
{
	float meanSpeed;
	float accelRms;	 // chassis vertical acceleration, m/s^2
	float accelMax;	 // worst single step
	float travelRms; // suspension travel motion, mm - is the shake absorbed or passed through?
	float pointsPerWheel;
	float freshPercent; // share of loaded contact points the solver had to re-seed this step
	bool finite;
};

// Loaded contact points on one wheel, and how many of them are brand new.
// A contact the solver has seen before carries its warm-started impulse; a
// fresh one starts from zero. A wheel whose contact set is replaced every step
// gets no warm start at all, which is a force discontinuity on every step.
void SampleWheelContacts( b3BodyId wheelId, int* loadedOut, int* freshOut )
{
	static b3ContactData contacts[128];
	int count = b3Body_GetContactData( wheelId, contacts, 128 );
	for ( int i = 0; i < count; ++i )
	{
		for ( int m = 0; m < contacts[i].manifoldCount; ++m )
		{
			const b3Manifold* manifold = &contacts[i].manifolds[m];
			for ( int k = 0; k < manifold->pointCount; ++k )
			{
				const b3ManifoldPoint* point = &manifold->points[k];
				if ( point->totalNormalImpulse > 0.0f )
				{
					*loadedOut += 1;
					if ( point->persisted == false )
					{
						*freshOut += 1;
					}
				}
			}
		}
	}
}

// One run: build the car on the given envelope, hold targetSpeed, then measure
// over a window. steerHold != 0 turns the measurement window into a corner.
// Flat ground made of TRIANGLES instead of a box. The product map is a mesh and
// the mesh contact path in the engine is a different one, so "smooth on a box"
// does not imply "smooth on the map" - it has to be measured, not assumed.
b3BodyId CreateFlatMeshGround( b3WorldId worldId, b3MeshData** meshOut )
{
	b3World_EnableContinuous( worldId, false ); // same as the box ground: vehicle worlds run without CCD
	*meshOut = b3CreateGridMesh( 400, 400, 1.0f, 0, true );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	b3BodyId groundId = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.8f;
	shapeDef.filter.categoryBits = JOZZ_M6_TERRAIN_CATEGORY; // same tagging as the box ground
	b3CreateMeshShape( groundId, &shapeDef, *meshOut, b3Vec3_one );
	return groundId;
}

RideSample MeasureRide( const JozzVehiclePrimitiveDefaults& defaults, const RideEnvelope& envelope, float targetSpeed,
						float steerHold, int subStepCount, bool meshGround = false )
{
	RideSample out = {};

	JozzVehicleM6Config config =
		JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
	config.wheelEnvelope.mode = envelope.mode;
	if ( envelope.torusSegments > 0 )
	{
		config.wheelEnvelope.torusSegments = envelope.torusSegments;
	}
	if ( envelope.cylinderSides > 0 )
	{
		config.wheelEnvelope.cylinderSides = envelope.cylinderSides;
	}

	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );
	b3MeshData* mesh = nullptr;
	b3BodyId groundId = meshGround ? CreateFlatMeshGround( worldId, &mesh ) : CreateM6SmokeGround( worldId, 0.8f );
	float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
	JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );

	const float timeStep = 1.0f / 60.0f;
	JozzVehicleM6DriveInput input = {};

	auto holdSpeed = [&]( int steps, float steer ) {
		for ( int i = 0; i < steps; ++i )
		{
			float speed = GetJozzVehicleM6ForwardSpeed( vehicle );
			input.drive = b3ClampFloat( ( targetSpeed - speed ) * 0.6f, -1.0f, 1.0f );
			input.steer = steer;
			UpdateJozzVehicleM6Drive( vehicle, input );
			b3World_Step( worldId, timeStep, subStepCount );
		}
	};

	holdSpeed( 120, 0.0f ); // settle on the ground, throttle already working
	holdSpeed( 300, 0.0f ); // spin up to the target and let the suspension forget the launch

	const int windowSteps = 240;
	double accelSumSq = 0.0;
	double travelSumSq = 0.0;
	double speedSum = 0.0;
	double loadedSum = 0.0;
	double freshSum = 0.0;
	float previousVy = (float)b3Body_GetLinearVelocity( vehicle.chassisId ).y;
	float previousTravel[JOZZ_M6_CORNER_COUNT];
	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		previousTravel[corner] = GetJozzVehicleM6WheelTelemetry( vehicle, corner ).suspensionTravel;
	}

	for ( int i = 0; i < windowSteps; ++i )
	{
		float speed = GetJozzVehicleM6ForwardSpeed( vehicle );
		input.drive = b3ClampFloat( ( targetSpeed - speed ) * 0.6f, -1.0f, 1.0f );
		input.steer = steerHold;
		UpdateJozzVehicleM6Drive( vehicle, input );
		b3World_Step( worldId, timeStep, subStepCount );

		float vy = (float)b3Body_GetLinearVelocity( vehicle.chassisId ).y;
		float accel = ( vy - previousVy ) / timeStep;
		previousVy = vy;
		accelSumSq += (double)accel * (double)accel;
		if ( std::fabs( accel ) > out.accelMax )
		{
			out.accelMax = std::fabs( accel );
		}
		speedSum += (double)GetJozzVehicleM6ForwardSpeed( vehicle );

		int loaded = 0;
		int fresh = 0;
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			SampleWheelContacts( vehicle.corners[corner].wheelId, &loaded, &fresh );
			float travel = GetJozzVehicleM6WheelTelemetry( vehicle, corner ).suspensionTravel;
			float rate = ( travel - previousTravel[corner] ) * 1000.0f; // mm per step
			previousTravel[corner] = travel;
			travelSumSq += (double)rate * (double)rate;
		}
		loadedSum += (double)loaded;
		freshSum += (double)fresh;
	}

	out.meanSpeed = (float)( speedSum / windowSteps );
	out.accelRms = (float)std::sqrt( accelSumSq / windowSteps );
	out.travelRms = (float)std::sqrt( travelSumSq / ( windowSteps * JOZZ_M6_CORNER_COUNT ) );
	out.pointsPerWheel = (float)( loadedSum / ( windowSteps * JOZZ_M6_CORNER_COUNT ) );
	out.freshPercent = loadedSum > 0.0 ? (float)( 100.0 * freshSum / loadedSum ) : 0.0f;
	out.finite = IsM6VehicleStateValid( vehicle );

	b3DestroyWorld( worldId );
	if ( mesh != nullptr )
	{
		b3DestroyMesh( mesh );
	}
	return out;
}

} // namespace

// Diagnosis, not a gate: it prints the ride numbers per envelope and only
// asserts that every run stayed finite. What counts as "smooth enough" is a
// feel question and belongs to the owner - the probe exists so that answer can
// be attached to a number instead of a memory.
bool RunRideQualityDiagnosisProbe( const JozzVehiclePrimitiveDefaults& defaults )
{
	std::printf( "ride quality diagnosis probe (flat ground - a perfect wheel reads zero):\n" );
	bool allFinite = true;
	bool ok2 = true;

	const RideEnvelope envelopes[] = {
		{ "sfera", JOZZ_M6_ENVELOPE_SPHERE, 0, 0 },
		{ "sfera+walec (domyslne)", JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL, 0, 0 },
		{ "walec (1 ksztalt)", JOZZ_M6_ENVELOPE_CYLINDER, 0, 0 },
		{ "union (3 warstwy)", JOZZ_M6_ENVELOPE_PHASED_UNION, 0, 0 },
		{ "opona-16", JOZZ_M6_ENVELOPE_TORUS, 16, 0 },
		{ "opona-32", JOZZ_M6_ENVELOPE_TORUS, 32, 0 },
		{ "opona-64", JOZZ_M6_ENVELOPE_TORUS, 64, 0 },
		{ "KOLO (nowy ksztalt)", JOZZ_M6_ENVELOPE_WHEEL, 0, 0 },
	};
	const float speeds[] = { 8.0f, 16.0f, 24.0f };

	const char* header = "  %-24s %6s %7s %8s %8s %9s %8s %8s\n";
	const char* row = "  %-24s %6.1f %7.1f %8.3f %8.1f %9.4f %8.2f %7.1f%%\n";
	std::printf( header, "obwiednia", "v_zad", "v_real", "a_rms", "a_max", "skok_rms", "pkt/kolo", "swieze%" );
	for ( const RideEnvelope& envelope : envelopes )
	{
		for ( float speed : speeds )
		{
			RideSample sample = MeasureRide( defaults, envelope, speed, 0.0f, 4 );
			std::printf( row, envelope.name, speed, sample.meanSpeed, sample.accelRms, sample.accelMax,
						 sample.travelRms, sample.pointsPerWheel, sample.freshPercent );
			allFinite &= sample.finite;
		}
	}

	std::printf( "  --- w zakrecie (v_zad 16 m/s, skret 0.5) ---\n" );
	for ( const RideEnvelope& envelope : envelopes )
	{
		RideSample sample = MeasureRide( defaults, envelope, 16.0f, 0.5f, 4 );
		std::printf( row, envelope.name, 16.0f, sample.meanSpeed, sample.accelRms, sample.accelMax, sample.travelRms,
					 sample.pointsPerWheel, sample.freshPercent );
		allFinite &= sample.finite;
	}

	// Does the solver simply need more time? The wheel bench found that the
	// shape-count loss bias was under-convergence at 4 substeps and nearly gone
	// at 32. If the shake is the same illness, substeps cure it; if it is the
	// contact set being rebuilt from scratch every step, they will not, because
	// substeps do not give a discarded warm start back.
	std::printf( "  --- podkroki solvera (v_zad 16 m/s, prosto) ---\n" );
	const int subStepCounts[] = { 4, 8, 16, 32 };
	const RideEnvelope subStepEnvelopes[] = {
		{ "sfera", JOZZ_M6_ENVELOPE_SPHERE, 0, 0 },
		{ "opona-32", JOZZ_M6_ENVELOPE_TORUS, 32, 0 },
	};
	for ( const RideEnvelope& envelope : subStepEnvelopes )
	{
		for ( int subSteps : subStepCounts )
		{
			RideSample sample = MeasureRide( defaults, envelope, 16.0f, 0.0f, subSteps );
			char label[40];
			std::snprintf( label, sizeof( label ), "%s @ %d podkr.", envelope.name, subSteps );
			std::printf( row, label, 16.0f, sample.meanSpeed, sample.accelRms, sample.accelMax, sample.travelRms,
						 sample.pointsPerWheel, sample.freshPercent );
			allFinite &= sample.finite;
		}
	}

	// Would a finer wheel fix it? The radial ripple of an N-gon falls off as
	// 1-cos(pi/N), so a 6-sided wheel has ~28x the ripple of a 32-sided one. If
	// the shake came from that ripple, this column has to collapse with N. If it
	// does not, tessellation is not the cure and no shape budget buys one.
	std::printf( "  --- scianki walca (v_zad 16 m/s, prosto) ---\n" );
	const int sideCounts[] = { 6, 12, 24, 32 };
	for ( int sides : sideCounts )
	{
		RideEnvelope envelope = { "walec", JOZZ_M6_ENVELOPE_CYLINDER, 0, sides };
		RideSample sample = MeasureRide( defaults, envelope, 16.0f, 0.0f, 4 );
		char label[40];
		std::snprintf( label, sizeof( label ), "walec %d scianek", sides );
		std::printf( row, label, 16.0f, sample.meanSpeed, sample.accelRms, sample.accelMax, sample.travelRms,
					 sample.pointsPerWheel, sample.freshPercent );
		allFinite &= sample.finite;
	}

	// The map is a mesh, not a box, and the engine reaches triangles through a
	// different contact path. Same measurement, triangle ground.
	std::printf( "  --- grunt z TROJKATOW (v_zad 16 m/s, prosto) ---\n" );
	const RideEnvelope meshEnvelopes[] = {
		{ "sfera", JOZZ_M6_ENVELOPE_SPHERE, 0, 0 },
		{ "opona-64", JOZZ_M6_ENVELOPE_TORUS, 64, 0 },
		{ "KOLO (nowy ksztalt)", JOZZ_M6_ENVELOPE_WHEEL, 0, 0 },
	};
	for ( const RideEnvelope& envelope : meshEnvelopes )
	{
		RideSample sample = MeasureRide( defaults, envelope, 16.0f, 0.0f, 4, true );
		char label[40];
		std::snprintf( label, sizeof( label ), "%s /mesh", envelope.name );
		std::printf( row, label, 16.0f, sample.meanSpeed, sample.accelRms, sample.accelMax, sample.travelRms,
					 sample.pointsPerWheel, sample.freshPercent );
		allFinite &= sample.finite;
	}

	// The wheel shape is new, so the engine's ray cast had to be written for it
	// too. Without this the shape is invisible to picking and to ground probes -
	// silently, because the unknown-type branch just reports a miss. Cast down
	// the middle of a front wheel and check the hit lands on the tread.
	{
		JozzVehicleM6Config config =
			JozzVehicleM6DefaultConfig( defaults.wheelRadius, defaults.wheelWidth, defaults.assetSuspensionTravelHint );
		config.wheelEnvelope.mode = JOZZ_M6_ENVELOPE_WHEEL;

		b3WorldDef worldDef = b3DefaultWorldDef();
		b3WorldId worldId = b3CreateWorld( &worldDef );
		b3BodyId groundId = CreateM6SmokeGround( worldId, 0.8f );
		float spawnHeight = config.restDrop + config.wheelEnvelope.radius + 0.05f;
		JozzVehicleM6 vehicle = CreateJozzVehicleM6( worldId, groundId, config, { 0.0f, spawnHeight, 0.0f } );
		for ( int i = 0; i < 120; ++i )
		{
			b3World_Step( worldId, 1.0f / 60.0f, 4 );
		}

		b3Pos wheelCenter = b3Body_GetPosition( vehicle.corners[JOZZ_M6_FRONT_LEFT].wheelId );
		b3Pos origin = { wheelCenter.x, wheelCenter.y + 2.0f, wheelCenter.z };
		b3WorldCastOutput hit =
			b3Shape_RayCast( vehicle.corners[JOZZ_M6_FRONT_LEFT].wheelShapeIds[0], origin, { 0.0f, -3.0f, 0.0f } );
		float hitDrop = (float)( origin.y - hit.point.y );
		float expected = 2.0f - config.wheelEnvelope.radius;
		std::printf( "  promien w kolo: trafienie %s, zjazd %.4f m (oczekiwane %.4f)\n", hit.hit ? "TAK" : "NIE",
					 hitDrop, expected );
		ok2 = CheckTrue( "ray cast hits the new wheel shape on its tread",
						 hit.hit && std::fabs( hitDrop - expected ) < 0.02f );
		b3DestroyWorld( worldId );
	}

	bool ok = CheckTrue( "ride diagnosis runs stay finite", allFinite ) && ok2;
	std::printf( "ride quality diagnosis probe: %s\n", ok ? "ok" : "FAILED" );
	return ok;
}
