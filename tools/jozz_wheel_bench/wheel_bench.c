// Jozz wheel/tire research bench v2 - ISOLATED. Links box3d.lib, touches no repo file.
//
// v2 (2026-07-27) repairs every protocol flaw found in the v1 audit:
//   - TRUE reset: every timing rep rebuilds the world from scratch
//   - median + full spread instead of "best of 3"
//   - MARGINAL cost from a 0/1/2/4/8 wheel series (least-squares slope),
//     instead of dividing the whole world step by 4
//   - LOAD-BEARING contact metric (totalNormalImpulse > 0), not the touching flag
//   - realistic corner load (~1900 N) applied as constant downforce
//   - contact churn observed directly (persisted / featureId), no core patch
//   - provenance header (git sha, dirty, lib stamp, compiler, timestamp)
//
// Experiments:
//   A) hull budget, straight prism
//   B) hull budget, tire profile with rounded shoulders
//   C) mass / inertia confound per envelope
//   D) CPU cost: marginal per-wheel slope, box ground and mesh ground
//   E) roll quality under real load, with load-bearing contact + churn + penetration
//   F) contact compliance sweep - does softness absorb facet ripple?
//   G) profile lab - pure geometry, no engine: crown/shoulder curvature and the
//      behaviour of the contact point from upright through camber to lying flat

#include <box3d/box3d.h>

// Rig (swiat, cialo, material, masa, regulator, bilans pracy) zyje w osobnym
// module, wspolnym z okienkiem wizualnym. Stend odpowiada tylko za PROTOKOL:
// kwalifikacje, okno pomiarowe, CSV i manifest.
#include "jozz_qc_rig.h"
#include "jozz_wheel_rig.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined( __has_include )
#if __has_include( "bench_provenance.h" )
#include "bench_provenance.h"
#endif
#endif
#ifndef BENCH_GIT_SHA
#define BENCH_GIT_SHA "unknown"
#endif
#ifndef BENCH_GIT_DIRTY
#define BENCH_GIT_DIRTY "unknown"
#endif
#ifndef BENCH_LIB_STAMP
#define BENCH_LIB_STAMP "unknown"
#endif
#ifndef BENCH_SRC_SHA256
#define BENCH_SRC_SHA256 "unknown"
#endif
// Fizyka rigu jest w osobnym pliku - bez tych dwoch hashy provenance opisywalby
// tylko protokol i milczal o swiecie, materiale i nastawach regulatora.
#ifndef BENCH_RIG_SHA256
#define BENCH_RIG_SHA256 "unknown"
#endif
#ifndef BENCH_RIG_HEADER_SHA256
#define BENCH_RIG_HEADER_SHA256 "unknown"
#endif
#ifndef BENCH_LIB_SHA256
#define BENCH_LIB_SHA256 "unknown"
#endif
// Plik wykonywalny nie moze zahaszowac sam siebie w czasie budowy, wiec wartosc
// przychodzi z --exe-sha256; walidator Q2A sprawdza ja wobec pliku na dysku.
#define BENCH_EXE_SHA256 "supplied-via---exe-sha256"
static const char* g_exeSha = BENCH_EXE_SHA256;

// Tozsamosc kola nalezy do rigu - tutaj tylko krotkie aliasy, zeby nie mnozyc
// definicji tych samych liczb w dwoch plikach.
#define WHEEL_R JOZZ_RIG_WHEEL_R
#define WHEEL_W JOZZ_RIG_WHEEL_W
#define UNSPRUNG_KG JOZZ_RIG_UNSPRUNG_KG
#define PI JOZZ_RIG_PI

// Real corner of the M6 vehicle, from docs/M7_REAL_FORCES_FOUNDATION_PL.md.
// v1 measured a free 44 kg wheel = ~432 N, i.e. about a quarter of this.
// Uzywane wylacznie przez sekcje A-G; Q2A liczy obciazenie z grawitacji swiata.
#define CORNER_LOAD_N 1900.0f

// ---------------------------------------------------------------- helpers

static double NowMs( void )
{
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency( &f );
	QueryPerformanceCounter( &c );
	return 1000.0 * (double)c.QuadPart / (double)f.QuadPart;
}

static int CmpDouble( const void* a, const void* b )
{
	double x = *(const double*)a, y = *(const double*)b;
	return ( x < y ) ? -1 : ( x > y ? 1 : 0 );
}

static double Median( double* v, int n )
{
	qsort( v, (size_t)n, sizeof( double ), CmpDouble );
	return ( n & 1 ) ? v[n / 2] : 0.5 * ( v[n / 2 - 1] + v[n / 2] );
}

// Straight prism (what b3CreateCylinder makes): 2N points, sharp shoulders.
// Tire-like profile: crown radius R, section half width hw, shoulder radius sr.
static int MakeTirePoints( b3Vec3* out, int cap, int sides, int rings, float radius, float halfWidth, float shoulder )
{
	if ( rings < 2 )
		rings = 2;
	if ( sides * rings > cap )
		return 0;

	float flatHalf = halfWidth - shoulder;
	if ( flatHalf < 0.0f )
		flatHalf = 0.0f;

	int n = 0;
	for ( int i = 0; i < sides; ++i )
	{
		float a = 2.0f * PI * (float)i / (float)sides;
		float cx = cosf( a );
		float cz = sinf( a );

		for ( int r = 0; r < rings; ++r )
		{
			float t = ( rings == 1 ) ? 0.0f : ( 2.0f * (float)r / (float)( rings - 1 ) - 1.0f );
			float y, rad;
			if ( flatHalf <= 0.0f )
			{
				float ang = t * 1.57079632679f;
				y = halfWidth * sinf( ang );
				rad = radius - shoulder + shoulder * cosf( ang );
			}
			else
			{
				float split = flatHalf / halfWidth;
				float at = fabsf( t );
				if ( at <= split )
				{
					y = t * halfWidth;
					rad = radius;
				}
				else
				{
					float u = ( at - split ) / ( 1.0f - split );
					float ang = u * 1.57079632679f;
					y = ( t < 0.0f ? -1.0f : 1.0f ) * ( flatHalf + shoulder * sinf( ang ) );
					rad = radius - shoulder + shoulder * cosf( ang );
				}
			}
			out[n++] = ( b3Vec3 ){ rad * cx, y, rad * cz };
		}
	}
	return n;
}

static void MeasureRollingRipple( const b3HullData* hull, int samples, float* outPeakToPeakMm, float* outMeanR )
{
	const b3Vec3* pts = b3GetHullPoints( hull );
	int count = hull->vertexCount;
	float lo = 1e30f, hi = -1e30f, sum = 0.0f;
	for ( int s = 0; s < samples; ++s )
	{
		float a = 2.0f * PI * (float)s / (float)samples;
		float ca = cosf( a ), sa = sinf( a );
		float best = -1e30f;
		for ( int i = 0; i < count; ++i )
		{
			float h = pts[i].x * ca + pts[i].z * sa;
			if ( h > best )
				best = h;
		}
		if ( best < lo )
			lo = best;
		if ( best > hi )
			hi = best;
		sum += best;
	}
	*outPeakToPeakMm = 1000.0f * ( hi - lo );
	*outMeanR = sum / (float)samples;
}

// ---------------------------------------------------------------- envelopes

typedef enum
{
	ENV_SPHERE = 0,
	ENV_CYLINDER32,
	ENV_UNION4,
	ENV_PRISM_MAX,
	ENV_TIRE_PROFILE,
	ENV_COUNT
} EnvMode;

static const char* s_envName[ENV_COUNT] = { "sphere        ", "cylinder-32   ", "phased union-4", "prism-Nmax    ",
											"tire profile  " };

typedef struct
{
	int maxSides;
	int tireSides;
	int tireRings;
} Budgets;

static int BuildEnvelope( b3BodyId body, EnvMode mode, float density, Budgets b )
{
	b3ShapeDef sd = b3DefaultShapeDef();
	sd.density = density;
	static b3Vec3 pts[4096];

	if ( mode == ENV_SPHERE )
	{
		b3Sphere s = { { 0, 0, 0 }, WHEEL_R };
		b3CreateSphereShape( body, &sd, &s );
		return 1;
	}
	if ( mode == ENV_CYLINDER32 )
	{
		b3HullData* h = b3CreateCylinder( WHEEL_W, WHEEL_R, -0.5f * WHEEL_W, 32 );
		b3CreateHullShape( body, &sd, h );
		b3DestroyHull( h );
		return 1;
	}
	if ( mode == ENV_UNION4 )
	{
		b3ShapeDef layer = sd;
		layer.density = density / 4.0f;
		for ( int L = 0; L < 4; ++L )
		{
			float facet = 2.0f * PI / 32.0f;
			float phase = facet * (float)L / 4.0f;
			for ( int i = 0; i < 32; ++i )
			{
				float a = facet * (float)i + phase;
				pts[2 * i + 0] = ( b3Vec3 ){ WHEEL_R * cosf( a ), -0.5f * WHEEL_W, WHEEL_R * sinf( a ) };
				pts[2 * i + 1] = ( b3Vec3 ){ WHEEL_R * cosf( a ), +0.5f * WHEEL_W, WHEEL_R * sinf( a ) };
			}
			b3HullData* h = b3CreateHull( pts, 64, 64 );
			b3CreateHullShape( body, &layer, h );
			b3DestroyHull( h );
		}
		return 4;
	}
	if ( mode == ENV_PRISM_MAX )
	{
		int n = JozzRig_MakePrismPoints( pts, 4096, b.maxSides, WHEEL_R, 0.5f * WHEEL_W );
		b3HullData* h = b3CreateHull( pts, n, n );
		if ( h == NULL )
			return 0;
		b3CreateHullShape( body, &sd, h );
		b3DestroyHull( h );
		return 1;
	}
	{
		int n = MakeTirePoints( pts, 4096, b.tireSides, b.tireRings, WHEEL_R, 0.5f * WHEEL_W, 0.12f );
		b3HullData* h = b3CreateHull( pts, n, n );
		if ( h == NULL )
			return 0;
		b3CreateHullShape( body, &sd, h );
		b3DestroyHull( h );
		return 1;
	}
}

// Zamrozenie masy zyje w rigu (JozzRig_FreezeMass) - sekcje A-G i Q2A musza
// uzywac tej samej. Tu nie ma juz kopii.

// ---------------------------------------------------------------- contact telemetry
// Public API only. b3Body_GetContactData returns the full manifolds, so the
// churn / load-bearing question needs NO core patch (verified src/body.c:464).

typedef struct
{
	int touchingPoints;	 // every point in every manifold
	int loadedPoints;	 // totalNormalImpulse > 0  <- the honest one
	int newLoadedPoints; // loaded AND !persisted   <- churn
	float totalImpulse;
	float deepestPenetrationMm; // most negative separation among loaded points
	int manifolds;
} ContactSample;

static ContactSample SampleContacts( b3BodyId body )
{
	ContactSample cs;
	memset( &cs, 0, sizeof( cs ) );
	static b3ContactData cd[128];
	int cc = b3Body_GetContactData( body, cd, 128 );
	float deepest = 0.0f;
	for ( int i = 0; i < cc; ++i )
	{
		cs.manifolds += cd[i].manifoldCount;
		for ( int m = 0; m < cd[i].manifoldCount; ++m )
		{
			const b3Manifold* mf = &cd[i].manifolds[m];
			for ( int k = 0; k < mf->pointCount; ++k )
			{
				const b3ManifoldPoint* p = &mf->points[k];
				cs.touchingPoints++;
				if ( p->totalNormalImpulse > 0.0f )
				{
					cs.loadedPoints++;
					cs.totalImpulse += p->totalNormalImpulse;
					if ( p->persisted == false )
						cs.newLoadedPoints++;
					if ( p->separation < deepest )
						deepest = p->separation;
				}
			}
		}
	}
	cs.deepestPenetrationMm = -1000.0f * deepest;
	return cs;
}

// ---------------------------------------------------------------- A + B

static void ExperimentHullBudget( void )
{
	printf( "\n=== A) HULL BUDGET: straight prism (sharp shoulders) ===\n" );
	printf( "%6s %8s %8s %8s %8s %12s %10s\n", "sides", "pts_in", "verts", "halfedg", "faces", "ripple_mm", "meanR_m" );

	static b3Vec3 pts[4096];
	int lastGood = 0;
	for ( int sides = 8; sides <= 200; sides += ( sides < 40 ? 4 : 8 ) )
	{
		int n = JozzRig_MakePrismPoints( pts, 4096, sides, WHEEL_R, 0.5f * WHEEL_W );
		if ( n == 0 )
			break;
		b3HullData* h = b3CreateHull( pts, n, n );
		if ( h == NULL )
		{
			printf( "%6d %8d   ---- b3CreateHull returned NULL (budget exceeded)\n", sides, n );
			break;
		}
		float rip, meanR;
		MeasureRollingRipple( h, 2048, &rip, &meanR );
		printf( "%6d %8d %8d %8d %8d %12.3f %10.4f\n", sides, n, h->vertexCount, h->edgeCount, h->faceCount, rip,
				meanR );
		lastGood = sides;
		b3DestroyHull( h );
	}
	printf( "  -> max usable prism sides = %d\n", lastGood );

	printf( "\n=== B) HULL BUDGET: tire profile (rounded shoulders) ===\n" );
	printf( "  shoulder radius 0.12 m, section half width %.4f m\n", 0.5f * WHEEL_W );
	printf( "%6s %6s %8s %8s %8s %8s %12s\n", "sides", "rings", "pts_in", "verts", "halfedg", "faces", "ripple_mm" );
	for ( int rings = 2; rings <= 6; ++rings )
	{
		int best = 0;
		for ( int sides = 8; sides <= 128; sides += 2 )
		{
			int n = MakeTirePoints( pts, 4096, sides, rings, WHEEL_R, 0.5f * WHEEL_W, 0.12f );
			if ( n == 0 )
				break;
			b3HullData* h = b3CreateHull( pts, n, n );
			if ( h == NULL )
				break;
			best = sides;
			if ( sides % 8 == 0 || sides < 16 )
			{
				float rip, meanR;
				MeasureRollingRipple( h, 2048, &rip, &meanR );
				printf( "%6d %6d %8d %8d %8d %8d %12.3f\n", sides, rings, n, h->vertexCount, h->edgeCount, h->faceCount,
						rip );
			}
			b3DestroyHull( h );
		}
		printf( "  -> rings=%d : max sides = %d\n", rings, best );
	}
}

// ---------------------------------------------------------------- C

static void ExperimentMassConfound( Budgets b )
{
	printf( "\n=== C) MASS / INERTIA CONFOUND (density 77 kg/m^3, product default path) ===\n" );
	printf( "%-16s %8s %10s %10s %10s   %s\n", "envelope", "mass_kg", "I_spin", "I_trans", "I_sp/mr2", "note" );

	for ( int m = 0; m < ENV_COUNT; ++m )
	{
		b3WorldDef wd = b3DefaultWorldDef();
		b3WorldId w = b3CreateWorld( &wd );
		b3BodyDef bd = b3DefaultBodyDef();
		bd.type = b3_dynamicBody;
		b3BodyId body = b3CreateBody( w, &bd );
		int n = BuildEnvelope( body, (EnvMode)m, 77.0f, b );
		if ( n == 0 )
		{
			printf( "%-16s   (not representable)\n", s_envName[m] );
			b3DestroyWorld( w );
			continue;
		}
		b3MassData md = b3Body_GetMassData( body );
		float iSpin = md.inertia.cy.y;
		float iTrans = 0.5f * ( md.inertia.cx.x + md.inertia.cz.z );
		printf( "%-16s %8.2f %10.3f %10.3f %10.3f   %d shape(s)\n", s_envName[m], md.mass, iSpin, iTrans,
				iSpin / ( md.mass * WHEEL_R * WHEEL_R + 1e-9f ), n );
		b3DestroyWorld( w );
	}
	printf( "  physical reference: solid sphere I/mr2 = 0.400 ; real tire+rim ~0.55-0.80\n" );
}

// ---------------------------------------------------------------- D  (repaired)

// One timing run = one FRESH world. No state carries between reps.
static double TimeFreshWorld( EnvMode m, int wheelCount, int useMeshGround, int steps, Budgets bud, int* outOk )
{
	b3WorldDef wd = b3DefaultWorldDef();
	wd.workerCount = 1;
	b3WorldId w = b3CreateWorld( &wd );

	b3BodyDef gd = b3DefaultBodyDef();
	gd.position = ( b3Pos ){ 0.0f, -1.0f, 0.0f };
	b3BodyId ground = b3CreateBody( w, &gd );
	b3ShapeDef gs = b3DefaultShapeDef();
	gs.baseMaterial.friction = 0.9f;
	b3MeshData* mesh = NULL;
	b3BoxHull box = b3MakeBoxHull( 400.0f, 1.0f, 30.0f );
	if ( useMeshGround )
	{
		mesh = b3CreateGridMesh( 400, 30, 2.0f, 1, true );
		b3CreateMeshShape( ground, &gs, mesh, ( b3Vec3 ){ 1.0f, 1.0f, 1.0f } );
	}
	else
	{
		b3CreateHullShape( ground, &gs, &box.base );
	}

	b3BodyId wheels[16];
	int built = 0;
	for ( int i = 0; i < wheelCount; ++i )
	{
		b3BodyDef bd = b3DefaultBodyDef();
		bd.type = b3_dynamicBody;
		bd.position = ( b3Pos ){ (float)( i * 3 ) - 190.0f, WHEEL_R + 0.02f, 0.0f };
		bd.rotation = b3ComputeQuatBetweenUnitVectors( ( b3Vec3 ){ 0, 1, 0 }, ( b3Vec3 ){ 0, 0, 1 } );
		bd.angularVelocity = ( b3Vec3 ){ 0.0f, 0.0f, -13.0f / WHEEL_R };
		bd.linearVelocity = ( b3Vec3 ){ 13.0f, 0.0f, 0.0f };
		bd.enableSleep = false;
		b3BodyId body = b3CreateBody( w, &bd );
		int n = BuildEnvelope( body, m, 77.0f, bud );
		if ( n == 0 )
		{
			b3DestroyWorld( w );
			if ( mesh )
				b3DestroyMesh( mesh );
			*outOk = 0;
			return 0.0;
		}
		JozzRig_FreezeMass( body, UNSPRUNG_KG );
		wheels[built++] = body;
	}

	b3World_EnableContinuous( w, false );
	b3Vec3 down = ( b3Vec3 ){ 0.0f, -( CORNER_LOAD_N - UNSPRUNG_KG * 9.81f ), 0.0f };
	for ( int i = 0; i < 60; ++i )
	{
		for ( int k = 0; k < built; ++k )
			b3Body_ApplyForceToCenter( wheels[k], down, true );
		b3World_Step( w, 1.0f / 60.0f, 4 );
	}

	double t0 = NowMs();
	for ( int i = 0; i < steps; ++i )
	{
		for ( int k = 0; k < built; ++k )
			b3Body_ApplyForceToCenter( wheels[k], down, true );
		b3World_Step( w, 1.0f / 60.0f, 4 );
	}
	double t1 = NowMs();

	b3DestroyWorld( w );
	if ( mesh )
		b3DestroyMesh( mesh );
	*outOk = 1;
	return ( t1 - t0 ) / (double)steps;
}

