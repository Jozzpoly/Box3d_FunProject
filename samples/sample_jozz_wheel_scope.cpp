// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// Wheel Scope - mikroskop na aktualny rig badawczy kola.
//
// To NIE jest warstwa wizualna produktu i nie jest edytorem. Jedyne zadanie:
// pokazac na zywo DOKLADNIE te fizyke, ktora stend mierzy headless, i pozwolic
// wlascicielowi wejsc z nia w kontrolowana interakcje.
//
// Niezmienniki, ktore ten plik ma utrzymac:
//  1. Fizyka ma JEDNA implementacje - jozz_wheel_rig.c, wspolna ze stendem.
//     Ten plik nie tworzy swiata, nie liczy sily napedu i nie dotyka ciala
//     poza jawnie oznaczonymi funkcjami zaburzajacymi.
//  2. Kamera, overlay, tempo rysowania i pauza NIE zmieniaja kroku fizyki.
//     Krok jest staly (1/60, 4 podkroki) i pochodzi z rigu. Slow motion
//     POMIJA kroki, nigdy ich nie skraca.
//  3. Kazda ingerencja w fizyke jest oznaczona i lepka. Sesja z ingerencja
//     nie jest evidence runem i mowi to wprost na ekranie.

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "gfx/utility.h"
#include "imgui.h"
#include "sample.h"

#include "jozz_wheel_rig.h"

#include "box3d/box3d.h"

#include <algorithm>
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

const char* Env( const char* name )
{
	const char* v = getenv( name );
	return ( v && v[0] ) ? v : nullptr;
}

// Konfiguracja przezywajaca "R". Framework na restarcie NISZCZY sample i tworzy
// go od nowa, wiec wszystko, co siedzi w polach obiektu, wraca do wartosci
// poczatkowych - dlatego "R" po cichu przywracalo sfere, mimo ze HUD obiecywal
// reset. Trzymamy wiec ostatnia konfiguracje obok obiektu i odtwarzamy ja, gdy
// framework zglasza restart TEGO SAMEGO sample'a.
JozzRigConfig g_lastConfig;
bool g_haveLastConfig = false;

// Polka konstrukcji. Katalog jest jawny i plaski, bo jego zawartosc ma byc
// czytelna z linii polecen i z gita - to samo wejscie bierze stend przez
// --rig-config, wiec zapisany przypadek da sie oddac agentowi bez tlumaczenia.
std::filesystem::path BenchDir()
{
	const char* p = getenv( "JOZZ_WHEEL_BENCH_DIR" );
	return ( p && p[0] ) ? std::filesystem::path( p ) : std::filesystem::path( "wheel_scope_bench" );
}

// Framework nazywa swoje ingerencje po angielsku, bo sample.cpp jest wspolny
// z upstreamem i nie jego rzecza jest jezyk UI konkretnego sampla. Tlumaczenie
// siedzi tutaj, czyli tam, gdzie ten napis faktycznie trafia na ekran. Nieznana
// nazwa przechodzi bez zmian - lepiej pokazac obcy napis niz zgubic ingerencje.
const char* PerturbationName( const char* what )
{
	if ( what == NULL )
		return "?";
	if ( strcmp( what, "mouse grab" ) == 0 )
		return "chwyt myszy (sprezyna na kole)";
	if ( strcmp( what, "launched cylinder" ) == 0 )
		return "wstrzelony walec";
	if ( strcmp( what, "launched ragdoll" ) == 0 )
		return "wstrzelony ragdoll";
	if ( strcmp( what, "launched sphere" ) == 0 )
		return "wstrzelona kula";
	return what;
}

// Nazwa pliku pochodzi od czlowieka, wiec nie moze wskazywac poza polke.
// Wszystko poza literami, cyframi, myslnikiem i podkresleniem staje sie '_'.
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
		out = "konstrukcja";
	return out;
}

} // namespace

class JozzWheelScope : public Sample
{
public:
	explicit JozzWheelScope( SampleContext* context )
		: Sample( context )
	{
		m_maxSides = JozzRig_ProbeMaxPrismSides();

		// Konfiguracja jest JEDYNYM zrodlem tozsamosci przebiegu. Domyslna to
		// dokladnie kontrakt Q2A; restart odtwarza ostatnia, zeby "R" znaczylo
		// "od nowa TO SAMO", a nie "wroc do sfery".
		m_cfg = JozzRig_DefaultConfig();
		m_cfg.prismSides = m_maxSides;
		if ( context->restart && g_haveLastConfig )
			m_cfg = g_lastConfig;

		// Tryb odcisku stanu: uruchamiany przez test ekwiwalencji, nie przez
		// czlowieka. Wymusza tempo 1:1, gasi pauze i BLOKUJE zaburzenia, zeby
		// przypadkowy klawisz nie mogl po cichu zepsuc dowodu.
		const char* tracePath = Env( "JOZZ_RIG_TRACE" );
		if ( tracePath )
		{
			m_traceSteps = Env( "JOZZ_RIG_TRACE_STEPS" ) ? atoi( Env( "JOZZ_RIG_TRACE_STEPS" ) ) : 600;
			m_trace = fopen( tracePath, "wb" );
			m_context->pause = false;

			// Pokretla dla testu niezaleznosci renderera. Zaden z nich nie moze
			// zmienic ani jednej cyfry w odcisku stanu - i wlasnie to sprawdzamy.
			m_dividerIndex = 0;
			if ( const char* d = Env( "JOZZ_RIG_TRACE_DIVIDER" ) )
			{
				int want = atoi( d );
				for ( int i = 0; i < kDividerCount; ++i )
					if ( kDividers[i] == want )
						m_dividerIndex = i;
			}
			if ( Env( "JOZZ_RIG_TRACE_VIEW_PLAIN" ) )
			{
				m_follow = false;		// kamera nieruchoma
				m_showContacts = false; // overlay wylaczony
				m_showForce = false;
				m_context->showUI = false;
				m_context->enableShadows = false;
				m_context->enableGtao = false;
			}
		}
		// Srodowisko wygrywa z odtworzona konfiguracja: test ekwiwalencji musi
		// dostac dokladnie ten wariant, o ktory prosi.
		if ( const char* v = Env( "JOZZ_RIG_VARIANT" ) )
		{
			m_cfg.variant = ( strcmp( v, "prism-Nmax" ) == 0 ) ? JOZZ_RIG_PRISM_MAX : JOZZ_RIG_SPHERE;
			m_cfg.prismSides = m_maxSides;
		}
		// Sweep po N i po predkosci ma dzialac BEZ preprocesora - to byl warunek
		// wyjscia tego etapu. Wczesniej zmiana ktorejkolwiek z tych dwoch liczb
		// wymagala przekompilowania rigu z podmienionym `#define`.
		if ( const char* s = Env( "JOZZ_RIG_SIDES" ) )
		{
			int want = atoi( s );
			if ( want >= 3 && want <= m_maxSides )
				m_cfg.prismSides = want;
		}
		if ( const char* s = Env( "JOZZ_RIG_START_V" ) )
		{
			double want = atof( s );
			if ( want > 0.0 && want < 200.0 )
			{
				m_cfg.startSpeed = want;
				m_cfg.targetSpeed = want;
			}
		}

		// Konstrukcja z pliku wygrywa ze WSZYSTKIM powyzej i celowo stoi na koncu:
		// plik konstrukcji jest pelna tozsamoscia przebiegu, wiec czesciowe
		// nadpisanie go zmienna srodowiskowa dalo by przebieg, ktorego nie opisuje
		// zaden zapis. Ta sama sciezka co --rig-config w stendzie - i wlasnie
		// dlatego bramka moze porownac oba frontendy na tym samym pliku.
		std::string fromEnv;
		if ( const char* cfgPath = Env( "JOZZ_RIG_CONFIG" ) )
		{
			char err[256];
			JozzRigConfig loaded;
			if ( JozzRig_ConfigReadFile( &loaded, cfgPath, err, sizeof( err ) ) )
			{
				m_cfg = loaded;
				fromEnv = std::filesystem::path( cfgPath ).stem().string();
			}
			else
			{
				snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "BLAD JOZZ_RIG_CONFIG: %s", err );
			}
		}

