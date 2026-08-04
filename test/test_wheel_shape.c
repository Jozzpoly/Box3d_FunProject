// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "test_macros.h"

#include "box3d/box3d.h"
#include "box3d/collision.h"
#include "box3d/constants.h"
#include "box3d/math_functions.h"

static b3Wheel MakeCrownedWheel( void )
{
	// Odd point count gives one real crown vertex. The shoulders are close
	// enough to enter the speculative band under deeper overlap, which is the
	// confound WHEEL-RIGID-01 must remove from manifold topology.
	b3Vec2 profile[5] = {
		{ -0.20f, 0.480f },
		{ -0.10f, 0.497f },
		{ 0.00f, 0.500f },
		{ 0.10f, 0.497f },
		{ 0.20f, 0.480f },
	};
	return b3MakeWheelProfile( b3Vec3_zero, b3Vec3_axisZ, profile, ARRAY_COUNT( profile ), 0.014f );
}

static b3Wheel MakeFlatWheel( void )
{
	return b3MakeWheel( b3Vec3_zero, b3Vec3_axisZ, 0.514f, 0.21875f, 0.014f );
}

static void CollideWithHorizontalTriangle( b3LocalManifold* manifold, b3LocalManifoldPoint* points, int capacity,
										   const b3Wheel* wheel, float separation, b3Quat wheelRotation )
{
	// In world space the wheel center is zero and the ground is horizontal.
	// Transform the triangle into the wheel frame so changing wheelRotation is
	// a true spin/camber test of the low-level manifold.
	float groundY = -wheel->radius - separation;
	b3Vec3 worldVertices[3] = {
		{ -3.0f, groundY, -3.0f },
		{ 0.0f, groundY, 3.0f },
		{ 3.0f, groundY, -3.0f },
	};
	b3Vec3 localVertices[3];
	for ( int i = 0; i < 3; ++i )
	{
		localVertices[i] = b3InvRotateVector( wheelRotation, worldVertices[i] );
	}

	*manifold = ( b3LocalManifold ){ 0 };
	manifold->points = points;
	b3CollideWheelAndTriangle( manifold, capacity, wheel, localVertices[0], localVertices[1], localVertices[2] );
}

static int WheelCrownUsesOneStrictSupportVertex( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	b3Vec2 normalized[B3_MAX_WHEEL_PROFILE_POINTS];
	ENSURE( b3GetWheelProfile( &wheel, normalized ) == 5 );

	const float separations[] = { 0.018f, -0.005f, -0.030f };
	uint8_t feature = UINT8_MAX;
	for ( int i = 0; i < ARRAY_COUNT( separations ); ++i )
	{
		b3LocalManifoldPoint points[8] = { 0 };
		b3LocalManifold manifold;
		CollideWithHorizontalTriangle( &manifold, points, ARRAY_COUNT( points ), &wheel, separations[i], b3Quat_identity );

		ENSURE( manifold.pointCount == 1 );
		ENSURE( manifold.points[0].pair.index1 == 2 );
		ENSURE( manifold.points[0].pair.index2 == 2 );
		ENSURE_SMALL( manifold.points[0].separation - separations[i], 2.0e-6f );
		if ( i == 0 )
		{
			feature = manifold.points[0].pair.index1;
		}
		else
		{
			ENSURE( manifold.points[0].pair.index1 == feature );
		}
	}
	return 0;
}

static int WheelFlatTreadUsesTwoSupportEndpoints( void )
{
	b3Wheel wheel = MakeFlatWheel();
	const float separations[] = { 0.018f, -0.005f, -0.030f };

	for ( int i = 0; i < ARRAY_COUNT( separations ); ++i )
	{
		b3LocalManifoldPoint points[8] = { 0 };
		b3LocalManifold manifold;
		CollideWithHorizontalTriangle( &manifold, points, ARRAY_COUNT( points ), &wheel, separations[i], b3Quat_identity );

		ENSURE( manifold.pointCount == 2 );
		ENSURE( manifold.points[0].pair.index1 == 0 );
		ENSURE( manifold.points[1].pair.index1 == 1 );
		ENSURE( manifold.points[0].pair.index1 != manifold.points[1].pair.index1 );
		ENSURE_SMALL( manifold.points[0].separation - separations[i], 2.0e-6f );
		ENSURE_SMALL( manifold.points[1].separation - separations[i], 2.0e-6f );
	}
	return 0;
}

