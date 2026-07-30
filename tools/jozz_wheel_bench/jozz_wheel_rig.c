// Wspolny rig kola - implementacja. Patrz jozz_wheel_rig.h.
//
// Kazda linia, ktora dotyka swiata, ciala albo regulatora, jest TUTAJ. Ani
// wheel_bench.c, ani sample nie tworza swiata rigu i nie licza sily napedu.

#include "jozz_wheel_rig.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const char* JozzRig_VariantName( JozzRigVariant v )
{
	if ( v == JOZZ_RIG_SPHERE )
		return "sphere";
	if ( v == JOZZ_RIG_PRISM_MAX )
		return "prism-Nmax";
	return "?";
}

// ---------------------------------------------------------------- kinematyka

double JozzRig_Dot3( b3Vec3 a, b3Vec3 b )
{
	return (double)a.x * b.x + (double)a.y * b.y + (double)a.z * b.z;
}

// Os obrotu kola to LOKALNE Y - JozzRig_FreezeMass stawia iSpin na inertia.cy.
// Nie wolno zalozyc swiatowego Z: to prawda tylko w kroku 0, zanim cialo zdazy
// sie obrocic.
b3Vec3 JozzRig_AxleWorld( b3BodyId body )
{
	b3Vec3 localY;
	localY.x = 0.0f;
	localY.y = 1.0f;
	localY.z = 0.0f;
	return b3RotateVector( b3Body_GetRotation( body ), localY );
}

// omega_spin = rzut predkosci katowej na os kola. Przy stanie zadanym
// (os +z, omega_z = -v/R) wychodzi UJEMNE - i tak ma zostac.
double JozzRig_OmegaSpin( b3BodyId body )
{
	b3Vec3 axle = JozzRig_AxleWorld( body );
	double n = sqrt( JozzRig_Dot3( axle, axle ) );
	if ( n < 1e-9 )
		return 0.0;
	return JozzRig_Dot3( b3Body_GetAngularVelocity( body ), axle ) / n;
}

JozzWheelKin JozzRig_Kinematics( b3BodyId body )
{
	JozzWheelKin k;
	memset( &k, 0, sizeof( k ) );

	b3Vec3 v = b3Body_GetLinearVelocity( body );
	b3Vec3 w = b3Body_GetAngularVelocity( body );
	b3Quat q = b3Body_GetRotation( body );
	b3MassData md = b3Body_GetMassData( body );

	b3Vec3 axle = JozzRig_AxleWorld( body );
	double axleLen = sqrt( JozzRig_Dot3( axle, axle ) );
	k.axleUnit.x = 0.0f;
	k.axleUnit.y = 0.0f;
	k.axleUnit.z = 1.0f;
	if ( axleLen > 1e-9 )
	{
		k.axleUnit.x = (float)( axle.x / axleLen );
		k.axleUnit.y = (float)( axle.y / axleLen );
		k.axleUnit.z = (float)( axle.z / axleLen );
	}

	// forward = up x axle. Przy osi rownoleglej do pionu kierunek jazdy przestaje
	// byc okreslony - wtedy swiatowe X i jawna flaga, zamiast cichej bzdury.
	b3Vec3 up;
	up.x = 0.0f;
	up.y = 1.0f;
	up.z = 0.0f;
	b3Vec3 fwd = b3Cross( up, k.axleUnit );
	double fl = sqrt( JozzRig_Dot3( fwd, fwd ) );
	if ( fl < 1e-6 )
	{
		k.forward.x = 1.0f;
		k.forward.y = 0.0f;
		k.forward.z = 0.0f;
		k.degenerate = 1;
	}
	else
	{
		k.forward.x = (float)( fwd.x / fl );
		k.forward.y = (float)( fwd.y / fl );
		k.forward.z = (float)( fwd.z / fl );
	}

	k.omegaSpin = JozzRig_Dot3( w, k.axleUnit );
	b3Vec3 r;
	r.x = -JOZZ_RIG_WHEEL_R * up.x;
	r.y = -JOZZ_RIG_WHEEL_R * up.y;
	r.z = -JOZZ_RIG_WHEEL_R * up.z;
	double rimContribution = JozzRig_Dot3( b3Cross( w, r ), k.forward );
	k.referenceRimSpeed = -rimContribution;
	k.referenceSlipSpeed = JozzRig_Dot3( v, k.forward ) - k.referenceRimSpeed;

	k.keTrans = 0.5 * md.mass * JozzRig_Dot3( v, v );
	b3Vec3 wLocal = b3InvRotateVector( q, w );
	k.keRot = 0.5 * JozzRig_Dot3( wLocal, b3MulMV( md.inertia, wLocal ) );
	return k;
}

