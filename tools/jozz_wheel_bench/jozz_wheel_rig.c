// Wspolny rig kola - implementacja. Patrz jozz_wheel_rig.h.
//
// Kazda linia, ktora dotyka swiata, ciala albo regulatora, jest TUTAJ. Ani
// wheel_bench.c, ani sample nie tworza swiata rigu i nie licza sily napedu.

#include "jozz_wheel_rig.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* JozzRig_VariantName( JozzRigVariant v )
{
	if ( v == JOZZ_RIG_SPHERE )
		return "sphere";
	if ( v == JOZZ_RIG_PRISM_MAX )
		return "prism-Nmax";
	if ( v == JOZZ_RIG_TORUS )
		return "torus-N";
	return "?";
}

// ---------------------------------------------------------------- konfiguracja

JozzRigConfig JozzRig_DefaultConfig( void )
{
	JozzRigConfig c;
	memset( &c, 0, sizeof( c ) );

	c.variant = JOZZ_RIG_SPHERE;
	c.prismSides = 42; // nadpisywane pomiarem zdolnosci buildu tam, gdzie trzeba
	c.wheelR = JOZZ_RIG_WHEEL_R;
	c.wheelW = JOZZ_RIG_WHEEL_W;
	c.massKg = JOZZ_RIG_UNSPRUNG_KG;
	c.inertiaSpinFactor = 0.70f;
	c.inertiaTransFactor = 0.55f;
	c.density = 77.0f;
	// 0.20 m: przy W = 0.4375 zostawia 37.5 mm plaskiej bieżni i daje najmniejsze
	// tetnienie, jakie ta szerokosc dopuszcza (crownR <= W/2). Wybor jest swiadomie
	// po stronie GLADKOSCI, bo dzisiejszy kontakt to Coulomb - szerokosc plaskiego
	// styku nie wplywa na tarcie, dopoki nie ma prawa opony (W3).
	c.crownR = 0.20f;
	// Jeden rzad i plaska bieznia: DOKLADNIE bryla, na ktorej stoi cala
	// dotychczasowa tabela Q3 i blokada zachowania. Wysklepienie jest rozszerzeniem,
	// wiec musi byc wylaczone tam, gdzie nikt o nie nie poprosil.
	c.crownRows = 1;
	c.crownDrop = 0.0f;

	c.groundHalfX = 400.0f;
	c.groundHalfY = 1.0f;
	c.groundHalfZ = 60.0f;
	c.friction = JOZZ_RIG_FRICTION;
	c.gravity = JOZZ_RIG_GRAVITY;

	c.startX = JOZZ_RIG_START_X;
	c.startSpeed = JOZZ_RIG_TARGET_V;
	c.startGap = 0.001f;

	c.loadN = JOZZ_RIG_LOAD_N;
	c.controllerEnabled = 1;
	c.targetSpeed = JOZZ_RIG_TARGET_V;
	c.kp = JOZZ_RIG_KP;
	c.ki = JOZZ_RIG_KI;
	c.fmax = JOZZ_RIG_FMAX;

	c.dt = JOZZ_RIG_DT;
	c.substeps = JOZZ_RIG_SUBSTEPS;
	return c;
}

void JozzRig_ConfigDigest( const JozzRigConfig* c, char* out, size_t cap )
{
	if ( out == NULL || cap == 0 )
		return;
	// `crown=` pojawia sie WYLACZNIE dla wariantu, ktory go uzywa. Nie jest to
	// kosmetyka: odcisk konfiguracji idzie do naglowka wzorca zachowania, wiec
	// bezwarunkowe dopisanie pola oznaczaloby czerwona blokade na sphere i prism
	// - czyli sygnal "eksperyment sie zmienil" tam, gdzie nie zmienilo sie nic.
	// Z tego samego powodu `rows=`/`drop=` dopisuja sie dopiero przy wiecej niz
	// jednym rzedzie: konstrukcja z jednym rzedem to bryla sprzed wysklepienia i
	// jej odcisk ma zostac ten sam co do znaku.
	char crown[96];
	crown[0] = '\0';
	if ( c->variant == JOZZ_RIG_TORUS )
	{
		int used = snprintf( crown, sizeof( crown ), " crown=%.9g", (double)c->crownR );
		if ( c->crownRows > 1 && used > 0 && (size_t)used < sizeof( crown ) )
			snprintf( crown + used, sizeof( crown ) - (size_t)used, " rows=%d drop=%.9g", c->crownRows,
					  (double)c->crownDrop );
	}
	snprintf( out, cap,
			  "v=%s N=%d R=%.9g W=%.9g m=%.9g iS=%.9g iT=%.9g rho=%.9g "
			  "gnd=%.9g/%.9g/%.9g mu=%.9g g=%.17g "
			  "x0=%.17g v0=%.17g gap=%.9g "
			  "load=%.17g ctl=%d tgt=%.17g kp=%.17g ki=%.17g fmax=%.17g "
			  "dt=%.17g sub=%d%s",
			  JozzRig_VariantName( c->variant ), c->variant == JOZZ_RIG_SPHERE ? 0 : c->prismSides,
			  (double)c->wheelR, (double)c->wheelW, (double)c->massKg, (double)c->inertiaSpinFactor,
			  (double)c->inertiaTransFactor, (double)c->density, (double)c->groundHalfX, (double)c->groundHalfY,
			  (double)c->groundHalfZ, (double)c->friction, c->gravity, c->startX, c->startSpeed,
			  (double)c->startGap, c->loadN, c->controllerEnabled, c->targetSpeed, c->kp, c->ki, c->fmax, c->dt,
			  c->substeps, crown );
}

// ------------------------------------------------- konfiguracja jako plik
//
// Powod istnienia: dopoki konstrukcja zyla wylacznie w polach okna, "ciekawy
// przypadek" byl stanem ulotnym - Owner mogl go zobaczyc, ale nie mogl go ani
// odlozyc, ani podac dalej. Konfiguracja zapisana jako tekst jest jednoczesnie
// notatka, wejsciem stendu i diffem w gicie.
//
// Jedna TABELA pol obsluguje zapis i odczyt. To nie jest oszczednosc kodu, tylko
// zamkniecie konkretnej dziury: przy dwoch osobnych funkcjach dolozenie pola do
// JozzRigConfig i zapomnienie o nim po jednej ze stron daje plik, ktory CICHO
// gubi czesc tozsamosci przebiegu - a taki plik klamie tym mocniej, im bardziej
// wyglada na kompletny.

typedef enum
{
	JR_F_INT,
	JR_F_FLOAT,
	JR_F_DOUBLE,
	JR_F_VARIANT
} JrFieldKind;

typedef struct
{
	const char* key;
	JrFieldKind kind;
	size_t off;
	const char* group; // niepusty zaczyna nowa sekcje w zapisie
} JrField;

#define JR_OFF( f ) offsetof( JozzRigConfig, f )

