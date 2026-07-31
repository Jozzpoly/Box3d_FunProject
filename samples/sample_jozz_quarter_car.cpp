// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT
//
// Quarter Car Scope - okno na rig Q3.
//
// Ten sam kontrakt co Wheel Scope i z tego samego powodu:
//  1. Fizyka ma JEDNA implementacje - jozz_qc_rig.c, wspolna ze stendem. Ten
//     plik nie tworzy swiata, nie liczy momentu i nie dotyka cial.
//  2. Kamera, overlay i tempo NIE zmieniaja kroku fizyki. Spowolnienie POMIJA
//     klatki, nigdy nie skraca dt.
//  3. Kazda ingerencja jest oznaczona i lepka - a przebieg z ingerencja jest
//     przez rig oznaczany jako NIEWAZNY, tak samo jak headless.
//
// Po co okno, skoro liczby sa w tabeli: liczba mowi, ze `sprung_accel_rms` spadl
// o 83%, ale nie mowi, JAK to wyglada. Werdykt o feelu nalezy do Jozza i nie da
// sie go wydac z pliku CSV (reguła twarda 5).

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "sample.h"

#include "jozz_qc_rig.h"
#include "jozz_wheel_rig.h"

#include "box3d/box3d.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

constexpr int kDividers[] = { 1, 2, 4, 8, 16 };
constexpr int kDividerCount = (int)( sizeof( kDividers ) / sizeof( kDividers[0] ) );

// Kandydaci widoczni w oknie = kandydaci z macierzy porownawczej stendu.
// Rozjazd tych dwoch list oznaczalby, ze Owner ocenia feel czegos innego niz
// to, co zostalo zmierzone.
struct Candidate
{
	const char* label;
	JozzRigVariant variant;
	int segments;
};

const Candidate kCandidates[] = {
	{ "sphere (kontrola)", JOZZ_RIG_SPHERE, 0 },
	{ "prism-Nmax (dzis)", JOZZ_RIG_PRISM_MAX, 42 },
	{ "prism-32", JOZZ_RIG_PRISM_MAX, 32 },
	{ "torus-32 (nowy)", JOZZ_RIG_TORUS, 32 },
	{ "torus-64 (nowy)", JOZZ_RIG_TORUS, 64 },
};
constexpr int kCandidateCount = (int)( sizeof( kCandidates ) / sizeof( kCandidates[0] ) );

JozzQcConfig g_lastConfig;
bool g_haveLastConfig = false;

const char* Env( const char* name )
{
	const char* v = getenv( name );
	return ( v && v[0] ) ? v : nullptr;
}

} // namespace

class JozzQuarterCarScope : public Sample
{
public:
	explicit JozzQuarterCarScope( SampleContext* context )
		: Sample( context )
	{
		m_cfg = JozzQc_DefaultConfig();
		m_cfg.variant = JOZZ_RIG_TORUS;
		m_cfg.segments = 64;
		m_candidate = 4;
		if ( context->restart && g_haveLastConfig )
		{
			m_cfg = g_lastConfig;
			m_candidate = MatchCandidate( m_cfg );
		}

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
		m_candidate = MatchCandidate( m_cfg );

		Build();

		if ( context->restart == false )
		{
			// Kadr musi objac NARAZ: grunt, obwiednie i punkt mocowania nadwozia.
			// Przy ciasniejszym ustawieniu widac tylko kolo - a pytanie Q3 brzmi,
			// co z udaru dochodzi WYZEJ, wiec zawieszenie musi byc w kadrze.
			b3Pos p = b3Body_GetPosition( m_rig.wheel );
			p.y += 0.25f;
			m_camera->SetView( -60.0f, 10.0f, 5.2f, p );
		}
	}

	~JozzQuarterCarScope() override
	{
		ReleaseMouseGrab();
		FinishRecording();
		JozzQc_Destroy( &m_rig );
	}

