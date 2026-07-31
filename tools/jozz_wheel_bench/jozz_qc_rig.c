// Rig Q3 - QUARTER CAR, implementacja. Patrz jozz_qc_rig.h.
//
// Kazda linia, ktora dotyka swiata, drogi, cial, wiezu i napedu, jest TUTAJ.

#include "jozz_qc_rig.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define JQC_PI 3.14159265358979

const char* JozzQc_RoadName( JozzQcRoad r )
{
	if ( r == JOZZ_QC_ROAD_FLAT )
		return "flat";
	if ( r == JOZZ_QC_ROAD_CLEAT )
		return "cleat";
	if ( r == JOZZ_QC_ROAD_COMB )
		return "comb";
	return "?";
}

const char* JozzQc_DriveName( JozzQcDrive d )
{
	if ( d == JOZZ_QC_DRIVE_SPEED )
		return "speed";
	if ( d == JOZZ_QC_DRIVE_TORQUE )
		return "torque";
	if ( d == JOZZ_QC_DRIVE_COAST )
		return "coast";
	return "?";
}

JozzQcConfig JozzQc_DefaultConfig( void )
{
	JozzRigConfig w = JozzRig_DefaultConfig();
	JozzQcConfig c;
	memset( &c, 0, sizeof( c ) );

	// Kolo: bez zmian wobec Q2A. To jest warunek reguly transferu, nie oszczednosc.
	c.variant = JOZZ_RIG_SPHERE;
	c.segments = 32;
	c.wheelR = w.wheelR;
	c.wheelW = w.wheelW;
	c.crownR = w.crownR;
	c.unsprungKg = w.massKg;
	c.inertiaSpin = w.inertiaSpinFactor;
	c.inertiaTrans = w.inertiaTransFactor;
	c.density = w.density;
	c.friction = w.friction;

	// Nadwozie i zawieszenie - kontrakt par. 4 i par. 5.
	c.sprungKg = 150.0f;
	c.springNPerM = 13500.0f;
	c.zeta = 0.35f;
	// 200 mm skoku calkowitego, statyka na 55% - typowy narożnik drogowy.
	c.bumpTravel = 0.09f;
	c.droopTravel = 0.11f;
	// 0.95 m, nie 0.6: przy 0.6 bryla nadwozia przenikala kolo na ekranie.
	//
	// UWAGA, zmierzone: ta zmiana jest geometrycznie neutralna (os zawieszenia
	// jest pionowa, oba zaczepy leza na niej, ramie jest zerowe), a MIMO TO
	// przesunela wyniki - `airborne` dla torus-32 przy 13 m/s poszlo z 6.0% na
	// 10.8%, czyli przez prog wazności przebiegu. Przyczyna nie jest bledem:
	// kolo skaczace po fasetach jest ukladem chaotycznym i rozne zaokraglenie
	// zmiennoprzecinkowe rozjezdza trajektorie. Dlatego --qc-compare NIE ufa
	// pojedynczemu przebiegowi i powtarza kazda komorke (patrz QC_REPEATS).
	c.mountH = 0.95f;

	// Droga. Prog 20 mm przy 60 mm dlugosci: dobrany tak, zeby przy predkosci
	// kontraktowej udar byl mierzalny, a kolo nie spedzalo w powietrzu wiecej niz
	// 10% krokow (par. 8 punkt 1). To NIE jest wartosc bezpieczna z zalozenia -
	// przebieg i tak liczy `airborne_fraction` i sam sie uniewaznia.
	c.groundHalfX = w.groundHalfX;
	c.groundHalfZ = w.groundHalfZ;
	c.road = JOZZ_QC_ROAD_FLAT;
	c.obstacleH = 0.02f;
	c.obstacleLen = 0.06f;
	c.combSpacing = 1.5f;

	// Protokol: STALA PREDKOSC, mierzony moment (kontrakt par. 3).
	//
	// Nastawy sa PRZELICZONE z Q2A, a nie przepisane. Q2A: sila na cialo 44 kg,
	// kp 440, ki 1100 => omega_n 5 rad/s, zeta 1. Tutaj wejsciem jest moment, a
	// obiektem cale nadwozie plus bezwladnosc obrotowa kola sprowadzona do ruchu
	// postepowego:
	//     m_eff = (150 + 44) + I_spin/R^2 = 194 + 30.80 = 224.80 kg
	//     dv/dt = tau / (R * m_eff) = tau / 115.57
	// zeby dostac TE SAMA petle zamknieta (omega_n 5, zeta 1):
	//     ki = 25 * 115.57 = 2889.2      kp = 10 * 115.57 = 1155.7
	// Przepisanie 440/1100 wprost dawaloby regulator o 5x mniejszym wzmocnieniu i
	// mierzylibysmy jego opieszalosc zamiast oporu toczenia.
	c.drive = JOZZ_QC_DRIVE_SPEED;
	c.targetSpeed = JOZZ_RIG_TARGET_V;
	c.kp = 1155.7;
	c.ki = 2889.2;
	// mu * N * R przy mu = 1: powyzej tego kolo i tak buksuje, wiec wyzszy limit
	// nie jest mocniejszym napedem, tylko cichszym zerwaniem przyczepnosci.
	c.maxTorque = 1000.0;
	c.constTorque = 300.0;
	c.startSpeed = JOZZ_RIG_TARGET_V;
	c.startX = -80.0;

	c.gravity = w.gravity;
	c.dt = w.dt;
	c.substeps = w.substeps;
	return c;
}

