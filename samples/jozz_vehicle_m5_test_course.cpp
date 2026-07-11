// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m5_test_course.h"

namespace
{

void AddRamp( b3WorldId worldId, b3Pos position, float zAxisDegrees, float halfLength, float halfHeight, float halfWidth,
			  uint64_t terrainCategoryBits )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = position;
	bodyDef.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, zAxisDegrees * B3_PI / 180.0f );
	bodyDef.name = "m5_ramp";
	b3BodyId rampId = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.9f;
	shapeDef.filter.categoryBits = terrainCategoryBits;
	b3BoxHull ramp = b3MakeBoxHull( halfLength, halfHeight, halfWidth );
	b3CreateHullShape( rampId, &shapeDef, &ramp.base );
}

void AddWashboardLane( b3WorldId worldId, b3Pos start, b3Vec3 step, int count, uint64_t terrainCategoryBits )
{
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.9f;
	shapeDef.filter.categoryBits = terrainCategoryBits;

	for ( int i = 0; i < count; ++i )
	{
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.position = { start.x + step.x * (float)i, start.y, start.z + step.z * (float)i };
		bodyDef.name = "m5_washboard";
		b3BodyId bumpId = b3CreateBody( worldId, &bodyDef );

		b3BoxHull bump = b3MakeBoxHull( 0.45f, 0.10f, 2.6f );
		b3CreateHullShape( bumpId, &shapeDef, &bump.base );
	}
}

JozzVehicleM5TestCourseProp AddBoxProp( b3WorldId worldId, b3Pos position, float halfExtent, float density )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = position;
	bodyDef.name = "m5_prop_box";
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = density;
	shapeDef.baseMaterial.friction = 0.6f;
	shapeDef.baseMaterial.restitution = 0.05f;
	// Props are obstacles, not drivable terrain. The engine default category
	// is ALL bits, which would make a prop match the M6 rolling sphere's
	// terrain-only mask; a plain non-terrain bit keeps prop hits on the
	// true-width sidewall. Any all-bits mask collides with 0x1 exactly like
	// it did with the default, so the M5 lab is unaffected.
	shapeDef.filter.categoryBits = 0x1;
	b3BoxHull box = b3MakeBoxHull( halfExtent, halfExtent, halfExtent );
	b3CreateHullShape( bodyId, &shapeDef, &box.base );

	return { bodyId, position, b3Quat_identity };
}

JozzVehicleM5TestCourseProp AddSphereProp( b3WorldId worldId, b3Pos position, float radius, float density )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = position;
	bodyDef.name = "m5_prop_sphere";
	b3BodyId bodyId = b3CreateBody( worldId, &bodyDef );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = density;
	shapeDef.baseMaterial.friction = 0.5f;
	shapeDef.baseMaterial.restitution = 0.35f;
	// Same non-terrain tag as the box props; see AddBoxProp.
	shapeDef.filter.categoryBits = 0x1;
	b3Sphere sphere = { b3Vec3_zero, radius };
	b3CreateSphereShape( bodyId, &shapeDef, &sphere );

	return { bodyId, position, b3Quat_identity };
}

struct PropSpec
{
	float x, z, size, density;
	bool isSphere;
};

// Boxes: crates of mixed size/mass. Spheres: balls, light-ish (density 1) so
// they roll and scatter readily under a nudge from the vehicle - the point is
// interaction, not realism.
constexpr PropSpec kPropSpecs[] = {
	{ 10.0f, 6.0f, 0.5f, 400.0f, false },	{ 10.0f, -6.0f, 0.5f, 400.0f, false },
	{ -10.0f, 6.0f, 0.7f, 250.0f, false },	{ -10.0f, -6.0f, 0.35f, 600.0f, false },
	{ 16.0f, 12.0f, 0.9f, 150.0f, false }, { -16.0f, 12.0f, 0.6f, 300.0f, false },
	{ 0.0f, -14.0f, 0.55f, 350.0f, false }, { 24.0f, -4.0f, 0.4f, 500.0f, false },
	{ 8.0f, 18.0f, 0.45f, 1.0f, true },	{ -8.0f, 18.0f, 0.6f, 1.0f, true },
	{ 18.0f, -16.0f, 0.35f, 1.0f, true },	{ -18.0f, -16.0f, 0.5f, 1.0f, true },
	{ 2.0f, 24.0f, 0.4f, 1.0f, true },	{ -24.0f, -2.0f, 0.65f, 1.0f, true },
};

} // namespace

JozzVehicleM5TestCourse CreateJozzVehicleM5TestCourse( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits )
{
	JozzVehicleM5TestCourse course;

	// Ramps: four, spread across the now-2x-bigger ground for room to build speed.
	AddRamp( worldId, { 28.0f, groundTopY + 0.30f, 0.0f }, 8.0f, 4.0f, 0.25f, 5.0f, terrainCategoryBits );
	AddRamp( worldId, { -30.0f, groundTopY + 0.45f, -10.0f }, -12.0f, 4.0f, 0.25f, 4.0f, terrainCategoryBits );
	AddRamp( worldId, { 0.0f, groundTopY + 0.35f, 32.0f }, 10.0f, 3.5f, 0.22f, 4.5f, terrainCategoryBits );
	AddRamp( worldId, { 14.0f, groundTopY + 0.5f, -30.0f }, -16.0f, 3.0f, 0.28f, 4.0f, terrainCategoryBits );

	// Washboard: two lanes at different spacing for a lighter/harsher suspension test.
	AddWashboardLane( worldId, { -6.0f, groundTopY, 8.0f }, { -3.0f, 0.0f, 0.0f }, 6, terrainCategoryBits );
	AddWashboardLane( worldId, { 6.0f, groundTopY, -18.0f }, { 2.2f, 0.0f, 0.0f }, 6, terrainCategoryBits );

	// Scattered dynamic props.
	course.props.reserve( sizeof( kPropSpecs ) / sizeof( kPropSpecs[0] ) );
	for ( const PropSpec& spec : kPropSpecs )
	{
		b3Pos position = { spec.x, groundTopY + spec.size + 0.3f, spec.z };
		if ( spec.isSphere )
		{
			course.props.push_back( AddSphereProp( worldId, position, spec.size, spec.density ) );
		}
		else
		{
			course.props.push_back( AddBoxProp( worldId, position, spec.size, spec.density ) );
		}
	}

	return course;
}

void DestroyJozzVehicleM5TestCourse( JozzVehicleM5TestCourse* course )
{
	course->props.clear();
}

void ResetJozzVehicleM5TestCourseProps( const JozzVehicleM5TestCourse& course )
{
	for ( const JozzVehicleM5TestCourseProp& prop : course.props )
	{
		b3Body_SetTransform( prop.bodyId, prop.spawnPosition, prop.spawnRotation );
		b3Body_SetLinearVelocity( prop.bodyId, b3Vec3_zero );
		b3Body_SetAngularVelocity( prop.bodyId, b3Vec3_zero );
		b3Body_SetAwake( prop.bodyId, true );
	}
}