#define COST_REPS 7
#define COST_STEPS 400

static void ExperimentCost( Budgets bud, int useMeshGround )
{
	static const int counts[] = { 0, 1, 2, 4, 8 };
	const int nCounts = (int)( sizeof( counts ) / sizeof( counts[0] ) );

	printf( "\n=== D) CPU COST - MARGINAL, %s ground ===\n", useMeshGround ? "MESH" : "box hull" );
	printf( "    protocol: %d fresh worlds x %d steps per point; median reported; slope = least squares\n", COST_REPS,
			COST_STEPS );
	printf( "%-16s %10s %10s %10s %10s %10s   %12s %10s\n", "envelope", "n=0 ms", "n=1 ms", "n=2 ms", "n=4 ms",
			"n=8 ms", "us/wheel", "spread%" );

	// empty-world baseline is identical for every envelope, measure once
	double emptyReps[COST_REPS];
	for ( int r = 0; r < COST_REPS; ++r )
	{
		int ok = 0;
		emptyReps[r] = TimeFreshWorld( ENV_SPHERE, 0, useMeshGround, COST_STEPS, bud, &ok );
	}
	double emptyMed = Median( emptyReps, COST_REPS );

	double sphereSlope = 0.0;
	for ( int m = 0; m < ENV_COUNT; ++m )
	{
		double med[8];
		double worstSpread = 0.0;
		int ok = 1;
		med[0] = emptyMed;
		for ( int c = 1; c < nCounts; ++c )
		{
			double reps[COST_REPS];
			for ( int r = 0; r < COST_REPS; ++r )
			{
				int rok = 0;
				reps[r] = TimeFreshWorld( (EnvMode)m, counts[c], useMeshGround, COST_STEPS, bud, &rok );
				if ( rok == 0 )
					ok = 0;
			}
			if ( ok == 0 )
				break;
			med[c] = Median( reps, COST_REPS );
			double spread = 100.0 * ( reps[COST_REPS - 1] - reps[0] ) / ( med[c] > 0 ? med[c] : 1.0 );
			if ( spread > worstSpread )
				worstSpread = spread;
		}
		if ( ok == 0 )
		{
			printf( "%-16s   (not representable)\n", s_envName[m] );
			continue;
		}

		// least squares slope of ms vs wheel count
		double sx = 0, sy = 0, sxx = 0, sxy = 0;
		for ( int c = 0; c < nCounts; ++c )
		{
			double x = (double)counts[c];
			sx += x;
			sy += med[c];
			sxx += x * x;
			sxy += x * med[c];
		}
		double nn = (double)nCounts;
		double slope = ( nn * sxy - sx * sy ) / ( nn * sxx - sx * sx );
		if ( m == ENV_SPHERE )
			sphereSlope = slope;
		printf( "%-16s %10.4f %10.4f %10.4f %10.4f %10.4f   %12.2f %9.1f%%   (%.2fx sphere)\n", s_envName[m], med[0],
				med[1], med[2], med[3], med[4], 1000.0 * slope, worstSpread,
				sphereSlope > 0 ? slope / sphereSlope : 1.0 );
		fflush( stdout );
	}
	printf( "  note: n=0 column is the world+ground floor cost, identical for all rows.\n" );
	printf( "        us/wheel is the SLOPE, i.e. what one more wheel actually costs.\n" );
}

// ---------------------------------------------------------------- E  (repaired)

// ---------------------------------------------------------------- JP-02 phase telemetry
// Instrumentacja, nie zmiana eksperymentu. Kontrakt: JP02_PHASE_TELEMETRY_CONTRACT.md.
// Cala praca stoi za `if ( tele != NULL )`, wiec bez flagi --phase-telemetry do petli
// nie dochodzi ani jedna dodatkowa instrukcja.

#define TELE_SCHEMA 1
#define TELE_DT ( 1.0 / 60.0 )
#define TELE_SUBSTEPS 4
#define TELE_WARMUP_STEPS 120
#define TELE_MEASURE_STEPS 240

static FILE* g_tele = NULL;
static int g_teleRecords = 0;
static int g_teleFailed = 0;

typedef struct
{
	double pathLength;	 // calka |dx|
	double spinAngle;	 // calka omega_spin dt        (ZE ZNAKIEM, moze malec)
	double absSpinAngle; // calka |omega_spin| dt      (niemalejaca)
	double startX;
	b3Pos prevPos;
} PhaseAccum;

// s_envName jest dopelnione spacjami pod wydruk tabeli; CSV potrzebuje nazwy bez ogona
static const char* TrimEnvName( EnvMode m )
{
	static char buf[32];
	strncpy( buf, s_envName[m], sizeof( buf ) - 1 );
	buf[sizeof( buf ) - 1] = '\0';
	for ( int i = (int)strlen( buf ) - 1; i >= 0 && buf[i] == ' '; --i )
		buf[i] = '\0';
	return buf;
}

// Dot3 / os kola / omega_spin / JozzWheelKin / JozzRig_Kinematics zostaly
// PRZENIESIONE do jozz_wheel_rig.c. Konwencja znakow ma jedna implementacje;
// dwie kopie rozjechalyby sie przy pierwszej zmianie.

static void PhaseAccumInit( PhaseAccum* a, b3BodyId body )
{
	memset( a, 0, sizeof( *a ) );
	a->prevPos = b3Body_GetPosition( body );
	a->startX = a->prevPos.x;
}

static void PhaseAccumStep( PhaseAccum* a, b3BodyId body )
{
	b3Pos p = b3Body_GetPosition( body );
	double dx = (double)p.x - a->prevPos.x, dy = (double)p.y - a->prevPos.y, dz = (double)p.z - a->prevPos.z;
	a->pathLength += sqrt( dx * dx + dy * dy + dz * dz );
	a->prevPos = p;

	double ws = JozzRig_OmegaSpin( body );
	a->spinAngle += ws * TELE_DT;
	a->absSpinAngle += fabs( ws ) * TELE_DT;
}

static int TeleNum( double v )
{
	// Nie-skonczona wartosc nie moze trafic do pliku. Cichy NaN w dowodzie jest
	// gorszy niz brak dowodu.
	if ( !( v == v ) || v > 1e300 || v < -1e300 )
	{
		g_teleFailed = 1;
		return 0;
	}
	return fprintf( g_tele, ",%.9g", v ) >= 0;
}

static void EmitPhase( const char* loadCase, const char* envelope, const char* boundary, int globalStep,
					   b3BodyId body, const PhaseAccum* acc, float nominalLoadN )
{
	if ( g_tele == NULL || g_teleFailed )
		return;

	b3Pos p = b3Body_GetPosition( body );
	b3Vec3 v = b3Body_GetLinearVelocity( body );
	b3Vec3 w = b3Body_GetAngularVelocity( body );
	b3MassData md = b3Body_GetMassData( body );

	JozzWheelKin k = JozzRig_Kinematics( body );
	b3Vec3 axleU = k.axleUnit;
	int degenerate = k.degenerate;
	double omegaSpin = k.omegaSpin;
	double referenceRimSpeed = k.referenceRimSpeed;
	double referenceSlipSpeed = k.referenceSlipSpeed;
	double keTrans = k.keTrans, keRot = k.keRot;
	double vLong = JozzRig_Dot3( v, k.forward );

	// Slip ratio nie jest wyprowadzane przy predkosci bliskiej zeru - dziedzina
	// jawna, nie ukryta w wartosci.
	double ref = fabs( referenceRimSpeed ) > fabs( vLong ) ? fabs( referenceRimSpeed ) : fabs( vLong );
	int ratioValid = ref > 0.1;
	double slipRatio = ratioValid ? referenceSlipSpeed / ref : 0.0;

	double gravityLoad = (double)md.mass * 10.0;			   // swiat: b3DefaultWorldDef gravity.y = -10
	double externalDown = (double)nominalLoadN - md.mass * 9.81; // stend liczy docisk z 9.81
	if ( externalDown < 0.0 )
		externalDown = 0.0;

	int ok = fprintf( g_tele, "%d,%s,%s,%s,%d,%.9g", TELE_SCHEMA, loadCase, envelope, boundary, globalStep,
					  globalStep * TELE_DT ) >= 0;
	ok &= TeleNum( p.x ); ok &= TeleNum( p.y ); ok &= TeleNum( p.z );
	ok &= TeleNum( v.x ); ok &= TeleNum( v.y ); ok &= TeleNum( v.z );
	ok &= TeleNum( w.x ); ok &= TeleNum( w.y ); ok &= TeleNum( w.z );
	ok &= TeleNum( axleU.x ); ok &= TeleNum( axleU.y ); ok &= TeleNum( axleU.z );
	ok &= TeleNum( omegaSpin );
	ok &= TeleNum( WHEEL_R ); // reference_radius_m - jawnie, zeby nie bylo domyslne
	ok &= TeleNum( referenceRimSpeed ); ok &= TeleNum( referenceSlipSpeed );
	ok &= TeleNum( keTrans ); ok &= TeleNum( keRot ); ok &= TeleNum( keTrans + keRot );
	ok &= TeleNum( (double)p.x - acc->startX );
	ok &= TeleNum( acc->pathLength );
	ok &= TeleNum( acc->spinAngle );
	ok &= TeleNum( acc->absSpinAngle / ( 2.0 * PI ) );
	ok &= TeleNum( nominalLoadN );
	ok &= TeleNum( externalDown );
	ok &= TeleNum( gravityLoad );
	ok &= TeleNum( externalDown + gravityLoad );
	ok &= TeleNum( TELE_DT );
	ok &= ( fprintf( g_tele, ",%d,%.9g,%d,%d\n", TELE_SUBSTEPS, slipRatio, ratioValid, degenerate ) >= 0 );
	if ( !ok )
		g_teleFailed = 1;
	else
		++g_teleRecords;
}

static const char* TELE_HEADER =
	"schema_version,load_case,envelope,boundary,global_step,time_s,"
	"position_x,position_y,position_z,"
	"linear_velocity_x,linear_velocity_y,linear_velocity_z,"
	"angular_velocity_x,angular_velocity_y,angular_velocity_z,"
	"wheel_axle_world_x,wheel_axle_world_y,wheel_axle_world_z,"
	"omega_spin_rad_s,reference_radius_m,reference_rim_speed_m_s,reference_slip_speed_m_s,"
	"translational_kinetic_energy_J,rotational_kinetic_energy_J,total_kinetic_energy_J,"
	"cumulative_x_displacement_m,cumulative_path_length_m,cumulative_spin_angle_rad,cumulative_revolutions,"
	"nominal_load_N,external_downforce_N,gravity_load_N,effective_static_load_N,"
	"dt_s,substeps,slip_ratio,slip_ratio_domain_valid,axle_degenerate\n";

typedef struct
{
	double vyRms;
	double loadedPointsAvg;
	double churnPct;	 // new loaded points / loaded points
	double loadedStepPct; // steps with at least one loaded point
	double penetrationMm;
	double yDropMm;
	double vxEnd;
	double manifoldsAvg;
} RollResult;

// teleLoadCase == NULL  ->  zero telemetrii i zero dodatkowej pracy (sekcja F)
static RollResult RunRoll( EnvMode m, Budgets bud, float loadN, float contactHertz, float contactDamping,
						   float contactSpeed, int* outOk, const char* teleLoadCase )
{
	RollResult rr;
	memset( &rr, 0, sizeof( rr ) );

	b3WorldDef wd = b3DefaultWorldDef();
	wd.workerCount = 1;
	if ( contactHertz > 0.0f )
	{
		wd.contactHertz = contactHertz;
		wd.contactDampingRatio = contactDamping;
		wd.contactSpeed = contactSpeed;
	}
	b3WorldId w = b3CreateWorld( &wd );

	b3BodyDef gd = b3DefaultBodyDef();
	gd.position = ( b3Pos ){ 0.0f, -1.0f, 0.0f };
	b3BodyId ground = b3CreateBody( w, &gd );
	b3ShapeDef gs = b3DefaultShapeDef();
	gs.baseMaterial.friction = 1.2f;
	b3BoxHull box = b3MakeBoxHull( 400.0f, 1.0f, 60.0f );
	b3CreateHullShape( ground, &gs, &box.base );

	b3BodyDef bd = b3DefaultBodyDef();
	bd.type = b3_dynamicBody;
	bd.position = ( b3Pos ){ -180.0f, WHEEL_R + 0.001f, 0.0f };
	bd.rotation = b3ComputeQuatBetweenUnitVectors( ( b3Vec3 ){ 0, 1, 0 }, ( b3Vec3 ){ 0, 0, 1 } );
	bd.linearVelocity = ( b3Vec3 ){ 13.0f, 0.0f, 0.0f };
	bd.angularVelocity = ( b3Vec3 ){ 0.0f, 0.0f, -13.0f / WHEEL_R };
	bd.enableSleep = false;
	bd.allowFastRotation = true;
	b3BodyId body = b3CreateBody( w, &bd );
	if ( BuildEnvelope( body, m, 77.0f, bud ) == 0 )
	{
		b3DestroyWorld( w );
		*outOk = 0;
		return rr;
	}
	JozzRig_FreezeMass( body, UNSPRUNG_KG );

	b3Vec3 down = ( b3Vec3 ){ 0.0f, -( loadN - UNSPRUNG_KG * 9.81f ), 0.0f };
	b3World_EnableContinuous( w, false );

	PhaseAccum acc;
	if ( teleLoadCase )
	{
		PhaseAccumInit( &acc, body );
		EmitPhase( teleLoadCase, TrimEnvName( m ), "INITIAL", 0, body, &acc, loadN );
	}

	for ( int i = 0; i < 120; ++i )
	{
		b3Body_ApplyForceToCenter( body, down, true );
		b3World_Step( w, 1.0f / 60.0f, 4 );
		if ( teleLoadCase )
			PhaseAccumStep( &acc, body );
	}
	// Rekord powstaje PO 120. kroku i PRZED pierwszym krokiem pomiaru - to jest
	// dokladnie stan, z ktorego rusza obecna petla pomiarowa.
	if ( teleLoadCase )
		EmitPhase( teleLoadCase, TrimEnvName( m ), "MEASURE_START", TELE_WARMUP_STEPS, body, &acc, loadN );

	double sumSq = 0.0, sumLoaded = 0.0, sumNew = 0.0, sumPen = 0.0, sumMan = 0.0;
	int steps = 240, stepsLoaded = 0;
	float yMin = 1e30f, yMax = -1e30f;
	for ( int i = 0; i < steps; ++i )
	{
		b3Body_ApplyForceToCenter( body, down, true );
		b3World_Step( w, 1.0f / 60.0f, 4 );
		b3Vec3 v = b3Body_GetLinearVelocity( body );
		sumSq += (double)v.y * (double)v.y;
		b3Pos p = b3Body_GetPosition( body );
		if ( (float)p.y < yMin )
			yMin = (float)p.y;
		if ( (float)p.y > yMax )
			yMax = (float)p.y;

		ContactSample cs = SampleContacts( body );
		sumLoaded += cs.loadedPoints;
		sumNew += cs.newLoadedPoints;
		sumPen += cs.deepestPenetrationMm;
		sumMan += cs.manifolds;
		if ( cs.loadedPoints > 0 )
			++stepsLoaded;
		if ( teleLoadCase )
			PhaseAccumStep( &acc, body );
	}
	if ( teleLoadCase )
		EmitPhase( teleLoadCase, TrimEnvName( m ), "MEASURE_END", TELE_WARMUP_STEPS + TELE_MEASURE_STEPS, body, &acc,
				   loadN );
	b3Vec3 vEnd = b3Body_GetLinearVelocity( body );

	rr.vyRms = sqrt( sumSq / (double)steps );
	rr.loadedPointsAvg = sumLoaded / (double)steps;
	rr.churnPct = ( sumLoaded > 0.0 ) ? 100.0 * sumNew / sumLoaded : 0.0;
	rr.loadedStepPct = 100.0 * (double)stepsLoaded / (double)steps;
	rr.penetrationMm = sumPen / (double)steps;
	rr.yDropMm = 1000.0f * ( yMax - yMin );
	rr.vxEnd = vEnd.x;
	rr.manifoldsAvg = sumMan / (double)steps;

	b3DestroyWorld( w );
	*outOk = 1;
	return rr;
}

static void ExperimentRollQuality( Budgets bud )
{
	printf( "\n=== E) ROLL QUALITY under REAL CORNER LOAD (%.0f N, mass frozen %.0f kg, v=13 m/s, 4 s) ===\n",
			CORNER_LOAD_N, UNSPRUNG_KG );
	printf( "    v1 used a free %.0f kg wheel = %.0f N, about a quarter of this. Rankings may differ.\n", UNSPRUNG_KG,
			UNSPRUNG_KG * 9.81f );
	printf( "%-16s %10s %10s %10s %9s %10s %9s %8s\n", "envelope", "vy_rms", "loadedPts", "churn_%", "loaded_%",
			"penet_mm", "ydrop_mm", "vx_end" );

	for ( int m = 0; m < ENV_COUNT; ++m )
	{
		int ok = 0;
		RollResult r = RunRoll( (EnvMode)m, bud, CORNER_LOAD_N, 0.0f, 0.0f, 0.0f, &ok, "corner_1900N" );
		if ( !ok )
		{
			printf( "%-16s   (not representable)\n", s_envName[m] );
			continue;
		}
		printf( "%-16s %10.4f %10.2f %10.1f %8.1f%% %10.3f %9.3f %8.2f\n", s_envName[m], r.vyRms, r.loadedPointsAvg,
				r.churnPct, r.loadedStepPct, r.penetrationMm, r.yDropMm, r.vxEnd );
		fflush( stdout );
	}
	printf( "  loadedPts = manifold points with totalNormalImpulse > 0 (NOT the touching flag)\n" );
	printf( "  churn_%% = share of load-bearing points that did NOT exist last step (warm start loss)\n" );

	printf( "\n  --- same runs at the v1 load (%.0f N) for direct comparison with the 2026-07-25 evidence ---\n",
			UNSPRUNG_KG * 9.81f );
	printf( "%-16s %10s %10s %10s %9s %10s\n", "envelope", "vy_rms", "loadedPts", "churn_%", "loaded_%", "penet_mm" );
	for ( int m = 0; m < ENV_COUNT; ++m )
	{
		int ok = 0;
		RollResult r = RunRoll( (EnvMode)m, bud, UNSPRUNG_KG * 9.81f, 0.0f, 0.0f, 0.0f, &ok, "v1_432N" );
		if ( !ok )
			continue;
		printf( "%-16s %10.4f %10.2f %10.1f %8.1f%% %10.3f\n", s_envName[m], r.vyRms, r.loadedPointsAvg, r.churnPct,
				r.loadedStepPct, r.penetrationMm );
	}
}

// ---------------------------------------------------------------- F  (new)