	// Ten sample nie uzywa swiata klasy bazowej - wszystkie uslugi frameworku
	// (zaznaczanie, chwyt myszy, licznik cial, nagrywanie) maja adresowac rig.
	b3WorldId ActiveWorld() const override
	{
		return B3_IS_NON_NULL( m_rig.world ) ? m_rig.world : m_worldId;
	}

	void OnWorldPerturbed( const char* what ) override
	{
		JozzQc_MarkPerturbation( &m_rig, what ? what : "?" );
	}

	static int MatchCandidate( const JozzQcConfig& c )
	{
		for ( int i = 0; i < kCandidateCount; ++i )
			if ( kCandidates[i].variant == c.variant &&
				 ( c.variant == JOZZ_RIG_SPHERE || kCandidates[i].segments == c.segments ) )
				return i;
		return -1;
	}

	void Build()
	{
		if ( B3_IS_NON_NULL( m_rig.world ) )
		{
			ReleaseMouseGrab();
			ClearSelection();
			FinishRecording();
			JozzQc_Destroy( &m_rig );
		}

		b3WorldDef probe = b3DefaultWorldDef();
		AttachToWorldDef( &probe );
		JozzRigRenderHooks hooks;
		hooks.createDebugShape = probe.createDebugShape;
		hooks.destroyDebugShape = probe.destroyDebugShape;
		hooks.userDebugShapeContext = probe.userDebugShapeContext;

		m_ok = JozzQc_Create( &m_rig, &m_cfg, &hooks, m_err, sizeof( m_err ) ) != 0;
		g_lastConfig = m_cfg;
		g_haveLastConfig = true;
		memset( &m_last, 0, sizeof( m_last ) );
		m_stepCount = 0;
		m_frameTick = 0;
		m_trailCount = 0;
		m_trailHead = 0;
		JozzQc_WindowBegin( &m_win );
		m_winLive = m_win;
		m_winSteps = 0;
	}

	void Step() override
	{
		if ( m_ok == false )
		{
			DrawTextLine( "rig nie powstal: %s", m_err );
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
			if ( m_stepCount > kWarmupSteps )
			{
				JozzQc_WindowAdd( &m_win, &m_last );
				m_winSteps += 1;
				if ( ( m_winSteps % 30 ) == 0 )
				{
					m_winLive = m_win;
					JozzQc_WindowEnd( &m_winLive, &m_rig );
				}
			}

			b3Pos c = b3Body_GetPosition( m_rig.chassis );
			m_trail[m_trailHead] = c;
			m_trailHead = ( m_trailHead + 1 ) % kTrailCap;
			if ( m_trailCount < kTrailCap )
				m_trailCount += 1;
		}

		DrawHud();

		if ( m_follow )
		{
			b3Pos pivot = b3Body_GetPosition( m_rig.wheel );
			pivot.y += 0.25f;
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
			}
		}