static int WheelSupportFeaturesSurviveSpin( void )
{
	b3Wheel crown = MakeCrownedWheel();
	b3Wheel flat = MakeFlatWheel();
	b3Quat spin = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 1.173f );

	b3LocalManifoldPoint crownPoints[8] = { 0 };
	b3LocalManifold crownManifold;
	CollideWithHorizontalTriangle( &crownManifold, crownPoints, ARRAY_COUNT( crownPoints ), &crown, -0.005f, spin );
	ENSURE( crownManifold.pointCount == 1 );
	ENSURE( crownManifold.points[0].pair.index1 == 2 );

	b3LocalManifoldPoint flatPoints[8] = { 0 };
	b3LocalManifold flatManifold;
	CollideWithHorizontalTriangle( &flatManifold, flatPoints, ARRAY_COUNT( flatPoints ), &flat, -0.005f, spin );
	ENSURE( flatManifold.pointCount == 2 );
	ENSURE( flatManifold.points[0].pair.index1 == 0 );
	ENSURE( flatManifold.points[1].pair.index1 == 1 );
	return 0;
}

static int WheelCamberSelectsOneEndpoint( void )
{
	b3Wheel wheel = MakeFlatWheel();
	b3Quat camber = b3MakeQuatFromAxisAngle( b3Vec3_axisX, 5.0f * B3_PI / 180.0f );

	b3LocalManifoldPoint points[8] = { 0 };
	b3LocalManifold manifold;
	CollideWithHorizontalTriangle( &manifold, points, ARRAY_COUNT( points ), &wheel, -0.005f, camber );

	ENSURE( manifold.pointCount == 1 );
	ENSURE( manifold.points[0].pair.index1 == 1 );
	return 0;
}

static int WheelManifoldHonorsCallerCapacity( void )
{
	b3Wheel wheel = MakeFlatWheel();
	b3LocalManifoldPoint point = { 0 };
	b3LocalManifold manifold;
	CollideWithHorizontalTriangle( &manifold, &point, 1, &wheel, -0.005f, b3Quat_identity );

	ENSURE( manifold.pointCount == 1 );
	ENSURE( manifold.points[0].pair.index1 == 0 );
	return 0;
}

static int WheelOutsideSpeculativeDistanceHasNoContact( void )
{
	b3Wheel crown = MakeCrownedWheel();
	b3Wheel flat = MakeFlatWheel();
	float separation = B3_SPECULATIVE_DISTANCE + 0.001f;

	b3LocalManifoldPoint crownPoints[2] = { 0 };
	b3LocalManifold crownManifold;
	CollideWithHorizontalTriangle( &crownManifold, crownPoints, ARRAY_COUNT( crownPoints ), &crown, separation,
								   b3Quat_identity );
	ENSURE( crownManifold.pointCount == 0 );

	b3LocalManifoldPoint flatPoints[2] = { 0 };
	b3LocalManifold flatManifold;
	CollideWithHorizontalTriangle( &flatManifold, flatPoints, ARRAY_COUNT( flatPoints ), &flat, separation,
								   b3Quat_identity );
	ENSURE( flatManifold.pointCount == 0 );
	return 0;
}

static int WheelTriangleBackFaceDoesNotCollide( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	float planeY = wheel.radius - 0.005f;
	b3Vec3 v1 = { -2.0f, planeY, -2.0f };
	b3Vec3 v2 = { 0.0f, planeY, 2.0f };
	b3Vec3 v3 = { 2.0f, planeY, -2.0f };

	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold = { 0 };
	manifold.points = points;
	b3CollideWheelAndTriangle( &manifold, ARRAY_COUNT( points ), &wheel, v1, v2, v3 );
	ENSURE( manifold.pointCount == 0 );
	return 0;
}