static const JrField s_configFields[] = {
	{ "variant", JR_F_VARIANT, JR_OFF( variant ), "geometria i masa" },
	{ "prism_sides", JR_F_INT, JR_OFF( prismSides ), NULL },
	{ "wheel_r", JR_F_FLOAT, JR_OFF( wheelR ), NULL },
	{ "wheel_w", JR_F_FLOAT, JR_OFF( wheelW ), NULL },
	{ "mass_kg", JR_F_FLOAT, JR_OFF( massKg ), NULL },
	{ "inertia_spin", JR_F_FLOAT, JR_OFF( inertiaSpinFactor ), NULL },
	{ "inertia_trans", JR_F_FLOAT, JR_OFF( inertiaTransFactor ), NULL },
	{ "density", JR_F_FLOAT, JR_OFF( density ), NULL },
	{ "crown_r", JR_F_FLOAT, JR_OFF( crownR ), NULL },
	{ "crown_rows", JR_F_INT, JR_OFF( crownRows ), NULL },
	{ "crown_drop", JR_F_FLOAT, JR_OFF( crownDrop ), NULL },

	{ "ground_half_x", JR_F_FLOAT, JR_OFF( groundHalfX ), "scena" },
	{ "ground_half_y", JR_F_FLOAT, JR_OFF( groundHalfY ), NULL },
	{ "ground_half_z", JR_F_FLOAT, JR_OFF( groundHalfZ ), NULL },
	{ "friction", JR_F_FLOAT, JR_OFF( friction ), NULL },
	{ "gravity", JR_F_DOUBLE, JR_OFF( gravity ), NULL },

	{ "start_x", JR_F_DOUBLE, JR_OFF( startX ), "stan poczatkowy" },
	{ "start_speed", JR_F_DOUBLE, JR_OFF( startSpeed ), NULL },
	{ "start_gap", JR_F_FLOAT, JR_OFF( startGap ), NULL },

	{ "load_n", JR_F_DOUBLE, JR_OFF( loadN ), "obciazenie i regulator" },
	{ "controller", JR_F_INT, JR_OFF( controllerEnabled ), NULL },
	{ "target_speed", JR_F_DOUBLE, JR_OFF( targetSpeed ), NULL },
	{ "kp", JR_F_DOUBLE, JR_OFF( kp ), NULL },
	{ "ki", JR_F_DOUBLE, JR_OFF( ki ), NULL },
	{ "fmax", JR_F_DOUBLE, JR_OFF( fmax ), NULL },

	{ "dt", JR_F_DOUBLE, JR_OFF( dt ), "instrument" },
	{ "substeps", JR_F_INT, JR_OFF( substeps ), NULL },
};

static const int s_configFieldCount = (int)( sizeof( s_configFields ) / sizeof( s_configFields[0] ) );

static size_t JrFieldSize( JrFieldKind k )
{
	switch ( k )
	{
		case JR_F_INT:
			return sizeof( int );
		case JR_F_FLOAT:
			return sizeof( float );
		case JR_F_DOUBLE:
			return sizeof( double );
		case JR_F_VARIANT:
			return sizeof( JozzRigVariant );
	}
	return 0;
}

// Liczba cyfr nie jest kosmetyka. %.9g odtwarza kazdy float, %.17g kazdy double.
// Przy mniejszej liczbie cyfr zapis-odczyt przesuwalby konfiguracje o ostatni
// bit, a wtedy plik z konstrukcja dawalby INNY przebieg niz ten, ktory Owner
// zapisal - i blokada zachowania zaczelaby migac bez powodu.
static void JrWriteValue( const JrField* f, const void* base, char* out, size_t cap )
{
	const char* p = (const char*)base + f->off;
	switch ( f->kind )
	{
		case JR_F_INT:
			snprintf( out, cap, "%d", *(const int*)p );
			break;
		case JR_F_FLOAT:
			snprintf( out, cap, "%.9g", (double)*(const float*)p );
			break;
		case JR_F_DOUBLE:
			snprintf( out, cap, "%.17g", *(const double*)p );
			break;
		case JR_F_VARIANT:
			snprintf( out, cap, "%s", JozzRig_VariantName( *(const JozzRigVariant*)p ) );
			break;
	}
}

static int JrParseValue( const JrField* f, void* base, const char* value, char* err, size_t errCap )
{
	char* p = (char*)base + f->off;
	char* end = NULL;
	switch ( f->kind )
	{
		case JR_F_INT:
		{
			long v = strtol( value, &end, 10 );
			if ( end == value )
				break;
			*(int*)p = (int)v;
			return 1;
		}
		case JR_F_FLOAT:
		{
			double v = strtod( value, &end );
			if ( end == value )
				break;
			*(float*)p = (float)v;
			return 1;
		}
		case JR_F_DOUBLE:
		{
			double v = strtod( value, &end );
			if ( end == value )
				break;
			*(double*)p = v;
			return 1;
		}
		case JR_F_VARIANT:
		{
			int i;
			for ( i = 0; i < JOZZ_RIG_VARIANT_COUNT; ++i )
			{
				if ( strcmp( value, JozzRig_VariantName( (JozzRigVariant)i ) ) == 0 )
				{
					*(JozzRigVariant*)p = (JozzRigVariant)i;
					return 1;
				}
			}
			snprintf( err, errCap, "nieznany wariant '%s'", value );
			return 0;
		}
	}
	snprintf( err, errCap, "pole '%s': '%s' nie jest liczba", f->key, value );
	return 0;
}

