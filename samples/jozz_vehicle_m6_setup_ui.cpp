// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_m6_setup_ui.h"

#include "imgui.h"
#include "jozz_vehicle_body_registry.h"

#include <cstdio>
#include <cstring>

namespace
{

// Same two presentation helpers the workshop uses, so the ported tabs read and
// behave identically here.
void HelpMarker( const char* text )
{
	ImGui::SameLine();
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::BeginItemTooltip() )
	{
		ImGui::PushTextWrapPos( ImGui::GetFontSize() * 32.0f );
		ImGui::TextUnformatted( text );
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void SectionHeader( const char* title )
{
	ImGui::Spacing();
	ImGui::SeparatorText( title );
}

} // namespace

void JozzVehicleM6SetupEdit::SyncFromConfig( const JozzVehicleM6Config& config )
{
	frontRigType = config.frontRigType;
	rearRigType = config.rearRigType;
	wishbone = config.wishbone;
	trailingArm = config.trailingArm;
	knuckleMass = config.knuckleMass;
	armMass = config.armMass;
	envelopeMode = config.wheelEnvelope.mode;
	envelopeLayers = config.wheelEnvelope.unionLayerCount;
	strutCasterDeg = config.strutCasterDeg;
	maxSteeringAngleDegrees = config.maxSteeringAngleDegrees;
	frontToeDeg = config.frontToeDeg;
	rearToeDeg = config.rearToeDeg;
	chassisHalfExtents = config.chassisHalfExtents;
	chassisDensity = config.chassisDensity;
	cgVerticalOffset = config.cgVerticalOffset;
	axleHalfSpacing = config.axleHalfSpacing;
	trackHalfWidth = config.trackHalfWidth;
	restDrop = config.restDrop;
	wheelDensity = config.wheelDensity;
	structuralDirty = false;
	bodyModelChanged = false;
}

void JozzVehicleM6SetupEdit::ApplyToConfig( JozzVehicleM6Config* config )
{
	config->frontRigType = frontRigType;
	config->rearRigType = rearRigType;
	config->wishbone = wishbone;
	config->trailingArm = trailingArm;
	config->knuckleMass = knuckleMass;
	config->armMass = armMass;
	config->wheelEnvelope.mode = envelopeMode;
	config->wheelEnvelope.unionLayerCount = envelopeLayers;
	config->strutCasterDeg = strutCasterDeg;
	config->maxSteeringAngleDegrees = maxSteeringAngleDegrees;
	config->frontToeDeg = frontToeDeg;
	config->rearToeDeg = rearToeDeg;
	config->chassisHalfExtents = chassisHalfExtents;
	config->chassisDensity = chassisDensity;
	config->cgVerticalOffset = cgVerticalOffset;
	config->axleHalfSpacing = axleHalfSpacing;
	config->trackHalfWidth = trackHalfWidth;
	config->restDrop = restDrop;
	config->wheelDensity = wheelDensity;
	structuralDirty = false;
}

bool DrawJozzVehicleM6DriveTab( JozzVehicleM6Config* config )
{
	bool live = false;
	ImGui::TextWrapped( "Silnik trzyma limit obrotow, gaz skaluje moment." );
	HelpMarker( "To, czy kolo sie trzyma czy traci przyczepnosc (pali gume), zalezy od momentu wzgledem "
				"przyczepnosci w kontakcie z podlozem - nie ma tu osobnego 'przelacznika poslizgu'." );
	live |= ImGui::SliderFloat( "Moment napedowy", &config->maxDriveTorque, 0.0f, 2000.0f, "%.0f N*m" );
	HelpMarker( "Maksymalny moment silnika na kolo. Wiecej = mocniejsze przyspieszenie, ale tez latwiej "
				"przekrecic kola (wheelspin) na sliskiej nawierzchni." );
	live |= ImGui::SliderFloat( "Limit obrotow", &config->maxDriveSpeed, 5.0f, 100.0f, "%.0f rad/s" );
	HelpMarker( "Predkosc obrotowa kola (rad/s), przy ktorej silnik przestaje ciagnac - decyduje razem z "
				"promieniem kola o predkosci maksymalnej auta." );
	live |= ImGui::SliderFloat( "Prog spadku momentu", &config->driveTaperStart, 0.2f, 0.95f, "%.2f x obr." );
	HelpMarker( "Od jakiej czesci limitu obrotow moment zaczyna malec w strone zera - symuluje silnik "
				"dochodzacy do czerwonego pola." );
	live |= ImGui::SliderFloat( "Moment hamowania", &config->brakeTorque, 0.0f, 2500.0f, "%.0f N*m" );
	HelpMarker( "Moment hamulcow na kolo przy trzymaniu spacji. Wiecej = krotsza droga hamowania, ale latwiej "
				"zablokowac kola (utrata sterownosci)." );
	live |= ImGui::SliderFloat( "Moment na biegu jalowym", &config->coastTorque, 0.0f, 40.0f, "%.0f N*m" );
	HelpMarker( "Lekki opor silnika, gdy nie dotykasz gazu ani hamulca - jak puszczenie sprzegla bez gazu." );
	live |= ImGui::Checkbox( "Naped na wszystkie kola", &config->allWheelDrive );
	HelpMarker( "Wylaczone = naped tylko na tylna os. Wlaczone = moment idzie na wszystkie 4 kola - wiecej "
				"przyczepnosci przy starcie, mniej driftu na gazie." );
	ImGui::Separator();
	live |= ImGui::SliderFloat( "Opor aerodynamiczny", &config->aeroDragArea, 0.2f, 2.0f, "%.2f m^2" );
	HelpMarker( "Opor powietrza rosnacy z kwadratem predkosci. To ON ogranicza predkosc maksymalna, nie sztywny "
				"limit - wieksza wartosc = nizszy V-max." );
	return live;
}

bool DrawJozzVehicleM6SteeringTab( JozzVehicleM6Config* config, JozzVehicleM6SetupEdit* edit )
{
	bool live = false;
	ImGui::TextWrapped( "W ruchu puszczona kierownica sama wraca do srodka dzieki fizyce (caster), nie skryptowi. "
						"Na postoju kola zostaja skrecone - jak w prawdziwym aucie." );
	HelpMarker( "Trzymanie A/D wlacza sprezyne zebatki + serwo (wspomaganie). Puszczenie zostawia tylko tarcie "
				"zebatki, wiec geometria zwrotnicy i sily z kontaktu z podlozem same kieruja kolami." );
	ImGui::Checkbox( "Odwroc kierowanie (preferencja)", &edit->invertSteering );
	ImGui::Separator();
	if ( ImGui::SliderFloat( "Maksymalny skret kol (wymaga Zastosuj)", &edit->maxSteeringAngleDegrees, 20.0f, 45.0f,
							 "%.0f st." ) )
	{
		edit->structuralDirty = true;
	}
	HelpMarker( "Kat skretu kola przy pelnym locku kierownicy. Strukturalne - przebudowuje zebatke i plot "
				"bezpieczenstwa, stad wymaga Zastosuj. Zbyt wysoka wartosc razem z wysokim 'Udzial Ackermanna' "
				"moze wepchnac drazek w martwy punkt - Zastosuj SAM zacisnie te wartosc." );
	if ( edit->clampStatus.empty() == false )
	{
		ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ), "%s", edit->clampStatus.c_str() );
	}
	ImGui::Separator();
	live |= ImGui::SliderFloat( "Sztywnosc kierownicy", &config->steeringHertz, 2.0f, 25.0f, "%.1f Hz" );
	HelpMarker( "Jak szybko zebatka goni zadany kat skretu, gdy trzymasz A/D. Wyzej = ostrzejsza, bardziej "
				"'gokartowa' reakcja." );
	live |= ImGui::SliderFloat( "Tlumienie kierownicy", &config->steeringDampingRatio, 0.2f, 3.0f, "%.2f" );
	ImGui::SliderFloat( "Sila wspomagania", &config->rackServoForce, 0.0f, 20000.0f, "%.0f N" );
	HelpMarker( "Ile sily ma wspomaganie, gdy trzymasz kierownice - musi pokonac moment parkingowy obciazonej "
				"opony (~700 N*m na kolo), inaczej auto 'nie posluchа' przy postoju." );
	ImGui::SliderFloat( "Tarcie zebatki - bazowe", &config->rackFrictionBase, 0.0f, 200.0f, "%.0f N" );
	HelpMarker( "[FIZYCZNY] Staly opor uszczelek i lozysk kolumny - dziala zawsze, niezaleznie od obciazenia. "
				"Mniej = kierownica zywsza i sama sie prostuje; wiecej = spokojniejsza, ale moze zostawac lekko "
				"skrecona po wybojach." );
	ImGui::SliderFloat( "Tarcie zebatki - od obciazenia", &config->rackFrictionLoadCoeff, 0.0f, 0.40f, "%.2f" );
	HelpMarker( "[FIZYCZNY] Ile tarcia dokłada kazdy niuton bocznego obciazenia drazkow. Rosnie SAMO przy twardych "
				"ladowaniach i mocnym skrecie - trzyma wtedy stabilnosc - a przy spokojnej jezdzie prawie znika." );
	ImGui::SliderFloat( "Tarcie skretu kolumny", &config->steeringFrictionTorque, 0.0f, 200.0f, "%.0f N*m" );
	HelpMarker( "To samo co tarcie zebatki, ale dla osi na kolumnie McPhersona zamiast wahaczy." );
	ImGui::SliderFloat( "[ARCADE] Wspomaganie powrotu", &config->rackCenteringHertz, 0.0f, 30.0f, "%.0f Hz" );
	HelpMarker( "MECHANIKA POZA MODELEM FIZYCZNYM (ADR-0006). Domyslnie 0 = WYLACZONE (realistycznie). Prawdziwe "
				"auto NIE centruje kol na postoju. Zauwazalne dopiero od ok. 10 Hz; im wyzej, tym mocniej walczy z "
				"naturalnym kontra-skretem w poslizgu." );
	return live;
}