static int WheelTriangleEdgeFallbackKeepsContact( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	float groundY = -wheel.radius + 0.005f;
	// The strict face support projects to (0, groundY, 0), just outside this
	// triangle. Its edge at x=0.05 still cuts the rounded wheel footprint, so
	// dropping the face point must fall back to a real edge contact.
	b3Vec3 v1 = { 0.05f, groundY, -1.0f };
	b3Vec3 v2 = { 0.05f, groundY, 1.0f };
	b3Vec3 v3 = { 2.0f, groundY, 0.0f };

	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold = { 0 };
	manifold.points = points;
	b3CollideWheelAndTriangle( &manifold, ARRAY_COUNT( points ), &wheel, v1, v2, v3 );

	ENSURE( manifold.pointCount == 1 );
	ENSURE( manifold.feature == b3_featureEdge1 );
	ENSURE( manifold.points[0].pair.index1 == (uint8_t)b3_featureEdge1 );
	ENSURE( manifold.points[0].pair.owner2 == 1 );
	ENSURE( manifold.points[0].separation <= 0.0f );
	ENSURE( b3Dot( manifold.normal, b3Vec3_axisY ) > 0.5f );
	return 0;
}

static int WheelTriangleVertexFallbackKeepsContact( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	float groundY = -wheel.radius + 0.005f;
	// The triangle occupies the +x,+z quadrant. Its nearest corner lies inside
	// the rounded crown even though the wheel's face support at x=z=0 does not
	// lie in the triangle.
	b3Vec3 v1 = { 0.04f, groundY, 0.04f };
	b3Vec3 v2 = { 0.04f, groundY, 1.0f };
	b3Vec3 v3 = { 1.0f, groundY, 0.04f };

	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold = { 0 };
	manifold.points = points;
	b3CollideWheelAndTriangle( &manifold, ARRAY_COUNT( points ), &wheel, v1, v2, v3 );

	ENSURE( manifold.pointCount == 1 );
	ENSURE( manifold.feature == b3_featureVertex1 );
	ENSURE( manifold.points[0].pair.index1 == (uint8_t)b3_featureVertex1 );
	ENSURE( manifold.points[0].pair.owner2 == 1 );
	ENSURE( manifold.points[0].separation <= 0.0f );
	ENSURE( b3Dot( manifold.normal, b3Vec3_axisY ) > 0.5f );
	return 0;
}

static int WheelTriangleEdgeFeatureSurvivesSpin( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	float groundY = -wheel.radius + 0.005f;
	b3Vec3 worldVertices[3] = {
		{ 0.05f, groundY, -1.0f },
		{ 0.05f, groundY, 1.0f },
		{ 2.0f, groundY, 0.0f },
	};
	const float angles[] = { 0.0f, 0.37f, 1.19f, 2.41f };
	b3FeaturePair feature = { 0 };

	for ( int sample = 0; sample < ARRAY_COUNT( angles ); ++sample )
	{
		b3Quat spin = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, angles[sample] );
		b3Vec3 localVertices[3];
		for ( int i = 0; i < 3; ++i )
		{
			localVertices[i] = b3InvRotateVector( spin, worldVertices[i] );
		}

		b3LocalManifoldPoint points[4] = { 0 };
		b3LocalManifold manifold = { 0 };
		manifold.points = points;
		b3CollideWheelAndTriangle( &manifold, ARRAY_COUNT( points ), &wheel, localVertices[0], localVertices[1],
									   localVertices[2] );

		ENSURE( manifold.pointCount == 1 );
		ENSURE( manifold.feature == b3_featureEdge1 );
		if ( sample == 0 )
		{
			feature = manifold.points[0].pair;
		}
		else
		{
			ENSURE( manifold.points[0].pair.owner1 == feature.owner1 );
			ENSURE( manifold.points[0].pair.index1 == feature.index1 );
			ENSURE( manifold.points[0].pair.owner2 == feature.owner2 );
			ENSURE( manifold.points[0].pair.index2 == feature.index2 );
		}
	}
	return 0;
}