		Build();
		// Po Build(), bo Build() czysci nazwe - konfiguracja zmieniona recznie nie
		// moze dalej podawac sie za konstrukcje z pliku.
		snprintf( m_loadedName, sizeof( m_loadedName ), "%s", fromEnv.c_str() );
		RefreshShelf();

		// Nagrywanie uzbrojone ze srodowiska. Ta sama rola co JOZZ_RIG_TRACE:
		// pozwala sprawdzic sciezke nagrywania bez klikania w panel, wiec
		// "nagranie obejmuje swiat RIGU" jest testowalne, a nie deklarowane.
		if ( const char* rec = Env( "JOZZ_RIG_RECORD" ) )
		{
			snprintf( m_context->recordingFile, sizeof( m_context->recordingFile ), "%s", rec );
			StartRecording();
		}

		if ( context->restart == false )
		{
			// Widok z boku i lekko z gory: chodzi o to, zeby naraz bylo widac
			// fasetowanie obwodu i to, co dzieje sie przy gruncie.
			b3Pos p = b3Body_GetPosition( m_rig.body );
			p.y -= 0.22;
			m_camera->SetView( -52.0f, 9.0f, 2.6f, p );
		}
	}

	~JozzWheelScope() override
	{
		CloseTrace();
		ReleaseMouseGrab();
		// Przed zniszczeniem swiata rigu, zeby nagranie zamknelo sie na TYM
		// swiecie. Inaczej domyka je dopiero ~Sample, a tam wirtualne wywolanie
		// wraca juz do klasy bazowej i zatrzymuje nagrywanie na pustym swiecie.
		FinishRecording();
		JozzRig_Destroy( &m_rig );
	}

	// Swiat klasy bazowej istnieje, ale ten sample nigdy nie wklada do niego ciala
	// ani go nie krokuje. Kazda usluga frameworku, ktora go adresowala, dzialala
	// wiec na pustce: klikniecie nie zaznaczalo nic, Ctrl+przeciagniecie nie mialo
	// czego zlapac, zakladka Counters raportowala zera. Jedno nadpisanie prostuje
	// wszystkie naraz - z nagrywaniem wlacznie.
	// Gdy wariant jest nieprzedstawialny, rig NIE MA swiata (JozzRig_CreateFromConfig
	// zeruje strukture). Zwrocenie wtedy pustego identyfikatora wysadziloby wolajacych,
	// ktorzy pytaja bezwarunkowo - np. dopasowanie kaskady cieni zaraz po utworzeniu
	// sample'a. W tym stanie swiat bazowy jest jedynym, jaki naprawde istnieje.
	b3WorldId ActiveWorld() const override
	{
		return B3_IS_NON_NULL( m_rig.world ) ? m_rig.world : m_worldId;
	}

	// Framework wlasnie ruszyl fizyke: chwyt myszy albo wstrzelone cialo. Dopoki
	// te sciezki adresowaly pusty swiat, byly nieszkodliwe. Od teraz siegaja do
	// rigu, wiec MUSZA byc widoczne tak samo jak kopniak z klawiatury - inaczej
	// najlatwiejszy sposob zepsucia przebiegu bylby jedynym nieoznaczonym.
	void OnWorldPerturbed( const char* what ) override
	{
		JozzRig_MarkPerturbation( &m_rig, PerturbationName( what ) );
	}

	// ------------------------------------------------------------------ scena
	void Build()
	{
		if ( B3_IS_NON_NULL( m_rig.world ) )
		{
			// Chwyt myszy zyje w swiecie rigu, nie w naszym obiekcie. Puszczamy go
			// PRZED zniszczeniem swiata: inaczej identyfikatory stawu i ciala
			// kinematycznego przezylyby swoj swiat i przy puszczeniu przycisku
			// trafily w to, co zajelo ich miejsce w nowym.
			ReleaseMouseGrab();
			ClearSelection();

			// Tak samo jak Sample::CreateWorld: nagranie domykamy PRZED
			// zniszczeniem swiata, ktorego dotyczy. Przelaczenie wariantu w
			// trakcie nagrywania zapisuje wiec kompletny plik z tego, co bylo
			// do tej pory, zamiast po cichu przestac cokolwiek nagrywac.
			FinishRecording();
			JozzRig_Destroy( &m_rig );
		}

		// Bez hakow renderera swiat rigu rysuje sie jako pusty ekran - okno
		// pokazywalo tylko wlasny overlay i ani jednego collidera. Zlapane
		// przez ogladniecie zrzutu, nie przez test. Haki wyciagamy z definicji
		// probnej, bo adapter umie tylko wypelnic caly b3WorldDef - do rigu
		// trafiaja WYLACZNIE te trzy pola.
		b3WorldDef probe = b3DefaultWorldDef();
		AttachToWorldDef( &probe );
		JozzRigRenderHooks hooks;
		hooks.createDebugShape = probe.createDebugShape;
		hooks.destroyDebugShape = probe.destroyDebugShape;
		hooks.userDebugShapeContext = probe.userDebugShapeContext;

		m_ok = JozzRig_CreateFromConfig( &m_rig, &m_cfg, &hooks ) != 0;
		// Kazda przebudowa uniewaznia nazwe z polki. Przebudowa nastepuje po
		// KAZDEJ zmianie konstrukcji - takze z suwaka i z klawisza 1/2 - wiec bez
		// tego HUD twierdzilby, ze biezacy przebieg pochodzi z pliku, ktorego juz
		// nie opisuje. LoadConstruction ustawia nazwe z powrotem, PO Build().
		m_loadedName[0] = '\0';
		g_lastConfig = m_cfg;
		g_haveLastConfig = true;
		memset( &m_last, 0, sizeof( m_last ) );
		m_frameTick = 0;
		m_trailHead = 0;
		m_trailCount = 0;
		if ( m_trace )
		{
			fprintf( m_trace, "# variant=%s prism_sides=%d steps=%d dt=%.17g substeps=%d\n",
					 JozzRig_VariantName( m_cfg.variant ), m_rig.prismSides, m_traceSteps, m_cfg.dt,
					 m_cfg.substeps );
			// Pelna konfiguracja w naglowku: odcisk stanu bez opisu, z czego
			// powstal, nie jest odtwarzalny. Linie `#` sa metadanymi i test
			// ekwiwalencji ich nie porownuje.
			char cd[512];
			JozzRig_ConfigDigest( &m_cfg, cd, sizeof( cd ) );
			fprintf( m_trace, "# config %s\n", cd );
			fprintf( m_trace, "%s", JOZZ_RIG_DIGEST_HEADER );
			m_traceWritten = 0;
		}
	}

	// -------------------------------------------------------------------- krok
	void Step() override
	{
		if ( m_ok == false )
		{
			DrawTextLine( "wariant nieprzedstawialny - nic do pokazania" );
			return;
		}

		// Czy TA klatka posuwa fizyke? Pauza i pojedynczy krok naleza do
		// frameworku; dzielnik tempa jest nasz. Zaden z nich nie zmienia dt.
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

		m_didStep = false;
		if ( advance )
		{
			JozzRig_Step( &m_rig, &m_last );
			m_stepCount += 1;
			m_didStep = true;

			// Slad wysokosci srodka - WYLACZNIE widok. Toczenie gladkie daje linie
			// prosta, mlotkowanie daje falbanke. To najkrotsza droga od "widze,
			// ze cos jest nie tak" do liczby, ktora mozna zmierzyc headless.
			b3Pos c = b3Body_GetPosition( m_rig.body );
			m_trail[m_trailHead] = c;
			m_trailHead = ( m_trailHead + 1 ) % kTrailCap;
			if ( m_trailCount < kTrailCap )
				m_trailCount += 1;

			if ( m_trace && m_traceWritten < m_traceSteps )
			{
				char line[512];
				JozzRig_DigestLine( &m_rig, line, sizeof( line ) );
				fputs( line, m_trace );
				if ( ++m_traceWritten >= m_traceSteps )
					CloseTrace();
			}

			// Profil silnika: pierscien bazy pozostalby zerowy, bo nie wolamy
			// Sample::Step. Zerowe liczby w szufladzie metryk to klamstwo
			// o swiecie, a nie brak danych.
			if ( m_profileWriteIndex - m_profileReadIndex == m_profileCapacity )
				m_profileReadIndex += 1;
			m_currentProfileIndex = m_profileWriteIndex & ( m_profileCapacity - 1 );
			m_profiles[m_currentProfileIndex] = b3World_GetProfile( m_rig.world );
			m_profileWriteIndex += 1;
		}

		DrawHud();

		if ( m_follow )
		{
			b3Pos pivot = b3Body_GetPosition( m_rig.body );
			pivot.y -= 0.22; // celuj miedzy os i grunt, nie w sam srodek kola
			m_camera->SetPivot( pivot );
		}

		// Ogon rysowania, jak w Sample::Step. Kolejnosc jest istotna: origin
		// najpierw, bo kamera mogla sie przesunac przy sledzeniu ciala.
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

		b3Pos p = b3Body_GetPosition( m_rig.body );
		JozzWheelKin k = JozzRig_Kinematics( m_rig.body );

		// Diagnostyka rysuje sie w trybie DASHED: to, co jest za bryla, widac
		// przerywana linia. Bez tego os kola siedziala CALA wewnatrz sfery
		// i nie bylo jej na ekranie - sprawdzone na zrzucie.
		const OverlayOcclusionMode thru = OVERLAY_OCCLUSION_DASHED;

		// Os kola: to LOKALNE Y ciala, nie swiatowe Z. Rysowanie jej wprost jest
		// najtanszym sposobem zobaczenia, ze kolo nie zaczelo sie przekrecac.
		float halfW = 0.75f;
		b3Pos a1 = { p.x - k.axleUnit.x * halfW, p.y - k.axleUnit.y * halfW, p.z - k.axleUnit.z * halfW };
		b3Pos a2 = { p.x + k.axleUnit.x * halfW, p.y + k.axleUnit.y * halfW, p.z + k.axleUnit.z * halfW };
		DrawLineEx( a1, a2, MakeColor( b3_colorCyan ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );

		// Kierunek jazdy wyprowadzony z osi (up x axle), nie zalozony.
		b3Pos f2 = { p.x + k.forward.x, p.y + k.forward.y, p.z + k.forward.z };
		DrawArrowEx( p, f2, MakeColor( b3_colorLightGreen ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru, 0.2f );

		// Punkt odniesienia R pod srodkiem masy - z niego licza sie pola
		// reference_* w telemetrii. NIE jest to punkt kontaktu z manifoldu,
		// i wlasnie dlatego warto miec oba na ekranie naraz.
		b3Pos ref = { p.x, p.y - JOZZ_RIG_WHEEL_R, p.z };
		DrawCrossEx( ref, 0.16f, MakeColor( b3_colorMagenta ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );

		// Sila napedu w skali: pelna dlugosc = limit sily regulatora.
		if ( m_showForce && m_last.force != 0.0 )
		{
			float s = (float)( m_last.force / JOZZ_RIG_FMAX ) * 1.5f;
			b3Pos tip = { p.x + s, p.y, p.z };
			DrawArrowEx( p, tip, MakeColor( m_last.force >= 0.0 ? b3_colorOrange : b3_colorTomato ), 3.0f,
						 OVERLAY_THICKNESS_PIXELS, thru, 0.2f );
		}

		// Slad wysokosci srodka + linia nominalna R nad gruntem (gorna plaszczyzna
		// gruntu lezy na y = 0). Pionowe odchylenie bywa submilimetrowe, dlatego
		// jest suwak przewyzszenia - z JAWNA etykieta, ze to nie skala 1:1.
		if ( m_showTrail && m_trailCount > 1 )
		{
			for ( int i = 1; i < m_trailCount; ++i )
			{
				int i0 = ( m_trailHead - m_trailCount + i - 1 + 2 * kTrailCap ) % kTrailCap;
				int i1 = ( m_trailHead - m_trailCount + i + 2 * kTrailCap ) % kTrailCap;
				b3Pos a = m_trail[i0], b = m_trail[i1];
				a.y = JOZZ_RIG_WHEEL_R + ( a.y - JOZZ_RIG_WHEEL_R ) * m_trailGain;
				b.y = JOZZ_RIG_WHEEL_R + ( b.y - JOZZ_RIG_WHEEL_R ) * m_trailGain;
				DrawLineEx( a, b, MakeColor( b3_colorGold ), 2.0f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_DIM );
			}
			// Linia nominalna musi objac DOKLADNIE ten sam zakres x co slad -
			// dwa odcinki roznej dlugosci na tej samej wysokosci wygladaja
			// w perspektywie jak rozjezdzajace sie, i porownanie klamie.
			// Pola wypelniane po kopii, a nie lista inicjalizacyjna z literalami
			// double: b3Pos jest float albo double zaleznie od BOX3D_DOUBLE_PRECISION,
			// wiec `{ p.x + 1.0, ... }` jest zawezeniem w jednym z tych dwoch buildow.
			int iOld = ( m_trailHead - m_trailCount + 2 * kTrailCap ) % kTrailCap;
			b3Pos n0 = p, n1 = p;
			n0.x = m_trail[iOld].x;
			n1.x = p.x + 1.0f;
			n0.y = n1.y = JOZZ_RIG_WHEEL_R;
			DrawLineEx( n0, n1, MakeColor( b3_colorDimGray ), 1.5f, OVERLAY_THICKNESS_PIXELS, OVERLAY_OCCLUSION_DIM );
		}

		// Kontakt: punkty i normalne wprost z manifoldow ciala rigu.
		if ( m_showContacts )
		{
			JozzRigContactPoint cp[64];
			int n = JozzRig_ContactPoints( &m_rig, cp, 64 );
			m_contactCount = n;
			m_loadedCount = 0;
			for ( int i = 0; i < n; ++i )
			{
				bool loaded = cp[i].normalImpulse > 0.0f;
				if ( loaded )
					m_loadedCount += 1;
				DrawPointEx( cp[i].point, MakeColor( loaded ? b3_colorWhite : b3_colorYellow ),
							 loaded ? 13.0f : 7.0f, OVERLAY_THICKNESS_PIXELS, thru );
				float len = loaded ? 0.35f : 0.15f;
				b3Pos tip = { cp[i].point.x + cp[i].normal.x * len, cp[i].point.y + cp[i].normal.y * len,
							  cp[i].point.z + cp[i].normal.z * len };
				DrawLineEx( cp[i].point, tip, MakeColor( loaded ? b3_colorSkyBlue : b3_colorGray ), 2.0f,
							OVERLAY_THICKNESS_PIXELS, thru );
			}
		}
	}

	// Pasek rezimu. To jest odpowiedz na zmierzony fakt: przy domyslnej
	// konfiguracji rig jedzie 2.76x POWYZEJ predkosci, przy ktorej sztywny
	// wielokat w ogole utrzymuje ciagly kontakt - i nic tego nie mowilo.
	// Suwak celu ma zakres 4-20 m/s, wiec Owner przekracza te granice przy
	// niemal kazdej sesji. Liczby, nie ostrzezenie w dokumentacji.
	void DrawRegime()
	{
		// v_kryt opisuje obtaczanie sie wokol WIERZCHOLKA. Sfera zadnego nie ma,
		// wiec ta granica jej nie dotyczy - i zmierzone dane to potwierdzaja:
		// sfera ma 0% czasu w powietrzu na calym zakresie 1-18 m/s. Pokazywanie
		// jej "v/v_kryt 2.76" byloby dokladnie tym rodzajem liczby, ktory ten
		// pasek ma eliminowac.
		if ( m_cfg.variant == JOZZ_RIG_SPHERE )
		{
			DrawTextLine( "rezim    : gladka bryla - granica fasetowania nie dotyczy" );
			return;
		}

		double vcrit = JozzRig_CriticalSpeed( &m_cfg );
		double v = m_last.speed;
		double ratio = vcrit > 1e-9 ? v / vcrit : 0.0;
		double facets = JozzRig_FacetsPerStep( &m_cfg, v );

		DrawTextLine( "rezim    : v/v_kryt %.2f   v_kryt %.2f m/s  faset %.2f", ratio, vcrit, facets );

		if ( ratio > 1.0 )
			DrawTextLine( "  ! ponad v_kryt: kolo traci kontakt z gruntem" );
		else
			DrawTextLine( "  kontakt ciagly - jest model odniesienia" );

		// Fasety na krok > 1 znaczy, ze miedzy dwiema aktualizacjami manifoldu
		// mija cala scianka. b3Collide wykonuje sie RAZ na krok swiata, wiec
		// podkroki tego NIE ratuja - przesolwowuja ten sam manifold.
		if ( m_cfg.variant != JOZZ_RIG_SPHERE && facets > 1.0 )
			DrawTextLine( "  ! %.1f faset na krok - kontakt niedoprobkowany", facets );
	}

	// ------------------------------------------------------------------- HUD
	// Wspolny renderer tekstu przycina linie do 63 znakow (gfx/text.c). To nie
	// jest plik do zmiany pod wygode jednego okna, wiec HUD miesci sie w limicie.
	void DrawHud()
	{
		DrawTextLine( "WHEEL SCOPE - ta sama fizyka co stend headless" );
		DrawTextLine( "wariant  : %-11s scianek %d%s", JozzRig_VariantName( m_cfg.variant ),
					  m_cfg.variant == JOZZ_RIG_SPHERE ? 0 : m_rig.prismSides,
					  m_cfg.variant == JOZZ_RIG_SPHERE ? "  (kontrola)" : "" );
		DrawTextLine( "predkosc : %8.4f m/s  cel %.2f  blad %+.4f", m_last.speed, m_rig.targetSpeed,
					  m_last.error );
		DrawTextLine( "omega    : %9.4f rad/s  obwod %8.4f m/s", m_last.omegaSpin, m_last.refRimSpeed );
		DrawTextLine( "poslizg  : %+.4f m/s (odniesienie, nie manifold)", m_last.refSlipSpeed );
		DrawTextLine( "regulator: %s  sila %+8.1f N  limit %.0f", m_rig.controllerEnabled ? "PI ON" : "OFF  ",
					  m_last.force, m_cfg.fmax );
		DrawTextLine( "obciazenie %.0f N = %.0f grawitacja + %.0f docisk%s", m_rig.loadN,
					  (double)m_cfg.massKg * m_cfg.gravity, JozzRig_Downforce( &m_rig ),
					  m_last.saturated ? "  SAT" : "" );
		DrawRegime();
		DrawTextLine( "kontakt  : %d punktow, %d nosnych (impuls > 0)", m_contactCount, m_loadedCount );
		DrawTextLine( "krok %d  dystans %.3f m  obroty %.3f", m_rig.step, m_last.distance,
					  m_last.revolutions );
		// Praca liczy sie OD ostatniego zerowania (W). Bez tego jedna liczba
		// mieszalaby faze osiadania z jazda ustabilizowana - a stend raportuje
		// wylacznie okno pomiarowe.
		double perM = m_last.distance > 0.01 ? m_rig.wDriveSigned / m_last.distance : 0.0;
		DrawTextLine( "praca od zerowania %+.1f J   na metr %+.3f J/m", m_rig.wDriveSigned, perM );
		DrawTextLine( "wysokosc srodka %.5f m   nominal %.5f m", m_last.posY, (double)m_cfg.wheelR );
		DrawTextLine( "tempo: %s 1:%d   dt 1/%.0f s   %d podkrokow", m_context->pause ? "PAUZA" : "bieg ",
					  kDividers[m_dividerIndex], m_cfg.dt > 0.0 ? 1.0 / m_cfg.dt : 0.0, m_cfg.substeps );

		if ( m_rig.perturbed )
		{
			DrawTextLine( "SESJA: EXPLORATION - fizyka ruszona rekcznie %d x", m_rig.perturbCount );
			DrawTextLine( "  ostatnio: %.40s", m_rig.lastPerturbation );
			DrawTextLine( "  to NIE jest evidence run" );
		}
		else
		{
			DrawTextLine( "SESJA: OBSERVATION - zero ingerencji w fizyke" );
			DrawTextLine( "  obserwacja nie jest nowym faktem silnikowym" );
		}

		if ( m_trace )
			DrawTextLine( "trace: %d/%d krokow", m_traceWritten, m_traceSteps );
		DrawTextLine( "1/2 wariant  R reset  P pauza  O krok  , . tempo  W" );
		DrawTextLine( "C regulator  K kopniak  J zrzut  B hamulec" );
		DrawTextLine( "Ctrl+LPM chwyt kola - to TEZ jest ingerencja" );
		DrawTextLine( "V kamera  G zapis obserwacji  S zapis konstrukcji" );
		if ( m_loadedName[0] )
			DrawTextLine( "konstrukcja z polki: %.40s", m_loadedName );
	}

	// -------------------------------------------------------------- wejscie
	void Keyboard( int key, int action, int mods ) override
	{
		if ( action != ACTION_PRESS )
			return;
		(void)mods;

		switch ( key )
		{
			case SAPP_KEYCODE_1:
				if ( m_cfg.variant != JOZZ_RIG_SPHERE )
				{
					m_cfg.variant = JOZZ_RIG_SPHERE;
					Build();
				}
				return;
			case SAPP_KEYCODE_2:
				if ( m_cfg.variant != JOZZ_RIG_PRISM_MAX )
				{
					m_cfg.variant = JOZZ_RIG_PRISM_MAX;
					Build();
				}
				return;
			case SAPP_KEYCODE_COMMA:
				m_dividerIndex = m_dividerIndex > 0 ? m_dividerIndex - 1 : 0;
				return;
			case SAPP_KEYCODE_PERIOD:
				m_dividerIndex = m_dividerIndex < kDividerCount - 1 ? m_dividerIndex + 1 : m_dividerIndex;
				return;
			case KEY_V:
				m_follow = !m_follow;
				return;
			case SAPP_KEYCODE_G:
				WriteObservation();
				return;
			case SAPP_KEYCODE_S:
				// Skrot istnieje, bo interesujacy przypadek pojawia sie, gdy Owner
				// patrzy na kolo, a nie na panel. Siegniecie po myszke w tym
				// momencie kosztuje wlasnie ten stan, ktory mial byc zapisany.
				SaveConstruction();
				return;
			case KEY_W:
				// Zerowanie licznikow pracy = zdefiniowanie okna pomiarowego.
				// NIE jest zaburzeniem: nie dotyka stanu ciala.
				JozzRig_ResetWork( &m_rig );
				return;
			default:
				break;
		}

		// Ponizej sa juz WYLACZNIE zaburzenia fizyki. W trybie odcisku stanu sa
		// martwe: dowod ekwiwalencji nie moze zaleznec od tego, czy ktos nie
		// trafil w klawisz.
		if ( m_trace )
			return;

		switch ( key )
		{
			case SAPP_KEYCODE_C:
				JozzRig_SetControllerEnabled( &m_rig, m_rig.controllerEnabled ? 0 : 1 );
				break;
			case SAPP_KEYCODE_K:
			{
				b3Vec3 imp = { 0.0f, 0.0f, 260.0f }; // 260 N*s w bok
				JozzRig_ApplyImpulse( &m_rig, imp, "kopniak w bok +Z 260 Ns" );
				break;
			}
			case SAPP_KEYCODE_J:
			{
				b3Vec3 imp = { 0.0f, -260.0f, 0.0f };
				JozzRig_ApplyImpulse( &m_rig, imp, "zrzut w dol 260 Ns" );
				break;
			}
			case KEY_B:
			{
				b3Vec3 imp = { -260.0f, 0.0f, 0.0f };
				JozzRig_ApplyImpulse( &m_rig, imp, "impuls hamowania -X 260 Ns" );
				break;
			}
			default:
				break;
		}
	}

	// -------------------------------------------------------------- panel UI
	bool DrawControls() override
	{
		ImGui::TextUnformatted( "Wheel Scope - fizyczny mikroskop" );
		ImGui::TextWrapped( "Okno na ten sam rig, ktory stend mierzy headless. Nie jest to warstwa "
							"wizualna produktu ani edytor." );
		ImGui::Separator();

		int v = (int)m_cfg.variant;
		if ( ImGui::RadioButton( "sphere (kontrola)", &v, (int)JOZZ_RIG_SPHERE ) )
		{
			m_cfg.variant = JOZZ_RIG_SPHERE;
			Build();
		}
		char label[64];
		snprintf( label, sizeof( label ), "prism (%d scianek)", m_cfg.prismSides );
		if ( ImGui::RadioButton( label, &v, (int)JOZZ_RIG_PRISM_MAX ) )
		{
			m_cfg.variant = JOZZ_RIG_PRISM_MAX;
			Build();
		}

		// KONSTRUKCJA: zmiana wymaga przebudowy, wiec nie jest zaburzeniem
		// biegnacego przebiegu - jest nowym przebiegiem. Przebudowa kosztuje
		// 0.1 ms (zmierzone), wiec stosujemy ja przy puszczeniu suwaka: ciagniecie
		// zostaje plynne, a restart nastepuje raz.
		ImGui::SeparatorText( "Konstrukcja (przebudowa)" );

		int sides = m_cfg.prismSides;
		ImGui::BeginDisabled( m_cfg.variant == JOZZ_RIG_SPHERE );
		ImGui::SliderInt( "scianek", &sides, 3, m_maxSides );
		if ( ImGui::IsItemDeactivatedAfterEdit() && sides != m_cfg.prismSides )
		{
			m_cfg.prismSides = sides;
			Build();
		}
		ImGui::EndDisabled();
		if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
			ImGui::SetTooltip( "Glowna zmienna programu kol. Kolo produktu to prism-32:\n"
							   "b3CreateCylinder jest teselowany i ma twardy limit 32." );

		float v0 = (float)m_cfg.startSpeed;
		ImGui::SliderFloat( "predkosc startowa", &v0, 1.0f, 20.0f, "%.1f m/s" );
		if ( ImGui::IsItemDeactivatedAfterEdit() && (double)v0 != m_cfg.startSpeed )
		{
			m_cfg.startSpeed = (double)v0;
			m_cfg.targetSpeed = (double)v0; // start i cel razem: inaczej rig startuje w transiencie
			Build();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Warunek POCZATKOWY, nie zaburzenie. Suwak 'cel predkosci'\n"
							   "nizej zmienia biegnacy przebieg i oznacza sesje jako EXPLORATION." );

		if ( m_cfg.variant != JOZZ_RIG_SPHERE )
		{
			double vcrit = JozzRig_CriticalSpeed( &m_cfg );
			ImGui::Text( "v_kryt %.2f m/s  (start %.2f -> %.2fx)", vcrit, m_cfg.startSpeed,
						 vcrit > 1e-9 ? m_cfg.startSpeed / vcrit : 0.0 );
			if ( m_cfg.startSpeed > vcrit )
				ImGui::TextWrapped( "Powyzej v_kryt sztywne kolo nie utrzymuje ciaglego kontaktu "
									"i nie ma dla niego modelu odniesienia." );
		}
		ImGui::Separator();

		ImGui::Text( "tempo 1:%d klatek na krok", kDividers[m_dividerIndex] );
		ImGui::SliderInt( "spowolnienie", &m_dividerIndex, 0, kDividerCount - 1, "" );
		ImGui::TextWrapped( "Spowolnienie POMIJA klatki. Krok fizyki zostaje 1/60 s - inaczej "
							"obraz i pomiar przestalyby dotyczyc tej samej symulacji." );
		ImGui::Separator();

		ImGui::Checkbox( "punkty i normalne kontaktu", &m_showContacts );
		ImGui::Checkbox( "wektor sily napedu", &m_showForce );
		ImGui::Checkbox( "slad wysokosci srodka", &m_showTrail );
		if ( m_showTrail )
		{
			ImGui::SliderFloat( "przewyzszenie sladu", &m_trailGain, 1.0f, 500.0f, "x%.0f" );
			if ( m_trailGain > 1.5f )
				ImGui::TextWrapped( "UWAGA: slad jest przewyzszony w pionie x%.0f. To NIE jest "
									"skala 1:1 - sluzy do zobaczenia kierunku zjawiska, nie "
									"do odczytu amplitudy.",
									m_trailGain );
		}
		ImGui::Checkbox( "kamera sledzi kolo", &m_follow );
		ImGui::Separator();

		ImGui::TextUnformatted( "Zaburzenia (zmieniaja fizyke):" );
		bool ctrl = m_rig.controllerEnabled != 0;
		if ( ImGui::Checkbox( "regulator PI wlaczony", &ctrl ) )
			JozzRig_SetControllerEnabled( &m_rig, ctrl ? 1 : 0 );
		float target = (float)m_rig.targetSpeed;
		if ( ImGui::SliderFloat( "cel predkosci", &target, 4.0f, 20.0f, "%.1f m/s" ) )
			JozzRig_SetTargetSpeed( &m_rig, (double)target );
		float load = (float)m_rig.loadN;
		if ( ImGui::SliderFloat( "obciazenie", &load, 600.0f, 3000.0f, "%.0f N" ) )
			JozzRig_SetLoad( &m_rig, (double)load );

		if ( m_rig.perturbed )
		{
			ImGui::Separator();
			ImGui::TextWrapped( "EXPLORATION: fizyka ruszona rekcznie %d x (ostatnio: %s). Ta sesja "
								"generuje hipotezy, nie dowody. Reset: R.",
								m_rig.perturbCount, m_rig.lastPerturbation );
		}
		ImGui::Separator();
		if ( ImGui::Button( "zapisz obserwacje (G)" ) )
			WriteObservation();
		if ( m_obsCount > 0 )
			ImGui::Text( "zapisanych obserwacji: %d -> %s", m_obsCount, ObsPath() );

		// POLKA: zapisany plik to nie zrzut ekranu ani notatka, tylko wykonywalne
		// wejscie. Ten sam plik wczytuje sie tutaj i podaje stendowi przez
		// --rig-config, wiec "ciekawy przypadek" przestaje byc czyms, co trzeba
		// odtworzyc z pamieci suwakow.
		ImGui::SeparatorText( "Polka konstrukcji" );
		ImGui::TextWrapped( "Zapisuje KONSTRUKCJE (z czego zbudowac przebieg), nie wynik. "
							"Ten sam plik uruchomi stend: wheel_bench --rig-trace out.csv "
							"--rig-config <plik>." );
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
		{
			ImGui::TextDisabled( "polka pusta (%s)", BenchDir().string().c_str() );
		}
		else
		{
			for ( const std::string& s : m_shelf )
			{
				ImGui::PushID( s.c_str() );
				if ( ImGui::SmallButton( "wczytaj" ) )
					LoadConstruction( s );
				ImGui::SameLine();
				ImGui::TextUnformatted( s.c_str() );
				ImGui::PopID();
			}
		}
		if ( m_shelfMsg[0] )
			ImGui::TextWrapped( "%s", m_shelfMsg );
		return true;
	}

	// Solver frameworku ustawialby dt i podkroki spod nas. Krok nalezy do rigu.
	bool HasSolverControls() const override
	{
		return false;
	}

	// ...ale nagrywanie jest czym innym niz sterowanie solverem. Swiat rigu jest
	// realnie krokowany, wiec ma co nagrywac - i nagranie otwiera sie potem w
	// przegladarce Replay (przewijanie, klatka po klatce, wykrywanie rozjazdu).
	bool HasRecordingControls() const override
	{
		return true;
	}

	bool CondenseDebugOverlay() const override
	{
		return true;
	}

	b3BodyId FocusBody() const override
	{
		return m_rig.body;
	}

private:
	const char* ObsPath() const
	{
		const char* p = Env( "JOZZ_WHEEL_OBS" );
		return p ? p : "jozz_wheel_scope_observations.txt";
	}

	// Zaklaka obserwacji: to co Jozz widzi, plus dokladny kontekst techniczny
	// tej chwili. Bez kontekstu obserwacja jest niepodpieta do niczego.
	void WriteObservation()
	{
		FILE* f = fopen( ObsPath(), "ab" );
		if ( f == nullptr )
			return;
		time_t now = time( nullptr );
		char stamp[64];
		strftime( stamp, sizeof( stamp ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );
		char digest[512];
		JozzRig_DigestLine( &m_rig, digest, sizeof( digest ) );
		char cfgDigest[512];
		JozzRig_ConfigDigest( &m_cfg, cfgDigest, sizeof( cfgDigest ) );
		double vcrit = JozzRig_CriticalSpeed( &m_cfg );
		fprintf( f,
				 "%s | sesja=%s | wariant=%s scianek=%d | krok=%d | v=%.4f cel=%.2f blad=%+.4f | "
				 "omega=%.4f poslizg=%+.4f | sila=%+.1f sat=%d | obciazenie=%.0f | kontakt=%d/%d nosnych | "
				 "dystans=%.3f obroty=%.3f praca=%+.2f | zaburzen=%d ostatnie=%s | "
				 "v_kryt=%.4f v/v_kryt=%.3f faset_na_krok=%.3f | config=%s | digest=%s",
				 stamp, m_rig.perturbed ? "EXPLORATION" : "OBSERVATION", JozzRig_VariantName( m_cfg.variant ),
				 m_cfg.variant == JOZZ_RIG_SPHERE ? 0 : m_rig.prismSides, m_rig.step, m_last.speed,
				 m_rig.targetSpeed, m_last.error, m_last.omegaSpin, m_last.refSlipSpeed, m_last.force,
				 m_last.saturated, m_rig.loadN, m_loadedCount, m_contactCount, m_last.distance,
				 m_last.revolutions, m_rig.wDriveSigned, m_rig.perturbCount,
				 m_rig.perturbCount ? m_rig.lastPerturbation : "-", vcrit,
				 vcrit > 1e-9 ? m_last.speed / vcrit : 0.0, JozzRig_FacetsPerStep( &m_cfg, m_last.speed ),
				 cfgDigest, digest );
		fclose( f );
		m_obsCount += 1;
	}

	// ------------------------------------------------------ polka konstrukcji
	void RefreshShelf()
	{
		m_shelf.clear();
		std::error_code ec;
		for ( const auto& e : std::filesystem::directory_iterator( BenchDir(), ec ) )
		{
			if ( e.is_regular_file( ec ) && e.path().extension() == ".rig" )
				m_shelf.push_back( e.path().stem().string() );
		}
		std::sort( m_shelf.begin(), m_shelf.end() );
	}

	// Zapis konstrukcji, NIE zapis wyniku. Plik opisuje z czego zbudowac przebieg
	// od nowa - dlatego wedruja do niego tylko pola JozzRigConfig, a wszystko, co
	// opisuje TEN konkretny przebieg (krok, klasa sesji, ustawiony na zywo cel),
	// idzie do komentarza. Inaczej "wczytaj konstrukcje" znaczyloby raz jedno,
	// raz drugie, zaleznie od tego, kiedy Owner nacisnal zapis.
	void SaveConstruction()
	{
		std::error_code ec;
		std::filesystem::create_directories( BenchDir(), ec );

		std::string name = SafeName( m_shelfName );
		std::filesystem::path path = BenchDir() / ( name + ".rig" );
		for ( int n = 2; std::filesystem::exists( path ) && n < 1000; ++n )
		{
			// Nie nadpisujemy cicho. Ciekawy przypadek sprzed godziny jest wart
			// tyle samo co ten sprzed minuty, a Owner nie ma jak cofnac utraty.
			char suffix[16];
			snprintf( suffix, sizeof( suffix ), "_%d", n );
			path = BenchDir() / ( name + suffix + ".rig" );
		}

		time_t now = time( nullptr );
		char stamp[64];
		strftime( stamp, sizeof( stamp ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );

		// Kontekst chwili zapisu. Nie jest czescia konstrukcji i wlasnie dlatego
		// jest komentarzem: przy wczytaniu nie wplywa na nic, ale mowi, skad sie
		// wziela i czy przebieg, ktory ja zainspirowal, byl czysty.
		char note[512];
		double vcrit = JozzRig_CriticalSpeed( &m_cfg );
		snprintf( note, sizeof( note ),
				  "%s | %s | zapisane w kroku %d przy v=%.4f | sesja=%s%s | v_kryt=%.4f v/v_kryt=%.3f",
				  stamp, m_shelfNote[0] ? m_shelfNote : "(bez notatki)", m_rig.step, m_last.speed,
				  m_rig.perturbed ? "EXPLORATION" : "OBSERVATION",
				  m_rig.perturbed ? " - przebieg byl ruszony recznie" : "", vcrit,
				  vcrit > 1e-9 ? m_last.speed / vcrit : 0.0 );

		if ( JozzRig_ConfigWriteFile( &m_cfg, path.string().c_str(), note ) )
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
		std::filesystem::path path = BenchDir() / ( stem + ".rig" );
		JozzRigConfig loaded;
		char err[256];
		if ( JozzRig_ConfigReadFile( &loaded, path.string().c_str(), err, sizeof( err ) ) == 0 )
		{
			snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "BLAD odczytu: %s", err );
			return;
		}
		// Nie przycinamy N po cichu. Plik zadajacy wiecej scianek, niz ten build
		// potrafi zbudowac, opisuje inna konstrukcje niz ta, ktora dalo by sie
		// uruchomic - a milczaca podmiana bylaby najgorsza z trzech odpowiedzi.
		if ( loaded.variant != JOZZ_RIG_SPHERE && loaded.prismSides > m_maxSides )
		{
			snprintf( m_shelfMsg, sizeof( m_shelfMsg ),
					  "'%s' zada %d scianek, ten build potrafi %d - NIE wczytano", stem.c_str(),
					  loaded.prismSides, m_maxSides );
			return;
		}
		m_cfg = loaded;
		Build();
		snprintf( m_shelfMsg, sizeof( m_shelfMsg ), "wczytano: %s%s", stem.c_str(),
				  m_ok ? "" : "  (wariant nieprzedstawialny)" );
		snprintf( m_loadedName, sizeof( m_loadedName ), "%s", stem.c_str() );
	}

	void CloseTrace()
	{
		if ( m_trace )
		{
			fflush( m_trace );
			fclose( m_trace );
			m_trace = nullptr;
		}
	}

	JozzRig m_rig{};
	JozzRigSample m_last{};
	JozzRigConfig m_cfg{};
	int m_maxSides = 8; // zdolnosc buildu, nie tozsamosc fizyczna
	int m_dividerIndex = 0;
	int m_frameTick = 0;
	int m_contactCount = 0;
	int m_loadedCount = 0;
	int m_obsCount = 0;
	bool m_ok = false;
	bool m_follow = true;
	bool m_showContacts = true;
	bool m_showForce = true;
	bool m_showTrail = true;
	float m_trailGain = 1.0f;

	static constexpr int kTrailCap = 360; // 6 s przy 60 Hz
	b3Pos m_trail[kTrailCap]{};
	int m_trailHead = 0;
	int m_trailCount = 0;

	FILE* m_trace = nullptr;
	int m_traceSteps = 0;
	int m_traceWritten = 0;

	// polka konstrukcji
	std::vector<std::string> m_shelf;
	char m_shelfName[64] = "konstrukcja";
	char m_shelfNote[160] = "";
	char m_shelfMsg[320] = "";
	char m_loadedName[64] = "";
};

static Sample* CreateJozzWheelScope( SampleContext* context )
{
	return new JozzWheelScope( context );
}

static int sampleJozzWheelScope = RegisterSample( "Jozz Wheel", "Wheel Scope", CreateJozzWheelScope );