// The cheapest possible falsifier for the whole "we need a new shape" premise.
// If contact compliance at a physically meaningful tire frequency absorbs facet
// ripple, the faceted-hull family survives and no core patch is justified by
// ripple alone. A real tire on a car sits at roughly 10-15 Hz wheel hop.
static void ExperimentCompliance( Budgets bud )
{
	static const float hz[] = { 60.0f, 30.0f, 20.0f, 15.0f, 10.0f, 6.0f };
	const int nHz = (int)( sizeof( hz ) / sizeof( hz[0] ) );
	static const EnvMode probe[] = { ENV_SPHERE, ENV_CYLINDER32, ENV_PRISM_MAX, ENV_TIRE_PROFILE };
	const int nProbe = (int)( sizeof( probe ) / sizeof( probe[0] ) );

	b3WorldDef def = b3DefaultWorldDef();
	printf( "\n=== F) CONTACT COMPLIANCE SWEEP (load %.0f N; world default hertz=%.1f damping=%.2f speed=%.1f) ===\n",
			CORNER_LOAD_N, def.contactHertz, def.contactDampingRatio, def.contactSpeed );
	printf( "    question: does softening the contact absorb facet ripple without any new shape?\n" );
	printf( "%-16s", "envelope" );
	for ( int i = 0; i < nHz; ++i )
		printf( " %8.0fHz", hz[i] );
	printf( "   metric\n" );

	for ( int pi = 0; pi < nProbe; ++pi )
	{
		double vy[8], pen[8], churn[8], loaded[8];
		int ok = 1;
		for ( int i = 0; i < nHz; ++i )
		{
			int rok = 0;
			RollResult r = RunRoll( probe[pi], bud, CORNER_LOAD_N, hz[i], def.contactDampingRatio, def.contactSpeed,
									&rok, NULL ); // sekcja F: bez telemetrii
			if ( !rok )
			{
				ok = 0;
				break;
			}
			vy[i] = r.vyRms;
			pen[i] = r.penetrationMm;
			churn[i] = r.churnPct;
			loaded[i] = r.loadedStepPct;
		}
		if ( !ok )
		{
			printf( "%-16s   (not representable)\n", s_envName[probe[pi]] );
			continue;
		}
		printf( "%-16s", s_envName[probe[pi]] );
		for ( int i = 0; i < nHz; ++i )
			printf( " %10.4f", vy[i] );
		printf( "   vy_rms\n" );
		printf( "%-16s", "" );
		for ( int i = 0; i < nHz; ++i )
			printf( " %10.2f", pen[i] );
		printf( "   penetration_mm\n" );
		printf( "%-16s", "" );
		for ( int i = 0; i < nHz; ++i )
			printf( " %10.1f", churn[i] );
		printf( "   churn_%%\n" );
		printf( "%-16s", "" );
		for ( int i = 0; i < nHz; ++i )
			printf( " %10.1f", loaded[i] );
		printf( "   loaded_step_%%\n" );
		fflush( stdout );
	}
	printf( "  reference: a real tire deflects ~20-40 mm at 1900 N. Penetration here IS the deflection\n" );
	printf( "             the solver is willing to tolerate before it pushes back.\n" );
}

// ---------------------------------------------------------------- G  (new)
// PROFILE LAB - pure mathematics, no engine, no src/. Answers the Owner's
// question directly: what happens on the crown, on the shoulder, on the side.

typedef enum
{
	PF_SPHERE = 0,
	PF_ELLIPSOID,
	PF_SWEPT_DISK,
	PF_LAME4,
	PF_LAME8,
	PF_HULL_PRISM,
	PF_HULL_TIRE,
	PF_COUNT
} ProfileKind;

static const char* s_pfName[PF_COUNT] = { "sphere R=0.514", "ellipsoid     ", "swept-disk r80", "revolved Lame p=4",
										  "revolved Lame p=8", "prism-42 hull ", "tire-prof hull" };

static b3Vec3 s_labPts[4096];
static int s_labPrismN = 0;
static int s_labTireN = 0;

// Support function h(d) for a NON-unit d, positively homogeneous of degree 1.
// Spin axis = local Y. s = radial magnitude, q = axial component.
static double ProfileSupport( ProfileKind k, double dx, double dy, double dz )
{
	double s = sqrt( dx * dx + dz * dz );
	double q = fabs( dy );
	double len = sqrt( dx * dx + dy * dy + dz * dz );
	double R = WHEEL_R, HW = 0.5 * WHEEL_W;

	switch ( k )
	{
		case PF_SPHERE:
			return R * len;

		case PF_ELLIPSOID:
			// radial semi-axis R, axial semi-axis HW
			return sqrt( R * R * s * s + HW * HW * q * q );

		case PF_SWEPT_DISK:
		{
			// disk(Rc) (+) segment(hc) (+) sphere(r) ; r = 80 mm shoulder
			double r = 0.080;
			double Rc = R - r;
			double hc = HW - r;
			if ( hc < 0.0 )
				hc = 0.0;
			return Rc * s + hc * q + r * len;
		}

		case PF_LAME4:
		case PF_LAME8:
		{
			// revolved Lame (superellipse) profile: |u/R|^p + |v/HW|^p <= 1
			// support = dual norm  ->  ((R*s)^p' + (HW*q)^p')^(1/p'),  1/p+1/p'=1
			double p = ( k == PF_LAME4 ) ? 4.0 : 8.0;
			double pp = p / ( p - 1.0 );
			return pow( pow( R * s, pp ) + pow( HW * q, pp ), 1.0 / pp );
		}

		case PF_HULL_PRISM:
		case PF_HULL_TIRE:
		{
			int n = ( k == PF_HULL_PRISM ) ? s_labPrismN : s_labTireN;
			int off = ( k == PF_HULL_PRISM ) ? 0 : s_labPrismN;
			double best = -1e30;
			for ( int i = 0; i < n; ++i )
			{
				const b3Vec3* p = &s_labPts[off + i];
				double h = p->x * dx + p->y * dy + p->z * dz;
				if ( h > best )
					best = h;
			}
			return best;
		}
		default:
			return 0.0;
	}
}

// Support POINT = gradient of the support function (homogeneity degree 1).
static void ProfileSupportPoint( ProfileKind k, double dx, double dy, double dz, double out[3] )
{
	const double e = 1e-5;
	out[0] = ( ProfileSupport( k, dx + e, dy, dz ) - ProfileSupport( k, dx - e, dy, dz ) ) / ( 2 * e );
	out[1] = ( ProfileSupport( k, dx, dy + e, dz ) - ProfileSupport( k, dx, dy - e, dz ) ) / ( 2 * e );
	out[2] = ( ProfileSupport( k, dx, dy, dz + e ) - ProfileSupport( k, dx, dy, dz - e ) ) / ( 2 * e );
}

// Radius of curvature of the surface at the point whose outward normal is u,
// measured in the plane spanned by u and t:  R = h(u) + h''(theta).
static double ProfileCurvatureAt( ProfileKind k, const double u[3], const double t[3], double e )
{
	double hp[3], hm[3];
	for ( int i = 0; i < 3; ++i )
	{
		hp[i] = cos( e ) * u[i] + sin( e ) * t[i];
		hm[i] = cos( e ) * u[i] - sin( e ) * t[i];
	}
	double h0 = ProfileSupport( k, u[0], u[1], u[2] );
	double h1 = ProfileSupport( k, hp[0], hp[1], hp[2] );
	double h2 = ProfileSupport( k, hm[0], hm[1], hm[2] );
	return h0 + ( h1 + h2 - 2.0 * h0 ) / ( e * e );
}

// A true curvature is step-independent. A value that doubles when the probe
// halves is a KINK: a flat band (support has a corner) or a hull facet edge.
// A value near zero is a CORNER of the surface (a hull vertex).
static void DescribeCurvature( ProfileKind k, const double u[3], const double t[3], char* out, size_t cap )
{
	double a = ProfileCurvatureAt( k, u, t, 2e-3 );
	double b = ProfileCurvatureAt( k, u, t, 1e-3 );
	if ( fabs( a ) < 1e-3 && fabs( b ) < 1e-3 )
	{
		snprintf( out, cap, "CORNER" );
		return;
	}
	if ( fabs( a - b ) <= 0.25 * fabs( a ) )
	{
		snprintf( out, cap, "rho=%.4f", a );
		return;
	}
	// Not step-independent -> the surface is flat here (kink in the support
	// function, or genuinely zero curvature). Report what actually matters:
	// how far the contact has already travelled at one degree of lean.
	{
		double g = 1.0 * PI / 180.0;
		double n0[3], n1[3], p0[3], p1[3];
		for ( int i = 0; i < 3; ++i )
		{
			n0[i] = u[i];
			n1[i] = cos( g ) * u[i] + sin( g ) * t[i];
		}
		ProfileSupportPoint( k, n0[0], n0[1], n0[2], p0 );
		ProfileSupportPoint( k, n1[0], n1[1], n1[2], p1 );
		double d = 0.0;
		for ( int i = 0; i < 3; ++i )
			d += ( p1[i] - p0[i] ) * ( p1[i] - p0[i] );
		snprintf( out, cap, "flat b=%.0fmm", 1000.0 * sqrt( d ) );
	}
}

static void ExperimentProfileLab( Budgets bud )
{
	s_labPrismN = JozzRig_MakePrismPoints( s_labPts, 2048, bud.maxSides, WHEEL_R, 0.5f * WHEEL_W );
	s_labTireN = MakeTirePoints( s_labPts + s_labPrismN, 2048, bud.tireSides, bud.tireRings, WHEEL_R, 0.5f * WHEEL_W,
								 0.12f );

	printf( "\n=== G) PROFILE LAB - pure geometry, no engine ===\n" );
	printf( "    all candidates normalised to outer radius %.4f m and total width %.4f m\n", WHEEL_R, WHEEL_W );

	printf( "\n  G.1 CROWN: what the tread looks like where it meets flat ground\n" );
	printf( "    a curvature that doubles when the probe step halves is NOT a curvature -\n" );
	printf( "    it is a KINK, i.e. a flat band or a hull facet. Flagged explicitly.\n" );
	printf( "%-18s %10s %16s %16s\n", "profile", "ride_h_m", "crown", "circumference" );
	for ( int k = 0; k < PF_COUNT; ++k )
	{
		double u[3] = { 1.0, 0.0, 0.0 };	 // outward normal at the contact
		double tLat[3] = { 0.0, 1.0, 0.0 };	 // across the tread (axial)
		double tRoll[3] = { 0.0, 0.0, 1.0 }; // around the circumference
		double h = ProfileSupport( (ProfileKind)k, u[0], u[1], u[2] );
		char lat[24], roll[24];
		DescribeCurvature( (ProfileKind)k, u, tLat, lat, sizeof( lat ) );
		DescribeCurvature( (ProfileKind)k, u, tRoll, roll, sizeof( roll ) );
		printf( "%-18s %10.4f %16s %16s\n", s_pfName[k], h, lat, roll );
	}
	printf( "    crown: real car tire rho 0.5-1.5 m ; motorcycle 0.06-0.12 m ; slick = FLAT\n" );
	printf( "    circumference: must be rho = %.4f m. FLAT/CORNER here means a faceted wheel.\n", WHEEL_R );

	printf( "\n  G.2 CAMBER SWEEP: where the contact point sits as the wheel leans over\n" );
	printf( "    y_contact = mm from the wheel centreplane. 0 = crown, %.0f = outer edge.\n", 500.0f * WHEEL_W );
	printf( "%-18s", "profile\\camber" );
	static const double cam[] = { 0.0, 2.0, 5.0, 10.0, 20.0, 45.0, 80.0, 90.0 };
	const int nCam = (int)( sizeof( cam ) / sizeof( cam[0] ) );
	for ( int c = 0; c < nCam; ++c )
		printf( " %7.0fdeg", cam[c] );
	printf( "\n" );
	for ( int k = 0; k < PF_COUNT; ++k )
	{
		printf( "%-18s", s_pfName[k] );
		for ( int c = 0; c < nCam; ++c )
		{
			double g = cam[c] * PI / 180.0;
			// ground normal in the wheel frame; at 90 deg the wheel lies flat
			double n[3] = { cos( g ), sin( g ), 0.0 };
			double p[3];
			ProfileSupportPoint( (ProfileKind)k, n[0], n[1], n[2], p );
			printf( " %10.1f", 1000.0 * p[1] );
		}
		printf( "   y_contact_mm\n" );

		printf( "%-18s", "" );
		for ( int c = 0; c < nCam; ++c )
		{
			double g = cam[c] * PI / 180.0;
			double n[3] = { cos( g ), sin( g ), 0.0 };
			printf( " %10.4f", ProfileSupport( (ProfileKind)k, n[0], n[1], n[2] ) );
		}
		printf( "   ride_height_m\n" );
	}
	printf( "    sphere: y_contact stays 0 and ride height stays constant at every camber -\n" );
	printf( "            geometrically the wheel never knows it is leaning. That is the isotropy problem.\n" );

	printf( "\n  G.3 CONTACT POINT JUMP: how far the contact slides for 1 deg of extra camber\n" );
	printf( "%-18s %14s %14s %14s\n", "profile", "0->1deg_mm", "4->5deg_mm", "19->20deg_mm" );
	for ( int k = 0; k < PF_COUNT; ++k )
	{
		double res[3];
		static const double base[] = { 0.0, 4.0, 19.0 };
		for ( int b = 0; b < 3; ++b )
		{
			double g0 = base[b] * PI / 180.0, g1 = ( base[b] + 1.0 ) * PI / 180.0;
			double n0[3] = { cos( g0 ), sin( g0 ), 0.0 };
			double n1[3] = { cos( g1 ), sin( g1 ), 0.0 };
			double p0[3], p1[3];
			ProfileSupportPoint( (ProfileKind)k, n0[0], n0[1], n0[2], p0 );
			ProfileSupportPoint( (ProfileKind)k, n1[0], n1[1], n1[2], p1 );
			res[b] = 1000.0 * fabs( p1[1] - p0[1] );
		}
		printf( "%-18s %14.1f %14.1f %14.1f\n", s_pfName[k], res[0], res[1], res[2] );
	}
	printf( "    a big number here is a warm-start hazard: the solver loses the contact identity\n" );
	printf( "    it was reusing. A flat crown jumps hard at 0 deg; a smooth crown slides continuously.\n" );

	printf( "\n  G.4 SIDEWALL: does the shape react to lying on its side at all\n" );
	printf( "%-18s %14s %16s\n", "profile", "half_width_m", "side_shape" );
	for ( int k = 0; k < PF_COUNT; ++k )
	{
		double u[3] = { 0.0, 1.0, 0.0 };
		double t[3] = { 1.0, 0.0, 0.0 };
		char s[24];
		DescribeCurvature( (ProfileKind)k, u, t, s, sizeof( s ) );
		printf( "%-18s %14.4f %16s\n", s_pfName[k], ProfileSupport( (ProfileKind)k, 0, 1, 0 ), s );
	}
	printf( "    half_width must be %.4f m. The sphere reports %.4f m - that is the %.0f mm bulge\n",
			0.5f * WHEEL_W, WHEEL_R, 1000.0f * ( WHEEL_R - 0.5f * WHEEL_W ) );
	printf( "    Jozz feels on every rock, on every side of every wheel.\n" );
}

// ---------------------------------------------------------------- main

// ---------------------------------------------------------------- Q2A
// Eksperyment przy utrzymywanej predkosci. Kontrakt: Q2A_CONSTANT_SPEED_BOX_CONTRACT.md.
// KAZDA liczba ponizej jest zapisana w kontrakcie PRZED pierwszym przebiegiem.

// Nastawy regulatora, obciazenie, tarcie i grawitacja NALEZA DO RIGU - tutaj
// tylko aliasy, zeby jedna zmiana nie musiala trafic w dwa pliki.
#define Q2A_TARGET_V JOZZ_RIG_TARGET_V
#define Q2A_KP JOZZ_RIG_KP
#define Q2A_KI JOZZ_RIG_KI
#define Q2A_FMAX JOZZ_RIG_FMAX
#define Q2A_LOAD_N JOZZ_RIG_LOAD_N
#define Q2A_GRAVITY JOZZ_RIG_GRAVITY
#define Q2A_FRICTION JOZZ_RIG_FRICTION

// Kryteria kwalifikacji i okna naleza do PROTOKOLU, nie do rigu - rig nie wie,
// co znaczy "zakwalifikowany".
#define Q2A_STAB_TOL 0.05
#define Q2A_STAB_STEPS 60
#define Q2A_TIMEOUT_STEPS 900
#define Q2A_DISTANCE_M 50.0
#define Q2A_MEAS_CAP 12000 // bezpiecznik, nie kryterium

// Budowa otoczki, swiat, cialo, material i regulator zyja w jozz_wheel_rig.c.
// Tutaj zostaje wylacznie protokol eksperymentu.

typedef struct
{
	int qualified;
	int qualSteps;
	double qualTime, qualDistance;
	int measSteps, satSteps;
	double duration, distance, pathLength, revolutions;
	double errSumAbs, errSumSq, errMax;
	// Diagnostyka fazy kwalifikacji - bez niej "failed_to_qualify" jest gola
	// asercja, a nie wynikiem. Nie jest kryterium, tylko opisem tego, co bylo.
	double qErrSumAbs, qErrSumSq, qErrMax;
	int qBestConsec;
	double wDriveSigned, wDrivePos, wDriveNeg, wDriveAbs, wDownforce, wGravity;
	double keT0, keR0, peG0, keT1, keR1, peG1;
	// Kontrola spojnosci calkowania w PIONIE: suma v_y*dt kontra faktyczne delta y.
	// Gdy sie rozjezdzaja, pionowe czlony bilansu pracy sa bez wartosci - i to ma
	// byc WIDOCZNE w danych, nie ukryte w residualu.
	double vyIntegral, deltaY;
} Q2AResult;

