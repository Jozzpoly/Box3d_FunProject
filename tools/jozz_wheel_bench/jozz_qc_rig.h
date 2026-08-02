// Rig Q3 - QUARTER CAR. Kolo na piascie i zawieszeniu, pod masa resorowana,
// napedzane momentem, na zadanym profilu drogi.
//
// Kontrakt eksperymentu: Q3_QUARTER_CAR_CONTRACT.md. KAZDA liczba domyslna w tym
// pliku jest tam zapisana PRZED pierwszym przebiegiem; zmiana ktorejkolwiek
// zmienia eksperyment, a nie widok.
//
// Ten sam podzial odpowiedzialnosci co w jozz_wheel_rig.h i z tego samego powodu:
//   ten modul     - swiat, droga, ciala, wiez, naped, metryki, PROTOKOL POMIARU
//   wheel_bench.c - CSV, tabela, linia polecen
//   sample        - kamera, overlay, wejscie
//
// Protokol pomiaru (rozgrzewka, okno, powtorzenia, lista kandydatow) przeniosl
// sie tutaj ze stendu, bo od wersji warsztatowej uruchamiaja go OBA frontendy:
// stend przy --qc-compare i okno przyciskiem "zmierz jak stend". Dopoki siedzial
// w wheel_bench.c, jedyna droga do liczby bench-grade z okna bylo napisanie
// drugiej kopii - a dwie kopie protokolu to dwa niezalezne twierdzenia o tym
// samym przebiegu, czyli dokladnie to, czemu ten podzial mial zapobiec.
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

// Najmniejsze N, przy ktorym TA konfiguracja jest budowalna. Poza `torus-N`
// zwraca 3. Istnieje osobno od JozzRig_MinTorusSegments, zeby frontend nie
// musial skladac JozzRigConfig tylko po to, zeby zapytac o granice suwaka.
int JozzQc_MinSegments( const JozzQcConfig* c );

// Najwieksze N budowalne dla tego wariantu: dla pryzmatu twardy limit hulla
// (F-01), dla pierscienia kapsul limit praktyczny.
int JozzQc_MaxSegments( const JozzQcConfig* c );

// Wciska `segments` w zakres BUDOWALNY i zwraca 1, gdy cos zmienil.
// Powod: pierscien kapsul robi sie nieszczelny, gdy Owner ciagnie promien korony
// w dol, a przelaczenie torus-64 -> pryzmat zada 64 scianek, ktorych hull nie
// przyjmie. W obu przypadkach KAZDA przebudowa konczyla sie bledem, a okno
// pokazywalo "rig nie powstal" bez drogi powrotnej. Wciskniecie N jest jedyna
// zmiana zachowujaca zamowiona geometrie przekroju; jest JAWNE, bo frontend
// pokazuje, ze N zostalo zmienione i o ile.
//
// Dla kuli NIC NIE ROBI: pole nie bierze udzialu w budowie, a wciskanie go w
// zakres kuli kasowalo liczbe - przejscie torus-64 -> sphere -> torus-N wracalo
// z N=3. Przelaczenie wariantu tam i z powrotem ma byc odwracalne.
//
// Wolane WYLACZNIE ze sciezki suwaka. Plik konstrukcji i linia polecen maja byc
// odrzucane, a nie po cichu poprawiane: opisuja inna konstrukcje niz ta, ktora
// dalo by sie uruchomic.
int JozzQc_ClampSegments( JozzQcConfig* c );

// --- konstrukcja jako plik ---------------------------------------------------
// Ten sam format i ten sam kontrakt co `.rig` (jozz_wheel_rig.h): `klucz wartosc`,
// `#` to komentarz, brak klucza = domyslna, nieznany klucz = BLAD, zapis->odczyt
// ->zapis jest bajtowo identyczny. Rozszerzenie `.qc`, bo opisuje CALY narożnik,
// nie samo kolo.
#define JOZZ_QC_CONFIG_FORMAT 1
#define JOZZ_QC_CONFIG_TEXT_CAP 3072

int JozzQc_ConfigToText( const JozzQcConfig* c, char* out, size_t cap, const char* note );
int JozzQc_ConfigFromText( JozzQcConfig* c, const char* text, char* err, size_t errCap );
int JozzQc_ConfigWriteFile( const JozzQcConfig* c, const char* path, const char* note );
int JozzQc_ConfigReadFile( JozzQcConfig* c, const char* path, char* err, size_t errCap );

// Sprawdza SAM format: kompletnosc tabeli pol, przebieg tam i z powrotem,
// stabilnosc powtornego zapisu i odrzucanie zepsutego pliku. Wolane przez bramke.
int JozzQc_ConfigSelfTest( char* err, size_t errCap );