JozzRigConfig JozzQc_WheelConfig( const JozzQcConfig* c )
{
	JozzRigConfig w = JozzRig_DefaultConfig();
	w.variant = c->variant;
	w.prismSides = c->segments;
	w.wheelR = c->wheelR;
	w.wheelW = c->wheelW;
	w.crownR = c->crownR;
	w.massKg = c->unsprungKg;
	w.inertiaSpinFactor = c->inertiaSpin;
	w.inertiaTransFactor = c->inertiaTrans;
	w.density = c->density;
	w.friction = c->friction;
	w.gravity = c->gravity;
	w.dt = c->dt;
	w.substeps = c->substeps;
	return w;
}

double JozzQc_ReducedMass( const JozzQcConfig* c )
{
	double a = (double)c->sprungKg, b = (double)c->unsprungKg;
	if ( a <= 0.0 || b <= 0.0 )
		return 0.0;
	return a * b / ( a + b );
}

double JozzQc_Hertz( const JozzQcConfig* c )
{
	double m = JozzQc_ReducedMass( c );
	if ( m <= 0.0 || c->springNPerM <= 0.0f )
		return 0.0;
	return sqrt( (double)c->springNPerM / m ) / ( 2.0 * JQC_PI );
}

double JozzQc_StaticSag( const JozzQcConfig* c )
{
	if ( c->springNPerM <= 0.0f )
		return 0.0;
	return (double)c->sprungKg * c->gravity / (double)c->springNPerM;
}

void JozzQc_ConfigDigest( const JozzQcConfig* c, char* out, size_t cap )
{
	if ( out == NULL || cap == 0 )
		return;
	char crown[48];
	crown[0] = '\0';
	if ( c->variant == JOZZ_RIG_TORUS )
		snprintf( crown, sizeof( crown ), " crown=%.9g", (double)c->crownR );
	snprintf( out, cap,
			  "v=%s N=%d R=%.9g W=%.9g%s mu=%.9g mus=%.9g ms=%.9g iS=%.9g iT=%.9g "
			  "k=%.9g zeta=%.9g hz=%.9g bump=%.9g droop=%.9g mount=%.9g "
			  "road=%s h=%.9g len=%.9g s=%.9g "
			  "drive=%s tgt=%.17g kp=%.17g ki=%.17g taumax=%.17g tau=%.17g "
			  "x0=%.17g v0=%.17g g=%.17g dt=%.17g sub=%d",
			  JozzRig_VariantName( c->variant ), c->variant == JOZZ_RIG_SPHERE ? 0 : c->segments,
			  (double)c->wheelR, (double)c->wheelW, crown, (double)c->friction, (double)c->unsprungKg,
			  (double)c->sprungKg, (double)c->inertiaSpin, (double)c->inertiaTrans, (double)c->springNPerM,
			  (double)c->zeta, JozzQc_Hertz( c ), (double)c->bumpTravel, (double)c->droopTravel,
			  (double)c->mountH, JozzQc_RoadName( c->road ), (double)c->obstacleH, (double)c->obstacleLen,
			  (double)c->combSpacing, JozzQc_DriveName( c->drive ), c->targetSpeed, c->kp, c->ki, c->maxTorque,
			  c->constTorque, c->startX, c->startSpeed, c->gravity, c->dt, c->substeps );
}

