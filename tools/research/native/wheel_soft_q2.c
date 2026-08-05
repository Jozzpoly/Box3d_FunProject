// SPDX-License-Identifier: MIT
// WHEEL-SOFT-03 Q2: deterministic headless quarter-car contact rig.

#include "box3d/box3d.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define JV_Q2_SCHEMA "jv-wheel-soft-q2/v1"
#define JV_Q2_PI 3.14159265358979323846
#define JV_Q2_MAX_STEPS 720
#define JV_Q2_MAX_FEATURES 8

static const double s_dt = 1.0 / 60.0;
static const int s_substeps = 4;
static const double s_gravity = 10.0;
static const double s_worldHertz = 30.0;
static const double s_worldDamping = 10.0;
static const double s_wheelRadius = 0.5141;
static const double s_wheelWidth = 0.4375;
static const double s_cornerRadius = 0.20;
static const double s_wheelMass = 44.0;
static const double s_sprungMass = 150.0;
static const double s_springNPerM = 13500.0;
static const double s_suspensionDamping = 0.35;
static const double s_mountHeight = 0.95;
static const double s_kickImpulse = -25.0;
static const int s_warmupSteps = 360;
static const int s_staticWindowSteps = 120;
static const int s_responseSteps = 360;

typedef struct
{
    const char* caseDir;
    const char* variant;
    double hertzScale;
} Args;

typedef struct
{
    double compression;
    double suspensionTravel;
    double chassisY;
    double chassisVy;
    double chassisAccel;
    double wheelY;
    double wheelVy;
    double normalImpulse;
    double stepMs;
    int loadedPoints;
    int persistedPoints;
    int featureCount;
    uint32_t featureIds[JV_Q2_MAX_FEATURES];
    int finite;
} Sample;

typedef struct
{
    double mean;
    double rms;
    double peakAbs;
    double minValue;
    double maxValue;
} Stats;

