// Rig Q3 - QUARTER CAR. Kolo na piascie i zawieszeniu, pod masa resorowana,
// napedzane momentem, na zadanym profilu drogi.
//
// Kontrakt eksperymentu: Q3_QUARTER_CAR_CONTRACT.md. KAZDA liczba domyslna w tym
// pliku jest tam zapisana PRZED pierwszym przebiegiem; zmiana ktorejkolwiek
// zmienia eksperyment, a nie widok.
//
// Ten sam podzial odpowiedzialnosci co w jozz_wheel_rig.h i z tego samego powodu:
//   ten modul     - swiat, droga, ciala, wiez, naped, metryki
//   wheel_bench.c - PROTOKOL (okno pomiarowe, CSV, porownanie kandydatow)
//   sample        - kamera, overlay, wejscie
//
// Obwiednia i masa kola pochodza z jozz_wheel_rig - to NIE jest wygoda, tylko
// warunek reguly transferu: kandydat w Q3 musi byc fizycznie tym samym obiektem,
// ktory jedzie w Q2A, inaczej porownanie Q2->Q3 nie mierzy zawieszenia, tylko
// dwie rozne implementacje tego samego kola.

#pragma once

#include "box3d/box3d.h"
#include "jozz_wheel_rig.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// --- droga -------------------------------------------------------------------
// Na plaskiej plycie Q3 mierzy FASETY obwiedni, a nie transfer udaru - czyli
// odpowiada na pytanie Q1/Q2, nie na swoje (kontrakt par. 6). Plaski przebieg
// zostaje jako KONTROLA i jako punkt odniesienia dla Q2->Q3.
typedef enum
{
	JOZZ_QC_ROAD_FLAT = 0,
	JOZZ_QC_ROAD_CLEAT = 1, // pojedynczy prog
	JOZZ_QC_ROAD_COMB = 2,	// grzebien progow, wymuszenie okresowe
	JOZZ_QC_ROAD_COUNT = 3
} JozzQcRoad;

const char* JozzQc_RoadName( JozzQcRoad r );

typedef enum
{
	JOZZ_QC_DRIVE_SPEED = 0,  // DOMYSLNY: stala predkosc, mierzony moment
	JOZZ_QC_DRIVE_TORQUE = 1, // staly moment - wyniki NIEporownywalne miedzy kandydatami
	JOZZ_QC_DRIVE_COAST = 2,  // wersja bierna, kontrola
	JOZZ_QC_DRIVE_COUNT = 3
} JozzQcDrive;

const char* JozzQc_DriveName( JozzQcDrive d );

// --- konfiguracja ------------------------------------------------------------
typedef struct
{
	// KOLO - te same pola i te same warianty co Q2A.
	JozzRigVariant variant;
	int segments; // scianki pryzmatu ALBO kapsuly pierscienia
	float wheelR, wheelW, crownR;
	float unsprungKg, inertiaSpin, inertiaTrans, density, friction;

	// NADWOZIE I ZAWIESZENIE
	float sprungKg;
	// Sztywnosc podawana FIZYCZNIE, w N/m. `suspensionHertz` NIE jest polem tego
	// rigu - jest wielkoscia WYLICZANA (JozzQc_Hertz), bo w box3d idzie za masa
	// ZREDUKOWANA obu cial, a nie za masa resorowana (F-18). Ta sama liczba
	// herców na dwoch kandydatach o roznej masie nieresorowanej to dwie rozne
	// sprezyny - a program kol porownuje kandydatow, wiec to nie jest niuans.
	float springNPerM;
	float zeta;
	// Skok podawany OD PUNKTU STATYCZNEGO, bo tak jest podawany w pojezdzie i
	// tak jedyny ma sens: przy 13 500 N/m i 150 kg samo ugiecie statyczne to
	// 111 mm, wiec "skok 100 mm" liczony od zera oznaczalby rig stojacy na
	// zderzaku juz na postoju. Zlapane przez samokontrole przy pierwszym
	// przebiegu Q3 - kontrakt par. 5 podawal tylko sztywnosc.
	float bumpTravel;  // m od statyki do pelnego sciskania
	float droopTravel; // m od statyki do pelnego wywieszenia
	float mountH;	   // wysokosc mocowania nad osia kola

	// DROGA
	float groundHalfX, groundHalfZ;
	JozzQcRoad road;
	float obstacleH, obstacleLen, combSpacing;

	// PROTOKOL
	JozzQcDrive drive;
	double targetSpeed;
	double kp, ki;		 // regulator PI, wyjscie w N*m
	double maxTorque;	 // ogranicznik momentu; saturacja UNIEWAZNIA przebieg
	double constTorque;	 // tylko dla JOZZ_QC_DRIVE_TORQUE
	double startSpeed;	 // warunek poczatkowy; toczenie nominalne -v/R
	double startX;

	// PRZYRZAD - zmiana zmienia NARZEDZIE, nie badany obiekt
	double gravity;
	double dt;
	int substeps;
} JozzQcConfig;

JozzQcConfig JozzQc_DefaultConfig( void );

// Odcisk konfiguracji do naglowka przebiegu i do overlaya okna. Bez niego liczba
// nie wie, z czego pochodzi.
void JozzQc_ConfigDigest( const JozzQcConfig* c, char* out, size_t cap );

// Masa zredukowana obu cial - wielkosc, za ktora idzie sztywnosc wiezu (F-18).
double JozzQc_ReducedMass( const JozzQcConfig* c );

