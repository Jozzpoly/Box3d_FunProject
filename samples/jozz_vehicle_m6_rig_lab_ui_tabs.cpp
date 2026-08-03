// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_rig_lab_internal.h"

#include <chrono>  // scan reader load-time measurement
#include <cstdio>  // JOZZ_SCAN_DUMP timing output
#include <cstdlib> // getenv
#include <cstring> // strncmp: split teleport anchors by segment

	// ---- Tabbed control panel. Live sliders act immediately; anything that
	// rebuilds bodies goes through the pending-edit + Apply pattern, and the
	// Apply bar at the bottom stays visible from every tab.
	//
	// UX conventions used throughout these tabs:
	//  - One short line of context per group, not a paragraph. Deeper "why /
	//    when / what breaks" explanations live behind a "(?)" HelpMarker so the
	//    panel stays scannable but nothing is hidden.
	//  - Slider ranges are cropped tight around a sensible vehicle span (drift
	//    car to light truck) so dragging can actually hit a precise value.
	//    Ctrl+click any slider to type an exact number outside that range.
	//  - SectionHeader marks a logical group; CollapsingHeader (closed by
	//    default) hides advanced hardpoint geometry most users never touch.

	// Small "(?)" marker that shows a tooltip on hover - the standard Dear
	// ImGui pattern for keeping detail out of the main flow without hiding it.
void JozzVehicleM6RigLab::HelpMarker( const char* text )
	{
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() )
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos( ImGui::GetFontSize() * 28.0f );
			ImGui::TextUnformatted( text );
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

void JozzVehicleM6RigLab::SectionHeader( const char* title )
	{
		ImGui::Spacing();
		ImGui::TextColored( ImVec4( 0.45f, 0.75f, 1.0f, 1.0f ), "%s", title );
		ImGui::Separator();
	}

void JozzVehicleM6RigLab::DrawDriveTab()
	{
		ImGui::TextWrapped( "Silnik trzyma limit obrotów, gaz skaluje moment." );
		HelpMarker( "To, czy koło się trzyma czy traci przyczepność (pali gumę), zależy od momentu względem "
					"przyczepności w kontakcie z podłożem - nie ma tu osobnego 'przełącznika poślizgu'." );
		ImGui::SliderFloat( "Moment napędowy", &m_config.maxDriveTorque, 0.0f, 2000.0f, "%.0f N*m" );
		HelpMarker( "Maksymalny moment silnika na koło. Więcej = mocniejsze przyspieszenie, ale też łatwiej "
					"przekręcić koła (wheelspin) na śliskiej nawierzchni." );
		ImGui::SliderFloat( "Limit obrotów", &m_config.maxDriveSpeed, 5.0f, 100.0f, "%.0f rad/s" );
		HelpMarker( "Prędkość obrotowa koła (rad/s), przy której silnik przestaje ciągnąć - decyduje razem z "
					"promieniem koła o prędkości maksymalnej auta." );
		ImGui::SliderFloat( "Próg spadku momentu", &m_config.driveTaperStart, 0.2f, 0.95f, "%.2f x obr." );
		HelpMarker( "Od jakiej części limitu obrotów moment zaczyna maleć w stronę zera - symuluje silnik "
					"dochodzący do czerwonego pola." );
		ImGui::SliderFloat( "Moment hamowania", &m_config.brakeTorque, 0.0f, 2500.0f, "%.0f N*m" );
		HelpMarker( "Moment hamulców na koło przy trzymaniu spacji. Więcej = krótsza droga hamowania, ale łatwiej "
					"zablokować koła (utrata sterowności)." );
		ImGui::SliderFloat( "Moment na biegu jałowym", &m_config.coastTorque, 0.0f, 40.0f, "%.0f N*m" );
		HelpMarker( "Lekki opór silnika, gdy nie dotykasz gazu ani hamulca - jak puszczenie sprzęgła bez gazu." );
		ImGui::Checkbox( "Napęd na wszystkie koła", &m_config.allWheelDrive );
		HelpMarker( "Wyłączone = napęd tylko na tylną oś. Włączone = moment idzie na wszystkie 4 koła - więcej "
					"przyczepności przy starcie, mniej driftu na gazie." );
		ImGui::Separator();
		ImGui::SliderFloat( "Opór aerodynamiczny", &m_config.aeroDragArea, 0.2f, 2.0f, "%.2f m^2" );
		HelpMarker( "Opór powietrza rosnący z kwadratem prędkości. To ON ogranicza prędkość maksymalną, nie sztywny "
					"limit - większa wartość = niższy V-max." );
	}

void JozzVehicleM6RigLab::DrawSteeringTab()
	{
		ImGui::TextWrapped( "W ruchu puszczona kierownica sama wraca do środka dzięki fizyce (caster), nie skryptowi. "
							"Na postoju koła zostają skręcone - jak w prawdziwym aucie." );
		HelpMarker( "Trzymanie A/D włącza sprężynę zębatki + serwo (wspomaganie). Puszczenie zostawia tylko tarcie "
					"zębatki, więc geometria zwrotnicy i siły z kontaktu z podłożem same kierują kołami - kontra w "
					"poślizgu i prostowanie na wyjściu z zakrętu wynikają z sił, nie ze skryptu." );
		ImGui::Checkbox( "Odwróć kierowanie (preferencja)", &m_invertSteering );
		ImGui::Separator();
		if ( ImGui::SliderFloat( "Maksymalny skręt kół (wymaga Zastosuj)", &m_editMaxSteeringAngleDegrees, 20.0f, 45.0f,
								  "%.0f st." ) )
		{
			m_structuralSetupDirty = true;
		}
		HelpMarker( "Kąt skrętu koła przy pełnym locku kierownicy. Strukturalne - przebudowuje zębatkę i płot "
					"bezpieczeństwa (P1), stąd wymaga Zastosuj. UWAGA: zbyt wysoka wartość razem z wysokim 'Udział "
					"Ackermanna' (zakładka Zawieszenie, zaawansowane) może wepchnąć drążek w jego martwy punkt - w "
					"takim wypadku Zastosuj SAMO zaciśnie tę wartość i pokaże komunikat, nie trzeba zgadywać "
					"bezpiecznej liczby ręcznie." );
		if ( m_steeringClampStatus.empty() == false )
		{
			ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ), "%s", m_steeringClampStatus.c_str() );
		}
		ImGui::Separator();
		if ( ImGui::SliderFloat( "Sztywność kierownicy", &m_config.steeringHertz, 2.0f, 25.0f, "%.1f Hz" ) )
		{
			ApplySteeringTuning();
		}
		HelpMarker( "Jak szybko zębatka goni zadany kąt skrętu, gdy trzymasz A/D. Wyżej = ostrzejsza, bardziej "
					"'gokartowa' reakcja." );
		if ( ImGui::SliderFloat( "Tłumienie kierownicy", &m_config.steeringDampingRatio, 0.2f, 3.0f, "%.2f" ) )
		{
			ApplySteeringTuning();
		}
		// Live: the drive update pushes these to the joints every step.
		ImGui::SliderFloat( "Siła wspomagania", &m_config.rackServoForce, 0.0f, 20000.0f, "%.0f N" );
		HelpMarker( "Ile siły ma wspomaganie, gdy trzymasz kierownicę - musi pokonać moment parkingowy obciążonej "
					"opony (~700 N*m na koło), inaczej auto 'nie posłucha' przy postoju." );
		ImGui::SliderFloat( "Tarcie zębatki - bazowe", &m_config.rackFrictionBase, 0.0f, 200.0f, "%.0f N" );
		HelpMarker( "[FIZYCZNY] Stały opór uszczelek i łożysk kolumny - działa zawsze, niezależnie od obciążenia. "
					"Decyduje, jak łatwo MAŁE siły (ślad casteru przy jeździe na wprost) poruszają kierownicą. "
					"Mniej = kierownica żywsza i sama się prostuje po drobnych szarpnięciach; więcej = spokojniejsza, "
					"ale może zostawać lekko skręcona po wybojach." );
		ImGui::SliderFloat( "Tarcie zębatki - od obciążenia", &m_config.rackFrictionLoadCoeff, 0.0f, 0.40f, "%.2f" );
		HelpMarker( "[FIZYCZNY] Ile tarcia dokłada każdy niuton bocznego obciążenia drążków (siły dociskające "
					"zębatkę do prowadnic - tak powstaje tarcie w prawdziwej przekładni). Rośnie SAMO przy twardych "
					"lądowaniach i mocnym skręcie - trzyma wtedy stabilność układu - a przy spokojnej jeździe na "
					"wprost prawie znika, więc nie usztywnia powrotu kierownicy. Mniej = luźniejszy układ pod "
					"obciążeniem (ryzyko szarpnięć przy lądowaniu), więcej = stabilniej, ale powrót po mocnym "
					"skręcie wolniejszy." );
		ImGui::SliderFloat( "Tarcie skrętu kolumny", &m_config.steeringFrictionTorque, 0.0f, 200.0f, "%.0f N*m" );
		HelpMarker( "To samo co tarcie zębatki, ale dla osi na kolumnie McPhersona zamiast wahaczy." );
		ImGui::SliderFloat( "[ARCADE] Wspomaganie powrotu", &m_config.rackCenteringHertz, 0.0f, 30.0f, "%.0f Hz" );
		HelpMarker( "MECHANIKA POZA MODELEM FIZYCZNYM (ADR-0006). Domyślnie 0 = WYŁĄCZONE (realistycznie). "
					"Prawdziwe auto NIE centruje kół na postoju - koła wracają do środka dopiero w ruchu, dzięki "
					"wleczeniu casterem (to domyślne, uczciwe zachowanie). Podniesienie dodaje sztuczną sprężynę "
					"ciągnącą kierownicę do środka nawet na postoju. Zmierzone: zauważalne dopiero od ok. 10 Hz "
					"(niżej opona za mocno trzyma o ziemię), od 10-15 Hz koła wracają do środka same. Im wyżej, tym "
					"mocniej - ale tym bardziej walczy z naturalnym kontra-skrętem w poślizgu." );
	}

	// The single "Zawieszenie" tab, front to back: what kind of suspension ->
	// where it sits and how far it moves (the thing people actually came here
	// for) -> spring feel -> anti-roll -> advanced hardpoint geometry, folded
	// away by default because most users never need it.
