// Rig Q3 - QUARTER CAR, implementacja. Patrz jozz_qc_rig.h.
//
// Kazda linia, ktora dotyka swiata, drogi, cial, wiezu i napedu, jest TUTAJ.

#include "jozz_qc_rig.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#define JQC_PI 3.14159265358979

// Zegar do kosztu CPU na krok. Osobny od stendu, bo protokol pomiaru mieszka
// teraz tutaj, a ten plik kompiluje sie takze do `samples` - stad warunek na
// windows.h zamiast bezwarunkowego include.
static double JqcNowMs( void )
{
#if defined( _WIN32 )
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency( &f );
	QueryPerformanceCounter( &c );
	return 1000.0 * (double)c.QuadPart / (double)f.QuadPart;
#else
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return 1000.0 * (double)ts.tv_sec + 1e-6 * (double)ts.tv_nsec;
#endif
}

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

int JozzQc_MinSegments( const JozzQcConfig* c )
{
	if ( c->variant != JOZZ_RIG_TORUS )
		return 3;
	{
		JozzRigConfig w = JozzQc_WheelConfig( c );
		int n = JozzRig_MinTorusSegments( &w );
		return n > 3 ? n : 3;
	}
}

int JozzQc_MaxSegments( const JozzQcConfig* c )
{
	if ( c->variant == JOZZ_RIG_PRISM_MAX )
		return JozzRig_ProbeMaxPrismSides();
	if ( c->variant == JOZZ_RIG_TORUS )
		return 128;
	return 3;
}