// Przeliczenie N/m -> hertz. Zmierzone jako DOKLADNE (F-18): k = m_red*(2 pi f)^2
// zgadza sie z pomiarem do 5 cyfr w zakresie 0.5-6.0 Hz.
double JozzQc_Hertz( const JozzQcConfig* c );

// Ugiecie statyczne przy tej sztywnosci i tej masie resorowanej, w metrach.
double JozzQc_StaticSag( const JozzQcConfig* c );

// Widok kola jako JozzRigConfig - JEDNO zrodlo obwiedni i masy dla obu rigow.
JozzRigConfig JozzQc_WheelConfig( const JozzQcConfig* c );

// --- odczyt po kroku ---------------------------------------------------------
typedef struct
{
	int step;
	double time;
	double x;			 // pozycja wzdluzna nadwozia
	double speed;		 // predkosc wzdluzna nadwozia
	double driveTorque;	 // N*m, ZNAK jak przylozony
	int saturated;
	double travel;		 // m, DODATNIE = sciskanie (zmierzona konwencja, F-18)
	double sprungY;
	double sprungAccelY; // m/s^2
	double wheelY;
	double omegaSpin;
	double slipRatio;
	double normalImpulse; // suma impulsow normalnych na kole w tym kroku
	int loadedPoints;
	int newLoadedPoints;
	int airborne;
	int limitHit;
} JozzQcSample;

// --- rig ---------------------------------------------------------------------
typedef struct
{
	b3WorldId world;
	b3BodyId ground;
	b3BodyId chassis;
	b3BodyId wheel;
	b3JointId joint;

	JozzQcConfig cfg;

	// Z CZEGO to naprawde zbudowano - wypelniane PO budowie, odczytem z silnika,
	// nie przepisaniem z konfiguracji. Rozroznienie jest istotne: rig, ktory
	// raportuje zamowione liczby zamiast zbudowanych, jest przyrzadem, ktory
	// potwierdza wlasne zalozenia.
	double builtSprungKg, builtUnsprungKg;
	double builtHertz;
	double travelUpper, travelLower; // granice skoku w tej samej konwencji co `travel`
	int shapeCount;
	int obstacleCount;

	int step;
	double prevSprungVy;
	int perturbed;
	int perturbCount;
	char lastPerturbation[96];

	double integral;
} JozzQcRig;

// Buduje rig i SPRAWDZA, czy zbudowany uklad jest tym, ktory zamowiono.
// Zwraca 1, albo 0 z opisem w `err`. Zwrocenie 0 jest normalnym wynikiem:
// nieszczelny pierscien kapsul albo wariant nieprzedstawialny to odpowiedz, a
// nie awaria.
//
// Powod istnienia samokontroli, dosownie z pomiaru: pierwsza wersja sondy Q3-1
// ustawiala mase nadwozia PRZED blokadami ruchu, a b3Body_SetMotionLocks wola
// b3UpdateBodyMassData i przelicza mase Z KSZTALTOW (src/body.c). 150 kg po cichu
// stawalo sie 0 kg i cztery tabele pomiarow opisywaly nieruchomy sufit.
int JozzQc_Create( JozzQcRig* rig, const JozzQcConfig* cfg, const JozzRigRenderHooks* hooks, char* err,
				   size_t errCap );
void JozzQc_Destroy( JozzQcRig* rig );

// DOKLADNIE jeden krok o stalym dt. Kolejnosc jest czescia kontraktu:
// odczyt -> regulator -> ApplyTorque -> b3World_Step -> pomiar.
void JozzQc_Step( JozzQcRig* rig, JozzQcSample* out );

void JozzQc_MarkPerturbation( JozzQcRig* rig, const char* what );

// Punkty kontaktu kola do rysowania. Bez klasyfikacji - klasyfikacja nalezy do
// eksperymentu, nie do rysowania.
int JozzQc_ContactPoints( const JozzQcRig* rig, JozzRigContactPoint* out, int cap );

// --- okno pomiarowe ----------------------------------------------------------
// Metryki z par. 7 kontraktu, liczone na oknie po rozgrzewce.
typedef struct
{
	int steps;
	double driveTorqueMean;
	double lossPower;
	double sprungAccelRms;
	double airborneFraction;
	double travelRms, travelMin, travelMax;
	int limitHits;
	double contactChurnPct;
	double loadedPointsAvg;
	double slipRatioMean;
	double speedMean;
	int saturatedSteps;

	// Werdykt kontraktu par. 8. `invalid` != 0 znaczy: przebieg SIE ODBYL i jest
	// zapisany, ale nie wolno go uzyc do porownania. Reguła twarda 6: nie kasujemy
	// wynikow negatywnych, oznaczamy je.
	int invalid;
	char invalidWhy[160];
} JozzQcWindow;

void JozzQc_WindowBegin( JozzQcWindow* w );
void JozzQc_WindowAdd( JozzQcWindow* w, const JozzQcSample* s );
void JozzQc_WindowEnd( JozzQcWindow* w, const JozzQcRig* rig );

#define JOZZ_QC_TRACE_HEADER                                                                                           \
	"step,time,x,speed,drive_torque,travel,sprung_y,sprung_accel_y,wheel_y,omega_spin,slip,normal_impulse,"           \
	"loaded_pts,new_loaded_pts,airborne,limit_hit,saturated\n"
void JozzQc_TraceLine( const JozzQcSample* s, char* out, size_t cap );

#ifdef __cplusplus
}
#endif
