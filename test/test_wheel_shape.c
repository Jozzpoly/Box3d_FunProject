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


static void CollideWheelWithBoxHull( b3LocalManifold* manifold, b3LocalManifoldPoint* points, int capacity,
									  const b3Wheel* wheel, b3Vec3 boxCenter, b3Vec3 halfExtents )
{
	b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );
	*manifold = ( b3LocalManifold ){ 0 };
	manifold->points = points;
	b3Transform transformBtoA = { boxCenter, b3Quat_identity };
	b3CollideWheelAndHull( manifold, capacity, wheel, &box.base, transformBtoA );
}

static int WheelHullDoesNotUseInfiniteFaceOutsideBox( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold;

	// Each adjacent face plane is only 15 mm outside the wheel support, so the
	// old face-normal-only path accepted the infinite plane. The finite corner
	// is 234 mm away from the wheel surface and must not create a contact.
	CollideWheelWithBoxHull( &manifold, points, ARRAY_COUNT( points ), &wheel,
							 ( b3Vec3 ){ 0.629f, 0.629f, 0.0f }, ( b3Vec3 ){ 0.100f, 0.100f, 0.300f } );

	ENSURE( manifold.pointCount == 0 );
	return 0;
}

static int WheelHullCornerUsesFiniteEdgeNormal( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold;

	// The closest hull feature is the vertical edge at (0.35, 0.35). A plane
	// manifold would report an axis-aligned normal and a point outside the
	// bounded face; the finite hull contact must point towards that edge.
	CollideWheelWithBoxHull( &manifold, points, ARRAY_COUNT( points ), &wheel,
							 ( b3Vec3 ){ 0.450f, 0.450f, 0.0f }, ( b3Vec3 ){ 0.100f, 0.100f, 0.300f } );

	ENSURE( manifold.pointCount == 1 );
	b3Vec3 expected = b3Normalize( ( b3Vec3 ){ 1.0f, 1.0f, 0.0f } );
	ENSURE( b3Dot( manifold.normal, expected ) > 0.98f );
	ENSURE( manifold.points[0].separation < 0.0f );
	return 0;
}

static int WheelHullRejectedFaceFallsBackToBoundaryFeature( void )
{
	// A sphere-like wheel makes the geometry easy to audit. The +X face plane
	// is the preferred SAT axis by less than the tie tolerance, but its support
	// point at y=0 lies outside the finite face whose lower edge is y=0.02.
	// The manifold must therefore use the nearby finite edge/vertex feature,
	// not reinterpret the numeric face index as a vertex index.
	b3Wheel wheel = b3MakeWheel( b3Vec3_zero, b3Vec3_axisZ, 0.500f, 0.500f, 0.500f );
	b3LocalManifoldPoint point = { 0 };
	b3LocalManifold manifold;
	CollideWheelWithBoxHull( &manifold, &point, 1, &wheel, ( b3Vec3 ){ 0.590f, 0.120f, 0.0f },
							 ( b3Vec3 ){ 0.100f, 0.100f, 0.100f } );

	ENSURE( manifold.pointCount == 1 );
	ENSURE( point.pair.owner1 == 1 );
	ENSURE( manifold.normal.x > 0.99f );
	ENSURE( manifold.normal.y > 0.02f );
	ENSURE_SMALL( point.separation - ( sqrtf( 0.490f * 0.490f + 0.020f * 0.020f ) - 0.500f ), 5.0e-4f );
	return 0;
}


static int WheelHullDeepCornerChoosesNearestBoundary( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold;

	CollideWheelWithBoxHull( &manifold, points, ARRAY_COUNT( points ), &wheel,
							 ( b3Vec3 ){ 0.300f, 0.300f, 0.0f }, ( b3Vec3 ){ 0.100f, 0.100f, 0.300f } );

	ENSURE( manifold.pointCount == 1 );
	b3Vec3 expected = b3Normalize( ( b3Vec3 ){ 1.0f, 1.0f, 0.0f } );
	ENSURE( b3Dot( manifold.normal, expected ) > 0.98f );
	// Nearest vertical edge is sqrt(0.2^2 + 0.2^2) from the axle.
	// The expected outer-surface penetration is about 0.231 m, not a full
	// cornerRadius collapse against some unrelated edge inside the wheel core.
	ENSURE( manifold.points[0].separation < -0.20f );
	ENSURE( manifold.points[0].separation > -0.26f );
	return 0;
}