int JozzRig_ConfigToText( const JozzRigConfig* c, char* out, size_t cap, const char* note )
{
	size_t used = 0;
	int i;
	int n = snprintf( out, cap, "format %d\n", JOZZ_RIG_CONFIG_FORMAT );
	if ( n < 0 || (size_t)n >= cap )
		return 0;
	used = (size_t)n;

	if ( note && note[0] )
	{
		// Notatka idzie w komentarzu, wiec nie da sie jej pomylic z parametrem.
		// Znaki konca linii sa zamieniane na spacje: wielolinijkowy komentarz
		// bez prefiksu '#' w kazdej linii zamienilby sie przy odczycie w blad.
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

	for ( i = 0; i < s_configFieldCount; ++i )
	{
		char value[64];
		JrWriteValue( &s_configFields[i], c, value, sizeof( value ) );
		n = snprintf( out + used, cap - used, "%s%s%s%s %s\n",
					  s_configFields[i].group ? "\n# " : "", s_configFields[i].group ? s_configFields[i].group : "",
					  s_configFields[i].group ? "\n" : "", s_configFields[i].key, value );
		if ( n < 0 || (size_t)n >= cap - used )
			return 0;
		used += (size_t)n;
	}
	return (int)used;
}

// Znacznik kolejnosci bajtow z poczatku pliku. Notatnik i PowerShell dopisuja go
// przy kazdym zapisie w UTF-8, a bez tego pominiecia pierwszy klucz przestawal
// byc rozpoznawalny i plik odbijal sie komunikatem "nieznany klucz 'format'" -
// czyli nazwa klucza, ktory na ekranie wyglada dokladnie poprawnie. Format ma byc
// edytowalny reka, wiec musi znosic to, co robi zwykly edytor.
const char* JrSkipBom( const char* s )
{
	if ( s && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF )
		return s + 3;
	return s;
}

// Zakresy, ktorych tabela pol nie wyrazi, bo zaleza od INNYCH pol. Sprawdzane
// przy BUDOWIE, nie przy parsowaniu - dokladnie tam, gdzie juz stoi odrzucenie
// nieszczelnego pierscienia. Plik wolno wczytac, ale nie wolno po cichu
// uruchomic czegos innego, niz opisuje: wczesniej `crown_rows 20` budowalo
// dziewiec rzedow i podpisywalo przebieg jako dwadziescia.
//
// Format sprawdza samokontrola formatu; TO sprawdza, czy konstrukcja istnieje.
int JozzRig_ValidateCrown( const JozzRigConfig* c, char* err, size_t errCap )
{
	if ( c->crownRows < 1 || c->crownRows > JOZZ_RIG_CROWN_ROWS_MAX )
	{
		snprintf( err, errCap, "crown_rows %d poza zakresem 1..%d", c->crownRows, JOZZ_RIG_CROWN_ROWS_MAX );
		return 0;
	}
	if ( c->crownDrop < 0.0f )
	{
		snprintf( err, errCap, "crown_drop %.6g jest ujemny", (double)c->crownDrop );
		return 0;
	}
	if ( c->variant == JOZZ_RIG_TORUS && c->crownRows > 1 && c->crownDrop > 0.0f )
	{
		double a = 0.5 * (double)c->wheelW - (double)c->crownR;
		if ( (double)c->crownDrop > a * ( 1.0 + 1e-5 ) + 1e-9 )
		{
			snprintf( err, errCap, "crown_drop %.6g przekracza polowe biezni %.6g - luk przestaje byc profilem",
					  (double)c->crownDrop, a );
			return 0;
		}
	}
	return 1;
}

int JozzRig_ConfigFromText( JozzRigConfig* c, const char* text, char* err, size_t errCap )
{
	JozzRigConfig parsed = JozzRig_DefaultConfig();
	const char* p = JrSkipBom( text );
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

		// Komentarz i pusta linia odpadaja PRZED limitem dlugosci i bez kopiowania.
		// Kolejnosc jest istotna: notatka Ownera z polki potrafi miec kilkaset
		// znakow, wiec limit nalozony wczesniej odrzucalby wlasne pliki narzedzia
		// - zapis by sie udawal, a odczyt nie. Limit dotyczy tylko linii z danymi,
		// gdzie 255 znakow to i tak wielokrotnie za duzo.
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
			// obetnij bialy ogon, zeby strtod nie dostal '\r' z pliku CRLF
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
			if ( atoi( v ) != JOZZ_RIG_CONFIG_FORMAT )
			{
				snprintf( err, errCap, "format pliku %s, obslugiwany %d", v, JOZZ_RIG_CONFIG_FORMAT );
				return 0;
			}
			sawFormat = 1;
			continue;
		}

		for ( i = 0; i < s_configFieldCount; ++i )
		{
			if ( strcmp( key, s_configFields[i].key ) != 0 )
				continue;
			if ( JrParseValue( &s_configFields[i], &parsed, v, err, errCap ) == 0 )
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
		snprintf( err, errCap, "brak linii 'format' - to nie jest plik konstrukcji rigu" );
		return 0;
	}
	*c = parsed;
	return 1;
}

int JozzRig_ConfigWriteFile( const JozzRigConfig* c, const char* path, const char* note )
{
	char text[JOZZ_RIG_CONFIG_TEXT_CAP];
	FILE* f;
	int n = JozzRig_ConfigToText( c, text, sizeof( text ), note );
	if ( n <= 0 )
		return 0;
	f = fopen( path, "wb" );
	if ( f == NULL )
		return 0;
	fwrite( text, 1, (size_t)n, f );
	fclose( f );
	return 1;
}

// Trzy niezalezne mechanizmy, kazdy na inna usterke. Zaden sam nie wystarcza:
//
//   1. STRAZ ROZMIARU lapie pole dodane do JozzRigConfig i niedopisane do tabeli.
//   2. MAPA POKRYCIA lapie pole USUNIETE z tabeli, wpis wskazujacy zly offset i
//      dwa wpisy celujace w to samo miejsce. Sam rozmiar tego nie widzi: usuniecie
//      wiersza z tabeli nie zmienia struktury, a petla po tabeli nigdy nie dotknie
//      pola, ktorego w tabeli nie ma - i test przeszedlby na zielono.
//   3. PRZEBIEG TAM I Z POWROTEM lapie pole obecne w tabeli, ale zapisywane ze
//      zla dokladnoscia albo pod zlym typem.
//
// Obie liczby ponizej to ostatnio zaudytowany uklad struktury na x64. Gdy ktoras
// przestanie sie zgadzac: NAJPIERW dopisz pole do s_configFields, dopiero potem
// zaktualizuj liczby. Odwrotna kolejnosc kasuje calego straznika.
#define JOZZ_RIG_CONFIG_SIZEOF 160
#define JOZZ_RIG_CONFIG_PADDING 16 // bajty wyrownania: 4 dziury po 4 przed double