bool DrawJozzVehicleM6ChassisTab( JozzVehicleM6Config* config, JozzVehicleM6SetupEdit* edit )
{
	bool edited = false;

	// Body skin choice + offset are LIVE (visual only), so they deliberately
	// stay out of the structural dirty flow - picking a skin must not demand an
	// Apply it does not need.
	SectionHeader( "Model nadwozia (wyglad)" );
	{
		int count = 0;
		const JozzVehicleBodyModelDef* models = GetJozzVehicleBodyModels( &count );
		int current = 0; // fallback to "brak" when the key is unknown
		for ( int i = 0; i < count; ++i )
		{
			if ( std::strcmp( models[i].key, config->bodyVisualModel ) == 0 )
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
					std::snprintf( config->bodyVisualModel, sizeof( config->bodyVisualModel ), "%s", models[i].key );
					edit->bodyModelChanged = true;
				}
			}
			ImGui::EndCombo();
		}
		HelpMarker( "Wyglad nadwozia - czysto wizualna skora na bryle fizycznej. Nie zmienia fizyki: bryla "
					"kolizyjna i jej wymiary (sekcje nizej) dzialaja jak dotad. Wybor wchodzi do presetow." );

		ImGui::SliderFloat( "Przesuniecie przod/tyl", &config->bodyVisualOffset.x, -0.50f, 0.50f, "%.2f m" );
		ImGui::SliderFloat( "Przesuniecie gora/dol", &config->bodyVisualOffset.y, -0.50f, 0.50f, "%.2f m" );
		ImGui::SliderFloat( "Przesuniecie lewo/prawo", &config->bodyVisualOffset.z, -0.50f, 0.50f, "%.2f m" );
		HelpMarker( "Dostrojenie pozycji modelu wzgledem bryly fizycznej, w osiach nadwozia (X przod, Y gora, "
					"Z lewo). Baza per model siedzi w rejestrze - to jest korekta." );
		if ( ImGui::Button( "Wyzeruj przesuniecie" ) )
		{
			config->bodyVisualOffset = { 0.0f, 0.0f, 0.0f };
		}
	}
	ImGui::Separator();

	SectionHeader( "Wymiary nadwozia" );
	edited |= ImGui::SliderFloat( "Polowa dlugosci", &edit->chassisHalfExtents.x, 0.8f, 2.5f, "%.2f m" );
	edited |= ImGui::SliderFloat( "Polowa wysokosci", &edit->chassisHalfExtents.y, 0.15f, 0.70f, "%.2f m" );
	edited |= ImGui::SliderFloat( "Polowa szerokosci", &edit->chassisHalfExtents.z, 0.35f, 1.00f, "%.2f m" );
	HelpMarker( "Rozmiar skrzyni nadwozia (pudelka). 'Polowa' bo liczone od srodka - realna dlugosc/szerokosc to "
				"dwa razy tyle." );
	edited |= ImGui::SliderFloat( "Gestosc (masa)", &edit->chassisDensity, 50.0f, 600.0f, "%.0f kg/m^3" );
	HelpMarker( "Razem z wymiarami wyzej decyduje o masie nadwozia. Lekki drifter: nisko; ciezarowka: wysoko." );
	edited |= ImGui::SliderFloat( "Obnizenie srodka ciezkosci", &edit->cgVerticalOffset, -0.10f, 0.40f, "%.2f m" );
	HelpMarker( "Jak nisko pod geometrycznym srodkiem nadwozia lezy faktyczny srodek ciezkosci. Nizej = "
				"stabilniej w zakretach." );

	SectionHeader( "Rozstaw osi i kol" );
	edited |= ImGui::SliderFloat( "Polowa rozstawu osi", &edit->axleHalfSpacing, 0.6f, 2.5f, "%.2f m" );
	HelpMarker( "Odleglosc przedniej/tylnej osi od srodka auta - razem daja rozstaw osi (wheelbase)." );
	edited |= ImGui::SliderFloat( "Polowa rozstawu kol", &edit->trackHalfWidth, 0.6f, 1.8f, "%.2f m" );
	HelpMarker( "Odleglosc lewego/prawego kola od srodka auta - razem daja rozstaw kol (track). Szerzej = "
				"stabilniej w zakretach, weziej = zwrotniej." );
	edited |= ImGui::SliderFloat( "Opuszczenie spoczynkowe", &edit->restDrop, 0.20f, 1.20f, "%.2f m" );
	HelpMarker( "Jak daleko pod nadwoziem lezy srodek kola w pozycji spoczynkowej - bazowy przeswit." );
	edited |= ImGui::SliderFloat( "Gestosc kola", &edit->wheelDensity, 20.0f, 300.0f, "%.0f kg/m^3" );
	HelpMarker( "Masa kola (niesprezona) - wplywa na to, jak szybko kolo reaguje na nierownosci." );

	if ( edited )
	{
		edit->structuralDirty = true;
	}
	return false; // everything live here is visual; the physics bits need Apply
}