static int WheelHullShoulderCornerOutsideRoundedProfileDoesNotCollide( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold;

	// The nearest box vertex is (radial=0.511, axial=0.216). Radial and axial
	// face tests are each inside the speculative band, but the finite vertex is
	// farther than cornerRadius + speculative distance from the profile endpoint
	// (0.480, 0.200). A face-only or edge-x-axis-only SAT reports a phantom hit.
	CollideWheelWithBoxHull( &manifold, points, ARRAY_COUNT( points ), &wheel,
							 ( b3Vec3 ){ 0.611f, 0.0f, 0.316f }, ( b3Vec3 ){ 0.100f, 0.100f, 0.100f } );

	ENSURE( manifold.pointCount == 0 );
	return 0;
}

static int WheelHullFaceKeepsFlatSupportSegment( void )
{
	b3Wheel wheel = MakeFlatWheel();
	b3LocalManifoldPoint points[4] = { 0 };
	b3LocalManifold manifold;

	CollideWheelWithBoxHull( &manifold, points, ARRAY_COUNT( points ), &wheel,
							 ( b3Vec3 ){ 0.0f, -0.614f, 0.0f }, ( b3Vec3 ){ 1.0f, 0.100f, 1.0f } );

	ENSURE( manifold.pointCount == 2 );
	ENSURE( manifold.points[0].pair.index1 != manifold.points[1].pair.index1 );
	ENSURE( manifold.points[0].pair.owner2 == 1 );
	ENSURE( manifold.points[1].pair.owner2 == 1 );
	ENSURE( manifold.points[0].pair.index2 == manifold.points[1].pair.index2 );
	return 0;
}

static int WheelHullEdgeFeatureSurvivesSpin( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	b3BoxHull box = b3MakeBoxHull( 0.100f, 0.100f, 0.300f );
	const float angles[] = { 0.0f, 0.41f, 1.37f, 2.63f };
	uint8_t hullFeature = UINT8_MAX;
	uint8_t wheelFeature = UINT8_MAX;

	for ( int i = 0; i < ARRAY_COUNT( angles ); ++i )
	{
		b3Quat spin = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, angles[i] );
		b3Transform boxWorld = { ( b3Vec3 ){ 0.450f, 0.450f, 0.0f }, b3Quat_identity };
		b3Transform wheelWorld = { b3Vec3_zero, spin };
		b3Transform transformBtoA = b3InvMulTransforms( wheelWorld, boxWorld );

		b3LocalManifoldPoint point = { 0 };
		b3LocalManifold manifold = { 0 };
		manifold.points = &point;
		b3CollideWheelAndHull( &manifold, 1, &wheel, &box.base, transformBtoA );
		ENSURE( manifold.pointCount == 1 );
		// Edge contacts use B/A owner order so they cannot alias a face
		// contact that happens to carry the same numeric index.
		ENSURE( point.pair.owner1 == 1 );
		ENSURE( point.pair.owner2 == 0 );
		if ( i == 0 )
		{
			hullFeature = point.pair.index1;
			wheelFeature = point.pair.index2;
		}
		else
		{
			ENSURE( point.pair.index1 == hullFeature );
			ENSURE( point.pair.index2 == wheelFeature );
		}
	}
	return 0;
}


static int WheelHullVertexConeFindsThreeAxisCorner( void )
{
	// A fully rounded wheel is a sphere represented by the wheel support map.
	// The nearest box feature lies in the INTERIOR of a three-face vertex cone;
	// neither face normals nor any two-face edge arc contains the diagonal axis.
	b3Wheel wheel = b3MakeWheel( b3Vec3_zero, b3Vec3_axisZ, 0.500f, 0.500f, 0.500f );
	b3LocalManifoldPoint point = { 0 };
	b3LocalManifold manifold;
	CollideWheelWithBoxHull( &manifold, &point, 1, &wheel, ( b3Vec3 ){ 0.390f, 0.390f, 0.390f },
							 ( b3Vec3 ){ 0.100f, 0.100f, 0.100f } );

	ENSURE( manifold.pointCount == 1 );
	b3Vec3 expected = b3Normalize( ( b3Vec3 ){ 1.0f, 1.0f, 1.0f } );
	ENSURE( b3Dot( manifold.normal, expected ) > 0.995f );
	ENSURE_SMALL( point.separation - ( sqrtf( 3.0f * 0.290f * 0.290f ) - 0.500f ), 2.0e-4f );
	ENSURE( point.pair.owner1 == 1 );
	ENSURE( point.pair.owner2 == 1 );
	return 0;
}