// ---------------------------------------------------------------- geometria

int JozzRig_MakePrismPoints( b3Vec3* out, int cap, int sides, float radius, float halfWidth )
{
	if ( 2 * sides > cap )
		return 0;
	for ( int i = 0; i < sides; ++i )
	{
		float a = 2.0f * JOZZ_RIG_PI * (float)i / (float)sides;
		float x = radius * cosf( a );
		float z = radius * sinf( a );
		out[2 * i + 0].x = x;
		out[2 * i + 0].y = -halfWidth;
		out[2 * i + 0].z = z;
		out[2 * i + 1].x = x;
		out[2 * i + 1].y = +halfWidth;
		out[2 * i + 1].z = z;
	}
	return 2 * sides;
}

int JozzRig_ProbeMaxPrismSides( void )
{
	static b3Vec3 pts[4096];
	int best = 8;
	for ( int s = 8; s <= 256; ++s )
	{
		int n = JozzRig_MakePrismPoints( pts, 4096, s, JOZZ_RIG_WHEEL_R, 0.5f * JOZZ_RIG_WHEEL_W );
		if ( n == 0 )
			break;
		b3HullData* h = b3CreateHull( pts, n, n );
		if ( h == NULL )
			break;
		best = s;
		b3DestroyHull( h );
	}
	return best;
}

// Freeze mass so geometry is the only variable. Tire-like ring inertia.
void JozzRig_FreezeMass( b3BodyId body, float kg )
{
	b3MassData md = b3Body_GetMassData( body );
	md.mass = kg;
	md.center.x = 0.0f;
	md.center.y = 0.0f;
	md.center.z = 0.0f;
	float iSpin = 0.70f * kg * JOZZ_RIG_WHEEL_R * JOZZ_RIG_WHEEL_R;
	float iTr = 0.55f * kg * JOZZ_RIG_WHEEL_R * JOZZ_RIG_WHEEL_R;
	md.inertia.cx.x = iTr;
	md.inertia.cx.y = 0.0f;
	md.inertia.cx.z = 0.0f;
	md.inertia.cy.x = 0.0f;
	md.inertia.cy.y = iSpin;
	md.inertia.cy.z = 0.0f;
	md.inertia.cz.x = 0.0f;
	md.inertia.cz.y = 0.0f;
	md.inertia.cz.z = iTr;
	b3Body_SetMassData( body, md );
}

int JozzRig_BuildEnvelope( b3BodyId body, JozzRigVariant v, float density, int prismSides )
{
	b3ShapeDef sd = b3DefaultShapeDef();
	sd.density = density;
	sd.baseMaterial.friction = JOZZ_RIG_FRICTION;
	sd.baseMaterial.rollingResistance = 0.0f; // jawnie zero, nie odziedziczone
	static b3Vec3 pts[4096];

	if ( v == JOZZ_RIG_SPHERE )
	{
		b3Sphere s;
		s.center.x = 0.0f;
		s.center.y = 0.0f;
		s.center.z = 0.0f;
		s.radius = JOZZ_RIG_WHEEL_R;
		b3CreateSphereShape( body, &sd, &s );
		return 1;
	}
	if ( v == JOZZ_RIG_PRISM_MAX )
	{
		int n = JozzRig_MakePrismPoints( pts, 4096, prismSides, JOZZ_RIG_WHEEL_R, 0.5f * JOZZ_RIG_WHEEL_W );
		if ( n == 0 )
			return 0;
		b3HullData* h = b3CreateHull( pts, n, n );
		if ( h == NULL )
			return 0;
		b3CreateHullShape( body, &sd, h );
		b3DestroyHull( h );
		return 1;
	}
	return 0;
}

// ---------------------------------------------------------------- rig