		// Punkt mocowania zawieszenia i os kola: bez nich nie widac, czy skok
		// pracuje, czy nadwozie po prostu jedzie z kolem jak sztywna bryla.
		{
			b3Pos pc = b3Body_GetPosition( m_rig.chassis );
			b3Pos pw = b3Body_GetPosition( m_rig.wheel );
			b3Pos mount = { pc.x, pc.y - m_cfg.mountH, pc.z };
			DrawLineEx( pc, mount, MakeColor( b3_colorSkyBlue ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );
			DrawCrossEx( mount, 0.14f, MakeColor( b3_colorMagenta ), 2.0f, OVERLAY_THICKNESS_PIXELS, thru );
			DrawLineEx( mount, pw, MakeColor( b3_colorOrange ), 3.0f, OVERLAY_THICKNESS_PIXELS, thru );
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

	void DrawHud()
	{
		DrawTextLine( "Q3 QUARTER CAR   krok %d   %s  N=%d  ksztaltow %d", m_stepCount,
					  JozzRig_VariantName( m_cfg.variant ),
					  m_cfg.variant == JOZZ_RIG_SPHERE ? 0 : m_cfg.segments, m_rig.shapeCount );
		DrawTextLine( "droga %s   v %.2f / %.2f m/s   moment %.1f Nm%s", JozzQc_RoadName( m_cfg.road ),
					  m_last.speed, m_cfg.targetSpeed, m_last.driveTorque, m_last.saturated ? "  SATURACJA" : "" );
		DrawTextLine( "skok %.1f mm (statyka %.1f)   a_pion %.2f m/s2   %s", 1000.0 * m_last.travel,
					  1000.0 * JozzQc_StaticSag( &m_cfg ), m_last.sprungAccelY,
					  m_last.airborne ? "KOLO W POWIETRZU" : "styk" );
		if ( m_winSteps > 0 )
		{
			DrawTextLine( "okno %d krokow:  moment %.1f Nm   strata %.0f W   a_rms %.3f   powietrze %.1f%%",
						  m_winSteps, m_winLive.driveTorqueMean, m_winLive.lossPower, m_winLive.sprungAccelRms,
						  100.0 * m_winLive.airborneFraction );
			if ( m_winLive.invalid )
				DrawTextLine( "PRZEBIEG NIEWAZNY: %s", m_winLive.invalidWhy );
		}
		if ( m_rig.perturbed )
			DrawTextLine( "EXPLORATION: fizyka ruszona recznie %dx (%s)", m_rig.perturbCount,
						  m_rig.lastPerturbation );
	}

	void DrawSampleWindows() override
	{
		float h = 30.0f * ImGui::GetFontSize();
		ImGui::SetNextWindowPos( ImVec2( 0.5f * ImGui::GetFontSize(), ImGui::GetIO().DisplaySize.y - h - 50.0f ),
								 ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 22.0f * ImGui::GetFontSize(), h ), ImGuiCond_Once );
		ImGui::Begin( "Quarter Car Scope", nullptr, ImGuiWindowFlags_NoResize );

		ImGui::TextUnformatted( "Q3 - kolo na zawieszeniu pod masa resorowana" );
		ImGui::TextWrapped( "To samo, co stend mierzy headless (--qc-compare). Werdykt o feelu "
							"nalezy do Ciebie - liczby ponizej go nie zastepuja." );
		ImGui::Separator();

		ImGui::SeparatorText( "Kandydat (przebudowa)" );
		for ( int i = 0; i < kCandidateCount; ++i )
		{
			if ( ImGui::RadioButton( kCandidates[i].label, &m_candidate, i ) )
			{
				m_cfg.variant = kCandidates[i].variant;
				if ( kCandidates[i].variant != JOZZ_RIG_SPHERE )
					m_cfg.segments = kCandidates[i].segments;
				Build();
			}
		}
		{
			JozzRigConfig w = JozzQc_WheelConfig( &m_cfg );
			double ripple = JozzRig_EnvelopeRipple( &w );
			ImGui::Text( "tetnienie promienia %.3f mm,  ksztaltow %d", 1000.0 * ripple, m_rig.shapeCount );
			if ( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "R nominalne minus R najmniejsze. Jedyna liczba porownywalna\n"
								   "miedzy pryzmatem a pierscieniem kapsul wprost.\n"
								   "UWAGA: pomiar pokazal, ze to NIE ona rzadzi jakoscia toczenia." );
			if ( m_cfg.variant == JOZZ_RIG_TORUS )
			{
				float crown = m_cfg.crownR;
				ImGui::SliderFloat( "promien korony", &crown, 0.05f, 0.5f * m_cfg.wheelW, "%.3f m" );
				if ( ImGui::IsItemDeactivatedAfterEdit() && crown != m_cfg.crownR )
				{
					m_cfg.crownR = crown;
					Build();
				}
				if ( ImGui::IsItemHovered() )
					ImGui::SetTooltip( "Promien przekroju opony = promien kapsuly.\n"
									   "Wiekszy = gladziej, ale wezsza plaska biezni." );
				int nMin = JozzRig_MinTorusSegments( &w );
				ImGui::Text( "minimum kapsul dla szczelnosci: %d", nMin );
			}
		}

		ImGui::SeparatorText( "Droga (przebudowa)" );
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
		ImGui::TextWrapped( "Na plaskiej plycie Q3 mierzy fasety obwiedni, a nie transfer udaru." );

		ImGui::SeparatorText( "Warunki (przebudowa)" );
		float v = (float)m_cfg.targetSpeed;
		ImGui::SliderFloat( "predkosc", &v, 1.0f, 20.0f, "%.1f m/s" );
		if ( ImGui::IsItemDeactivatedAfterEdit() && (double)v != m_cfg.targetSpeed )
		{
			m_cfg.targetSpeed = v;
			m_cfg.startSpeed = v;
			Build();
		}
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Powyzej ~4.8 m/s sztywne kolo nie utrzymuje ciaglego styku,\n"
							   "a struktura obwiedni schodzi ponizej rozdzielczosci solvera\n"
							   "(przy 13 m/s mija ok. 2.2 elementu na krok fizyki)." );
		float k = m_cfg.springNPerM;
		ImGui::SliderFloat( "sprezyna", &k, 6000.0f, 30000.0f, "%.0f N/m" );
		if ( ImGui::IsItemDeactivatedAfterEdit() && k != m_cfg.springNPerM )
		{
			m_cfg.springNPerM = k;
			Build();
		}
		ImGui::Text( "hertz wiezu %.4f, ugiecie statyczne %.1f mm", JozzQc_Hertz( &m_cfg ),
					 1000.0 * JozzQc_StaticSag( &m_cfg ) );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "suspensionHertz NIE jest czestotliwoscia resorowania: idzie za\n"
							   "masa ZREDUKOWANA obu cial (34.02 kg), nie za resorowana.\n"
							   "Dlatego rig przyjmuje N/m, a herce wylicza." );