static Q2AResult RunQ2A( EnvMode m, Budgets bud, FILE* samples, const char* variant, int rep, int* outOk )
{
	Q2AResult r;
	memset( &r, 0, sizeof( r ) );
	*outOk = 0;

	if ( m != ENV_SPHERE && m != ENV_PRISM_MAX )
		return r; // Q2A obsluguje wylacznie kontrole i JEDEN wariant fasetowany
	JozzRigVariant rv = ( m == ENV_SPHERE ) ? JOZZ_RIG_SPHERE : JOZZ_RIG_PRISM_MAX;

	// Swiat, grunt, cialo, material, masa i regulator: jozz_wheel_rig.c. Ponizej
	// zostaje tylko PROTOKOL - kryteria kwalifikacji, okno i zapis.
	JozzRig rig;
	if ( JozzRig_Create( &rig, rv, bud.maxSides ) == 0 )
		return r;

	int consec = 0, step = 0;
	JozzRigSample s;

	// --- kwalifikacja: staly warm-up NIE jest bramka -----------------------
	for ( step = 0; step < Q2A_TIMEOUT_STEPS; ++step )
	{
		JozzRig_Step( &rig, &s );

		double e2 = s.error;
		r.qErrSumAbs += fabs( e2 );
		r.qErrSumSq += e2 * e2;
		if ( fabs( e2 ) > r.qErrMax )
			r.qErrMax = fabs( e2 );
		if ( fabs( e2 ) <= Q2A_STAB_TOL )
			++consec;
		else
			consec = 0;
		if ( consec > r.qBestConsec )
			r.qBestConsec = consec;

		if ( samples )
		{
			// Kolumna cumulative_revolutions jest w fazie kwalifikacji stale 0 -
			// tak bylo w pierwszym przebiegu Q2A i tak zostaje, zeby refaktor nie
			// zmienil zapisanych danych. Poprawka jest w backlogu, nie tutaj.
			if ( fprintf( samples, "%s,%d,qualification,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%.9g,%.9g,%.9g,"
								   "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
						  variant, rep, s.step, s.time, s.distance, s.targetSpeed, s.speed, e2, s.force, s.power,
						  s.saturated, s.omegaSpin, s.refRimSpeed, s.refSlipSpeed, 0.0, s.posY, s.velY, s.keTrans,
						  s.keRot, s.keTotal, s.peGravity ) < 0 )
				g_teleFailed = 1;
		}

		if ( consec >= Q2A_STAB_STEPS )
		{
			r.qualified = 1;
			break;
		}
	}
	r.qualSteps = ( r.qualified ? step + 1 : Q2A_TIMEOUT_STEPS );
	r.qualTime = r.qualSteps * TELE_DT;
	r.qualDistance = JozzRig_Distance( &rig );

	// --- okno pomiarowe: staly dystans ------------------------------------
	if ( r.qualified )
	{
		JozzWheelKin k0 = JozzRig_Kinematics( rig.body );
		b3Pos p0 = b3Body_GetPosition( rig.body );
		r.keT0 = k0.keTrans;
		r.keR0 = k0.keRot;
		r.peG0 = UNSPRUNG_KG * Q2A_GRAVITY * (double)p0.y;
		JozzRig_ResetWork( &rig );

		int ms = 0;
		while ( ms < Q2A_MEAS_CAP )
		{
			JozzRig_Step( &rig, &s );
			++ms;

			double e2 = s.error;
			r.errSumAbs += fabs( e2 );
			r.errSumSq += e2 * e2;
			if ( fabs( e2 ) > r.errMax )
				r.errMax = fabs( e2 );
			r.distance = s.distance;

			if ( samples )
			{
				if ( fprintf( samples,
							  "%s,%d,measurement,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,"
							  "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
							  variant, rep, s.step, s.time, s.distance, s.targetSpeed, s.speed, e2, s.force,
							  s.power, s.saturated, s.omegaSpin, s.refRimSpeed, s.refSlipSpeed, s.revolutions,
							  s.posY, s.velY, s.keTrans, s.keRot, s.keTotal, s.peGravity ) < 0 )
					g_teleFailed = 1;
			}

			if ( r.distance >= Q2A_DISTANCE_M )
				break;
		}
		r.measSteps = ms;
		r.duration = ms * TELE_DT;

		// Bilans pracy i droga naliczaja sie w rigu - stend je tylko odczytuje.
		r.revolutions = rig.absSpin / ( 2.0 * PI );
		r.satSteps = rig.satSteps;
		r.pathLength = rig.pathLength;
		r.wDriveSigned = rig.wDriveSigned;
		r.wDrivePos = rig.wDrivePos;
		r.wDriveNeg = rig.wDriveNeg;
		r.wDriveAbs = rig.wDriveAbs;
		r.wDownforce = rig.wDownforce;
		r.wGravity = rig.wGravity;
		r.vyIntegral = rig.vyIntegral;

		JozzWheelKin k1 = JozzRig_Kinematics( rig.body );
		b3Pos p1 = b3Body_GetPosition( rig.body );
		r.keT1 = k1.keTrans;
		r.keR1 = k1.keRot;
		r.peG1 = UNSPRUNG_KG * Q2A_GRAVITY * (double)p1.y;
		r.deltaY = (double)p1.y - (double)p0.y;
	}

	JozzRig_Destroy( &rig );
	*outOk = 1;
	return r;
}

static FILE* OpenNew( const char* dir, const char* name, char* pathOut, size_t cap )
{
	snprintf( pathOut, cap, "%s\\%s", dir, name );
	FILE* probe = fopen( pathOut, "rb" );
	if ( probe )
	{
		fclose( probe );
		fprintf( stderr, "BLAD: %s juz istnieje - Q2A nie nadpisuje plikow\n", pathOut );
		return NULL;
	}
	FILE* f = fopen( pathOut, "wb" );
	if ( f == NULL )
		fprintf( stderr, "BLAD: nie moge otworzyc %s do zapisu\n", pathOut );
	return f;
}

static const char* Q2A_SAMPLE_HEADER =
	"variant,rep,phase,step,time_s,distance_m,target_speed_m_s,actual_speed_m_s,speed_error_m_s,"
	"drive_force_N,drive_power_W,controller_saturated,"
	"omega_spin_rad_s,reference_rim_speed_m_s,reference_slip_speed_m_s,cumulative_revolutions,"
	"position_y_m,velocity_y_m_s,"
	"kinetic_translation_J,kinetic_rotation_J,kinetic_total_J,potential_gravity_J\n";

static const char* Q2A_SUMMARY_HEADER =
	"variant,rep,stage,status,qualification_steps,qualification_time_s,qualification_distance_m,"
	"qual_err_rms_m_s,qual_err_max_m_s,qual_best_consecutive_steps,"
	"measure_steps,duration_s,distance_m,path_length_m,revolutions,"
	"err_mean_abs_m_s,err_rms_m_s,err_max_m_s,saturated_steps,saturated_fraction,"
	"W_drive_signed_J,W_drive_positive_J,W_drive_negative_J,W_drive_absolute_J,"
	"W_downforce_J,W_gravity_J,"
	"dKE_translation_J,dKE_rotation_J,dKE_total_J,dPE_gravity_J,energy_residual_J,"
	// Kolejnosc MUSI odpowiadac kolejnosci argumentow w WriteQ2ASummaryRow.
	// Pierwsza wersja miala te dwie kolumny wyzej, a wartosci nizej - wynikiem
	// bylo przesuniecie: `vy_integral_m` niosl w rzeczywistosci W_drive_per_m_J.
	"W_drive_per_m_J,W_drive_per_rev_J,vy_integral_m,delta_y_m,"
	"gate_mean_err,gate_rms,gate_saturation\n";

static void WriteQ2ASummaryRow( FILE* f, const char* variant, int rep, const char* stage, const Q2AResult* r )
{
	const char* status = r->qualified ? "qualified" : "failed_to_qualify";
	double n = r->measSteps > 0 ? (double)r->measSteps : 1.0;
	double meanAbs = r->errSumAbs / n;
	double rms = sqrt( r->errSumSq / n );
	double satFrac = (double)r->satSteps / n;
	double dKeT = r->keT1 - r->keT0, dKeR = r->keR1 - r->keR0;
	double dKeTot = dKeT + dKeR, dPe = r->peG1 - r->peG0;
	double residual = r->wDriveSigned + r->wDownforce + r->wGravity - dKeTot;
	double perM = r->distance > 0.0 ? r->wDriveSigned / r->distance : 0.0;
	double perRev = r->revolutions > 0.0 ? r->wDriveSigned / r->revolutions : 0.0;
	double qn = r->qualSteps > 0 ? (double)r->qualSteps : 1.0;
	// Bramki dotycza OKNA POMIAROWEGO. Gdy przebieg sie nie zakwalifikowal, okna
	// nie ma - wtedy "PASS" bylby klamstwem policzonym z zer.
	const char* gMean = r->qualified ? ( meanAbs <= 0.05 ? "PASS" : "FAIL" ) : "n/a";
	const char* gRms = r->qualified ? ( rms <= 0.10 ? "PASS" : "FAIL" ) : "n/a";
	const char* gSat = r->qualified ? ( satFrac <= 0.05 ? "PASS" : "FAIL" ) : "n/a";
	fprintf( f,
			 "%s,%d,%s,%s,%d,%.9g,%.9g,"
			 "%.9g,%.9g,%d,"
			 "%d,%.9g,%.9g,%.9g,%.9g,"
			 "%.9g,%.9g,%.9g,%d,%.9g,"
			 "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,"
			 "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%s,%s\n",
			 variant, rep, stage, status, r->qualSteps, r->qualTime, r->qualDistance,
			 sqrt( r->qErrSumSq / qn ), r->qErrMax, r->qBestConsec, r->measSteps, r->duration, r->distance,
			 r->pathLength, r->revolutions, meanAbs, rms, r->errMax, r->satSteps, satFrac, r->wDriveSigned,
			 r->wDrivePos, r->wDriveNeg, r->wDriveAbs, r->wDownforce, r->wGravity, dKeT, dKeR, dKeTot, dPe,
			 residual, perM, perRev, r->vyIntegral, r->deltaY, gMean, gRms, gSat );
}

// Sciezki Windows zawieraja backslashe, ktore w JSON sa znakiem ucieczki. Bez
// tego manifest byl SKLADNIOWO ZEPSUTY - a manifest, ktorego nie da sie
// sparsowac, nie jest provenance.
static void JsonEscape( const char* in, char* out, size_t cap )
{
	size_t j = 0;
	for ( size_t i = 0; in[i] != '\0' && j + 2 < cap; ++i )
	{
		unsigned char c = (unsigned char)in[i];
		if ( c == '\\' || c == '"' )
		{
			out[j++] = '\\';
			out[j++] = (char)c;
		}
		else if ( c < 0x20 )
			out[j++] = ' ';
		else
			out[j++] = (char)c;
	}
	out[j] = '\0';
}

static int ExperimentQ2A( Budgets bud, const char* outDir, const char* cmdlineRaw )
{
	char cmdline[2048];
	JsonEscape( cmdlineRaw, cmdline, sizeof( cmdline ) );
	char pathM[1024], pathS[1024], pathU[1024];
	FILE* fs = OpenNew( outDir, "q2a_samples.csv", pathS, sizeof( pathS ) );
	if ( fs == NULL )
		return 3;
	FILE* fu = OpenNew( outDir, "q2a_summary.csv", pathU, sizeof( pathU ) );
	if ( fu == NULL )
	{
		fclose( fs );
		return 3;
	}
	FILE* fm = OpenNew( outDir, "q2a_manifest.json", pathM, sizeof( pathM ) );
	if ( fm == NULL )
	{
		fclose( fs );
		fclose( fu );
		return 3;
	}

	fprintf( fs, "%s", Q2A_SAMPLE_HEADER );
	fprintf( fu, "%s", Q2A_SUMMARY_HEADER );

	fprintf( fm,
			 "{\n"
			 "  \"experiment\": \"Q2A\",\n"
			 "  \"contract\": \"tools/jozz_wheel_bench/Q2A_CONSTANT_SPEED_BOX_CONTRACT.md\",\n"
			 "  \"provenance\": {\n"
			 "    \"git_sha\": \"%s\",\n"
			 "    \"git_dirty\": \"%s\",\n"
			 "    \"box3d_lib_stamp\": \"%s\",\n"
			 "    \"source_sha256\": \"%s\",\n"
			 "    \"rig_source_sha256\": \"%s\",\n"
			 "    \"rig_header_sha256\": \"%s\",\n"
			 "    \"box3d_lib_sha256\": \"%s\",\n"
			 "    \"exe_sha256\": \"%s\",\n"
			 "    \"box3d_api\": \"%d.%d.%d\",\n"
			 "    \"compiler_msc_full_ver\": %d,\n"
			 "    \"command_line\": \"%s\"\n"
			 "  },\n"
			 "  \"physics\": {\n"
			 "    \"ground\": \"flat box hull 400x1x60 at y=-1\",\n"
			 "    \"dt_s\": %.17g, \"substeps\": %d,\n"
			 "    \"world_gravity_y\": %.17g,\n"
			 "    \"mass_kg\": %.17g,\n"
			 "    \"inertia_spin_factor\": 0.70, \"inertia_transverse_factor\": 0.55,\n"
			 "    \"wheel_radius_m\": %.17g, \"wheel_width_m\": %.17g,\n"
			 "    \"friction_each_material\": %.17g, \"friction_effective\": %.17g,\n"
			 "    \"rolling_resistance_each_material\": 0.0,\n"
			 "    \"effective_normal_load_N\": %.17g,\n"
			 "    \"external_downforce_N\": %.17g,\n"
			 "    \"gravity_load_N\": %.17g,\n"
			 "    \"continuous_collision\": false,\n"
			 "    \"prism_sides\": %d\n"
			 "  },\n"
			 "  \"controller\": {\n"
			 "    \"type\": \"PI with anti-windup (integral frozen while saturated)\",\n"
			 "    \"target_speed_m_s\": %.17g, \"Kp_N_per_m_s\": %.17g, \"Ki_N_per_m\": %.17g,\n"
			 "    \"force_limit_N\": %.17g,\n"
			 "    \"force_application\": \"ApplyForceToCenter along world +X, no intended torque\",\n"
			 "    \"integration_rule\": \"rectangle on post-step state, dt=1/60\"\n"
			 "  },\n"
			 "  \"qualification\": {\n"
			 "    \"stabilisation_tolerance_m_s\": %.17g,\n"
			 "    \"stabilisation_consecutive_steps\": %d,\n"
			 "    \"timeout_steps\": %d,\n"
			 "    \"measure_window\": \"fixed distance\",\n"
			 "    \"measure_distance_m\": %.17g,\n"
			 "    \"gates\": { \"err_mean_abs_max\": 0.05, \"err_rms_max\": 0.10,"
			 " \"saturated_fraction_max\": 0.05, \"err_max_gated\": false }\n"
			 "  },\n"
			 "  \"variants\": [\"sphere\", \"prism-Nmax\"],\n"
			 "  \"registered_in_raw_manifest\": false\n"
			 "}\n",
			 BENCH_GIT_SHA, BENCH_GIT_DIRTY, BENCH_LIB_STAMP, BENCH_SRC_SHA256, BENCH_RIG_SHA256,
			 BENCH_RIG_HEADER_SHA256, BENCH_LIB_SHA256, g_exeSha, b3GetVersion().major,
			 b3GetVersion().minor, b3GetVersion().revision,
#if defined( _MSC_FULL_VER )
			 _MSC_FULL_VER,
#else
			 0,
#endif
			 cmdline, TELE_DT, TELE_SUBSTEPS, -Q2A_GRAVITY, (double)UNSPRUNG_KG, (double)WHEEL_R,
			 (double)WHEEL_W, (double)Q2A_FRICTION, (double)Q2A_FRICTION, Q2A_LOAD_N,
			 Q2A_LOAD_N - UNSPRUNG_KG * Q2A_GRAVITY, UNSPRUNG_KG * Q2A_GRAVITY, bud.maxSides, Q2A_TARGET_V,
			 Q2A_KP, Q2A_KI, Q2A_FMAX, Q2A_STAB_TOL, Q2A_STAB_STEPS, Q2A_TIMEOUT_STEPS, Q2A_DISTANCE_M );

	printf( "\n=== Q2A) CONSTANT-SPEED BOX - paired experiment ===\n" );
	printf( "    target %.1f m/s, effective load %.0f N (gravity %.0f N + downforce %.0f N),\n", Q2A_TARGET_V,
			Q2A_LOAD_N, UNSPRUNG_KG * Q2A_GRAVITY, Q2A_LOAD_N - UNSPRUNG_KG * Q2A_GRAVITY );
	printf( "    friction %.1f/%.1f, rollingResistance 0/0, window = fixed %.0f m\n", (double)Q2A_FRICTION,
			(double)Q2A_FRICTION, Q2A_DISTANCE_M );
	printf( "%-12s %4s %-18s %8s %8s %9s %9s %10s %12s %12s\n", "variant", "rep", "status", "qual_s", "dist_m",
			"err_mean", "err_max", "sat_%", "W_drive_J", "W_per_m_J" );

	struct
	{
		EnvMode mode;
		const char* name;
	} runs[2] = { { ENV_SPHERE, "sphere" }, { ENV_PRISM_MAX, "prism-Nmax" } };

	int failures = 0;
	// Etap A: kwalifikacja regulatora WYLACZNIE na sphere.
	for ( int stage = 0; stage < 2; ++stage )
	{
		int lo = ( stage == 0 ) ? 0 : 0, hi = ( stage == 0 ) ? 1 : 2;
		int reps = ( stage == 0 ) ? 1 : 3;
		for ( int i = lo; i < hi; ++i )
		{
			for ( int rep = 1; rep <= reps; ++rep )
			{
				int ok = 0;
				Q2AResult r = RunQ2A( runs[i].mode, bud, fs, runs[i].name, stage == 0 ? 0 : rep, &ok );
				if ( !ok )
				{
					printf( "%-12s %4d  (not representable)\n", runs[i].name, rep );
					++failures;
					continue;
				}
				const char* stageName = ( stage == 0 ) ? "A_qualification" : "B_locked";
				WriteQ2ASummaryRow( fu, runs[i].name, stage == 0 ? 0 : rep, stageName, &r );
				double n = r.measSteps > 0 ? (double)r.measSteps : 1.0;
				printf( "%-12s %4d %-18s %8.3f %8.2f %9.4f %9.4f %9.1f%% %12.1f %12.3f\n", runs[i].name,
						stage == 0 ? 0 : rep, r.qualified ? "qualified" : "FAILED_TO_QUALIFY", r.qualTime,
						r.distance, r.errSumAbs / n, r.errMax, 100.0 * r.satSteps / n, r.wDriveSigned,
						r.distance > 0.0 ? r.wDriveSigned / r.distance : 0.0 );
				if ( !r.qualified )
				{
					// Wynik negatywny musi byc SPRAWDZALNY, nie tylko oznajmiony.
					double qn = r.qualSteps > 0 ? (double)r.qualSteps : 1.0;
					printf( "             ^ faza kwalifikacji: przejechal %.1f m, blad RMS %.4f m/s, "
							"max %.4f m/s,\n               najdluzsza seria w tolerancji %d/%d krokow "
							"(tolerancja %.2f m/s)\n",
							r.qualDistance, sqrt( r.qErrSumSq / qn ), r.qErrMax, r.qBestConsec, Q2A_STAB_STEPS,
							Q2A_STAB_TOL );
				}
				fflush( stdout );
			}
		}
		if ( stage == 0 )
			printf( "    ^ etap A: regulator zakwalifikowany na sphere; nastawy ZAMROZONE\n" );
	}

	int bad = 0;
	if ( fflush( fs ) != 0 || ferror( fs ) || fclose( fs ) != 0 )
		bad = 1;
	if ( fflush( fu ) != 0 || ferror( fu ) || fclose( fu ) != 0 )
		bad = 1;
	if ( fflush( fm ) != 0 || ferror( fm ) || fclose( fm ) != 0 )
		bad = 1;
	if ( bad || g_teleFailed )
	{
		fprintf( stderr, "BLAD: zapis Q2A nieudany\n" );
		return 4;
	}
	printf( "Q2A -> %s\n", outDir );
	return failures ? 5 : 0;
}