void JozzVehicleM6RigLab::DrawSuspensionTab()
	{
		bool edited = false; // any control here that needs "Apply rig rebuild"

		SectionHeader( "Typ zawieszenia" );
		const char* rigTypes[] = { "Kolumna (prosta, tania - McPherson)", "Podwójny wahacz (widoczne ramiona)",
								   "Wahacz wleczony (model Jozza)" };
		edited |= ImGui::Combo( "Przednia oś", &m_editFrontRigType, rigTypes, 3 );
		edited |= ImGui::Combo( "Tylna oś", &m_editRearRigType, rigTypes, 3 );
		if ( m_editFrontRigType == JOZZ_M6_RIG_TRAILING_ARM )
		{
			ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ),
								 "Wahacz wleczony nie skręca - z przodu auto pojedzie tylko na wprost." );
		}

		SectionHeader( "Postawa - jak stoi auto (najważniejsze ustawienia)" );
		edited |= ImGui::SliderFloat( "Opadanie wahacza", &m_editWishbone.restArmDroopDeg, 0.0f, 16.0f, "%.1f st." );
		HelpMarker( "Wahacze zwisają W DÓŁ do koła w spoczynku (jak w BeamNG) zamiast wyginać się do góry. 16 st. to "
					"zbadany bezpieczny sufit - wyżej kierownica traci geometrię (jedno koło blokuje się do oporu). "
					"Działa tylko na osiach z podwójnym wahaczem. Wymaga Zastosuj." );
		if ( ImGui::SliderFloat( "Prześwit przód", &m_config.suspensionPreloadFront, -0.08f, 0.20f, "%.3f m" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Prześwit tył", &m_config.suspensionPreloadRear, -0.08f, 0.20f, "%.3f m" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Docisk wstępny sprężyny: podnosi lub obniża daną oś, na żywo, bez przebudowy, NIEZALEŻNIE od "
					"twardości sprężyny (suwaki 'Mnożnik twardości' poniżej). Osobno przód/tył - podnieś tył pod "
					"ciężki bagażnik albo przód pod docisk, bez zmiany sztywności." );
		if ( ImGui::SliderFloat( "Skok ściskania", &m_config.compressionTravel, 0.10f, 0.70f, "%.2f m" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Skok odbicia", &m_config.reboundTravel, 0.10f, 0.60f, "%.2f m" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Jak daleko koło może się ruszyć w górę (ściskanie) i w dół (odbicie) od pozycji spoczynkowej, "
					"zanim amortyzator dojdzie do ogranicznika. Offroad chce obu dużo, drift/tor chce ciasno. Na "
					"żywo, bez przebudowy." );
		// P6 (audit S2): the hinge anti-fold guard computes its angle from
		// travel/armLength and clamps at asin(0.95); past that the requested
		// travel physically exceeds what the arm arc can deliver and the
		// "25% margin" in the formula is fiction. Warn instead of silently
		// saturating - the default config itself trips this (a known, accepted
		// state; changing the default travel is Jozz's call, not a hotfix).
		{
			float travel = b3MaxFloat( m_config.compressionTravel, m_config.reboundTravel );
			float saturation = 1.25f * travel / b3MaxFloat( m_editWishbone.lowerArmLength, 0.05f );
			if ( saturation >= 0.95f )
			{
				ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ) );
				ImGui::TextWrapped( "Skok większy niż zasięg wahacza (%.0f%% nasycenia) - realny skok ogranicza "
									 "długość ramienia, nie suwak.",
									 (double)( 100.0f * saturation / 0.95f ) );
				ImGui::PopStyleColor();
				HelpMarker( "Ogranicznik kąta wahacza siedzi na swoim suficie (55 st.) - powyżej tej granicy "
							"zwiększanie skoku suwakiem NIE wydłuża realnego ruchu koła, bo geometria ramienia "
							"go nie pokryje. Dłuższy dolny wahacz (sekcja Zaawansowane) podnosi granicę." );
			}
		}

		SectionHeader( "Sprężyny i tłumienie (na żywo)" );
		if ( ImGui::SliderFloat( "Twardość sprężyny", &m_config.suspensionHertz, 1.0f, 12.0f, "%.1f Hz" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Miękko = więcej komfortu i przyczepności w terenie, ale więcej przechyłu. Twardo = szybsza, "
					"bardziej torowa reakcja." );
		if ( ImGui::SliderFloat( "Tłumienie", &m_config.suspensionDampingRatio, 0.2f, 2.0f, "%.2f" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Jak szybko gasną drgania po odbiciu. Za mało = auto 'skacze' po nierównościach; za dużo = "
					"zawieszenie sztywnieje na nierównym terenie." );
		if ( ImGui::SliderFloat( "Mnożnik twardości - przód", &m_config.frontSuspensionScale, 0.5f, 2.0f, "%.2f x" ) )
		{
			ApplySuspensionTuning();
		}
		if ( ImGui::SliderFloat( "Mnożnik twardości - tył", &m_config.rearSuspensionScale, 0.5f, 2.0f, "%.2f x" ) )
		{
			ApplySuspensionTuning();
		}
		HelpMarker( "Mnożnik twardości i tłumienia osobno dla przodu i tyłu - podbij tył dla auta z ciężkim "
					"bagażnikiem albo przód dla nosowego silnika." );

		SectionHeader( "Stabilizatory przechyłu (na żywo)" );
		ImGui::SliderFloat( "Stabilizator przód", &m_config.arbFrontStiffness, 0.0f, 40000.0f, "%.0f N/m" );
		ImGui::SliderFloat( "Stabilizator tył", &m_config.arbRearStiffness, 0.0f, 40000.0f, "%.0f N/m" );
		HelpMarker( "Ogranicza przechył nadwozia w zakręcie, przenosząc obciążenie między lewym a prawym kołem tej "
					"samej osi. Mocniejszy przedni = więcej podsterowności; mocniejszy tylny = auto chętniej "
					"'wchodzi w tył' (żywsza rotacja)." );
		if ( ImGui::Checkbox( "[ARCADE] Wspomaganie pionowania", &m_config.uprightAssist ) )
		{
			CreateVehicle();
		}
		HelpMarker( "MECHANIKA POZA MODELEM FIZYCZNYM (ADR-0006). Sztuczna siła trzymająca nadwozie poziomo - "
					"włącz tylko gdy auto się przewraca mimo dobrze ustawionych stabilizatorów. Domyślnie "
					"wyłączone: przechył kontrolują stabilizatory powyżej, uczciwie." );

		if ( ImGui::CollapsingHeader( "Zaawansowane: geometria wahaczy" ) )
		{
			ImGui::Indent();
			ImGui::TextWrapped( "Punkty mocowania zawieszenia. Zmieniają charakter jazdy w subtelny sposób - "
								 "większość osób nigdy nie musi tu wchodzić." );
			ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ),
								 "Zmiany działają na fizykę i linie debug - model 3D auta NIE przeskalowuje się "
								 "(rysowany z socketów oryginalnego modelu)." );
			edited |= ImGui::SliderFloat( "Caster (wyprzedzenie)", &m_editWishbone.casterDeg, -2.0f, 12.0f, "%.1f st." );
			HelpMarker( "Większy caster = silniejsze samo-centrowanie kierownicy i mocniejsza kontra w poślizgu. "
						"Ustawienia driftowe: 7-10 st." );
			edited |= ImGui::SliderFloat( "Pochylenie sworznia", &m_editWishbone.kingpinInclinationDeg, 0.0f, 15.0f,
										   "%.1f st." );
			HelpMarker( "Przechyla oś sworznia (kingpin) do środka auta u góry. Wyżej = mniejszy scrub radius (mniej "
						"'szarpania' kierownicą przy hamowaniu asymetrycznym), ale też mniej mechanicznego "
						"samo-centrowania. Subtelny efekt - większość aut ma 6-10 st." );
			edited |= ImGui::SliderFloat( "Offset sworznia", &m_editWishbone.kingpinOffset, 0.05f, 0.25f, "%.2f m" );
			HelpMarker( "Jak daleko do środka auta leżą przeguby kulowe względem środka koła. Wpływa na scrub radius "
						"i na to, jak mocno hamowanie/napęd 'ciągnie' za kierownicę. Subtelne - dotykaj po innych "
						"suwakach." );
			edited |= ImGui::SliderFloat( "Wysokość zwrotnicy", &m_editWishbone.uprightHalfHeight, 0.10f, 0.30f, "%.2f m" );
			HelpMarker( "Rozstaw górnego/dolnego przegubu kulowego od środka koła - de facto 'wysokość' zwrotnicy. "
						"Wyższa = sztywniejsza geometria kątowa, subtelnie mniejszy przyrost campera przy skoku." );
			edited |= ImGui::SliderFloat( "Długość górnego wahacza", &m_editWishbone.upperArmLength, 0.20f, 0.55f, "%.2f m" );
			HelpMarker( "Krótszy górny wahacz względem dolnego = szybszy przyrost camberu przy skoku (typowe dla "
						"aut torowych/driftowych); zbliżone długości = camber prawie się nie zmienia (offroad, "
						"komfort)." );
			edited |= ImGui::SliderFloat( "Długość dolnego wahacza", &m_editWishbone.lowerArmLength, 0.25f, 0.70f, "%.2f m" );
			HelpMarker( "Dłuższe dolne wahacze = łagodniejszy przyrost kąta pochylenia koła (camber) przy skoku." );
			edited |= ImGui::SliderFloat( "Rozstaw mocowań wahacza", &m_editWishbone.armHalfSpread, 0.12f, 0.40f, "%.2f m" );
			HelpMarker( "Jak szeroko (wzdłuż auta) rozstawione są dwa punkty mocowania każdego wahacza na nadwoziu - "
						"decyduje o sztywności skrętnej wahacza wobec sił wzdłużnych (hamowanie/napęd). Kosmetyczny "
						"dla samej jazdy, ważny dla wyglądu linii debug." );
			edited |= ImGui::SliderFloat( "Cofnięcie ramienia kierown.", &m_editWishbone.steeringArmBack, 0.10f, 0.25f, "%.2f m" );
			HelpMarker( "Jak daleko za środkiem koła siedzi ramię, do którego mocuje się drążek kierowniczy. Dłuższe "
						"ramię = mniejsza siła w drążku na ten sam moment na kole (lżejsza kierownica), ale też "
						"mniejszy zakres skrętu przy tej samej długości maglownicy. Zmienia też, gdzie leży martwy "
						"punkt drążka (zakładka Kierownica, suwak maks. skrętu się do tego dostosuje)." );
			edited |= ImGui::Checkbox( "Trapez Ackermanna (mechaniczny)", &m_editWishbone.ackermannTrapezoid );
			HelpMarker( "Kątuje ramiona kierownicze do środka, żeby koło wewnętrzne w zakręcie skręcało mocniej niż "
						"zewnętrzne - czysto geometrycznie, bez elektroniki. Wyłączone = oba koła skręcają identycznie "
						"(prościej, ale mniej naturalnie w ostrych zakrętach)." );
			if ( m_editWishbone.ackermannTrapezoid )
			{
				edited |= ImGui::SliderFloat( "Udział Ackermanna", &m_editWishbone.ackermannFraction, 0.0f, 1.0f, "%.2f" );
				HelpMarker( "0 = brak Ackermanna (jak wyłączony trapez), 1 = pełna geometria. WAŻNE: wyższa wartość "
							"przybliża drążek do jego martwego punktu (przy 1.0 to ok. 50 st., przy domyślnym 0.6 to "
							"ok. 60 st.) - suwak 'Maksymalny skręt kół' (zakładka Kierownica) SAM się zacieśni po "
							"Zastosuj, jeśli ta kombinacja stałaby się niebezpieczna, więc nie trzeba tego liczyć "
							"ręcznie. Domyślne 0.6 to kompromis producentów - pełne 1.0 bywa zbyt agresywne." );
			}
			edited |= ImGui::SliderFloat( "Wysokość mocowania amortyzatora", &m_editWishbone.coiloverTopHeight, 0.25f,
										   0.60f, "%.2f m" );
			HelpMarker( "Jak wysoko nad środkiem koła amortyzator mocuje się do nadwozia. Wyżej = amortyzator bardziej "
						"pionowy = mniejsze przełożenie ruchu koła na ruch sprężyny (motion ratio bliżej 1)." );
			edited |= ImGui::SliderFloat( "Masa zwrotnicy", &m_editKnuckleMass, 10.0f, 50.0f, "%.0f kg" );
			HelpMarker( "Masa nieresorowana zwrotnicy/piasty. Więcej = zawieszenie wolniej reaguje na nierówności, "
						"ale i mniej 'nerwowe' przy uderzeniach." );
			edited |= ImGui::SliderFloat( "Masa wahacza", &m_editArmMass, 2.0f, 15.0f, "%.1f kg" );
			HelpMarker( "Masa samego ramienia wahacza. Głównie wpływa na to, jak łatwo solver fizyki radzi sobie z "
						"tym ciałem - rzadko trzeba ruszać." );
			edited |= ImGui::SliderFloat( "Caster kolumny (osie kolumnowe)", &m_editStrutCasterDeg, -2.0f, 12.0f, "%.1f st." );
			HelpMarker( "To samo co Caster wyżej, ale dla osi ustawionej na Kolumnę (McPherson) zamiast Podwójny "
						"wahacz - osobne pole, bo to inny typ zawieszenia z inną geometrią." );
			edited |= ImGui::SliderFloat( "Zbieżność (toe) przód", &m_editFrontToeDeg, -3.0f, 3.0f, "%.1f st." );
			HelpMarker( "Statyczny kąt kół w spoczynku, przód. Dodatni = zbieżność (toe-in, przody kół do środka) - "
						"stabilniej na wprost, kosztem odrobiny zwrotności. Ujemny = rozbieżność (toe-out) - żywszy "
						"skręt, mniej stabilnie. Działa tylko na osiach z podwójnym wahaczem." );
			edited |= ImGui::SliderFloat( "Zbieżność (toe) tył", &m_editRearToeDeg, -3.0f, 3.0f, "%.1f st." );
			HelpMarker( "To samo, tył. Zbieżność z tyłu = więcej stabilności/podsterowności w zakręcie; rozbieżność "
						"z tyłu = auto chętniej rotuje (bliżej nadsterowności), ale bywa nerwowe na wprost." );
			ImGui::Unindent();
		}

		if ( ImGui::CollapsingHeader( "Zaawansowane: wahacz wleczony (model Jozza)" ) )
		{
			ImGui::Indent();
			ImGui::TextWrapped( "%s", m_trailingImport.status.c_str() );
			if ( ImGui::Button( "Wczytaj ponownie z kontraktu" ) )
			{
				m_trailingImport = LoadJozzVehicleM7TrailingArmGeometry( "one_sided_wheel_mount.asset.json" );
				m_editTrailingArm = m_trailingImport.geometry;
				LoadMountVisual();
				m_structuralSetupDirty = true;
			}
			edited |= ImGui::SliderFloat( "Oś obrotu przed kołem", &m_editTrailingArm.pivotOffset.x, 0.30f, 0.90f, "%.2f m" );
			HelpMarker( "Jak daleko PRZED środkiem koła leży oś, wokół której obraca się cały wahacz wleczony. "
						"Dłuższe ramię = łagodniejszy łuk ruchu koła przy skoku (mniejsza zmiana kąta na metr skoku)." );
			edited |= ImGui::SliderFloat( "Oś obrotu nad kołem", &m_editTrailingArm.pivotOffset.y, -0.05f, 0.35f, "%.2f m" );
			HelpMarker( "Jak wysoko nad środkiem koła leży ta sama oś obrotu. Wpływa na to, jak koło porusza się w "
						"bok (nie tylko w górę/dół) podczas skoku." );
			edited |= ImGui::SliderFloat( "Masa wahacza wleczonego", &m_editTrailingArm.armMass, 6.0f, 25.0f, "%.0f kg" );
			HelpMarker( "Masa samego ramienia (bez koła). Wpływa na kompensację efektywnej masy amortyzatora "
						"(patrz TECH_DEBT/komentarz w kodzie) - zmiana tutaj automatycznie przelicza sztywność "
						"tak, żeby twardość 'na kole' została ta sama." );
			ImGui::Unindent();
		}

		if ( ImGui::CollapsingHeader( "Zaawansowane: kształt kolizji koła" ) )
		{
			ImGui::Indent();
			const char* envelopes[] = { "Sfera (gładka, wybrzusza się na boki)",
										"Walec (prawdziwa szerokość, graniasty)", "Suma fazowa (eksperymentalne)",
										"Mieszana: sfera + prawdziwa szerokość (domyślne)",
										"Opona: pierścień kapsuł (najlepsze toczenie, shimmy kierownicy)",
										"KOŁO: prawdziwy kolider koła w silniku (NOWE, najgładsze)" };
			edited |= ImGui::Combo( "Kształt", &m_editEnvelopeMode, envelopes, 6 );
			HelpMarker( "Kształt fizycznej bryły koła (nie wizualny model 3D - ten rysuje się zawsze tak samo).\n\n"
						"OPONA: pierścień kapsuł o płaskiej bieżni i zaokrąglonych barkach. Prawdziwa szerokość we "
						"WSZYSTKICH kierunkach naraz, żadnych masek kolizji i ani jednej ostrej krawędzi - a to one, "
						"nie liczba ścianek, psują toczenie. Na stanowisku bije dzisiejsze koło o 33% straty i 83% "
						"twardości.\n\n"
						"MIESZANA (domyślna): gładka sfera do terenu plus walec prawdziwej szerokości do przeszkód, "
						"sklejone maskami kolizji. Sfera dotyka ziemi w JEDNYM punkcie na osi symetrii koła.\n\n"
						"Masa i bezwładność koła są TE SAME we wszystkich trybach - przełączasz wyłącznie kształt." );
			if ( m_editEnvelopeMode == JOZZ_M6_ENVELOPE_TORUS )
			{
				ImGui::TextColored( ImVec4( 0.95f, 0.75f, 0.35f, 1.0f ), "Uwaga: przy tym kształcie przód SZARPIE." );
				HelpMarker( "Zmierzone, nie przypuszczenie: walidator produktowy spada z 18/18 na 15/18, a trzy "
							"czerwone to sondy kierownicy - po puszczeniu koło oscyluje 4,7 stopnia i zatrzymuje "
							"się 4 stopnie od prosto.\n\n"
							"To NIE jest wina pierścienia kapsuł. Kontrola: sam WALEC - jeden kształt - jest gorszy "
							"(8 czerwonych, 6,7 stopnia), a szarpanie rośnie razem z szerokością płaskiej bieżni. "
							"Wyzwalaczem jest styk z ziemią na PRAWDZIWEJ SZEROKOŚCI, którego mieszana obwiednia "
							"nigdy nie miała.\n\n"
							"Czyli: geometria kierownicy była strojona pod styk punktowy i nigdy nie spotkała opony. "
							"Naprawa siedzi w kierownicy (wyprzedzenie, promień zataczania, tłumienie, tarcie "
							"przekładni), nie w kole - i zmienia to, jak auto się prowadzi, więc jest decyzją "
							"właściciela.\n\n"
							"Jeździć tym MOŻNA: zawieszenie i wyboje czuje się w pełni, tor jazdy błądzi." );
			}
			if ( m_editEnvelopeMode == JOZZ_M6_ENVELOPE_WHEEL )
			{
				ImGui::TextColored( ImVec4( 0.45f, 0.85f, 0.50f, 1.0f ), "Nowy kolider: gladszy OD SFERY, pelna szerokosc." );
				HelpMarker( "To NIE jest koło składane z klocków - to nowy typ bryły dodany do silnika fizyki "
							"(src/wheel_shape.c). Walec o zaokrąglonych barkach, obrotowo symetryczny względem osi "
							"koła, a styk liczony analitycznie Z OSI zamiast z wierzchołków.\n\n"
							"Dlatego działa: na płaszczyźnie daje dokładnie DWA punkty styku, na obu końcach "
							"szerokości bieżni, i są to TE SAME dwa punkty po obrocie koła. Każde koło składane "
							"z kształtów gubi tę własność i przez to podskakuje.\n\n"
							"Zmierzone w TYM aucie przy 58 km/h (trzęsienie nadwozia, m/s2):\n"
							"  sfera 0,061  |  opona z 64 kapsuł 2,244  |  TO KOŁO 0,022\n"
							"Na gruncie z trójkątów (mapa): sfera 0,060  |  opona 1,945  |  TO KOŁO 0,023\n"
							"Punktów styku 2,00 w każdym kroku wobec 1,00 sfery, prędkość maksymalna 17,8 wobec "
							"17,3 m/s - nie traci energii na młotkowanie.\n\n"
							"Promień barku ustawia profil: 0 = slick o ostrej krawędzi, połowa szerokości = "
							"balonówka o okrągłym przekroju.\n\n"
							"NIESPRAWDZONE: zachowanie na wybojach i na krawędziach skrzyń. Kierownica może "
							"szarpać jak przy oponie, bo styk ma prawdziwą szerokość - tego jeszcze nie mierzyłem." );
				float halfWidth = 0.5f * m_config.wheelEnvelope.width;
				edited |= ImGui::SliderFloat( "Promień barku", &m_editEnvelopeCrownRadius, 0.0f, halfWidth, "%.3f m" );

				float maxDrop = 0.25f * m_config.wheelEnvelope.radius;
				edited |= ImGui::SliderFloat( "Wysklepienie bieżni", &m_editWheelCrownDrop, 0.0f, maxDrop, "%.3f m" );
				HelpMarker( "O ile środek bieżni jest wyżej niż jej brzegi.\n\n"
							"0 = bieżnia płaska: koło stoi na ziemi obydwoma brzegami, dwa punkty styku, najszersza "
							"stopa. Tak jest DOMYŚLNIE i tak zmierzone są liczby wyżej.\n\n"
							"Więcej = opona wypukła jak w motocyklu albo w terenówce: na płaskim dotyka tylko "
							"środkiem, więc stopa jest węższa, ale przy przechyle koło przetacza się na bok bieżni "
							"zamiast wchodzić na ostry brzeg.\n\n"
							"Im mocniej koło jest dociśnięte do ziemi, tym więcej punktów bieżni dotyka - stopa "
							"rośnie z obciążeniem, jak w prawdziwej oponie.\n\n"
							"Zmierzone w TYM aucie (trzęsienie nadwozia m/s2 | punktów styku na koło):\n"
							"  prosto 58 km/h:  0mm 0,023|2,00   3mm 0,024|2,94   10mm 0,034|2,16   30mm 0,022|1,00\n"
							"  ZAKRĘT 43 km/h:  0mm 0,603|2,00   3mm 0,447|2,43   10mm 0,459|1,53   30mm 0,468|1,00\n"
							"Czyli: na prostej wysklepienie nic nie daje, ale w zakręcie 3 mm zbija szarpanie o 26%, "
							"bo koło dostaje WIĘCEJ punktów styku, nie mniej. Przy 30 mm zostaje jeden punkt - to "
							"znowu kulka.\n\n"
							"Domyślnie 0. Jak ma jeździć auto, to Twoja decyzja, nie moja." );

				edited |= ImGui::SliderInt( "Punkty przekroju", &m_editWheelProfilePoints, 2, B3_MAX_WHEEL_PROFILE_POINTS );
				HelpMarker( "Iloma punktami narysowany jest łuk bieżni. Więcej = gładszy przekrój, NIE gładsza "
							"jazda. Przy zerowym wysklepieniu ta liczba nic nie zmienia: punkty leżą w jednej "
							"linii i silnik wyrzuca zbędne.\n\n"
							"Solver i tak bierze najwyżej cztery punkty styku na koło - reszta przekroju kształtuje "
							"bryłę, nie liczbę kontaktów." );
			}
			if ( m_editEnvelopeMode == JOZZ_M6_ENVELOPE_PHASED_UNION )
			{
				edited |= ImGui::SliderInt( "Warstwy sumy", &m_editEnvelopeLayers, 2, 4 );
			}
			if ( m_editEnvelopeMode == JOZZ_M6_ENVELOPE_TORUS )
			{
				// Zakresy liczone z BIEŻĄCEJ opony, nie stałe: przy innym kole
				// stała granica w metrach albo odcinałaby sensowny zakres, albo
				// pozwalała zbudować oponę bez bieżni.
				float halfWidth = 0.5f * m_config.wheelEnvelope.width;
				edited |= ImGui::SliderFloat( "Promień barku", &m_editEnvelopeCrownRadius, 0.02f, 0.49f * 2.0f * halfWidth,
											  "%.3f m" );
				HelpMarker( "Jak mocno zaokrąglone są barki opony. Większy promień = gładsze toczenie I TAŃSZE "
							"(grubszy bark uszczelnia pierścień mniejszą liczbą kapsuł). Cena jest w szerokości "
							"płaskiej bieżni, która o tyle się zwęża." );

				JozzVehicleM6WheelEnvelopeDesc probe = m_config.wheelEnvelope;
				probe.mode = JOZZ_M6_ENVELOPE_TORUS;
				probe.torusCrownRadius = m_editEnvelopeCrownRadius;
				int minSegments = JozzVehicleM6MinTorusSegments( &probe );
				edited |= ImGui::SliderInt( "Kapsuł w obwodzie", &m_editEnvelopeTorusSegments, minSegments,
											JOZZ_M6_MAX_WHEEL_SHAPES );
				HelpMarker( "Ile kapsuł tworzy pierścień. Poniżej minimum pierścień ma DZIURY - kontakt wpadałby w nie "
							"raz na kapsułę - więc suwak się tam nie cofnie. Powyżej minimum kupujesz tylko mniejsze "
							"tętnienie promienia, a płacisz procesorem: to są kształty razy CZTERY KOŁA.\n\n"
							"ZMIERZONE w tym aucie, na tej mapie, w jeździe (Release, mediana z 3 przebiegów, "
							"ms na krok fizyki):\n"
							"  sama sfera        0,099\n"
							"  mieszana          0,105   <- dzisiejsze koło\n"
							"  opona 16 kapsuł   0,143\n"
							"  opona 32 kapsuły  0,197\n"
							"  opona 64 kapsuły  0,403\n"
							"Budżet klatki to 16,7 ms, więc nawet 64 kapsuły biorą z niej 2,4%." );
				ImGui::TextDisabled( "minimum dla szczelności: %d   |   bieżnia płaska: %.0f mm   |   %d kształtów na auto",
									 minSegments, 1000.0f * ( 2.0f * halfWidth - 2.0f * m_editEnvelopeCrownRadius ),
									 4 * m_editEnvelopeTorusSegments );
			}
			ImGui::Unindent();
		}

		if ( edited )
		{
			m_structuralSetupDirty = true;
		}

		ImGui::Spacing();
		// Confirmed, not instant: this wipes every dial across all tabs, and
		// since restarting/reopening the app now RESTORES last session (the R
		// fix above), an accidental click here would otherwise get silently
		// baked in as the new "last session" the moment the app closes -
		// exactly the kind of quiet data loss this whole pass was hunting for.
		if ( ImGui::Button( "Przywróć wszystkie ustawienia domyślne" ) )
		{
			ImGui::OpenPopup( "Potwierdz reset##ConfirmDefaults" );
		}
		HelpMarker( "Resetuje CAŁY pojazd do domyślnych ustawień fabrycznych - łącznie z nadwoziem (zakładka "
					"Nadwozie)." );
		if ( ImGui::BeginPopupModal( "Potwierdz reset##ConfirmDefaults", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
		{
			ImGui::TextWrapped( "To wyzeruje WSZYSTKIE ustawienia (Zawieszenie, Nadwozie, Napęd, Kierownica) do "
								 "wartości fabrycznych. Jeśli chcesz zachować obecne strojenie, zamknij to okno i "
								 "najpierw zapisz je jako preset (pole 'nazwa nowego presetu' powyżej)." );
			if ( ImGui::Button( "Tak, resetuj" ) )
			{
				// Same single source of truth as preset loads: the factory
				// baseline stashed by the constructor (this used to rebuild its
				// own copy and could drift from what presets overlay onto).
				m_config = m_factoryConfig;
				RecomputeRackTravel();
				CreateVehicle();
				ApplyBodyVisualFromConfig(); // R4: config-replacing paths must re-sync the body mesh too
				SyncEditFromConfig();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Anuluj" ) )
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	// Body dimensions and axle layout - "what car am I building", separate from
	// "how does each corner's suspension move" (Zawieszenie tab). Everything
	// here rebuilds the vehicle.
void JozzVehicleM6RigLab::DrawChassisTab()
	{
		bool edited = false;

		// Body skin choice + position offset: LIVE, no rebuild needed, so this
		// stays OUTSIDE the edited|=/m_structuralSetupDirty flow below on purpose -
		// otherwise picking a skin would falsely flag "Nadwozie *" and demand an
		// Apply it doesn't need (and would retrigger the tab-identity bug the
		// "###TabChassis" suffix guards against - see DrawControls below).
		SectionHeader( "Model nadwozia (wygląd)" );
		{
			int count = 0;
			const JozzVehicleBodyModelDef* models = GetJozzVehicleBodyModels( &count );
			int current = 0; // fallback to "brak" when the key is unknown
			for ( int i = 0; i < count; ++i )
			{
				if ( std::strcmp( models[i].key, m_config.bodyVisualModel ) == 0 )
				{
					current = i;
				}
			}
			ImGui::SetNextItemWidth( 14.0f * ImGui::GetFontSize() );
			if ( ImGui::BeginCombo( "##BodyModelSelect", models[current].label ) )
			{
				for ( int i = 0; i < count; ++i )
				{
					if ( ImGui::Selectable( models[i].label, i == current ) && i != current )
					{
						std::snprintf( m_config.bodyVisualModel, sizeof( m_config.bodyVisualModel ), "%s", models[i].key );
						ApplyBodyVisualFromConfig();
					}
				}
				ImGui::EndCombo();
			}
			HelpMarker( "Wygląd nadwozia - czysto wizualna skóra na bryle fizycznej. Nie zmienia fizyki: "
						"bryła kolizyjna i jej wymiary (sekcje niżej) działają jak dotąd (bryła chowa się "
						"pod nadwoziem - podgląd: Debug > 'Pokaż bryłę kolizyjną nadwozia'). "
						"Wybór wchodzi do presetów i przeżywa R." );

			ImGui::SliderFloat( "Przesunięcie przód/tył", &m_config.bodyVisualOffset.x, -0.50f, 0.50f, "%.2f m" );
			ImGui::SliderFloat( "Przesunięcie góra/dół", &m_config.bodyVisualOffset.y, -0.50f, 0.50f, "%.2f m" );
			ImGui::SliderFloat( "Przesunięcie lewo/prawo", &m_config.bodyVisualOffset.z, -0.50f, 0.50f, "%.2f m" );
			HelpMarker( "Dostrojenie pozycji modelu względem bryły fizycznej, w osiach nadwozia "
						"(X przód, Y góra, Z lewo). Baza per model siedzi w rejestrze - to jest korekta." );
			if ( ImGui::Button( "Wyzeruj przesunięcie" ) )
			{
				m_config.bodyVisualOffset = { 0.0f, 0.0f, 0.0f };
			}
		}
		ImGui::Separator();

		SectionHeader( "Wymiary nadwozia" );
		edited |= ImGui::SliderFloat( "Połowa długości", &m_editChassisHalfExtents.x, 0.8f, 2.5f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Połowa wysokości", &m_editChassisHalfExtents.y, 0.15f, 0.70f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Połowa szerokości", &m_editChassisHalfExtents.z, 0.35f, 1.00f, "%.2f m" );
		HelpMarker( "Rozmiar skrzyni nadwozia (pudełka). 'Połowa' bo liczone od środka - realna długość/szerokość to "
					"dwa razy tyle." );
		edited |= ImGui::SliderFloat( "Gęstość (masa)", &m_editChassisDensity, 50.0f, 600.0f, "%.0f kg/m^3" );
		HelpMarker( "Razem z wymiarami wyżej decyduje o masie nadwozia. Lekki drifter: nisko; ciężarówka: wysoko." );
		edited |= ImGui::SliderFloat( "Obniżenie środka ciężkości", &m_editCgVerticalOffset, -0.10f, 0.40f, "%.2f m" );
		HelpMarker( "Jak nisko pod geometrycznym środkiem nadwozia leży faktyczny środek ciężkości (silnik, "
					"pasażerowie, ładunek). Niżej = stabilniej w zakrętach." );

		SectionHeader( "Rozstaw osi i kół" );
		edited |= ImGui::SliderFloat( "Połowa rozstawu osi", &m_editAxleHalfSpacing, 0.6f, 2.5f, "%.2f m" );
		HelpMarker( "Odległość przedniej/tylnej osi od środka auta - razem dają rozstaw osi (wheelbase)." );
		edited |= ImGui::SliderFloat( "Połowa rozstawu kół", &m_editTrackHalfWidth, 0.6f, 1.8f, "%.2f m" );
		HelpMarker( "Odległość lewego/prawego koła od środka auta - razem dają rozstaw kół (track). Szerzej = "
					"stabilniej w zakrętach, węziej = zwrotniej." );
		edited |= ImGui::SliderFloat( "Opuszczenie spoczynkowe", &m_editRestDrop, 0.20f, 1.20f, "%.2f m" );
		HelpMarker( "Jak daleko pod nadwoziem leży środek koła w pozycji spoczynkowej - bazowy prześwit przed "
					"dostrojeniem suwakiem 'Prześwit' w zakładce Zawieszenie." );
		edited |= ImGui::SliderFloat( "Gęstość koła", &m_editWheelDensity, 20.0f, 300.0f, "%.0f kg/m^3" );
		HelpMarker( "Masa koła (niesprężona) - wpływa na to, jak szybko koło reaguje na nierówności." );

		if ( edited )
		{
			m_structuralSetupDirty = true;
		}
	}

	// The sandbox itself - grip against the ground and how firmly the physics
	// solver resolves contact - plus the two everyday reset buttons. Visual
	// toggles and status live in Debug now; this tab is short on purpose.
void JozzVehicleM6RigLab::DrawWorldTab()
	{
		// Moved here from above the tab bar (2026-07-08, UI compaction pass):
		// switching a whole setup (drift/offroad/street) is a "which car am I
		// driving" decision, not something tied to whichever tuning tab happens
		// to be open, but it also doesn't need to sit in front of every tab all
		// the time - Świat (the sandbox/reset tab) is a natural home for a
		// once-in-a-while pick. Loading a preset still commits immediately (like
		// "Przywróć wszystkie ustawienia domyślne" already does) rather than
		// staging through Apply - loading half of a preset would leave the car
		// in a state that was never actually designed.
		SectionHeader( "Presety pojazdu" );
		{
			std::vector<const char*> items;
			items.reserve( m_availablePresets.size() );
			for ( const std::string& name : m_availablePresets )
			{
				items.push_back( name.c_str() );
			}
			ImGui::SetNextItemWidth( 12.0f * ImGui::GetFontSize() );
			if ( items.empty() )
			{
				ImGui::TextDisabled( "(brak zapisanych presetów - zapisz jeden poniżej)" );
			}
			else
			{
				ImGui::Combo( "##PresetSelect", &m_selectedPresetIndex, items.data(), (int)items.size() );
				ImGui::SameLine();
				bool validSelection = m_selectedPresetIndex >= 0 && m_selectedPresetIndex < (int)m_availablePresets.size();
				if ( ImGui::Button( "Wczytaj" ) && validSelection )
				{
					LoadPresetByName( m_availablePresets[m_selectedPresetIndex] );
				}
			}
			ImGui::SetNextItemWidth( 12.0f * ImGui::GetFontSize() );
			ImGui::InputTextWithHint( "##PresetName", "nazwa nowego presetu...", m_presetNameBuffer,
									   sizeof( m_presetNameBuffer ) );
			ImGui::SameLine();
			if ( ImGui::Button( "Zapisz jako" ) )
			{
				SaveCurrentAsPreset( m_presetNameBuffer );
			}
			// Quiet heads-up, not a confirm popup: overwriting your OWN preset by
			// re-saving under the same name is a normal, expected part of
			// iterating on a setup, so this only needs to be visible, not gate
			// the click behind another dialog.
			bool nameExists = false;
			for ( const std::string& name : m_availablePresets )
			{
				if ( name == m_presetNameBuffer )
				{
					nameExists = true;
					break;
				}
			}
			if ( nameExists )
			{
				ImGui::TextDisabled( "Preset '%s' już istnieje - zapis go nadpisze.", m_presetNameBuffer );
			}
			if ( m_presetStatus.empty() == false )
			{
				ImGui::TextColored( ImVec4( 0.6f, 0.85f, 0.6f, 1.0f ), "%s", m_presetStatus.c_str() );
			}
		}

		SectionHeader( "Przyczepność" );
		if ( ImGui::SliderFloat( "Tarcie opon", &m_config.wheelFriction, 0.4f, 2.5f, "%.2f" ) )
		{
			ApplyWheelFriction();
		}
		HelpMarker( "Mnożnik przyczepności opon o podłoże. Poniżej 1.0 = ślisko (lód, mokra nawierzchnia); powyżej "
					"1.0 = lepka nawierzchnia (slicki, asfalt na sucho)." );

		SectionHeader( "Solver kontaktu (zaawansowane)" );
		bool contactEdited = false;
		contactEdited |= ImGui::SliderFloat( "Sztywność kontaktu", &m_contactHertz, 10.0f, 100.0f, "%.0f Hz" );
		contactEdited |= ImGui::SliderFloat( "Tłumienie kontaktu", &m_contactDampingRatio, 2.0f, 25.0f, "%.1f" );
		contactEdited |= ImGui::SliderFloat( "Prędkość wypychania", &m_contactSpeed, 0.5f, 8.0f, "%.1f m/s" );
		HelpMarker( "Jak twardo silnik fizyki rozwiązuje przenikanie koła z podłożem. Domyślne wartości silnika są "
					"dobre dla większości przypadków - dotykaj tylko jeśli koła 'tuną' albo zapadają się w podłoże." );
		if ( contactEdited )
		{
			ApplyContactTuning();
		}
		if ( ImGui::Button( "Przywróć domyślne solvera" ) )
		{
			m_contactHertz = 30.0f;
			m_contactDampingRatio = 10.0f;
			m_contactSpeed = 3.0f;
			ApplyContactTuning();
		}

		// (Sterowanie mapą — seed offroad, przebudowa, teleporty per segment —
		// przeniesione do dedykowanej zakładki "Mapa" (fundament v3). Świat trzyma
		// tylko sterowanie pojazdu i symulacji, żeby nie było dwóch miejsc na to samo.)

		SectionHeader( "Reset" );
		if ( ImGui::Button( "Zresetuj swiat" ) )
		{
			ResetWorld();
		}
		HelpMarker( "Pelny restart symulacji: auto na miejsce startowe, przeszkody na miejsce, telemetria od zera. "
					"Nie rusza dostrojenia (Zawieszenie/Nadwozie/Naped/Kierownica) ani ustawien Debug - to samo co "
					"robi klawisz R, ale bez ryzyka utraty wlasnie zmienionych suwakow czy checkboxow." );
		ImGui::Spacing();
		if ( ImGui::Button( "Zresetuj pojazd" ) )
		{
			CreateVehicle();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Zresetuj przeszkody" ) )
		{
			ResetJozzVehicleM5TestCourseProps( m_testCourse );
		}
	}

	// Everything here is for LOOKING AT the rig, never for tuning how it
	// drives: visualization toggles, raw per-corner numbers, live plots, and
	// asset load status. Nothing on this tab changes vehicle behavior.
void JozzVehicleM6RigLab::DrawDebugTab()
	{
		SectionHeader( "Wizualizacje" );
		ImGui::Checkbox( "Model 3D kół", &m_showWheelVisuals );
		ImGui::Checkbox( "Model 3D zawieszenia", &m_showMountVisuals );
		bool useSteeringRig = UseSteeringRig();
		if ( ImGui::Checkbox( "Nowy rig kierowniczy — przód (rozgrzewka)", &useSteeringRig ) )
		{
			std::snprintf( m_config.frontSuspensionVisualModel, sizeof( m_config.frontSuspensionVisualModel ),
							useSteeringRig ? "rig_kierowniczy" : "klasyczny" );
			// Re-bake so the toggle is live without a rebuild (front corners only).
			SetupSteeringRig();
		}
		HelpMarker( "Przód: nowy model OneSided_Steering_Suspension_Rig zamiast starego mocowania; "
					"tył zostaje na starym. WheelCenter skręca z kołem, ChassisMount_b jedzie na ramieniu "
					"(nie skręca). Drążek (inboard→środek racka, L/P łączą się) i dumper: G3 zrobione. "
					"Cardan: G4." );
		if ( ImGui::Checkbox( "Pokaż nadwozie 3D", &m_showBodyVisual ) )
		{
			// The chassis collision box swaps with the skin (wariant A, Etap 3).
			UpdateChassisShapeVisibility();
		}
		HelpMarker( "Przełącznik WIDOKU - który model jest wybrany, ustawia zakładka Nadwozie. "
					"Wyłącz, by w tym labie zobaczyć samo zawieszenie (i bryłę kolizyjną) bez ramy." );
		if ( ImGui::Checkbox( "Pokaż bryłę kolizyjną nadwozia", &m_showChassisCollider ) )
		{
			UpdateChassisShapeVisibility();
		}
		HelpMarker( "Podgląd WARSTWY KOLIZJI: wymusza rysowanie bryły kolizyjnej chassis nawet pod ramą. "
					"Normalnie bryła chowa się pod nadwoziem 3D (i wraca sama przy modelu 'Brak')." );
		if ( ImGui::Checkbox( "Surowe kształty kolizji kół", &m_showPrimitiveWheelShapes ) )
		{
			UpdateWheelShapeVisibility();
		}
		ImGui::Checkbox( "Linie geometrii zawieszenia (wahacze/drążki)", &m_showRigDiagnostics );
		ImGui::Checkbox( "Podświetl wahacze (góra=czerwony, dół=niebieski)", &m_armTint );
		if ( ImGui::Button( "Wypisz geometrię narożników do konsoli" ) )
		{
			DumpCornerGeometry();
		}

		SectionHeader( "Status wczytanych assetów" );
		ImGui::TextWrapped( "koło: %s", m_wheelVisual.status.c_str() );
		ImGui::TextWrapped( "mocowanie L: %s", m_riggedMountL.status.c_str() );
		ImGui::TextWrapped( "mocowanie R: %s", m_riggedMountR.status.c_str() );
		ImGui::TextWrapped( "rig kier. L: %s", m_riggedSteeringL.status.c_str() );
		ImGui::TextWrapped( "rig kier. R: %s", m_riggedSteeringR.status.c_str() );
		ImGui::TextWrapped( "kontrakt rig kier.: %s", m_steeringContract.status.c_str() );
		ImGui::TextWrapped( "metadane: %s", m_assetMetadata.status.c_str() );

		SectionHeader( "Telemetria na żywo (PL/PP/TL/TP)" );
		const char* cornerNames[JOZZ_M6_CORNER_COUNT] = { "PL", "PP", "TL", "TP" };
		JozzVehicleM6WheelTelemetry current[JOZZ_M6_CORNER_COUNT];
		for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
		{
			current[corner] = GetJozzVehicleM6WheelTelemetry( m_vehicle, corner );
		}

		ImGui::Text( "kontakt z podłożem: PL %s  PP %s  TL %s  TP %s", current[0].groundContact ? "TAK" : "powietrze",
					 current[1].groundContact ? "TAK" : "powietrze", current[2].groundContact ? "TAK" : "powietrze",
					 current[3].groundContact ? "TAK" : "powietrze" );
		ImGui::Text( "obciążenie N: PL %.0f  PP %.0f  TL %.0f  TP %.0f", current[0].suspensionLoad,
					 current[1].suspensionLoad, current[2].suspensionLoad, current[3].suspensionLoad );
		ImGui::Text( "poślizg st.: PL %.1f  PP %.1f  TL %.1f  TP %.1f", 180.0f / B3_PI * current[0].slipAngle,
					 180.0f / B3_PI * current[1].slipAngle, 180.0f / B3_PI * current[2].slipAngle,
					 180.0f / B3_PI * current[3].slipAngle );
		ImGui::Text( "pochylenie koła st.: PL %.1f  PP %.1f  TL %.1f  TP %.1f", 180.0f / B3_PI * current[0].camberAngle,
					 180.0f / B3_PI * current[1].camberAngle, 180.0f / B3_PI * current[2].camberAngle,
					 180.0f / B3_PI * current[3].camberAngle );
		ImGui::Text( "obroty rad/s: PL %.1f  PP %.1f  TL %.1f  TP %.1f", current[0].spinSpeed, current[1].spinSpeed,
					 current[2].spinSpeed, current[3].spinSpeed );

		if ( m_telemetryCount < 2 )
		{
			return;
		}

		float latestTime = m_telemetryTime[( m_telemetryHead + TELEMETRY_CAPACITY - 1 ) % TELEMETRY_CAPACITY];
		int count = m_telemetryCount;
		int start = ( m_telemetryHead + TELEMETRY_CAPACITY - count ) % TELEMETRY_CAPACITY;

		static float times[TELEMETRY_CAPACITY];
		static float series[JOZZ_M6_CORNER_COUNT][TELEMETRY_CAPACITY];
		for ( int i = 0; i < count; ++i )
		{
			int index = ( start + i ) % TELEMETRY_CAPACITY;
			times[i] = m_telemetryTime[index];
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				series[corner][i] = m_telemetryTravel[corner][index];
			}
		}

		ImVec2 plotSize( ImGui::GetContentRegionAvail().x, 130.0f );
		if ( ImPlot::BeginPlot( "Skok zawieszenia", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "skok m" );
			ImPlot::SetupAxisLimits( ImAxis_X1, latestTime - 10.0, latestTime, ImPlotCond_Always );
			ImPlot::SetupAxisLimits( ImAxis_Y1, -m_config.reboundTravel * 1.2, m_config.compressionTravel * 1.2 );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				ImPlot::PlotLine( cornerNames[corner], times, series[corner], count );
			}
			ImPlot::EndPlot();
		}

		for ( int i = 0; i < count; ++i )
		{
			int index = ( start + i ) % TELEMETRY_CAPACITY;
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				series[corner][i] = m_telemetrySlip[corner][index];
			}
		}

		if ( ImPlot::BeginPlot( "Kąt poślizgu", plotSize, ImPlotFlags_NoTitle ) )
		{
			ImPlot::SetupAxes( "t", "poślizg st." );
			ImPlot::SetupAxisLimits( ImAxis_X1, latestTime - 10.0, latestTime, ImPlotCond_Always );
			for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
			{
				ImPlot::PlotLine( cornerNames[corner], times, series[corner], count );
			}
			ImPlot::EndPlot();
		}
	}

	// The engine/camera stats block above this (frame time, step count, camera
	// pivot) is folded behind a closed CollapsingHeader for this lab
	// (CondenseDebugOverlay override below) - with 6 tabs of sliders to fit,
	// that block ate a third of the panel for numbers nobody tunes with. Same
	// reasoning killed the multi-line control hint here: one line + a "(?)"
	// tooltip instead of three permanent TextDisabled lines. Presets used to
	// live here too, above the tab bar; they now live in the Świat tab
	// (DrawWorldTab) - "which car am I driving" is a per-session choice, not
	// something that needs to occupy prime panel space on every tab, and the
	// tab bar itself is what should greet you first.
bool JozzVehicleM6RigLab::DrawControls()
	{
		ImGui::Text( "prędkość %.1f m/s (%.0f km/h)", GetJozzVehicleM6ForwardSpeed( m_vehicle ),
					 3.6f * GetJozzVehicleM6ForwardSpeed( m_vehicle ) );
		ImGui::TextDisabled( "Sterowanie i R restart" );
		HelpMarker( "W/S jazda, A/D skręt, Spacja hamulec, T kamera, R restart.\n"
					"R restart zachowuje strojenie i ustawienia Debug (auto-zapis sesji).\n"
					"Przycisk 'Zresetuj świat' (zakładka Świat) robi to samo bez klawiatury." );
		ImGui::Separator();

		// The narrow default item width fits the old single-column panel; the
		// tabs host wider sliders.
		ImGui::PushItemWidth( 9.0f * ImGui::GetFontSize() );

		// Tab order follows a natural setup flow: what kind of suspension and
		// how it sits (Zawieszenie) -> what car it's bolted to (Nadwozie) ->
		// engine/steering feel -> sandbox -> look under the hood (Debug).
		// JOZZ_M6_TAB env (0-6) force-selects a tab once, for headless
		// --screenshot verification of a specific tab without UI interaction.
		auto tabFlags = [this]( int index ) {
			return ( m_forceTabIndex == index ) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		};
		if ( ImGui::BeginTabBar( "M6RigLabTabs", ImGuiTabBarFlags_None ) )
		{
			// "###TabX" pins the tab's identity to a fixed string regardless of
			// the visible label: ImGui hashes everything from "###" onward as the
			// ID and ignores what's before it for that purpose (see ImHashStr).
			// Without it, "Zawieszenie" and "Zawieszenie *" are two DIFFERENT
			// tabs as far as the tab bar is concerned - the moment a pending edit
			// appends " *", the bar sees the old tab vanish and a new one appear,
			// and its own closed-tab fallback jumps the active selection to
			// whatever tab it last remembers as second-most-recent (observed:
			// editing anything on Zawieszenie booted the user to Kierownica).
			// Same bug fires in reverse on Apply/Discard when " *" drops back off.
			if ( ImGui::BeginTabItem( m_structuralSetupDirty ? "Zawieszenie *###TabSuspension" : "Zawieszenie###TabSuspension",
									   nullptr, tabFlags( 0 ) ) )
			{
				DrawSuspensionTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( m_structuralSetupDirty ? "Nadwozie *###TabChassis" : "Nadwozie###TabChassis", nullptr,
									   tabFlags( 1 ) ) )
			{
				DrawChassisTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Napęd", nullptr, tabFlags( 2 ) ) )
			{
				DrawDriveTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Kierownica", nullptr, tabFlags( 3 ) ) )
			{
				DrawSteeringTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Świat", nullptr, tabFlags( 4 ) ) )
			{
				DrawWorldTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Mapa", nullptr, tabFlags( 5 ) ) )
			{
				DrawMapTab();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Debug", nullptr, tabFlags( 6 ) ) )
			{
				DrawDebugTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		m_forceTabIndex = -1;

		ImGui::PopItemWidth();

		// Apply bar: visible from every tab so a pending rebuild is never lost
		// behind the Zawieszenie/Nadwozie tabs.
		if ( m_structuralSetupDirty )
		{
			ImGui::Separator();
			ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ), "Są niezastosowane zmiany geometrii" );
			ImGui::SameLine();
			if ( ImGui::Button( "Zastosuj (przebuduj pojazd)" ) )
			{
				ApplyPendingStructuralSetup();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Odrzuć" ) )
			{
				SyncEditFromConfig();
			}
		}

		return true;
	}

// ============================================================================
// "Mapa" tab: one nested sub-tab per world segment (fundament v3). Each map
// fragment controls itself here instead of sharing the "Świat" list, so a new
// segment (like the scan island) gets its own panel without touching the others.
// ============================================================================
void JozzVehicleM6RigLab::DrawMapTab()
	{
		if ( ImGui::BeginTabBar( "SegmentyMapy", ImGuiTabBarFlags_None ) )
		{
			if ( ImGui::BeginTabItem( "Płyta" ) )
			{
				DrawMapPlateSegment();
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Offroad" ) )
			{
				DrawMapOffroadSegment();
				ImGui::EndTabItem();
			}
			// Label flips to "- wczytany" when live (ASCII only: the UI font has no
			// glyphs past Latin Extended-A). "###TabScan" pins the tab id so the
			// label change never reshuffles the tab bar.
			if ( ImGui::BeginTabItem( m_scanLoaded ? "Skan (wyspa) - wczytany###TabScan" : "Skan (wyspa)###TabScan" ) )
			{
				DrawMapScanSegment();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}

void JozzVehicleM6RigLab::DrawMapPlateSegment()
	{
		SectionHeader( "Płyta 400×400" );
		ImGui::TextWrapped( "Centralna płyta testowa (3×3 kafle, wierzch y=0) oraz poligon zawieszeń przy jej "
							"wschodniej krawędzi. Nawierzchnia płaska, deterministyczna." );
		ImGui::TextUnformatted( "Teleport" );
		bool first = true;
		for ( int i = 0; i < JozzWorldLayout::kWorldAnchorCount; ++i )
		{
			const JozzWorldLayout::JozzWorldAnchor& a = JozzWorldLayout::kWorldAnchors[i];
			if ( std::strncmp( a.name, "Offroad", 7 ) == 0 )
			{
				continue; // offroad points live in their own segment
			}
			if ( !first )
			{
				ImGui::SameLine();
			}
			first = false;
			if ( ImGui::Button( a.name ) )
			{
				TeleportTo( a.x, a.z );
			}
		}

		DrawFragmentSpawnControls( JozzWorldLayout::FragmentPlate );
	}

void JozzVehicleM6RigLab::DrawMapOffroadSegment()
	{
		SectionHeader( "Offroad (Etap 1)" );
		ImGui::TextWrapped( "Proceduralny teren offroad z centralnym masywem górskim, przylegający do wschodniej "
							"krawędzi płyty. W pełni deterministyczny względem seeda." );
		ImGui::SetNextItemWidth( 8.0f * ImGui::GetFontSize() );
		ImGui::InputInt( "Seed terenu", &m_worldSeedInput );
		ImGui::SameLine();
		if ( ImGui::Button( "Przebuduj teren" ) )
		{
			RegenerateTerrain();
		}
		HelpMarker( "Ten sam seed zawsze daje ten sam układ wzgórz i grani. Wpisz inną liczbę i przebuduj, "
					"żeby wylosować nowy wariant; płyta, przeszkody i skan zostają bez zmian." );
		ImGui::TextUnformatted( "Teleport" );
		bool first = true;
		for ( int i = 0; i < JozzWorldLayout::kWorldAnchorCount; ++i )
		{
			const JozzWorldLayout::JozzWorldAnchor& a = JozzWorldLayout::kWorldAnchors[i];
			if ( std::strncmp( a.name, "Offroad", 7 ) != 0 )
			{
				continue;
			}
			if ( !first )
			{
				ImGui::SameLine();
			}
			first = false;
			if ( ImGui::Button( a.name ) )
			{
				TeleportTo( a.x, a.z );
			}
		}

		DrawFragmentSpawnControls( JozzWorldLayout::FragmentOffroad );
	}

void JozzVehicleM6RigLab::DrawMapScanSegment()
	{
		SectionHeader( "Skan terenu (wyspa, fundament v3)" );
		ImGui::TextWrapped( "Zaimportowany skan fotogrametrii jako OSOBNA wyspa na północy mapy, dojazd tylko "
							"teleportem, krawędzie zostawione jako urwiska. Grunt to dokładny mesh w kategorii "
							"terenu (koła toczą się po nim). Ten skan jest brudny i nieprzerobiony - zaczepianie "
							"o korony drzew jest oczekiwane; czyste, rozdzielone skany wejdą tym samym kanałem." );
		ImGui::Spacing();

		if ( !m_scanLoaded )
		{
			if ( ImGui::Button( "Wczytaj skan" ) )
			{
				LoadScanTile();
			}
		}
		else
		{
			if ( ImGui::Button( "Przeładuj" ) )
			{
				UnloadScanTile();
				LoadScanTile();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Wyładuj" ) )
			{
				UnloadScanTile();
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Teleportuj na skan" ) )
			{
				TeleportToScan();
			}
		}

		ImGui::Checkbox( "Odwróć nawinięcie trójkątów", &m_scanFlipWinding );
		HelpMarker( "Fotogrametria nie gwarantuje zgodności nawinięcia z konwencją Box3D. Jeśli auto przenika "
					"przez powierzchnię skanu zamiast po niej jechać, zaznacz i przeładuj." );

		if ( m_scanLoaded && m_scanVisual.loaded )
		{
			if ( ImGui::Checkbox( "Pokaż siatkę kolizji (debug)", &m_scanShowCollider ) )
			{
				// Toggle only affects the debug overlay; the textured skin stays.
				SetShapeHidden( m_scanBodies.terrainShape, m_scanShowCollider == false );
			}
			HelpMarker( "Domyślnie widać teksturowaną skórę terenu. Zaznacz, żeby nałożyć na nią surową siatkę "
						"kolizyjną (mesh, po którym faktycznie toczą się koła) do inspekcji." );
		}

		ImGui::Spacing();
		ImGui::TextColored( m_scanLoaded ? ImVec4( 0.60f, 0.85f, 0.60f, 1.0f ) : ImVec4( 0.85f, 0.80f, 0.50f, 1.0f ),
							"%s", m_scanStatus.c_str() );
		if ( m_scanLoaded )
		{
			const b3AABB& b = m_scanBodies.worldBounds;
			ImGui::Text( "Kafle: %d    Trójkąty: %d    Wierzchołki: %d", (int)m_scanTiles.size(),
						 m_scanBodies.terrainTriangleCount, m_scanBodies.terrainVertexCount );
			ImGui::Text( "AABB świata:  x[%.0f, %.0f]   z[%.0f, %.0f]   y[%.1f, %.1f]", b.lowerBound.x, b.upperBound.x,
						 b.lowerBound.z, b.upperBound.z, b.lowerBound.y, b.upperBound.y );
			if ( m_scanVisual.loaded )
			{
				ImGui::TextColored( ImVec4( 0.60f, 0.85f, 0.60f, 1.0f ), "Render: %d grup, tekstury %d/%d, %.0f MB GPU",
									(int)m_scanVisual.groups.size(), m_scanVisual.textureCount,
									(int)m_scanVisual.groups.size(), (double)m_scanVisual.textureBytes / ( 1024.0 * 1024.0 ) );
			}
			else
			{
				ImGui::TextColored( ImVec4( 0.85f, 0.55f, 0.50f, 1.0f ), "Render: brak skóry (tylko kolizja / debug-draw)" );
			}

			// Spawn of the scan fragment: only meaningful while the island is live
			// (its built-in default is the island center). The persistent tier keys
			// by pack id, so a different scan brings its own saved spawn.
			DrawFragmentSpawnControls( JozzWorldLayout::FragmentScan );
		}
		else
		{
			ImGui::TextDisabled( "Źródło paczki: zmienna środowiskowa JOZZ_SCAN_PREVIEW_PACK (katalog z COMPLETE.json)" );
		}
	}

	// Effective spawn for a fragment: session ?? persistent ?? built-in. Writes
	// x/z and returns the active tier's label ("sesyjny"/"domyślny"/"wbudowany")
	// for the status line. Scan reads its persistent slot from m_scanSpawnById
	// (keyed by pack id); plate/offroad use their own persistent slot.
const char* JozzVehicleM6RigLab::EffectiveFragmentSpawn( JozzWorldLayout::JozzMapFragment fragment, float* outX,
														 float* outZ ) const
	{
		const JozzFragmentSpawn& fs = m_fragmentSpawn[fragment];
		if ( fs.session.set )
		{
			*outX = fs.session.x;
			*outZ = fs.session.z;
			return "sesyjny";
		}
		if ( fragment == JozzWorldLayout::FragmentScan )
		{
			auto it = m_scanSpawnById.find( m_scanId );
			if ( m_scanId.empty() == false && it != m_scanSpawnById.end() && it->second.set )
			{
				*outX = it->second.x;
				*outZ = it->second.z;
				return "domyślny";
			}
		}
		else if ( fs.persistent.set )
		{
			*outX = fs.persistent.x;
			*outZ = fs.persistent.z;
			return "domyślny";
		}
		BuiltinFragmentSpawn( fragment, outX, outZ );
		return "wbudowany";
	}

	// Built-in fallback spawn: the scan island's live center, else the named world
	// anchor for the plate/offroad ("Start" / "Offroad - wjazd").
void JozzVehicleM6RigLab::BuiltinFragmentSpawn( JozzWorldLayout::JozzMapFragment fragment, float* outX,
												float* outZ ) const
	{
		if ( fragment == JozzWorldLayout::FragmentScan && m_scanLoaded )
		{
			const b3AABB& b = m_scanBodies.worldBounds;
			*outX = 0.5f * ( b.lowerBound.x + b.upperBound.x );
			*outZ = 0.5f * ( b.lowerBound.z + b.upperBound.z );
			return;
		}
		const char* wanted = fragment == JozzWorldLayout::FragmentOffroad ? "Offroad - wjazd" : "Start";
		for ( int i = 0; i < JozzWorldLayout::kWorldAnchorCount; ++i )
		{
			if ( std::strcmp( JozzWorldLayout::kWorldAnchors[i].name, wanted ) == 0 )
			{
				*outX = JozzWorldLayout::kWorldAnchors[i].x;
				*outZ = JozzWorldLayout::kWorldAnchors[i].z;
				return;
			}
		}
		*outX = 0.0f;
		*outZ = 0.0f;
	}

	// Per-fragment spawn UI (2026-07-24), drawn inside each Mapa segment. Set from
	// the car's current spot: "Respawn tutaj" arms the SESSION default (nietrwały,
	// klawisz "R" wraca w to miejsce); "Zapisz jako domyślny" writes the PERSISTENT
	// default to the committed assets/vehicle_spawns.txt. Scratch X/Z is the working
	// value both actions consume, seeded once from the effective spawn.
void JozzVehicleM6RigLab::DrawFragmentSpawnControls( JozzWorldLayout::JozzMapFragment fragment )
	{
		JozzFragmentSpawn& fs = m_fragmentSpawn[fragment];

		if ( m_spawnScratchSeeded[fragment] == false )
		{
			float ex = 0.0f, ez = 0.0f;
			EffectiveFragmentSpawn( fragment, &ex, &ez );
			m_spawnScratch[fragment].x = ex;
			m_spawnScratch[fragment].z = ez;
			m_spawnScratchSeeded[fragment] = true;
		}

		ImGui::Spacing();
		SectionHeader( "Spawn pojazdu" );

		float effX = 0.0f, effZ = 0.0f;
		const char* source = EffectiveFragmentSpawn( fragment, &effX, &effZ );
		ImGui::Text( "Aktywny spawn: (%.1f, %.1f)  -  źródło: %s", effX, effZ, source );
		HelpMarker( "Kolejność: sesyjny (ustawiony teraz, nietrwały) > domyślny (zapisany między sesjami) > "
					"wbudowany. Pola X/Z poniżej to wartość robocza używana przez przyciski." );

		float xz[2] = { m_spawnScratch[fragment].x, m_spawnScratch[fragment].z };
		ImGui::SetNextItemWidth( 14.0f * ImGui::GetFontSize() );
		if ( ImGui::InputFloat2( "X / Z", xz, "%.1f" ) )
		{
			m_spawnScratch[fragment].x = xz[0];
			m_spawnScratch[fragment].z = xz[1];
		}

		if ( ImGui::Button( "Z pozycji auta" ) )
		{
			if ( m_vehicle.valid )
			{
				b3Pos p = b3Body_GetPosition( m_vehicle.chassisId );
				m_spawnScratch[fragment].x = p.x;
				m_spawnScratch[fragment].z = p.z;
			}
		}
		HelpMarker( "Przepisuje bieżącą pozycję auta (X/Z) do pól powyżej. Dojedź w dobre miejsce i kliknij." );
		ImGui::SameLine();
		if ( ImGui::Button( "Respawn tutaj" ) )
		{
			fs.session = { true, m_spawnScratch[fragment].x, m_spawnScratch[fragment].z };
			TeleportTo( m_spawnScratch[fragment].x, m_spawnScratch[fragment].z );
		}
		HelpMarker( "Odradza auto na polach X/Z i ustawia to jako spawn SESYJNY tego fragmentu (znika po "
					"restarcie; klawisz \"R\" wraca w to miejsce)." );

		if ( ImGui::Button( "Zapisz jako domyślny" ) )
		{
			if ( fragment == JozzWorldLayout::FragmentScan )
			{
				if ( m_scanId.empty() == false )
				{
					m_scanSpawnById[m_scanId] = { true, m_spawnScratch[fragment].x, m_spawnScratch[fragment].z };
					SaveFragmentSpawns();
				}
			}
			else
			{
				fs.persistent = { true, m_spawnScratch[fragment].x, m_spawnScratch[fragment].z };
				SaveFragmentSpawns();
			}
		}
		HelpMarker( "Zapisuje pola X/Z jako TRWAŁY domyślny spawn fragmentu do assets/vehicle_spawns.txt "
					"(przeżywa restart i czysty build). Skan zapisuje się per identyfikator paczki." );

		bool hasSession = fs.session.set;
		bool hasPersistent = fragment == JozzWorldLayout::FragmentScan
								 ? ( m_scanId.empty() == false && m_scanSpawnById.count( m_scanId ) > 0 &&
									 m_scanSpawnById.at( m_scanId ).set )
								 : fs.persistent.set;
		if ( hasSession || hasPersistent )
		{
			if ( hasSession )
			{
				if ( ImGui::Button( "Wyczyść sesyjny" ) )
				{
					fs.session.set = false;
					m_spawnScratchSeeded[fragment] = false; // re-seed from the next tier down
				}
				if ( hasPersistent )
				{
					ImGui::SameLine();
				}
			}
			if ( hasPersistent )
			{
				if ( ImGui::Button( "Usuń domyślny" ) )
				{
					if ( fragment == JozzWorldLayout::FragmentScan )
					{
						m_scanSpawnById.erase( m_scanId );
					}
					else
					{
						fs.persistent.set = false;
					}
					SaveFragmentSpawns();
					m_spawnScratchSeeded[fragment] = false;
				}
			}
		}
	}

void JozzVehicleM6RigLab::LoadScanTile()
	{
		// Default source: the JOZZ_SCAN_PREVIEW_PACK env dir. The path overload below
		// is the seam a future multi-scan picker calls with a chosen library entry -
		// today the only caller resolves the single env pack, so behaviour is unchanged.
		std::filesystem::path dir = FindJozzScanPackDir();
		if ( dir.empty() )
		{
			m_scanStatus = "skan: ustaw JOZZ_SCAN_PREVIEW_PACK na katalog z COMPLETE.json";
			m_scanLoaded = false;
			return;
		}
		LoadScanTile( dir );
	}

void JozzVehicleM6RigLab::LoadScanTile( const std::filesystem::path& dir )
	{
		m_scanTiles.clear();
		std::string err;
		auto readT0 = std::chrono::steady_clock::now();
		bool readOk = LoadJozzScanPackGeometry( dir, &m_scanTiles, &err, m_scanFlipWinding );
		if ( std::getenv( "JOZZ_SCAN_DUMP" ) != nullptr )
		{
			double readMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - readT0 ).count();
			std::printf( "[scan] reader %.0f ms  (%d kafli)\n", readMs, (int)m_scanTiles.size() );
			std::fflush( stdout );
		}
		if ( readOk == false || m_scanTiles.empty() )
		{
			m_scanStatus = "skan: " + ( err.empty() ? std::string( "paczka pusta" ) : err );
			m_scanLoaded = false;
			return;
		}

		// Union the tile AABBs (reader fills each tile's bounds) to place the whole
		// island: pin its south edge north of the plate, floor its lowest point at
		// ground level, center it on x=0. Any scan size then clears the world.
		b3AABB local = m_scanTiles[0].bounds;
		for ( size_t i = 1; i < m_scanTiles.size(); ++i )
		{
			const b3AABB& tb = m_scanTiles[i].bounds;
			local.lowerBound.x = b3MinFloat( local.lowerBound.x, tb.lowerBound.x );
			local.lowerBound.y = b3MinFloat( local.lowerBound.y, tb.lowerBound.y );
			local.lowerBound.z = b3MinFloat( local.lowerBound.z, tb.lowerBound.z );
			local.upperBound.x = b3MaxFloat( local.upperBound.x, tb.upperBound.x );
			local.upperBound.y = b3MaxFloat( local.upperBound.y, tb.upperBound.y );
			local.upperBound.z = b3MaxFloat( local.upperBound.z, tb.upperBound.z );
		}

		JozzScanTilePlacement placement;
		placement.origin.x = -0.5f * ( local.lowerBound.x + local.upperBound.x );
		placement.origin.y = JozzWorldLayout::kScanGroundY - local.lowerBound.y;
		placement.origin.z = JozzWorldLayout::kScanSouthEdgeZ - local.lowerBound.z;

		// Cooked-BVH cache key: the scan's identity (pack dir leaf -- content-hashed
		// by the pipeline) + the build flags + winding, all of which change the
		// cooked mesh. First load cooks + writes it; later loads read it back
		// (~instant) instead of rebuilding the BVH (the 14 s Debug / 1.8 s Release
		// cost). Lives under build/ so it is disposable and never committed.
		auto envBool = []( const char* n, bool d ) { const char* v = std::getenv( n ); return v ? atoi( v ) != 0 : d; };
		std::filesystem::path leaf = dir.filename().empty() ? dir.parent_path().filename() : dir.filename();
		m_scanId = leaf.string(); // pack identity: keys the persistent scan spawn (multi-scan foundation) + BVH cache
		m_scanPackDir = dir.string(); // remembered so "R"/reopen can reload THIS pack (persisted to the build/-local debug session)
		char flags[24];
		std::snprintf( flags, sizeof( flags ), "w%de%dm%df%d", envBool( "JOZZ_SCAN_WELD", true ) ? 1 : 0,
					   envBool( "JOZZ_SCAN_EDGES", true ) ? 1 : 0, envBool( "JOZZ_SCAN_MEDIAN", false ) ? 1 : 0,
					   m_scanFlipWinding ? 1 : 0 );
		std::error_code ec;
		std::filesystem::create_directories( "build/scan_cache", ec );
		std::string cachePath = ( std::filesystem::path( "build/scan_cache" ) / ( leaf.string() + "_" + flags + ".b3mesh" ) )
									.generic_string();

		m_scanBodies = BuildJozzScanTileFromPack( m_worldId, m_scanTiles, placement, JOZZ_M6_TERRAIN_CATEGORY,
												  cachePath.c_str() );
		m_scanLoaded = m_scanBodies.ok;
		m_scanStatus = "skan: " + m_scanBodies.status;

		// Textured render skin (M2): load from the SAME pack at the SAME origin the
		// collider was placed, so the skin sits exactly on the collision surface (no
		// independent recomputation, no drift). Render uses source winding (flip
		// false) -- the front-face convention that the offline preview renders
		// correctly -- independent of the collision winding toggle. One repeatable
		// channel: any pack the pipeline emits loads collision + skin together.
		if ( m_scanBodies.ok )
		{
			std::string visualErr;
			auto visT0 = std::chrono::steady_clock::now();
			bool visOk = LoadJozzScanVisual( dir, placement.origin, false, &m_scanVisual, &visualErr );
			if ( std::getenv( "JOZZ_SCAN_DUMP" ) != nullptr )
			{
				double visMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - visT0 ).count();
				std::printf( "[scan] visual %.0f ms  (%s)\n", visMs, visOk ? m_scanVisual.status.c_str() : visualErr.c_str() );
				std::fflush( stdout );
			}
			if ( visOk == false )
			{
				m_scanStatus += "  |  render: " + visualErr;
			}
			// Hide the collision mesh from debug-draw once the skin covers it (same
			// SetShapeHidden pattern the chassis box uses under the body), so the
			// island shows as textured terrain, not a gray wireframe shell. If the
			// skin failed, leave the collider visible so there is still something.
			SetShapeHidden( m_scanBodies.terrainShape, m_scanVisual.loaded && m_scanShowCollider == false );
		}
	}

void JozzVehicleM6RigLab::UnloadScanTile()
	{
		DestroyJozzScanTile( m_worldId, &m_scanBodies );
		DestroyJozzScanVisual( &m_scanVisual );
		m_scanTiles.clear();
		m_scanBodies = JozzScanTileBodies{};
		m_scanLoaded = false;
		m_scanPackDir.clear();		 // explicit unload: do NOT reload this pack on the next "R"/reopen
		m_scanReloadOnBoot = false;
		m_scanStatus = "skan: wyładowany";
	}

void JozzVehicleM6RigLab::TeleportToScan()
	{
		if ( m_scanLoaded == false )
		{
			return;
		}
		// Center of the island; GetGroundHeightAt is scan-aware (raycast) so the
		// four-wheel spawn sampling in CreateVehicle lands the car on the surface.
		const b3AABB& b = m_scanBodies.worldBounds;
		float cx = 0.5f * ( b.lowerBound.x + b.upperBound.x );
		float cz = 0.5f * ( b.lowerBound.z + b.upperBound.z );
		TeleportTo( cx, cz );
	}