bool DrawJozzVehicleM6SuspensionTab( JozzVehicleM6Config* config, JozzVehicleM6SetupEdit* edit )
{
	bool edited = false; // needs a rebuild
	bool live = false;

	SectionHeader( "Typ zawieszenia" );
	const char* rigTypes[] = { "Kolumna (prosta, tania - McPherson)", "Podwojny wahacz (widoczne ramiona)",
							   "Wahacz wleczony (model Jozza)" };
	edited |= ImGui::Combo( "Przednia os", &edit->frontRigType, rigTypes, 3 );
	edited |= ImGui::Combo( "Tylna os", &edit->rearRigType, rigTypes, 3 );
	if ( edit->frontRigType == JOZZ_M6_RIG_TRAILING_ARM )
	{
		ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ),
							"Wahacz wleczony nie skreca - z przodu auto pojedzie tylko na wprost." );
	}

	SectionHeader( "Postawa - jak stoi auto (najwazniejsze ustawienia)" );
	edited |= ImGui::SliderFloat( "Opadanie wahacza", &edit->wishbone.restArmDroopDeg, 0.0f, 16.0f, "%.1f st." );
	HelpMarker( "Wahacze zwisaja W DOL do kola w spoczynku zamiast wyginac sie do gory. 16 st. to zbadany "
				"bezpieczny sufit. Dziala tylko na osiach z podwojnym wahaczem. Wymaga Zastosuj." );
	live |= ImGui::SliderFloat( "Przeswit przod", &config->suspensionPreloadFront, -0.08f, 0.20f, "%.3f m" );
	live |= ImGui::SliderFloat( "Przeswit tyl", &config->suspensionPreloadRear, -0.08f, 0.20f, "%.3f m" );
	HelpMarker( "Docisk wstepny sprezyny: podnosi lub obniza dana os, na zywo, NIEZALEZNIE od twardosci sprezyny." );
	live |= ImGui::SliderFloat( "Skok sciskania", &config->compressionTravel, 0.10f, 0.70f, "%.2f m" );
	live |= ImGui::SliderFloat( "Skok odbicia", &config->reboundTravel, 0.10f, 0.60f, "%.2f m" );
	HelpMarker( "Jak daleko kolo moze sie ruszyc w gore (sciskanie) i w dol (odbicie) od pozycji spoczynkowej. "
				"Offroad chce obu duzo, drift/tor chce ciasno. Na zywo, bez przebudowy." );
	{
		// The hinge anti-fold guard saturates past asin(0.95): beyond that the
		// requested travel exceeds what the arm arc can deliver, so warn rather
		// than let the slider imply travel the geometry cannot give.
		float travel = b3MaxFloat( config->compressionTravel, config->reboundTravel );
		float saturation = 1.25f * travel / b3MaxFloat( edit->wishbone.lowerArmLength, 0.05f );
		if ( saturation >= 0.95f )
		{
			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ) );
			ImGui::TextWrapped( "Skok wiekszy niz zasieg wahacza (%.0f%% nasycenia) - realny skok ogranicza "
								"dlugosc ramienia, nie suwak.",
								(double)( 100.0f * saturation / 0.95f ) );
			ImGui::PopStyleColor();
		}
	}

	SectionHeader( "Sprezyny i tlumienie (na zywo)" );
	live |= ImGui::SliderFloat( "Twardosc sprezyny", &config->suspensionHertz, 1.0f, 12.0f, "%.1f Hz" );
	HelpMarker( "Miekko = wiecej komfortu i przyczepnosci w terenie, ale wiecej przechylu. Twardo = szybsza, "
				"bardziej torowa reakcja." );
	live |= ImGui::SliderFloat( "Tlumienie", &config->suspensionDampingRatio, 0.2f, 2.0f, "%.2f" );
	HelpMarker( "Jak szybko gasna drgania po odbiciu. Za malo = auto 'skacze'; za duzo = zawieszenie sztywnieje." );
	live |= ImGui::SliderFloat( "Mnoznik twardosci - przod", &config->frontSuspensionScale, 0.5f, 2.0f, "%.2f x" );
	live |= ImGui::SliderFloat( "Mnoznik twardosci - tyl", &config->rearSuspensionScale, 0.5f, 2.0f, "%.2f x" );
	HelpMarker( "Mnoznik twardosci i tlumienia osobno dla przodu i tylu." );

	SectionHeader( "Stabilizatory przechylu (na zywo)" );
	ImGui::SliderFloat( "Stabilizator przod", &config->arbFrontStiffness, 0.0f, 40000.0f, "%.0f N/m" );
	ImGui::SliderFloat( "Stabilizator tyl", &config->arbRearStiffness, 0.0f, 40000.0f, "%.0f N/m" );
	HelpMarker( "Ogranicza przechyl nadwozia w zakrecie. Mocniejszy przedni = wiecej podsterownosci; mocniejszy "
				"tylny = auto chetniej 'wchodzi w tyl'." );
	edited |= ImGui::Checkbox( "[ARCADE] Wspomaganie pionowania", &config->uprightAssist );
	HelpMarker( "MECHANIKA POZA MODELEM FIZYCZNYM (ADR-0006). Sztuczna sila trzymajaca nadwozie poziomo - wlacz "
				"tylko gdy auto sie przewraca mimo dobrze ustawionych stabilizatorow." );

	if ( ImGui::CollapsingHeader( "Zaawansowane: geometria wahaczy" ) )
	{
		ImGui::Indent();
		ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ),
							"Zmiany dzialaja na fizyke - model 3D auta NIE przeskalowuje sie." );
		edited |= ImGui::SliderFloat( "Caster (wyprzedzenie)", &edit->wishbone.casterDeg, -2.0f, 12.0f, "%.1f st." );
		HelpMarker( "Wiekszy caster = silniejsze samo-centrowanie kierownicy i mocniejsza kontra w poslizgu. "
					"Ustawienia driftowe: 7-10 st." );
		edited |= ImGui::SliderFloat( "Pochylenie sworznia", &edit->wishbone.kingpinInclinationDeg, 0.0f, 15.0f, "%.1f st." );
		edited |= ImGui::SliderFloat( "Offset sworznia", &edit->wishbone.kingpinOffset, 0.05f, 0.25f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Wysokosc zwrotnicy", &edit->wishbone.uprightHalfHeight, 0.10f, 0.30f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Dlugosc gornego wahacza", &edit->wishbone.upperArmLength, 0.20f, 0.55f, "%.2f m" );
		HelpMarker( "Krotszy gorny wahacz wzgledem dolnego = szybszy przyrost camberu przy skoku." );
		edited |= ImGui::SliderFloat( "Dlugosc dolnego wahacza", &edit->wishbone.lowerArmLength, 0.25f, 0.70f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Rozstaw mocowan wahacza", &edit->wishbone.armHalfSpread, 0.12f, 0.40f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Cofniecie ramienia kierown.", &edit->wishbone.steeringArmBack, 0.10f, 0.25f, "%.2f m" );
		edited |= ImGui::Checkbox( "Trapez Ackermanna (mechaniczny)", &edit->wishbone.ackermannTrapezoid );
		HelpMarker( "Katuje ramiona kierownicze do srodka, zeby kolo wewnetrzne skrecalo mocniej niz zewnetrzne - "
					"czysto geometrycznie." );
		if ( edit->wishbone.ackermannTrapezoid )
		{
			edited |= ImGui::SliderFloat( "Udzial Ackermanna", &edit->wishbone.ackermannFraction, 0.0f, 1.0f, "%.2f" );
			HelpMarker( "0 = brak Ackermanna, 1 = pelna geometria. Wyzsza wartosc przybliza drazek do martwego "
						"punktu - 'Maksymalny skret kol' SAM sie zacisnie po Zastosuj." );
		}
		edited |= ImGui::SliderFloat( "Wysokosc mocowania amortyzatora", &edit->wishbone.coiloverTopHeight, 0.25f, 0.60f,
									  "%.2f m" );
		edited |= ImGui::SliderFloat( "Masa zwrotnicy", &edit->knuckleMass, 10.0f, 50.0f, "%.0f kg" );
		edited |= ImGui::SliderFloat( "Masa wahacza", &edit->armMass, 2.0f, 15.0f, "%.1f kg" );
		edited |= ImGui::SliderFloat( "Caster kolumny (osie kolumnowe)", &edit->strutCasterDeg, -2.0f, 12.0f, "%.1f st." );
		edited |= ImGui::SliderFloat( "Zbieznosc (toe) przod", &edit->frontToeDeg, -3.0f, 3.0f, "%.1f st." );
		HelpMarker( "Dodatni = zbieznosc (toe-in) - stabilniej na wprost. Ujemny = rozbieznosc - zywszy skret." );
		edited |= ImGui::SliderFloat( "Zbieznosc (toe) tyl", &edit->rearToeDeg, -3.0f, 3.0f, "%.1f st." );
		ImGui::Unindent();
	}

	if ( ImGui::CollapsingHeader( "Zaawansowane: wahacz wleczony (model Jozza)" ) )
	{
		ImGui::Indent();
		edited |= ImGui::SliderFloat( "Os obrotu przed kolem", &edit->trailingArm.pivotOffset.x, 0.30f, 0.90f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Os obrotu nad kolem", &edit->trailingArm.pivotOffset.y, -0.05f, 0.35f, "%.2f m" );
		edited |= ImGui::SliderFloat( "Masa wahacza wleczonego", &edit->trailingArm.armMass, 6.0f, 25.0f, "%.0f kg" );
		HelpMarker( "Masa samego ramienia (bez kola). Zmiana tutaj automatycznie przelicza sztywnosc tak, zeby "
					"twardosc 'na kole' zostala ta sama." );
		ImGui::Unindent();
	}

	if ( ImGui::CollapsingHeader( "Zaawansowane: ksztalt kolizji kola" ) )
	{
		ImGui::Indent();
		const char* envelopes[] = { "Sfera (gladka, wybrzusza sie na boki)", "Walec (prawdziwa szerokosc, graniasty)",
									"Suma fazowa (eksperymentalne)",
									"Mieszana: sfera + prawdziwa szerokosc (domyslne)" };
		edited |= ImGui::Combo( "Ksztalt", &edit->envelopeMode, envelopes, 4 );
		HelpMarker( "Ksztalt fizycznej bryly kola (nie wizualny model 3D). Domyslna mieszana daje idealny kontakt "
					"z terenem BEZ 'niewidzialnej sciany' obok przeszkod." );
		if ( edit->envelopeMode == JOZZ_M6_ENVELOPE_PHASED_UNION )
		{
			edited |= ImGui::SliderInt( "Warstwy sumy", &edit->envelopeLayers, 2, 4 );
		}
		ImGui::Unindent();
	}

	if ( edited )
	{
		edit->structuralDirty = true;
	}
	return live;
}