static int WheelHullVertexConeRejectsDiagonalGap( void )
{
	b3Wheel wheel = b3MakeWheel( b3Vec3_zero, b3Vec3_axisZ, 0.500f, 0.500f, 0.500f );
	b3LocalManifoldPoint point = { 0 };
	b3LocalManifold manifold;

	// Every individual face slab overlaps the wheel, and every two-face edge
	// axis still overlaps it. Only the three-axis vertex direction proves the
	// 54 mm diagonal gap, so a face/edge-only SAT would report a ghost contact.
	CollideWheelWithBoxHull( &manifold, &point, 1, &wheel, ( b3Vec3 ){ 0.420f, 0.420f, 0.420f },
							 ( b3Vec3 ){ 0.100f, 0.100f, 0.100f } );
	ENSURE( manifold.pointCount == 0 );
	return 0;
}


static uint32_t WheelHullRandom( uint32_t* state )
{
	*state = 1664525u * *state + 1013904223u;
	return *state;
}

static float WheelHullRandom01( uint32_t* state )
{
	return (float)( WheelHullRandom( state ) >> 8 ) * ( 1.0f / 16777216.0f );
}

static float BruteWheelHullSeparation( const b3Wheel* wheel, const b3HullData* hull, b3Transform transformBtoA,
									 b3Vec3* bestNormalOut, int* bestSupportVertexOut )
{
	const b3Vec3* localPoints = b3GetHullPoints( hull );
	b3Vec3 points[16];
	if ( localPoints == NULL || hull->vertexCount <= 0 || hull->vertexCount > ARRAY_COUNT( points ) )
	{
		return -FLT_MAX;
	}
	for ( int i = 0; i < hull->vertexCount; ++i )
	{
		points[i] = b3TransformPoint( transformBtoA, localPoints[i] );
	}

	enum
	{
		DirectionCount = 8192,
	};
	const float goldenAngle = 2.39996322972865332f;
	float best = -FLT_MAX;
	b3Vec3 bestNormal = b3Vec3_zero;
	for ( int i = 0; i < DirectionCount; ++i )
	{
		float y = 1.0f - 2.0f * ( (float)i + 0.5f ) / (float)DirectionCount;
		float radius = sqrtf( b3MaxFloat( 0.0f, 1.0f - y * y ) );
		float angle = goldenAngle * (float)i;
		b3Vec3 normal = { radius * cosf( angle ), y, radius * sinf( angle ) };
		float hullMinimum = FLT_MAX;
		for ( int j = 0; j < hull->vertexCount; ++j )
		{
			hullMinimum = b3MinFloat( hullMinimum, b3Dot( normal, points[j] ) );
		}
		float separation = hullMinimum - b3Dot( normal, b3ComputeWheelSupport( wheel, normal ) );
		if ( separation > best )
		{
			best = separation;
			bestNormal = normal;
		}
	}
	if ( bestNormalOut != NULL )
	{
		*bestNormalOut = bestNormal;
	}
	if ( bestSupportVertexOut != NULL )
	{
		float minimumProjection = FLT_MAX;
		int minimumIndex = B3_NULL_INDEX;
		for ( int i = 0; i < hull->vertexCount; ++i )
		{
			float projection = b3Dot( bestNormal, points[i] );
			if ( projection < minimumProjection )
			{
				minimumProjection = projection;
				minimumIndex = i;
			}
		}
		*bestSupportVertexOut = minimumIndex;
	}
	return best;
}

