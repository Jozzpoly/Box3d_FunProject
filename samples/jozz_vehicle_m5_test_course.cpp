// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m5_test_course.h"

#include "jozz_vehicle_obstacle_kit.h"
#include "jozz_vehicle_world_layout.h"

#include <cstdio>

using namespace JozzWorldLayout;

namespace
{

uint32_t DifficultyColor( JozzLaneDifficulty difficulty )
{
	switch ( difficulty )
	{
		case kLaneDifficultyEasy:
			return b3_colorLimeGreen;
		case kLaneDifficultyMedium:
			return b3_colorGold;
		case kLaneDifficultyHard:
		default:
			return b3_colorOrangeRed;
	}
}

// One lane's obstacle sequence + its per-station labels. `laneOriginX` is the
// world X where the FIRST station starts (kPoligonOriginX +
// kPoligonStationMargin); each Build* below advances a running `x` cursor by
// however much footprint it actually consumes so the next station never
// overlaps it - stations are packed, not fixed-grid, matching the E2 doc's
// "~8 m" spacing as a target rather than a rigid slot size.
struct LaneBuilder
{
	b3WorldId worldId;
	float laneZ;
	float groundTopY;
	uint64_t terrainCategoryBits;
	uint32_t color;
	std::vector<JozzCourseLabel>* labels;
	float x;

	void Label( const char* text )
	{
		labels->push_back( { { x, groundTopY + 1.4f, laneZ }, text, color } );
	}