		ImGui::SeparatorText( "Widok" );
		ImGui::Text( "tempo 1:%d klatek na krok", kDividers[m_dividerIndex] );
		ImGui::SliderInt( "spowolnienie", &m_dividerIndex, 0, kDividerCount - 1, "" );
		ImGui::Checkbox( "punkty kontaktu", &m_showContacts );
		ImGui::Checkbox( "slad nadwozia", &m_showTrail );
		if ( m_showTrail )
			ImGui::SliderFloat( "przewyzszenie", &m_trailGain, 1.0f, 200.0f, "x%.0f" );
		ImGui::Checkbox( "kamera sledzi kolo", &m_follow );

		if ( ImGui::Button( "zeruj okno pomiarowe" ) )
		{
			JozzQc_WindowBegin( &m_win );
			m_winSteps = 0;
		}

		ImGui::End();
	}

private:
	static constexpr int kWarmupSteps = 120;
	static constexpr int kTrailCap = 360;

	JozzQcConfig m_cfg{};
	JozzQcRig m_rig{};
	JozzQcSample m_last{};
	JozzQcWindow m_win{};
	JozzQcWindow m_winLive{};
	int m_winSteps = 0;

	bool m_ok = false;
	char m_err[192] = "";
	int m_candidate = 0;
	int m_stepCount = 0;
	int m_frameTick = 0;
	int m_dividerIndex = 0;

	bool m_follow = true;
	bool m_showContacts = true;
	bool m_showTrail = true;
	float m_trailGain = 10.0f;

	b3Pos m_trail[kTrailCap]{};
	int m_trailHead = 0;
	int m_trailCount = 0;
};

static Sample* CreateJozzQuarterCarScope( SampleContext* context )
{
	return new JozzQuarterCarScope( context );
}

static int sampleJozzQuarterCar = RegisterSample( "Jozz Wheel", "Quarter Car Scope", CreateJozzQuarterCarScope );