double JozzRig_Downforce( const JozzRig* rig )
{
	return rig->loadN - (double)JOZZ_RIG_UNSPRUNG_KG * JOZZ_RIG_GRAVITY;
}

double JozzRig_Distance( const JozzRig* rig )
{
	return (double)b3Body_GetPosition( rig->body ).x - rig->workStartX;
}

int JozzRig_Create( JozzRig* rig, JozzRigVariant v, int prismSides )
{
	return JozzRig_CreateWithRenderHooks( rig, v, prismSides, NULL );
}

int JozzRig_CreateWithRenderHooks( JozzRig* rig, JozzRigVariant v, int prismSides,
								   const JozzRigRenderHooks* hooks )
{
	memset( rig, 0, sizeof( *rig ) );
	rig->variant = v;
	rig->prismSides = prismSides;
	rig->controllerEnabled = 1;
	rig->targetSpeed = JOZZ_RIG_TARGET_V;
	rig->loadN = JOZZ_RIG_LOAD_N;

	b3WorldDef wd = b3DefaultWorldDef();
	wd.workerCount = 1;
	// Haki renderera to JEDYNE pola, ktore frontend graficzny moze dolozyc.
	if ( hooks )
	{
		wd.createDebugShape = hooks->createDebugShape;
		wd.destroyDebugShape = hooks->destroyDebugShape;
		wd.userDebugShapeContext = hooks->userDebugShapeContext;
	}
	rig->world = b3CreateWorld( &wd );

	b3BodyDef gd = b3DefaultBodyDef();
	gd.position.x = 0.0f;
	gd.position.y = -1.0f;
	gd.position.z = 0.0f;
	rig->ground = b3CreateBody( rig->world, &gd );
	b3ShapeDef gs = b3DefaultShapeDef();
	gs.baseMaterial.friction = JOZZ_RIG_FRICTION;
	gs.baseMaterial.rollingResistance = 0.0f;
	b3BoxHull box = b3MakeBoxHull( 400.0f, 1.0f, 60.0f );
	b3CreateHullShape( rig->ground, &gs, &box.base );

	b3BodyDef bd = b3DefaultBodyDef();
	bd.type = b3_dynamicBody;
	bd.position.x = (float)JOZZ_RIG_START_X;
	bd.position.y = JOZZ_RIG_WHEEL_R + 0.001f;
	bd.position.z = 0.0f;
	{
		b3Vec3 from, to;
		from.x = 0.0f;
		from.y = 1.0f;
		from.z = 0.0f;
		to.x = 0.0f;
		to.y = 0.0f;
		to.z = 1.0f;
		bd.rotation = b3ComputeQuatBetweenUnitVectors( from, to );
	}
	bd.linearVelocity.x = (float)JOZZ_RIG_TARGET_V;
	bd.linearVelocity.y = 0.0f;
	bd.linearVelocity.z = 0.0f;
	bd.angularVelocity.x = 0.0f;
	bd.angularVelocity.y = 0.0f;
	bd.angularVelocity.z = -(float)JOZZ_RIG_TARGET_V / JOZZ_RIG_WHEEL_R;
	bd.enableSleep = false;
	bd.allowFastRotation = true;
	rig->body = b3CreateBody( rig->world, &bd );
	if ( JozzRig_BuildEnvelope( rig->body, v, 77.0f, prismSides ) == 0 )
	{
		b3DestroyWorld( rig->world );
		memset( rig, 0, sizeof( *rig ) );
		return 0;
	}
	JozzRig_FreezeMass( rig->body, JOZZ_RIG_UNSPRUNG_KG );
	b3World_EnableContinuous( rig->world, false );

	rig->startX = (double)b3Body_GetPosition( rig->body ).x;
	rig->workStartX = rig->startX;
	rig->prevPos = b3Body_GetPosition( rig->body );
	return 1;
}

void JozzRig_Destroy( JozzRig* rig )
{
	if ( B3_IS_NON_NULL( rig->world ) )
		b3DestroyWorld( rig->world );
	memset( rig, 0, sizeof( *rig ) );
}