static int WheelHullFeatureWalkReachesRemoteVertex( void )
{
	// Regression from the random SAT audit. The best face is not adjacent to
	// the true support vertex, so a search limited to that face boundary stops
	// at a materially deeper axis. The hull-graph walk must reach vertex 6.
	b3Wheel wheel = MakeCrownedWheel();
	b3Vec3 halfExtents = { 0.317333817f, 0.217708632f, 0.241965309f };
	b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );
	b3Transform transformBtoA = {
		.p = { 0.182800964f, -0.255118370f, 0.199782580f },
		.q = { { -0.0267029256f, 0.291615725f, 0.224896982f }, -0.929337740f },
	};

	b3LocalManifoldPoint point = { 0 };
	b3LocalManifold manifold = { 0 };
	manifold.points = &point;
	b3CollideWheelAndHull( &manifold, 1, &wheel, &box.base, transformBtoA );

	b3Vec3 bruteNormal = b3Vec3_zero;
	int bruteSupportVertex = B3_NULL_INDEX;
	float bruteSeparation =
		BruteWheelHullSeparation( &wheel, &box.base, transformBtoA, &bruteNormal, &bruteSupportVertex );
	ENSURE( manifold.pointCount == 1 );
	ENSURE( point.pair.owner1 == 1 && point.pair.owner2 == 1 );
	ENSURE( point.pair.index1 == 6 );
	ENSURE( bruteSupportVertex == 6 );
	ENSURE( b3Dot( manifold.normal, b3Vec3_axisZ ) > 0.995f );
	ENSURE( point.separation >= bruteSeparation - 0.003f );
	return 0;
}

static int WheelHullVertexConeFindsConstrainedInteriorMaximum( void )
{
	// Regression from the random SAT audit. The correct hull feature is vertex
	// 1, but the best normal lies inside its three-face normal cone rather than
	// on a face ray, an edge arc, or the unconstrained point-to-wheel direction.
	b3Wheel wheel = MakeCrownedWheel();
	b3Vec3 halfExtents = { 0.225684494f, 0.070502162f, 0.284663737f };
	b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );
	b3Transform transformBtoA = {
		.p = { -0.601516962f, 0.362439930f, -0.173805743f },
		.q = { { 0.380558997f, -0.0255413745f, 0.923170447f }, -0.0477340743f },
	};

	b3LocalManifoldPoint point = { 0 };
	b3LocalManifold manifold = { 0 };
	manifold.points = &point;
	b3CollideWheelAndHull( &manifold, 1, &wheel, &box.base, transformBtoA );

	b3Vec3 bruteNormal = b3Vec3_zero;
	int bruteSupportVertex = B3_NULL_INDEX;
	float bruteSeparation =
		BruteWheelHullSeparation( &wheel, &box.base, transformBtoA, &bruteNormal, &bruteSupportVertex );
	b3Vec3 expectedNormal = { -0.621524096f, 0.767321467f, -0.157878175f };
	ENSURE( manifold.pointCount == 1 );
	ENSURE( point.pair.owner1 == 1 && point.pair.owner2 == 1 );
	ENSURE( point.pair.index1 == 1 );
	ENSURE( bruteSupportVertex == 1 );
	ENSURE( b3Dot( manifold.normal, expectedNormal ) > 0.995f );
	ENSURE( point.separation >= bruteSeparation - 0.003f );
	return 0;
}