// ---------------------------------------------------------------- ramki wiezu
//
// Te dwie funkcje sa dokladnie tym miejscem, w ktorym blad NIE wywala programu,
// tylko po cichu podstawia inna os. Zmierzone przez sonde Q3-1 (pomiar D):
// alfa/(tau/I_spin) = 1.013, wiec ramka B trafia w os, na ktorej siedzi I_spin.

static b3Quat JqcSuspensionFrame( void )
{
	// cx ramki A = os zawieszenia (src/wheel_joint.c:459) -> swiatowe +Y
	b3Matrix3 m;
	m.cx.x = 0.0f, m.cx.y = 1.0f, m.cx.z = 0.0f;
	m.cy.x = -1.0f, m.cy.y = 0.0f, m.cy.z = 0.0f;
	m.cz.x = 0.0f, m.cz.y = 0.0f, m.cz.z = 1.0f;
	return b3MakeQuatFromMatrix( &m );
}

static b3Quat JqcWheelFrame( void )
{
	// cz ramki B = os obrotu (src/wheel_joint.c:473). Cialo kola jest obrocone
	// tak, ze jego LOKALNE Y lezy na swiatowym Z - a I_spin siedzi na lokalnym Y.
	b3Matrix3 m;
	m.cx.x = 1.0f, m.cx.y = 0.0f, m.cx.z = 0.0f;
	m.cy.x = 0.0f, m.cy.y = 0.0f, m.cy.z = -1.0f;
	m.cz.x = 0.0f, m.cz.y = 1.0f, m.cz.z = 0.0f;
	return b3MakeQuatFromMatrix( &m );
}

// Skok zawieszenia. DODATNI = sciskanie. To nie jest zalozenie - jest rowny
// translacji wiezu z konstrukcji: wheelY - (chassisY - mountH).
static double JqcTravel( const JozzQcRig* r )
{
	b3Pos c = b3Body_GetPosition( r->chassis );
	b3Pos w = b3Body_GetPosition( r->wheel );
	return (double)w.y - ( (double)c.y - (double)r->cfg.mountH );
}

// ---------------------------------------------------------------- droga

static int JqcBuildRoad( JozzQcRig* r )
{
	const JozzQcConfig* c = &r->cfg;
	if ( c->road == JOZZ_QC_ROAD_FLAT )
		return 0;

	b3ShapeDef sd = b3DefaultShapeDef();
	sd.baseMaterial.friction = c->friction;
	sd.baseMaterial.rollingResistance = 0.0f;
	b3BoxHull box = b3MakeBoxHull( 0.5f * c->obstacleLen, 0.5f * c->obstacleH, c->groundHalfZ );

	// Progi stoja NA plycie, ktorej gorna powierzchnia jest na y = 0. Kazdy jest
	// osobnym cialem statycznym - prosciej niz jeden kadlub z N ksztaltami i tak
	// samo tanio, bo statyczne ciala nie wchodza do solvera.
	double first, last, spacing;
	if ( c->road == JOZZ_QC_ROAD_CLEAT )
	{
		// Jeden prog, ustawiony ZA rozgrzewka: 20 m za punktem startu, czyli
		// ~1.5 s jazdy przy predkosci kontraktowej.
		first = c->startX + 20.0;
		last = first;
		spacing = 1.0;
	}
	else
	{
		first = c->startX + 20.0;
		last = (double)c->groundHalfX - 5.0;
		spacing = (double)c->combSpacing;
	}
	if ( spacing <= 0.0 )
		return 0;

	int n = 0;
	for ( double x = first; x <= last + 1e-9; x += spacing )
	{
		b3BodyDef bd = b3DefaultBodyDef();
		bd.position.x = (float)x;
		bd.position.y = 0.5f * c->obstacleH;
		bd.position.z = 0.0f;
		b3BodyId b = b3CreateBody( r->world, &bd );
		b3CreateHullShape( b, &sd, &box.base );
		++n;
		if ( n > 4000 )
			break;
	}
	return n;
}