void JozzRig_ResetWork( JozzRig* rig )
{
	rig->wDriveSigned = 0.0;
	rig->wDrivePos = 0.0;
	rig->wDriveNeg = 0.0;
	rig->wDriveAbs = 0.0;
	rig->wDownforce = 0.0;
	rig->wGravity = 0.0;
	rig->vyIntegral = 0.0;
	rig->pathLength = 0.0;
	rig->absSpin = 0.0;
	rig->satSteps = 0;
	rig->workSteps = 0;
	rig->workStartX = (double)b3Body_GetPosition( rig->body ).x;
	rig->prevPos = b3Body_GetPosition( rig->body );
}

void JozzRig_Step( JozzRig* rig, JozzRigSample* out )
{
	const double downN = JozzRig_Downforce( rig );

	// 1) predkosc PRZED krokiem
	b3Vec3 v = b3Body_GetLinearVelocity( rig->body );

	// 2) regulator PI z anti-windup: calka zamarza w saturacji
	double f = 0.0;
	if ( rig->controllerEnabled )
	{
		double err = rig->targetSpeed - (double)v.x;
		double fraw = JOZZ_RIG_KP * err + JOZZ_RIG_KI * rig->integral;
		f = fraw > JOZZ_RIG_FMAX ? JOZZ_RIG_FMAX : ( fraw < -JOZZ_RIG_FMAX ? -JOZZ_RIG_FMAX : fraw );
		if ( fabs( fraw ) <= JOZZ_RIG_FMAX )
			rig->integral += err * JOZZ_RIG_DT;
	}

	// 3) sila: naped wzdluz swiatowego +X, docisk w dol, oba do srodka masy
	b3Vec3 force;
	force.x = (float)f;
	force.y = -(float)downN;
	force.z = 0.0f;
	b3Body_ApplyForceToCenter( rig->body, force, true );

	// 4) DOKLADNIE ten krok, ktory wykonuje stend
	b3World_Step( rig->world, 1.0f / 60.0f, JOZZ_RIG_SUBSTEPS );
	rig->step += 1;
	rig->workSteps += 1;

	// 5) odczyt po kroku; calkowanie regula prostokata na stanie POKROKOWYM
	b3Vec3 v2 = b3Body_GetLinearVelocity( rig->body );
	b3Pos p = b3Body_GetPosition( rig->body );
	JozzWheelKin k = JozzRig_Kinematics( rig->body );

	double pDrive = f * (double)v2.x;
	double pDown = -downN * (double)v2.y;
	double pGrav = -(double)JOZZ_RIG_UNSPRUNG_KG * JOZZ_RIG_GRAVITY * (double)v2.y;
	rig->wDriveSigned += pDrive * JOZZ_RIG_DT;
	if ( pDrive > 0.0 )
		rig->wDrivePos += pDrive * JOZZ_RIG_DT;
	else
		rig->wDriveNeg += pDrive * JOZZ_RIG_DT;
	rig->wDriveAbs += fabs( pDrive ) * JOZZ_RIG_DT;
	rig->wDownforce += pDown * JOZZ_RIG_DT;
	rig->wGravity += pGrav * JOZZ_RIG_DT;
	rig->vyIntegral += (double)v2.y * JOZZ_RIG_DT;

	double dx = (double)p.x - (double)rig->prevPos.x;
	double dy = (double)p.y - (double)rig->prevPos.y;
	double dz = (double)p.z - (double)rig->prevPos.z;
	rig->pathLength += sqrt( dx * dx + dy * dy + dz * dz );
	rig->prevPos = p;
	rig->absSpin += fabs( k.omegaSpin ) * JOZZ_RIG_DT;

	int sat = fabs( f ) >= 0.999 * JOZZ_RIG_FMAX;
	if ( sat )
		rig->satSteps += 1;

	if ( out )
	{
		out->step = rig->workSteps;
		out->time = rig->workSteps * JOZZ_RIG_DT;
		out->distance = (double)p.x - rig->workStartX;
		out->targetSpeed = rig->targetSpeed;
		out->speed = (double)v2.x;
		out->error = rig->targetSpeed - (double)v2.x;
		out->force = f;
		out->power = pDrive;
		out->saturated = sat;
		out->omegaSpin = k.omegaSpin;
		out->refRimSpeed = k.referenceRimSpeed;
		out->refSlipSpeed = k.referenceSlipSpeed;
		out->revolutions = rig->absSpin / ( 2.0 * JOZZ_RIG_PI );
		out->posY = (double)p.y;
		out->velY = (double)v2.y;
		out->keTrans = k.keTrans;
		out->keRot = k.keRot;
		out->keTotal = k.keTrans + k.keRot;
		out->peGravity = (double)JOZZ_RIG_UNSPRUNG_KG * JOZZ_RIG_GRAVITY * (double)p.y;
	}
}