static int WheelTriangleDistantBoundaryHasNoContact( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	float groundY = -wheel.radius + 0.005f;
	b3Vec3 v1 = { 0.18f, groundY, -1.0f };
	b3Vec3 v2 = { 0.18f, groundY, 1.0f };
	b3Vec3 v3 = { 2.0f, groundY, 0.0f };

	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold = { 0 };
	manifold.points = points;
	b3CollideWheelAndTriangle( &manifold, ARRAY_COUNT( points ), &wheel, v1, v2, v3 );
	ENSURE( manifold.pointCount == 0 );
	return 0;
}

static int WheelCrossesCoplanarTriangleSeamWithoutGap( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	float groundY = -wheel.radius + 0.005f;
	const b3Vec3 triangleA[3] = {
		{ -1.0f, groundY, -1.0f },
		{ -1.0f, groundY, 1.0f },
		{ 1.0f, groundY, 1.0f },
	};
	const b3Vec3 triangleB[3] = {
		{ -1.0f, groundY, -1.0f },
		{ 1.0f, groundY, 1.0f },
		{ 1.0f, groundY, -1.0f },
	};

	for ( int sample = -20; sample <= 20; ++sample )
	{
		float d = 0.001f * (float)sample;
		b3Vec3 wheelPosition = { d, 0.0f, -d };
		int totalPoints = 0;
		const b3Vec3* triangles[2] = { triangleA, triangleB };
		for ( int triangleIndex = 0; triangleIndex < 2; ++triangleIndex )
		{
			b3Vec3 local[3];
			for ( int i = 0; i < 3; ++i )
			{
				local[i] = b3Sub( triangles[triangleIndex][i], wheelPosition );
			}
			b3LocalManifoldPoint points[4] = { 0 };
			b3LocalManifold manifold = { 0 };
			manifold.points = points;
			b3CollideWheelAndTriangle( &manifold, ARRAY_COUNT( points ), &wheel, local[0], local[1], local[2] );
			totalPoints += manifold.pointCount;
		}
		ENSURE( totalPoints >= 1 );
	}
	return 0;
}

static int WheelWorldMeshBoundaryUsesFiniteEdge( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	b3BodyId groundId = b3CreateBody( worldId, &groundDef );
	b3MeshData* meshData = b3CreateGridMesh( 1, 1, 2.0f, 0, true );
	ENSURE( meshData != NULL );
	b3ShapeDef groundShapeDef = b3DefaultShapeDef();
	groundShapeDef.baseMaterial.friction = 0.0f;
	b3CreateMeshShape( groundId, &groundShapeDef, meshData, b3Vec3_one );

	b3Wheel wheel = MakeCrownedWheel();
	b3BodyDef wheelBodyDef = b3DefaultBodyDef();
	wheelBodyDef.type = b3_dynamicBody;
	wheelBodyDef.position = ( b3Pos ){ 1.05f, wheel.radius - 0.005f, 0.0f };
	wheelBodyDef.motionLocks.linearX = true;
	wheelBodyDef.motionLocks.linearY = true;
	wheelBodyDef.motionLocks.linearZ = true;
	wheelBodyDef.motionLocks.angularX = true;
	wheelBodyDef.motionLocks.angularY = true;
	wheelBodyDef.motionLocks.angularZ = true;
	b3BodyId wheelBodyId = b3CreateBody( worldId, &wheelBodyDef );
	b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
	wheelShapeDef.density = 1.0f;
	wheelShapeDef.baseMaterial.friction = 0.0f;
	b3ShapeId wheelShapeId = b3CreateWheelShape( wheelBodyId, &wheelShapeDef, &wheel );

	b3World_Step( worldId, 1.0f / 60.0f, 4 );
	b3ContactData contacts[4];
	int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
	ENSURE( contactCount >= 1 );
	int pointCount = 0;
	for ( int i = 0; i < contactCount; ++i )
	{
		for ( int j = 0; j < contacts[i].manifoldCount; ++j )
		{
			pointCount += contacts[i].manifolds[j].pointCount;
		}
	}
	ENSURE( pointCount >= 1 );

	b3DestroyWorld( worldId );
	b3DestroyMesh( meshData );
	return 0;
}