// --- grupy pol ---------------------------------------------------------------
// Te same grupy, ktore dziela plik `.qc` na sekcje. Sluza do dwoch rzeczy naraz:
// zapisu i PRZYWRACANIA. Powod, dla ktorego stoja tutaj, a nie w oknie: gdyby
// frontend mial wlasna liste "co nalezy do zawieszenia", kazde nowe pole trzeba
// by dopisac w dwoch miejscach, a pole pominiete w tej drugiej liscie byloby
// polem, ktorego NIE DA SIE przywrocic - i nikt by tego nie zauwazyl.
#define JOZZ_QC_GROUP_WHEEL "kolo"
#define JOZZ_QC_GROUP_SUSPENSION "zawieszenie"
#define JOZZ_QC_GROUP_ROAD "droga"
#define JOZZ_QC_GROUP_DRIVE "naped"
#define JOZZ_QC_GROUP_INSTRUMENT "instrument"

// Przywraca wartosci domyslne w JEDNEJ grupie i zwraca liczbe zmienionych pol.
// `group == NULL` znaczy WSZYSTKIE pola.
int JozzQc_ResetGroup( JozzQcConfig* c, const char* group );

// Ile pol tej grupy rozni sie od domyslnych. Okno pokazuje te liczbe przy
// naglowku sekcji, bo "co ja wlasciwie poruszylem" jest pytaniem, ktore pada po
// dziesieciu minutach pracy, a panel bez tego na nie nie odpowiada.
int JozzQc_ChangedInGroup( const JozzQcConfig* c, const char* group );

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

	// Wstrzasarka. Stan RIGU, nie konstrukcji: nie idzie do `.qc` i nie da sie
	// jej wlaczyc z linii polecen, bo przebieg z wymuszeniem nie jest przebiegiem
	// kontraktowym. Domyslnie wylaczona, wiec kazdy pomiar headless przechodzi
	// przez ta sama sciezke co przed jej dodaniem.
	int shakerOn;
	double shakerHz, shakerN;

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

// --- strojenie na zywo -------------------------------------------------------
// Pola, ktore box3d potrafi zmienic na biegnacym wiezie. Istnieja, bo warsztat
// bez nich nie jest warsztatem: kazde pokretlo wymagalo przebudowy, a przebudowa
// kasuje przebieg, wiec zmiany sztywnosci NIE DALO SIE poczuc - dalo sie tylko
// obejrzec dwa osobne przebiegi i porownac je z pamieci.
//
// Kontrakt: te funkcje aktualizuja TAKZE rig->cfg, wiec odcisk konfiguracji
// zawsze opisuje to, co wiez faktycznie robi. Nie sa zaburzeniem fizyki (nie
// dotykaja stanu cial), ale uniewazniaja OKNO POMIAROWE - dlatego frontend
// zeruje okno po kazdym wywolaniu. Liczby bench-grade i tak pochodza z
// JozzQc_MeasureCell, ktore ZAWSZE buduje rig od nowa.

// Sztywnosc w N/m i tlumienie. Przelicza tez granice skoku, bo ugiecie statyczne
// idzie za sztywnoscia - rozdzielenie tych dwoch operacji dawalo zawieszenie,
// ktore przy miekkiej sprezynie stalo na zderzaku bez zadnego komunikatu.
void JozzQc_SetSuspension( JozzQcRig* rig, float springNPerM, float zeta );

// Skok liczony OD PUNKTU STATYCZNEGO, jak w konfiguracji.
void JozzQc_SetTravel( JozzQcRig* rig, float bumpTravel, float droopTravel );

// Tryb napedu i jego nastawy. Zmiana trybu zeruje calke regulatora: bez tego
// powrot do trybu predkosci startuje z windupem uzbieranym, zanim naped w ogole
// byl wlaczony.
void JozzQc_SetDrive( JozzQcRig* rig, JozzQcDrive mode, double targetSpeed, double constTorque );
void JozzQc_SetDriveGains( JozzQcRig* rig, double kp, double ki, double maxTorque );

// --- reka: ingerencja w biegnacy rig -----------------------------------------
// Podstawowe proby zawieszenia, ktorych mysz nie umie wykonac powtarzalnie.
// Kazda z nich jest ZABURZENIEM i sama sie tak oznacza (JozzQc_MarkPerturbation),
// wiec przebieg po niej jest niewazny - dokladnie tak jak po chwycie mysza.
// Stoja w module rigu, a nie w oknie, z tego samego powodu co protokol pomiaru:
// jedyna implementacja tego, co wolno zrobic cialom, ma byc jedna.

// Podnosi CALY narożnik o `meters` i zeruje predkosci pionowe. Podnoszone sa OBA
// ciala, bo podniesienie samego nadwozia naciaga wiez do zderzaka wywieszenia i
// to, co potem widac, jest odbiciem od ogranicznika, a nie upadkiem zawieszenia.
void JozzQc_LiftCorner( JozzQcRig* rig, double meters );