// ---------------------------------------------------------------- zaburzenia

static void MarkPerturbed( JozzRig* rig, const char* what )
{
	rig->perturbed = 1;
	rig->perturbCount += 1;
	snprintf( rig->lastPerturbation, sizeof( rig->lastPerturbation ), "%s", what ? what : "?" );
}

void JozzRig_ApplyImpulse( JozzRig* rig, b3Vec3 impulse, const char* what )
{
	b3Body_ApplyLinearImpulseToCenter( rig->body, impulse, true );
	MarkPerturbed( rig, what );
}

void JozzRig_SetControllerEnabled( JozzRig* rig, int enabled )
{
	if ( rig->controllerEnabled == enabled )
		return;
	rig->controllerEnabled = enabled;
	MarkPerturbed( rig, enabled ? "controller ON" : "controller OFF" );
}

void JozzRig_SetTargetSpeed( JozzRig* rig, double v )
{
	if ( rig->targetSpeed == v )
		return;
	rig->targetSpeed = v;
	MarkPerturbed( rig, "target speed changed" );
}

void JozzRig_SetLoad( JozzRig* rig, double n )
{
	if ( rig->loadN == n )
		return;
	rig->loadN = n;
	MarkPerturbed( rig, "load changed" );
}

// ---------------------------------------------------------------- kontakt

int JozzRig_ContactPoints( const JozzRig* rig, JozzRigContactPoint* out, int cap )
{
	static b3ContactData cd[128];
	int cc = b3Body_GetContactData( rig->body, cd, 128 );
	b3Pos com = b3Body_GetPosition( rig->body ); // md.center == 0, wiec origin == srodek masy
	int n = 0;
	for ( int i = 0; i < cc && n < cap; ++i )
	{
		// Anchory sa wzgledne do srodka masy CIALA A / CIALA B. Trzeba wiedziec,
		// ktorym z nich jest cialo rigu, inaczej punkty ladowaly by w gruncie.
		b3BodyId ba = b3Shape_GetBody( cd[i].shapeIdA );
		int weAreA = B3_ID_EQUALS( ba, rig->body );
		for ( int m = 0; m < cd[i].manifoldCount && n < cap; ++m )
		{
			const b3Manifold* mf = &cd[i].manifolds[m];
			for ( int k = 0; k < mf->pointCount && n < cap; ++k )
			{
				const b3ManifoldPoint* p = &mf->points[k];
				b3Vec3 a = weAreA ? p->anchorA : p->anchorB;
				out[n].point.x = com.x + a.x;
				out[n].point.y = com.y + a.y;
				out[n].point.z = com.z + a.z;
				out[n].normal = mf->normal;
				out[n].normalImpulse = p->totalNormalImpulse;
				out[n].separation = p->separation;
				out[n].persisted = p->persisted ? 1 : 0;
				++n;
			}
		}
	}
	return n;
}

// ---------------------------------------------------------------- odcisk stanu

void JozzRig_DigestLine( const JozzRig* rig, char* out, size_t cap )
{
	b3Pos p = b3Body_GetPosition( rig->body );
	b3Quat q = b3Body_GetRotation( rig->body );
	b3Vec3 v = b3Body_GetLinearVelocity( rig->body );
	b3Vec3 w = b3Body_GetAngularVelocity( rig->body );
	snprintf( out, cap,
			  "%d,%.17g,%.17g,%.17g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.17g,%.17g\n", rig->step,
			  (double)p.x, (double)p.y, (double)p.z, (double)q.v.x, (double)q.v.y, (double)q.v.z, (double)q.s,
			  (double)v.x, (double)v.y, (double)v.z, (double)w.x, (double)w.y, (double)w.z, rig->integral,
			  rig->wDriveSigned );
}
