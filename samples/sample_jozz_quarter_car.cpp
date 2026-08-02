// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// Quarter Car Scope - WARSZTAT rigu Q3.
//
// Nie jest to juz podglad na jeden przebieg. Zadanie: zbudowac kolo, powiesic je
// na zawieszeniu, puscic po drodze, POCZUC roznice i - tym samym przyciskiem -
// dostac liczbe, ktora znaczy dokladnie to samo co liczba ze stendu.
//
// Cztery niezmienniki, ktore ten plik ma utrzymac:
//
//  1. Fizyka ma JEDNA implementacje - jozz_qc_rig.c, wspolna ze stendem. Ten plik
//     nie tworzy swiata, nie liczy momentu i nie dotyka cial poza jawnie
//     oznaczonymi funkcjami.
//  2. Kamera, overlay i tempo NIE zmieniaja kroku fizyki. Spowolnienie POMIJA
//     klatki, nigdy nie skraca dt.
//  3. Przycisk "zmierz jak stend" uruchamia JozzQc_MeasureCell - ten sam kod,
//     ktory wykonuje --qc-compare, na rigu zbudowanym OD NOWA. Dzieki temu liczba
//     z okna nie zalezy od tego, co Owner robil z biegnacym przebiegiem przed
//     nacisnieciem przycisku.
//  4. Kazda ingerencja jest oznaczona i lepka, a przebieg z ingerencja jest
//     przez rig oznaczany jako NIEWAZNY - tak samo jak headless.
//
// Po co okno, skoro liczby sa w tabeli: liczba mowi, ze `sprung_accel_rms` spadl
// o 83%, ale nie mowi, JAK to wyglada. Werdykt o feelu nalezy do Jozza i nie da
// sie go wydac z pliku CSV (reguła twarda 5).

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "implot.h"
#include "sample.h"

#include "jozz_qc_rig.h"
#include "jozz_wheel_rig.h"

#include "box3d/box3d.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

// Dzielnik tempa: ile klatek na jeden krok fizyki. dt zostaje 1/60 ZAWSZE.
constexpr int kDividers[] = { 1, 2, 4, 8, 16 };
constexpr int kDividerCount = (int)( sizeof( kDividers ) / sizeof( kDividers[0] ) );

// Po co istnieje kazdy kandydat. Sama lista przychodzi z JozzQc_Candidates, czyli
// stamtad, skad bierze ja stend - tutaj sa wylacznie objasnienia dla czlowieka,
// dopasowane po etykiecie. Nieznana etykieta nie jest bledem: lista moze urosnac,
// a brak zdania opisu nie jest powodem, zeby kandydat zniknal z okna.
const char* CandidateWhy( const char* label )
{
	if ( strcmp( label, "sphere" ) == 0 )
		return "KONTROLA: obwiednia bez zadnej struktury. Wszystko, co ten\n"
			   "przebieg pokazuje, jest wlasnoscia rigu, nie ksztaltu kola.";
	if ( strcmp( label, "prism-Nmax" ) == 0 )
		return "DZISIEJSZE KOLO PRODUKTU, na granicy tego, co przyjmuje\n"
			   "b3CreateHull (F-01). Punkt odniesienia dla kazdego kandydata.";
	if ( strcmp( label, "prism-32" ) == 0 )
		return "Ten sam pryzmat przy N rownym torusowi - uczciwe zestawienie\n"
			   "gestosci podzialu, nie budzetu hulla.";
	if ( strcmp( label, "torus-32" ) == 0 )
		return "Nowy kandydat przy tym samym N co pryzmat. Ma WIEKSZE tetnienie\n"
			   "promienia i mimo to wygrywa - patrz F-22.";
	if ( strcmp( label, "torus-64" ) == 0 )
		return "Nowy kandydat TAM, GDZIE PRYZMAT JUZ NIE SIEGA. 64 kapsuly przy\n"
			   "twardym limicie 42 scianek hulla.";
	return NULL;
}

const char* Env( const char* name )
{
	const char* v = getenv( name );
	return ( v && v[0] ) ? v : nullptr;
}

// Polka konstrukcji. Ten sam katalog co polka Wheel Scope, bo to jedno miejsce
// na "przypadki, ktore warto miec pod reka" - rozne rozszerzenia (.rig kolo,
// .qc caly narożnik) mowia, ktorego szczebla dotyczy plik.
std::filesystem::path BenchDir()
{
	const char* p = getenv( "JOZZ_WHEEL_BENCH_DIR" );
	return ( p && p[0] ) ? std::filesystem::path( p ) : std::filesystem::path( "wheel_scope_bench" );
}

// Nazwa pliku pochodzi od czlowieka, wiec nie moze wskazywac poza polke.
std::string SafeName( const char* raw )
{
	std::string out;
	for ( const char* s = raw; s && *s && out.size() < 60; ++s )
	{
		unsigned char c = (unsigned char)*s;
		bool ok = ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '-' ||
				  c == '_';
		out.push_back( ok ? (char)c : '_' );
	}
	if ( out.empty() )
		out = "narożnik";
	return out;
}

const char* PerturbationName( const char* what )
{
	if ( what == NULL )
		return "?";
	if ( strcmp( what, "mouse grab" ) == 0 )
		return "chwyt myszy";
	if ( strcmp( what, "launched cylinder" ) == 0 )
		return "wstrzelony walec";
	if ( strcmp( what, "launched ragdoll" ) == 0 )
		return "wstrzelony ragdoll";
	if ( strcmp( what, "launched sphere" ) == 0 )
		return "wstrzelona kula";
	return what;
}

// Wiersz tabeli porownan. Poza klasa, bo przezywa restart razem z warsztatem.
struct Row
{
	char label[80];
	char detail[256];
	JozzQcConfig cfg;
	JozzQcCell cell;
};

// STAN WARSZTATU PRZEZYWAJACY "R".
//
// Framework na "R" NISZCZY sample i tworzy go od nowa (samples/main.cpp:236 ->
// SelectSample -> delete + CreateFcn). Poprzednia wersja przenosila przez ten
// restart WYLACZNIE konfiguracje, wiec "R" kasowal tabele pomiarow, tempo,
// ustawienia widoku, nastawy przemiatania, nazwe na polce i narzedzia reki. Z
// perspektywy pracy wygladalo to jak reset USTAWIEN, a nie przebiegu - a "R" ma
// byc najtanszym ruchem w tym oknie: puscic to samo jeszcze raz.
//
// Zapis idzie w destruktorze, bo to jedyne miejsce widzace stan koncowy
// niezaleznie od tego, co restart wywolalo. Przywracanie jest jawnie zalezne od
// `context->restart`: przejscie na inny sample i powrot ma dac warsztat czysty.
struct WorkshopState
{
	JozzQcConfig cfg;

	int dividerIndex;
	int warmupSteps;
	bool autoWarmup, follow, rewind, showContacts, showTrail, showTravelRuler;
	float trailGain, plotSpan;

	std::vector<Row> rows;
	int baseRow;
	bool baseRowPinned, showDelta, fullProtocol;

	int sweepParam, sweepSteps;
	float sweepFrom, sweepTo;

	float liftH, kickNs, shakerHz, shakerN;
	float grabForce;

	char shelfName[64];
	char shelfNote[160];
	char tab[32];
};
WorkshopState g_saved;
bool g_haveSaved = false;

} // namespace

class JozzQuarterCarScope : public Sample
{
public:
	explicit JozzQuarterCarScope( SampleContext* context )
		: Sample( context )
	{
		m_candCount = JozzQc_Candidates( m_cand, JOZZ_QC_CANDIDATE_CAP );
		m_mouseForceScale = kGrabForce;

		m_cfg = JozzQc_DefaultConfig();
		m_cfg.variant = JOZZ_RIG_TORUS;
		m_cfg.segments = 64;
		// 4 m/s, nie 13: przy predkosci kontraktowej obwiednia mija ~2.2 elementu
		// na krok fizyki, czyli struktura kola jest PONIZEJ rozdzielczosci czasowej
		// solvera i okno pokazuje szum. Warsztat ma sie otwierac tam, gdzie widac
		// to, co sie bada.
		m_cfg.targetSpeed = 4.0;
		m_cfg.startSpeed = 4.0;
		m_openCfg = m_cfg; // konstrukcja otwarcia warsztatu, do "reset calosci"
		if ( context->restart && g_haveSaved )
			RestoreWorkshop();

		if ( const char* v = Env( "JOZZ_QC_VARIANT" ) )
		{
			for ( int i = 0; i < JOZZ_RIG_VARIANT_COUNT; ++i )
				if ( strcmp( v, JozzRig_VariantName( (JozzRigVariant)i ) ) == 0 )
					m_cfg.variant = (JozzRigVariant)i;
		}
		if ( const char* s = Env( "JOZZ_QC_SEGMENTS" ) )
			m_cfg.segments = atoi( s );
		if ( const char* s = Env( "JOZZ_QC_ROAD" ) )
		{
			for ( int i = 0; i < JOZZ_QC_ROAD_COUNT; ++i )
				if ( strcmp( s, JozzQc_RoadName( (JozzQcRoad)i ) ) == 0 )
					m_cfg.road = (JozzQcRoad)i;
		}
		if ( const char* s = Env( "JOZZ_QC_SPEED" ) )
		{
			m_cfg.targetSpeed = atof( s );
			m_cfg.startSpeed = m_cfg.targetSpeed;
		}
		// Plik konstrukcji wygrywa ze WSZYSTKIM powyzej i celowo stoi na koncu:
		// opisuje pelna tozsamosc przebiegu, wiec czesciowe nadpisanie go zmienna
		// srodowiskowa dalo by przebieg, ktorego nie opisuje zaden zapis.
		std::string fromEnv;
		if ( const char* p = Env( "JOZZ_QC_CONFIG" ) )
		{
			char err[256];
			JozzQcConfig loaded;
			if ( JozzQc_ConfigReadFile( &loaded, p, err, sizeof( err ) ) )
			{
				m_cfg = loaded;
				fromEnv = std::filesystem::path( p ).stem().string();
			}
			else
			{
				snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "BLAD JOZZ_QC_CONFIG: %s", err );
			}
		}

		Build();
		snprintf( m_loadedName, sizeof( m_loadedName ), "%s", fromEnv.c_str() );
		RefreshShelf();

		// Dwie sciezki dla zrzutu ekranu i dla agenta. Powod jest praktyczny:
		// zakladki i tabela porownan sa czescia deliverable'u, a bez tego jedynym
		// sposobem ich obejrzenia bylo klikniecie - czyli nie dalo sie sprawdzic,
		// czy dzialaja, inaczej niz prosac o to czlowieka.
		if ( const char* t = Env( "JOZZ_QC_TAB" ) )
			snprintf( m_forceTab, sizeof( m_forceTab ), "%s", t );
		if ( const char* m = Env( "JOZZ_QC_MEASURE" ) )
		{
			if ( strncmp( m, "sweep", 5 ) == 0 )
			{
				// JOZZ_QC_MEASURE=sweep:<parametr>:<od>:<do>:<punktow>
				const char* p = strchr( m, ':' );
				if ( p )
					m_sweepParam = atoi( p + 1 );
				SweepDefaults();
				m_sweepParamPrev = m_sweepParam;
				if ( p && ( p = strchr( p + 1, ':' ) ) )
					m_sweepFrom = (float)atof( p + 1 );
				if ( p && ( p = strchr( p + 1, ':' ) ) )
					m_sweepTo = (float)atof( p + 1 );
				if ( p && ( p = strchr( p + 1, ':' ) ) )
					m_sweepSteps = atoi( p + 1 );
				RunSweep();
			}
			else
			{
				MeasureAllCandidates();
			}
			if ( const char* out = Env( "JOZZ_QC_MEASURE_CSV" ) )
				SaveRowsCsvTo( out );
		}

