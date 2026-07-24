// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m5_test_course.h"

#include "jozz_vehicle_central_test_campus.h"
#include "jozz_vehicle_world_layout.h"

JozzVehicleM5TestCourse CreateJozzVehicleM5TestCourse( b3WorldId worldId, float groundTopY, uint64_t terrainCategoryBits )
{
	JozzVehicleM5TestCourse course;

	// E2R course orchestration only. The old six-lane placement and prop
	// scatter remain available in git history/obstacle-kit audit work, but are
	// no longer compiled into the active world builder.
	// E2R central campus only. The E3 track (BuildJozzTrackBase/Profiles) is
	// intentionally NOT built here - Jozz's decision 2026-07-24: recover the
	// campus without the track (the audit flagged the E3 loop as having no car
	// clearance). The track_* sources stay in the tree, dormant, for later.
	BuildCentralTestCampus( worldId, groundTopY, terrainCategoryBits, JozzWorldLayout::kOffroadDefaultSeed );
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
