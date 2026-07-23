// SPDX-License-Identifier: MIT
// spike_kernel_v0 — eksperyment założycielski projektu "Ultimate" (2026-07-15).
//
// Dowodzi w miniaturze architektury nowego projektu:
//   katalog części (dane) -> blueprint (trwałe ID autorskie)
//   -> kompilator (blueprint => ciała/jointy box3d; bodyId TYLKO compile-time)
//   -> symulacja headless (fixed-step, deterministyczna telemetria + hash).
//
// Świadomie ZERO zależności od hosta sampli (samples/), grafiki i ImGui —
// linkuje wyłącznie statyczną bibliotekę box3d + jej publiczne nagłówki.
// To jest test "przenośnego rdzenia": jeśli ten plik się kompiluje, buduje
// i daje powtarzalny hash, to rdzeń nowego projektu może żyć poza powłoką.

#include "box3d/box3d.h"
#include "box3d/collision.h"
#include "box3d/math_functions.h"
#include "box3d/types.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------- katalog ---
// Część jest DANYMI, nie kodem. (Wzór: VAW foundation.catalog + kontrakty JV.)

enum class PartType
{
	FrameCore, // sztywna skrzynia nośna
	Wheel,	   // koło napędzane (sfera + revolute z motorem, oś = Z)
};

struct PartDef
{
	PartType type;
	const char* catalogKey;
	float halfExtent[3]; // FrameCore: połówki boków; Wheel: [promień, -, -]
	float density;
	float motorTorque; // tylko Wheel
};

static const PartDef kCatalog[] = {
	{ PartType::FrameCore, "frame_core", { 0.9f, 0.25f, 0.6f }, 300.0f, 0.0f },
	{ PartType::Wheel, "wheel_std", { 0.35f, 0.0f, 0.0f }, 700.0f, 250.0f },
};

static const PartDef* FindPart( const char* key )
{
	for ( const PartDef& def : kCatalog )
	{
		if ( std::strcmp( def.catalogKey, key ) == 0 )
		{
			return &def;
		}
	}
	return nullptr;
}

// -------------------------------------------------------------- blueprint ---
// Trwała tożsamość autorska: partId. Ulotna tożsamość runtime: b3BodyId —
// istnieje wyłącznie w CompiledMachine, nigdy w blueprincie (lekcja VAW).

struct BlueprintPart
{
	uint32_t partId; // trwałe, autorskie
	const char* catalogKey;
	float localPos[3]; // względem roota konstrukcji
};

struct Blueprint
{
	const char* name;
	std::vector<BlueprintPart> parts;
};

static Blueprint MakeTestRover()
{
	Blueprint bp;
	bp.name = "spike_rover_v0";
	bp.parts.push_back( { 1, "frame_core", { 0.0f, 0.0f, 0.0f } } );
	bp.parts.push_back( { 2, "wheel_std", { 0.7f, -0.25f, 0.75f } } );
	bp.parts.push_back( { 3, "wheel_std", { 0.7f, -0.25f, -0.75f } } );
	bp.parts.push_back( { 4, "wheel_std", { -0.7f, -0.25f, 0.75f } } );
	bp.parts.push_back( { 5, "wheel_std", { -0.7f, -0.25f, -0.75f } } );
	return bp;
}

// -------------------------------------------------------------- kompilator ---

struct CompiledPart
{
	uint32_t partId; // mapowanie trwałe -> runtime, odtwarzane przy każdej kompilacji
	b3BodyId bodyId; // COMPILE-TIME ONLY
};

struct CompiledMachine
{
	b3BodyId rootBodyId;
	std::vector<CompiledPart> parts;
	std::vector<b3JointId> joints;
};