int JozzRig_ConfigSelfTest( char* err, size_t errCap )
{
	JozzRigConfig a = JozzRig_DefaultConfig();
	JozzRigConfig b;
	char textA[JOZZ_RIG_CONFIG_TEXT_CAP];
	char textB[JOZZ_RIG_CONFIG_TEXT_CAP];
	char e2[256];
	unsigned char cover[JOZZ_RIG_CONFIG_SIZEOF];
	int i, uncovered = 0;

	if ( err && errCap )
		err[0] = '\0';

	if ( sizeof( JozzRigConfig ) != JOZZ_RIG_CONFIG_SIZEOF )
	{
		snprintf( err, errCap,
				  "rozmiar JozzRigConfig to %d, audytowany %d - pole doszlo albo zniknelo; "
				  "sprawdz, czy jest w s_configFields",
				  (int)sizeof( JozzRigConfig ), JOZZ_RIG_CONFIG_SIZEOF );
		return 0;
	}

	memset( cover, 0, sizeof( cover ) );
	for ( i = 0; i < s_configFieldCount; ++i )
	{
		size_t n = JrFieldSize( s_configFields[i].kind );
		size_t j;
		for ( j = 0; j < n; ++j )
		{
			size_t at = s_configFields[i].off + j;
			if ( at >= sizeof( cover ) )
			{
				snprintf( err, errCap, "pole '%s' wystaje poza strukture", s_configFields[i].key );
				return 0;
			}
			if ( cover[at] )
			{
				snprintf( err, errCap, "pole '%s' nachodzi na inne (bajt %d)", s_configFields[i].key, (int)at );
				return 0;
			}
			cover[at] = 1;
		}
	}
	for ( i = 0; i < (int)sizeof( cover ); ++i )
		uncovered += cover[i] ? 0 : 1;
	if ( uncovered != JOZZ_RIG_CONFIG_PADDING )
	{
		snprintf( err, errCap,
				  "tabela opisuje %d z %d bajtow struktury (nieopisane %d, wyrownanie %d) - "
				  "ktores pole nie ma wpisu w s_configFields",
				  (int)sizeof( cover ) - uncovered, (int)sizeof( cover ), uncovered, JOZZ_RIG_CONFIG_PADDING );
		return 0;
	}

	// Kazde pole dostaje WLASNA wartosc, rozna od domyslnej i rozna od sasiadow.
	// Gdyby dwa pola mialy te sama wartosc, zamiana ich miejscami w zapisie albo
	// odczycie przeszlaby niezauwazona.
	for ( i = 0; i < s_configFieldCount; ++i )
	{
		char* p = (char*)&a + s_configFields[i].off;
		switch ( s_configFields[i].kind )
		{
			case JR_F_INT:
				*(int*)p = 3 + i * 7;
				break;
			case JR_F_FLOAT:
				*(float*)p = (float)( i + 1 ) * 1.2345678f;
				break;
			case JR_F_DOUBLE:
				*(double*)p = (double)( i + 1 ) * 1.23456789012345678;
				break;
			case JR_F_VARIANT:
				*(JozzRigVariant*)p = JOZZ_RIG_PRISM_MAX;
				break;
		}
	}

	if ( JozzRig_ConfigToText( &a, textA, sizeof( textA ), "self-test" ) <= 0 )
	{
		snprintf( err, errCap, "zapis do tekstu nieudany (bufor?)" );
		return 0;
	}
	if ( JozzRig_ConfigFromText( &b, textA, e2, sizeof( e2 ) ) == 0 )
	{
		snprintf( err, errCap, "odczyt wlasnego zapisu nieudany: %s", e2 );
		return 0;
	}

	for ( i = 0; i < s_configFieldCount; ++i )
	{
		const char* pa = (const char*)&a + s_configFields[i].off;
		const char* pb = (const char*)&b + s_configFields[i].off;
		size_t n = JrFieldSize( s_configFields[i].kind );
		// Porownanie POLAMI, nie calej struktury: bajty wyrownania nigdy nie
		// przechodza przez plik, wiec ich roznica bylaby falszywym alarmem.
		if ( memcmp( pa, pb, n ) != 0 )
		{
			snprintf( err, errCap, "pole '%s' nie przezylo zapisu i odczytu", s_configFields[i].key );
			return 0;
		}
	}

	if ( JozzRig_ConfigToText( &b, textB, sizeof( textB ), "self-test" ) <= 0 || strcmp( textA, textB ) != 0 )
	{
		snprintf( err, errCap, "ponowny zapis daje inny tekst - format nie jest stabilny" );
		return 0;
	}

	// Odrzucenia. Kazde z nich to konkretny sposob, w jaki plik moglby KLAMAC.
	if ( JozzRig_ConfigFromText( &b, "format 1\nscianek 7\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "nieznany klucz zostal przyjety" );
		return 0;
	}
	if ( JozzRig_ConfigFromText( &b, "dt 0.01\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "plik bez linii 'format' zostal przyjety" );
		return 0;
	}
	if ( JozzRig_ConfigFromText( &b, "format 99\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "obca wersja formatu zostala przyjeta" );
		return 0;
	}
	if ( JozzRig_ConfigFromText( &b, "format 1\ndt szybko\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "wartosc nieliczbowa zostala przyjeta" );
		return 0;
	}
	if ( JozzRig_ConfigFromText( &b, "format 1\nvariant kwadrat\n", e2, sizeof( e2 ) ) != 0 )
	{
		snprintf( err, errCap, "nieznany wariant zostal przyjety" );
		return 0;
	}

	// Plik niepelny jest LEGALNY i domyka sie domyslnymi. Bez tego kazdy zapisany
	// przez Ownera plik przestawalby dzialac przy najblizszym nowym polu.
	if ( JozzRig_ConfigFromText( &b, "format 1\nprism_sides 9\n", e2, sizeof( e2 ) ) == 0 )
	{
		snprintf( err, errCap, "plik czesciowy odrzucony: %s", e2 );
		return 0;
	}
	{
		JozzRigConfig d = JozzRig_DefaultConfig();
		if ( b.prismSides != 9 || b.dt != d.dt || b.loadN != d.loadN )
		{
			snprintf( err, errCap, "plik czesciowy nie domknal sie domyslnymi" );
			return 0;
		}
	}

	// Dluga notatka. Polka pisze do komentarza znacznik czasu, tekst Ownera, krok,
	// klase sesji i v_kryt - lacznie grubo ponad 255 znakow. Gdy limit dlugosci
	// linii obejmowal takze komentarze, narzedzie zapisywalo pliki, ktorych samo
	// nie potrafilo wczytac. Ten przypadek pilnuje, zeby to nie wrocilo.
	{
		char big[JOZZ_RIG_CONFIG_TEXT_CAP];
		char note[600];
		memset( note, 'x', sizeof( note ) - 1 );
		note[sizeof( note ) - 1] = '\0';
		if ( JozzRig_ConfigToText( &a, big, sizeof( big ), note ) <= 0 )
		{
			snprintf( err, errCap, "zapis z dluga notatka nie zmiescil sie w buforze" );
			return 0;
		}
		if ( JozzRig_ConfigFromText( &b, big, e2, sizeof( e2 ) ) == 0 )
		{
			snprintf( err, errCap, "plik z dluga notatka zostal odrzucony: %s", e2 );
			return 0;
		}
	}

	// Warstwa plikowa osobno od warstwy tekstowej. Bez tego caly test dotyczylby
	// bufora w pamieci, a Owner zapisuje na dysk - i to wlasnie ta droga (tryb
	// binarny, konce linii, domkniecie pliku) ma najwiecej sposobow na zawiedzenie.
	{
		const char* tmp = "jozz_rig_config_selftest.tmp";
		JozzRigConfig back;
		int ok;
		if ( JozzRig_ConfigWriteFile( &a, tmp, "self-test warstwy plikowej" ) == 0 )
		{
			snprintf( err, errCap, "zapis do pliku nieudany" );
			return 0;
		}
		ok = JozzRig_ConfigReadFile( &back, tmp, e2, sizeof( e2 ) );
		remove( tmp );
		if ( ok == 0 )
		{
			snprintf( err, errCap, "odczyt wlasnego pliku nieudany: %s", e2 );
			return 0;
		}
		for ( i = 0; i < s_configFieldCount; ++i )
		{
			if ( memcmp( (const char*)&a + s_configFields[i].off, (const char*)&back + s_configFields[i].off,
						 JrFieldSize( s_configFields[i].kind ) ) != 0 )
			{
				snprintf( err, errCap, "pole '%s' nie przezylo drogi przez dysk", s_configFields[i].key );
				return 0;
			}
		}
	}
	return 1;
}

int JozzRig_ConfigReadFile( JozzRigConfig* c, const char* path, char* err, size_t errCap )
{
	char text[JOZZ_RIG_CONFIG_TEXT_CAP];
	size_t n;
	FILE* f = fopen( path, "rb" );
	if ( f == NULL )
	{
		snprintf( err, errCap, "nie moge otworzyc '%s'", path );
		return 0;
	}
	n = fread( text, 1, sizeof( text ) - 1, f );
	fclose( f );
	text[n] = '\0';
	return JozzRig_ConfigFromText( c, text, err, errCap );
}

double JozzRig_CriticalSpeed( const JozzRigConfig* c )
{
	if ( c->massKg <= 0.0f || c->wheelR <= 0.0f || c->loadN <= 0.0 )
		return 0.0;
	double aEff = c->loadN / (double)c->massKg;
	return sqrt( aEff * (double)c->wheelR );
}

double JozzRig_FacetsPerStep( const JozzRigConfig* c, double speed )
{
	// Dotyczy kazdej obwiedni DZIELONEJ NA ELEMENTY, nie tylko pryzmatu: dla
	// `torus-N` element to kapsula i pytanie jest identyczne - ile ich mija punkt
	// styku miedzy dwiema aktualizacjami manifoldu.
	if ( c->variant == JOZZ_RIG_SPHERE || c->prismSides <= 0 || c->wheelR <= 0.0f || c->dt <= 0.0 )
		return 0.0;
	// obwod / liczba scianek = dlugosc cieciwy; ile ich mija w jednym kroku
	double chord = 2.0 * JOZZ_RIG_PI * (double)c->wheelR / (double)c->prismSides;
	return fabs( speed ) * c->dt / chord;
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
	return JozzRig_KinematicsR( body, JOZZ_RIG_WHEEL_R );
}

JozzWheelKin JozzRig_KinematicsR( b3BodyId body, float radius )
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
	r.x = -radius * up.x;
	r.y = -radius * up.y;
	r.z = -radius * up.z;
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
	JozzRig_FreezeMassEx( body, kg, JOZZ_RIG_WHEEL_R, 0.70f, 0.55f );
}

void JozzRig_FreezeMassEx( b3BodyId body, float kg, float radius, float spinFactor, float transFactor )
{
	b3MassData md = b3Body_GetMassData( body );
	md.mass = kg;
	md.center.x = 0.0f;
	md.center.y = 0.0f;
	md.center.z = 0.0f;
	float iSpin = spinFactor * kg * radius * radius;
	float iTr = transFactor * kg * radius * radius;
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
	return JozzRig_BuildEnvelopeEx( body, v, density, prismSides, JOZZ_RIG_WHEEL_R, JOZZ_RIG_WHEEL_W,
									JOZZ_RIG_FRICTION );
}

int JozzRig_BuildEnvelopeEx( b3BodyId body, JozzRigVariant v, float density, int prismSides, float radius,
							 float width, float friction )
{
	b3ShapeDef sd = b3DefaultShapeDef();
	sd.density = density;
	sd.baseMaterial.friction = friction;
	sd.baseMaterial.rollingResistance = 0.0f; // jawnie zero, nie odziedziczone
	static b3Vec3 pts[4096];

	if ( v == JOZZ_RIG_SPHERE )
	{
		b3Sphere s;
		s.center.x = 0.0f;
		s.center.y = 0.0f;
		s.center.z = 0.0f;
		s.radius = radius;
		b3CreateSphereShape( body, &sd, &s );
		return 1;
	}
	if ( v == JOZZ_RIG_PRISM_MAX )
	{
		int n = JozzRig_MakePrismPoints( pts, 4096, prismSides, radius, 0.5f * width );
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

// --------------------------------------------------------------- torus-N
//
// Geometria pierscienia kapsul, wypisana raz i uzywana przez wszystkie trzy
// miejsca, ktore o nia pytaja (budowa, warunek szczelnosci, tetnienie).
// Uklad lokalny kola jest ten sam co w JozzRig_MakePrismPoints: os obrotu to
// LOKALNE Y, obwod lezy w plaszczyznie X-Z.
//
//   ringR   = wheelR - crownR      promien, na ktorym siedza osie kapsul
//   halfLen = W/2 - crownR         polowa PLASKIEJ czesci bieżni
//
// Kapsula ma os rownolegla do osi kola, wiec w kierunku toczenia daje luk o
// promieniu crownR, a w poprzek - walec zakonczony czaszami. To jest dokladnie
// profil opony: plaska bieznia i zaokraglone barki.

typedef struct
{
	double ringR;
	double halfLen;
	double crownR;
	double drop; // zwis barku wzgledem srodka biezni; 0 = bieznia plaska
	int rows;
	int n;
	int valid; // 0 = wymiary same w sobie bez sensu (nie mylic ze szczelnoscia)
} JrTorusGeom;

static JrTorusGeom JrTorus( const JozzRigConfig* c )
{
	JrTorusGeom g;
	memset( &g, 0, sizeof( g ) );
	g.crownR = (double)c->crownR;
	g.ringR = (double)c->wheelR - g.crownR;
	g.halfLen = 0.5 * (double)c->wheelW - g.crownR;
	g.n = c->prismSides;
	// Zero i wartosci ujemne znacza "jeden rzad": konfiguracja wyzerowana
	// pamiecia ma dawac bryle sprzed tej zmiany, a nie zero ksztaltow.
	g.rows = c->crownRows > 1 ? c->crownRows : 1;
	g.drop = g.rows > 1 && c->crownDrop > 0.0f ? (double)c->crownDrop : 0.0;
	// Poza zakresem konstrukcja jest NIEPRZEDSTAWIALNA, a nie po cichu przycieta.
	// Wczesniej byla przycinana, a odcisk konfiguracji drukowal dalej wartosc
	// ZAMOWIONA - plik `.qc` z `crown_rows 20` budowal dziewiec rzedow i podpisywal
	// sie jako dwadziescia. Przyrzad, ktory sam sobie poprawia wejscie i nie mowi
	// o tym w swoim odcisku, klamie w rejestrze przebiegow.
	//
	// Zwis wiekszy od polowy biezni nie ma sensu geometrycznie: luk przestaje byc
	// funkcja y. Epsilon jest po to, ze suwak okna wylicza granice w `float`.
	if ( g.rows > JOZZ_RIG_CROWN_ROWS_MAX || g.drop > g.halfLen * ( 1.0 + 1e-5 ) + 1e-9 )
		g.valid = 0;
	else
	{
		if ( g.drop > g.halfLen )
			g.drop = g.halfLen;
		g.valid = ( g.crownR > 0.0 && g.ringR > 0.0 && g.halfLen >= 0.0 && g.n >= 3 );
	}
	return g;
}

// Zwis profilu w odleglosci `y` od srodka biezni. Luk kolowy przechodzacy przez
// (0, 0) i (+-a, drop), czyli dokladnie ten "crown radius", ktorym opisuje sie
// opone - tylko podany od strony, ktora ma skonczony zakres.
static double JrCrownSag( double a, double drop, double y )
{
	double rc;
	if ( drop <= 0.0 || a <= 0.0 )
		return 0.0;
	if ( y < 0.0 )
		y = -y;
	if ( y > a )
		y = a;
	rc = ( a * a + drop * drop ) / ( 2.0 * drop );
	return rc - sqrt( rc * rc - y * y );
}

// Rzedy dla ZADANEJ ich liczby. Podzial jest na `m` rownych pasm biezni, a os
// kapsuly kazdego pasma siedzi na promieniu profilu w SRODKU pasma - czyli
// schodek lezy w poprzek luku, a nie pod nim ani nad nim.
//
// Przy m == 1 wychodzi yCenter = 0 i halfLen = calej polowie biezni, czyli
// dokladnie jedna kapsula od bark do barku. To jest ta sama kapsula, ktora
// powstawala przed dolozeniem rzedow.
//
// SZCZYT jest normalizowany do zamowionego promienia. Powod jest zmierzony:
// przy PARZYSTEJ liczbie rzedow zaden rzad nie lezy na srodku biezni, wiec
// najwyzszy pasek siedzi juz na zwisie sag(a/m) i kolo wychodzilo CICHO mniejsze
// niz `wheelR` - przy m=2, a=0.169 i zwisie 20 mm bylo to 4.9 mm, czyli 1% na
// promieniu. Przemiatanie po liczbie rzedow mieszalo wtedy dwie rzeczy: wiernosc
// profilu i rozmiar opony. `wheelR` znaczy promien zewnetrzny i ma nim zostac,
// a zwis jest liczony OD SZCZYTU, ktory naprawde powstal.
//
// Przy nieparzystym m i przy m == 1 przesuniecie wychodzi dokladnie 0.0, wiec
// dotychczasowa bryla jest nietknieta co do bitu.
// Granica pasma nr t (t w [-1,1]). Pasma sa rowne po ZWISIE, a nie po szerokosci.
//
// Powod jest zmierzony. Blad schodka bierze sie z tego, o ile promien zmienia sie
// W OBREBIE jednego pasma, a luk korony jest przy barku kilkanascie razy bardziej
// stromy niz na srodku biezni: przy zwisie 80 mm nachylenie rosnie od zera na
// srodku do 0.95 przy barku. Rowne pasma po szerokosci marnowaly wiec rozdzielczosc
// tam, gdzie profil jest plaski, i oszczedzaly ja tam, gdzie sie lamie - dziewiec
// rzedow gubilo 18.1 mm profilu, czyli tyle, ile ma polowa badanego kamienia.
//
// Odwrocenie zwisu: sag(y) = Rc - sqrt(Rc^2 - y^2) = s  =>  y = sqrt(2*Rc*s - s^2).
// Przy t = 1 wychodzi dokladnie halfLen, wiec bieznia konczy sie tam, gdzie ma.
// Przy zwisie 0 luk jest plaski i podzial wraca do rownego po szerokosci - to jest
// ta sama bryla co przed dolozeniem profilu poprzecznego.
static double JrBandEdge( const JrTorusGeom* g, double t )
{
	double s, rc, y;
	if ( g->drop <= 0.0 )
		return g->halfLen * t;
	s = g->drop * ( t < 0.0 ? -t : t );
	rc = ( g->halfLen * g->halfLen + g->drop * g->drop ) / ( 2.0 * g->drop );
	y = 2.0 * rc * s - s * s;
	y = y > 0.0 ? sqrt( y ) : 0.0;
	return t < 0.0 ? -y : y;
}

static int JrMakeRows( const JrTorusGeom* g, int m, JozzRigTorusRow* out )
{
	double lift = 0.0;
	int j;
	if ( m < 1 || g->valid == 0 )
		return 0;
	for ( j = 0; j < m; ++j )
	{
		double lo = JrBandEdge( g, -1.0 + 2.0 * (double)j / (double)m );
		double hi = JrBandEdge( g, -1.0 + 2.0 * (double)( j + 1 ) / (double)m );
		double yc = 0.5 * ( lo + hi );
		double sag = JrCrownSag( g->halfLen, g->drop, yc );
		out[j].capR = g->crownR;
		out[j].halfLen = 0.5 * ( hi - lo );
		out[j].yCenter = yc;
		out[j].ringR = -sag;
		if ( j == 0 || sag < lift )
			lift = sag;
	}
	for ( j = 0; j < m; ++j )
		out[j].ringR += g->ringR + lift;
	return m;
}

// Obwiednia w miejscu `y`: kazdy rzad daje w tym przekroju okrag o promieniu
// zaleznym od odleglosci od jego osi, a obwiednia to MAKSIMUM po rzedach.
// Ta sama funkcja liczy profil zbudowany (rzedy realne) i zamowiony (gesty
// podzial tego samego luku) - dzieki temu ich roznica nie moze byc artefaktem
// dwoch roznych wzorow.
static double JrRowsEnvelope( const JozzRigTorusRow* rows, int m, double y )
{
	double best = 0.0;
	int j;
	for ( j = 0; j < m; ++j )
	{
		double d = y - rows[j].yCenter;
		double over = ( d < 0.0 ? -d : d ) - rows[j].halfLen;
		double h;
		if ( over <= 0.0 )
			h = rows[j].capR;
		else if ( over < rows[j].capR )
			h = sqrt( rows[j].capR * rows[j].capR - over * over );
		else
			continue;
		if ( rows[j].ringR + h > best )
			best = rows[j].ringR + h;
	}
	return best;
}

// Gestosc "profilu zamowionego". Nie jest to liczba rzedow, ktore powstana -
// jest to luk, ktory rzedy MAJA przyblizyc.
#define JR_PROFILE_DENSE 96

int JozzRig_MinTorusSegments( const JozzRigConfig* c )
{
	if ( c->variant != JOZZ_RIG_TORUS )
		return 0;
	JrTorusGeom g = JrTorus( c );
	if ( !g.valid || g.crownR >= g.ringR )
		return 3;
	// Sasiednie kapsuly zachodza na siebie, gdy polowa rozstawu osi jest mniejsza
	// od promienia: ringR * sin(pi/N) <= crownR.
	double nMin = JOZZ_RIG_PI / asin( g.crownR / g.ringR );
	int n = (int)ceil( nMin - 1e-9 );
	return n < 3 ? 3 : n;
}

int JozzRig_TorusRows( const JozzRigConfig* c, JozzRigTorusRow* out, int cap )
{
	JrTorusGeom g;
	if ( c->variant != JOZZ_RIG_TORUS || out == NULL )
		return 0;
	g = JrTorus( c );
	if ( !g.valid || g.rows > cap )
		return 0;
	return JrMakeRows( &g, g.rows, out );
}

double JozzRig_CrownRadius( const JozzRigConfig* c )
{
	JrTorusGeom g = JrTorus( c );
	if ( !g.valid || g.drop <= 0.0 )
		return 0.0;
	return ( g.halfLen * g.halfLen + g.drop * g.drop ) / ( 2.0 * g.drop );
}

int JozzRig_ShapeCount( const JozzRigConfig* c )
{
	if ( c->variant != JOZZ_RIG_TORUS )
		return 1;
	{
		JrTorusGeom g = JrTorus( c );
		return g.valid ? g.n * g.rows : 0;
	}
}

double JozzRig_ProfileTarget( const JozzRigConfig* c, double y )
{
	JozzRigTorusRow rows[JR_PROFILE_DENSE];
	JrTorusGeom g = JrTorus( c );
	int m;
	if ( !g.valid )
		return 0.0;
	m = JrMakeRows( &g, JR_PROFILE_DENSE, rows );
	return JrRowsEnvelope( rows, m, y );
}

double JozzRig_ProfileBuilt( const JozzRigConfig* c, double y )
{
	JozzRigTorusRow rows[JOZZ_RIG_CROWN_ROWS_MAX];
	int m = JozzRig_TorusRows( c, rows, JOZZ_RIG_CROWN_ROWS_MAX );
	if ( m <= 0 )
		return 0.0;
	return JrRowsEnvelope( rows, m, y );
}

double JozzRig_ProfileError( const JozzRigConfig* c )
{
	JozzRigTorusRow built[JOZZ_RIG_CROWN_ROWS_MAX];
	JozzRigTorusRow want[JR_PROFILE_DENSE];
	JrTorusGeom g = JrTorus( c );
	double worst = 0.0;
	int mb, mw, i;
	if ( c->variant != JOZZ_RIG_TORUS || !g.valid )
		return 0.0;
	mb = JrMakeRows( &g, g.rows, built );
	mw = JrMakeRows( &g, JR_PROFILE_DENSE, want );
	if ( mb <= 0 || mw <= 0 )
		return 0.0;
	// Probkujemy CALA polowke przekroju, razem z barkiem: najwieksza odchylka
	// zwykle nie siedzi na srodku biezni, tylko przy ostatnim pasmie.
	//
	// Probki leza w SRODKACH przedzialow, nie na ich koncach. Powod jest zmierzony:
	// na samej krawedzi barku obwiednia jest NIECIAGLA - o wlos wewnatrz ma jeszcze
	// promien osi, o wlos na zewnatrz nie ma materialu i funkcja daje zero. Probka
	// postawiona dokladnie na krawedzi porownywala "krawedz" z "pustka" i dawala
	// odchylke 464 mm na kole o promieniu 514 mm. Ta sama pomylka rozjezdzala
	// odcisk obwiedni: rzedy 1/5/7 i 3/9 dostawaly rozne odciski przy IDENTYCZNEJ
	// bryle, wiec straznik "ta sama bryla" sam siebie oslepial.
	for ( i = 0; i < 200; ++i )
	{
		double y = ( g.halfLen + g.crownR ) * ( (double)i + 0.5 ) / 200.0;
		double d = JrRowsEnvelope( want, mw, y ) - JrRowsEnvelope( built, mb, y );
		if ( d < 0.0 )
			d = -d;
		if ( d > worst )
			worst = d;
	}
	return worst;
}

// --- odcisk OBWIEDNI ---------------------------------------------------------
// Powod istnienia jest zmierzony i kosztowal jeden bledny wniosek. Przemiatanie
// po liczbie rzedow zostalo wykonane przy zwisie 0 - a przy zwisie 0 wszystkie
// rzedy leza na tym samym promieniu, wiec ich UNIA to dokladnie ta sama kapsula
// co przy jednym rzedzie. Tabela pokazywala pieciokrotnie drozsze punkty o
// roznych liczbach i wygladala jak pomiar ksztaltu, a byla pomiarem NICZEGO.
//
// Odcisk liczy sie z POWIERZCHNI, nie z listy kapsul: dwie rozne konstrukcje o
// tej samej obwiedni maja ten sam odcisk, bo droga ich nie odroznia. Kwant 1 nm
// jest po to, ze rownowazne wzory daja wynik rozny o ulp, a ulp nie jest
// ksztaltem.
#define JR_SIGNATURE_SAMPLES 64

static void JrHashU64( uint64_t* h, uint64_t v )
{
	int i;
	for ( i = 0; i < 8; ++i )
	{
		*h ^= ( v >> ( 8 * i ) ) & 0xffu;
		*h *= 1099511628211ull;
	}
}

static void JrHashLen( uint64_t* h, double v )
{
	double q = v * 1e9;
	JrHashU64( h, (uint64_t)(int64_t)( q < 0.0 ? q - 0.5 : q + 0.5 ) );
}

uint64_t JozzRig_EnvelopeSignature( const JozzRigConfig* c )
{
	uint64_t h = 1469598103934665603ull;
	JrHashU64( &h, (uint64_t)(int)c->variant );
	JrHashLen( &h, (double)c->wheelR );
	JrHashLen( &h, (double)c->wheelW );
	if ( c->variant == JOZZ_RIG_SPHERE )
		return h;
	JrHashU64( &h, (uint64_t)c->prismSides );
	if ( c->variant != JOZZ_RIG_TORUS )
		return h;
	{
		JozzRigTorusRow rows[JOZZ_RIG_CROWN_ROWS_MAX];
		JrTorusGeom g = JrTorus( c );
		int m, i;
		if ( !g.valid )
			return 0;
		m = JrMakeRows( &g, g.rows, rows );
		if ( m <= 0 )
			return 0;
		// Srodki przedzialow, nie konce - na krawedzi barku obwiednia jest nieciagla
		// i probka postawiona dokladnie tam rozroznia bryly, ktore sa identyczne.
		for ( i = 0; i < JR_SIGNATURE_SAMPLES; ++i )
		{
			double y = ( g.halfLen + g.crownR ) * ( (double)i + 0.5 ) / (double)JR_SIGNATURE_SAMPLES;
			JrHashLen( &h, JrRowsEnvelope( rows, m, y ) );
		}
	}
	return h;
}

double JozzRig_EnvelopeRipple( const JozzRigConfig* c )
{
	if ( c->variant == JOZZ_RIG_SPHERE )
		return 0.0;
	if ( c->prismSides < 3 || c->wheelR <= 0.0f )
		return 0.0;
	double theta = JOZZ_RIG_PI / (double)c->prismSides;
	if ( c->variant == JOZZ_RIG_PRISM_MAX )
		return (double)c->wheelR * ( 1.0 - cos( theta ) );

	JrTorusGeom g = JrTorus( c );
	if ( !g.valid )
		return 0.0;
	// Mierzone na rzedzie o NAJWIEKSZYM promieniu, bo to on dotyka plyty. Przy
	// jednym rzedzie jest to `ringR`, czyli dawny wzor co do bitu.
	{
		JozzRigTorusRow rows[JOZZ_RIG_CROWN_ROWS_MAX];
		int m = JrMakeRows( &g, g.rows, rows );
		double rMax = 0.0, s, rMin;
		int j;
		for ( j = 0; j < m; ++j )
			if ( rows[j].ringR > rMax )
				rMax = rows[j].ringR;
		s = rMax * sin( theta );
		if ( s > g.crownR )
			return -1.0; // obwiednia NIESZCZELNA - nie ma czego mierzyc
		// Najdalszy punkt obwiedni na dwusiecznej miedzy dwiema kapsulami.
		rMin = rMax * cos( theta ) + sqrt( g.crownR * g.crownR - s * s );
		return ( rMax + g.crownR ) - rMin;
	}
}

static int JrBuildTorus( b3BodyId body, const b3ShapeDef* sd, const JozzRigConfig* c )
{
	JozzRigTorusRow rows[JOZZ_RIG_CROWN_ROWS_MAX];
	JrTorusGeom g = JrTorus( c );
	int m, i, j;
	if ( !g.valid )
		return 0;
	if ( c->prismSides < JozzRig_MinTorusSegments( c ) )
		return 0; // dziurawa obwiednia; patrz naglowek JozzRig_MinTorusSegments
	m = JrMakeRows( &g, g.rows, rows );
	if ( m <= 0 )
		return 0;
	for ( j = 0; j < m; ++j )
		if ( rows[j].ringR <= 0.0 || rows[j].halfLen <= 0.0 )
			return 0; // rzad zapadl sie pod os albo ma zerowa dlugosc

	// Kolejnosc petli jest czescia kontraktu: przy jednym rzedzie ciag ksztaltow
	// jest CO DO KOLEJNOSCI ten sam co przed dolozeniem rzedow, a kolejnosc
	// ksztaltow wchodzi do rozwiazywania kontaktow.
	for ( i = 0; i < g.n; ++i )
	{
		double a = 2.0 * JOZZ_RIG_PI * (double)i / (double)g.n;
		double ca = cos( a ), sa = sin( a );
		for ( j = 0; j < m; ++j )
		{
			b3Capsule cap;
			cap.radius = (float)rows[j].capR;
			cap.center1.x = (float)( rows[j].ringR * ca );
			cap.center1.y = (float)( rows[j].yCenter - rows[j].halfLen );
			cap.center1.z = (float)( rows[j].ringR * sa );
			cap.center2.x = cap.center1.x;
			cap.center2.y = (float)( rows[j].yCenter + rows[j].halfLen );
			cap.center2.z = cap.center1.z;
			if ( B3_IS_NULL( b3CreateCapsuleShape( body, sd, &cap ) ) )
				return 0;
		}
	}
	return 1;
}

int JozzRig_BuildEnvelopeFromConfig( b3BodyId body, const JozzRigConfig* c )
{
	if ( c->variant == JOZZ_RIG_TORUS )
	{
		b3ShapeDef sd = b3DefaultShapeDef();
		sd.density = c->density;
		sd.baseMaterial.friction = c->friction;
		sd.baseMaterial.rollingResistance = 0.0f;
		return JrBuildTorus( body, &sd, c );
	}
	return JozzRig_BuildEnvelopeEx( body, c->variant, c->density, c->prismSides, c->wheelR, c->wheelW,
									c->friction );
}

// ---------------------------------------------------------------- rig

double JozzRig_Downforce( const JozzRig* rig )
{
	return rig->loadN - (double)rig->cfg.massKg * rig->cfg.gravity;
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
	JozzRigConfig cfg = JozzRig_DefaultConfig();
	cfg.variant = v;
	cfg.prismSides = prismSides;
	return JozzRig_CreateFromConfig( rig, &cfg, hooks );
}

int JozzRig_CreateFromConfig( JozzRig* rig, const JozzRigConfig* cfg, const JozzRigRenderHooks* hooks )
{
	memset( rig, 0, sizeof( *rig ) );
	rig->cfg = *cfg;
	rig->variant = cfg->variant;
	rig->prismSides = cfg->prismSides;
	rig->controllerEnabled = cfg->controllerEnabled;
	rig->targetSpeed = cfg->targetSpeed;
	rig->loadN = cfg->loadN;

	b3WorldDef wd = b3DefaultWorldDef();
	wd.workerCount = 1;
	wd.gravity.x = 0.0f;
	wd.gravity.y = -(float)cfg->gravity;
	wd.gravity.z = 0.0f;
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
	gd.position.y = -cfg->groundHalfY;
	gd.position.z = 0.0f;
	rig->ground = b3CreateBody( rig->world, &gd );
	b3ShapeDef gs = b3DefaultShapeDef();
	gs.baseMaterial.friction = cfg->friction;
	gs.baseMaterial.rollingResistance = 0.0f;
	b3BoxHull box = b3MakeBoxHull( cfg->groundHalfX, cfg->groundHalfY, cfg->groundHalfZ );
	b3CreateHullShape( rig->ground, &gs, &box.base );

	b3BodyDef bd = b3DefaultBodyDef();
	bd.type = b3_dynamicBody;
	bd.position.x = (float)cfg->startX;
	bd.position.y = cfg->wheelR + cfg->startGap;
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
	bd.linearVelocity.x = (float)cfg->startSpeed;
	bd.linearVelocity.y = 0.0f;
	bd.linearVelocity.z = 0.0f;
	bd.angularVelocity.x = 0.0f;
	bd.angularVelocity.y = 0.0f;
	bd.angularVelocity.z = -(float)cfg->startSpeed / cfg->wheelR;
	bd.enableSleep = false;
	bd.allowFastRotation = true;
	rig->body = b3CreateBody( rig->world, &bd );
	if ( JozzRig_BuildEnvelopeFromConfig( rig->body, cfg ) == 0 )
	{
		b3DestroyWorld( rig->world );
		memset( rig, 0, sizeof( *rig ) );
		return 0;
	}
	JozzRig_FreezeMassEx( rig->body, cfg->massKg, cfg->wheelR, cfg->inertiaSpinFactor,
						  cfg->inertiaTransFactor );
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
	const double dt = rig->cfg.dt;

	double f = 0.0;
	if ( rig->controllerEnabled )
	{
		double err = rig->targetSpeed - (double)v.x;
		double fraw = rig->cfg.kp * err + rig->cfg.ki * rig->integral;
		f = fraw > rig->cfg.fmax ? rig->cfg.fmax : ( fraw < -rig->cfg.fmax ? -rig->cfg.fmax : fraw );
		if ( fabs( fraw ) <= rig->cfg.fmax )
			rig->integral += err * dt;
	}

	// 3) sila: naped wzdluz swiatowego +X, docisk w dol, oba do srodka masy
	b3Vec3 force;
	force.x = (float)f;
	force.y = -(float)downN;
	force.z = 0.0f;
	b3Body_ApplyForceToCenter( rig->body, force, true );

	// 4) DOKLADNIE ten krok, ktory wykonuje stend. `dt` ma JEDNO zrodlo: wczesniej
	// solver dostawal literal 1/60, a regulator i praca liczyly sie z makra -
	// zmiana jednego rozjezdzala pomiar z fizyka po cichu.
	b3World_Step( rig->world, (float)dt, rig->cfg.substeps );
	rig->step += 1;
	rig->workSteps += 1;

	// 5) odczyt po kroku; calkowanie regula prostokata na stanie POKROKOWYM
	b3Vec3 v2 = b3Body_GetLinearVelocity( rig->body );
	b3Pos p = b3Body_GetPosition( rig->body );
	JozzWheelKin k = JozzRig_KinematicsR( rig->body, rig->cfg.wheelR );

	double pDrive = f * (double)v2.x;
	double pDown = -downN * (double)v2.y;
	double pGrav = -(double)rig->cfg.massKg * rig->cfg.gravity * (double)v2.y;
	rig->wDriveSigned += pDrive * dt;
	if ( pDrive > 0.0 )
		rig->wDrivePos += pDrive * dt;
	else
		rig->wDriveNeg += pDrive * dt;
	rig->wDriveAbs += fabs( pDrive ) * dt;
	rig->wDownforce += pDown * dt;
	rig->wGravity += pGrav * dt;
	rig->vyIntegral += (double)v2.y * dt;

	double dx = (double)p.x - (double)rig->prevPos.x;
	double dy = (double)p.y - (double)rig->prevPos.y;
	double dz = (double)p.z - (double)rig->prevPos.z;
	rig->pathLength += sqrt( dx * dx + dy * dy + dz * dz );
	rig->prevPos = p;
	rig->absSpin += fabs( k.omegaSpin ) * dt;

	int sat = fabs( f ) >= 0.999 * rig->cfg.fmax;
	if ( sat )
		rig->satSteps += 1;

	if ( out )
	{
		out->step = rig->workSteps;
		out->time = rig->workSteps * dt;
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
		out->peGravity = (double)rig->cfg.massKg * rig->cfg.gravity * (double)p.y;
	}
}

// ---------------------------------------------------------------- zaburzenia

static void MarkPerturbed( JozzRig* rig, const char* what )
{
	rig->perturbed = 1;
	rig->perturbCount += 1;
	snprintf( rig->lastPerturbation, sizeof( rig->lastPerturbation ), "%s", what ? what : "?" );
}

// Zaburzenie, ktorego ten modul NIE wykonuje, ale ktore go dotyczy: framework
// okna umie wpiac motor joint w cialo rigu albo wstrzelic cialo do jego swiata.
// Bez tej funkcji taka ingerencja bylaby jedyna, ktora nie podnosi flagi - a
// wtedy `perturbed` znaczyloby "ruszony przez rig", nie "ruszony".
void JozzRig_MarkPerturbation( JozzRig* rig, const char* what )
{
	MarkPerturbed( rig, what );
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