		if ( context->restart == false )
		{
			// Kadr musi objac NARAZ: grunt, obwiednie, kolumne zawieszenia i mase
			// resorowana - pytanie Q3 brzmi, co z udaru dochodzi WYZEJ. Poprzednie
			// ustawienie (pochylenie 10 stopni) patrzylo niemal poziomo i plyta
			// 400x120 m zajmowala pol kadru, przez co kolo wygladalo na wiszace.
			// Sprawdzone na zrzucie, nie wyliczone.
			b3Pos p = b3Body_GetPosition( m_rig.wheel );
			p.y += 0.55f;
			m_camera->SetView( -72.0f, 14.0f, 3.8f, p );
		}
	}

	~JozzQuarterCarScope() override
	{
		SaveWorkshop();
		ReleaseMouseGrab();
		FinishRecording();
		JozzQc_Destroy( &m_rig );
	}

	// Wszystko, co Owner ustawil reka - lacznie z tabela pomiarow. Pominiecie
	// czegokolwiek tutaj wraca jako "R skasowal mi ustawienia", wiec lista ma byc
	// nudna i kompletna, a nie wybiorcza.
	void SaveWorkshop()
	{
		g_saved.cfg = m_cfg;
		g_saved.dividerIndex = m_dividerIndex;
		g_saved.warmupSteps = m_warmupSteps;
		g_saved.autoWarmup = m_autoWarmup;
		g_saved.follow = m_follow;
		g_saved.rewind = m_rewind;
		g_saved.showContacts = m_showContacts;
		g_saved.showTrail = m_showTrail;
		g_saved.showTravelRuler = m_showTravelRuler;
		g_saved.trailGain = m_trailGain;
		g_saved.plotSpan = m_plotSpan;
		g_saved.rows = m_rows;
		g_saved.baseRow = m_baseRow;
		g_saved.baseRowPinned = m_baseRowPinned;
		g_saved.showDelta = m_showDelta;
		g_saved.fullProtocol = m_fullProtocol;
		g_saved.sweepParam = m_sweepParam;
		g_saved.sweepSteps = m_sweepSteps;
		g_saved.sweepFrom = m_sweepFrom;
		g_saved.sweepTo = m_sweepTo;
		g_saved.liftH = m_liftH;
		g_saved.kickNs = m_kickNs;
		g_saved.shakerHz = m_shakerHz;
		g_saved.shakerN = m_shakerN;
		g_saved.grabForce = m_mouseForceScale;
		snprintf( g_saved.shelfName, sizeof( g_saved.shelfName ), "%s", m_shelfName );
		snprintf( g_saved.shelfNote, sizeof( g_saved.shelfNote ), "%s", m_shelfNote );
		snprintf( g_saved.tab, sizeof( g_saved.tab ), "%s", m_openTab );
		g_haveSaved = true;
	}

	void RestoreWorkshop()
	{
		m_cfg = g_saved.cfg;
		m_dividerIndex = g_saved.dividerIndex;
		m_warmupSteps = g_saved.warmupSteps;
		m_autoWarmup = g_saved.autoWarmup;
		m_follow = g_saved.follow;
		m_rewind = g_saved.rewind;
		m_showContacts = g_saved.showContacts;
		m_showTrail = g_saved.showTrail;
		m_showTravelRuler = g_saved.showTravelRuler;
		m_trailGain = g_saved.trailGain;
		m_plotSpan = g_saved.plotSpan;
		m_rows = g_saved.rows;
		m_baseRow = g_saved.baseRow;
		m_baseRowPinned = g_saved.baseRowPinned;
		m_showDelta = g_saved.showDelta;
		m_fullProtocol = g_saved.fullProtocol;
		m_sweepParam = g_saved.sweepParam;
		m_sweepParamPrev = g_saved.sweepParam;
		m_sweepSteps = g_saved.sweepSteps;
		m_sweepFrom = g_saved.sweepFrom;
		m_sweepTo = g_saved.sweepTo;
		m_liftH = g_saved.liftH;
		m_kickNs = g_saved.kickNs;
		m_shakerHz = g_saved.shakerHz;
		m_shakerN = g_saved.shakerN;
		m_mouseForceScale = g_saved.grabForce;
		snprintf( m_shelfName, sizeof( m_shelfName ), "%s", g_saved.shelfName );
		snprintf( m_shelfNote, sizeof( m_shelfNote ), "%s", g_saved.shelfNote );
		// Zakladka tez wraca: po "R" panel otwarty na "Zawieszenie" ma zostac na
		// "Zawieszeniu". Wstrzasarka NIE wraca wlaczona - zaburzenie jest tym,
		// czego "R" ma sie pozbyc.
		snprintf( m_forceTab, sizeof( m_forceTab ), "%s", g_saved.tab );
		m_restored = true;
	}

	// Ten sample nie uzywa swiata klasy bazowej - wszystkie uslugi frameworku
	// (zaznaczanie, chwyt myszy, licznik cial, nagrywanie, kadrowanie) maja
	// adresowac rig. Gdy wariant jest nieprzedstawialny, rig NIE MA swiata i
	// jedynym istniejacym jest bazowy.
	b3WorldId ActiveWorld() const override
	{
		return B3_IS_NON_NULL( m_rig.world ) ? m_rig.world : m_worldId;
	}

	void OnWorldPerturbed( const char* what ) override
	{
		JozzQc_MarkPerturbation( &m_rig, PerturbationName( what ) );
	}

	bool HasSolverControls() const override
	{
		return false; // krok nalezy do rigu, nie do frameworku
	}

	bool HasRecordingControls() const override
	{
		return true; // swiat rigu jest realnie krokowany, wiec ma co nagrywac
	}

	bool CondenseDebugOverlay() const override
	{
		return true;
	}

	float InfoPanelWidthEm() const override
	{
		return 33.0f; // zakladki, wykresy i tabela porownan nie mieszcza sie w 20
	}

	b3BodyId FocusBody() const override
	{
		return B3_IS_NON_NULL( m_rig.world ) ? m_rig.wheel : b3_nullBodyId;
	}

	// ---------------------------------------------------------------- budowa
	void Build()
	{
		if ( B3_IS_NON_NULL( m_rig.world ) )
		{
			// Chwyt myszy zyje w swiecie rigu. Puszczamy go PRZED zniszczeniem
			// swiata, inaczej identyfikatory stawu przezylyby swoj swiat.
			ReleaseMouseGrab();
			ClearSelection();
			FinishRecording();
			JozzQc_Destroy( &m_rig );
		}

		NormalizeConstruction();

		// Bez hakow renderera swiat rigu rysuje sie jako pusty ekran. Haki
		// wyciagamy z definicji probnej, bo adapter umie wypelnic tylko caly
		// b3WorldDef - do rigu trafiaja WYLACZNIE te trzy pola.
		b3WorldDef probe = b3DefaultWorldDef();
		AttachToWorldDef( &probe );
		JozzRigRenderHooks hooks;
		hooks.createDebugShape = probe.createDebugShape;
		hooks.destroyDebugShape = probe.destroyDebugShape;
		hooks.userDebugShapeContext = probe.userDebugShapeContext;

		m_ok = JozzQc_Create( &m_rig, &m_cfg, &hooks, m_err, sizeof( m_err ) ) != 0;
		if ( m_ok )
		{
			m_lastGood = m_cfg;
			m_haveLastGood = true;
			// Wstrzasarka jest stanem RIGU, a przebudowa robi rig od nowa. Bez tego
			// kazda zmiana suwaka po cichu ja wylaczala i wygladalo to na zepsuty
			// przelacznik, a nie na skutek przebudowy.
			if ( m_shakerOn )
				JozzQc_SetShaker( &m_rig, 1, (double)m_shakerHz, (double)m_shakerN );
		}
		m_loadedName[0] = '\0'; // kazda przebudowa uniewaznia nazwe z polki

		memset( &m_last, 0, sizeof( m_last ) );
		m_stepCount = 0;
		m_frameTick = 0;
		m_trailCount = 0;
		m_trailHead = 0;
		m_histCount = 0;
		ResetWindow();

		// ROZGRZEWKA OD RAZU. To jest jedna zmiana, ktora najbardziej zmienia
		// prace w tym oknie: przebudowa startuje przy x = -80 i przez pierwsze
		// 2 sekundy rig dojezdza do warunkow pomiaru. Przy 1:1 znaczylo to 2 s
		// czekania po KAZDYM ruchu suwaka, a przy spowolnieniu 1:8 - pol minuty.
		// Rozgrzewka wykonana w jednej klatce kosztuje ulamek sekundy i stawia
		// obraz dokladnie tam, gdzie stend zaczyna liczyc.
		if ( m_ok && m_autoWarmup )
		{
			for ( int i = 0; i < m_warmupSteps; ++i )
			{
				JozzQc_Step( &m_rig, &m_last );
				m_stepCount += 1;
			}
		}
	}

	// Suwak nie moze zbudowac slepego zaulka. Trzy zmiany, kazda na inna droge do
	// konstrukcji, ktorej silnik nie zbuduje:
	//   * promien korony wiekszy niz polowa szerokosci -> kapsula o ujemnej dlugosci
	//   * promien korony wiekszy od promienia kola     -> pierscien o ujemnym R
	//   * N poza zakresem wariantu (nieszczelnosc albo limit hulla)
	// Kazda jest ZGLASZANA w panelu. Cicha poprawka bylaby gorsza niz blad -
	// Owner ogladalby inna konstrukcje niz ta, ktora ustawil.
	void NormalizeConstruction()
	{
		m_fixNote[0] = '\0';
		if ( m_cfg.variant == JOZZ_RIG_TORUS )
		{
			// Granica jest GEOMETRYCZNA: przy crownR = W/2 dlugosc kapsuly schodzi
			// do zera i pierscien staje sie pierscieniem kul. Wczesniej stalo tu
			// 0.45*W - liczba wzieta "z zapasem", ktora po cichu scinala KONTRAKTOWE
			// crown_r 0.20 do 0.197 i okno przestawalo odtwarzac liczby stendu
			// (torus-64 dawal -31% zamiast -33%). Zlapane przez porownanie tabeli
			// z okna z zarejestrowanym przebiegiem, nie przez test.
			float maxCrown = 0.4995f * m_cfg.wheelW;
			if ( maxCrown > 0.9f * m_cfg.wheelR )
				maxCrown = 0.9f * m_cfg.wheelR;
			if ( m_cfg.crownR > maxCrown )
			{
				snprintf( m_fixNote, sizeof( m_fixNote ), "promien korony obcięty z %.3f na %.3f m",
						  (double)m_cfg.crownR, (double)maxCrown );
				m_cfg.crownR = maxCrown;
			}
			if ( m_cfg.crownR < 0.01f )
				m_cfg.crownR = 0.01f;
		}
		int was = m_cfg.segments;
		if ( JozzQc_ClampSegments( &m_cfg ) )
		{
			size_t used = strlen( m_fixNote );
			snprintf( m_fixNote + used, sizeof( m_fixNote ) - used, "%sN zmienione z %d na %d (zakres budowalny "
																   "%d..%d)",
					  used ? "; " : "", was, m_cfg.segments, JozzQc_MinSegments( &m_cfg ),
					  JozzQc_MaxSegments( &m_cfg ) );
		}
	}

	void ResetWindow()
	{
		JozzQc_WindowBegin( &m_win );
		m_winLive = m_win;
		m_winSteps = 0;
	}

	// Zmiana na zywo: fizyka biegnie dalej, ale liczby zebrane przed zmiana i po
	// niej nie opisuja tego samego ukladu, wiec okno startuje od nowa.
	void AfterLiveChange()
	{
		m_cfg.springNPerM = m_rig.cfg.springNPerM;
		m_cfg.zeta = m_rig.cfg.zeta;
		m_cfg.bumpTravel = m_rig.cfg.bumpTravel;
		m_cfg.droopTravel = m_rig.cfg.droopTravel;
		m_cfg.drive = m_rig.cfg.drive;
		m_cfg.targetSpeed = m_rig.cfg.targetSpeed;
		m_cfg.constTorque = m_rig.cfg.constTorque;
		m_cfg.kp = m_rig.cfg.kp;
		m_cfg.ki = m_rig.cfg.ki;
		m_cfg.maxTorque = m_rig.cfg.maxTorque;
		ResetWindow();
		m_liveTuned = true;
	}

	// ----------------------------------------------------------------- krok
	void Step() override
	{
		if ( m_ok == false )
		{
			DrawTextLine( "rig nie powstal - panel po prawej mowi dlaczego" );
			DrawTextLine( "%.60s", m_err );
			return;
		}

		bool advance = false;
		if ( m_context->singleStep > 0 )
		{
			m_context->singleStep -= 1;
			advance = true;
		}
		else if ( m_context->pause == false )
		{
			m_frameTick += 1;
			advance = ( m_frameTick % kDividers[m_dividerIndex] ) == 0;
		}

		if ( advance )
		{
			JozzQc_Step( &m_rig, &m_last );
			m_stepCount += 1;

			// Okno pomiarowe liczy sie na zywo, dokladnie tym samym kodem co w
			// stendzie - inaczej liczba na ekranie i liczba w tabeli bylyby dwoma
			// niezaleznymi twierdzeniami o tym samym przebiegu.
			JozzQc_WindowAdd( &m_win, &m_last );
			m_winSteps += 1;
			if ( ( m_winSteps % 15 ) == 0 )
			{
				m_winLive = m_win;
				JozzQc_WindowEnd( &m_winLive, &m_rig );
			}

			HistAdd( m_last );

			b3Pos c = b3Body_GetPosition( m_rig.chassis );
			m_trail[m_trailHead] = c;
			m_trailHead = ( m_trailHead + 1 ) % kTrailCap;
			if ( m_trailCount < kTrailCap )
				m_trailCount += 1;

			// PRZEWIJANIE. Plyta ma 400 m polowy dlugosci, a rig startuje na -80 i
			// jedzie w prawo - wiec po ~2 minutach jazdy zjezdza z jej konca i leci
			// w prozni. Zlapane przez zostawienie okna wlaczonego, nie przez test:
			// panel dalej pokazywal liczby, tylko opisywaly spadanie. Pomiaru to nie
			// dotyczy (720 krokow = 48 m), wiec przewijanie nalezy do OKNA i tylko do
			// niego. Przebudowa stawia rig z powrotem na starcie razem z rozgrzewka.
			if ( m_rewind && m_last.x > (double)m_cfg.groundHalfX - 10.0 )
			{
				m_rewindCount += 1;
				Build();
				if ( m_ok == false )
					return; // swiat wlasnie zniknal - nie ma czego rysowac
			}
		}

		DrawHud();

		if ( m_follow )
		{
			b3Pos pivot = b3Body_GetPosition( m_rig.wheel );
			pivot.y += 0.55f;
			m_camera->SetPivot( pivot );
		}

		SetDrawOrigin( m_camera->DrawOrigin() );
		b3DebugDraw debugDraw;
		MakeDebugDraw( &debugDraw );
		debugDraw.drawingBounds = m_camera->DrawBounds();
		ApplyGuiFlags( &debugDraw );
		b3World_Draw( m_rig.world, &debugDraw, B3_DEFAULT_MASK_BITS );
	}

	void Render() override
	{
		if ( m_ok == false )
			return;

		// DASHED, bo punkty styku pierscienia kapsul leza czesciowo WEWNATRZ
		// bryly sasiedniej kapsuly - bez tego polowa overlaya jest niewidoczna.
		const OverlayOcclusionMode thru = OVERLAY_OCCLUSION_DASHED;

		b3Pos pc = b3Body_GetPosition( m_rig.chassis );
		b3Pos pw = b3Body_GetPosition( m_rig.wheel );
		b3Pos mount = pc;
		mount.y = pc.y - m_cfg.mountH;

		if ( m_showContacts )
		{
			JozzRigContactPoint pts[128];
			int n = JozzQc_ContactPoints( &m_rig, pts, 128 );
			for ( int i = 0; i < n; ++i )
			{
				// Nosny punkt jasny i duzy, dotykajacy bez impulsu maly. To
				// rozroznienie jest calym sensem tego overlaya: obwiednia potrafi
				// "dotykac" gruntu i nie przenosic ani niutona.
				bool loaded = pts[i].normalImpulse > 0.0f;
				DrawPointEx( pts[i].point, MakeColor( loaded ? b3_colorWhite : b3_colorYellow ),
							 loaded ? 13.0f : 7.0f, OVERLAY_THICKNESS_PIXELS, thru );
				if ( loaded )
				{
					b3Pos tip = pts[i].point;
					tip.x += pts[i].normal.x * 0.3f;
					tip.y += pts[i].normal.y * 0.3f;
					tip.z += pts[i].normal.z * 0.3f;
					DrawLineEx( pts[i].point, tip, MakeColor( b3_colorSkyBlue ), 2.0f, OVERLAY_THICKNESS_PIXELS,
								thru );
				}
			}
		}

		// Kolumna zawieszenia + LINIJKA SKOKU. Bez niej nie widac, ile skoku
		// zostalo - a "kolo dojechalo do zderzaka" i "kolo pracuje w srodku
		// zakresu" wygladaja na ekranie identycznie.
		DrawLineEx( pc, mount, MakeColor( b3_colorSkyBlue ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );
		DrawLineEx( mount, pw, MakeColor( b3_colorOrange ), 3.0f, OVERLAY_THICKNESS_PIXELS, thru );
		if ( m_showTravelRuler )
		{
			const double sag = JozzQc_StaticSag( &m_cfg );
			b3Pos tick = mount;
			tick.z += 0.45f;
			b3Pos a = tick, b = tick;

			a.y = (float)( mount.y + m_rig.travelUpper );
			b.y = a.y;
			a.z -= 0.16f, b.z += 0.16f;
			DrawLineEx( a, b, MakeColor( b3_colorTomato ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );

			a.y = (float)( mount.y + m_rig.travelLower );
			b.y = a.y;
			DrawLineEx( a, b, MakeColor( b3_colorTomato ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );

			a.y = (float)( mount.y + sag );
			b.y = a.y;
			DrawLineEx( a, b, MakeColor( b3_colorDimGray ), 1.5f, OVERLAY_THICKNESS_PIXELS, thru );

			b3Pos lo = tick, hi = tick;
			lo.y = (float)( mount.y + m_rig.travelLower );
			hi.y = (float)( mount.y + m_rig.travelUpper );
			DrawLineEx( lo, hi, MakeColor( b3_colorDimGray ), 1.0f, OVERLAY_THICKNESS_PIXELS, thru );

			b3Pos now = tick;
			now.y = pw.y;
			DrawCrossEx( now, 0.10f, MakeColor( m_last.limitHit ? b3_colorTomato : b3_colorLightGreen ), 3.0f,
						 OVERLAY_THICKNESS_PIXELS, thru );
		}

		if ( m_showTrail && m_trailCount > 1 )
		{
			// Slad wysokosci nadwozia, PRZEWYZSZONY. To nie jest tor ruchu w skali
			// 1:1 - przewyzszenie istnieje po to, zeby milimetry byly widoczne.
			const float y0 = m_cfg.wheelR + m_cfg.mountH - (float)JozzQc_StaticSag( &m_cfg );
			for ( int i = 1; i < m_trailCount; ++i )
			{
				int i0 = ( m_trailHead - m_trailCount + i - 1 + 2 * kTrailCap ) % kTrailCap;
				int i1 = ( m_trailHead - m_trailCount + i + 2 * kTrailCap ) % kTrailCap;
				b3Pos a = m_trail[i0], b = m_trail[i1];
				a.y = y0 + ( a.y - y0 ) * m_trailGain;
				b.y = y0 + ( b.y - y0 ) * m_trailGain;
				DrawLineEx( a, b, MakeColor( b3_colorGold ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );
			}
		}
	}

	// Wspolny renderer tekstu przycina linie do 63 znakow (gfx/text.c), wiec HUD
	// miesci sie w limicie. Rzeczy dlugie ida do panelu, nie tutaj.
	void DrawHud()
	{
		DrawTextLine( "Q3 WARSZTAT  krok %d  %s N=%d  ksztaltow %d", m_stepCount,
					  JozzRig_VariantName( m_cfg.variant ),
					  m_cfg.variant == JOZZ_RIG_SPHERE ? 0 : m_cfg.segments, m_rig.shapeCount );
		DrawTextLine( "droga %-5s  naped %-6s  v %.2f / %.2f m/s%s", JozzQc_RoadName( m_cfg.road ),
					  JozzQc_DriveName( m_cfg.drive ), m_last.speed, m_cfg.targetSpeed,
					  m_last.saturated ? "  SAT" : "" );
		DrawTextLine( "moment %.1f Nm   skok %.1f mm (statyka %.1f)%s", m_last.driveTorque, 1000.0 * m_last.travel,
					  1000.0 * JozzQc_StaticSag( &m_cfg ), m_last.limitHit ? "  ZDERZAK" : "" );
		DrawTextLine( "a_pion %.2f m/s2   styk %d pkt   %s", m_last.sprungAccelY, m_last.loadedPoints,
					  m_last.airborne ? "KOLO W POWIETRZU" : "na gruncie" );
		if ( m_winSteps > 0 )
			DrawTextLine( "okno %d: strata %.0f W  a_rms %.3f  powietrze %.1f%%", m_winSteps, m_winLive.lossPower,
						  m_winLive.sprungAccelRms, 100.0 * m_winLive.airborneFraction );
		if ( m_winLive.invalid )
			DrawTextLine( "NIEWAZNY: %.48s", m_winLive.invalidWhy );
		if ( m_rig.perturbed )
			DrawTextLine( "EXPLORATION: fizyka ruszona recznie %dx", m_rig.perturbCount );
		if ( m_benchMode )
			DrawTextLine( "STANOWISKO - narożnik stoi, naped zdjety (H = jazda)" );
		if ( m_shakerOn )
			DrawTextLine( "WSTRZASARKA %.2f Hz  %.0f N", m_shakerHz, m_shakerN );
		DrawTextLine( "1-5 kandydat  B droga  C naped  X zmierz  Z zeruj okna" );
		DrawTextLine( "H stanowisko  J podnies  K uderz  Ctrl+LPM chwyt" );
		DrawTextLine( "P pauza  O krok  , . tempo  V kamera  S zapis  G obs." );
		DrawTextLine( "R = ten sam przebieg od nowa, ustawienia zostaja" );
	}

	// -------------------------------------------------------------- wejscie
	void Keyboard( int key, int action, int mods ) override
	{
		if ( action != ACTION_PRESS )
			return;
		(void)mods;

		if ( key >= SAPP_KEYCODE_1 && key < SAPP_KEYCODE_1 + m_candCount )
		{
			ApplyCandidate( key - SAPP_KEYCODE_1 );
			return;
		}
		switch ( key )
		{
			case SAPP_KEYCODE_COMMA:
				m_dividerIndex = m_dividerIndex > 0 ? m_dividerIndex - 1 : 0;
				return;
			case SAPP_KEYCODE_PERIOD:
				m_dividerIndex = m_dividerIndex < kDividerCount - 1 ? m_dividerIndex + 1 : m_dividerIndex;
				return;
			case KEY_V:
				m_follow = !m_follow;
				return;
			case SAPP_KEYCODE_Z:
				ResetWindow();
				return;
			case SAPP_KEYCODE_X:
				MeasureNow();
				return;
			case KEY_B:
				m_cfg.road = (JozzQcRoad)( ( (int)m_cfg.road + 1 ) % JOZZ_QC_ROAD_COUNT );
				Build();
				return;
			case SAPP_KEYCODE_C:
				// Tryb napedu zmienia sie NA ZYWO - to jest sens tego skrotu:
				// zdjecie napedu w trakcie jazdy pokazuje sam opor toczenia.
				JozzQc_SetDrive( &m_rig, (JozzQcDrive)( ( (int)m_cfg.drive + 1 ) % JOZZ_QC_DRIVE_COUNT ),
								 m_cfg.targetSpeed, m_cfg.constTorque );
				AfterLiveChange();
				return;
			case SAPP_KEYCODE_S:
				// Skrot istnieje, bo interesujacy przypadek pojawia sie, gdy Owner
				// patrzy na kolo, a nie na panel. Siegniecie po myszke w tym
				// momencie kosztuje wlasnie ten stan, ktory mial byc zapisany.
				SaveConstruction();
				return;
			case SAPP_KEYCODE_G:
				WriteObservation();
				return;
			case SAPP_KEYCODE_H:
				if ( m_benchMode )
					LeaveBenchMode();
				else
					EnterBenchMode();
				return;
			case SAPP_KEYCODE_J:
				// Podniesienie i uderzenie pod reka na klawiaturze, bo obie proby
				// robi sie PATRZAC NA KOLO. Siegniecie po mysz w tym momencie
				// kosztuje wlasnie ten stan, ktory mial byc zbadany.
				JozzQc_LiftCorner( &m_rig, (double)m_liftH );
				return;
			case SAPP_KEYCODE_K:
				JozzQc_KickChassis( &m_rig, (double)m_kickNs );
				return;
			default:
				break;
		}
	}

	// --------------------------------------------------------------- panel
	bool DrawControls() override
	{
		if ( m_ok == false )
		{
			// Nigdy nie zostawiamy Ownera bez drogi powrotnej. Wczesniej zla
			// wartosc suwaka konczyla sie napisem "rig nie powstal" i jedynym
			// wyjsciem bylo przeladowanie sampla.
			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.45f, 0.35f, 1.0f ) );
			ImGui::TextWrapped( "RIG NIE POWSTAL: %s", m_err );
			ImGui::PopStyleColor();
			if ( m_haveLastGood && ImGui::Button( "wroc do ostatniej dzialajacej konstrukcji" ) )
			{
				m_cfg = m_lastGood;
				Build();
			}
			if ( ImGui::Button( "wroc do domyslnej" ) )
			{
				m_cfg = JozzQc_DefaultConfig();
				Build();
			}
			ImGui::Separator();
		}

		ImGui::TextUnformatted( "Q3 - warsztat narożnika: kolo, zawieszenie, droga" );
		ImGui::TextWrapped( "Ta sama fizyka, ktora stend mierzy headless. Werdykt o feelu nalezy do "
							"Ciebie - liczby go nie zastepuja." );
		if ( m_restored )
			ImGui::TextDisabled( "R: przebieg od nowa, ustawienia warsztatu przeniesione" );

		// Stan przejsciowy nazwany wprost. Suwak przebudowy pisze do konstrukcji od
		// razu, a rig powstaje dopiero przy puszczeniu - bez tego paska panel przez
		// caly przeciag pokazywalby liczby konstrukcji, ktorej jeszcze nie ma.
		if ( m_ok && memcmp( &m_cfg, &m_rig.cfg, sizeof( JozzQcConfig ) ) != 0 )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ), "konstrukcja zmieniona - rig powstanie po "
																 "puszczeniu suwaka" );

		if ( ImGui::BeginTabBar( "qc_tabs" ) )
		{
			Tab( "Kolo", &JozzQuarterCarScope::TabWheel );
			Tab( "Zawieszenie", &JozzQuarterCarScope::TabSuspension );
			Tab( "Droga", &JozzQuarterCarScope::TabRoad );
			Tab( "Reka", &JozzQuarterCarScope::TabHand );
			Tab( "Pomiar", &JozzQuarterCarScope::TabMeasure );
			Tab( "Polka", &JozzQuarterCarScope::TabShelf );
			Tab( "Widok", &JozzQuarterCarScope::TabView );
			ImGui::EndTabBar();
		}
		// Wymuszenie zakladki dziala DOKLADNIE RAZ. Dopoki `m_forceTab` bylo
		// niepuste, ImGuiTabItemFlags_SetSelected leciala w kazdej klatce i zakladki
		// nie dalo sie przelaczyc mysza - okno wygladalo na zepsute, a bylo
		// zablokowane wlasna zmienna srodowiskowa.
		m_forceTab[0] = '\0';
		return true;
	}

	void Tab( const char* name, void ( JozzQuarterCarScope::*body )() )
	{
		ImGuiTabItemFlags flags = 0;
		if ( m_forceTab[0] && strcmp( m_forceTab, name ) == 0 )
			flags |= ImGuiTabItemFlags_SetSelected;
		if ( ImGui::BeginTabItem( name, nullptr, flags ) )
		{
			snprintf( m_openTab, sizeof( m_openTab ), "%s", name );
			( this->*body )();
			ImGui::EndTabItem();
		}
	}