// --------------------------------------------------- test ekwiwalencji headless/visual
// Zapisuje odcisk stanu po kazdym kroku TA SAMA funkcja, ktorej uzywa okienko
// wizualne. Gdyby kazda strona miala wlasny format, porownanie bajtowe nie
// dowodzilo by niczego o fizyce.
static int RigTrace( const char* path, const char* variantName, int steps, const char* configPath )
{
	JozzRigVariant v = JOZZ_RIG_SPHERE;
	JozzRigConfig cfg = JozzRig_DefaultConfig();

	if ( configPath && variantName )
	{
		// Dwa zrodla wariantu naraz to nie wygoda, tylko pytanie bez odpowiedzi:
		// ktore z nich opisuje przebieg, ktory za chwile trafi do dowodu?
		fprintf( stderr, "BLAD: --rig-config i --rig-trace-variant wykluczaja sie\n" );
		return 2;
	}
	if ( configPath )
	{
		char err[256];
		if ( JozzRig_ConfigReadFile( &cfg, configPath, err, sizeof( err ) ) == 0 )
		{
			fprintf( stderr, "BLAD: %s\n", err );
			return 2;
		}
		v = cfg.variant;
	}
	else
	{
		if ( variantName && strcmp( variantName, "prism-Nmax" ) == 0 )
			v = JOZZ_RIG_PRISM_MAX;
		else if ( variantName && strcmp( variantName, "sphere" ) != 0 )
		{
			fprintf( stderr, "BLAD: nieznany wariant '%s' (sphere|prism-Nmax)\n", variantName );
			return 2;
		}
		cfg.variant = v;
		cfg.prismSides = JozzRig_ProbeMaxPrismSides();
	}
	if ( steps <= 0 )
		steps = 600;

	FILE* probe = fopen( path, "rb" );
	if ( probe )
	{
		fclose( probe );
		fprintf( stderr, "BLAD: %s juz istnieje - trace nie nadpisuje plikow\n", path );
		return 3;
	}
	FILE* f = fopen( path, "wb" );
	if ( f == NULL )
	{
		fprintf( stderr, "BLAD: nie moge otworzyc %s do zapisu\n", path );
		return 3;
	}

	JozzRig rig;
	if ( JozzRig_CreateFromConfig( &rig, &cfg, NULL ) == 0 )
	{
		fclose( f );
		fprintf( stderr, "BLAD: wariant nieprzedstawialny\n" );
		return 5;
	}
	fprintf( f, "# variant=%s prism_sides=%d steps=%d dt=%.17g substeps=%d\n", JozzRig_VariantName( v ),
			 rig.prismSides, steps, cfg.dt, cfg.substeps );
	// Pelna konfiguracja w naglowku - dokladnie ta sama linia co po stronie okna.
	// Bez niej trace z niedomyslnej konstrukcji nie mowi, z czego powstal, a taki
	// plik jest nieodrozniamy od trace'u kontraktowego.
	{
		char cd[512];
		JozzRig_ConfigDigest( &cfg, cd, sizeof( cd ) );
		fprintf( f, "# config %s\n", cd );
	}
	fprintf( f, "%s", JOZZ_RIG_DIGEST_HEADER );
	char line[512];
	for ( int i = 0; i < steps; ++i )
	{
		JozzRig_Step( &rig, NULL );
		JozzRig_DigestLine( &rig, line, sizeof( line ) );
		if ( fputs( line, f ) < 0 )
		{
			JozzRig_Destroy( &rig );
			fclose( f );
			fprintf( stderr, "BLAD: zapis trace nieudany\n" );
			return 4;
		}
	}
	JozzRig_Destroy( &rig );
	int bad = ( fflush( f ) != 0 || ferror( f ) || fclose( f ) != 0 );
	if ( bad )
	{
		fprintf( stderr, "BLAD: zapis trace nieudany\n" );
		return 4;
	}
	printf( "rig trace: %s, %d krokow -> %s\n", JozzRig_VariantName( v ), steps, path );
	return 0;
}

// --------------------------------------------------- test oznaczania zaburzen
// Sprawdza MECHANIZM, ktorego uzywa okno wizualne: czy zaburzenie rzeczywiscie
// zmienia fizyke i czy zostaje oznaczone. Nie sprawdza podpiecia klawiatury -
// to trzeba zobaczyc na ekranie.
static int RigPerturbCheck( void )
{
	JozzRig ctl, per;
	int sides = JozzRig_ProbeMaxPrismSides();
	if ( JozzRig_Create( &ctl, JOZZ_RIG_SPHERE, sides ) == 0 || JozzRig_Create( &per, JOZZ_RIG_SPHERE, sides ) == 0 )
	{
		fprintf( stderr, "BLAD: nie moge zbudowac rigu\n" );
		return 5;
	}
	for ( int i = 0; i < 100; ++i )
	{
		JozzRig_Step( &ctl, NULL );
		JozzRig_Step( &per, NULL );
	}
	char a[512], b[512];
	JozzRig_DigestLine( &ctl, a, sizeof( a ) );
	JozzRig_DigestLine( &per, b, sizeof( b ) );
	int fail = 0;
	if ( strcmp( a, b ) != 0 )
	{
		fprintf( stderr, "BLAD: dwa nietkniete rigi rozjechaly sie przed zaburzeniem\n" );
		fail = 1;
	}
	if ( per.perturbed != 0 )
	{
		fprintf( stderr, "BLAD: flaga zaburzenia podniesiona bez zaburzenia\n" );
		fail = 1;
	}

	b3Vec3 imp = { 0.0f, 0.0f, 260.0f };
	JozzRig_ApplyImpulse( &per, imp, "test: kopniak w bok" );
	if ( per.perturbed != 1 || per.perturbCount != 1 )
	{
		fprintf( stderr, "BLAD: zaburzenie NIE zostalo oznaczone\n" );
		fail = 1;
	}
	for ( int i = 0; i < 100; ++i )
	{
		JozzRig_Step( &ctl, NULL );
		JozzRig_Step( &per, NULL );
	}
	JozzRig_DigestLine( &ctl, a, sizeof( a ) );
	JozzRig_DigestLine( &per, b, sizeof( b ) );
	if ( strcmp( a, b ) == 0 )
	{
		fprintf( stderr, "BLAD: zaburzenie NIE zmienilo fizyki - oznaczenie bez skutku\n" );
		fail = 1;
	}
	// Flaga jest lepka: skutek zaburzenia zostaje w stanie ciala na zawsze.
	JozzRig_ApplyImpulse( &per, imp, "test: drugi kopniak" );
	if ( per.perturbCount != 2 || per.perturbed != 1 )
	{
		fprintf( stderr, "BLAD: licznik zaburzen nie rosnie albo flaga spadla\n" );
		fail = 1;
	}

	printf( "PERTURB CHECK %s\n", fail ? "FAILED" : "OK" );
	if ( !fail )
	{
		printf( "  zaburzenie zmienia fizyke      : tak (odciski stanu sie roznia)\n" );
		printf( "  zaburzenie jest oznaczone      : tak (perturbed=1, licznik=%d)\n", per.perturbCount );
		printf( "  oznaczenie jest lepkie         : tak\n" );
		printf( "  UWAGA: to test mechanizmu, nie podpiecia klawiatury.\n" );
	}
	JozzRig_Destroy( &ctl );
	JozzRig_Destroy( &per );
	return fail ? 1 : 0;
}

// ------------------------------------------------------------- sonda Q3-1
// KOLA_05 par.1.3 TWIERDZI, ze `suspensionHertz` nie jest czestotliwoscia
// resorowania, bo sztywnosc podaza za masa efektywna wiezu. To twierdzenie
// powstalo z CZYTANIA src/wheel_joint.c. Przeczytany kod jest hipoteza
// o zachowaniu, a nie zachowaniem - dlatego ta sonda go MIERZY.
//
// Sonda jest DIAGNOSTYKA, nie rigiem Q3. Buduje minimalny uklad wprost na API
// silnika, zeby wynik nie zalezal od zadnej naszej warstwy. Rig Q3 powstaje PO
// niej i ma prawo wygladac inaczej wlasnie dlatego, ze ona cos pokazala.
//
// Cztery pomiary, kazdy odpowiada na jedno pytanie z rejestru:
//   A  sztywnosc statyczna przy kole        -> U-24
//   B  czestotliwosc wlasna i tlumienie     -> U-24
//   C  wrazliwosc na liczbe podkrokow       -> U-25
//   D  alfa = tau / I, ktore I              -> U-26

#define QC_PROBE_DT ( 1.0 / 60.0 )
#define QC_MOUNT_H 0.6f

typedef struct
{
	b3WorldId world;
	b3BodyId ground;
	b3BodyId chassis;
	b3BodyId wheel;
	b3JointId joint;
} QcProbe;

typedef struct
{
	double sprungKg;
	double unsprungKg;
	float hertz;
	float zeta;
	int substeps;
	double gravity;
	int withGround;
	int chassisAngularFree;
	float maxSpinTorque;
} QcProbeDef;

static b3Quat QcSuspensionFrame( void )
{
	// cx ramki A = os zawieszenia (src/wheel_joint.c:459) -> swiatowe +Y
	// cz ramki A = os obrotu w spoczynku                  -> swiatowe +Z
	b3Matrix3 m;
	m.cx.x = 0.0f, m.cx.y = 1.0f, m.cx.z = 0.0f;
	m.cy.x = -1.0f, m.cy.y = 0.0f, m.cy.z = 0.0f;
	m.cz.x = 0.0f, m.cz.y = 0.0f, m.cz.z = 1.0f;
	return b3MakeQuatFromMatrix( &m );
}

static b3Quat QcWheelFrame( void )
{
	// cz ramki B = os obrotu (src/wheel_joint.c:473). Cialo kola jest obrocone
	// tak, ze jego LOKALNE Y lezy na swiatowym Z - a I_spin siedzi wlasnie na
	// lokalnym Y (jozz_wheel_rig.c:810). Wiec cz ramki B musi byc lokalnym Y.
	// To jest DOKLADNIE to miejsce, w ktorym blad sie nie wywala, tylko po cichu
	// podstawia I_trans zamiast I_spin. Pomiar D sprawdza, czy trafilem.
	b3Matrix3 m;
	m.cx.x = 1.0f, m.cx.y = 0.0f, m.cx.z = 0.0f;
	m.cy.x = 0.0f, m.cy.y = 0.0f, m.cy.z = -1.0f;
	m.cz.x = 0.0f, m.cz.y = 1.0f, m.cz.z = 0.0f;
	return b3MakeQuatFromMatrix( &m );
}

static void QcProbeBuild( QcProbe* p, const QcProbeDef* d )
{
	memset( p, 0, sizeof( *p ) );
	JozzRigConfig base = JozzRig_DefaultConfig();

	b3WorldDef wd = b3DefaultWorldDef();
	wd.workerCount = 1;
	wd.gravity.x = 0.0f;
	wd.gravity.y = -(float)d->gravity;
	wd.gravity.z = 0.0f;
	p->world = b3CreateWorld( &wd );
	b3World_EnableContinuous( p->world, false );

	const float R = base.wheelR;

	if ( d->withGround )
	{
		b3BodyDef gd = b3DefaultBodyDef();
		gd.position.x = 0.0f, gd.position.y = -base.groundHalfY, gd.position.z = 0.0f;
		p->ground = b3CreateBody( p->world, &gd );
		b3ShapeDef gs = b3DefaultShapeDef();
		gs.baseMaterial.friction = base.friction;
		gs.baseMaterial.rollingResistance = 0.0f;
		b3BoxHull box = b3MakeBoxHull( base.groundHalfX, base.groundHalfY, base.groundHalfZ );
		b3CreateHullShape( p->ground, &gs, &box.base );
	}

	// Kolo: ta sama obwiednia, masa i bezwladnosc co Q2A. Sonda nie ma prawa
	// mierzyc innego kola niz to, ktore potem pojedzie w rigu.
	b3BodyDef bw = b3DefaultBodyDef();
	bw.type = b3_dynamicBody;
	bw.position.x = 0.0f, bw.position.y = R, bw.position.z = 0.0f;
	{
		b3Vec3 from, to;
		from.x = 0.0f, from.y = 1.0f, from.z = 0.0f;
		to.x = 0.0f, to.y = 0.0f, to.z = 1.0f;
		bw.rotation = b3ComputeQuatBetweenUnitVectors( from, to );
	}
	bw.enableSleep = false;
	bw.allowFastRotation = true;
	p->wheel = b3CreateBody( p->world, &bw );
	JozzRig_BuildEnvelopeEx( p->wheel, JOZZ_RIG_SPHERE, base.density, 0, base.wheelR, base.wheelW,
							 base.friction );
	JozzRig_FreezeMassEx( p->wheel, (float)d->unsprungKg, base.wheelR, base.inertiaSpinFactor,
						  base.inertiaTransFactor );

	// Nadwozie. KOLEJNOSC TYCH TRZECH KROKOW JEST CZESCIA KONSTRUKCJI, nie stylem:
	//   shape -> blokady -> masa.
	// Pierwsza wersja sondy nie miala shape'u i ustawiala mase PRZED blokadami.
	// b3Body_SetMotionLocks wola b3UpdateBodyMassData, gdy zmienia sie status
	// fixedRotation (src/body.c), a ten przelicza mase Z KSZTALTOW - wiec 150 kg
	// po cichu stawalo sie 0 kg i caly pomiar mierzyl nieruchomy sufit.
	// Shape istnieje po to, zeby cialo bylo samo w sobie spojne; maskBits = 0
	// zamyka mu kolizje na poziomie filtra, wiec nadal o nic nie zawadza.
	b3BodyDef bc = b3DefaultBodyDef();
	bc.type = b3_dynamicBody;
	bc.position.x = 0.0f, bc.position.y = R + QC_MOUNT_H, bc.position.z = 0.0f;
	bc.enableSleep = false;
	p->chassis = b3CreateBody( p->world, &bc );
	{
		b3ShapeDef cs = b3DefaultShapeDef();
		cs.filter.categoryBits = 0;
		cs.filter.maskBits = 0;
		b3BoxHull cb = b3MakeBoxHull( 0.30f, 0.20f, 0.30f );
		b3CreateHullShape( p->chassis, &cs, &cb.base );
	}
	{
		b3MotionLocks lk;
		memset( &lk, 0, sizeof( lk ) );
		lk.linearZ = true;
		lk.angularX = lk.angularY = lk.angularZ = d->chassisAngularFree ? false : true;
		b3Body_SetMotionLocks( p->chassis, lk );
	}
	{
		b3MassData md = b3Body_GetMassData( p->chassis );
		md.mass = (float)d->sprungKg;
		md.center.x = 0.0f, md.center.y = 0.0f, md.center.z = 0.0f;
		float I = (float)d->sprungKg * 0.25f; // dowolna, ale JAWNA i wypisana
		md.inertia.cx.x = I, md.inertia.cx.y = 0.0f, md.inertia.cx.z = 0.0f;
		md.inertia.cy.x = 0.0f, md.inertia.cy.y = I, md.inertia.cy.z = 0.0f;
		md.inertia.cz.x = 0.0f, md.inertia.cz.y = 0.0f, md.inertia.cz.z = I;
		b3Body_SetMassData( p->chassis, md );
	}

	b3WheelJointDef jd = b3DefaultWheelJointDef();
	jd.base.bodyIdA = p->chassis;
	jd.base.bodyIdB = p->wheel;
	jd.base.localFrameA.p.x = 0.0f, jd.base.localFrameA.p.y = -QC_MOUNT_H, jd.base.localFrameA.p.z = 0.0f;
	jd.base.localFrameA.q = QcSuspensionFrame();
	jd.base.localFrameB.p.x = 0.0f, jd.base.localFrameB.p.y = 0.0f, jd.base.localFrameB.p.z = 0.0f;
	jd.base.localFrameB.q = QcWheelFrame();
	jd.base.collideConnected = false;
	jd.enableSuspensionSpring = true;
	jd.suspensionHertz = d->hertz;
	jd.suspensionDampingRatio = d->zeta;
	jd.enableSuspensionLimit = false; // sonda mierzy SPREZYNE, nie zderzaki
	jd.enableSteering = false;
	jd.enableSpinMotor = d->maxSpinTorque > 0.0f;
	jd.maxSpinTorque = d->maxSpinTorque;
	// Silnik predkosciowy z nieosiagalna zadana predkoscia = zrodlo STALEGO
	// momentu. To jedyny sposob, zeby "napedzany momentem" znaczylo moment.
	jd.spinSpeed = d->maxSpinTorque > 0.0f ? 1.0e6f : 0.0f;
	p->joint = b3CreateWheelJoint( p->world, &jd );
}

static void QcProbeDestroy( QcProbe* p )
{
	if ( B3_IS_NON_NULL( p->world ) )
		b3DestroyWorld( p->world );
	memset( p, 0, sizeof( *p ) );
}

// Dodatnia = nadwozie BLIZEJ kola. Znak jest tu ZMIERZONY przez konstrukcje,
// nie zalozony: kontrakt Q3 par.10 wymaga wypisania konwencji.
static double QcTravel( const QcProbe* p )
{
	b3Pos c = b3Body_GetPosition( p->chassis );
	b3Pos w = b3Body_GetPosition( p->wheel );
	return (double)QC_MOUNT_H - ( (double)c.y - (double)w.y );
}

static void QcStep( QcProbe* p, const QcProbeDef* d, int n )
{
	for ( int i = 0; i < n; ++i )
		b3World_Step( p->world, (float)QC_PROBE_DT, d->substeps );
}

typedef struct
{
	double staticTravel; // m, ugiecie w rownowadze
	double kMeasured;	 // N/m, sprung*g / ugiecie
	double fn;			 // Hz, z przejsc przez zero
	double zetaMeasured; // z dekrementu logarytmicznego
	int crossings;
} QcSpringResult;

static QcSpringResult QcMeasureSpring( const QcProbeDef* d )
{
	QcSpringResult r;
	memset( &r, 0, sizeof( r ) );

	QcProbe p;
	QcProbeBuild( &p, d );

	// A) statyka: 20 s na uspokojenie, potem ugiecie wzgledem stanu montazu
	QcStep( &p, d, 1200 );
	r.staticTravel = QcTravel( &p );
	if ( fabs( r.staticTravel ) > 1e-9 )
		r.kMeasured = d->sprungKg * d->gravity / r.staticTravel;

	// B) dynamika: kopniecie PREDKOSCIA (nie teleportem - teleport wnosi wlasny
	// artefakt), potem przejscia przez zero wzgledem nowej rownowagi
	const double eq = r.staticTravel;
	{
		b3Vec3 v = b3Body_GetLinearVelocity( p.chassis );
		v.y = -1.0f;
		b3Body_SetLinearVelocity( p.chassis, v );
	}

	enum
	{
		QC_SAMPLES = 900
	};
	static double sig[QC_SAMPLES];
	for ( int i = 0; i < QC_SAMPLES; ++i )
	{
		QcStep( &p, d, 1 );
		sig[i] = QcTravel( &p ) - eq;
	}

	// czestotliwosc: srednia odleglosc miedzy kolejnymi przejsciami przez zero
	double firstCross = -1.0, lastCross = -1.0;
	int nc = 0;
	for ( int i = 1; i < QC_SAMPLES; ++i )
	{
		if ( ( sig[i - 1] <= 0.0 && sig[i] > 0.0 ) || ( sig[i - 1] >= 0.0 && sig[i] < 0.0 ) )
		{
			double denom = sig[i] - sig[i - 1];
			double frac = fabs( denom ) > 1e-15 ? ( -sig[i - 1] / denom ) : 0.0;
			double t = ( (double)( i - 1 ) + frac ) * QC_PROBE_DT;
			if ( nc == 0 )
				firstCross = t;
			lastCross = t;
			nc += 1;
		}
	}
	r.crossings = nc;
	if ( nc >= 3 )
	{
		double halfPeriod = ( lastCross - firstCross ) / (double)( nc - 1 );
		if ( halfPeriod > 1e-9 )
			r.fn = 1.0 / ( 2.0 * halfPeriod );
	}

	// tlumienie: dekrement logarytmiczny na dwoch pierwszych szczytach tego
	// samego znaku (szukamy ekstremow lokalnych po obu stronach zera)
	{
		double a1 = 0.0, a2 = 0.0;
		int found = 0;
		for ( int i = 1; i < QC_SAMPLES - 1 && found < 2; ++i )
		{
			int isPeak = ( sig[i] > sig[i - 1] && sig[i] >= sig[i + 1] && sig[i] > 0.0 );
			if ( isPeak )
			{
				if ( found == 0 )
					a1 = sig[i];
				else
					a2 = sig[i];
				found += 1;
			}
		}
		if ( found == 2 && a2 > 1e-12 && a1 > a2 )
		{
			double delta = log( a1 / a2 );
			r.zetaMeasured = delta / sqrt( 4.0 * JOZZ_RIG_PI * JOZZ_RIG_PI + delta * delta );
		}
	}

	QcProbeDestroy( &p );
	return r;
}