static double NowMs( void )
{
#if defined( _WIN32 )
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency( &frequency );
    QueryPerformanceCounter( &counter );
    return 1000.0 * (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;
    if ( timespec_get( &ts, TIME_UTC ) != TIME_UTC )
    {
        return 0.0;
    }
    return 1000.0 * (double)ts.tv_sec + 1.0e-6 * (double)ts.tv_nsec;
#endif
}

static int CompareDouble( const void* a, const void* b )
{
    double av = *(const double*)a;
    double bv = *(const double*)b;
    return ( av > bv ) - ( av < bv );
}

static int CompareU32( const void* a, const void* b )
{
    uint32_t av = *(const uint32_t*)a;
    uint32_t bv = *(const uint32_t*)b;
    return ( av > bv ) - ( av < bv );
}

static Stats ComputeStats( const double* values, int count )
{
    Stats result = { 0 };
    if ( count <= 0 )
    {
        return result;
    }

    result.minValue = values[0];
    result.maxValue = values[0];
    double sum = 0.0;
    double sumSq = 0.0;
    for ( int i = 0; i < count; ++i )
    {
        double value = values[i];
        double absolute = fabs( value );
        sum += value;
        sumSq += value * value;
        if ( absolute > result.peakAbs )
        {
            result.peakAbs = absolute;
        }
        if ( value < result.minValue )
        {
            result.minValue = value;
        }
        if ( value > result.maxValue )
        {
            result.maxValue = value;
        }
    }
    result.mean = sum / (double)count;
    result.rms = sqrt( sumSq / (double)count );
    return result;
}

static int ParseDouble( const char* text, double* out )
{
    char* end = NULL;
    errno = 0;
    double value = strtod( text, &end );
    if ( errno != 0 || end == text || *end != '\0' || !isfinite( value ) )
    {
        return 0;
    }
    *out = value;
    return 1;
}

static int ParseArgs( int argc, char** argv, Args* out )
{
    memset( out, 0, sizeof( *out ) );
    for ( int i = 1; i < argc; ++i )
    {
        if ( strcmp( argv[i], "--case-dir" ) == 0 && i + 1 < argc )
        {
            out->caseDir = argv[++i];
        }
        else if ( strcmp( argv[i], "--variant" ) == 0 && i + 1 < argc )
        {
            out->variant = argv[++i];
        }
        else if ( strcmp( argv[i], "--hertz-scale" ) == 0 && i + 1 < argc )
        {
            if ( !ParseDouble( argv[++i], &out->hertzScale ) )
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    return out->caseDir != NULL && out->variant != NULL && out->hertzScale > 0.0 && out->hertzScale <= 1.0;
}

static void FreezeMass( b3BodyId body, double mass, double spinFactor, double transverseFactor )
{
    b3MassData data = b3Body_GetMassData( body );
    float spin = (float)( spinFactor * mass * s_wheelRadius * s_wheelRadius );
    float transverse = (float)( transverseFactor * mass * s_wheelRadius * s_wheelRadius );
    data.mass = (float)mass;
    data.center = b3Vec3_zero;
    data.inertia.cx = (b3Vec3){ transverse, 0.0f, 0.0f };
    data.inertia.cy = (b3Vec3){ 0.0f, transverse, 0.0f };
    data.inertia.cz = (b3Vec3){ 0.0f, 0.0f, spin };
    b3Body_SetMassData( body, data );
}

static b3Quat SuspensionFrame( void )
{
    b3Matrix3 matrix;
    matrix.cx = (b3Vec3){ 0.0f, 1.0f, 0.0f };
    matrix.cy = (b3Vec3){ -1.0f, 0.0f, 0.0f };
    matrix.cz = (b3Vec3){ 0.0f, 0.0f, 1.0f };
    return b3MakeQuatFromMatrix( &matrix );
}

static double SuspensionTravel( b3BodyId chassis, b3BodyId wheel )
{
    b3Pos c = b3Body_GetPosition( chassis );
    b3Pos w = b3Body_GetPosition( wheel );
    return (double)w.y - ( (double)c.y - s_mountHeight );
}

static int CollectContact( b3BodyId wheel, Sample* sample )
{
    b3ContactData contacts[8];
    int capacity = b3Body_GetContactCapacity( wheel );
    if ( capacity > 8 )
    {
        capacity = 8;
    }
    int count = b3Body_GetContactData( wheel, contacts, capacity );
    sample->featureCount = 0;
    sample->loadedPoints = 0;
    sample->persistedPoints = 0;
    sample->normalImpulse = 0.0;
    for ( int i = 0; i < count; ++i )
    {
        for ( int j = 0; j < contacts[i].manifoldCount; ++j )
        {
            const b3Manifold* manifold = contacts[i].manifolds + j;
            for ( int k = 0; k < manifold->pointCount; ++k )
            {
                const b3ManifoldPoint* point = manifold->points + k;
                if ( point->totalNormalImpulse <= 0.0f )
                {
                    continue;
                }
                sample->loadedPoints += 1;
                sample->normalImpulse += point->totalNormalImpulse;
                sample->persistedPoints += point->persisted ? 1 : 0;
                if ( sample->featureCount < JV_Q2_MAX_FEATURES )
                {
                    sample->featureIds[sample->featureCount++] = point->featureId;
                }
            }
        }
    }
    qsort( sample->featureIds, (size_t)sample->featureCount, sizeof( sample->featureIds[0] ), CompareU32 );
    return sample->loadedPoints > 0;
}

static Sample StepRig( b3WorldId world, b3BodyId chassis, b3BodyId wheel, double* previousChassisVy )
{
    Sample sample;
    memset( &sample, 0, sizeof( sample ) );
    double started = NowMs();
    b3World_Step( world, (float)s_dt, s_substeps );
    sample.stepMs = NowMs() - started;
    b3Pos chassisPosition = b3Body_GetPosition( chassis );
    b3Pos wheelPosition = b3Body_GetPosition( wheel );
    b3Vec3 chassisVelocity = b3Body_GetLinearVelocity( chassis );
    b3Vec3 wheelVelocity = b3Body_GetLinearVelocity( wheel );
    sample.chassisY = chassisPosition.y;
    sample.chassisVy = chassisVelocity.y;
    sample.chassisAccel = ( sample.chassisVy - *previousChassisVy ) / s_dt;
    *previousChassisVy = sample.chassisVy;
    sample.wheelY = wheelPosition.y;
    sample.wheelVy = wheelVelocity.y;
    sample.compression = fmax( 0.0, s_wheelRadius - sample.wheelY );
    sample.suspensionTravel = SuspensionTravel( chassis, wheel );
    CollectContact( wheel, &sample );
    sample.finite = isfinite( sample.chassisY ) && isfinite( sample.chassisVy ) && isfinite( sample.chassisAccel ) &&
                    isfinite( sample.wheelY ) && isfinite( sample.wheelVy ) && isfinite( sample.compression ) &&
                    isfinite( sample.suspensionTravel ) && isfinite( sample.normalImpulse ) && isfinite( sample.stepMs );
    return sample;
}

static int SameFeatures( const Sample* sample, const uint32_t* expected, int expectedCount )
{
    if ( sample->featureCount != expectedCount )
    {
        return 0;
    }
    return memcmp( sample->featureIds, expected, (size_t)expectedCount * sizeof( expected[0] ) ) == 0;
}

static double Percentile( const double* values, int count, double fraction )
{
    if ( count <= 0 )
    {
        return 0.0;
    }
    double copy[JV_Q2_MAX_STEPS];
    if ( count > JV_Q2_MAX_STEPS )
    {
        count = JV_Q2_MAX_STEPS;
    }
    memcpy( copy, values, (size_t)count * sizeof( copy[0] ) );
    qsort( copy, (size_t)count, sizeof( copy[0] ), CompareDouble );
    int index = (int)ceil( fraction * (double)count ) - 1;
    if ( index < 0 )
    {
        index = 0;
    }
    if ( index >= count )
    {
        index = count - 1;
    }
    return copy[index];
}

static double FindSettlingTime( const Sample* samples, int count, double equilibriumY )
{
    double peakDisplacement = 0.0;
    for ( int i = 0; i < count; ++i )
    {
        double displacement = fabs( samples[i].chassisY - equilibriumY );
        if ( displacement > peakDisplacement )
        {
            peakDisplacement = displacement;
        }
    }
    double positionTolerance = fmax( 0.001, 0.05 * peakDisplacement );
    int holdSteps = 30;
    for ( int i = 30; i + holdSteps <= count; ++i )
    {
        int stable = 1;
        for ( int j = i; j < i + holdSteps; ++j )
        {
            if ( fabs( samples[j].chassisY - equilibriumY ) > positionTolerance || fabs( samples[j].chassisVy ) > 0.02 )
            {
                stable = 0;
                break;
            }
        }
        if ( stable )
        {
            return (double)i * s_dt;
        }
    }
    return -1.0;
}

static int WriteTrace( const char* caseDir, const Sample* staticSamples, int staticCount, const Sample* response, int responseCount )
{
    char path[4096];
    if ( snprintf( path, sizeof( path ), "%s/trace.csv", caseDir ) <= 0 )
    {
        return 0;
    }
    FILE* file = fopen( path, "wb" );
    if ( file == NULL )
    {
        return 0;
    }
    fputs( "phase,step,time_s,chassis_y,chassis_vy,chassis_accel,wheel_y,wheel_vy,compression,suspension_travel,normal_impulse,loaded_points,persisted_points,step_ms\n", file );
    for ( int phase = 0; phase < 2; ++phase )
    {
        const Sample* samples = phase == 0 ? staticSamples : response;
        int count = phase == 0 ? staticCount : responseCount;
        const char* name = phase == 0 ? "static" : "response";
        for ( int i = 0; i < count; ++i )
        {
            const Sample* s = samples + i;
            fprintf( file, "%s,%d,%.9g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,%.17g\n",
                     name, i, (double)i * s_dt, s->chassisY, s->chassisVy, s->chassisAccel, s->wheelY, s->wheelVy,
                     s->compression, s->suspensionTravel, s->normalImpulse, s->loadedPoints, s->persistedPoints, s->stepMs );
        }
    }
    return fclose( file ) == 0;
}

static void WriteStatsJson( FILE* file, const char* name, Stats stats )
{
    fprintf( file,
             "    \"%s\": {\"mean\": %.17g, \"rms\": %.17g, \"peak_abs\": %.17g, \"min\": %.17g, \"max\": %.17g}",
             name, stats.mean, stats.rms, stats.peakAbs, stats.minValue, stats.maxValue );
}

static int WriteMetrics( const Args* args, const Sample* staticSamples, int staticCount, const Sample* response, int responseCount,
                         double shapeHertz, double equilibriumY, const uint32_t* referenceFeatures, int referenceCount,
                         int topologyDrift, int gapSteps, int finite, double settlingTime )
{
    char tempPath[4096];
    char finalPath[4096];
    if ( snprintf( tempPath, sizeof( tempPath ), "%s/metrics.json.tmp", args->caseDir ) <= 0 ||
         snprintf( finalPath, sizeof( finalPath ), "%s/metrics.json", args->caseDir ) <= 0 )
    {
        return 0;
    }

    double staticCompression[JV_Q2_MAX_STEPS], staticImpulse[JV_Q2_MAX_STEPS];
    double responseCompression[JV_Q2_MAX_STEPS], responseAccel[JV_Q2_MAX_STEPS], responseTravel[JV_Q2_MAX_STEPS];
    double responseImpulse[JV_Q2_MAX_STEPS], responseStepMs[JV_Q2_MAX_STEPS];
    double persisted = 0.0, loaded = 0.0;
    for ( int i = 0; i < staticCount; ++i )
    {
        staticCompression[i] = staticSamples[i].compression;
        staticImpulse[i] = staticSamples[i].normalImpulse;
        persisted += staticSamples[i].persistedPoints;
        loaded += staticSamples[i].loadedPoints;
    }
    for ( int i = 0; i < responseCount; ++i )
    {
        responseCompression[i] = response[i].compression;
        responseAccel[i] = response[i].chassisAccel;
        responseTravel[i] = response[i].suspensionTravel;
        responseImpulse[i] = response[i].normalImpulse;
        responseStepMs[i] = response[i].stepMs;
        persisted += response[i].persistedPoints;
        loaded += response[i].loadedPoints;
    }

    double substepInvH = (double)s_substeps / s_dt;
    double cap = 0.125 * substepInvH;
    double selectedBaseHertz = fmin( shapeHertz > 0.0 ? shapeHertz : s_worldHertz, cap );
    double staticSolverHertz = 2.0 * selectedBaseHertz;
    double staticSolverDamping = 0.5 * s_worldDamping;

    Stats staticCompressionStats = ComputeStats( staticCompression, staticCount );
    Stats staticImpulseStats = ComputeStats( staticImpulse, staticCount );
    Stats responseCompressionStats = ComputeStats( responseCompression, responseCount );
    Stats responseAccelStats = ComputeStats( responseAccel, responseCount );
    Stats responseTravelStats = ComputeStats( responseTravel, responseCount );
    Stats responseImpulseStats = ComputeStats( responseImpulse, responseCount );
    Stats performanceStats = ComputeStats( responseStepMs, responseCount );

    FILE* file = fopen( tempPath, "wb" );
    if ( file == NULL )
    {
        return 0;
    }
    fprintf( file, "{\n" );
    fprintf( file, "  \"schema\": \"%s\",\n", JV_Q2_SCHEMA );
    fprintf( file, "  \"variant\": \"%s\",\n", args->variant );
    fprintf( file, "  \"wheel_contact_hertz_scale\": %.17g,\n", args->hertzScale );
    fprintf( file, "  \"finite\": %s,\n", finite ? "true" : "false" );
    fprintf( file, "  \"rig\": {\"dt\": %.17g, \"substeps\": %d, \"gravity\": %.17g, \"wheel_mass_kg\": %.17g, \"sprung_mass_kg\": %.17g, \"kick_impulse_ns\": %.17g},\n",
             s_dt, s_substeps, s_gravity, s_wheelMass, s_sprungMass, s_kickImpulse );
    fprintf( file, "  \"softness\": {\"world_hertz\": %.17g, \"world_damping_ratio\": %.17g, \"shape_hertz\": %.17g, \"inherits_world\": %s, \"base_hertz_cap\": %.17g, \"selected_base_hertz\": %.17g, \"static_solver_hertz\": %.17g, \"static_solver_damping_ratio\": %.17g},\n",
             s_worldHertz, s_worldDamping, shapeHertz, shapeHertz == 0.0 ? "true" : "false", cap, selectedBaseHertz,
             staticSolverHertz, staticSolverDamping );
    fprintf( file, "  \"equilibrium\": {\"chassis_y\": %.17g, \"reference_feature_ids\": [", equilibriumY );
    for ( int i = 0; i < referenceCount; ++i )
    {
        fprintf( file, "%s%u", i == 0 ? "" : ", ", referenceFeatures[i] );
    }
    fprintf( file, "]},\n" );
    fprintf( file, "  \"static_window\": {\n" );
    WriteStatsJson( file, "compression_m", staticCompressionStats );
    fprintf( file, ",\n" );
    WriteStatsJson( file, "normal_impulse_ns", staticImpulseStats );
    fprintf( file, "\n  },\n" );
    fprintf( file, "  \"response_window\": {\n" );
    WriteStatsJson( file, "compression_m", responseCompressionStats );
    fprintf( file, ",\n" );
    WriteStatsJson( file, "chassis_acceleration_mps2", responseAccelStats );
    fprintf( file, ",\n" );
    WriteStatsJson( file, "suspension_travel_m", responseTravelStats );
    fprintf( file, ",\n" );
    WriteStatsJson( file, "normal_impulse_ns", responseImpulseStats );
    if ( settlingTime >= 0.0 )
    {
        fprintf( file, ",\n    \"settling_time_s\": %.17g", settlingTime );
    }
    else
    {
        fprintf( file, ",\n    \"settling_time_s\": null" );
    }
    fprintf( file, "\n  },\n" );
    fprintf( file, "  \"manifold\": {\"reference_loaded_points\": %d, \"topology_drift_steps\": %d, \"contact_gap_steps\": %d, \"persisted_point_ratio\": %.17g},\n",
             referenceCount, topologyDrift, gapSteps, loaded > 0.0 ? persisted / loaded : 0.0 );
    fprintf( file, "  \"performance\": {\"step_ms_mean\": %.17g, \"step_ms_p95\": %.17g, \"step_ms_p99\": %.17g, \"step_ms_max\": %.17g},\n",
             performanceStats.mean, Percentile( responseStepMs, responseCount, 0.95 ), Percentile( responseStepMs, responseCount, 0.99 ),
             performanceStats.maxValue );
    fprintf( file, "  \"residual\": {\"chassis_velocity_mps\": %.17g, \"wheel_velocity_mps\": %.17g}\n",
             response[responseCount - 1].chassisVy, response[responseCount - 1].wheelVy );
    fprintf( file, "}\n" );
    if ( fclose( file ) != 0 )
    {
        return 0;
    }
    remove( finalPath );
    return rename( tempPath, finalPath ) == 0;
}

int main( int argc, char** argv )
{
    Args args;
    if ( !ParseArgs( argc, argv, &args ) )
    {
        fprintf( stderr, "usage: jv_wheel_soft_q2 --case-dir DIR --variant ID --hertz-scale (0,1]\n" );
        return 2;
    }

    double shapeHertz = args.hertzScale >= 1.0 - 1.0e-12 ? 0.0 : s_worldHertz * args.hertzScale;
    b3WorldDef worldDef = b3DefaultWorldDef();
    worldDef.workerCount = 1;
    worldDef.gravity = (b3Vec3){ 0.0f, (float)-s_gravity, 0.0f };
    worldDef.contactHertz = (float)s_worldHertz;
    worldDef.contactDampingRatio = (float)s_worldDamping;
    worldDef.contactSpeed = 3.0f;
    b3WorldId world = b3CreateWorld( &worldDef );
    b3World_EnableContinuous( world, false );

    b3BodyDef groundDef = b3DefaultBodyDef();
    groundDef.position = (b3Pos){ 0.0f, -1.0f, 0.0f };
    b3BodyId ground = b3CreateBody( world, &groundDef );
    b3ShapeDef groundShapeDef = b3DefaultShapeDef();
    groundShapeDef.baseMaterial.friction = 1.0f;
    b3BoxHull groundHull = b3MakeBoxHull( 5.0f, 1.0f, 5.0f );
    b3CreateHullShape( ground, &groundShapeDef, &groundHull.base );

    b3BodyDef wheelDef = b3DefaultBodyDef();
    wheelDef.type = b3_dynamicBody;
    wheelDef.position = (b3Pos){ 0.0f, (float)( s_wheelRadius + 0.003 ), 0.0f };
    wheelDef.enableSleep = false;
    wheelDef.allowFastRotation = true;
    b3BodyId wheelBody = b3CreateBody( world, &wheelDef );
    b3ShapeDef wheelShapeDef = b3DefaultShapeDef();
    wheelShapeDef.density = 77.0f;
    wheelShapeDef.baseMaterial.friction = 1.0f;
    wheelShapeDef.contactHertz = (float)shapeHertz;
    wheelShapeDef.contactDampingRatio = 0.0f;
    b3Wheel wheelShape = b3MakeWheel( b3Vec3_zero, (b3Vec3){ 0.0f, 0.0f, 1.0f }, (float)s_wheelRadius,
                                      (float)( 0.5 * s_wheelWidth ), (float)s_cornerRadius );
    b3CreateWheelShape( wheelBody, &wheelShapeDef, &wheelShape );
    b3MotionLocks wheelLocks = { 0 };
    wheelLocks.linearX = true;
    wheelLocks.linearZ = true;
    wheelLocks.angularX = true;
    wheelLocks.angularY = true;
    b3Body_SetMotionLocks( wheelBody, wheelLocks );
    FreezeMass( wheelBody, s_wheelMass, 0.70, 0.55 );

    double sag = s_sprungMass * s_gravity / s_springNPerM;
    b3BodyDef chassisDef = b3DefaultBodyDef();
    chassisDef.type = b3_dynamicBody;
    chassisDef.position = (b3Pos){ 0.0f, (float)( s_wheelRadius + s_mountHeight - sag ), 0.0f };
    chassisDef.enableSleep = false;
    b3BodyId chassis = b3CreateBody( world, &chassisDef );
    b3MotionLocks chassisLocks = { 0 };
    chassisLocks.linearX = true;
    chassisLocks.linearZ = true;
    chassisLocks.angularX = true;
    chassisLocks.angularY = true;
    chassisLocks.angularZ = true;
    b3Body_SetMotionLocks( chassis, chassisLocks );
    b3MassData chassisMass = { 0 };
    chassisMass.mass = (float)s_sprungMass;
    chassisMass.center = b3Vec3_zero;
    chassisMass.inertia.cx.x = chassisMass.inertia.cy.y = chassisMass.inertia.cz.z = (float)( 0.25 * s_sprungMass );
    b3Body_SetMassData( chassis, chassisMass );

    double reducedMass = s_sprungMass * s_wheelMass / ( s_sprungMass + s_wheelMass );
    double suspensionHertz = sqrt( s_springNPerM / reducedMass ) / ( 2.0 * JV_Q2_PI );
    b3WheelJointDef jointDef = b3DefaultWheelJointDef();
    jointDef.base.bodyIdA = chassis;
    jointDef.base.bodyIdB = wheelBody;
    jointDef.base.localFrameA.p = (b3Vec3){ 0.0f, (float)-s_mountHeight, 0.0f };
    jointDef.base.localFrameA.q = SuspensionFrame();
    jointDef.base.localFrameB = b3Transform_identity;
    jointDef.base.collideConnected = false;
    jointDef.enableSuspensionSpring = true;
    jointDef.suspensionHertz = (float)suspensionHertz;
    jointDef.suspensionDampingRatio = (float)s_suspensionDamping;
    jointDef.enableSuspensionLimit = true;
    jointDef.lowerSuspensionLimit = (float)( sag - 0.11 );
    jointDef.upperSuspensionLimit = (float)( sag + 0.09 );
    jointDef.enableSpinMotor = false;
    jointDef.enableSteering = false;
    b3CreateWheelJoint( world, &jointDef );

    Sample staticSamples[JV_Q2_MAX_STEPS];
    Sample response[JV_Q2_MAX_STEPS];
    double previousChassisVy = b3Body_GetLinearVelocity( chassis ).y;
    int finite = 1;
    for ( int i = 0; i < s_warmupSteps; ++i )
    {
        Sample sample = StepRig( world, chassis, wheelBody, &previousChassisVy );
        finite = finite && sample.finite;
        if ( i >= s_warmupSteps - s_staticWindowSteps )
        {
            staticSamples[i - ( s_warmupSteps - s_staticWindowSteps )] = sample;
        }
    }

    uint32_t referenceFeatures[JV_Q2_MAX_FEATURES];
    int referenceCount = 0;
    double equilibriumY = 0.0;
    int equilibriumCount = 0;
    for ( int i = 0; i < s_staticWindowSteps; ++i )
    {
        equilibriumY += staticSamples[i].chassisY;
        equilibriumCount += 1;
        if ( referenceCount == 0 && staticSamples[i].loadedPoints > 0 )
        {
            referenceCount = staticSamples[i].featureCount;
            memcpy( referenceFeatures, staticSamples[i].featureIds, (size_t)referenceCount * sizeof( referenceFeatures[0] ) );
        }
    }
    equilibriumY /= (double)equilibriumCount;
    if ( referenceCount == 0 )
    {
        finite = 0;
    }

    b3Body_ApplyLinearImpulseToCenter( wheelBody, (b3Vec3){ 0.0f, (float)s_kickImpulse, 0.0f }, true );
    int topologyDrift = 0;
    int gapSteps = 0;
    for ( int i = 0; i < s_responseSteps; ++i )
    {
        response[i] = StepRig( world, chassis, wheelBody, &previousChassisVy );
        finite = finite && response[i].finite;
        if ( response[i].loadedPoints == 0 )
        {
            gapSteps += 1;
        }
        else if ( !SameFeatures( response + i, referenceFeatures, referenceCount ) )
        {
            topologyDrift += 1;
        }
    }

    double settlingTime = FindSettlingTime( response, s_responseSteps, equilibriumY );
    int wroteTrace = WriteTrace( args.caseDir, staticSamples, s_staticWindowSteps, response, s_responseSteps );
    int wroteMetrics = WriteMetrics( &args, staticSamples, s_staticWindowSteps, response, s_responseSteps, shapeHertz,
                                     equilibriumY, referenceFeatures, referenceCount, topologyDrift, gapSteps, finite,
                                     settlingTime );
    b3DestroyWorld( world );

    if ( !wroteTrace || !wroteMetrics )
    {
        fprintf( stderr, "failed to write Q2 artifacts\n" );
        return 3;
    }
    if ( !finite || topologyDrift != 0 )
    {
        fprintf( stderr, "invalid Q2 result: finite=%d topology_drift=%d\n", finite, topologyDrift );
        return 4;
    }
    return 0;
}