static int RunLoadedFlatMeshSeamPass( bool reverse, float spinAngle )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = ( b3Vec3 ){ 0.0f, -10.0f, 0.0f };
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	b3BodyId groundId = b3CreateBody( worldId, &groundDef );
	b3MeshData* meshData = b3CreateGridMesh( 1, 1, 2.0f, 0, true );
	ENSURE( meshData != NULL );
	b3ShapeDef groundShapeDef = b3DefaultShapeDef();
	groundShapeDef.baseMaterial.friction = 0.0f;
	b3CreateMeshShape( groundId, &groundShapeDef, meshData, b3Vec3_one );

	b3Wheel wheel = MakeCrownedWheel();
	float side = reverse ? 1.0f : -1.0f;
	b3BodyDef wheelBodyDef = b3DefaultBodyDef();
	wheelBodyDef.type = b3_dynamicBody;
	wheelBodyDef.position = ( b3Pos ){ 0.20f * side, wheel.radius + 0.01f, -0.20f * side };
	wheelBodyDef.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, spinAngle );
	wheelBodyDef.motionLocks.angularX = true;
	wheelBodyDef.motionLocks.angularY = true;
	wheelBodyDef.motionLocks.angularZ = true;
	b3BodyId wheelBodyId = b3CreateBody( worldId, &wheelBodyDef );
	b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
	wheelShapeDef.density = 80.0f;
	wheelShapeDef.baseMaterial.friction = 0.0f;
	b3ShapeId wheelShapeId = b3CreateWheelShape( wheelBodyId, &wheelShapeDef, &wheel );

	for ( int step = 0; step < 180; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	b3Body_SetLinearVelocity( wheelBodyId, ( b3Vec3 ){ -0.50f * side, 0.0f, 0.50f * side } );

	float referenceImpulse = 0.0f;
	b3Vec3 previousNormal = b3Vec3_axisY;
	uint32_t featureId = UINT32_MAX;
	int firstTriangle = B3_NULL_INDEX;
	int lastTriangle = B3_NULL_INDEX;
	int triangleChanges = 0;
	int nonPersistedAfterWarmup = 0;

	for ( int step = 0; step < 60; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
		b3ContactData contacts[4];
		int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
		ENSURE( contactCount >= 1 );

		int pointCount = 0;
		float normalImpulse = 0.0f;
		b3Vec3 contactNormal = b3Vec3_zero;
		uint32_t currentFeatureId = UINT32_MAX;
		int currentTriangle = B3_NULL_INDEX;
		bool persisted = false;
		for ( int i = 0; i < contactCount; ++i )
		{
			for ( int j = 0; j < contacts[i].manifoldCount; ++j )
			{
				const b3Manifold* manifold = contacts[i].manifolds + j;
				contactNormal = manifold->normal;
				for ( int k = 0; k < manifold->pointCount; ++k )
				{
					const b3ManifoldPoint* point = manifold->points + k;
					pointCount += 1;
					normalImpulse += point->totalNormalImpulse;
					currentFeatureId = point->featureId;
					currentTriangle = point->triangleIndex;
					persisted = point->persisted;
				}
			}
		}

		// One solver point proves the two coplanar triangles did not double the
		// normal constraint at their shared edge.
		ENSURE( pointCount == 1 );
		ENSURE( normalImpulse > 0.0f );
		ENSURE( b3Dot( previousNormal, contactNormal ) > 0.9999f );
		previousNormal = contactNormal;

		if ( step == 0 )
		{
			referenceImpulse = normalImpulse;
			featureId = currentFeatureId;
			firstTriangle = currentTriangle;
		}
		else
		{
			float tolerance = b3MaxFloat( 1.0e-4f, 0.05f * referenceImpulse );
			ENSURE_SMALL( normalImpulse - referenceImpulse, tolerance );
			ENSURE( currentFeatureId == featureId );
			if ( currentTriangle != lastTriangle && lastTriangle != B3_NULL_INDEX )
			{
				triangleChanges += 1;
			}
			if ( persisted == false )
			{
				nonPersistedAfterWarmup += 1;
			}
		}
		lastTriangle = currentTriangle;
	}

	ENSURE( firstTriangle != lastTriangle );
	ENSURE( triangleChanges == 1 );
	// The mesh cache keys warm starting by triangleIndex, so the handoff may
	// reset persisted once, but it must not churn on either side of the seam.
	ENSURE( nonPersistedAfterWarmup <= 1 );

	b3DestroyWorld( worldId );
	b3DestroyMesh( meshData );
	return 0;
}