static int WheelHullSATMatchesDenseGlobalDirectionSearch( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	uint32_t random = 0xC0FFEE12u;
	for ( int caseIndex = 0; caseIndex < 200; ++caseIndex )
	{
		b3Vec3 halfExtents = {
			0.050f + 0.300f * WheelHullRandom01( &random ),
			0.050f + 0.300f * WheelHullRandom01( &random ),
			0.050f + 0.300f * WheelHullRandom01( &random ),
		};
		b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );

		b3Vec3 direction = {
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
		};
		if ( b3LengthSquared( direction ) < 0.01f )
		{
			direction = b3Vec3_axisX;
		}
		direction = b3Normalize( direction );
		float distance = 0.20f + 0.85f * WheelHullRandom01( &random );
		b3Vec3 center = b3MulSV( distance, direction );

		b3Vec3 rotationAxis = {
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
		};
		if ( b3LengthSquared( rotationAxis ) < 0.01f )
		{
			rotationAxis = b3Vec3_axisY;
		}
		rotationAxis = b3Normalize( rotationAxis );
		b3Quat rotation = b3MakeQuatFromAxisAngle( rotationAxis, 2.0f * B3_PI * WheelHullRandom01( &random ) );
		b3Transform transformBtoA = { center, rotation };

		b3LocalManifoldPoint points[4] = { 0 };
		b3LocalManifold manifold = { 0 };
		manifold.points = points;
		b3CollideWheelAndHull( &manifold, ARRAY_COUNT( points ), &wheel, &box.base, transformBtoA );

		b3Vec3 bruteNormal = b3Vec3_zero;
		int bruteSupportVertex = B3_NULL_INDEX;
		float bruteSeparation =
			BruteWheelHullSeparation( &wheel, &box.base, transformBtoA, &bruteNormal, &bruteSupportVertex );
		if ( bruteSeparation > B3_SPECULATIVE_DISTANCE + 0.002f )
		{
			ENSURE( manifold.pointCount == 0 );
		}
		if ( manifold.pointCount > 0 )
		{
			if ( manifold.points[0].separation < bruteSeparation - 0.003f )
			{
				printf( "WHEEL_HULL_FAIL case=%d sep=%.9g brute=%.9g center=(%.9g,%.9g,%.9g) half=(%.9g,%.9g,%.9g) quat=(%.9g,%.9g,%.9g,%.9g) pair=(%u,%u,%u,%u) normal=(%.9g,%.9g,%.9g)\n",
						caseIndex, (double)manifold.points[0].separation, (double)bruteSeparation,
						(double)center.x, (double)center.y, (double)center.z,
						(double)halfExtents.x, (double)halfExtents.y, (double)halfExtents.z,
						(double)rotation.v.x, (double)rotation.v.y, (double)rotation.v.z, (double)rotation.s,
						(unsigned)manifold.points[0].pair.owner1, (unsigned)manifold.points[0].pair.index1,
						(unsigned)manifold.points[0].pair.owner2, (unsigned)manifold.points[0].pair.index2,
						(double)manifold.normal.x, (double)manifold.normal.y, (double)manifold.normal.z );
				printf( "WHEEL_HULL_FAIL bruteNormal=(%.9g,%.9g,%.9g) bruteSupportVertex=%d\n",
						(double)bruteNormal.x, (double)bruteNormal.y, (double)bruteNormal.z, bruteSupportVertex );
			}
			// Dense search is an under-estimate of the true maximum. The SAT axis
			// selected by the implementation must not be materially worse.
			ENSURE( manifold.points[0].separation >= bruteSeparation - 0.003f );
			ENSURE( manifold.points[0].separation <= B3_SPECULATIVE_DISTANCE + 1.0e-5f );
			ENSURE_SMALL( b3Length( manifold.normal ) - 1.0f, 2.0e-5f );
		}
	}
	return 0;
}