typedef struct
{
	double alpha;	  // rad/s^2, zmierzone
	double omegaEnd;  // rad/s po oknie
	double axisPurity; // |w_os| / |w|, 1.0 = obrot dokladnie wokol osi kola
} QcTorqueResult;

static QcTorqueResult QcMeasureTorque( const QcProbeDef* d, int steps )
{
	QcTorqueResult r;
	memset( &r, 0, sizeof( r ) );

	QcProbe p;
	QcProbeBuild( &p, d );
	QcStep( &p, d, steps );

	b3Vec3 w = b3Body_GetAngularVelocity( p.wheel );
	double mag = sqrt( (double)w.x * w.x + (double)w.y * w.y + (double)w.z * w.z );
	// os kola w swiecie to +Z (cialo obrocone tak, ze lokalne Y lezy na Z)
	r.omegaEnd = fabs( (double)w.z );
	r.axisPurity = mag > 1e-12 ? r.omegaEnd / mag : 0.0;
	r.alpha = r.omegaEnd / ( (double)steps * QC_PROBE_DT );

	QcProbeDestroy( &p );
	return r;
}

static void QcEmit( FILE* f, const char* fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
	if ( f )
	{
		va_start( ap, fmt );
		vfprintf( f, fmt, ap );
		va_end( ap );
	}
}

static int QcProbeRun( const char* outPath )
{
	FILE* f = outPath ? fopen( outPath, "wb" ) : NULL;
	if ( outPath && f == NULL )
	{
		fprintf( stderr, "BLAD: nie moge zapisac %s\n", outPath );
		return 3;
	}

	JozzRigConfig base = JozzRig_DefaultConfig();
	const double SPRUNG = 150.0;
	const double UNSPRUNG = base.massKg;
	const double G = base.gravity;

	QcEmit( f, "# SONDA Q3-1 - kalibracja wiezu b3WheelJoint\n" );
	QcEmit( f, "# git %s (%s), rig sha256 %s\n", BENCH_GIT_SHA, BENCH_GIT_DIRTY, BENCH_RIG_SHA256 );
	QcEmit( f, "#\n" );
	QcEmit( f, "# masa resorowana %.1f kg, nieresorowana %.1f kg, g = %.2f\n", SPRUNG, UNSPRUNG, G );
	QcEmit( f, "# dt = 1/60 s, kolo = sfera R %.4f m, I_spin %.2f mR^2, I_trans %.2f mR^2\n",
			base.wheelR, base.inertiaSpinFactor, base.inertiaTransFactor );
	QcEmit( f, "# nadwozie: b3MotionLocks linearZ + wszystkie katowe (jesli nie napisano inaczej)\n\n" );

	// ---- 0: czy uklad jest tym, co myslalem, ze zbudowalem --------------
	// Ta sekcja istnieje, bo pierwszy przebieg sondy dal ugiecie ~7000x mniejsze
	// od modelu I zerowa roznice miedzy zablokowanym a swobodnym nadwoziem.
	// Zanim wyciagne wniosek o silniku, sprawdzam WLASNA konstrukcje.
	{
		QcProbeDef d;
		memset( &d, 0, sizeof( d ) );
		d.sprungKg = 150.0;
		d.unsprungKg = UNSPRUNG;
		d.hertz = 1.5f;
		d.zeta = 0.35f;
		d.substeps = base.substeps;
		d.gravity = G;
		d.withGround = 1;
		QcProbe p;
		QcProbeBuild( &p, &d );
		b3MassData mc = b3Body_GetMassData( p.chassis );
		b3MassData mw = b3Body_GetMassData( p.wheel );
		QcEmit( f, "== 0: kontrola konstrukcji ==\n" );
		QcEmit( f, "nadwozie: b3Body_GetMass %.4f kg, massData.mass %.4f kg, I_zz %.4f\n",
				b3Body_GetMass( p.chassis ), mc.mass, mc.inertia.cz.z );
		QcEmit( f, "kolo    : b3Body_GetMass %.4f kg, massData.mass %.4f kg, I_yy %.4f (os obrotu)\n",
				b3Body_GetMass( p.wheel ), mw.mass, mw.inertia.cy.y );
		QcEmit( f, "zadane  : nadwozie %.1f kg, kolo %.1f kg, I_spin %.4f\n", d.sprungKg, d.unsprungKg,
				base.inertiaSpinFactor * UNSPRUNG * (double)base.wheelR * base.wheelR );
		QcEmit( f, "translacja wiezu w chwili montazu: %.9f m\n", QcTravel( &p ) );

		// Sonda, ktora nie sprawdza wlasnej konstrukcji, mierzy cokolwiek.
		// Ten warunek zapalil sie na czerwono przy pierwszym przebiegu i wlasnie
		// dlatego zostaje w kodzie na stale.
		int bad = 0;
		if ( fabs( (double)b3Body_GetMass( p.chassis ) - d.sprungKg ) > 1e-3 )
			bad = 1;
		if ( fabs( (double)b3Body_GetMass( p.wheel ) - d.unsprungKg ) > 1e-3 )
			bad = 1;
		if ( fabs( QcTravel( &p ) ) > 1e-6 )
			bad = 1;
		QcProbeDestroy( &p );
		if ( bad )
		{
			QcEmit( f, "\nSONDA ODMAWIA PRACY: zbudowany uklad nie jest tym, ktory zamowiono.\n" );
			if ( f )
				fclose( f );
			return 1;
		}
		QcEmit( f, "kontrola konstrukcji: OK\n\n" );
	}

	const double mRed = SPRUNG * UNSPRUNG / ( SPRUNG + UNSPRUNG );
	QcEmit( f, "masa zredukowana obu cial : %.4f kg\n", mRed );
	QcEmit( f, "masa resorowana           : %.4f kg\n", SPRUNG );
	QcEmit( f, "iloraz                    : %.4f\n\n", SPRUNG / mRed );

	// ---- A + B: co naprawde ustawia `hertz` -------------------------------
	QcEmit( f, "== A+B: sztywnosc i czestotliwosc wlasna wobec zadanego `hertz` ==\n" );
	QcEmit( f, "# k_pred_sprung = m_sprung*(2pi*f)^2   - gdyby hertz byl czestotliwoscia resoru\n" );
	QcEmit( f, "# k_pred_red    = m_red*(2pi*f)^2      - gdyby szedl za masa efektywna wiezu\n\n" );
	QcEmit( f, "%8s %12s %12s %12s %12s %10s %10s\n", "hertz", "ugiecie[m]", "k_zmierz", "k_pred_spr",
			"k_pred_red", "f_n[Hz]", "zeta_zm" );

	const float hertzSweep[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 6.0f };
	for ( int i = 0; i < (int)( sizeof( hertzSweep ) / sizeof( hertzSweep[0] ) ); ++i )
	{
		QcProbeDef d;
		memset( &d, 0, sizeof( d ) );
		d.sprungKg = SPRUNG;
		d.unsprungKg = UNSPRUNG;
		d.hertz = hertzSweep[i];
		d.zeta = 0.35f;
		d.substeps = base.substeps;
		d.gravity = G;
		d.withGround = 1;
		QcSpringResult r = QcMeasureSpring( &d );

		double omega = 2.0 * JOZZ_RIG_PI * hertzSweep[i];
		QcEmit( f, "%8.2f %12.6f %12.1f %12.1f %12.1f %10.3f %10.3f\n", hertzSweep[i], r.staticTravel,
				r.kMeasured, SPRUNG * omega * omega, mRed * omega * omega, r.fn, r.zetaMeasured );
	}

	// ---- punkt pracy kontraktu Q3 -----------------------------------------
	// Kontrakt zamawia sztywnosc w N/m. Tu jest jedyne miejsce, w ktorym ta
	// liczba zamienia sie na `hertz` - i od razu jest SPRAWDZANA pomiarem.
	{
		const double K_TARGET = 13500.0;
		const double hz = sqrt( K_TARGET / mRed ) / ( 2.0 * JOZZ_RIG_PI );
		QcProbeDef d;
		memset( &d, 0, sizeof( d ) );
		d.sprungKg = SPRUNG;
		d.unsprungKg = UNSPRUNG;
		d.hertz = (float)hz;
		d.zeta = 0.35f;
		d.substeps = base.substeps;
		d.gravity = G;
		d.withGround = 1;
		QcSpringResult r = QcMeasureSpring( &d );
		QcEmit( f, "\n== punkt pracy kontraktu Q3 ==\n" );
		QcEmit( f, "zadana sztywnosc przy kole : %.1f N/m\n", K_TARGET );
		QcEmit( f, "wyliczony hertz            : %.5f Hz\n", hz );
		QcEmit( f, "OSIAGNIETA sztywnosc       : %.1f N/m  (blad %+.3f %%)\n", r.kMeasured,
				100.0 * ( r.kMeasured - K_TARGET ) / K_TARGET );
		QcEmit( f, "ugiecie statyczne          : %.4f m\n", r.staticTravel );
		QcEmit( f, "czestotliwosc resoru f_n   : %.4f Hz  (zadany hertz byl %.4f)\n", r.fn, hz );
		QcEmit( f, "tlumienie zmierzone        : %.4f  (zadane zeta 0.35)\n", r.zetaMeasured );
	}

	// ---- C: wrazliwosc na liczbe podkrokow --------------------------------
	QcEmit( f, "\n== C: ta sama sprezyna, rozna liczba podkrokow (U-25) ==\n" );
	QcEmit( f, "# hertz = 1.50, zeta = 0.35, wszystko inne zamrozone\n\n" );
	QcEmit( f, "%10s %12s %12s %10s\n", "podkroki", "ugiecie[m]", "k_zmierz", "f_n[Hz]" );
	const int subSweep[] = { 1, 2, 4, 8 };
	for ( int i = 0; i < 4; ++i )
	{
		QcProbeDef d;
		memset( &d, 0, sizeof( d ) );
		d.sprungKg = SPRUNG;
		d.unsprungKg = UNSPRUNG;
		d.hertz = 1.5f;
		d.zeta = 0.35f;
		d.substeps = subSweep[i];
		d.gravity = G;
		d.withGround = 1;
		QcSpringResult r = QcMeasureSpring( &d );
		QcEmit( f, "%10d %12.6f %12.1f %10.3f\n", subSweep[i], r.staticTravel, r.kMeasured, r.fn );
	}

	// ---- D: alfa = tau / I, ktore I ---------------------------------------
	QcEmit( f, "\n== D: moment napedowy - ktora bezwladnosc odpowiada (U-26) ==\n" );
	const double R2 = (double)base.wheelR * base.wheelR;
	const double iSpin = base.inertiaSpinFactor * UNSPRUNG * R2;
	const double iTrans = base.inertiaTransFactor * UNSPRUNG * R2;
	const float TAU = 500.0f;
	QcEmit( f, "# tau = %.1f N*m, kolo w powietrzu, g = 0\n", TAU );
	QcEmit( f, "# I_spin  = %.4f kg*m^2 -> alfa oczekiwana %.3f rad/s^2\n", iSpin, TAU / iSpin );
	QcEmit( f, "# I_trans = %.4f kg*m^2 -> alfa oczekiwana %.3f rad/s^2\n\n", iTrans, TAU / iTrans );
	// KONTROLA: moment przylozony WPROST do ciala, z pominieciem wiezu. Bez tego
	// nie da sie odroznic "silnik wiezu jest przeskalowany" od "bezwladnosc kola
	// jest inna, niz mysle" - a to sa dwa zupelnie rozne wnioski.
	{
		QcProbeDef d;
		memset( &d, 0, sizeof( d ) );
		d.sprungKg = SPRUNG;
		d.unsprungKg = UNSPRUNG;
		d.hertz = 1.5f;
		d.zeta = 0.35f;
		d.substeps = base.substeps;
		d.gravity = 0.0;
		d.withGround = 0;
		d.maxSpinTorque = 0.0f; // silnik WYLACZONY
		QcProbe p;
		QcProbeBuild( &p, &d );
		b3Vec3 t;
		t.x = 0.0f, t.y = 0.0f, t.z = TAU;
		for ( int i = 0; i < 60; ++i )
		{
			b3Body_ApplyTorque( p.wheel, t, true );
			b3World_Step( p.world, (float)QC_PROBE_DT, d.substeps );
		}
		b3Vec3 w = b3Body_GetAngularVelocity( p.wheel );
		double alpha = fabs( (double)w.z ) / ( 60.0 * QC_PROBE_DT );
		QcEmit( f, "KONTROLA b3Body_ApplyTorque (bez wiezu): alfa %.3f rad/s^2, iloraz_spin %.4f\n\n",
				alpha, alpha / ( TAU / iSpin ) );
		QcProbeDestroy( &p );
	}

	QcEmit( f, "%22s %8s %6s %12s %12s %12s\n", "nadwozie", "tau", "podkr", "alfa_zmierz", "iloraz_spin",
			"czystosc_osi" );
	const float tauSweep[] = { 100.0f, 500.0f, 1000.0f };
	const int tauSubs[] = { 1, 4, 8 };
	for ( int variant = 0; variant < 2; ++variant )
	{
		for ( int ti = 0; ti < 3; ++ti )
		{
			for ( int si = 0; si < 3; ++si )
			{
				QcProbeDef d;
				memset( &d, 0, sizeof( d ) );
				d.sprungKg = SPRUNG;
				d.unsprungKg = UNSPRUNG;
				d.hertz = 1.5f;
				d.zeta = 0.35f;
				d.substeps = tauSubs[si];
				d.gravity = 0.0;
				d.withGround = 0;
				d.chassisAngularFree = variant;
				d.maxSpinTorque = tauSweep[ti];
				QcTorqueResult r = QcMeasureTorque( &d, 60 );
				QcEmit( f, "%22s %8.0f %6d %12.3f %12.4f %12.4f\n",
						variant ? "obrot SWOBODNY" : "obrot zablokowany", tauSweep[ti], tauSubs[si], r.alpha,
						r.alpha / ( tauSweep[ti] / iSpin ), r.axisPurity );
			}
		}
	}

	QcEmit( f, "\n# Czytanie wyniku: `iloraz_spin` blisko 1.000 znaczy, ze moment\n" );
	QcEmit( f, "# dziala na I_spin - czyli ramka B jest ustawiona poprawnie.\n" );
	QcEmit( f, "# `czystosc_osi` blisko 1.000 znaczy, ze kolo kreci sie wokol swojej osi.\n" );

	if ( f )
		fclose( f );
	return 0;
}

// ================================================================ Q3: QUARTER CAR
//
// Protokol, nie fizyka. Fizyka jest w jozz_qc_rig.c i jest ta sama dla okna.
// Kontrakt: Q3_QUARTER_CAR_CONTRACT.md.

// Protokol pomiaru (rozgrzewka, okno, powtorzenia, lista kandydatow) ZYJE W
// jozz_qc_rig.c, bo uruchamiaja go oba frontendy - stend tutaj i okno przyciskiem
// "zmierz jak stend". Dopoki byl w tym pliku, okno nie mialo do niego dostepu i
// jedyna droga do liczby bench-grade z okna bylo napisanie drugiej kopii.
//
// Stendowi zostaje to, co jest jego: linia polecen, tabela na ekran i CSV.

static int QcParseVariant( const char* name, JozzRigVariant* out )
{
	for ( int i = 0; i < JOZZ_RIG_VARIANT_COUNT; ++i )
	{
		if ( strcmp( name, JozzRig_VariantName( (JozzRigVariant)i ) ) == 0 )
		{
			*out = (JozzRigVariant)i;
			return 1;
		}
	}
	return 0;
}

static int QcParseRoad( const char* name, JozzQcRoad* out )
{
	for ( int i = 0; i < JOZZ_QC_ROAD_COUNT; ++i )
	{
		if ( strcmp( name, JozzQc_RoadName( (JozzQcRoad)i ) ) == 0 )
		{
			*out = (JozzQcRoad)i;
			return 1;
		}
	}
	return 0;
}

static int QcTrace( const char* path, const JozzQcConfig* cfg )
{
	FILE* probe = fopen( path, "rb" );
	if ( probe )
	{
		fclose( probe );
		fprintf( stderr, "BLAD: %s juz istnieje - trace nie nadpisuje plikow\n", path );
		return 3;
	}
	// Probki okna ida do bufora, a plik powstaje PO przebiegu: zapis w petli
	// wchodzil do mierzonego kosztu kroku, wiec `--qc-trace` raportowal cene CPU
	// obwiedni razem z cena fprintf.
	static JozzQcSample trace[JOZZ_QC_WINDOW_STEPS];
	JozzQcRunResult r = JozzQc_MeasureOne( cfg, JOZZ_QC_WARMUP_STEPS, JOZZ_QC_WINDOW_STEPS, trace,
										   JOZZ_QC_WINDOW_STEPS );
	if ( !r.built )
	{
		fprintf( stderr, "BLAD: %s\n", r.err );
		return 5;
	}
	{
		FILE* f = fopen( path, "wb" );
		if ( f == NULL )
		{
			fprintf( stderr, "BLAD: nie moge otworzyc %s do zapisu\n", path );
			return 3;
		}
		char cd[768];
		JozzQc_ConfigDigest( cfg, cd, sizeof( cd ) );
		fprintf( f, "# rig=Q3-quarter-car warmup=%d window=%d shapes=%d ripple_mm=%.4f\n", JOZZ_QC_WARMUP_STEPS,
				 JOZZ_QC_WINDOW_STEPS, r.shapes, 1000.0 * r.ripple );
		fprintf( f, "# config %s\n", cd );
		fprintf( f, "# build %s %s rig=%s qc=%s\n", BENCH_GIT_SHA, BENCH_GIT_DIRTY, BENCH_RIG_SHA256,
				 BENCH_QC_SHA256 );
		fprintf( f, "%s", JOZZ_QC_TRACE_HEADER );
		for ( int i = 0; i < r.traceCount; ++i )
		{
			char line[640];
			JozzQc_TraceLine( &trace[i], line, sizeof( line ) );
			fputs( line, f );
		}
		fclose( f );
	}
	printf( "Q3 trace -> %s\n", path );
	printf( "  kandydat %s N=%d  ksztaltow %d  tetnienie %.3f mm\n", JozzRig_VariantName( cfg->variant ),
			cfg->segments, r.shapes, 1000.0 * r.ripple );
	printf( "  droga %s  napd %s  v_zad %.2f m/s  hertz %.5f (z %.0f N/m)\n", JozzQc_RoadName( cfg->road ),
			JozzQc_DriveName( cfg->drive ), cfg->targetSpeed, JozzQc_Hertz( cfg ), (double)cfg->springNPerM );
	printf( "  moment %.2f Nm  strata %.1f W  a_rms %.3f m/s2  w powietrzu %.1f%%  skok rms %.4f m\n",
			r.win.driveTorqueMean, r.win.lossPower, r.win.sprungAccelRms, 100.0 * r.win.airborneFraction,
			r.win.travelRms );
	if ( r.win.invalid )
		printf( "  PRZEBIEG NIEWAZNY: %s\n", r.win.invalidWhy );
	return 0;
}