static int WheelWorldMeshSeamHasNoContactGap( void )
{
	const float phases[] = { 0.0f, 0.73f, 1.61f };
	for ( int reverse = 0; reverse < 2; ++reverse )
	{
		for ( int phase = 0; phase < ARRAY_COUNT( phases ); ++phase )
		{
			ENSURE( RunLoadedFlatMeshSeamPass( reverse != 0, phases[phase] ) == 0 );
		}
	}
	return 0;
}

static b3MeshData* MakeFoldedWheelSeamMesh( void )
{
	b3Vec3 vertices[4] = {
		{ 0.0f, 0.0f, -1.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 1.0f, 0.02f, 0.0f },
	};
	int indices[6] = { 0, 1, 2, 0, 2, 3 };
	b3MeshDef meshDef = { 0 };
	meshDef.vertices = vertices;
	meshDef.vertexCount = ARRAY_COUNT( vertices );
	meshDef.indices = indices;
	meshDef.triangleCount = 2;
	meshDef.identifyEdges = true;
	meshDef.useMedianSplit = true;
	return b3CreateMesh( &meshDef, NULL, 0 );
}

static int RunLoadedFoldedMeshSeamPass( bool reverse, float spinAngle )
{
	b3MeshData* meshData = MakeFoldedWheelSeamMesh();
	ENSURE( meshData != NULL );

	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = ( b3Vec3 ){ 0.0f, -10.0f, 0.0f };
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	b3BodyId groundId = b3CreateBody( worldId, &groundDef );
	b3ShapeDef groundShapeDef = b3DefaultShapeDef();
	groundShapeDef.baseMaterial.friction = 0.0f;
	b3CreateMeshShape( groundId, &groundShapeDef, meshData, b3Vec3_one );

	b3Wheel wheel = MakeCrownedWheel();
	float startX = reverse ? 0.20f : -0.20f;
	float startGroundY = reverse ? 0.02f * startX : 0.0f;
	b3BodyDef wheelBodyDef = b3DefaultBodyDef();
	wheelBodyDef.type = b3_dynamicBody;
	wheelBodyDef.position = ( b3Pos ){ startX, startGroundY + wheel.radius + 0.01f, 0.0f };
	wheelBodyDef.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, spinAngle );
	wheelBodyDef.motionLocks.linearX = true;
	wheelBodyDef.motionLocks.linearZ = true;
	wheelBodyDef.motionLocks.angularX = true;
	wheelBodyDef.motionLocks.angularY = true;
	wheelBodyDef.motionLocks.angularZ = true;
	b3BodyId wheelBodyId = b3CreateBody( worldId, &wheelBodyDef );
	b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
	wheelShapeDef.density = 80.0f;
	wheelShapeDef.baseMaterial.friction = 0.0f;
	b3ShapeId wheelShapeId = b3CreateWheelShape( wheelBodyId, &wheelShapeDef, &wheel );

	for ( int step = 0; step < 180; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
	}
	b3MotionLocks movingLocks = { 0 };
	movingLocks.linearZ = true;
	movingLocks.angularX = true;
	movingLocks.angularY = true;
	movingLocks.angularZ = true;
	b3Body_SetMotionLocks( wheelBodyId, movingLocks );
	b3Body_SetLinearVelocity( wheelBodyId, ( b3Vec3 ){ reverse ? -0.50f : 0.50f, 0.0f, 0.0f } );

	float impulses[60];
	b3Vec3 previousNormal = b3Vec3_zero;
	uint32_t featureId = UINT32_MAX;
	int firstTriangle = B3_NULL_INDEX;
	int lastTriangle = B3_NULL_INDEX;
	int triangleChanges = 0;
	int nonPersistedAfterWarmup = 0;

	for ( int step = 0; step < ARRAY_COUNT( impulses ); ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
		b3ContactData contacts[4];
		int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
		ENSURE( contactCount >= 1 );

		int pointCount = 0;
		float normalImpulse = 0.0f;
		b3Vec3 contactNormal = b3Vec3_zero;
		uint32_t currentFeatureId = UINT32_MAX;
		int currentTriangle = B3_NULL_INDEX;
		bool persisted = false;
		for ( int i = 0; i < contactCount; ++i )
		{
			for ( int j = 0; j < contacts[i].manifoldCount; ++j )
			{
				const b3Manifold* manifold = contacts[i].manifolds + j;
				contactNormal = manifold->normal;
				for ( int k = 0; k < manifold->pointCount; ++k )
				{
					const b3ManifoldPoint* point = manifold->points + k;
					pointCount += 1;
					normalImpulse += point->totalNormalImpulse;
					currentFeatureId = point->featureId;
					currentTriangle = point->triangleIndex;
					persisted = point->persisted;
				}
			}
		}

		// A folded seam may legitimately carry one point on each differently
		// oriented face. Coplanar duplicate suppression is tested separately;
		// here the invariant is bounded topology plus continuous total impulse.
		ENSURE( 1 <= pointCount && pointCount <= 2 );
		ENSURE( normalImpulse > 0.0f );
		impulses[step] = normalImpulse;
		if ( step > 0 )
		{
			// The authored fold is 1.15 degrees, so adjacent solved normals should
			// remain much closer than even a five-degree discontinuity.
			ENSURE( b3Dot( previousNormal, contactNormal ) > 0.995f );
			ENSURE( currentFeatureId == featureId );
			if ( currentTriangle != lastTriangle )
			{
				triangleChanges += 1;
			}
			if ( persisted == false )
			{
				nonPersistedAfterWarmup += 1;
			}
		}
		else
		{
			featureId = currentFeatureId;
			firstTriangle = currentTriangle;
		}
		previousNormal = contactNormal;
		lastTriangle = currentTriangle;
	}

	float referenceImpulse = 0.0f;
	for ( int step = 8; step < 16; ++step )
	{
		referenceImpulse += impulses[step];
	}
	referenceImpulse *= 1.0f / 8.0f;
	for ( int step = 4; step < ARRAY_COUNT( impulses ); ++step )
	{
		// Measured before fixing the threshold: +9% uphill and +13.5% downhill.
		// Twenty percent catches a double impulse while leaving physical ramp
		// acceleration and the single triangle-cache handoff alone.
		float tolerance = b3MaxFloat( 1.0e-4f, 0.20f * referenceImpulse );
		ENSURE_SMALL( impulses[step] - referenceImpulse, tolerance );
	}

	ENSURE( firstTriangle != lastTriangle );
	ENSURE( triangleChanges == 1 );
	ENSURE( nonPersistedAfterWarmup <= 1 );

	b3DestroyWorld( worldId );
	b3DestroyMesh( meshData );
	return 0;
}