static int WheelHullSATMatchesDenseSearchForRandomConvexHulls( void )
{
	b3Wheel wheel = MakeCrownedWheel();
	uint32_t random = 0x51A7C0DEu;
	int completed = 0;
	for ( int attempt = 0; attempt < 160 && completed < 60; ++attempt )
	{
		b3Vec3 cloud[12];
		float sx = 0.08f + 0.30f * WheelHullRandom01( &random );
		float sy = 0.08f + 0.30f * WheelHullRandom01( &random );
		float sz = 0.08f + 0.30f * WheelHullRandom01( &random );
		float shearXY = 0.35f * ( 2.0f * WheelHullRandom01( &random ) - 1.0f );
		float shearXZ = 0.35f * ( 2.0f * WheelHullRandom01( &random ) - 1.0f );
		float shearYZ = 0.35f * ( 2.0f * WheelHullRandom01( &random ) - 1.0f );
		for ( int i = 0; i < ARRAY_COUNT( cloud ); ++i )
		{
			b3Vec3 direction = {
				2.0f * WheelHullRandom01( &random ) - 1.0f,
				2.0f * WheelHullRandom01( &random ) - 1.0f,
				2.0f * WheelHullRandom01( &random ) - 1.0f,
			};
			if ( b3LengthSquared( direction ) < 0.02f )
			{
				direction = ( i & 1 ) != 0 ? b3Vec3_axisX : b3Vec3_axisY;
			}
			direction = b3Normalize( direction );
			b3Vec3 ellipsoid = { sx * direction.x, sy * direction.y, sz * direction.z };
			cloud[i] = ( b3Vec3 ){
				ellipsoid.x + shearXY * ellipsoid.y + shearXZ * ellipsoid.z,
				ellipsoid.y + shearYZ * ellipsoid.z,
				ellipsoid.z,
			};
		}

		b3HullData* hull = b3CreateHull( cloud, ARRAY_COUNT( cloud ), ARRAY_COUNT( cloud ) );
		if ( hull == NULL || hull->vertexCount > 16 )
		{
			if ( hull != NULL )
			{
				b3DestroyHull( hull );
			}
			continue;
		}

		b3Vec3 direction = {
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
		};
		if ( b3LengthSquared( direction ) < 0.01f )
		{
			direction = b3Vec3_axisX;
		}
		direction = b3Normalize( direction );
		b3Vec3 center = b3MulSV( 0.18f + 0.95f * WheelHullRandom01( &random ), direction );

		b3Vec3 rotationAxis = {
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
			2.0f * WheelHullRandom01( &random ) - 1.0f,
		};
		if ( b3LengthSquared( rotationAxis ) < 0.01f )
		{
			rotationAxis = b3Vec3_axisY;
		}
		rotationAxis = b3Normalize( rotationAxis );
		b3Quat rotation = b3MakeQuatFromAxisAngle( rotationAxis, 2.0f * B3_PI * WheelHullRandom01( &random ) );
		b3Transform transformBtoA = { center, rotation };

		b3LocalManifoldPoint points[4] = { 0 };
		b3LocalManifold manifold = { 0 };
		manifold.points = points;
		b3CollideWheelAndHull( &manifold, ARRAY_COUNT( points ), &wheel, hull, transformBtoA );

		b3Vec3 bruteNormal = b3Vec3_zero;
		int bruteSupportVertex = B3_NULL_INDEX;
		float bruteSeparation =
			BruteWheelHullSeparation( &wheel, hull, transformBtoA, &bruteNormal, &bruteSupportVertex );
		if ( bruteSeparation > B3_SPECULATIVE_DISTANCE + 0.002f )
		{
			ENSURE( manifold.pointCount == 0 );
		}
		if ( manifold.pointCount > 0 )
		{
			if ( manifold.points[0].separation < bruteSeparation - 0.003f )
			{
				printf( "WHEEL_HULL_RANDOM_FAIL case=%d sep=%.9g brute=%.9g normal=(%.9g,%.9g,%.9g) bruteNormal=(%.9g,%.9g,%.9g) support=%d vertices=%d faces=%d edges=%d\n",
						completed, (double)manifold.points[0].separation, (double)bruteSeparation,
						(double)manifold.normal.x, (double)manifold.normal.y, (double)manifold.normal.z,
						(double)bruteNormal.x, (double)bruteNormal.y, (double)bruteNormal.z,
						bruteSupportVertex, hull->vertexCount, hull->faceCount, hull->edgeCount );
			}
			ENSURE( manifold.points[0].separation >= bruteSeparation - 0.003f );
			ENSURE( manifold.points[0].separation <= B3_SPECULATIVE_DISTANCE + 1.0e-5f );
			ENSURE_SMALL( b3Length( manifold.normal ) - 1.0f, 2.0e-5f );
		}

		b3DestroyHull( hull );
		completed += 1;
	}

	ENSURE( completed == 60 );
	return 0;
}


static b3ShapeId CreateStaticWheelHullBox( b3WorldId worldId, b3Vec3 center, b3Vec3 halfExtents )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = ( b3Pos ){ center.x, center.y, center.z };
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( halfExtents.x, halfExtents.y, halfExtents.z );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.0f;
	return b3CreateHullShape( bodyId, &shapeDef, &box.base );
}

static b3ShapeId CreateLockedSpinningWheel( b3WorldId worldId, const b3Wheel* wheel )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.angularVelocity = ( b3Vec3 ){ 0.0f, 0.0f, 5.0f };
	bodyDef.motionLocks.linearX = true;
	bodyDef.motionLocks.linearY = true;
	bodyDef.motionLocks.linearZ = true;
	bodyDef.motionLocks.angularX = true;
	bodyDef.motionLocks.angularY = true;
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = 80.0f;
	shapeDef.baseMaterial.friction = 0.0f;
	return b3CreateWheelShape( bodyId, &shapeDef, wheel );
}