// ---------------------------------------------------------------- konstrukcja

int JozzQc_Create( JozzQcRig* rig, const JozzQcConfig* cfg, const JozzRigRenderHooks* hooks, char* err,
				   size_t errCap )
{
	memset( rig, 0, sizeof( *rig ) );
	if ( err && errCap )
		err[0] = '\0';
	rig->cfg = *cfg;

	JozzRigConfig wcfg = JozzQc_WheelConfig( cfg );
	if ( cfg->variant == JOZZ_RIG_TORUS )
	{
		int nMin = JozzRig_MinTorusSegments( &wcfg );
		if ( cfg->segments < nMin )
		{
			snprintf( err, errCap, "torus-N: %d kapsul przy crown_r %.4g daje NIESZCZELNA obwiednie, minimum %d",
					  cfg->segments, (double)cfg->crownR, nMin );
			return 0;
		}
	}

	b3WorldDef wd = b3DefaultWorldDef();
	wd.workerCount = 1;
	wd.gravity.x = 0.0f;
	wd.gravity.y = -(float)cfg->gravity;
	wd.gravity.z = 0.0f;
	if ( hooks )
	{
		wd.createDebugShape = hooks->createDebugShape;
		wd.destroyDebugShape = hooks->destroyDebugShape;
		wd.userDebugShapeContext = hooks->userDebugShapeContext;
	}
	rig->world = b3CreateWorld( &wd );
	b3World_EnableContinuous( rig->world, false );

	// Plyta: gorna powierzchnia na y = 0, jak w Q2A.
	{
		b3BodyDef gd = b3DefaultBodyDef();
		gd.position.x = 0.0f;
		gd.position.y = -1.0f;
		gd.position.z = 0.0f;
		rig->ground = b3CreateBody( rig->world, &gd );
		b3ShapeDef gs = b3DefaultShapeDef();
		gs.baseMaterial.friction = cfg->friction;
		gs.baseMaterial.rollingResistance = 0.0f;
		b3BoxHull box = b3MakeBoxHull( cfg->groundHalfX, 1.0f, cfg->groundHalfZ );
		b3CreateHullShape( rig->ground, &gs, &box.base );
	}
	rig->obstacleCount = JqcBuildRoad( rig );

	// KOLO
	const float R = cfg->wheelR;
	b3BodyDef bw = b3DefaultBodyDef();
	bw.type = b3_dynamicBody;
	bw.position.x = (float)cfg->startX;
	bw.position.y = R;
	bw.position.z = 0.0f;
	{
		b3Vec3 from, to;
		from.x = 0.0f, from.y = 1.0f, from.z = 0.0f;
		to.x = 0.0f, to.y = 0.0f, to.z = 1.0f;
		bw.rotation = b3ComputeQuatBetweenUnitVectors( from, to );
	}
	bw.linearVelocity.x = (float)cfg->startSpeed;
	bw.angularVelocity.z = -(float)cfg->startSpeed / R;
	bw.enableSleep = false;
	bw.allowFastRotation = true;
	rig->wheel = b3CreateBody( rig->world, &bw );
	if ( JozzRig_BuildEnvelopeFromConfig( rig->wheel, &wcfg ) == 0 )
	{
		snprintf( err, errCap, "wariant %s (N=%d) nieprzedstawialny", JozzRig_VariantName( cfg->variant ),
				  cfg->segments );
		b3DestroyWorld( rig->world );
		memset( rig, 0, sizeof( *rig ) );
		return 0;
	}
	rig->shapeCount = b3Body_GetShapeCount( rig->wheel );
	JozzRig_FreezeMassEx( rig->wheel, cfg->unsprungKg, cfg->wheelR, cfg->inertiaSpin, cfg->inertiaTrans );

	// NADWOZIE. KOLEJNOSC TYCH TRZECH KROKOW JEST CZESCIA KONSTRUKCJI:
	//   shape -> blokady -> masa.
	// b3Body_SetMotionLocks wola b3UpdateBodyMassData przy zmianie statusu
	// fixedRotation (src/body.c), a ten przelicza mase Z KSZTALTOW. Odwrotna
	// kolejnosc daje 0 kg zamiast 150 kg i przyrzad mierzy nieruchomy sufit -
	// zmierzone, nie przewidziane (Q3-1).
	//
	// Nadwozie startuje JUZ UGIETE o ugiecie statyczne. Bez tego kazdy przebieg
	// zaczyna sie od 2-3 s dzwonienia zawieszenia, ktore albo trzeba przeczekac,
	// albo - gorzej - wpuscic do okna pomiarowego.
	const double sag = JozzQc_StaticSag( cfg );
	b3BodyDef bc = b3DefaultBodyDef();
	bc.type = b3_dynamicBody;
	bc.position.x = (float)cfg->startX;
	bc.position.y = (float)( (double)R + (double)cfg->mountH - sag );
	bc.position.z = 0.0f;
	bc.linearVelocity.x = (float)cfg->startSpeed;
	bc.enableSleep = false;
	rig->chassis = b3CreateBody( rig->world, &bc );
	{
		// maskBits = 0 wystarcza, zeby nadwozie z niczym nie kolidowalo: test
		// kolizji wymaga (A.category & B.mask) I (B.category & A.mask), wiec
		// zerowa maska konczy sprawe. categoryBits MUSI zostac niezerowe, bo
		// b3World_Draw filtruje ksztalty ta sama maska - przy zerowej kategorii
		// masa resorowana byla NIEWIDOCZNA w oknie, czyli okno pokazywalo kolo
		// wiszace w powietrzu i milczalo o tym, co Q3 wlasciwie mierzy.
		// Zlapane na zrzucie, nie przez test.
		b3ShapeDef cs = b3DefaultShapeDef();
		cs.filter.maskBits = 0;
		b3BoxHull cb = b3MakeBoxHull( 0.45f, 0.25f, 0.35f );
		b3CreateHullShape( rig->chassis, &cs, &cb.base );
	}
	{
		// linearX (jazda) i linearY (skok) swobodne; reszta zablokowana, bo
		// narożnik pojazdu w izolacji nie ma tych stopni swobody (kontrakt par. 4).
		b3MotionLocks lk;
		memset( &lk, 0, sizeof( lk ) );
		lk.linearZ = true;
		lk.angularX = lk.angularY = lk.angularZ = true;
		b3Body_SetMotionLocks( rig->chassis, lk );
	}
	{
		b3MassData md = b3Body_GetMassData( rig->chassis );
		md.mass = cfg->sprungKg;
		md.center.x = 0.0f, md.center.y = 0.0f, md.center.z = 0.0f;
		float I = cfg->sprungKg * 0.25f; // obroty zablokowane; wartosc JAWNA
		md.inertia.cx.x = I, md.inertia.cx.y = 0.0f, md.inertia.cx.z = 0.0f;
		md.inertia.cy.x = 0.0f, md.inertia.cy.y = I, md.inertia.cy.z = 0.0f;
		md.inertia.cz.x = 0.0f, md.inertia.cz.y = 0.0f, md.inertia.cz.z = I;
		b3Body_SetMassData( rig->chassis, md );
	}

	// WIEZ
	rig->builtHertz = JozzQc_Hertz( cfg );
	rig->travelUpper = sag + (double)cfg->bumpTravel;
	rig->travelLower = sag - (double)cfg->droopTravel;
	{
		b3WheelJointDef jd = b3DefaultWheelJointDef();
		jd.base.bodyIdA = rig->chassis;
		jd.base.bodyIdB = rig->wheel;
		jd.base.localFrameA.p.x = 0.0f, jd.base.localFrameA.p.y = -cfg->mountH, jd.base.localFrameA.p.z = 0.0f;
		jd.base.localFrameA.q = JqcSuspensionFrame();
		jd.base.localFrameB.p.x = 0.0f, jd.base.localFrameB.p.y = 0.0f, jd.base.localFrameB.p.z = 0.0f;
		jd.base.localFrameB.q = JqcWheelFrame();
		jd.base.collideConnected = false;
		jd.enableSuspensionSpring = true;
		jd.suspensionHertz = (float)rig->builtHertz;
		jd.suspensionDampingRatio = cfg->zeta;
		jd.enableSuspensionLimit = true;
		jd.lowerSuspensionLimit = (float)rig->travelLower;
		jd.upperSuspensionLimit = (float)rig->travelUpper;
		jd.enableSteering = false;
		// Silnik wiezu WYLACZONY. Zmierzone (F-20): przy nasyceniu dostarcza
		// 2.018x zadanego momentu, a wspolczynnik zalezy od liczby podkrokow.
		// Naped idzie przez b3Body_ApplyTorque, gdzie N*m znaczy N*m.
		jd.enableSpinMotor = false;
		jd.maxSpinTorque = 0.0f;
		jd.spinSpeed = 0.0f;
		rig->joint = b3CreateWheelJoint( rig->world, &jd );
	}

	// --- samokontrola: czy zbudowany uklad JEST tym, ktory zamowiono ----------
	rig->builtSprungKg = (double)b3Body_GetMass( rig->chassis );
	rig->builtUnsprungKg = (double)b3Body_GetMass( rig->wheel );
	{
		double travel0 = JqcTravel( rig );
		if ( fabs( rig->builtSprungKg - (double)cfg->sprungKg ) > 1e-3 )
		{
			snprintf( err, errCap, "masa nadwozia zbudowana %.6g kg, zamowiona %.6g kg", rig->builtSprungKg,
					  (double)cfg->sprungKg );
			JozzQc_Destroy( rig );
			return 0;
		}
		if ( fabs( rig->builtUnsprungKg - (double)cfg->unsprungKg ) > 1e-3 )
		{
			snprintf( err, errCap, "masa kola zbudowana %.6g kg, zamowiona %.6g kg", rig->builtUnsprungKg,
					  (double)cfg->unsprungKg );
			JozzQc_Destroy( rig );
			return 0;
		}
		if ( fabs( travel0 - sag ) > 1e-4 )
		{
			snprintf( err, errCap, "skok poczatkowy %.6g m, oczekiwane ugiecie statyczne %.6g m", travel0, sag );
			JozzQc_Destroy( rig );
			return 0;
		}
		if ( cfg->bumpTravel <= 0.0f || cfg->droopTravel <= 0.0f )
		{
			snprintf( err, errCap, "skok bump %.4g / droop %.4g - zawieszenie bez skoku nie jest zawieszeniem",
					  (double)cfg->bumpTravel, (double)cfg->droopTravel );
			JozzQc_Destroy( rig );
			return 0;
		}
	}

	rig->prevSprungVy = (double)b3Body_GetLinearVelocity( rig->chassis ).y;
	return 1;
}