// PRZEMIATANIE jednego parametru. Porownanie kandydatow odpowiada na pytanie
// "ktory lepszy". Nie odpowiada na "co ten parametr wlasciwie robi" - a od tego
// zaczyna sie kazdy nowy system kola. Ten sam protokol pomiarowy, N punktow.
//
// Istnieje tez w oknie (zakladka Pomiar). Tutaj jest po to, zeby wynik byl
// zwyklym przebiegiem konsolowym - czyli zeby dalo sie go zarejestrowac jako
// dowod i uruchomic bez czlowieka przy klawiaturze.
typedef enum
{
	QC_SWEEP_SEGMENTS = 0,
	QC_SWEEP_CROWN,
	QC_SWEEP_ROWS,
	QC_SWEEP_DROP,
	QC_SWEEP_SPRING,
	QC_SWEEP_SPEED,
	QC_SWEEP_OBSTACLE,
	QC_SWEEP_STONE_W,
	QC_SWEEP_STONE_Z,
	QC_SWEEP_COUNT
} QcSweepParam;

static const char* s_qcSweepNames[QC_SWEEP_COUNT] = { "segments", "crown",	  "rows",	 "drop",   "spring",
													  "speed",	  "obstacle", "stone_w", "stone_z" };

// Ktore parametry zmieniaja BRYLE kola. Tylko dla nich powtorzony odcisk
// obwiedni jest ostrzezeniem - przy przemiataniu sprezyny czy predkosci ta sama
// bryla jest oczywista i nie ma o czym mowic.
static int QcSweepTouchesShape( QcSweepParam p )
{
	return p == QC_SWEEP_SEGMENTS || p == QC_SWEEP_CROWN || p == QC_SWEEP_ROWS || p == QC_SWEEP_DROP;
}

static int QcParseSweepParam( const char* name, QcSweepParam* out )
{
	for ( int i = 0; i < QC_SWEEP_COUNT; ++i )
		if ( strcmp( name, s_qcSweepNames[i] ) == 0 )
		{
			*out = (QcSweepParam)i;
			return 1;
		}
	return 0;
}

static void QcSweepApply( JozzQcConfig* c, QcSweepParam p, double v, char* tag, size_t tagCap )
{
	switch ( p )
	{
		case QC_SWEEP_SEGMENTS:
			c->segments = (int)( v + 0.5 );
			snprintf( tag, tagCap, "N=%d", c->segments );
			break;
		case QC_SWEEP_CROWN:
			c->crownR = (float)v;
			snprintf( tag, tagCap, "crown=%.4g", v );
			break;
		case QC_SWEEP_ROWS:
			c->crownRows = (int)( v + 0.5 );
			snprintf( tag, tagCap, "rzedow=%d", c->crownRows );
			break;
		case QC_SWEEP_DROP:
			c->crownDrop = (float)v;
			snprintf( tag, tagCap, "zwis=%.4g", v );
			break;
		case QC_SWEEP_SPRING:
			c->springNPerM = (float)v;
			snprintf( tag, tagCap, "k=%.6g", v );
			break;
		case QC_SWEEP_SPEED:
			c->targetSpeed = v;
			c->startSpeed = v;
			snprintf( tag, tagCap, "v=%.4g", v );
			break;
		case QC_SWEEP_STONE_W:
			c->obstacleHalfZ = (float)v;
			snprintf( tag, tagCap, "kam_pol=%.4g", v );
			break;
		case QC_SWEEP_STONE_Z:
			c->obstacleZ = (float)v;
			snprintf( tag, tagCap, "kam_z=%.4g", v );
			break;
		default:
			c->obstacleH = (float)v;
			snprintf( tag, tagCap, "prog=%.4g", v );
			break;
	}
}

static int QcSweep( const char* csvPath, const JozzQcConfig* base, QcSweepParam param, double from, double to,
					int steps )
{
	FILE* probe = fopen( csvPath, "rb" );
	if ( probe )
	{
		fclose( probe );
		fprintf( stderr, "BLAD: %s juz istnieje\n", csvPath );
		return 3;
	}
	if ( steps < 2 )
	{
		fprintf( stderr, "BLAD: przemiatanie potrzebuje co najmniej 2 punktow\n" );
		return 2;
	}
	FILE* f = fopen( csvPath, "wb" );
	if ( f == NULL )
	{
		fprintf( stderr, "BLAD: nie moge otworzyc %s\n", csvPath );
		return 3;
	}

	char baseDigest[768];
	JozzQc_ConfigDigest( base, baseDigest, sizeof( baseDigest ) );
	fprintf( f, "# Q3 quarter car - przemiatanie parametru '%s' od %.17g do %.17g w %d punktach\n",
			 s_qcSweepNames[param], from, to, steps );
	fprintf( f, "# build %s %s rig=%s qc=%s\n", BENCH_GIT_SHA, BENCH_GIT_DIRTY, BENCH_RIG_SHA256, BENCH_QC_SHA256 );
	fprintf( f, "# warmup=%d window=%d repeats=%d\n", JOZZ_QC_WARMUP_STEPS, JOZZ_QC_WINDOW_STEPS, JOZZ_QC_REPEATS );
	fprintf( f, "# baza %s\n", baseDigest );
	// `travel_dyn_rms` stoi OBOK `travel_rms`, a nie zamiast niego: tamta kolumna
	// jest w zarejestrowanych przebiegach i ma dalej znaczyc to samo. Nowa mowi to,
	// co tamta mowic mialaby - ile zawieszenie NAPRAWDE pracuje.
	// `envelope` to odcisk POWIERZCHNI kola. Stoi w tabeli, bo jego brak kosztowal
	// bledny wniosek: przemiatanie po liczbie rzedow przy zwisie 0 przemiatalo TE
	// SAMA bryle i wygladalo jak pomiar ksztaltu.
	fprintf( f, "point,value,segments,shapes,envelope,profile_err_mm,loaded_points,ripple_mm,road,target_v,"
				"drive_torque_nm,torque_spread,loss_power_w,"
				"sprung_accel_rms,accel_spread,airborne_frac,airborne_spread,travel_rms,travel_dyn_rms,churn_pct,"
				"slip_mean,speed_mean,ms_per_step,valid,why\n" );

	printf( "\n=== Q3 PRZEMIATANIE: %s od %.6g do %.6g, %d punktow ===\n", s_qcSweepNames[param], from, to,
			steps );
	printf( "baza: %s N=%d droga %s v_zad %.2f k %.0f N/m zeta %.3f\n", JozzRig_VariantName( base->variant ),
			base->variant == JOZZ_RIG_SPHERE ? 0 : base->segments, JozzQc_RoadName( base->road ),
			base->targetSpeed, (double)base->springNPerM, (double)base->zeta );
	if ( base->obstacleHalfZ > 0.0f || base->obstacleZ != 0.0f )
		printf( "prog WASKI: polowa szerokosci %.4g m, srodek w z %.4g (kolo ma polowe %.4g m)\n",
				(double)base->obstacleHalfZ, (double)base->obstacleZ, 0.5 * (double)base->wheelW );
	printf( "kazdy punkt to %d przebiegi z przesunietym startem; +- to POLOWA ROZRZUTU\n\n", JOZZ_QC_REPEATS );
	printf( "%-14s %9s %6s %6s %9s %8s %16s %8s %15s %13s %8s %7s\n", "punkt", "bryla", "shape", "pkt",
			"profil_mm", "tetn_mm", "moment Nm", "strata_W", "a_rms m/s2", "w powietrzu", "churn%", "ms/krok" );

	uint64_t* sigs = (uint64_t*)calloc( (size_t)steps, sizeof( uint64_t ) );
	for ( int i = 0; i < steps; ++i )
	{
		double t = (double)i / (double)( steps - 1 );
		double v = from + t * ( to - from );
		JozzQcConfig cfg = *base;
		char tag[48];
		QcSweepApply( &cfg, param, v, tag, sizeof( tag ) );

		JozzQcCell c = JozzQc_MeasureCell( &cfg, JOZZ_QC_WARMUP_STEPS, JOZZ_QC_WINDOW_STEPS );
		if ( !c.built )
		{
			// Punkt niebudowalny NIE przerywa przemiatania i NIE jest po cichu
			// pomijany: "tutaj konstrukcja sie konczy" jest wynikiem.
			printf( "%-14s   NIEZBUDOWANY: %s\n", tag, c.err );
			fprintf( f, "%d,%.17g,%d,0,%016llx,,,0,%s,%.17g,,,,,,,,,,,,,,0,%s\n", i, v, cfg.segments,
					 (unsigned long long)c.envelope, JozzQc_RoadName( cfg.road ), cfg.targetSpeed, c.err );
			continue;
		}
		if ( sigs )
			sigs[i] = c.envelope;
		{
			// Powtorzony odcisk przy parametrze KSZTALTU znaczy, ze punkt kosztowal
			// czas i nie zmienil bryly. To ma byc widac w wierszu, a nie w domysle.
			const char* same = "";
			if ( QcSweepTouchesShape( param ) && i > 0 && sigs && sigs[i] == sigs[i - 1] )
				same = "  = TA SAMA BRYLA";
			printf( "%-14s %9llx %6d %6.2f %9.3f %8.3f %9.2f+-%-4.2f %8.1f %8.3f+-%-5.3f %6.1f+-%-4.1f%% %8.1f "
					"%7.3f%s%s\n",
					tag, (unsigned long long)( c.envelope & 0xffffffffull ), c.shapes, c.points,
					1000.0 * c.profileErr, 1000.0 * c.ripple, c.torque, c.torqueSpread, c.loss, c.accel,
					c.accelSpread, 100.0 * c.airborne, 100.0 * c.airborneSpread, c.churn, c.msPerStep, same,
					c.invalid ? "  NIEWAZNY" : "" );
		}
		fprintf( f,
				 "%d,%.17g,%d,%d,%016llx,%.6f,%.17g,%.6f,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
				 "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%s\n",
				 i, v, cfg.segments, c.shapes, (unsigned long long)c.envelope, 1000.0 * c.profileErr, c.points,
				 1000.0 * c.ripple, JozzQc_RoadName( cfg.road ), cfg.targetSpeed, c.torque, c.torqueSpread,
				 c.loss, c.accel, c.accelSpread, c.airborne, c.airborneSpread, c.travelRms, c.travelDyn, c.churn,
				 c.slip, c.speed, c.msPerStep, c.invalid ? 0 : 1, c.why );
		fflush( stdout );
	}
	fclose( f );

	// Ile ROZNYCH bryl naprawde przeszlo przez to przemiatanie. Bez tej linii
	// tabela z pieciu punktow o rosnacym koszcie CPU wyglada jak pomiar ksztaltu
	// nawet wtedy, gdy ksztalt ani drgnal - dokladnie tak powstal bledny wniosek
	// z przemiatania liczby rzedow przy zwisie 0.
	if ( sigs && QcSweepTouchesShape( param ) )
	{
		int distinct = 0;
		for ( int i = 0; i < steps; ++i )
		{
			int seen = 0;
			if ( sigs[i] == 0 )
				continue;
			for ( int j = 0; j < i; ++j )
				if ( sigs[j] == sigs[i] )
					seen = 1;
			distinct += seen ? 0 : 1;
		}
		printf( "\nroznych bryl w tym przemiataniu: %d na %d punktow\n", distinct, steps );
		if ( distinct <= 1 )
			printf( "UWAGA: ten parametr NIE ZMIENIL obwiedni. Rozrzut ponizej jest podloga szumu\n"
					"przyrzadu przy zmianie liczby ksztaltow, a nie skutkiem ksztaltu.\n" );
	}
	free( sigs );
	printf( "\ntabela -> %s\n", csvPath );
	printf( "Czego to NIE mowi: nic o feelu (V3, wylacznie Jozz) i nic o pelnym pojezdzie (Q4).\n" );
	return 0;
}

// Macierz kandydat x droga x predkosc. To jest realny wynik etapu Q3: jedna
// tabela, w ktorej nowy kandydat stoi obok dzisiejszego kola produktu w tych
// samych warunkach, z jawnym znacznikiem wazności kazdego przebiegu.
static int QcCompare( const char* csvPath )
{
	FILE* probe = fopen( csvPath, "rb" );
	if ( probe )
	{
		fclose( probe );
		fprintf( stderr, "BLAD: %s juz istnieje\n", csvPath );
		return 3;
	}
	FILE* f = fopen( csvPath, "wb" );
	if ( f == NULL )
	{
		fprintf( stderr, "BLAD: nie moge otworzyc %s\n", csvPath );
		return 3;
	}

	JozzQcCandidate cand[JOZZ_QC_CANDIDATE_CAP];
	int nc = JozzQc_Candidates( cand, JOZZ_QC_CANDIDATE_CAP );
	// Dwie predkosci, bo to dwa ROZNE rezimy przyrzadu, nie dwa punkty pracy:
	//   13.0  predkosc kontraktowa Q2A; ~2.2 elementu obwiedni na krok, wiec
	//         struktura obwiedni jest PONIZEJ rozdzielczosci czasowej solvera
	//   4.0   ponizej predkosci ciaglego styku dla tego rigu (~4.8 m/s) i ~0.7
	//         elementu na krok, wiec obwiednia jest widzialna
	const double speeds[2] = { 13.0, 4.0 };

	fprintf( f, "# Q3 quarter car - porownanie kandydatow\n" );
	fprintf( f, "# build %s %s rig=%s qc=%s\n", BENCH_GIT_SHA, BENCH_GIT_DIRTY, BENCH_RIG_SHA256,
			 BENCH_QC_SHA256 );
	fprintf( f, "# warmup=%d window=%d repeats=%d\n", JOZZ_QC_WARMUP_STEPS, JOZZ_QC_WINDOW_STEPS,
			 JOZZ_QC_REPEATS );
	fprintf( f, "candidate,segments,shapes,ripple_mm,road,target_v,drive_torque_nm,torque_spread,loss_power_w,"
				"sprung_accel_rms,accel_spread,airborne_frac,airborne_spread,travel_rms,travel_dyn_rms,churn_pct,"
				"slip_mean,speed_mean,ms_per_step,valid,why\n" );

	printf( "\n=== Q3 QUARTER CAR - porownanie kandydatow ===\n" );
	printf( "sprezyna 13500 N/m, zeta 0.35, masa resorowana 150 kg, nieresorowana 44 kg\n" );
	printf( "okno %d krokow po %d krokach rozgrzewki, dt 1/60, 4 podkroki\n", JOZZ_QC_WINDOW_STEPS,
			JOZZ_QC_WARMUP_STEPS );
	printf( "kazda komorka to %d przebiegi z przesunietym punktem startu; +- to POLOWA ROZRZUTU\n\n",
			JOZZ_QC_REPEATS );

	for ( int si = 0; si < 2; ++si )
	{
		for ( int ri = 0; ri < JOZZ_QC_ROAD_COUNT; ++ri )
		{
			printf( "--- droga %s, v_zad %.1f m/s ---\n", JozzQc_RoadName( (JozzQcRoad)ri ), speeds[si] );
			printf( "%-12s %5s %6s %8s %16s %8s %15s %13s %8s %7s\n", "kandydat", "N", "shape", "tetn_mm",
					"moment Nm", "strata_W", "a_rms m/s2", "w powietrzu", "churn%", "ms/krok" );
			for ( int ci = 0; ci < nc; ++ci )
			{
				JozzQcConfig cfg = JozzQc_DefaultConfig();
				cfg.variant = cand[ci].variant;
				cfg.segments = cand[ci].segments;
				cfg.road = (JozzQcRoad)ri;
				cfg.targetSpeed = speeds[si];
				cfg.startSpeed = speeds[si];

				JozzQcCell c = JozzQc_MeasureCell( &cfg, JOZZ_QC_WARMUP_STEPS, JOZZ_QC_WINDOW_STEPS );
				if ( !c.built )
				{
					printf( "%-12s   NIEZBUDOWANY: %s\n", cand[ci].label, c.err );
					fprintf( f, "%s,%d,0,0,%s,%.17g,,,,,,,,,,,,,,0,%s\n", cand[ci].label, cand[ci].segments,
							 JozzQc_RoadName( (JozzQcRoad)ri ), speeds[si], c.err );
					continue;
				}
				printf( "%-12s %5d %6d %8.3f %9.2f+-%-4.2f %8.1f %8.3f+-%-5.3f %6.1f+-%-4.1f%% %8.1f %7.3f%s\n",
						cand[ci].label, cand[ci].segments, c.shapes, 1000.0 * c.ripple, c.torque, c.torqueSpread,
						c.loss, c.accel, c.accelSpread, 100.0 * c.airborne, 100.0 * c.airborneSpread, c.churn,
						c.msPerStep, c.invalid ? "  NIEWAZNY" : "" );
				fprintf( f, "%s,%d,%d,%.6f,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
							"%.17g,%.17g,%.17g,%.17g,%d,%s\n",
						 cand[ci].label, cand[ci].segments, c.shapes, 1000.0 * c.ripple,
						 JozzQc_RoadName( (JozzQcRoad)ri ), speeds[si], c.torque, c.torqueSpread, c.loss,
						 c.accel, c.accelSpread, c.airborne, c.airborneSpread, c.travelRms, c.travelDyn, c.churn,
						 c.slip, c.speed, c.msPerStep, c.invalid ? 0 : 1, c.why );
				fflush( stdout );
			}
			printf( "\n" );
		}
	}
	fclose( f );
	printf( "tabela -> %s\n", csvPath );
	printf( "\nCzego ta tabela NIE mowi: nic o feelu (to V3, wylacznie Jozz) i nic o\n" );
	printf( "pelnym pojezdzie (to Q4). Wynik Q3 nie awansuje na prawo bez Q4.\n" );
	return 0;
}