static int WheelWorldHullCornerDoesNotCreateGhostContact( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );
	CreateStaticWheelHullBox( worldId, ( b3Vec3 ){ 0.629f, 0.629f, 0.0f },
							  ( b3Vec3 ){ 0.100f, 0.100f, 0.300f } );
	b3Wheel wheel = MakeCrownedWheel();
	b3ShapeId wheelShapeId = CreateLockedSpinningWheel( worldId, &wheel );

	for ( int step = 0; step < 8; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
		b3ContactData contacts[2];
		ENSURE( b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) ) == 0 );
	}

	b3DestroyWorld( worldId );
	return 0;
}

static int WheelWorldHullEdgeContactPersistsWhileSpinning( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = b3Vec3_zero;
	b3WorldId worldId = b3CreateWorld( &worldDef );
	CreateStaticWheelHullBox( worldId, ( b3Vec3 ){ 0.450f, 0.450f, 0.0f },
							  ( b3Vec3 ){ 0.100f, 0.100f, 0.300f } );
	b3Wheel wheel = MakeCrownedWheel();
	b3ShapeId wheelShapeId = CreateLockedSpinningWheel( worldId, &wheel );

	uint32_t featureId = UINT32_MAX;
	b3Vec3 expected = b3Normalize( ( b3Vec3 ){ 1.0f, 1.0f, 0.0f } );
	for ( int step = 0; step < 12; ++step )
	{
		b3World_Step( worldId, 1.0f / 60.0f, 4 );
		b3ContactData contacts[2];
		int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
		ENSURE( contactCount == 1 );
		ENSURE( contacts[0].manifoldCount == 1 );
		const b3Manifold* manifold = contacts[0].manifolds;
		ENSURE( manifold->pointCount == 1 );
		ENSURE( b3Dot( manifold->normal, expected ) > 0.98f );
		const b3ManifoldPoint* point = manifold->points;
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


typedef enum WheelHullContactTopology
{
	WheelHullContactFace = 0,
	WheelHullContactEdge = 1,
	WheelHullContactVertex = 2,
} WheelHullContactTopology;

static WheelHullContactTopology DecodeWheelHullContactTopology( uint32_t featureId )
{
	uint8_t owner1 = (uint8_t)( featureId >> 24 );
	uint8_t owner2 = (uint8_t)( featureId >> 8 );
	if ( owner1 == 0 )
	{
		return WheelHullContactFace;
	}
	return owner2 == 0 ? WheelHullContactEdge : WheelHullContactVertex;
}

static int RunLoadedWheelHullTopologyTransition( float direction, float spinPhase )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = ( b3Vec3 ){ 0.0f, -9.8f, 0.0f };
	b3WorldId worldId = b3CreateWorld( &worldDef );

	b3BodyDef groundDef = b3DefaultBodyDef();
	b3BodyId groundId = b3CreateBody( worldId, &groundDef );
	b3BoxHull box = b3MakeBoxHull( 0.8f, 0.1f, 0.5f );
	b3ShapeDef groundShapeDef = b3DefaultShapeDef();
	groundShapeDef.baseMaterial.friction = 0.0f;
	b3CreateHullShape( groundId, &groundShapeDef, &box.base );

	b3BodyDef wheelBodyDef = b3DefaultBodyDef();
	wheelBodyDef.type = b3_dynamicBody;
	wheelBodyDef.position = ( b3Pos ){ 0.60f * direction, 0.62f, 0.20f * direction };
	wheelBodyDef.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, spinPhase );
	wheelBodyDef.linearVelocity = ( b3Vec3 ){ direction, 0.0f, direction };
	wheelBodyDef.angularVelocity = ( b3Vec3 ){ 0.0f, 0.0f, -direction / 0.514f };
	wheelBodyDef.allowFastRotation = true;
	wheelBodyDef.enableSleep = false;
	b3BodyId wheelBodyId = b3CreateBody( worldId, &wheelBodyDef );

	b3Wheel wheel = MakeCrownedWheel();
	b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
	wheelShapeDef.density = 80.0f;
	wheelShapeDef.baseMaterial.friction = 0.0f;
	b3ShapeId wheelShapeId = b3CreateWheelShape( wheelBodyId, &wheelShapeDef, &wheel );

	bool seen[3] = { false, false, false };
	bool loaded[3] = { false, false, false };
	bool contactStarted = false;
	WheelHullContactTopology previousTopology = WheelHullContactFace;
	b3Vec3 previousNormal = b3Vec3_zero;
	bool havePreviousNormal = false;

	for ( int step = 0; step < 100; ++step )
	{
		b3World_Step( worldId, 1.0f / 120.0f, 4 );
		b3ContactData contacts[2];
		int contactCount = b3Shape_GetContactData( wheelShapeId, contacts, ARRAY_COUNT( contacts ) );
		if ( contactCount == 0 )
		{
			// Once the loaded traversal starts, contact may end only after the
			// corner (vertex topology) has actually been crossed.
			if ( contactStarted )
			{
				ENSURE( seen[WheelHullContactVertex] );
				break;
			}
			continue;
		}

		contactStarted = true;
		ENSURE( contactCount == 1 );
		ENSURE( contacts[0].manifoldCount == 1 );
		const b3Manifold* manifold = contacts[0].manifolds;
		ENSURE( manifold->pointCount >= 1 );

		WheelHullContactTopology topology = DecodeWheelHullContactTopology( manifold->points[0].featureId );
		ENSURE( topology >= previousTopology );
		seen[topology] = true;

		float totalImpulse = 0.0f;
		for ( int pointIndex = 0; pointIndex < manifold->pointCount; ++pointIndex )
		{
			totalImpulse += manifold->points[pointIndex].totalNormalImpulse;
		}
		if ( totalImpulse > 0.1f )
		{
			loaded[topology] = true;
		}

		if ( havePreviousNormal )
		{
			// The topology may change, but the physical boundary is continuous.
			// A sudden normal flip here is the old phantom-corner failure mode.
			ENSURE( b3Dot( previousNormal, manifold->normal ) > 0.85f );
		}
		previousNormal = manifold->normal;
		havePreviousNormal = true;
		previousTopology = topology;
	}

	ENSURE( contactStarted );
	ENSURE( seen[WheelHullContactFace] );
	ENSURE( seen[WheelHullContactEdge] );
	ENSURE( seen[WheelHullContactVertex] );
	ENSURE( loaded[WheelHullContactFace] );
	ENSURE( loaded[WheelHullContactEdge] );
	ENSURE( loaded[WheelHullContactVertex] );

	b3DestroyWorld( worldId );
	return 0;
}