private:
	static constexpr int kTrailCap = 360;
	static constexpr int kHistCap = 1800; // 30 s przy 60 Hz

	// ============================================================ zakladki

	// KOLO: to jest badany obiekt. Kazda zmiana tutaj wymaga przebudowy, bo
	// obwiedni i masy nie da sie podmienic na biegnacym ciele.
	void TabWheel()
	{
		ImGui::SeparatorText( "Kandydat (1-5)" );
		int match = MatchCandidate();
		for ( int i = 0; i < m_candCount; ++i )
		{
			char label[64];
			snprintf( label, sizeof( label ), "%d  %s%s", i + 1, m_cand[i].label,
					  m_cand[i].variant == JOZZ_RIG_SPHERE ? "" : "" );
			if ( ImGui::RadioButton( label, match == i ) )
				ApplyCandidate( i );
			if ( ImGui::IsItemHovered() )
			{
				const char* why = CandidateWhy( m_cand[i].label );
				if ( why )
					ImGui::SetTooltip( "%s", why );
			}
		}
		if ( match < 0 )
			ImGui::TextDisabled( "konstrukcja wlasna - poza lista stendu" );

		GroupHeader( "Obwiednia, wymiary i masa (przebudowa)", JOZZ_QC_GROUP_WHEEL );
		int variant = (int)m_cfg.variant;
		for ( int i = 0; i < JOZZ_RIG_VARIANT_COUNT; ++i )
		{
			if ( ImGui::RadioButton( JozzRig_VariantName( (JozzRigVariant)i ), &variant, i ) )
			{
				m_cfg.variant = (JozzRigVariant)i;
				Build();
			}
			if ( i + 1 < JOZZ_RIG_VARIANT_COUNT )
				ImGui::SameLine();
		}

		JozzRigConfig w = JozzQc_WheelConfig( &m_cfg );
		int nMin = JozzQc_MinSegments( &m_cfg );
		int nMax = m_cfg.variant == JOZZ_RIG_PRISM_MAX ? JozzRig_ProbeMaxPrismSides() : 128;

		char segTip[320];
		snprintf( segTip, sizeof( segTip ),
				  "Scianki pryzmatu ALBO kapsuly pierscienia - ta sama zmienna\n"
				  "programu kol, wiec porownywalna miedzy wariantami przy rownym N.\n"
				  "Pryzmat konczy sie na %d (twardy limit b3CreateHull, F-01).",
				  JozzRig_ProbeMaxPrismSides() );
		ImGui::BeginDisabled( m_cfg.variant == JOZZ_RIG_SPHERE );
		IntRebuild( "elementow obwodu", &m_cfg.segments, nMin, nMax, segTip );
		ImGui::EndDisabled();

		if ( m_cfg.variant == JOZZ_RIG_TORUS )
		{
			// Gora zakresu to granica GEOMETRYCZNA: przy crownR = W/2 dlugosc
			// kapsuly schodzi do zera i pierscien staje sie pierscieniem kul.
			FloatRebuild( "promien korony", &m_cfg.crownR, 0.02f, 0.4995f * m_cfg.wheelW, "%.3f m",
						  "Promien przekroju opony = promien kapsuly. Wiekszy = mniejsze\n"
						  "tetnienie promienia i WEZSZA plaska bieznia.\n"
						  "ZMIERZONE (F-25, przemiatanie z tego okna): na plaskiej plycie\n"
						  "przy 4 m/s to NIE jest kompromis - w zakresie 0.04..0.19 m strata\n"
						  "spada monotonicznie o 43%, a churn z 27% do zera. Zwezenie\n"
						  "bieznia z 358 do 58 mm nie kosztuje tu NIC mierzalnego.\n"
						  "Ale Q3 nie mierzy przyczepnosci bocznej - jesli handel istnieje,\n"
						  "lezy poza tym szczeblem." );
			ImGui::Text( "minimum kapsul dla szczelnosci: %d", nMin );
		}
		if ( m_fixNote[0] )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ), "konstrukcja poprawiona: %s", m_fixNote );

		ImGui::Spacing();
		FloatRebuild( "promien kola", &m_cfg.wheelR, 0.20f, 0.90f, "%.4f m",
					  "Zmienia obwiednie I ramie momentu. Domyslnie 0.5141 m - to\n"
					  "kolo stendu v2, wiec kazde porownanie z Q2A wymaga tej wartosci." );
		FloatRebuild( "szerokosc kola", &m_cfg.wheelW, 0.10f, 0.70f, "%.4f m",
					  "Dla torusa ogranicza promien korony: bieznia plaska ma\n"
					  "szerokosc W - 2*crown_r." );
		FloatRebuild( "masa nieresorowana", &m_cfg.unsprungKg, 5.0f, 120.0f, "%.1f kg",
					  "Masa jest ZAMRAZANA - geometria ma byc jedyna zmienna.\n"
					  "Zmiana tej liczby zmienia rowniez czestotliwosc wiezu (F-18)." );
		FloatRebuild( "tarcie", &m_cfg.friction, 0.1f, 2.0f, "%.2f",
					  "Material gruntu i kola naraz. Limit momentu ~mu*N*R, wiec przy\n"
					  "niskim tarciu regulator wchodzi w saturacje i przebieg jest niewazny." );

		if ( ImGui::TreeNode( "Zaawansowane: bezwladnosc" ) )
		{
			FloatRebuild( "i_spin", &m_cfg.inertiaSpin, 0.1f, 1.0f, "%.3f",
						  "iSpin = f*m*R^2 wokol osi kola. Wchodzi do masy efektywnej\n"
						  "napedu: m_eff = m_calk + iSpin/R^2." );
			FloatRebuild( "i_trans", &m_cfg.inertiaTrans, 0.1f, 1.0f, "%.3f", NULL );
			ImGui::TreePop();
		}

		ImGui::SeparatorText( "Co z tego wyszlo" );
		double ripple = JozzRig_EnvelopeRipple( &w );
		ImGui::Text( "tetnienie promienia  %.3f mm", 1000.0 * ripple );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "R nominalne minus R najmniejsze. Jedyna liczba porownywalna\n"
							   "miedzy pryzmatem a pierscieniem kapsul wprost.\n"
							   "UWAGA (F-22): pomiar pokazal, ze to NIE ona rzadzi jakoscia\n"
							   "toczenia - rzadza ostre krawedzie." );
		ImGui::Text( "ksztaltow w ciele    %d", m_rig.shapeCount );
		if ( m_cfg.variant == JOZZ_RIG_TORUS )
			ImGui::Text( "plaska bieznia       %.1f mm", 1000.0 * ( m_cfg.wheelW - 2.0f * m_cfg.crownR ) );
		double facets = JozzRig_FacetsPerStep( &w, m_cfg.targetSpeed );
		ImGui::Text( "elementow na krok    %.2f", facets );
		if ( facets > 1.0 && m_cfg.variant != JOZZ_RIG_SPHERE )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ),
								"kontakt NIEDOPROBKOWANY: miedzy dwiema aktualizacjami\n"
								"manifoldu mija %.1f elementu. b3Collide wykonuje sie RAZ\n"
								"na krok swiata, wiec podkroki tego nie ratuja.",
								facets );
		double vcrit = JozzRig_CriticalSpeed( &w );
		if ( m_cfg.variant != JOZZ_RIG_SPHERE && vcrit > 1e-9 )
			ImGui::Text( "v_kryt (obtoczenie)  %.2f m/s   v/v_kryt %.2f", vcrit, m_cfg.targetSpeed / vcrit );
	}

	// ZAWIESZENIE: jedyna zakladka, w ktorej WIEKSZOSC pokretel dziala na zywo.
	void TabSuspension()
	{
		GroupHeader( "Sprezyna i tlumienie (na zywo)", JOZZ_QC_GROUP_SUSPENSION );

		float k = m_cfg.springNPerM;
		// Dol zakresu to 5000 N/m: ponizej ugiecie statyczne przekracza 300 mm i
		// bryla nadwozia zaczyna wchodzic w kolo na ekranie. Kolizji nie ma (maska
		// zerowa), ale obraz przestaje byc czytelny, a to jest okno do patrzenia.
		if ( ImGui::SliderFloat( "sztywnosc", &k, 5000.0f, 40000.0f, "%.0f N/m" ) )
		{
			JozzQc_SetSuspension( &m_rig, k, m_cfg.zeta );
			AfterLiveChange();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Podawana FIZYCZNIE, w N/m. `suspensionHertz` NIE jest\n"
							   "czestotliwoscia resorowania: idzie za masa ZREDUKOWANA obu\n"
							   "cial (F-18), nie za resorowana. Dlatego rig przyjmuje N/m,\n"
							   "a herce wylicza - ta sama liczba hercow na dwoch kandydatach\n"
							   "o roznej masie nieresorowanej to dwie rozne sprezyny." );

		float z = m_cfg.zeta;
		if ( ImGui::SliderFloat( "tlumienie zeta", &z, 0.02f, 1.20f, "%.3f" ) )
		{
			JozzQc_SetSuspension( &m_rig, m_cfg.springNPerM, z );
			AfterLiveChange();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "OTWARTE (U-27): zmierzone zeta z dekrementu logarytmicznego\n"
							   "wyszlo 0.204 przy zadanym 0.35. Nie wiadomo, czy to zla skala\n"
							   "w silniku, czy wlasnosc ukladu o dwoch stopniach swobody." );

		ImGui::Text( "hertz wiezu %.4f   masa zredukowana %.3f kg", JozzQc_Hertz( &m_cfg ),
					 JozzQc_ReducedMass( &m_cfg ) );
		ImGui::Text( "ugiecie statyczne %.1f mm", 1000.0 * JozzQc_StaticSag( &m_cfg ) );

		ImGui::SeparatorText( "Skok od punktu statycznego (na zywo)" );
		float bump = m_cfg.bumpTravel, droop = m_cfg.droopTravel;
		bool travelChanged = false;
		travelChanged |= ImGui::SliderFloat( "sciskanie", &bump, 0.01f, 0.30f, "%.3f m" );
		travelChanged |= ImGui::SliderFloat( "wywieszenie", &droop, 0.01f, 0.30f, "%.3f m" );
		if ( travelChanged )
		{
			JozzQc_SetTravel( &m_rig, bump, droop );
			AfterLiveChange();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Liczone OD STATYKI, bo tak jest podawane w pojezdzie i tylko\n"
							   "tak ma sens: przy 13500 N/m i 150 kg samo ugiecie statyczne to\n"
							   "111 mm, wiec 'skok 100 mm' liczony od zera oznaczalby rig\n"
							   "stojacy na zderzaku juz na postoju." );
		ImGui::Text( "granice skoku: %.1f .. %.1f mm   teraz %.1f mm", 1000.0 * m_rig.travelLower,
					 1000.0 * m_rig.travelUpper, 1000.0 * m_last.travel );
		if ( m_winSteps > 0 && m_winLive.limitHits > 0 )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ), "zderzak dotkniety w %d z %d krokow okna",
								m_winLive.limitHits, m_winLive.steps );

		ImGui::SeparatorText( "Masa i geometria (przebudowa)" );
		FloatRebuild( "masa resorowana", &m_cfg.sprungKg, 30.0f, 500.0f, "%.0f kg",
					  "Narożnik pojazdu. Zmienia ugiecie statyczne, wiec zmienia takze\n"
					  "polozenie startowe nadwozia - rig startuje JUZ UGIETY." );
		FloatRebuild( "wysokosc mocowania", &m_cfg.mountH, 0.30f, 1.60f, "%.2f m",
					  "Geometrycznie neutralna dla tego wiezu (os pionowa, zerowe ramie),\n"
					  "a MIMO TO przesuwa wyniki - kolo skaczace po fasetach jest ukladem\n"
					  "chaotycznym (F-24). Dlatego pomiar powtarza kazda komorke 3x." );
	}

	// DROGA I NAPED.
	void TabRoad()
	{
		GroupHeader( "Profil drogi (przebudowa, B)", JOZZ_QC_GROUP_ROAD );
		int road = (int)m_cfg.road;
		for ( int i = 0; i < JOZZ_QC_ROAD_COUNT; ++i )
		{
			if ( ImGui::RadioButton( JozzQc_RoadName( (JozzQcRoad)i ), &road, i ) )
			{
				m_cfg.road = (JozzQcRoad)i;
				Build();
			}
			if ( i + 1 < JOZZ_QC_ROAD_COUNT )
				ImGui::SameLine();
		}
		if ( m_cfg.road == JOZZ_QC_ROAD_FLAT )
			ImGui::TextWrapped( "Na plaskiej plycie Q3 mierzy FASETY obwiedni, a nie transfer udaru - "
								"czyli odpowiada na pytanie Q1/Q2, nie na swoje. Zostaje jako kontrola." );
		else
		{
			FloatRebuild( "wysokosc progu", &m_cfg.obstacleH, 0.005f, 0.10f, "%.3f m",
						  "Dobrana tak, zeby udar byl mierzalny, a kolo nie spedzalo w\n"
						  "powietrzu wiecej niz 10% krokow. To NIE jest wartosc bezpieczna\n"
						  "z zalozenia - przebieg liczy `airborne` i sam sie uniewaznia." );
			FloatRebuild( "dlugosc progu", &m_cfg.obstacleLen, 0.02f, 0.40f, "%.3f m", NULL );
			if ( m_cfg.road == JOZZ_QC_ROAD_COMB )
				FloatRebuild( "rozstaw progow", &m_cfg.combSpacing, 0.30f, 6.00f, "%.2f m",
							  "Wymuszenie okresowe. Przy v/rozstaw bliskim hertzowi wiezu\n"
							  "mierzysz rezonans zawieszenia, nie opone." );
			ImGui::Text( "progow na drodze: %d", m_rig.obstacleCount );
			if ( m_cfg.road == JOZZ_QC_ROAD_COMB && m_cfg.combSpacing > 0.0f )
			{
				double fRoad = m_cfg.targetSpeed / (double)m_cfg.combSpacing;
				ImGui::Text( "wymuszenie %.2f Hz   hertz wiezu %.2f Hz", fRoad, JozzQc_Hertz( &m_cfg ) );
			}
		}

		GroupHeader( "Naped (na zywo, C)", JOZZ_QC_GROUP_DRIVE );
		int drive = (int)m_cfg.drive;
		for ( int i = 0; i < JOZZ_QC_DRIVE_COUNT; ++i )
		{
			if ( ImGui::RadioButton( JozzQc_DriveName( (JozzQcDrive)i ), &drive, i ) )
			{
				JozzQc_SetDrive( &m_rig, (JozzQcDrive)i, m_cfg.targetSpeed, m_cfg.constTorque );
				AfterLiveChange();
			}
			if ( i + 1 < JOZZ_QC_DRIVE_COUNT )
				ImGui::SameLine();
		}
		if ( m_cfg.drive == JOZZ_QC_DRIVE_SPEED )
			ImGui::TextWrapped( "Stala predkosc, mierzony moment. To jest protokol kontraktowy: "
								"opor toczenia czyta sie wprost z momentu." );
		else if ( m_cfg.drive == JOZZ_QC_DRIVE_TORQUE )
			ImGui::TextWrapped( "Staly moment. Wyniki NIEporownywalne miedzy kandydatami - kazdy "
								"jedzie z inna predkoscia." );
		else
			ImGui::TextWrapped( "Bieg jalowy. Kontrola: pokazuje sam opor, bez regulatora." );

		float v = (float)m_cfg.targetSpeed;
		if ( ImGui::SliderFloat( "predkosc zadana", &v, 0.5f, 20.0f, "%.2f m/s" ) )
		{
			JozzQc_SetDrive( &m_rig, m_cfg.drive, (double)v, m_cfg.constTorque );
			AfterLiveChange();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "DZIALA NA ZYWO - regulator dociagnie rig do nowej predkosci.\n"
							   "Powyzej ~4.8 m/s sztywne kolo nie utrzymuje ciaglego styku,\n"
							   "a przy 13 m/s struktura obwiedni schodzi ponizej rozdzielczosci\n"
							   "czasowej solvera (~2.2 elementu na krok)." );
		{
			// Predkosc POCZATKOWA to co innego niz zadana: pomiar bench-grade
			// buduje rig od nowa i startuje wlasnie od niej. Rozjazd tych dwoch
			// znaczy, ze pomiar zacznie sie od transientu.
			// Pole jest `double`, suwak potrzebuje `float`: kopia jest tu KONIECZNA,
			// wiec zapis do pola dzieje sie na kazdej zmianie, nie na deaktywacji.
			// Przebudowa nadal czeka na puszczenie - inaczej rig przebudowywalby sie
			// co klatke przeciagania.
			float v0 = (float)m_cfg.startSpeed;
			if ( ImGui::SliderFloat( "predkosc startowa", &v0, 0.0f, 20.0f, "%.2f m/s" ) )
				m_cfg.startSpeed = (double)v0;
			if ( ImGui::IsItemDeactivatedAfterEdit() )
				Build();
			if ( fabs( m_cfg.startSpeed - m_cfg.targetSpeed ) > 1e-6 )
			{
				ImGui::SameLine();
				if ( ImGui::SmallButton( "= zadanej" ) )
				{
					m_cfg.startSpeed = m_cfg.targetSpeed;
					Build();
				}
			}
		}

		if ( m_cfg.drive == JOZZ_QC_DRIVE_TORQUE )
		{
			float t = (float)m_cfg.constTorque;
			if ( ImGui::SliderFloat( "moment staly", &t, 0.0f, 1500.0f, "%.0f Nm" ) )
			{
				JozzQc_SetDrive( &m_rig, m_cfg.drive, m_cfg.targetSpeed, (double)t );
				AfterLiveChange();
			}
		}

		if ( ImGui::TreeNode( "Zaawansowane: regulator" ) )
		{
			float kp = (float)m_cfg.kp, ki = (float)m_cfg.ki, tmax = (float)m_cfg.maxTorque;
			bool ch = false;
			ch |= ImGui::SliderFloat( "kp", &kp, 100.0f, 4000.0f, "%.0f" );
			ch |= ImGui::SliderFloat( "ki", &ki, 100.0f, 9000.0f, "%.0f" );
			ch |= ImGui::SliderFloat( "limit momentu", &tmax, 100.0f, 3000.0f, "%.0f Nm" );
			if ( ch )
			{
				JozzQc_SetDriveGains( &m_rig, kp, ki, tmax );
				AfterLiveChange();
			}
			ImGui::TextWrapped( "Nastawy kontraktowe (1155.7 / 2889.2) sa PRZELICZONE z Q2A na mase "
								"efektywna 224.8 kg, a nie przepisane. Limit 1000 Nm to mu*N*R przy "
								"mu=1 - powyzej kolo i tak buksuje." );
			if ( ImGui::SmallButton( "przywroc kontraktowe" ) )
			{
				JozzQcConfig d = JozzQc_DefaultConfig();
				JozzQc_SetDriveGains( &m_rig, d.kp, d.ki, d.maxTorque );
				AfterLiveChange();
			}
			ImGui::TreePop();
		}
	}

	// POMIAR: okno na zywo, wykresy i tabela porownan bench-grade.
	void TabMeasure()
	{
		ImGui::SeparatorText( "Okno na zywo" );
		if ( m_liveTuned )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ),
								"rig przestrojony na zywo - okno liczy od zmiany" );
		ImGui::Text( "krokow w oknie: %d", m_winSteps );
		if ( m_winSteps > 0 )
		{
			ImGui::Text( "moment  %8.2f Nm      strata   %8.1f W", m_winLive.driveTorqueMean, m_winLive.lossPower );
			ImGui::Text( "a_rms   %8.3f m/s2    skok rms %8.1f mm", m_winLive.sprungAccelRms,
						 1000.0 * m_winLive.travelRms );
			ImGui::Text( "powietrze %6.1f %%      churn    %8.1f %%", 100.0 * m_winLive.airborneFraction,
						 m_winLive.contactChurnPct );
			ImGui::Text( "poslizg %8.4f        v srednie %7.3f m/s", m_winLive.slipRatioMean, m_winLive.speedMean );
			if ( m_winLive.invalid )
				ImGui::TextColored( ImVec4( 1.0f, 0.45f, 0.35f, 1.0f ), "NIEWAZNY: %s", m_winLive.invalidWhy );
		}
		if ( ImGui::Button( "zeruj okno (Z)" ) )
			ResetWindow();

		ImGui::SeparatorText( "Pomiar jak stend (X)" );
		ImGui::TextWrapped( "Buduje rig OD NOWA z aktualnej konstrukcji i wykonuje dokladnie ten "
							"protokol, co --qc-compare. Biegnacy przebieg nie ma na to wplywu." );
		ImGui::Checkbox( "pelny protokol (3 przebiegi, okno 600)", &m_fullProtocol );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Wylaczony = 1 przebieg i okno 300 krokow. Szybciej iteruje, ale\n"
							   "NIE jest liczba bench-grade i tabela oznacza taki wiersz jako\n"
							   "'szybki'. Powtorzenia istnieja, bo kolo skaczace po fasetach\n"
							   "jest chaotyczne (F-24) - jeden przebieg podaje dokladnosc,\n"
							   "ktorej nie ma." );
		if ( ImGui::Button( "zmierz te konstrukcje" ) )
			MeasureNow();
		ImGui::SameLine();
		if ( ImGui::Button( "zmierz wszystkich kandydatow" ) )
			MeasureAllCandidates();
		if ( m_measureMs > 0.0 )
			ImGui::TextDisabled( "ostatni pomiar zajal %.0f ms (okno stoi na ten czas)", m_measureMs );
		// Kolumna `ms` jest kosztem TEGO buildu. Okno chodzi w Debug, stend w /O2,
		// wiec liczby nie sa zamienne - a cena CPU jest realnym argumentem w tym
		// programie (torus-64 kosztuje 13x tyle co pryzmat, zmierzone w Release).
		ImGui::TextWrapped( "Kolumna `ms` pochodzi z TEGO buildu. Cene CPU do cytowania bierz ze "
							"stendu (--qc-compare, kompilowany z /O2) - tutaj sluzy do porownania "
							"kandydatow miedzy soba, nie do podawania liczby bezwzglednej." );

		DrawSweep();
		DrawRowTable();

		ImGui::SeparatorText( "Przebieg" );
		DrawPlots();
	}

	// PRZEMIATANIE. A/B dwoch konstrukcji odpowiada na pytanie "ktora lepsza".
	// Nie odpowiada na "co ten parametr wlasciwie robi" - a to jest pytanie, od
	// ktorego zaczyna sie kazdy nowy system kola. Jeden guzik, ta sama funkcja
	// pomiarowa, N wierszy w tabeli.
	void DrawSweep()
	{
		if ( ImGui::TreeNode( "Przemiataj jeden parametr" ) )
		{
			static const char* names[] = { "elementow obwodu", "promien korony", "sztywnosc", "predkosc zadana",
										   "wysokosc progu" };
			ImGui::Combo( "parametr", &m_sweepParam, names, (int)( sizeof( names ) / sizeof( names[0] ) ) );
			if ( m_sweepParam != m_sweepParamPrev )
			{
				SweepDefaults();
				m_sweepParamPrev = m_sweepParam;
			}
			ImGui::InputFloat( "od", &m_sweepFrom );
			ImGui::InputFloat( "do", &m_sweepTo );
			ImGui::SliderInt( "punktow", &m_sweepSteps, 2, 12 );
			if ( m_measureMs > 0.0 )
				ImGui::TextDisabled( "ostatni pojedynczy pomiar %.0f ms - przemiatanie zajmie okolo %.0f s",
									 m_measureMs, 0.001 * m_measureMs * (double)m_sweepSteps );
			ImGui::TextWrapped( "Kazdy punkt to osobny pomiar tym samym protokolem. Konstrukcja wraca "
								"na koniec do tej, ktora byla." );
			if ( ImGui::Button( "przemiataj" ) )
				RunSweep();
			ImGui::TreePop();
		}
	}

	void SweepDefaults()
	{
		switch ( m_sweepParam )
		{
			case 0:
				m_sweepFrom = (float)JozzQc_MinSegments( &m_cfg );
				m_sweepTo = (float)JozzQc_MaxSegments( &m_cfg );
				break;
			case 1:
				m_sweepFrom = 0.05f;
				m_sweepTo = 0.4995f * m_cfg.wheelW;
				break;
			case 2:
				m_sweepFrom = 6000.0f;
				m_sweepTo = 30000.0f;
				break;
			case 3:
				m_sweepFrom = 1.0f;
				m_sweepTo = 8.0f;
				break;
			default:
				m_sweepFrom = 0.005f;
				m_sweepTo = 0.050f;
				break;
		}
	}

	void RunSweep()
	{
		JozzQcConfig save = m_cfg;
		int n = m_sweepSteps < 2 ? 2 : m_sweepSteps;
		for ( int i = 0; i < n; ++i )
		{
			float t = (float)i / (float)( n - 1 );
			float v = m_sweepFrom + t * ( m_sweepTo - m_sweepFrom );
			char tag[32];
			switch ( m_sweepParam )
			{
				case 0:
					m_cfg.segments = (int)( v + 0.5f );
					snprintf( tag, sizeof( tag ), "N=%d", m_cfg.segments );
					break;
				case 1:
					m_cfg.crownR = v;
					snprintf( tag, sizeof( tag ), "crown=%.3f", (double)v );
					break;
				case 2:
					m_cfg.springNPerM = v;
					snprintf( tag, sizeof( tag ), "k=%.0f", (double)v );
					break;
				case 3:
					m_cfg.targetSpeed = (double)v;
					m_cfg.startSpeed = (double)v;
					snprintf( tag, sizeof( tag ), "v=%.2f", (double)v );
					break;
				default:
					m_cfg.obstacleH = v;
					snprintf( tag, sizeof( tag ), "prog=%.0fmm", 1000.0 * (double)v );
					break;
			}
			// Ta sama normalizacja co przy suwaku: punkt przemiatania, ktorego nie
			// da sie zbudowac, ma dac SASIEDNIA budowalna konstrukcje z jawna
			// etykieta, a nie przerwac przemiatanie w polowie.
			NormalizeConstruction();
			if ( m_sweepParam == 0 )
				snprintf( tag, sizeof( tag ), "N=%d", m_cfg.segments );
			MeasureNow( tag );
		}
		m_cfg = save;
		Build();
	}

	void DrawRowTable()
	{
		if ( m_rows.empty() )
		{
			ImGui::TextDisabled( "brak pomiarow - nacisnij X albo przycisk wyzej" );
			return;
		}

		ImGui::SeparatorText( "Tabela porownan" );
		ImGui::Checkbox( "pokaz zmiane wzgledem bazy", &m_showDelta );
		ImGui::SameLine();
		if ( ImGui::SmallButton( "wyczysc" ) )
			m_rows.clear();
		ImGui::SameLine();
		if ( ImGui::SmallButton( "zapisz CSV" ) )
			SaveRowsCsv();
		if ( m_rowsMsg[0] )
			ImGui::TextDisabled( "%s", m_rowsMsg );
		if ( m_rows.empty() )
			return;
		if ( m_baseRow >= (int)m_rows.size() )
			m_baseRow = 0;

		const Row& base = m_rows[m_baseRow];
		// Wysokosc JAWNA, bo tabela z ImVec2(0,0) zabiera cala reszte panelu i
		// wykresy pod nia staja sie nieosiagalne - zlapane na zrzucie.
		float rowH = ImGui::GetTextLineHeightWithSpacing();
		float tableH = rowH * ( 3.2f + (float)std::min<size_t>( m_rows.size(), 10 ) );
		ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
								ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
		if ( ImGui::BeginTable( "qc_rows", 8, flags, ImVec2( 0.0f, tableH ) ) )
		{
			ImGui::TableSetupScrollFreeze( 2, 1 );
			ImGui::TableSetupColumn( "" );
			ImGui::TableSetupColumn( "konstrukcja" );
			ImGui::TableSetupColumn( "tetn" );
			ImGui::TableSetupColumn( "strata W" );
			ImGui::TableSetupColumn( "a_rms" );
			ImGui::TableSetupColumn( "air" );
			ImGui::TableSetupColumn( "churn" );
			ImGui::TableSetupColumn( "ms" );
			ImGui::TableHeadersRow();

			for ( int i = 0; i < (int)m_rows.size(); ++i )
			{
				const Row& r = m_rows[i];
				ImGui::TableNextRow();
				ImGui::PushID( i );

				ImGui::TableNextColumn();
				bool isBase = ( i == m_baseRow );
				if ( ImGui::RadioButton( "##base", isBase ) )
				{
					m_baseRow = i;
					m_baseRowPinned = true;
				}
				if ( ImGui::IsItemHovered() )
					ImGui::SetTooltip( "baza porownania" );

				ImGui::TableNextColumn();
				if ( ImGui::SmallButton( r.label ) )
				{
					// Wiersz tabeli jest wykonywalny: klikniecie przywraca
					// konstrukcje, ktora dala te liczby. Bez tego "ciekawy wynik
					// sprzed dziesieciu ruchow suwaka" jest nie do odzyskania.
					m_cfg = r.cfg;
					Build();
				}
				if ( ImGui::IsItemHovered() )
					ImGui::SetTooltip( "%s\nklikniecie PRZYWRACA te konstrukcje\n%s", r.detail,
									   r.cell.invalid ? r.cell.why : "przebieg wazny" );

				ImGui::TableNextColumn();
				ImGui::Text( "%.3f", 1000.0 * r.cell.ripple );

				DeltaCell( r.cell.loss, base.cell.loss, i == m_baseRow, "%.1f", true );
				DeltaCell( r.cell.accel, base.cell.accel, i == m_baseRow, "%.3f", true );

				ImGui::TableNextColumn();
				if ( r.cell.invalid )
					ImGui::TextColored( ImVec4( 1.0f, 0.45f, 0.35f, 1.0f ), "%.1f%%!", 100.0 * r.cell.airborne );
				else
					ImGui::Text( "%.1f%%", 100.0 * r.cell.airborne );

				ImGui::TableNextColumn();
				ImGui::Text( "%.1f", r.cell.churn );
				ImGui::TableNextColumn();
				ImGui::Text( "%.3f", r.cell.msPerStep );
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::TextWrapped( "Czerwona liczba w kolumnie 'powietrze' znaczy, ze przebieg jest NIEWAZNY - "
							"powyzej 10%% krokow bez styku tabela mierzy profil drogi, nie opone. "
							"Wiersz zostaje, bo wynikow negatywnych sie nie kasuje." );
		if ( m_rows.size() > 1 )
			ImGui::TextDisabled( "baza: %s", m_rows[m_baseRow].label );
	}

	// Zmiana procentowa ma sens tylko wtedy, gdy baza jest wielkoscia tego samego
	// rzedu co porownywana liczba. Sfera ma strate rzedu 1e-5 W, wiec proste
	// dzielenie dawalo w tabeli "+4078559%" - liczbe formalnie poprawna i
	// kompletnie bezuzyteczna. Zlapane na zrzucie.
	void DeltaCell( double v, double b, bool isBase, const char* fmt, bool lowerIsBetter )
	{
		ImGui::TableNextColumn();
		bool comparable = fabs( b ) > 1e-9 && fabs( b ) > 0.001 * fabs( v );
		if ( m_showDelta && !isBase && comparable )
		{
			double pct = 100.0 * ( v - b ) / fabs( b );
			bool good = lowerIsBetter ? ( pct < 0.0 ) : ( pct > 0.0 );
			ImGui::TextColored( fabs( pct ) < 1.0 ? ImVec4( 0.7f, 0.7f, 0.7f, 1.0f )
												  : ( good ? ImVec4( 0.45f, 0.9f, 0.5f, 1.0f )
														   : ImVec4( 1.0f, 0.55f, 0.45f, 1.0f ) ),
								"%+.0f%%", pct );
			if ( ImGui::IsItemHovered() )
				ImGui::SetTooltip( fmt, v );
		}
		else
		{
			ImGui::Text( fmt, v );
			if ( m_showDelta && !isBase && !comparable && ImGui::IsItemHovered() )
				ImGui::SetTooltip( "baza jest bliska zeru - procent nic by nie znaczyl" );
		}
	}

	void DrawPlots()
	{
		if ( m_histCount < 2 )
		{
			ImGui::TextDisabled( "brak historii - puscic przebieg" );
			return;
		}
		float t1 = m_hT[m_histCount - 1];
		float t0 = t1 - m_plotSpan;

		ImVec2 size( ImGui::GetContentRegionAvail().x, 120.0f );
		if ( ImPlot::BeginPlot( "a_pion", size, ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText ) )
		{
			ImPlot::SetupAxes( "t", "m/s2" );
			ImPlot::SetupAxisLimits( ImAxis_X1, t0, t1, ImPlotCond_Always );
			ImPlot::PlotLine( "a nadwozia", m_hT, m_hAccel, m_histCount );
			ImPlot::EndPlot();
		}
		// Skok liczony OD STATYKI i z autoskala. W pelnej skali skoku (1..201 mm)
		// caly interesujacy sygnal - ulamki milimetra - byl plaska linia; sprawdzone
		// na zrzucie. Ile skoku zostalo, mowi zakladka Zawieszenie liczbami.
		if ( ImPlot::BeginPlot( "skok wzgledem statyki", size, ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText ) )
		{
			ImPlot::SetupAxes( "t", "mm", 0, ImPlotAxisFlags_AutoFit );
			ImPlot::SetupAxisLimits( ImAxis_X1, t0, t1, ImPlotCond_Always );
			ImPlot::PlotLine( "skok - statyka", m_hT, m_hTravel, m_histCount );
			ImPlot::EndPlot();
		}
		if ( ImPlot::BeginPlot( "os kola nad gruntem", size, ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText ) )
		{
			ImPlot::SetupAxes( "t", "mm - R" );
			ImPlot::SetupAxisLimits( ImAxis_X1, t0, t1, ImPlotCond_Always );
			ImPlot::PlotLine( "wysokosc osi", m_hT, m_hWheelDy, m_histCount );
			ImPlot::EndPlot();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Wysokosc osi kola minus promien nominalny. Gladka bryla daje\n"
							   "linie prosta, obwiednia fasetowana - falbanke o amplitudzie\n"
							   "tetnienia promienia." );
		if ( ImPlot::BeginPlot( "moment i styk", size, ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText ) )
		{
			ImPlot::SetupAxes( "t", "" );
			ImPlot::SetupAxisLimits( ImAxis_X1, t0, t1, ImPlotCond_Always );
			ImPlot::PlotLine( "moment Nm", m_hT, m_hTorque, m_histCount );
			ImPlot::PlotLine( "punktow nosnych", m_hT, m_hLoaded, m_histCount );
			ImPlot::EndPlot();
		}
		ImGui::SliderFloat( "okno wykresu", &m_plotSpan, 1.0f, 30.0f, "%.0f s" );
	}

	// POLKA: zapisany plik to nie notatka, tylko WYKONYWALNE wejscie. Ten sam plik
	// wczytuje sie tutaj i podaje stendowi przez --qc-config, wiec "ciekawy
	// przypadek" przestaje byc czyms, co trzeba odtworzyc z pamieci suwakow.
	void TabShelf()
	{
		ImGui::TextWrapped( "Zapisuje KONSTRUKCJE (z czego zbudowac przebieg), nie wynik. Ten sam plik "
							"uruchomi stend:" );
		ImGui::TextDisabled( "wheel_bench --qc-trace out.csv --qc-config <plik.qc>" );
		ImGui::Separator();

		ImGui::PushItemWidth( 12.0f * ImGui::GetFontSize() );
		ImGui::InputText( "nazwa", m_shelfName, sizeof( m_shelfName ) );
		ImGui::InputText( "notatka", m_shelfNote, sizeof( m_shelfNote ) );
		ImGui::PopItemWidth();
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Po co ten przypadek istnieje. Idzie do komentarza w pliku,\n"
							   "razem ze znacznikiem czasu, krokiem i klasa sesji." );
		if ( ImGui::Button( "zapisz konstrukcje (S)" ) )
			SaveConstruction();
		ImGui::SameLine();
		if ( ImGui::Button( "odswiez liste" ) )
			RefreshShelf();

		if ( m_shelf.empty() )
			ImGui::TextDisabled( "polka pusta (%s)", BenchDir().string().c_str() );
		else
			for ( const std::string& s : m_shelf )
			{
				ImGui::PushID( s.c_str() );
				if ( ImGui::SmallButton( "wczytaj" ) )
					LoadConstruction( s );
				ImGui::SameLine();
				ImGui::TextUnformatted( s.c_str() );
				ImGui::PopID();
			}
		if ( m_shelfMsg[0] )
			ImGui::TextWrapped( "%s", m_shelfMsg );
		if ( m_loadedName[0] )
			ImGui::Text( "biezaca konstrukcja z pliku: %s", m_loadedName );

		ImGui::SeparatorText( "Dziennik obserwacji" );
		ImGui::TextWrapped( "Jedna linia: co widzisz + pelny kontekst techniczny tej chwili. "
							"Obserwacja bez kontekstu nie jest podpieta do niczego." );
		if ( ImGui::Button( "zapisz obserwacje (G)" ) )
			WriteObservation();
		if ( m_obsCount > 0 )
			ImGui::Text( "zapisanych: %d -> %s", m_obsCount, ObsPath() );

		ImGui::SeparatorText( "Odcisk konstrukcji" );
		char digest[900];
		JozzQc_ConfigDigest( &m_cfg, digest, sizeof( digest ) );
		ImGui::TextWrapped( "%s", digest );
	}

	// REKA. Zakladka istnieje, bo "podnies, podrzuc, potrzas" jest podstawowa
	// proba zawieszenia, a przez sama mysz nie da sie jej wykonac ani powtarzalnie,
	// ani w ogole: masa resorowana byla do niedawna niechwytalna (patrz komentarz
	// przy JQC_CHASSIS_CATEGORY), a kolo pedzace 4 m/s ucieka spod kursora.
	// Kazde narzedzie z tej zakladki oznacza przebieg jako ruszony recznie.
	void TabHand()
	{
		ImGui::SeparatorText( "Stanowisko" );
		ImGui::TextWrapped( "Rig w trybie jazdy jedzie %.1f m/s - chwytanie i podrzucanie czegos, co "
							"ucieka spod kursora, nie jest testem, tylko walka z regulatorem. "
							"Stanowisko zatrzymuje narożnik na plycie i dopiero wtedy reka ma sens.",
							m_cfg.targetSpeed );
		if ( m_benchMode == false )
		{
			if ( ImGui::Button( "postaw na stanowisku (H)" ) )
				EnterBenchMode();
		}
		else
		{
			ImGui::TextColored( ImVec4( 0.45f, 0.9f, 0.5f, 1.0f ), "STANOWISKO: narożnik stoi, naped zdjety" );
			if ( ImGui::Button( "wroc do jazdy (H)" ) )
				LeaveBenchMode();
		}

		ImGui::SeparatorText( "Podnies i upusc" );
		ImGui::SliderFloat( "wysokosc", &m_liftH, 0.01f, 1.00f, "%.3f m" );
		if ( ImGui::Button( "podnies i upusc" ) )
			JozzQc_LiftCorner( &m_rig, (double)m_liftH );
		ImGui::SameLine();
		if ( ImGui::Button( "przyciagnij w dol" ) )
			JozzQc_LiftCorner( &m_rig, -(double)m_liftH );
		ImGui::TextWrapped( "Podnoszony jest CALY narożnik, nie samo nadwozie: podniesienie samej masy "
							"resorowanej naciaga wiez do zderzaka wywieszenia i to, co potem widac, "
							"jest odbiciem od ogranicznika, a nie upadkiem zawieszenia." );
		if ( m_liftH > (float)( m_cfg.wheelR ) )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ),
								"wysokosc wieksza od promienia kola - to juz zrzut, nie proba skoku" );

		ImGui::SeparatorText( "Uderzenie" );
		ImGui::SliderFloat( "impuls", &m_kickNs, -1500.0f, 1500.0f, "%.0f N*s" );
		if ( ImGui::Button( "uderz" ) )
			JozzQc_KickChassis( &m_rig, (double)m_kickNs );
		ImGui::SameLine();
		ImGui::TextDisabled( "= %.2f m/s masy resorowanej", m_cfg.sprungKg > 0.0f ? m_kickNs / m_cfg.sprungKg : 0.0f );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Impuls dzielony przez mase resorowana - czyli skok predkosci\n"
							   "pionowej, ktory ten guzik zadaje. Latwiej celowac w wielkosc\n"
							   "fizyczna niz w niutonosekundy." );

		ImGui::SeparatorText( "Wstrzasarka" );
		bool sh = m_shakerOn;
		if ( ImGui::Checkbox( "wymuszenie sinusoidalne", &sh ) )
		{
			m_shakerOn = sh;
			JozzQc_SetShaker( &m_rig, m_shakerOn ? 1 : 0, (double)m_shakerHz, (double)m_shakerN );
		}
		ImGui::BeginDisabled( m_shakerOn == false );
		bool ch = false;
		ch |= ImGui::SliderFloat( "czestotliwosc", &m_shakerHz, 0.1f, 12.0f, "%.2f Hz" );
		ch |= ImGui::SliderFloat( "amplituda sily", &m_shakerN, 10.0f, 3000.0f, "%.0f N" );
		if ( ch )
			JozzQc_SetShaker( &m_rig, 1, (double)m_shakerHz, (double)m_shakerN );
		ImGui::EndDisabled();
		double fJoint = JozzQc_Hertz( &m_cfg );
		ImGui::Text( "hertz wiezu %.3f Hz", fJoint );
		if ( m_shakerOn && fJoint > 1e-6 )
		{
			ImGui::SameLine();
			ImGui::TextDisabled( "  wymuszenie/wiez %.2f", (double)m_shakerHz / fJoint );
		}
		if ( ImGui::SmallButton( "ustaw na hertz wiezu" ) )
		{
			m_shakerHz = (float)fJoint;
			JozzQc_SetShaker( &m_rig, m_shakerOn ? 1 : 0, (double)m_shakerHz, (double)m_shakerN );
		}
		ImGui::TextWrapped( "Przemiatanie czestotliwoscia pokazuje rezonans zawieszenia wprost. Hertz wiezu "
							"jest tu wielkoscia WYLICZANA z masy zredukowanej (F-18), a nie zadana - warto "
							"umiec go zobaczyc, a nie tylko przeczytac." );

		ImGui::SeparatorText( "Chwyt mysza" );
		ImGui::TextWrapped( "Ctrl + przeciagniecie chwyta kolo ALBO mase resorowana. Do niedawna nadwozia "
							"nie dalo sie zlapac wcale: mialo zerowa maske kolizji, a ten sam predykat "
							"decyduje o trafieniu promienia myszy. Teraz ma wlasna kategorie." );
		ImGui::SliderFloat( "sila chwytu", &m_mouseForceScale, 1.0f, 400.0f, "x%.0f m*g" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Gorne ograniczenie sily sprezyny chwytu, w wielokrotnosciach\n"
							   "ciezaru chwyconego ciala. Za malo = chwyt sie zsuwa,\n"
							   "za duzo = ciala przelatuja przez siebie." );
		ImGui::TextDisabled( "Shift+klik wystrzeliwuje kule, Shift+Ctrl walec, Shift+Alt ragdolla." );

		if ( m_rig.perturbed )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ),
								"przebieg ruszony recznie %d x (ostatnio: %s)\n"
								"pomiar 'jak stend' i tak buduje rig OD NOWA - jest czysty",
								m_rig.perturbCount, m_rig.lastPerturbation );
	}

	void TabView()
	{
		ImGui::Text( "tempo 1:%d klatek na krok", kDividers[m_dividerIndex] );
		ImGui::SliderInt( "spowolnienie", &m_dividerIndex, 0, kDividerCount - 1, "" );
		ImGui::TextWrapped( "Spowolnienie POMIJA klatki. Krok fizyki zostaje 1/60 s - inaczej obraz "
							"i pomiar przestalyby dotyczyc tej samej symulacji." );
		ImGui::Separator();

		ImGui::Checkbox( "punkty i normalne kontaktu", &m_showContacts );
		ImGui::Checkbox( "linijka skoku zawieszenia", &m_showTravelRuler );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Czerwone kreski = zderzaki, szara = punkt statyczny,\n"
							   "zielony krzyzyk = teraz. 'Kolo dojechalo do zderzaka' i\n"
							   "'kolo pracuje w srodku zakresu' wygladaja bez niej identycznie." );
		ImGui::Checkbox( "slad nadwozia", &m_showTrail );
		if ( m_showTrail )
		{
			ImGui::SliderFloat( "przewyzszenie", &m_trailGain, 1.0f, 200.0f, "x%.0f" );
			if ( m_trailGain > 1.5f )
				ImGui::TextWrapped( "UWAGA: slad jest przewyzszony w pionie x%.0f. To NIE jest skala 1:1 - "
									"sluzy do zobaczenia kierunku zjawiska, nie do odczytu amplitudy.",
									m_trailGain );
		}
		ImGui::Checkbox( "kamera sledzi kolo (V)", &m_follow );
		ImGui::Checkbox( "przewijaj na start przed koncem plyty", &m_rewind );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Plyta ma %.0f m od srodka, rig startuje na %.0f i jedzie w prawo -\n"
							   "bez przewijania zjezdza z jej konca po okolo %.0f s jazdy i dalej\n"
							   "leci w prozni, a panel nadal pokazuje liczby. Pomiaru to nie\n"
							   "dotyczy: jeden przebieg to 720 krokow, czyli 48 m.",
							   (double)m_cfg.groundHalfX, m_cfg.startX,
							   m_cfg.targetSpeed > 0.1 ? ( (double)m_cfg.groundHalfX - 10.0 - m_cfg.startX ) /
															 m_cfg.targetSpeed
													   : 0.0 );
		if ( m_rewindCount > 0 )
			ImGui::TextDisabled( "przewiniete %d x", m_rewindCount );

		ImGui::SeparatorText( "Przebudowa" );
		ImGui::Checkbox( "rozgrzewka od razu po przebudowie", &m_autoWarmup );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Wykonuje %d krokow w jednej klatce, zeby obraz startowal tam,\n"
							   "gdzie stend zaczyna liczyc. Wylaczenie pokazuje faze dojazdu -\n"
							   "przydatne, gdy badasz sam start, a nie jazde ustabilizowana.",
							   m_warmupSteps );
		ImGui::BeginDisabled( m_autoWarmup == false );
		ImGui::SliderInt( "krokow rozgrzewki", &m_warmupSteps, 0, 600 );
		ImGui::EndDisabled();
		if ( ImGui::Button( "przebuduj teraz" ) )
			Build();
		ImGui::SameLine();
		if ( ImGui::Button( "konstrukcja kontraktowa" ) )
		{
			m_cfg = JozzQc_DefaultConfig();
			Build();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Wszystkie pola konstrukcji na wartosci z kontraktu Q3.\n"
							   "Widok i narzedzia reki zostaja - to osobny przycisk nizej." );

		ImGui::SeparatorText( "Reset calosci" );
		if ( ImGui::Button( "RESET WARSZTATU" ) )
			ResetWorkshop();
		ImGui::SameLine();
		ImGui::TextDisabled( "(tabela pomiarow zostaje)" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Konstrukcja, widok, tempo, narzedzia reki i nastawy przemiatania\n"
							   "wracaja do stanu otwarcia okna. Tabela pomiarow NIE - wynikow sie\n"
							   "nie kasuje mimochodem, a ma wlasny przycisk 'wyczysc'." );

		GroupHeader( "Instrument (zmienia NARZEDZIE, nie badany obiekt)", JOZZ_QC_GROUP_INSTRUMENT );
		IntRebuild( "podkrokow", &m_cfg.substeps, 1, 16,
					"Podkroki NIE ratuja niedoprobkowanego kontaktu: b3Collide\n"
					"wykonuje sie RAZ na b3World_Step, wiec podkroki przesolwowuja\n"
					"ten sam manifold. Kontraktowa wartosc to 4." );
		float hz = (float)( m_cfg.dt > 0.0 ? 1.0 / m_cfg.dt : 60.0 );
		if ( ImGui::SliderFloat( "krokow na sekunde", &hz, 30.0f, 240.0f, "%.0f Hz" ) && hz > 0.0f )
			m_cfg.dt = 1.0 / (double)hz;
		if ( ImGui::IsItemDeactivatedAfterEdit() )
			Build();
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Kontraktowe 60 Hz. Kazda inna wartosc daje liczby, ktorych NIE\n"
							   "wolno zestawiac z tabela stendu - to inny przyrzad." );
		float grav = (float)m_cfg.gravity;
		if ( ImGui::SliderFloat( "grawitacja", &grav, 0.5f, 20.0f, "%.3f m/s2" ) )
			m_cfg.gravity = (double)grav;
		if ( ImGui::IsItemDeactivatedAfterEdit() )
			Build();
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Wchodzi w ugiecie statyczne i w obciazenie kola, wiec zmienia\n"
							   "PUNKT PRACY zawieszenia, a nie tylko sile ciezkosci." );
		if ( fabs( m_cfg.dt - 1.0 / 60.0 ) > 1e-9 || m_cfg.substeps != 4 )
			ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.3f, 1.0f ),
								"PRZYRZAD ZMIENIONY wobec kontraktu (1/60 s, 4 podkroki)" );

		if ( m_rig.perturbed )
		{
			ImGui::SeparatorText( "Sesja" );
			ImGui::TextWrapped( "EXPLORATION: fizyka ruszona recznie %d x (ostatnio: %s). Ta sesja "
								"generuje hipotezy, nie dowody. Reset: R.",
								m_rig.perturbCount, m_rig.lastPerturbation );
		}
	}

	// ============================================================ mechanika

	// Suwak, ktory PRZEBUDOWUJE rig przy puszczeniu.
	//
	// TU BYL BLAD, przez ktory szesnascie kontrolek tego okna bylo martwych i
	// "wracalo do domyslnej" natychmiast po zmianie. Poprzednia wersja trzymala
	// wartosc w lokalnej kopii i zatwierdzala ja przez
	//     if ( IsItemDeactivatedAfterEdit() && v != *field ) { *field = v; }
	// To NIE MOZE zadzialac: w klatce, w ktorej przycisk myszy zostaje puszczony,
	// ImGui wola ClearActiveID() i pomija zapis wartosci
	// (.fetchcontent-cache/imgui-src/imgui_widgets.cpp:3126-3129, `set_new_value`
	// zostaje falszem). Deaktywacja zglasza sie wiec w tej samej klatce, w ktorej
	// suwak niczego nie zapisal - `v` jest rowne `*field` i warunek zatwierdzenia
	// nigdy nie przechodzi. Przez caly przeciag wartosc zyla tylko w lokalnej
	// zmiennej, ktora ginela z koncem klatki.
	//
	// Teraz suwak pisze WPROST do pola konfiguracji, a puszczenie przebudowuje rig.
	// Miedzy chwytem a puszczeniem konstrukcja jest zmieniona, a rig jeszcze nie -
	// panel nazywa ten stan wprost ("czeka na przebudowe"), zamiast udawac, ze
	// nic sie nie dzieje.
	bool FloatRebuild( const char* label, float* field, float lo, float hi, const char* fmt, const char* tip )
	{
		ImGui::SliderFloat( label, field, lo, hi, fmt );
		bool done = ImGui::IsItemDeactivatedAfterEdit();
		if ( tip && ImGui::IsItemHovered() )
			ImGui::SetTooltip( "%s\n(Ctrl+klik = wpisz liczbe z klawiatury)", tip );
		if ( done )
			Build();
		return done;
	}

	bool IntRebuild( const char* label, int* field, int lo, int hi, const char* tip )
	{
		ImGui::SliderInt( label, field, lo, hi );
		bool done = ImGui::IsItemDeactivatedAfterEdit();
		if ( tip && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
			ImGui::SetTooltip( "%s\n(Ctrl+klik = wpisz liczbe z klawiatury)", tip );
		if ( done )
			Build();
		return done;
	}

	// Naglowek sekcji z licznikiem zmian i przyciskiem przywracania. Licznik
	// odpowiada na pytanie, ktore pada po kwadransie pracy i na ktore panel bez
	// niego nie odpowiada: co ja wlasciwie poruszylem. Grupy pol pochodza z
	// tabeli `.qc`, wiec nie da sie dodac pola, ktorego licznik nie widzi.
	void GroupHeader( const char* title, const char* group )
	{
		int changed = JozzQc_ChangedInGroup( &m_cfg, group );
		if ( changed > 0 )
		{
			char buf[96];
			snprintf( buf, sizeof( buf ), "%s  (zmienione: %d)", title, changed );
			ImGui::SeparatorText( buf );
		}
		else
		{
			ImGui::SeparatorText( title );
		}

		ImGui::PushID( group );
		ImGui::BeginDisabled( changed == 0 );
		if ( ImGui::SmallButton( "przywroc domyslne" ) )
		{
			JozzQc_ResetGroup( &m_cfg, group );
			Build();
		}
		ImGui::EndDisabled();
		ImGui::PopID();
		if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
		{
			if ( changed )
				ImGui::SetTooltip( "Przywraca %d pol sekcji '%s' do wartosci kontraktowych.", changed, group );
			else
				ImGui::SetTooltip( "Wszystkie pola sekcji '%s' sa kontraktowe.", group );
		}
	}

	// ---------------------------------------------------------- stanowisko
	// Zatrzymany narożnik jest osobnym STANEM PRACY, nie ustawieniem: zmienia
	// naped, obie predkosci i punkt startu naraz. Gdyby trzeba bylo zlozyc go z
	// czterech suwakow, powrot do jazdy wymagalby pamietania czterech liczb -
	// wiec zapamietuje je narzedzie.
	void EnterBenchMode()
	{
		if ( m_benchMode )
			return;
		m_driveSave = m_cfg.drive;
		m_targetSave = m_cfg.targetSpeed;
		m_startSave = m_cfg.startSpeed;
		m_startXSave = m_cfg.startX;

		m_cfg.drive = JOZZ_QC_DRIVE_COAST;
		m_cfg.targetSpeed = 0.0;
		m_cfg.startSpeed = 0.0;
		m_cfg.startX = 0.0; // przy -80 m narożnik stalby poza kadrem otwarcia
		m_benchMode = true;
		Build();
	}

	void LeaveBenchMode()
	{
		if ( m_benchMode == false )
			return;
		m_cfg.drive = m_driveSave;
		m_cfg.targetSpeed = m_targetSave;
		m_cfg.startSpeed = m_startSave;
		m_cfg.startX = m_startXSave;
		m_benchMode = false;
		Build();
	}

	void ResetView()
	{
		m_dividerIndex = 0;
		m_follow = true;
		m_rewind = true;
		m_showContacts = true;
		m_showTrail = false;
		m_showTravelRuler = true;
		m_trailGain = 10.0f;
		m_plotSpan = 6.0f;
		m_autoWarmup = true;
		m_warmupSteps = JOZZ_QC_WARMUP_STEPS;
		m_mouseForceScale = kGrabForce;
	}

	// Jeden ruch z powrotem do stanu otwarcia okna. Tabela pomiarow zostaje
	// swiadomie: wynikow sie nie kasuje mimochodem (reguła twarda 6), a ma wlasny
	// przycisk.
	void ResetWorkshop()
	{
		m_cfg = m_openCfg;
		m_benchMode = false;
		m_shakerOn = false;
		m_liftH = 0.25f;
		m_kickNs = 400.0f;
		m_shakerHz = 1.5f;
		m_shakerN = 400.0f;
		m_sweepParam = 0;
		m_sweepParamPrev = -1;
		m_sweepSteps = 6;
		ResetView();
		Build();
		SweepDefaults();
		m_sweepParamPrev = m_sweepParam;
		m_liveTuned = false;
		snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "warsztat zresetowany do stanu otwarcia okna" );
	}

	int MatchCandidate() const
	{
		for ( int i = 0; i < m_candCount; ++i )
			if ( m_cand[i].variant == m_cfg.variant &&
				 ( m_cfg.variant == JOZZ_RIG_SPHERE || m_cand[i].segments == m_cfg.segments ) )
				return i;
		return -1;
	}

	void ApplyCandidate( int i )
	{
		if ( i < 0 || i >= m_candCount )
			return;
		m_cfg.variant = m_cand[i].variant;
		if ( m_cand[i].variant != JOZZ_RIG_SPHERE )
			m_cfg.segments = m_cand[i].segments;
		Build();
	}

	// Cala macierz kandydatow przy AKTUALNYCH warunkach (droga, predkosc,
	// zawieszenie). To nie to samo, co --qc-compare: tam warunki sa kontraktowe,
	// tutaj sa Twoje. Konstrukcja wraca na koniec do tej, ktora byla.
	void MeasureAllCandidates()
	{
		JozzQcConfig save = m_cfg;
		for ( int i = 0; i < m_candCount; ++i )
		{
			m_cfg.variant = m_cand[i].variant;
			if ( m_cand[i].variant != JOZZ_RIG_SPHERE )
				m_cfg.segments = m_cand[i].segments;
			JozzQc_ClampSegments( &m_cfg );
			MeasureNow();
		}
		m_cfg = save;
		Build();
	}

	// Pomiar bench-grade. Rig budowany OD NOWA, wiec wynik nie zalezy od tego, co
	// dzialo sie z biegnacym przebiegiem - lacznie ze strojeniem na zywo.
	void MeasureNow( const char* extra = nullptr )
	{
		Row r;
		int window = m_fullProtocol ? JOZZ_QC_WINDOW_STEPS : 300;
		// Zegar SCIENNY, nie ImGui::GetTime(): to ostatnie jest stale przez cala
		// klatke, a caly pomiar dzieje sie wewnatrz jednej klatki - wiec pokazywalo
		// rowne zero i nikt nie wiedzial, na co czeka.
		auto tStart = std::chrono::steady_clock::now();
		if ( m_fullProtocol )
		{
			r.cell = JozzQc_MeasureCell( &m_cfg, JOZZ_QC_WARMUP_STEPS, window );
		}
		else
		{
			JozzQcRunResult one = JozzQc_MeasureOne( &m_cfg, JOZZ_QC_WARMUP_STEPS, window, NULL, 0 );
			memset( &r.cell, 0, sizeof( r.cell ) );
			r.cell.built = one.built;
			r.cell.shapes = one.shapes;
			r.cell.ripple = one.ripple;
			r.cell.repeats = 1;
			r.cell.msPerStep = one.msPerStep;
			r.cell.torque = one.win.driveTorqueMean;
			r.cell.loss = one.win.lossPower;
			r.cell.accel = one.win.sprungAccelRms;
			r.cell.airborne = one.win.airborneFraction;
			r.cell.travelRms = one.win.travelRms;
			r.cell.churn = one.win.contactChurnPct;
			r.cell.slip = one.win.slipRatioMean;
			r.cell.speed = one.win.speedMean;
			r.cell.invalid = one.win.invalid;
			snprintf( r.cell.why, sizeof( r.cell.why ), "%s", one.win.invalidWhy );
			snprintf( r.cell.err, sizeof( r.cell.err ), "%s", one.err );
		}
		m_measureMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - tStart ).count();

		if ( r.cell.built == 0 )
		{
			snprintf( m_rowsMsg, sizeof( m_rowsMsg ), "pomiar nieudany: %s", r.cell.err );
			return;
		}
		r.cfg = m_cfg;
		int match = MatchCandidate();
		snprintf( r.label, sizeof( r.label ), "%s%s%s%s", match >= 0 ? m_cand[match].label : "wlasna",
				  extra ? " " : "", extra ? extra : "", m_fullProtocol ? "" : " (szybki)" );
		snprintf( r.detail, sizeof( r.detail ),
				  "%s N=%d  droga %s  v %.2f m/s  k %.0f N/m  zeta %.2f\n"
				  "%d przebieg(ow), okno %d krokow, ksztaltow %d",
				  JozzRig_VariantName( m_cfg.variant ), m_cfg.variant == JOZZ_RIG_SPHERE ? 0 : m_cfg.segments,
				  JozzQc_RoadName( m_cfg.road ), m_cfg.targetSpeed, (double)m_cfg.springNPerM, (double)m_cfg.zeta,
				  r.cell.repeats, window, r.cell.shapes );
		m_rows.push_back( r );
		snprintf( m_rowsMsg, sizeof( m_rowsMsg ), "wiersz dodany: %s", r.label );

		// Baza porownania idzie domyslnie na DZISIEJSZE KOLO PRODUKTU, a nie na
		// pierwszy zmierzony wiersz. Sfera jest kontrola, nie odniesieniem: ma
		// strate rzedu mikrowata, wiec kazdy procent wzgledem niej jest szumem.
		// Wybor Ownera (klikniety radiobutton) wygrywa i juz sie nie zmienia.
		if ( m_baseRowPinned == false )
		{
			m_baseRow = 0;
			for ( int i = 0; i < (int)m_rows.size(); ++i )
				if ( strncmp( m_rows[i].label, "prism-Nmax", 10 ) == 0 )
					m_baseRow = i;
		}
	}

	void SaveRowsCsv()
	{
		std::error_code ec;
		std::filesystem::create_directories( BenchDir(), ec );
		std::filesystem::path path = BenchDir() / "qc_okno_pomiary.csv";
		for ( int n = 2; std::filesystem::exists( path ) && n < 1000; ++n )
		{
			char name[64];
			snprintf( name, sizeof( name ), "qc_okno_pomiary_%d.csv", n );
			path = BenchDir() / name;
		}
		SaveRowsCsvTo( path.string().c_str() );
	}

	void SaveRowsCsvTo( const char* pathStr )
	{
		std::filesystem::path path( pathStr );
		FILE* f = fopen( path.string().c_str(), "wb" );
		if ( f == nullptr )
		{
			snprintf( m_rowsMsg, sizeof( m_rowsMsg ), "BLAD zapisu %s", path.string().c_str() );
			return;
		}
		// Te same kolumny co --qc-compare plus `repeats`, zeby wiersz szybki dalo
		// sie odroznic od bench-grade po samym pliku, bez pamiecia o kliknietym
		// przelaczniku.
		fprintf( f, "# Q3 - pomiary z okna warsztatu (Quarter Car Scope)\n" );
		fprintf( f, "candidate,repeats,segments,shapes,ripple_mm,road,target_v,drive_torque_nm,torque_spread,"
					"loss_power_w,sprung_accel_rms,accel_spread,airborne_frac,airborne_spread,travel_rms,"
					"churn_pct,slip_mean,speed_mean,ms_per_step,valid,why\n" );
		for ( const Row& r : m_rows )
			fprintf( f, "%s,%d,%d,%d,%.6f,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
						"%.17g,%.17g,%d,%s\n",
					 r.label, r.cell.repeats, r.cfg.variant == JOZZ_RIG_SPHERE ? 0 : r.cfg.segments, r.cell.shapes,
					 1000.0 * r.cell.ripple, JozzQc_RoadName( r.cfg.road ), r.cfg.targetSpeed, r.cell.torque,
					 r.cell.torqueSpread, r.cell.loss, r.cell.accel, r.cell.accelSpread, r.cell.airborne,
					 r.cell.airborneSpread, r.cell.travelRms, r.cell.churn, r.cell.slip, r.cell.speed,
					 r.cell.msPerStep, r.cell.invalid ? 0 : 1, r.cell.why );
		fclose( f );
		snprintf( m_rowsMsg, sizeof( m_rowsMsg ), "zapisano %s", path.string().c_str() );
	}

	void HistAdd( const JozzQcSample& s )
	{
		if ( m_histCount == kHistCap )
		{
			// Zwykle przesuniecie o polowe. Prosciej niz bufor cykliczny z
			// przesunieciem w ImPlot, a koszt (50 kB memmove raz na 900 krokow)
			// nie jest widoczny.
			int keep = kHistCap / 2;
			int drop = kHistCap - keep;
			memmove( m_hT, m_hT + drop, keep * sizeof( float ) );
			memmove( m_hAccel, m_hAccel + drop, keep * sizeof( float ) );
			memmove( m_hTravel, m_hTravel + drop, keep * sizeof( float ) );
			memmove( m_hTorque, m_hTorque + drop, keep * sizeof( float ) );
			memmove( m_hWheelDy, m_hWheelDy + drop, keep * sizeof( float ) );
			memmove( m_hLoaded, m_hLoaded + drop, keep * sizeof( float ) );
			m_histCount = keep;
		}
		int i = m_histCount++;
		m_hT[i] = (float)s.time;
		m_hAccel[i] = (float)s.sprungAccelY;
		m_hTravel[i] = (float)( 1000.0 * ( s.travel - JozzQc_StaticSag( &m_cfg ) ) );
		m_hTorque[i] = (float)s.driveTorque;
		m_hWheelDy[i] = (float)( 1000.0 * ( s.wheelY - (double)m_cfg.wheelR ) );
		m_hLoaded[i] = (float)s.loadedPoints;
	}

	// ---------------------------------------------------------------- polka
	void RefreshShelf()
	{
		m_shelf.clear();
		std::error_code ec;
		for ( const auto& e : std::filesystem::directory_iterator( BenchDir(), ec ) )
		{
			if ( e.is_regular_file( ec ) && e.path().extension() == ".qc" )
				m_shelf.push_back( e.path().stem().string() );
		}
		std::sort( m_shelf.begin(), m_shelf.end() );
	}

	void SaveConstruction()
	{
		std::error_code ec;
		std::filesystem::create_directories( BenchDir(), ec );

		std::string name = SafeName( m_shelfName );
		std::filesystem::path path = BenchDir() / ( name + ".qc" );
		for ( int n = 2; std::filesystem::exists( path ) && n < 1000; ++n )
		{
			// Nie nadpisujemy cicho. Ciekawy przypadek sprzed godziny jest wart
			// tyle samo co ten sprzed minuty, a Owner nie ma jak cofnac utraty.
			char suffix[16];
			snprintf( suffix, sizeof( suffix ), "_%d", n );
			path = BenchDir() / ( name + suffix + ".qc" );
		}

		time_t now = time( nullptr );
		char stamp[64];
		strftime( stamp, sizeof( stamp ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );

		// Kontekst chwili zapisu. Nie jest czescia konstrukcji i wlasnie dlatego
		// jest komentarzem: przy wczytaniu nie wplywa na nic, ale mowi, skad sie
		// wziela i czy przebieg, ktory ja zainspirowal, byl czysty.
		char note[512];
		snprintf( note, sizeof( note ),
				  "%s | %s | zapisane w kroku %d przy v=%.3f | sesja=%s%s | okno %d krokow: strata %.1f W "
				  "a_rms %.3f powietrze %.1f%%",
				  stamp, m_shelfNote[0] ? m_shelfNote : "(bez notatki)", m_stepCount, m_last.speed,
				  m_rig.perturbed ? "EXPLORATION" : "OBSERVATION",
				  m_rig.perturbed ? " - przebieg byl ruszony recznie" : "", m_winSteps, m_winLive.lossPower,
				  m_winLive.sprungAccelRms, 100.0 * m_winLive.airborneFraction );

		if ( JozzQc_ConfigWriteFile( &m_cfg, path.string().c_str(), note ) )
		{
			snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "zapisano: %s", path.string().c_str() );
			RefreshShelf();
		}
		else
		{
			snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "BLAD zapisu: %s", path.string().c_str() );
		}
	}

	void LoadConstruction( const std::string& stem )
	{
		std::filesystem::path path = BenchDir() / ( stem + ".qc" );
		JozzQcConfig loaded;
		char err[256];
		if ( JozzQc_ConfigReadFile( &loaded, path.string().c_str(), err, sizeof( err ) ) == 0 )
		{
			snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "BLAD odczytu: %s", err );
			return;
		}
		// Nie przycinamy N po cichu. Plik zadajacy wiecej scianek, niz ten build
		// potrafi zbudowac, opisuje inna konstrukcje niz ta, ktora dalo by sie
		// uruchomic - a milcząca podmiana bylaby najgorsza z trzech odpowiedzi.
		if ( loaded.variant == JOZZ_RIG_PRISM_MAX && loaded.segments > JozzRig_ProbeMaxPrismSides() )
		{
			snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "'%s' zada %d scianek, ten build potrafi %d - NIE wczytano",
					  stem.c_str(), loaded.segments, JozzRig_ProbeMaxPrismSides() );
			return;
		}
		m_cfg = loaded;
		Build();
		m_liveTuned = false;
		snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "wczytano: %s%s", stem.c_str(), m_ok ? "" : "  (nie powstal)" );
		snprintf( m_loadedName, sizeof( m_loadedName ), "%s", stem.c_str() );
	}

	const char* ObsPath() const
	{
		const char* p = Env( "JOZZ_QC_OBS" );
		return p ? p : "jozz_quarter_car_observations.txt";
	}

	void WriteObservation()
	{
		FILE* f = fopen( ObsPath(), "ab" );
		if ( f == nullptr )
			return;
		time_t now = time( nullptr );
		char stamp[64];
		strftime( stamp, sizeof( stamp ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );
		char digest[900];
		JozzQc_ConfigDigest( &m_cfg, digest, sizeof( digest ) );
		fprintf( f,
				 "%s | sesja=%s | krok=%d | v=%.4f moment=%.2f sat=%d | skok=%.4f (%.4f..%.4f) | "
				 "a_pion=%.3f | styk=%d pkt airborne=%d | okno=%d krokow strata=%.1f W a_rms=%.4f "
				 "powietrze=%.2f%% churn=%.1f%% wazny=%d %s | zaburzen=%d %s | config=%s\n",
				 stamp, m_rig.perturbed ? "EXPLORATION" : "OBSERVATION", m_stepCount, m_last.speed,
				 m_last.driveTorque, m_last.saturated, m_last.travel, m_rig.travelLower, m_rig.travelUpper,
				 m_last.sprungAccelY, m_last.loadedPoints, m_last.airborne, m_winSteps, m_winLive.lossPower,
				 m_winLive.sprungAccelRms, 100.0 * m_winLive.airborneFraction, m_winLive.contactChurnPct,
				 m_winLive.invalid ? 0 : 1, m_winLive.invalid ? m_winLive.invalidWhy : "-", m_rig.perturbCount,
				 m_rig.perturbCount ? m_rig.lastPerturbation : "-", digest );
		fclose( f );
		m_obsCount += 1;
	}

	// ============================================================== stan
	// Chwyt myszy musi udzwignac 150 kg masy resorowanej na sprezynie, a nie tylko
	// 44 kg kola: framework skaluje ograniczenie sily ciezarem chwyconego ciala,
	// wiec mnoznik jest tu wyzszy niz domyslny.
	static constexpr float kGrabForce = 200.0f;

	JozzQcConfig m_cfg{};
	JozzQcConfig m_openCfg{}; // stan otwarcia okna - cel przycisku "RESET WARSZTATU"
	JozzQcConfig m_lastGood{};
	bool m_haveLastGood = false;
	JozzQcRig m_rig{};
	JozzQcSample m_last{};
	JozzQcWindow m_win{};
	JozzQcWindow m_winLive{};
	int m_winSteps = 0;

	JozzQcCandidate m_cand[JOZZ_QC_CANDIDATE_CAP]{};
	int m_candCount = 0;

	bool m_ok = false;
	char m_err[192] = "";
	char m_fixNote[160] = "";
	bool m_liveTuned = false;

	int m_stepCount = 0;
	int m_frameTick = 0;
	int m_dividerIndex = 0;
	int m_warmupSteps = JOZZ_QC_WARMUP_STEPS;
	bool m_autoWarmup = true;

	bool m_follow = true;
	bool m_rewind = true;
	int m_rewindCount = 0;
	bool m_showContacts = true;
	// Slad nadwozia DOMYSLNIE WYLACZONY: wykres 'skok' w zakladce Pomiar pokazuje
	// to samo ilosciowo, a przewyzszony slad przy 4 m/s ciagnie sie przez pol kadru.
	bool m_showTrail = false;
	bool m_showTravelRuler = true;
	float m_trailGain = 10.0f;
	float m_plotSpan = 6.0f;

	b3Pos m_trail[kTrailCap]{};
	int m_trailHead = 0;
	int m_trailCount = 0;

	int m_histCount = 0;
	float m_hT[kHistCap]{};
	float m_hAccel[kHistCap]{};
	float m_hTravel[kHistCap]{};
	float m_hTorque[kHistCap]{};
	float m_hWheelDy[kHistCap]{};
	float m_hLoaded[kHistCap]{};

	std::vector<Row> m_rows;
	int m_baseRow = 0;
	bool m_baseRowPinned = false;
	bool m_showDelta = true;
	bool m_fullProtocol = true;
	double m_measureMs = 0.0;
	char m_rowsMsg[256] = "";
	int m_sweepParam = 0;
	int m_sweepParamPrev = -1;
	float m_sweepFrom = 3.0f;
	float m_sweepTo = 42.0f;
	int m_sweepSteps = 6;

	// reka
	bool m_benchMode = false;
	JozzQcDrive m_driveSave = JOZZ_QC_DRIVE_SPEED;
	double m_targetSave = 0.0, m_startSave = 0.0, m_startXSave = 0.0;
	float m_liftH = 0.25f;
	float m_kickNs = 400.0f;
	bool m_shakerOn = false;
	float m_shakerHz = 1.5f;
	float m_shakerN = 400.0f;

	std::vector<std::string> m_shelf;
	char m_shelfName[64] = "narożnik";
	char m_shelfNote[160] = "";
	char m_shelfMsg[320] = "";
	char m_loadedName[64] = "";
	int m_obsCount = 0;
	char m_forceTab[32] = "";
	char m_openTab[32] = "Kolo";
	bool m_restored = false;
};

static Sample* CreateJozzQuarterCarScope( SampleContext* context )
{
	return new JozzQuarterCarScope( context );
}

static int sampleJozzQuarterCar = RegisterSample( "Jozz Wheel", "Quarter Car Scope", CreateJozzQuarterCarScope );