static CompiledMachine CompileBlueprint( b3WorldId worldId, const Blueprint& bp, float spawnX, float spawnY, float spawnZ,
										 float driveSpeedRadS )
{
	CompiledMachine machine = {};

	// Przebieg 1: root (FrameCore) — w tym spiku dokładnie jeden.
	for ( const BlueprintPart& part : bp.parts )
	{
		const PartDef* def = FindPart( part.catalogKey );
		if ( def == nullptr || def->type != PartType::FrameCore )
		{
			continue;
		}
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = { spawnX + part.localPos[0], spawnY + part.localPos[1], spawnZ + part.localPos[2] };
		bodyDef.name = def->catalogKey;
		machine.rootBodyId = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = def->density;
		b3BoxHull hull = b3MakeBoxHull( def->halfExtent[0], def->halfExtent[1], def->halfExtent[2] );
		b3CreateHullShape( machine.rootBodyId, &shapeDef, &hull.base );
		machine.parts.push_back( { part.partId, machine.rootBodyId } );
	}

	// Przebieg 2: koła zawieszone na root przez revolute (oś zawiasu = lokalne Z).
	for ( const BlueprintPart& part : bp.parts )
	{
		const PartDef* def = FindPart( part.catalogKey );
		if ( def == nullptr || def->type != PartType::Wheel )
		{
			continue;
		}
		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = { spawnX + part.localPos[0], spawnY + part.localPos[1], spawnZ + part.localPos[2] };
		bodyDef.name = def->catalogKey;
		bodyDef.allowFastRotation = true;
		b3BodyId wheelId = b3CreateBody( worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = def->density;
		b3Sphere sphere = { b3Vec3_zero, def->halfExtent[0] };
		b3CreateSphereShape( wheelId, &shapeDef, &sphere );

		b3RevoluteJointDef jointDef = b3DefaultRevoluteJointDef();
		jointDef.base.bodyIdA = machine.rootBodyId;
		jointDef.base.bodyIdB = wheelId;
		jointDef.base.localFrameA.p = { part.localPos[0], part.localPos[1], part.localPos[2] };
		jointDef.base.localFrameA.q = b3Quat_identity; // oś Z frame'u = oś obrotu koła
		jointDef.base.localFrameB.p = b3Vec3_zero;
		jointDef.base.localFrameB.q = b3Quat_identity;
		jointDef.base.collideConnected = false;
		jointDef.enableMotor = true;
		jointDef.maxMotorTorque = def->motorTorque;
		jointDef.motorSpeed = driveSpeedRadS;
		machine.joints.push_back( b3CreateRevoluteJoint( worldId, &jointDef ) );
		machine.parts.push_back( { part.partId, wheelId } );
	}

	return machine;
}

// ------------------------------------------------------------------- run ---

static uint64_t HashAccumulate( uint64_t hash, int64_t value )
{
	// FNV-1a po bajtach skwantowanej wartości — telemetria porównywalna run-do-run.
	const uint8_t* bytes = reinterpret_cast<const uint8_t*>( &value );
	for ( size_t i = 0; i < sizeof( value ); ++i )
	{
		hash ^= bytes[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

static int64_t Quantize( double v )
{
	return static_cast<int64_t>( std::llround( v * 10000.0 ) );
}

int main()
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	b3WorldId worldId = b3CreateWorld( &worldDef );

	// Lekcja JV/M7: światy pojazdów bez continuous — koła same są "fast".
	b3World_EnableContinuous( worldId, false );

	// Grunt: statyczny box, top na y=0.
	{
		b3BodyDef groundDef = b3DefaultBodyDef();
		groundDef.type = b3_staticBody;
		groundDef.position = { 0.0, -0.5, 0.0 };
		b3BodyId groundId = b3CreateBody( worldId, &groundDef );
		b3ShapeDef shapeDef = b3DefaultShapeDef();
		b3BoxHull hull = b3MakeBoxHull( 60.0f, 0.5f, 60.0f );
		b3CreateHullShape( groundId, &shapeDef, &hull.base );
	}

	Blueprint bp = MakeTestRover();
	// motorSpeed ujemny wokół +Z toczy w stronę +X (konwencja JV: forward=+X).
	CompiledMachine machine = CompileBlueprint( worldId, bp, 0.0f, 0.65f, 0.0f, -18.0f );

	std::printf( "spike_kernel_v0: blueprint '%s' -> %d parts, %d joints\n", bp.name, (int)machine.parts.size(),
				 (int)machine.joints.size() );

	const float timeStep = 1.0f / 60.0f;
	const int subSteps = 4;
	const int totalSteps = 600; // 10 s
	uint64_t hash = 14695981039346656037ULL;

	for ( int step = 1; step <= totalSteps; ++step )
	{
		b3World_Step( worldId, timeStep, subSteps );
		b3Pos p = b3Body_GetPosition( machine.rootBodyId );
		hash = HashAccumulate( hash, Quantize( (double)p.x ) );
		hash = HashAccumulate( hash, Quantize( (double)p.y ) );
		hash = HashAccumulate( hash, Quantize( (double)p.z ) );
		if ( step % 120 == 0 )
		{
			b3Vec3 v = b3Body_GetLinearVelocity( machine.rootBodyId );
			std::printf( "  t=%4.1fs  pos=(%8.3f, %6.3f, %8.3f)  speed=%5.2f m/s\n", step * timeStep, (double)p.x,
						 (double)p.y, (double)p.z, (double)b3Length( v ) );
		}
	}

	b3Pos finalPos = b3Body_GetPosition( machine.rootBodyId );
	bool drove = std::fabs( (double)finalPos.x ) > 5.0;
	bool upright = (double)finalPos.y > 0.2 && (double)finalPos.y < 1.5;

	std::printf( "trajectory hash: %016llx\n", (unsigned long long)hash );
	std::printf( "drove >5 m: %s, kept ride height: %s\n", drove ? "YES" : "NO", upright ? "YES" : "NO" );

	b3DestroyWorld( worldId );

	if ( !drove || !upright )
	{
		std::printf( "spike_kernel_v0: FAILED\n" );
		return 1;
	}
	std::printf( "spike_kernel_v0: OK\n" );
	return 0;
}