// Pionowy impuls w mase resorowana, w N*s. Dodatni = w gore.
void JozzQc_KickChassis( JozzQcRig* rig, double impulseNs );

// Wymuszenie sinusoidalne na masie resorowanej: sila `amplitudeN` o czestotliwosci
// `hz`. Przemiatanie czestotliwoscia pokazuje rezonans zawieszenia wprost - a
// hertz wiezu jest wielkoscia WYLICZANA (F-18), wiec warto go umiec zobaczyc.
void JozzQc_SetShaker( JozzQcRig* rig, int on, double hz, double amplitudeN );

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

// --- protokol pomiaru --------------------------------------------------------
// Kontrakt par. 8. Te trzy liczby sa czescia eksperymentu, nie ustawieniem
// wygody - zmiana ktorejkolwiek zmienia to, co znacza wszystkie dotychczasowe
// wyniki Q3.
#define JOZZ_QC_WARMUP_STEPS 120 // 2 s: dojazd do progu i wygaszenie startu
#define JOZZ_QC_WINDOW_STEPS 600 // 10 s okna pomiarowego
#define JOZZ_QC_REPEATS 3

// Przesuniecie punktu startu miedzy powtorzeniami. Powod jest ZMIERZONY (F-24):
// przesuniecie mocowania nadwozia o 35 cm - wielkosc geometrycznie neutralna dla
// tego wiezu - przerzucilo `airborne` dla torus-32 przy 13 m/s z 6.0% na 10.8%,
// czyli przez prog wazności. Kolo skaczace po fasetach jest ukladem chaotycznym,
// wiec pojedynczy przebieg podaje liczbe z dokladnoscia, ktorej nie ma.
// Przesuwamy PUNKT STARTU, bo na plaskiej plycie to jedyna zmiana nietykajaca
// ani kola, ani zawieszenia, ani drogi - zmienia sama faze spotkania z gruntem.
extern const double JozzQc_StartJitter[JOZZ_QC_REPEATS];

typedef struct
{
	JozzQcWindow win;
	double msPerStep;
	int shapes;
	double ripple;
	int traceCount;
	int built;
	char err[192];
} JozzQcRunResult;

// Jeden przebieg: rozgrzewka, potem okno. `traceOut` (moze byc NULL) dostaje
// probki okna - zapis do pliku nalezy do wolajacego i celowo dzieje sie PO
// przebiegu, zeby I/O nie wchodzilo do mierzonego `msPerStep`.
JozzQcRunResult JozzQc_MeasureOne( const JozzQcConfig* cfg, int warmup, int windowSteps, JozzQcSample* traceOut,
								   int traceCap );

// Komorka macierzy: srednia z JOZZ_QC_REPEATS przebiegow i POLOWA ROZRZUTU.
// Rozrzut jest tu rowno wazny z wartoscia - bez niego tabela zaprasza do
// czytania roznic, ktorych nie ma. `invalid` zapala sie, gdy KTOREKOLWIEK
// powtorzenie bylo niewazne.
typedef struct
{
	double torque, torqueSpread;
	double loss;
	double accel, accelSpread;
	double airborne, airborneSpread;
	double travelRms;
	double churn;
	double slip;
	double speed;
	double msPerStep;
	int shapes;
	double ripple;
	int repeats;
	int invalid;
	int built;
	char err[192];
	char why[160];
} JozzQcCell;

JozzQcCell JozzQc_MeasureCell( const JozzQcConfig* base, int warmup, int windowSteps );

// --- lista kandydatow --------------------------------------------------------
// Lista JEST eksperymentem, nie wygoda interfejsu, i dlatego stoi tutaj: gdy
// mialy ja obie strony osobno, okno pokazywalo `prism-Nmax` z wpisanym na sztywno
// N=42, a stend probowal realna granice hulla tego buildu. Rozjazd oznaczalby,
// ze Owner ocenia feel czegos innego niz to, co zostalo zmierzone.
//   sphere       kontrola: obwiednia bez zadnej struktury
//   prism-Nmax   dzisiejsze kolo produktu, na granicy tego, co przyjmuje hull
//   prism-32     ten sam pryzmat przy N rownym torusowi (uczciwe zestawienie)
//   torus-32     nowy kandydat przy tym samym N co pryzmat
//   torus-64     nowy kandydat TAM, GDZIE PRYZMAT JUZ NIE SIEGA
typedef struct
{
	const char* label;
	JozzRigVariant variant;
	int segments;
} JozzQcCandidate;

#define JOZZ_QC_CANDIDATE_CAP 8
int JozzQc_Candidates( JozzQcCandidate* out, int cap );

#ifdef __cplusplus
}
#endif