int JozzQc_ClampSegments( JozzQcConfig* c )
{
	int lo, hi, was;
	// Kula nie ma elementow obwodu, wiec nie ma czego wciskac w zakres. Wciskanie
	// i tak sie dzialo i KASOWALO liczbe: przejscie torus-64 -> sphere -> torus-N
	// wracalo z N=3, bo po drodze pole zostalo scisniete do zakresu kuli.
	// Nieuzywane pole ma zostac nietkniete, zeby przelaczenie wariantu tam i z
	// powrotem bylo operacja odwracalna.
	if ( c->variant == JOZZ_RIG_SPHERE )
		return 0;
	lo = JozzQc_MinSegments( c );
	hi = JozzQc_MaxSegments( c );
	was = c->segments;
	if ( hi < lo )
		hi = lo;
	if ( c->segments < lo )
		c->segments = lo;
	else if ( c->segments > hi )
		c->segments = hi;
	return c->segments != was;
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

// ---------------------------------------------------- konstrukcja jako plik
//
// Ta sama konstrukcja co `.rig`: JEDNA tabela pol obsluguje zapis i odczyt, bo
// przy dwoch osobnych funkcjach dolozenie pola do JozzQcConfig i zapomnienie o
// nim po jednej ze stron daje plik, ktory CICHO gubi czesc tozsamosci przebiegu.
// Rozszerzenie `.qc`, bo plik opisuje caly narożnik: kolo, zawieszenie, droge,
// naped i przyrzad.

typedef enum
{
	JQ_F_INT,
	JQ_F_FLOAT,
	JQ_F_DOUBLE,
	JQ_F_VARIANT,
	JQ_F_ROAD,
	JQ_F_DRIVE
} JqFieldKind;

typedef struct
{
	const char* key;
	JqFieldKind kind;
	size_t off;
	const char* group; // niepusty zaczyna nowa sekcje w zapisie
} JqField;

#define JQ_OFF( f ) offsetof( JozzQcConfig, f )

static const JqField s_qcFields[] = {
	{ "variant", JQ_F_VARIANT, JQ_OFF( variant ), "kolo" },
	{ "segments", JQ_F_INT, JQ_OFF( segments ), NULL },
	{ "wheel_r", JQ_F_FLOAT, JQ_OFF( wheelR ), NULL },
	{ "wheel_w", JQ_F_FLOAT, JQ_OFF( wheelW ), NULL },
	{ "crown_r", JQ_F_FLOAT, JQ_OFF( crownR ), NULL },
	{ "unsprung_kg", JQ_F_FLOAT, JQ_OFF( unsprungKg ), NULL },
	{ "inertia_spin", JQ_F_FLOAT, JQ_OFF( inertiaSpin ), NULL },
	{ "inertia_trans", JQ_F_FLOAT, JQ_OFF( inertiaTrans ), NULL },
	{ "density", JQ_F_FLOAT, JQ_OFF( density ), NULL },
	{ "friction", JQ_F_FLOAT, JQ_OFF( friction ), NULL },

	{ "sprung_kg", JQ_F_FLOAT, JQ_OFF( sprungKg ), "zawieszenie" },
	{ "spring_n_per_m", JQ_F_FLOAT, JQ_OFF( springNPerM ), NULL },
	{ "zeta", JQ_F_FLOAT, JQ_OFF( zeta ), NULL },
	{ "bump_travel", JQ_F_FLOAT, JQ_OFF( bumpTravel ), NULL },
	{ "droop_travel", JQ_F_FLOAT, JQ_OFF( droopTravel ), NULL },
	{ "mount_h", JQ_F_FLOAT, JQ_OFF( mountH ), NULL },

	{ "ground_half_x", JQ_F_FLOAT, JQ_OFF( groundHalfX ), "droga" },
	{ "ground_half_z", JQ_F_FLOAT, JQ_OFF( groundHalfZ ), NULL },
	{ "road", JQ_F_ROAD, JQ_OFF( road ), NULL },
	{ "obstacle_h", JQ_F_FLOAT, JQ_OFF( obstacleH ), NULL },
	{ "obstacle_len", JQ_F_FLOAT, JQ_OFF( obstacleLen ), NULL },
	{ "comb_spacing", JQ_F_FLOAT, JQ_OFF( combSpacing ), NULL },

	{ "drive", JQ_F_DRIVE, JQ_OFF( drive ), "naped" },
	{ "target_speed", JQ_F_DOUBLE, JQ_OFF( targetSpeed ), NULL },
	{ "kp", JQ_F_DOUBLE, JQ_OFF( kp ), NULL },
	{ "ki", JQ_F_DOUBLE, JQ_OFF( ki ), NULL },
	{ "max_torque", JQ_F_DOUBLE, JQ_OFF( maxTorque ), NULL },
	{ "const_torque", JQ_F_DOUBLE, JQ_OFF( constTorque ), NULL },
	{ "start_speed", JQ_F_DOUBLE, JQ_OFF( startSpeed ), NULL },
	{ "start_x", JQ_F_DOUBLE, JQ_OFF( startX ), NULL },

	{ "gravity", JQ_F_DOUBLE, JQ_OFF( gravity ), "instrument" },
	{ "dt", JQ_F_DOUBLE, JQ_OFF( dt ), NULL },
	{ "substeps", JQ_F_INT, JQ_OFF( substeps ), NULL },
};

static const int s_qcFieldCount = (int)( sizeof( s_qcFields ) / sizeof( s_qcFields[0] ) );

static size_t JqFieldSize( JqFieldKind k )
{
	switch ( k )
	{
		case JQ_F_INT:
			return sizeof( int );
		case JQ_F_FLOAT:
			return sizeof( float );
		case JQ_F_DOUBLE:
			return sizeof( double );
		case JQ_F_VARIANT:
			return sizeof( JozzRigVariant );
		case JQ_F_ROAD:
			return sizeof( JozzQcRoad );
		case JQ_F_DRIVE:
			return sizeof( JozzQcDrive );
	}
	return 0;
}

// %.9g odtwarza kazdy float, %.17g kazdy double. Przy mniejszej liczbie cyfr
// zapis-odczyt przesuwalby konstrukcje o ostatni bit, wiec plik dawalby INNY
// przebieg niz ten, ktory Owner zapisal.
static void JqWriteValue( const JqField* f, const void* base, char* out, size_t cap )
{
	const char* p = (const char*)base + f->off;
	switch ( f->kind )
	{
		case JQ_F_INT:
			snprintf( out, cap, "%d", *(const int*)p );
			break;
		case JQ_F_FLOAT:
			snprintf( out, cap, "%.9g", (double)*(const float*)p );
			break;
		case JQ_F_DOUBLE:
			snprintf( out, cap, "%.17g", *(const double*)p );
			break;
		case JQ_F_VARIANT:
			snprintf( out, cap, "%s", JozzRig_VariantName( *(const JozzRigVariant*)p ) );
			break;
		case JQ_F_ROAD:
			snprintf( out, cap, "%s", JozzQc_RoadName( *(const JozzQcRoad*)p ) );
			break;
		case JQ_F_DRIVE:
			snprintf( out, cap, "%s", JozzQc_DriveName( *(const JozzQcDrive*)p ) );
			break;
	}
}

static int JqParseValue( const JqField* f, void* base, const char* value, char* err, size_t errCap )
{
	char* p = (char*)base + f->off;
	char* end = NULL;
	int i;
	switch ( f->kind )
	{
		case JQ_F_INT:
		{
			long v = strtol( value, &end, 10 );
			if ( end == value )
				break;
			*(int*)p = (int)v;
			return 1;
		}
		case JQ_F_FLOAT:
		{
			double v = strtod( value, &end );
			if ( end == value )
				break;
			*(float*)p = (float)v;
			return 1;
		}
		case JQ_F_DOUBLE:
		{
			double v = strtod( value, &end );
			if ( end == value )
				break;
			*(double*)p = v;
			return 1;
		}
		case JQ_F_VARIANT:
			for ( i = 0; i < JOZZ_RIG_VARIANT_COUNT; ++i )
				if ( strcmp( value, JozzRig_VariantName( (JozzRigVariant)i ) ) == 0 )
				{
					*(JozzRigVariant*)p = (JozzRigVariant)i;
					return 1;
				}
			snprintf( err, errCap, "nieznany wariant '%s'", value );
			return 0;
		case JQ_F_ROAD:
			for ( i = 0; i < JOZZ_QC_ROAD_COUNT; ++i )
				if ( strcmp( value, JozzQc_RoadName( (JozzQcRoad)i ) ) == 0 )
				{
					*(JozzQcRoad*)p = (JozzQcRoad)i;
					return 1;
				}
			snprintf( err, errCap, "nieznana droga '%s'", value );
			return 0;
		case JQ_F_DRIVE:
			for ( i = 0; i < JOZZ_QC_DRIVE_COUNT; ++i )
				if ( strcmp( value, JozzQc_DriveName( (JozzQcDrive)i ) ) == 0 )
				{
					*(JozzQcDrive*)p = (JozzQcDrive)i;
					return 1;
				}
			snprintf( err, errCap, "nieznany naped '%s'", value );
			return 0;
	}
	snprintf( err, errCap, "pole '%s': '%s' nie jest liczba", f->key, value );
	return 0;
}

int JozzQc_ConfigToText( const JozzQcConfig* c, char* out, size_t cap, const char* note )
{
	size_t used = 0;
	int i;
	int n = snprintf( out, cap, "format %d\n", JOZZ_QC_CONFIG_FORMAT );
	if ( n < 0 || (size_t)n >= cap )
		return 0;
	used = (size_t)n;

	if ( note && note[0] )
	{
		const char* s = note;
		if ( used + 2 >= cap )
			return 0;
		out[used++] = '#';
		out[used++] = ' ';
		for ( ; *s && used + 1 < cap; ++s )
			out[used++] = ( *s == '\n' || *s == '\r' ) ? ' ' : *s;
		if ( used + 1 >= cap )
			return 0;
		out[used++] = '\n';
	}

	for ( i = 0; i < s_qcFieldCount; ++i )
	{
		char value[64];
		JqWriteValue( &s_qcFields[i], c, value, sizeof( value ) );
		n = snprintf( out + used, cap - used, "%s%s%s%s %s\n", s_qcFields[i].group ? "\n# " : "",
					  s_qcFields[i].group ? s_qcFields[i].group : "", s_qcFields[i].group ? "\n" : "",
					  s_qcFields[i].key, value );
		if ( n < 0 || (size_t)n >= cap - used )
			return 0;
		used += (size_t)n;
	}
	return (int)used;
}

int JozzQc_ConfigFromText( JozzQcConfig* c, const char* text, char* err, size_t errCap )
{
	JozzQcConfig parsed = JozzQc_DefaultConfig();
	const char* p = text;
	int line = 0;
	int sawFormat = 0;

	if ( err && errCap )
		err[0] = '\0';

	while ( *p )
	{
		char buf[256];
		char key[64];
		const char* eol = strchr( p, '\n' );
		size_t len = eol ? (size_t)( eol - p ) : strlen( p );
		size_t k = 0;
		const char* v;
		int i, found = 0;

		line += 1;

		// Komentarz i pusta linia odpadaja PRZED limitem dlugosci: notatka z polki
		// ma kilkaset znakow, wiec limit nalozony wczesniej odrzucalby wlasne pliki
		// narzedzia - zapis by sie udawal, a odczyt nie.
		{
			const char* s = p;
			const char* e = p + len;
			while ( s < e && ( *s == ' ' || *s == '\t' || *s == '\r' ) )
				++s;
			if ( s == e || *s == '#' )
			{
				p = eol ? eol + 1 : p + len;
				continue;
			}
		}

		if ( len >= sizeof( buf ) )
		{
			snprintf( err, errCap, "linia %d za dluga", line );
			return 0;
		}
		memcpy( buf, p, len );
		buf[len] = '\0';
		p = eol ? eol + 1 : p + len;

		{
			char* s = buf;
			while ( *s == ' ' || *s == '\t' || *s == '\r' )
				++s;
			memmove( buf, s, strlen( s ) + 1 );
		}

		while ( buf[k] && buf[k] != ' ' && buf[k] != '\t' )
			++k;
		if ( k == 0 || k >= sizeof( key ) )
		{
			snprintf( err, errCap, "linia %d: brak klucza", line );
			return 0;
		}
		memcpy( key, buf, k );
		key[k] = '\0';

		v = buf + k;
		while ( *v == ' ' || *v == '\t' )
			++v;
		{
			char* tail = (char*)v + strlen( v );
			while ( tail > v && ( tail[-1] == ' ' || tail[-1] == '\t' || tail[-1] == '\r' ) )
				*--tail = '\0';
		}
		if ( *v == '\0' )
		{
			snprintf( err, errCap, "linia %d: klucz '%s' bez wartosci", line, key );
			return 0;
		}

		if ( strcmp( key, "format" ) == 0 )
		{
			if ( atoi( v ) != JOZZ_QC_CONFIG_FORMAT )
			{
				snprintf( err, errCap, "format pliku %s, obslugiwany %d", v, JOZZ_QC_CONFIG_FORMAT );
				return 0;
			}
			sawFormat = 1;
			continue;
		}

		for ( i = 0; i < s_qcFieldCount; ++i )
		{
			if ( strcmp( key, s_qcFields[i].key ) != 0 )
				continue;
			if ( JqParseValue( &s_qcFields[i], &parsed, v, err, errCap ) == 0 )
				return 0;
			found = 1;
			break;
		}
		if ( found == 0 )
		{
			// Nieznany klucz to blad, nie ostrzezenie. Cicho zignorowana literowka
			// daje plik, ktory OPISUJE jedna konstrukcje, a URUCHAMIA inna.
			snprintf( err, errCap, "linia %d: nieznany klucz '%s'", line, key );
			return 0;
		}
	}

	if ( sawFormat == 0 )
	{
		snprintf( err, errCap, "brak linii 'format' - to nie jest plik konstrukcji narożnika" );
		return 0;
	}
	*c = parsed;
	return 1;
}

int JozzQc_ConfigWriteFile( const JozzQcConfig* c, const char* path, const char* note )
{
	char text[JOZZ_QC_CONFIG_TEXT_CAP];
	FILE* f;
	int n = JozzQc_ConfigToText( c, text, sizeof( text ), note );
	if ( n <= 0 )
		return 0;
	f = fopen( path, "wb" );
	if ( f == NULL )
		return 0;
	fwrite( text, 1, (size_t)n, f );
	fclose( f );
	return 1;
}

int JozzQc_ConfigReadFile( JozzQcConfig* c, const char* path, char* err, size_t errCap )
{
	char text[JOZZ_QC_CONFIG_TEXT_CAP];
	size_t n;
	FILE* f = fopen( path, "rb" );
	if ( f == NULL )
	{
		snprintf( err, errCap, "nie moge otworzyc %s", path );
		return 0;
	}
	n = fread( text, 1, sizeof( text ) - 1, f );
	fclose( f );
	text[n] = '\0';
	return JozzQc_ConfigFromText( c, text, err, errCap );
}

// Ten sam potrojny straznik co w `.rig` i z tego samego powodu: rozmiar lapie
// pole dodane do struktury i niedopisane do tabeli, mapa pokrycia lapie pole
// USUNIETE z tabeli albo wpis o zlym offsecie (czego rozmiar nie widzi), a
// przebieg tam i z powrotem lapie zla dokladnosc albo zly typ.
//
// Gdy ktoras liczba przestanie sie zgadzac: NAJPIERW dopisz pole do s_qcFields,
// dopiero potem zaktualizuj liczbe. Odwrotna kolejnosc kasuje calego straznika.
#define JOZZ_QC_CONFIG_SIZEOF 176
#define JOZZ_QC_CONFIG_PADDING 8 // jedna dziura 4 B przed pierwszym double + ogon

int JozzQc_ConfigSelfTest( char* err, size_t errCap )
{
	JozzQcConfig a = JozzQc_DefaultConfig();
	JozzQcConfig b;
	char textA[JOZZ_QC_CONFIG_TEXT_CAP];
	char textB[JOZZ_QC_CONFIG_TEXT_CAP];
	char e2[256];
	unsigned char cover[JOZZ_QC_CONFIG_SIZEOF];
	int i, uncovered = 0;

	if ( err && errCap )
		err[0] = '\0';

	if ( sizeof( JozzQcConfig ) != JOZZ_QC_CONFIG_SIZEOF )
	{
		snprintf( err, errCap,
				  "rozmiar JozzQcConfig to %d, audytowany %d - pole doszlo albo zniknelo; "
				  "sprawdz, czy jest w s_qcFields",
				  (int)sizeof( JozzQcConfig ), JOZZ_QC_CONFIG_SIZEOF );
		return 0;
	}

	memset( cover, 0, sizeof( cover ) );
	for ( i = 0; i < s_qcFieldCount; ++i )
	{
		size_t n = JqFieldSize( s_qcFields[i].kind );
		size_t j;
		for ( j = 0; j < n; ++j )
		{
			size_t at = s_qcFields[i].off + j;
			if ( at >= sizeof( cover ) )
			{
				snprintf( err, errCap, "pole '%s' wystaje poza strukture", s_qcFields[i].key );
				return 0;
			}
			if ( cover[at] )
			{
				snprintf( err, errCap, "pole '%s' nachodzi na inne (bajt %d)", s_qcFields[i].key, (int)at );
				return 0;
			}
			cover[at] = 1;
		}
	}
	for ( i = 0; i < (int)sizeof( cover ); ++i )
		uncovered += cover[i] ? 0 : 1;
	if ( uncovered != JOZZ_QC_CONFIG_PADDING )
	{
		snprintf( err, errCap,
				  "tabela opisuje %d z %d bajtow struktury (nieopisane %d, wyrownanie %d) - "
				  "ktores pole nie ma wpisu w s_qcFields",
				  (int)sizeof( cover ) - uncovered, (int)sizeof( cover ), uncovered, JOZZ_QC_CONFIG_PADDING );
		return 0;
	}

	// Kazde pole dostaje WLASNA wartosc, rozna od domyslnej i rozna od sasiadow:
	// przy dwoch rownych wartosciach zamiana pol miejscami przeszlaby niezauwazona.
	for ( i = 0; i < s_qcFieldCount; ++i )
	{
		char* p = (char*)&a + s_qcFields[i].off;
		switch ( s_qcFields[i].kind )
		{
			case JQ_F_INT:
				*(int*)p = 3 + i * 7;
				break;
			case JQ_F_FLOAT:
				*(float*)p = (float)( i + 1 ) * 1.2345678f;
				break;
			case JQ_F_DOUBLE:
				*(double*)p = (double)( i + 1 ) * 1.23456789012345678;
				break;
			case JQ_F_VARIANT:
				*(JozzRigVariant*)p = JOZZ_RIG_TORUS;
				break;
			case JQ_F_ROAD:
				*(JozzQcRoad*)p = JOZZ_QC_ROAD_COMB;
				break;
			case JQ_F_DRIVE:
				*(JozzQcDrive*)p = JOZZ_QC_DRIVE_COAST;
				break;
		}
	}

	if ( JozzQc_ConfigToText( &a, textA, sizeof( textA ), "self-test" ) <= 0 )
	{
		snprintf( err, errCap, "zapis do tekstu nieudany (bufor?)" );
		return 0;
	}
	if ( JozzQc_ConfigFromText( &b, textA, e2, sizeof( e2 ) ) == 0 )
	{
		snprintf( err, errCap, "odczyt wlasnego zapisu nieudany: %s", e2 );
		return 0;
	}
	for ( i = 0; i < s_qcFieldCount; ++i )
	{
		const char* pa = (const char*)&a + s_qcFields[i].off;
		const char* pb = (const char*)&b + s_qcFields[i].off;
		// Porownanie POLAMI: bajty wyrownania nigdy nie przechodza przez plik.
		if ( memcmp( pa, pb, JqFieldSize( s_qcFields[i].kind ) ) != 0 )
		{
			snprintf( err, errCap, "pole '%s' nie przezylo zapisu i odczytu", s_qcFields[i].key );
			return 0;
		}
	}
	if ( JozzQc_ConfigToText( &b, textB, sizeof( textB ), "self-test" ) <= 0 || strcmp( textA, textB ) != 0 )
	{
		snprintf( err, errCap, "ponowny zapis daje inny tekst - format nie jest stabilny" );
		return 0;
	}

	// Odrzucenia. Kazde to konkretny sposob, w jaki plik moglby KLAMAC.
	if ( JozzQc_ConfigFromText( &b, "format 1\nsegmenty 7\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "nieznany klucz zostal przyjety" );
		return 0;
	}
	if ( JozzQc_ConfigFromText( &b, "dt 0.01\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "plik bez linii 'format' zostal przyjety" );
		return 0;
	}
	if ( JozzQc_ConfigFromText( &b, "format 99\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "obca wersja formatu zostala przyjeta" );
		return 0;
	}
	if ( JozzQc_ConfigFromText( &b, "format 1\ndt szybko\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "wartosc nieliczbowa zostala przyjeta" );
		return 0;
	}
	if ( JozzQc_ConfigFromText( &b, "format 1\nroad kostka\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "nieznana droga zostala przyjeta" );
		return 0;
	}
	if ( JozzQc_ConfigFromText( &b, "format 1\ndrive turbo\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "nieznany naped zostal przyjety" );
		return 0;
	}

	// Plik niepelny jest LEGALNY i domyka sie domyslnymi. Bez tego kazdy zapisany
	// przez Ownera plik przestawalby dzialac przy najblizszym nowym polu.
	if ( JozzQc_ConfigFromText( &b, "format 1\nsegments 9\n", e2, sizeof( e2 ) ) == 0 )
	{
		snprintf( err, errCap, "plik czesciowy odrzucony: %s", e2 );
		return 0;
	}
	{
		JozzQcConfig d = JozzQc_DefaultConfig();
		if ( b.segments != 9 || b.dt != d.dt || b.sprungKg != d.sprungKg || b.road != d.road )
		{
			snprintf( err, errCap, "plik czesciowy nie domknal sie domyslnymi" );
			return 0;
		}
	}

	// Dluga notatka z polki: znacznik czasu, tekst Ownera, krok i klasa sesji to
	// grubo ponad 255 znakow. Gdy limit dlugosci linii obejmowal takze komentarze,
	// narzedzie zapisywalo pliki, ktorych samo nie potrafilo wczytac.
	{
		char big[JOZZ_QC_CONFIG_TEXT_CAP];
		char note[600];
		memset( note, 'x', sizeof( note ) - 1 );
		note[sizeof( note ) - 1] = '\0';
		if ( JozzQc_ConfigToText( &a, big, sizeof( big ), note ) <= 0 )
		{
			snprintf( err, errCap, "zapis z dluga notatka nieudany" );
			return 0;
		}
		if ( JozzQc_ConfigFromText( &b, big, e2, sizeof( e2 ) ) == 0 )
		{
			snprintf( err, errCap, "wlasny plik z dluga notatka odrzucony: %s", e2 );
			return 0;
		}
	}
	return 1;
}

// ---------------------------------------------------------------- grupy pol
//
// Grupa jest zapisana w tabeli pol tylko przy PIERWSZYM polu sekcji (`group`
// niepusty zaczyna nowa sekcje w zapisie), wiec przynaleznosc trzeba przeniesc w
// dol. Ta sama petla obsluguje przywracanie i liczenie zmian, zeby nie dalo sie
// przywrocic pola, ktorego licznik nie widzi.

static int JqcWalkGroup( JozzQcConfig* c, const JozzQcConfig* ref, const char* group, int write )
{
	const char* cur = NULL;
	int hits = 0;
	int i;
	for ( i = 0; i < s_qcFieldCount; ++i )
	{
		if ( s_qcFields[i].group )
			cur = s_qcFields[i].group;
		if ( group && ( cur == NULL || strcmp( cur, group ) != 0 ) )
			continue;

		{
			char* dst = (char*)c + s_qcFields[i].off;
			const char* src = (const char*)ref + s_qcFields[i].off;
			size_t n = JqFieldSize( s_qcFields[i].kind );
			if ( memcmp( dst, src, n ) == 0 )
				continue;
			hits += 1;
			if ( write )
				memcpy( dst, src, n );
		}
	}
	return hits;
}

int JozzQc_ResetGroup( JozzQcConfig* c, const char* group )
{
	JozzQcConfig d = JozzQc_DefaultConfig();
	return JqcWalkGroup( c, &d, group, 1 );
}

int JozzQc_ChangedInGroup( const JozzQcConfig* c, const char* group )
{
	JozzQcConfig d = JozzQc_DefaultConfig();
	JozzQcConfig tmp = *c;
	return JqcWalkGroup( &tmp, &d, group, 0 );
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

// Kategoria masy resorowanej. Nadwozie ma z niczym nie kolidowac, ale MA byc
// widoczne i MA dawac sie chwycic - a to trzy rozne testy, ktore wczesniej
// rozjezdzaly sie po cichu:
//
//   rysowanie      b3World_Draw filtruje po `categoryBits`
//   kolizja        (A.kat & B.maska) I (B.kat & A.maska)   (src/shape.h:145)
//   promien myszy  ten SAM predykat, z domyslnym filtrem zapytania
//
// Poprzednia wersja zerowala `maskBits` nadwozia. Kolizji faktycznie nie bylo i
// nadwozie bylo widoczne - ale zerowa maska przewraca takze druga polowe
// predykatu zapytania, wiec b3World_CastRayClosest NIGDY nie trafial w mase
// resorowana. Ctrl+przeciagniecie nie mialo wiec czego zlapac poza kolem, a kolo
// w trakcie jazdy ucieka spod kursora - stad "chwytanie dziala bardzo slabo".
//
// Teraz nadwozie ma WLASNA kategorie i pelna maske, a grunt i progi maja te
// kategorie wyciete ze swoich masek. Kolizji z gruntem nadal nie ma (pierwsza
// polowa predykatu wychodzi zerem), z kolem tez nie - bo wiez ma
// collideConnected = false. Kolo i grunt widza sie nawzajem tak samo jak
// wczesniej, wiec zachowanie przebiegu jest nietkniete.
#define JQC_CHASSIS_CATEGORY ( (uint64_t)1 << 63 )
#define JQC_WORLD_MASK ( B3_DEFAULT_MASK_BITS & ~JQC_CHASSIS_CATEGORY )

static int JqcBuildRoad( JozzQcRig* r )
{
	const JozzQcConfig* c = &r->cfg;
	if ( c->road == JOZZ_QC_ROAD_FLAT )
		return 0;

	b3ShapeDef sd = b3DefaultShapeDef();
	sd.filter.maskBits = JQC_WORLD_MASK;
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
		gs.filter.maskBits = JQC_WORLD_MASK;
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
		// Wlasna kategoria zamiast zerowej maski - powod przy JQC_CHASSIS_CATEGORY.
		// W skrocie: kategoria trzyma nadwozie widoczne dla b3World_Draw I dla
		// promienia myszy, a wyciecie jej z masek gruntu i progow zalatwia brak
		// kolizji. Zerowa maska zalatwiala kolizje i po cichu zabierala chwyt.
		b3ShapeDef cs = b3DefaultShapeDef();
		cs.filter.categoryBits = JQC_CHASSIS_CATEGORY;
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

// ---------------------------------------------------------- strojenie na zywo

// Granice skoku ida za ugieciem statycznym, a to za sztywnoscia. Jedna funkcja
// dla obu, bo rozdzielenie ich dawalo zawieszenie, ktore po zmiekczeniu sprezyny
// stalo na zderzaku bez zadnego komunikatu.
static void JqcApplyLimits( JozzQcRig* rig )
{
	double sag = JozzQc_StaticSag( &rig->cfg );
	rig->travelUpper = sag + (double)rig->cfg.bumpTravel;
	rig->travelLower = sag - (double)rig->cfg.droopTravel;
	b3WheelJoint_SetSuspensionLimits( rig->joint, (float)rig->travelLower, (float)rig->travelUpper );
}

void JozzQc_SetSuspension( JozzQcRig* rig, float springNPerM, float zeta )
{
	if ( B3_IS_NULL( rig->joint ) || springNPerM <= 0.0f )
		return;
	rig->cfg.springNPerM = springNPerM;
	rig->cfg.zeta = zeta < 0.0f ? 0.0f : zeta;
	rig->builtHertz = JozzQc_Hertz( &rig->cfg );
	b3WheelJoint_SetSuspensionHertz( rig->joint, (float)rig->builtHertz );
	b3WheelJoint_SetSuspensionDampingRatio( rig->joint, rig->cfg.zeta );
	JqcApplyLimits( rig );
}

void JozzQc_SetTravel( JozzQcRig* rig, float bumpTravel, float droopTravel )
{
	if ( B3_IS_NULL( rig->joint ) || bumpTravel <= 0.0f || droopTravel <= 0.0f )
		return;
	rig->cfg.bumpTravel = bumpTravel;
	rig->cfg.droopTravel = droopTravel;
	JqcApplyLimits( rig );
}

void JozzQc_SetDrive( JozzQcRig* rig, JozzQcDrive mode, double targetSpeed, double constTorque )
{
	if ( rig->cfg.drive != mode )
		rig->integral = 0.0; // inaczej powrot do trybu predkosci startuje z windupem
	rig->cfg.drive = mode;
	rig->cfg.targetSpeed = targetSpeed;
	rig->cfg.constTorque = constTorque;
}

void JozzQc_SetDriveGains( JozzQcRig* rig, double kp, double ki, double maxTorque )
{
	rig->cfg.kp = kp;
	rig->cfg.ki = ki;
	rig->cfg.maxTorque = maxTorque > 0.0 ? maxTorque : rig->cfg.maxTorque;
	rig->integral = 0.0;
}

// ------------------------------------------------------------------------ reka

void JozzQc_LiftCorner( JozzQcRig* rig, double meters )
{
	b3Pos pc, pw;
	b3Vec3 vc, vw;
	if ( B3_IS_NULL( rig->world ) || meters == 0.0 )
		return;

	pc = b3Body_GetPosition( rig->chassis );
	pw = b3Body_GetPosition( rig->wheel );
	pc.y += (float)meters;
	pw.y += (float)meters;
	b3Body_SetTransform( rig->chassis, pc, b3Body_GetRotation( rig->chassis ) );
	b3Body_SetTransform( rig->wheel, pw, b3Body_GetRotation( rig->wheel ) );

	// Predkosc pionowa zerowana, wzdluzna NIE. Podniesienie ma byc podniesieniem,
	// a nie takze zatrzymaniem - jesli rig jedzie, prowadzenie kola nad droga i
	// puszczenie go w wybranym miejscu jest wlasnie ta proba, ktorej sie szuka.
	vc = b3Body_GetLinearVelocity( rig->chassis );
	vw = b3Body_GetLinearVelocity( rig->wheel );
	vc.y = 0.0f;
	vw.y = 0.0f;
	b3Body_SetLinearVelocity( rig->chassis, vc );
	b3Body_SetLinearVelocity( rig->wheel, vw );

	// Predkosc pionowa nadwozia z POPRZEDNIEGO kroku idzie do przyspieszenia w
	// nastepnym. Bez tego skok pozycji zapisalby sie jako jedno gigantyczne
	// a_pion, ktore nie odpowiada zadnej sile - i wykres klamalby o udarze.
	rig->prevSprungVy = 0.0;

	JozzQc_MarkPerturbation( rig, "narożnik podniesiony" );
}

void JozzQc_KickChassis( JozzQcRig* rig, double impulseNs )
{
	b3Vec3 j;
	if ( B3_IS_NULL( rig->world ) || impulseNs == 0.0 )
		return;
	j.x = 0.0f;
	j.y = (float)impulseNs;
	j.z = 0.0f;
	b3Body_ApplyLinearImpulseToCenter( rig->chassis, j, true );
	JozzQc_MarkPerturbation( rig, impulseNs > 0.0 ? "podbicie nadwozia" : "uderzenie w nadwozie" );
}

void JozzQc_SetShaker( JozzQcRig* rig, int on, double hz, double amplitudeN )
{
	rig->shakerHz = hz > 0.0 ? hz : 0.0;
	rig->shakerN = amplitudeN;
	if ( on && rig->shakerOn == 0 )
		JozzQc_MarkPerturbation( rig, "wstrzasarka" );
	rig->shakerOn = on ? 1 : 0;
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

	// 3b) wstrzasarka. Zawsze WYLACZONA w przebiegu pomiarowym - nic jej nie
	// wlacza poza reka Ownera w oknie - wiec ta galaz nie dotyka ani jednego
	// przebiegu headless. Faza liczona z numeru kroku, nie z zegara: to samo
	// wymuszenie ma wyjsc przy kazdym tempie odtwarzania.
	if ( rig->shakerOn && rig->shakerN != 0.0 )
	{
		b3Vec3 f;
		f.x = 0.0f;
		f.y = (float)( rig->shakerN * sin( 2.0 * JQC_PI * rig->shakerHz * (double)rig->step * dt ) );
		f.z = 0.0f;
		b3Body_ApplyForceToCenter( rig->chassis, f, true );
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

// ---------------------------------------------------------- protokol pomiaru

const double JozzQc_StartJitter[JOZZ_QC_REPEATS] = { 0.0, 0.0137, 0.0271 };

JozzQcRunResult JozzQc_MeasureOne( const JozzQcConfig* cfg, int warmup, int windowSteps, JozzQcSample* traceOut,
								   int traceCap )
{
	JozzQcRunResult r;
	JozzQcRig rig;
	JozzQcSample s;
	double t0;
	int i;

	memset( &r, 0, sizeof( r ) );
	JozzQc_WindowBegin( &r.win );
	if ( warmup < 0 )
		warmup = 0;
	if ( windowSteps < 1 )
		windowSteps = 1;

	if ( JozzQc_Create( &rig, cfg, NULL, r.err, sizeof( r.err ) ) == 0 )
		return r;
	r.built = 1;
	r.shapes = rig.shapeCount;
	{
		JozzRigConfig w = JozzQc_WheelConfig( cfg );
		r.ripple = JozzRig_EnvelopeRipple( &w );
	}

	for ( i = 0; i < warmup; ++i )
		JozzQc_Step( &rig, &s );

	t0 = JqcNowMs();
	for ( i = 0; i < windowSteps; ++i )
	{
		JozzQc_Step( &rig, &s );
		JozzQc_WindowAdd( &r.win, &s );
		if ( traceOut && r.traceCount < traceCap )
			traceOut[r.traceCount++] = s;
	}
	r.msPerStep = ( JqcNowMs() - t0 ) / (double)windowSteps;

	JozzQc_WindowEnd( &r.win, &rig );
	JozzQc_Destroy( &rig );
	return r;
}

static void JqcAccumulate( double v, double* mean, double* lo, double* hi, int first )
{
	*mean += v;
	if ( first || v < *lo )
		*lo = v;
	if ( first || v > *hi )
		*hi = v;
}

JozzQcCell JozzQc_MeasureCell( const JozzQcConfig* base, int warmup, int windowSteps )
{
	JozzQcCell c;
	double tLo = 0, tHi = 0, aLo = 0, aHi = 0, bLo = 0, bHi = 0, n;
	int i;

	memset( &c, 0, sizeof( c ) );
	for ( i = 0; i < JOZZ_QC_REPEATS; ++i )
	{
		JozzQcConfig cfg = *base;
		JozzQcRunResult r;
		cfg.startX += JozzQc_StartJitter[i];
		r = JozzQc_MeasureOne( &cfg, warmup, windowSteps, NULL, 0 );
		if ( !r.built )
		{
			snprintf( c.err, sizeof( c.err ), "%s", r.err );
			return c;
		}
		c.built = 1;
		c.shapes = r.shapes;
		c.ripple = r.ripple;
		JqcAccumulate( r.win.driveTorqueMean, &c.torque, &tLo, &tHi, i == 0 );
		JqcAccumulate( r.win.sprungAccelRms, &c.accel, &aLo, &aHi, i == 0 );
		JqcAccumulate( r.win.airborneFraction, &c.airborne, &bLo, &bHi, i == 0 );
		c.loss += r.win.lossPower;
		c.travelRms += r.win.travelRms;
		c.churn += r.win.contactChurnPct;
		c.slip += r.win.slipRatioMean;
		c.speed += r.win.speedMean;
		c.msPerStep += r.msPerStep;
		c.repeats += 1;
		if ( r.win.invalid && c.invalid == 0 )
		{
			c.invalid = 1;
			snprintf( c.why, sizeof( c.why ), "%s", r.win.invalidWhy );
		}
	}

	n = (double)c.repeats;
	c.torque /= n, c.accel /= n, c.airborne /= n;
	c.loss /= n, c.travelRms /= n, c.churn /= n, c.slip /= n, c.speed /= n, c.msPerStep /= n;
	c.torqueSpread = 0.5 * ( tHi - tLo );
	c.accelSpread = 0.5 * ( aHi - aLo );
	c.airborneSpread = 0.5 * ( bHi - bLo );
	return c;
}

int JozzQc_Candidates( JozzQcCandidate* out, int cap )
{
	int nMax = JozzRig_ProbeMaxPrismSides();
	int n = 0;
	if ( n < cap )
	{
		out[n].label = "sphere";
		out[n].variant = JOZZ_RIG_SPHERE;
		out[n].segments = 0;
		++n;
	}
	if ( n < cap )
	{
		out[n].label = "prism-Nmax";
		out[n].variant = JOZZ_RIG_PRISM_MAX;
		out[n].segments = nMax;
		++n;
	}
	if ( n < cap )
	{
		out[n].label = "prism-32";
		out[n].variant = JOZZ_RIG_PRISM_MAX;
		out[n].segments = 32;
		++n;
	}
	if ( n < cap )
	{
		out[n].label = "torus-32";
		out[n].variant = JOZZ_RIG_TORUS;
		out[n].segments = 32;
		++n;
	}
	if ( n < cap )
	{
		out[n].label = "torus-64";
		out[n].variant = JOZZ_RIG_TORUS;
		out[n].segments = 64;
		++n;
	}
	return n;
}