int main( int argc, char** argv )
{
	setvbuf( stdout, NULL, _IONBF, 0 ); // never hide where a crash happened

	const char* telePath = NULL;
	const char* q2aDir = NULL;
	const char* tracePath = NULL;
	const char* traceVariant = NULL;
	const char* rigConfig = NULL;
	const char* configTemplate = NULL;
	int traceSteps = 600;
	int perturbCheck = 0;
	int configCheck = 0;
	const char* qcProbe = NULL;
	const char* qcTrace = NULL;
	const char* qcCompare = NULL;
	const char* qcConfig = NULL;
	const char* qcTemplate = NULL;
	const char* qcSweep = NULL;
	QcSweepParam qcSweepParam = QC_SWEEP_SEGMENTS;
	double qcSweepFrom = 3.0, qcSweepTo = 42.0;
	int qcSweepSteps = 6;
	JozzQcConfig qcCfg = JozzQc_DefaultConfig();
	int qcCfgTouched = 0;
	char cmdline[1024] = { 0 };
	for ( int i = 0; i < argc; ++i )
	{
		size_t used = strlen( cmdline );
		snprintf( cmdline + used, sizeof( cmdline ) - used, "%s%s", i ? " " : "", argv[i] );
	}
	for ( int i = 1; i < argc; ++i )
	{
		if ( strcmp( argv[i], "--phase-telemetry" ) == 0 && i + 1 < argc )
			telePath = argv[++i];
		else if ( strcmp( argv[i], "--q2a" ) == 0 && i + 1 < argc )
			q2aDir = argv[++i];
		else if ( strcmp( argv[i], "--exe-sha256" ) == 0 && i + 1 < argc )
			g_exeSha = argv[++i];
		else if ( strcmp( argv[i], "--rig-trace" ) == 0 && i + 1 < argc )
			tracePath = argv[++i];
		else if ( strcmp( argv[i], "--rig-trace-variant" ) == 0 && i + 1 < argc )
			traceVariant = argv[++i];
		else if ( strcmp( argv[i], "--rig-trace-steps" ) == 0 && i + 1 < argc )
			traceSteps = atoi( argv[++i] );
		else if ( strcmp( argv[i], "--rig-config" ) == 0 && i + 1 < argc )
			rigConfig = argv[++i];
		else if ( strcmp( argv[i], "--rig-config-template" ) == 0 && i + 1 < argc )
			configTemplate = argv[++i];
		else if ( strcmp( argv[i], "--rig-perturb-check" ) == 0 )
			perturbCheck = 1;
		else if ( strcmp( argv[i], "--rig-config-check" ) == 0 )
			configCheck = 1;
		else if ( strcmp( argv[i], "--qc-probe" ) == 0 && i + 1 < argc )
			qcProbe = argv[++i];
		else if ( strcmp( argv[i], "--qc-trace" ) == 0 && i + 1 < argc )
			qcTrace = argv[++i];
		else if ( strcmp( argv[i], "--qc-compare" ) == 0 && i + 1 < argc )
			qcCompare = argv[++i];
		else if ( strcmp( argv[i], "--qc-variant" ) == 0 && i + 1 < argc )
		{
			if ( QcParseVariant( argv[++i], &qcCfg.variant ) == 0 )
			{
				fprintf( stderr, "BLAD: nieznany wariant '%s' (sphere|prism-Nmax|torus-N)\n", argv[i] );
				return 2;
			}
			qcCfgTouched = 1;
		}
		else if ( strcmp( argv[i], "--qc-road" ) == 0 && i + 1 < argc )
		{
			if ( QcParseRoad( argv[++i], &qcCfg.road ) == 0 )
			{
				fprintf( stderr, "BLAD: nieznana droga '%s' (flat|cleat|comb)\n", argv[i] );
				return 2;
			}
			qcCfgTouched = 1;
		}
		else if ( strcmp( argv[i], "--qc-segments" ) == 0 && i + 1 < argc )
			qcCfg.segments = atoi( argv[++i] ), qcCfgTouched = 1;
		else if ( strcmp( argv[i], "--qc-crown" ) == 0 && i + 1 < argc )
			qcCfg.crownR = (float)atof( argv[++i] ), qcCfgTouched = 1;
		else if ( strcmp( argv[i], "--qc-rows" ) == 0 && i + 1 < argc )
			qcCfg.crownRows = atoi( argv[++i] ), qcCfgTouched = 1;
		else if ( strcmp( argv[i], "--qc-drop" ) == 0 && i + 1 < argc )
			qcCfg.crownDrop = (float)atof( argv[++i] ), qcCfgTouched = 1;
		else if ( strcmp( argv[i], "--qc-obstacle-h" ) == 0 && i + 1 < argc )
			qcCfg.obstacleH = (float)atof( argv[++i] ), qcCfgTouched = 1;
		else if ( strcmp( argv[i], "--qc-obstacle-half-z" ) == 0 && i + 1 < argc )
			qcCfg.obstacleHalfZ = (float)atof( argv[++i] ), qcCfgTouched = 1;
		else if ( strcmp( argv[i], "--qc-obstacle-z" ) == 0 && i + 1 < argc )
			qcCfg.obstacleZ = (float)atof( argv[++i] ), qcCfgTouched = 1;
		else if ( strcmp( argv[i], "--qc-speed" ) == 0 && i + 1 < argc )
		{
			qcCfg.targetSpeed = atof( argv[++i] );
			qcCfg.startSpeed = qcCfg.targetSpeed;
			qcCfgTouched = 1;
		}
		else if ( strcmp( argv[i], "--qc-config" ) == 0 && i + 1 < argc )
			qcConfig = argv[++i];
		else if ( strcmp( argv[i], "--qc-sweep" ) == 0 && i + 1 < argc )
			qcSweep = argv[++i];
		else if ( strcmp( argv[i], "--qc-sweep-param" ) == 0 && i + 1 < argc )
		{
			if ( QcParseSweepParam( argv[++i], &qcSweepParam ) == 0 )
			{
				fprintf( stderr,
						 "BLAD: nieznany parametr '%s' (segments|crown|rows|drop|spring|speed|obstacle|stone_w|stone_z)\n",
						 argv[i] );
				return 2;
			}
		}
		else if ( strcmp( argv[i], "--qc-sweep-from" ) == 0 && i + 1 < argc )
			qcSweepFrom = atof( argv[++i] );
		else if ( strcmp( argv[i], "--qc-sweep-to" ) == 0 && i + 1 < argc )
			qcSweepTo = atof( argv[++i] );
		else if ( strcmp( argv[i], "--qc-sweep-steps" ) == 0 && i + 1 < argc )
			qcSweepSteps = atoi( argv[++i] );
		else if ( strcmp( argv[i], "--qc-config-template" ) == 0 && i + 1 < argc )
			qcTemplate = argv[++i];
		else
		{
			fprintf( stderr,
					 "uzycie: %s [--phase-telemetry <plik.csv>] [--q2a <katalog>] "
					 "[--exe-sha256 <hex>]\n"
					 "       %s --rig-trace <plik.csv> [--rig-trace-variant sphere|prism-Nmax] "
					 "[--rig-trace-steps N] [--rig-config <plik.rig>]\n"
					 "       %s --rig-perturb-check | --rig-config-check | "
					 "--rig-config-template <plik.rig> | --qc-config-template <plik.qc> | "
					 "--qc-probe <plik.txt>\n"
					 "       %s --qc-compare <plik.csv>\n"
					 "       %s --qc-sweep <plik.csv> --qc-sweep-param "
					 "segments|crown|rows|drop|spring|speed|obstacle|stone_w|stone_z --qc-sweep-from A --qc-sweep-to B "
					 "[--qc-sweep-steps N] [+ opcje --qc-* jako baza]\n"
					 "       %s --qc-trace <plik.csv> [--qc-config <plik.qc>] "
					 "[--qc-variant sphere|prism-Nmax|torus-N] "
					 "[--qc-segments N] [--qc-crown m] [--qc-rows M] [--qc-drop m] "
					 "[--qc-road flat|cleat|comb] [--qc-obstacle-h m] "
					 "[--qc-obstacle-half-z m] [--qc-obstacle-z m] [--qc-speed m/s]\n",
					 argv[0], argv[0], argv[0], argv[0], argv[0], argv[0] );
			return 2;
		}
	}
	if ( ( qcTrace || qcCompare || qcSweep ) &&
		 ( telePath || q2aDir || tracePath || perturbCheck || configCheck || qcProbe ) )
	{
		fprintf( stderr, "BLAD: --qc-trace, --qc-compare i --qc-sweep sa osobnymi przebiegami\n" );
		return 2;
	}
	if ( ( qcTrace && qcCompare ) || ( qcTrace && qcSweep ) || ( qcCompare && qcSweep ) )
	{
		fprintf( stderr, "BLAD: --qc-trace, --qc-compare i --qc-sweep wykluczaja sie\n" );
		return 2;
	}
	if ( ( qcCfgTouched || qcConfig ) && qcTrace == NULL && qcSweep == NULL )
	{
		// Ustawienie kandydata bez przebiegu, ktory go uzyje, znaczy, ze ktos
		// mysli, ze wlasnie cos zmierzyl. --qc-compare ma WLASNA liste kandydatow;
		// --qc-sweep bierze te opcje jako BAZE, wiec tam sa na miejscu.
		fprintf( stderr, "BLAD: opcje --qc-* dzialaja tylko razem z --qc-trace albo --qc-sweep\n" );
		return 2;
	}
	if ( qcTemplate )
	{
		// Szablon z KOMPLETEM kluczy i ich wartosciami kontraktowymi. Latwiej
		// skasowac linie, ktorych sie nie zmienia, niz zgadnac nazwe klucza.
		JozzQcConfig c = JozzQc_DefaultConfig();
		if ( JozzQc_ConfigWriteFile( &c, qcTemplate,
									 "szablon: wartosci kontraktowe Q3, skasuj linie ktorych nie zmieniasz" ) == 0 )
		{
			fprintf( stderr, "BLAD: nie moge zapisac %s\n", qcTemplate );
			return 3;
		}
		printf( "szablon konstrukcji narożnika -> %s\n", qcTemplate );
		return 0;
	}
	if ( qcConfig )
	{
		// Plik wygrywa z opcjami --qc-* i celowo jest wczytywany PO nich: plik
		// konstrukcji jest pelna tozsamoscia przebiegu, wiec czesciowe nadpisanie
		// go flaga dawaloby przebieg, ktorego nie opisuje zaden zapis. Ta sama
		// sciezka co polka w oknie - i wlasnie dlatego okno moze oddac przypadek
		// stendowi bez przepisywania czegokolwiek.
		char err[256];
		JozzQcConfig loaded;
		if ( JozzQc_ConfigReadFile( &loaded, qcConfig, err, sizeof( err ) ) == 0 )
		{
			fprintf( stderr, "BLAD --qc-config: %s\n", err );
			return 3;
		}
		qcCfg = loaded;
		printf( "konstrukcja z pliku: %s\n", qcConfig );
	}
	if ( qcSweep )
		return QcSweep( qcSweep, &qcCfg, qcSweepParam, qcSweepFrom, qcSweepTo, qcSweepSteps );
	if ( qcCompare )
		return QcCompare( qcCompare );
	if ( qcTrace )
		return QcTrace( qcTrace, &qcCfg );
	if ( qcProbe && ( telePath || q2aDir || tracePath || perturbCheck || configCheck ) )
	{
		fprintf( stderr, "BLAD: --qc-probe jest osobnym przebiegiem\n" );
		return 2;
	}
	if ( qcProbe )
		return QcProbeRun( qcProbe );
	if ( ( telePath && q2aDir ) || ( tracePath && ( telePath || q2aDir ) ) ||
		 ( perturbCheck && ( telePath || q2aDir || tracePath ) ) ||
		 ( configCheck && ( telePath || q2aDir || tracePath || perturbCheck ) ) )
	{
		fprintf( stderr, "BLAD: --phase-telemetry, --q2a, --rig-trace, --rig-perturb-check i "
						 "--rig-config-check wykluczaja sie (osobne przebiegi)\n" );
		return 2;
	}
	if ( rigConfig && tracePath == NULL )
	{
		fprintf( stderr, "BLAD: --rig-config dziala tylko razem z --rig-trace\n" );
		return 2;
	}
	if ( configTemplate )
	{
		// Szablon z KOMPLETEM kluczy i ich wartosciami kontraktowymi. Punkt
		// wyjscia dla czlowieka i dla agenta: latwiej skasowac linie, ktorych sie
		// nie zmienia, niz zgadnac nazwe klucza, ktorego sie nie widzialo.
		JozzRigConfig c = JozzRig_DefaultConfig();
		c.prismSides = JozzRig_ProbeMaxPrismSides();
		if ( JozzRig_ConfigWriteFile( &c, configTemplate,
									  "szablon: wartosci kontraktowe Q2A, skasuj linie ktorych nie zmieniasz" ) == 0 )
		{
			fprintf( stderr, "BLAD: nie moge zapisac %s\n", configTemplate );
			return 3;
		}
		printf( "szablon konstrukcji -> %s\n", configTemplate );
		return 0;
	}
	if ( configCheck )
	{
		char err[256];
		if ( JozzRig_ConfigSelfTest( err, sizeof( err ) ) == 0 )
		{
			printf( "CONFIG CHECK FAILED (.rig)\n  %s\n", err );
			return 1;
		}
		// Formatow konstrukcji sa DWA i oba musza byc pod ta sama bramka: `.rig`
		// opisuje samo kolo (Q2A), `.qc` caly narożnik (Q3). Osobna bramka dla
		// drugiego formatu znaczylaby, ze da sie zapomniec o jednej z nich.
		if ( JozzQc_ConfigSelfTest( err, sizeof( err ) ) == 0 )
		{
			printf( "CONFIG CHECK FAILED (.qc)\n  %s\n", err );
			return 1;
		}
		printf( "CONFIG CHECK OK  (.rig kolo Q2A + .qc narożnik Q3)\n" );
		printf( "  kazde pole struktury jest w tabeli formatu : tak (rozmiar + mapa pokrycia)\n" );
		printf( "  zapis -> odczyt zachowuje kazde pole       : tak (co do bitu)\n" );
		printf( "  powtorny zapis daje ten sam tekst          : tak\n" );
		printf( "  plik czesciowy domyka sie domyslnymi       : tak\n" );
		printf( "  dluga notatka w komentarzu przechodzi      : tak\n" );
		printf( "  droga przez dysk zachowuje kazde pole      : tak\n" );
		printf( "  zepsuty plik jest odrzucany                : tak (5 rodzajow)\n" );
		printf( "  UWAGA: to test FORMATU. Nie mowi nic o tym, czy zapisana\n" );
		printf( "         konstrukcja jest fizycznie sensowna.\n" );
		return 0;
	}
	if ( perturbCheck )
		return RigPerturbCheck();
	// --rig-trace jest testem ekwiwalencji, nie eksperymentem: nie liczy zadnej
	// sekcji, nie pisze zadnego dowodu, tylko odcisk stanu krok po kroku.
	if ( tracePath )
		return RigTrace( tracePath, traceVariant, traceSteps, rigConfig );
	if ( telePath )
	{
		// Istniejacy plik telemetrii nie moze zniknac po cichu.
		FILE* probe = fopen( telePath, "rb" );
		if ( probe )
		{
			fclose( probe );
			fprintf( stderr, "BLAD: %s juz istnieje - telemetria nie nadpisuje plikow\n", telePath );
			return 3;
		}
		g_tele = fopen( telePath, "wb" );
		if ( g_tele == NULL )
		{
			fprintf( stderr, "BLAD: nie moge otworzyc %s do zapisu\n", telePath );
			return 3;
		}
		if ( fprintf( g_tele, "%s", TELE_HEADER ) < 0 )
			g_teleFailed = 1;
	}
	time_t now = time( NULL );
	char stamp[64];
	strftime( stamp, sizeof( stamp ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );

	printf( "Jozz wheel bench v2\n" );
	printf( "  run       : %s\n", stamp );
	printf( "  box3d api : %d.%d.%d\n", b3GetVersion().major, b3GetVersion().minor, b3GetVersion().revision );
	printf( "  git sha   : %s (working tree: %s)\n", BENCH_GIT_SHA, BENCH_GIT_DIRTY );
	printf( "  box3d.lib : %s\n", BENCH_LIB_STAMP );
#if defined( _MSC_FULL_VER )
	printf( "  compiler  : MSVC %d\n", _MSC_FULL_VER );
#endif
	printf( "  wheel     : R=%.4f m  W=%.4f m  unsprung=%.0f kg  corner load=%.0f N\n", WHEEL_R, WHEEL_W, UNSPRUNG_KG,
			CORNER_LOAD_N );

	ExperimentHullBudget();

	static b3Vec3 pts[4096];
	Budgets bud;
	// Budzet pryzmatu wyznacza rig - inaczej wariant fasetowany moglby miec inne
	// N w stendzie i w okienku wizualnym.
	bud.maxSides = JozzRig_ProbeMaxPrismSides();
	bud.tireRings = 4;
	bud.tireSides = 8;
	for ( int s = 8; s <= 128; ++s )
	{
		int n = MakeTirePoints( pts, 4096, s, bud.tireRings, WHEEL_R, 0.5f * WHEEL_W, 0.12f );
		if ( n == 0 )
			break;
		b3HullData* h = b3CreateHull( pts, n, n );
		if ( h == NULL )
			break;
		bud.tireSides = s;
		b3DestroyHull( h );
	}
	printf( "\n[chosen budgets] prism sides=%d ; tire profile sides=%d rings=%d\n", bud.maxSides, bud.tireSides,
			bud.tireRings );

	// Q2A jest osobnym eksperymentem: gdy jest wlaczony, sekcje C-G nie sa
	// uruchamiane wcale. Dzieki temu ich output nie moze byc przez Q2A dotkniety.
	if ( q2aDir )
	{
		int rc = ExperimentQ2A( bud, q2aDir, cmdline );
		printf( "\ndone\n" );
		return rc;
	}

	ExperimentMassConfound( bud );
	ExperimentProfileLab( bud );
	ExperimentRollQuality( bud );
	ExperimentCompliance( bud );
	ExperimentCost( bud, 0 );
	ExperimentCost( bud, 1 );

	printf( "\ndone\n" );

	if ( g_tele )
	{
		// Blad zapisu telemetrii nie moze zostac zignorowany - artefakt niepelny
		// jest gorszy niz brak artefaktu, bo wyglada tak samo.
		if ( fflush( g_tele ) != 0 || ferror( g_tele ) )
			g_teleFailed = 1;
		if ( fclose( g_tele ) != 0 )
			g_teleFailed = 1;
		g_tele = NULL;
		const int expected = 2 * ENV_COUNT * 3;
		if ( g_teleRecords != expected )
		{
			fprintf( stderr, "BLAD telemetrii: %d rekordow, oczekiwano %d\n", g_teleRecords, expected );
			return 4;
		}
		if ( g_teleFailed )
		{
			fprintf( stderr, "BLAD telemetrii: zapis nieudany albo wartosc nieskonczona\n" );
			return 4;
		}
		printf( "phase telemetry: %d rekordow -> %s\n", g_teleRecords, telePath );
	}
	return 0;
}