static int WheelWorldFoldedMeshSeamKeepsNormalAndImpulseContinuous( void )
{
	const float phases[] = { 0.0f, 1.17f };
	for ( int reverse = 0; reverse < 2; ++reverse )
	{
		for ( int phase = 0; phase < ARRAY_COUNT( phases ); ++phase )
		{
			ENSURE( RunLoadedFoldedMeshSeamPass( reverse != 0, phases[phase] ) == 0 );
		}
	}
	return 0;
}

static int WheelWorldFeaturePersistsWhileSpinning( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	groundDef.position = ( b3Pos ){ 0.0f, -0.1f, 0.0f };
	b3BodyId groundId = b3CreateBody( worldId, &groundDef );
	b3BoxHull ground = b3MakeBoxHull( 2.0f, 0.1f, 2.0f );
	b3ShapeDef groundShapeDef = b3DefaultShapeDef();
	groundShapeDef.baseMaterial.friction = 0.0f;
	b3CreateHullShape( groundId, &groundShapeDef, &ground.base );

	b3BodyDef wheelBodyDef = b3DefaultBodyDef();
	wheelBodyDef.type = b3_dynamicBody;
	wheelBodyDef.position = ( b3Pos ){ 0.0f, 0.509f, 0.0f };
	wheelBodyDef.angularVelocity = ( b3Vec3 ){ 0.0f, 0.0f, 8.0f };
	wheelBodyDef.motionLocks.linearX = true;
	wheelBodyDef.motionLocks.linearY = true;
	wheelBodyDef.motionLocks.linearZ = true;
	wheelBodyDef.motionLocks.angularX = true;
	wheelBodyDef.motionLocks.angularY = true;
	b3BodyId wheelBodyId = b3CreateBody( worldId, &wheelBodyDef );

	b3Wheel wheel = MakeCrownedWheel();
	b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
	wheelShapeDef.density = 1.0f;
	wheelShapeDef.baseMaterial.friction = 0.0f;
	b3ShapeId wheelShapeId = b3CreateWheelShape( wheelBodyId, &wheelShapeDef, &wheel );

	uint32_t featureId = UINT32_MAX;
	for ( int step = 0; step < 8; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );

		b3ContactData contacts[1];
		int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
		ENSURE( contactCount == 1 );
		ENSURE( contacts[0].manifoldCount == 1 );
		ENSURE( contacts[0].manifolds[0].pointCount == 1 );
		const b3ManifoldPoint* point = contacts[0].manifolds[0].points;
		if ( step == 0 )
		{
			featureId = point->featureId;
		}
		else
		{
			ENSURE( point->featureId == featureId );
			ENSURE( point->persisted );
		}
	}

	b3DestroyWorld( worldId );
	return 0;
}