void ApplyJozzVehicleM6LiveTuning( const JozzVehicleM6& vehicle, const JozzVehicleM6Config& config )
{
	if ( vehicle.valid == false )
	{
		return;
	}

	for ( int corner = 0; corner < JOZZ_M6_CORNER_COUNT; ++corner )
	{
		const JozzVehicleM6CornerRuntime& runtime = vehicle.corners[corner];
		bool isFront = corner == JOZZ_M6_FRONT_LEFT || corner == JOZZ_M6_FRONT_RIGHT;
		float scale = isFront ? config.frontSuspensionScale : config.rearSuspensionScale;

		if ( runtime.rigType == JOZZ_M6_RIG_INTEGRATED_STRUT )
		{
			b3WheelJoint_SetSuspensionHertz( runtime.strutJointId, config.suspensionHertz * scale );
			b3WheelJoint_SetSuspensionDampingRatio( runtime.strutJointId, config.suspensionDampingRatio * scale );
			b3Joint_WakeBodies( runtime.strutJointId );

			if ( isFront )
			{
				float maxAngle = config.maxSteeringAngleDegrees * B3_PI / 180.0f;
				b3WheelJoint_SetSteeringLimits( runtime.strutJointId, -maxAngle, maxAngle );
				b3WheelJoint_SetSteeringHertz( runtime.strutJointId, config.steeringHertz );
				b3WheelJoint_SetSteeringDampingRatio( runtime.strutJointId, config.steeringDampingRatio );
				b3WheelJoint_SetMaxSteeringTorque( runtime.strutJointId, config.maxSteeringTorque );
			}
		}
		else
		{
			// Trailing corners carry the effective-mass compensation they were
			// built with (1.0 on wishbone corners), and map wheel-space preload
			// and travel through their motion ratio so a click of the slider
			// means the same thing at the wheel on every rig type.
			float hertzScale = runtime.rigType == JOZZ_M6_RIG_TRAILING_ARM ? runtime.trailingCoiloverHertzScale : 1.0f;
			b3DistanceJoint_SetSpringHertz( runtime.coiloverJointId, config.suspensionHertz * scale * hertzScale );
			b3DistanceJoint_SetSpringDampingRatio( runtime.coiloverJointId, config.suspensionDampingRatio * scale );

			float motionRatio = runtime.rigType == JOZZ_M6_RIG_TRAILING_ARM ? runtime.trailingMotionRatio : 1.0f;
			float design = runtime.coiloverDesignLength;
			float preload = isFront ? config.suspensionPreloadFront : config.suspensionPreloadRear;
			b3DistanceJoint_SetLength( runtime.coiloverJointId, design + preload * motionRatio );
			b3DistanceJoint_SetLengthRange( runtime.coiloverJointId,
											b3MaxFloat( 0.05f, design - config.compressionTravel * motionRatio ),
											design + config.reboundTravel * motionRatio );
			b3Joint_WakeBodies( runtime.coiloverJointId );
		}

		for ( int i = 0; i < runtime.wheelShapeCount; ++i )
		{
			b3Shape_SetFriction( runtime.wheelShapeIds[i], config.wheelFriction );
		}
	}

	if ( B3_IS_NON_NULL( vehicle.rackJointId ) )
	{
		b3PrismaticJoint_SetSpringHertz( vehicle.rackJointId, config.steeringHertz );
		b3PrismaticJoint_SetSpringDampingRatio( vehicle.rackJointId, config.steeringDampingRatio );
		b3Joint_WakeBodies( vehicle.rackJointId );
	}
}