	b3Pos Anchor() const
	{
		return { x, groundTopY, laneZ };
	}
};

void BuildLaneL1Komfort( LaneBuilder& lane )
{
	lane.Label( "Komfort: garb R0.3" );
	AddSpeedBump( lane.worldId, lane.Anchor(), 0.0f, 0.3f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Komfort: tarka niska" );
	AddWashboard( lane.worldId, lane.Anchor(), 0.0f, 6, 0.6f, 0.08f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Komfort: uskok 0.1 m" );
	AddStepUp( lane.worldId, lane.Anchor(), 0.0f, 6.0f, 0.10f, 1.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;
}

void BuildLaneL2Szuter( LaneBuilder& lane )
{
	lane.Label( "Szuter: whoops plytkie" );
	AddWhoops( lane.worldId, lane.Anchor(), 0.0f, 5, 1.8f, 0.18f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Szuter: koleiny plytkie" );
	AddRuts( lane.worldId, lane.Anchor(), 0.0f, 6.0f, 0.12f, 1.6f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Szuter: off-camber 8st" );
	AddOffCamber( lane.worldId, lane.Anchor(), 0.0f, 6.0f, 6.0f, 8.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;
}

void BuildLaneL3Rajd( LaneBuilder& lane )
{
	lane.Label( "Rajd: kicker 12st R0.3" );
	AddKicker( lane.worldId, lane.Anchor(), 0.0f, 4.0f, 6.0f, 12.0f, 0.3f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Rajd: tabletop" );
	AddTabletop( lane.worldId, lane.Anchor(), 0.0f, 4.0f, 3.0f, 4.0f, 1.0f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += 11.0f + kPoligonStationSpacing; // approach+table+landing = 11 m, then the usual gap

	lane.Label( "Rajd: whoops srednie" );
	AddWhoops( lane.worldId, lane.Anchor(), 0.0f, 5, 2.0f, 0.30f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;
}

void BuildLaneL4Kamien( LaneBuilder& lane, uint32_t terrainSeed )
{
	lane.Label( "Kamien: rock garden gesty" );
	b3Pos gardenCenter = { lane.x + 3.0f, lane.groundTopY, lane.laneZ };
	AddRockGarden( lane.worldId, gardenCenter, 0.0f, 7.0f, 6.0f, 0.55f, 0.2f, 0.5f, lane.terrainCategoryBits,
				   terrainSeed + 401u, lane.color );
	lane.x += 8.0f + kPoligonStationSpacing;

	lane.Label( "Kamien: bal pojedynczy" );
	AddLogs( lane.worldId, lane.Anchor(), 0.0f, 1, 0.25f, 0.0f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Kamien: schody w dol" );
	AddStairs( lane.worldId, lane.Anchor(), 0.0f, 4, 0.15f, 1.0f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;
}

void BuildLaneL5Ekstrema( LaneBuilder& lane )
{
	lane.Label( "Ekstrema: uskok 0.35 m" );
	AddStepUp( lane.worldId, lane.Anchor(), 0.0f, 6.0f, 0.35f, 1.2f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Ekstrema: wykrzyzowanie" );
	// width=1.4 m per ramp -> ramps land ~1.55 m apart (AddArticulationRamps'
	// trackZ = width/2 + gap/2), close to a real wheel track.
	AddArticulationRamps( lane.worldId, lane.Anchor(), 0.0f, 3.0f, 1.4f, 15.0f, 2.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Ekstrema: koleiny glebokie" );
	AddRuts( lane.worldId, lane.Anchor(), 0.0f, 6.0f, 0.28f, 1.6f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Ekstrema: banda" );
	AddBerm( lane.worldId, lane.Anchor(), 0.0f, 15.0f, 45.0f, 5.0f, 20.0f, 6, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;
}

void BuildLaneL6Skocznie( LaneBuilder& lane )
{
	lane.Label( "Skocznie: klin 25st ostry" );
	AddWedgeRamp( lane.worldId, lane.Anchor(), 0.0f, 4.0f, 6.0f, 25.0f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;

	lane.Label( "Skocznie: gap 4 m" );
	AddGapJump( lane.worldId, lane.Anchor(), 0.0f, 1.5f, 20.0f, 4.0f, 6.0f, lane.terrainCategoryBits, lane.color );
	lane.x += 12.8f + kPoligonStationSpacing; // 2*rampLength(~4.4)+gap(4) = ~12.8 m, then the usual gap

	lane.Label( "Skocznie: kicker 20st R0.4" );
	AddKicker( lane.worldId, lane.Anchor(), 0.0f, 4.0f, 6.0f, 20.0f, 0.4f, lane.terrainCategoryBits, lane.color );
	lane.x += kPoligonStationSpacing;
	// Remaining run to the lane's east end is left flat - the "ladowisko
	// plaskie" the E2 doc asks for, already provided by the plate underneath.
}

// Builds all 6 lanes of the Etap 2 suspension poligon (docs/
// MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md §4), appending every station label
// to `outLabels`. `terrainSeed` only feeds AddRockGarden's determinism (L4) -
// unrelated to the offroad terrain's own seed, kept independent on purpose.
void BuildSuspensionPoligon( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits, uint32_t terrainSeed,
							  std::vector<JozzCourseLabel>* outLabels )
{
	for ( int laneIndex = 0; laneIndex < kLaneCount; ++laneIndex )
	{
		const JozzLaneSpec& spec = kLanes[laneIndex];
		LaneBuilder lane{ worldId,	 spec.zCenter, groundTopY, terrainCategoryBits, DifficultyColor( spec.difficulty ),
						   outLabels, kPoligonOriginX + kPoligonStationMargin };

		// Lane-entry sign: the lane's own name, at the very start of its run.
		outLabels->push_back(
			{ { kPoligonOriginX - 2.0f, groundTopY + 1.8f, spec.zCenter }, spec.name, lane.color } );

		switch ( laneIndex )
		{
			case 0:
				BuildLaneL1Komfort( lane );
				break;
			case 1:
				BuildLaneL2Szuter( lane );
				break;
			case 2:
				BuildLaneL3Rajd( lane );
				break;
			case 3:
				BuildLaneL4Kamien( lane, terrainSeed );
				break;
			case 4:
				BuildLaneL5Ekstrema( lane );
				break;
			case 5:
				BuildLaneL6Skocznie( lane );
				break;
			default:
				break;
		}
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
// interaction, not realism. Positions are relative to kPropZoneOriginX/Z (E2
// doc §2.4): Etap 2 moved this whole scatter off the driving axis around
// (0,0) onto the near edge of the future Etap 4 "plac fizyki" rectangle, so
// the centre stays clear for strojenie/spawn.
constexpr PropSpec kPropSpecs[] = {
	{ 0.0f, 6.0f, 0.5f, 400.0f, false },	{ 0.0f, -6.0f, 0.5f, 400.0f, false },
	{ -10.0f, 6.0f, 0.7f, 250.0f, false }, { -10.0f, -6.0f, 0.35f, 600.0f, false },
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

	// Etap 2 suspension poligon (docs/MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md):
	// replaces the old 4 hand-placed ramps + 2 washboard lanes with the
	// obstacle kit's 6-lane progression. Uses the default offroad seed as its
	// rock-garden seed base - unrelated to terrain regeneration, just a
	// convenient stable default so the garden is deterministic across runs.
	BuildSuspensionPoligon( worldId, groundTopY, terrainCategoryBits, kOffroadDefaultSeed, &course.labels );

	// Scattered dynamic props, moved off the driving axis onto the near edge
	// of the future plac-fizyki zone (E2 doc §2.4).
	course.props.reserve( sizeof( kPropSpecs ) / sizeof( kPropSpecs[0] ) );
	for ( const PropSpec& spec : kPropSpecs )
	{
		b3Pos position = { kPropZoneOriginX + spec.x, groundTopY + spec.size + 0.3f, kPropZoneOriginZ + spec.z };
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
	course->labels.clear();
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