int WheelShapeTest( void )
{
	RUN_SUBTEST( WheelCrownUsesOneStrictSupportVertex );
	RUN_SUBTEST( WheelFlatTreadUsesTwoSupportEndpoints );
	RUN_SUBTEST( WheelSupportFeaturesSurviveSpin );
	RUN_SUBTEST( WheelCamberSelectsOneEndpoint );
	RUN_SUBTEST( WheelManifoldHonorsCallerCapacity );
	RUN_SUBTEST( WheelOutsideSpeculativeDistanceHasNoContact );
	RUN_SUBTEST( WheelTriangleBackFaceDoesNotCollide );
	RUN_SUBTEST( WheelTriangleEdgeFallbackKeepsContact );
	RUN_SUBTEST( WheelTriangleVertexFallbackKeepsContact );
	RUN_SUBTEST( WheelTriangleEdgeFeatureSurvivesSpin );
	RUN_SUBTEST( WheelTriangleDistantBoundaryHasNoContact );
	RUN_SUBTEST( WheelCrossesCoplanarTriangleSeamWithoutGap );
	RUN_SUBTEST( WheelWorldMeshBoundaryUsesFiniteEdge );
	RUN_SUBTEST( WheelWorldMeshSeamHasNoContactGap );
	RUN_SUBTEST( WheelWorldFoldedMeshSeamKeepsNormalAndImpulseContinuous );
	RUN_SUBTEST( WheelWorldFeaturePersistsWhileSpinning );
	return 0;
}