void JozzQc_Destroy( JozzQcRig* rig )
{
	if ( B3_IS_NON_NULL( rig->world ) )
		b3DestroyWorld( rig->world );
	memset( rig, 0, sizeof( *rig ) );
}

void JozzQc_MarkPerturbation( JozzQcRig* rig, const char* what )
{
	rig->perturbed = 1;
	rig->perturbCount += 1;
	snprintf( rig->lastPerturbation, sizeof( rig->lastPerturbation ), "%s", what ? what : "?" );
}

int JozzQc_ContactPoints( const JozzQcRig* rig, JozzRigContactPoint* out, int cap )
{
	static b3ContactData cd[256];
	int cc = b3Body_GetContactData( rig->wheel, cd, 256 );
	b3Pos com = b3Body_GetPosition( rig->wheel );
	int n = 0;
	for ( int i = 0; i < cc && n < cap; ++i )
	{
		b3BodyId ba = b3Shape_GetBody( cd[i].shapeIdA );
		int weAreA = B3_ID_EQUALS( ba, rig->wheel );
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

// Telemetria kontaktu kola. Ta sama definicja co w Q2A (wheel_bench.c sekcja E):
// nosny punkt = totalNormalImpulse > 0, churn = nosny I nieprzeniesiony.
static void JqcContactTelemetry( const JozzQcRig* r, int* loaded, int* fresh, double* impulse )
{
	static b3ContactData cd[256];
	int cc = b3Body_GetContactData( r->wheel, cd, 256 );
	*loaded = 0;
	*fresh = 0;
	*impulse = 0.0;
	for ( int i = 0; i < cc; ++i )
	{
		for ( int m = 0; m < cd[i].manifoldCount; ++m )
		{
			const b3Manifold* mf = &cd[i].manifolds[m];
			for ( int k = 0; k < mf->pointCount; ++k )
			{
				const b3ManifoldPoint* p = &mf->points[k];
				if ( p->totalNormalImpulse > 0.0f )
				{
					*loaded += 1;
					*impulse += (double)p->totalNormalImpulse;
					if ( !p->persisted )
						*fresh += 1;
				}
			}
		}
	}
}

void JozzQc_Step( JozzQcRig* rig, JozzQcSample* out )
{
	const JozzQcConfig* c = &rig->cfg;
	const double dt = c->dt;

	// 1) stan PRZED krokiem
	b3Vec3 vc = b3Body_GetLinearVelocity( rig->chassis );

	// 2) naped
	double tau = 0.0;
	int saturated = 0;
	if ( c->drive == JOZZ_QC_DRIVE_SPEED )
	{
		double err = c->targetSpeed - (double)vc.x;
		double raw = c->kp * err + c->ki * rig->integral;
		tau = raw > c->maxTorque ? c->maxTorque : ( raw < -c->maxTorque ? -c->maxTorque : raw );
		saturated = ( fabs( raw ) > c->maxTorque );
		if ( !saturated )
			rig->integral += err * dt; // anti-windup: calka zamarza w saturacji
	}
	else if ( c->drive == JOZZ_QC_DRIVE_TORQUE )
	{
		tau = c->constTorque;
	}

	// 3) moment na kolo wokol JEGO osi. Kierunek jazdy +X przy osi wzdluz +Z
	// oznacza obrot UJEMNY - stad minus. Os liczona z ciala, nie zalozona:
	// po najechaniu na prog kolo jest juz obrocone.
	if ( tau != 0.0 )
	{
		b3Vec3 axle = JozzRig_AxleWorld( rig->wheel );
		double len = sqrt( JozzRig_Dot3( axle, axle ) );
		if ( len > 1e-9 )
		{
			b3Vec3 t;
			t.x = (float)( -tau * axle.x / len );
			t.y = (float)( -tau * axle.y / len );
			t.z = (float)( -tau * axle.z / len );
			b3Body_ApplyTorque( rig->wheel, t, true );
		}
	}

	// 4) krok
	b3World_Step( rig->world, (float)dt, c->substeps );
	rig->step += 1;

	// 5) pomiar POKROKOWY
	b3Vec3 vc2 = b3Body_GetLinearVelocity( rig->chassis );
	b3Pos pc = b3Body_GetPosition( rig->chassis );
	b3Pos pw = b3Body_GetPosition( rig->wheel );
	JozzWheelKin k = JozzRig_KinematicsR( rig->wheel, c->wheelR );

	int loaded = 0, fresh = 0;
	double impulse = 0.0;
	JqcContactTelemetry( rig, &loaded, &fresh, &impulse );

	double travel = JqcTravel( rig );
	double accelY = ( (double)vc2.y - rig->prevSprungVy ) / dt;
	rig->prevSprungVy = (double)vc2.y;

	if ( out )
	{
		memset( out, 0, sizeof( *out ) );
		out->step = rig->step;
		out->time = (double)rig->step * dt;
		out->x = (double)pc.x;
		out->speed = (double)vc2.x;
		out->driveTorque = tau;
		out->saturated = saturated;
		out->travel = travel;
		out->sprungY = (double)pc.y;
		out->sprungAccelY = accelY;
		out->wheelY = (double)pw.y;
		out->omegaSpin = k.omegaSpin;
		// Poslizg wzgledem predkosci jazdy. Mianownik ograniczony od dolu, zeby
		// przy zatrzymanym kole liczba nie uciekala w nieskonczonosc.
		{
			double v = fabs( (double)vc2.x );
			double denom = v > 0.5 ? v : 0.5;
			out->slipRatio = ( k.referenceRimSpeed - (double)vc2.x ) / denom;
		}
		out->normalImpulse = impulse;
		out->loadedPoints = loaded;
		out->newLoadedPoints = fresh;
		out->airborne = ( loaded == 0 );
		out->limitHit = ( travel >= rig->travelUpper - 1e-4 || travel <= rig->travelLower + 1e-4 );
	}
}

// ---------------------------------------------------------------- okno pomiarowe

void JozzQc_WindowBegin( JozzQcWindow* w )
{
	memset( w, 0, sizeof( *w ) );
	w->travelMin = 1e30;
	w->travelMax = -1e30;
}

// Sumy trzymane w tych samych polach, na koncu dzielone przez liczbe krokow.
// Prosto, ale trzeba pamietac: miedzy Add a End pola NIE sa srednimi.
void JozzQc_WindowAdd( JozzQcWindow* w, const JozzQcSample* s )
{
	w->steps += 1;
	w->driveTorqueMean += s->driveTorque;
	w->sprungAccelRms += s->sprungAccelY * s->sprungAccelY;
	w->airborneFraction += s->airborne ? 1.0 : 0.0;
	w->travelRms += s->travel * s->travel;
	if ( s->travel < w->travelMin )
		w->travelMin = s->travel;
	if ( s->travel > w->travelMax )
		w->travelMax = s->travel;
	w->limitHits += s->limitHit ? 1 : 0;
	w->contactChurnPct += s->newLoadedPoints;
	w->loadedPointsAvg += s->loadedPoints;
	w->slipRatioMean += s->slipRatio;
	w->speedMean += s->speed;
	w->saturatedSteps += s->saturated ? 1 : 0;
}

void JozzQc_WindowEnd( JozzQcWindow* w, const JozzQcRig* rig )
{
	if ( w->steps <= 0 )
		return;
	double n = (double)w->steps;
	double sumFresh = w->contactChurnPct;
	double sumLoaded = w->loadedPointsAvg;

	w->driveTorqueMean /= n;
	w->sprungAccelRms = sqrt( w->sprungAccelRms / n );
	w->airborneFraction /= n;
	w->travelRms = sqrt( w->travelRms / n );
	w->loadedPointsAvg = sumLoaded / n;
	w->contactChurnPct = ( sumLoaded > 0.0 ) ? 100.0 * sumFresh / sumLoaded : 0.0;
	w->slipRatioMean /= n;
	w->speedMean /= n;

	// omega z predkosci sredniej i promienia nominalnego - ta sama definicja co
	// referenceRimSpeed w Q2A, zeby moc porownac moc strat miedzy szczeblami.
	double omega = ( rig->cfg.wheelR > 0.0f ) ? w->speedMean / (double)rig->cfg.wheelR : 0.0;
	w->lossPower = w->driveTorqueMean * omega;

	// Werdykt par. 8 kontraktu.
	w->invalid = 0;
	w->invalidWhy[0] = '\0';
	if ( w->airborneFraction > 0.10 )
	{
		w->invalid = 1;
		snprintf( w->invalidWhy, sizeof( w->invalidWhy ),
				  "kolo w powietrzu przez %.1f%% krokow (limit 10%%) - to mierzy profil drogi, nie opone",
				  100.0 * w->airborneFraction );
	}
	else if ( (double)w->saturatedSteps / n > 0.05 )
	{
		w->invalid = 1;
		snprintf( w->invalidWhy, sizeof( w->invalidWhy ), "regulator w saturacji przez %.1f%% krokow (limit 5%%)",
				  100.0 * (double)w->saturatedSteps / n );
	}
	else if ( rig->perturbed )
	{
		w->invalid = 1;
		snprintf( w->invalidWhy, sizeof( w->invalidWhy ), "sesja EXPLORATION: fizyka ruszona recznie %dx (%s)",
				  rig->perturbCount, rig->lastPerturbation );
	}
}

void JozzQc_TraceLine( const JozzQcSample* s, char* out, size_t cap )
{
	snprintf( out, cap, "%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,%d,%d,%d\n",
			  s->step, s->time, s->x, s->speed, s->driveTorque, s->travel, s->sprungY, s->sprungAccelY, s->wheelY,
			  s->omegaSpin, s->slipRatio, s->normalImpulse, s->loadedPoints, s->newLoadedPoints, s->airborne,
			  s->limitHit, s->saturated );
}