static int WheelWorldHullLoadedFaceEdgeVertexTransitionIsContinuous( void )
{
	const float phases[] = { 0.0f, 0.73f };
	for ( int directionIndex = 0; directionIndex < 2; ++directionIndex )
	{
		float direction = directionIndex == 0 ? 1.0f : -1.0f;
		for ( int phaseIndex = 0; phaseIndex < ARRAY_COUNT( phases ); ++phaseIndex )
		{
			ENSURE( RunLoadedWheelHullTopologyTransition( direction, phases[phaseIndex] ) == 0 );
		}
	}
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
	RUN_SUBTEST( WheelHullDoesNotUseInfiniteFaceOutsideBox );
	RUN_SUBTEST( WheelHullCornerUsesFiniteEdgeNormal );
	RUN_SUBTEST( WheelHullRejectedFaceFallsBackToBoundaryFeature );
	RUN_SUBTEST( WheelHullDeepCornerChoosesNearestBoundary );
	RUN_SUBTEST( WheelHullShoulderCornerOutsideRoundedProfileDoesNotCollide );
	RUN_SUBTEST( WheelHullFaceKeepsFlatSupportSegment );
	RUN_SUBTEST( WheelHullEdgeFeatureSurvivesSpin );
	RUN_SUBTEST( WheelHullVertexConeFindsThreeAxisCorner );
	RUN_SUBTEST( WheelHullVertexConeRejectsDiagonalGap );
	RUN_SUBTEST( WheelHullFeatureWalkReachesRemoteVertex );
	RUN_SUBTEST( WheelHullVertexConeFindsConstrainedInteriorMaximum );
	RUN_SUBTEST( WheelHullSATMatchesDenseGlobalDirectionSearch );
	RUN_SUBTEST( WheelHullSATMatchesDenseSearchForRandomConvexHulls );
	RUN_SUBTEST( WheelWorldHullCornerDoesNotCreateGhostContact );
	RUN_SUBTEST( WheelWorldHullEdgeContactPersistsWhileSpinning );
	RUN_SUBTEST( WheelWorldHullLoadedFaceEdgeVertexTransitionIsContinuous );
	return 0;
}
