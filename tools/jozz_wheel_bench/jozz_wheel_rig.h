// Wspolny rig kola: JEDNA implementacja fizyki dla stendu headless i dla okna
// wizualnego. Powod istnienia tego pliku: dopoki tryb wizualny mial wlasna kopie
// swiata, ciala, materialu i regulatora, "widac to samo" bylo twierdzeniem, a nie
// faktem. Teraz jest to fakt na poziomie kodu - oba frontendy linkuja te same
// funkcje i te same stale.
//
// Podzial odpowiedzialnosci:
//   ten modul     - swiat, grunt, cialo, material, masa, regulator, calkowanie pracy
//   wheel_bench.c - PROTOKOL eksperymentu (kwalifikacja, okno, CSV, manifest)
//   sample        - kamera, overlay, wejscie, jawnie oznaczone zaburzenia
//
// Modul jest C i jest wolany takze z C++ (samples), wiec: zadnych literalow
// zlozonych w naglowku, extern "C" ponizej.

#pragma once

#include "box3d/box3d.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// --- tozsamosc kola (stend v2) ------------------------------------------------
#define JOZZ_RIG_WHEEL_R 0.5141f
#define JOZZ_RIG_WHEEL_W 0.4375f
#define JOZZ_RIG_UNSPRUNG_KG 44.0f
#define JOZZ_RIG_PI 3.14159265358979f

// --- stale eksperymentu Q2A --------------------------------------------------
// KAZDA z tych liczb jest zapisana w Q2A_CONSTANT_SPEED_BOX_CONTRACT.md PRZED
// pierwszym przebiegiem. Zmiana ktorejkolwiek zmienia eksperyment, nie widok.
#define JOZZ_RIG_TARGET_V 13.0
#define JOZZ_RIG_KP 440.0	 // m/tau przy tau = 0.1 s
#define JOZZ_RIG_KI 1100.0	 // omega_n = 5 rad/s, zeta = 1.0 (krytycznie tlumiony)
#define JOZZ_RIG_FMAX 1900.0 // = efektywne obciazenie, czyli limit przyczepnosci przy mu=1
#define JOZZ_RIG_LOAD_N 1900.0
#define JOZZ_RIG_GRAVITY 10.0 // FAKTYCZNA grawitacja swiata, nie 9.81
#define JOZZ_RIG_FRICTION 1.0f
#define JOZZ_RIG_DT ( 1.0 / 60.0 )
#define JOZZ_RIG_SUBSTEPS 4
#define JOZZ_RIG_START_X ( -180.0 )

typedef enum
{
	JOZZ_RIG_SPHERE = 0,
	JOZZ_RIG_PRISM_MAX = 1,
	// Pierscien kapsul: obwiednia zlozona z N kapsul o osi rownoleglej do osi
	// kola, rozstawionych po okregu o promieniu (wheelR - crownR). Powstaje
	// przyblizenie torusa. Powod istnienia jest zmierzony, nie estetyczny:
	//   * pryzmat ma OSTRE krawedzie - normalna kontaktu skacze na wierzcholku;
	//   * b3CreateHull nie przyjmuje wiecej niz ~42 scianek (F-01), wiec
	//     "zageszczaj dalej" nie jest droga wyjscia;
	//   * kapsul mozna dac 64 albo 96 i tetnienie promienia dalej spada.
	// Przy TYCH proporcjach kola (R 0.5141, W 0.4375) pierscien kapsul ma przy
	// rownym N WIEKSZE tetnienie niz pryzmat - wygrywa dopiero tam, gdzie
	// pryzmat sie konczy. Liczby: JozzRig_EnvelopeRipple.
	JOZZ_RIG_TORUS = 2,
	JOZZ_RIG_VARIANT_COUNT = 3
} JozzRigVariant;

const char* JozzRig_VariantName( JozzRigVariant v );

// --- konfiguracja rigu -------------------------------------------------------
// JEDNO zrodlo tozsamosci przebiegu. Kazda liczba, ktora wczesniej byla stala
// kompilacji, jest tutaj polem - dzieki temu sweep po N albo po predkosci nie
// wymaga preprocesora (zmierzone: bez tego trzeba bylo obejsc `#define`, zeby
// w ogole wykonac sweep prędkosci).
//
// Podzial pol jest czescia kontraktu, nie kosmetyka:
//
//   GEOMETRIA I MASA  - zmiana wymaga przebudowy ciala
//   SCENA             - zmiana wymaga przebudowy swiata
//   STAN POCZATKOWY   - zmiana wymaga przebudowy, ale NIE jest zmiana konstrukcji.
//                       Rozdzielone swiadomie: bez tego "zmien N i jedz dalej"
//                       jest nieodroznialne od "zacznij od nowa z innym kolem".
//   OBCIAZENIE/REGULATOR - czesc pol zmienia sie na zywo (settery ponizej)
//   INSTRUMENT        - dt i podkroki. Zmiana tych dwoch zmienia NARZEDZIE, nie
//                       badany obiekt, i musi byc oznaczona osobno.
typedef struct
{
	// geometria i masa
	JozzRigVariant variant;
	// Liczba ELEMENTOW obwiedni, nie tylko scianek pryzmatu: dla `prism-Nmax`
	// to liczba scianek, dla `torus-N` liczba kapsul. Jedno pole, bo to jest ta
	// sama zmienna programu - gestosc podzialu obwodu - i chcemy ja porownywac
	// miedzy wariantami przy rownej wartosci.
	int prismSides;
	float wheelR;
	float wheelW;
	float massKg;
	float inertiaSpinFactor;  // iSpin = f * m * R^2 wokol osi kola
	float inertiaTransFactor; // iTr   = f * m * R^2 wokol pozostalych osi
	float density;			  // gestosc shape'u; masa i tak jest zamrazana
	// Promien przekroju korony (`torus-N`). Jednoczesnie promien zaokraglenia
	// barku opony i promien kapsuly. Bieznia PLASKA ma szerokosc wheelW-2*crownR.
	//
	// Wygladalo to na kompromis (mniejsze tetnienie za wezszy plaski styk) i tak
	// bylo tu napisane. ZMIERZONE inaczej (F-25, przemiatanie z okna Q3): na
	// plaskiej plycie przy 4 m/s w zakresie 0.04-0.19 m strata spada MONOTONICZNIE
	// o 43%, churn z 26.9% do zera, a_rms o 34%. Zwezenie biezni z 358 do 58 mm
	// nie kosztuje nic mierzalnego. Kompromis, jesli istnieje, lezy poza Q3 -
	// ten szczebel nie stawia kolu ani sily bocznej, ani pochylenia.
	// Ignorowane przez pozostale warianty.
	float crownR;

	// scena
	float groundHalfX, groundHalfY, groundHalfZ;
	float friction;
	double gravity;

	// stan poczatkowy
	double startX;
	double startSpeed; // predkosc liniowa + toczenie nominalne -v/R
	float startGap;	   // ile nad nominalna wysokoscia srodka startuje cialo

	// obciazenie i regulator
	double loadN;
	int controllerEnabled;
	double targetSpeed;
	double kp, ki, fmax;

	// instrument
	double dt;
	int substeps;
} JozzRigConfig;

// Dokladnie dzisiejszy kontrakt Q2A. Kazda zmiana tej funkcji zmienia
// eksperyment, nie widok - i lamie blokade zachowania.
JozzRigConfig JozzRig_DefaultConfig( void );

// Jednolinijkowy odcisk konfiguracji. Idzie do naglowka trace, zakladki
// obserwacji i manifestu: bez niego liczba nie wie, z czego pochodzi.
void JozzRig_ConfigDigest( const JozzRigConfig* c, char* out, size_t cap );

// --- konstrukcja jako plik ---------------------------------------------------
// Format tekstowy `klucz wartosc`, jedna para na linie, `#` to komentarz.
// Czytelny dla czlowieka, dla gita i dla agenta - to samo wejscie obsluguje
// okno (polka konstrukcji) i stend (--rig-config). Wersja formatu jest polem
// `format`, wiec starszy plik nie zostanie po cichu zinterpretowany po nowemu.
//
// Kontrakt odczytu:
//   * brak klucza  = wartosc domyslna (plik nie musi byc kompletny),
//   * nieznany klucz = BLAD (literowka nie moze uchodzic za brak),
//   * zapis->odczyt->zapis daje identyczny tekst i identyczna strukture bajtowo.
#define JOZZ_RIG_CONFIG_FORMAT 1
#define JOZZ_RIG_CONFIG_TEXT_CAP 2048

// Zwraca liczbe zapisanych bajtow albo 0, gdy bufor jest za maly. `note` (moze
// byc NULL) trafia do komentarza - miejsce na "po co ten przypadek istnieje".
int JozzRig_ConfigToText( const JozzRigConfig* c, char* out, size_t cap, const char* note );

// 1 = wczytano; 0 = odrzucono, opis w `err`. Konfiguracja jest podmieniana
// dopiero po pelnym sukcesie, wiec bledny plik nie zostawia polowicznego stanu.
int JozzRig_ConfigFromText( JozzRigConfig* c, const char* text, char* err, size_t errCap );

int JozzRig_ConfigWriteFile( const JozzRigConfig* c, const char* path, const char* note );
int JozzRig_ConfigReadFile( JozzRigConfig* c, const char* path, char* err, size_t errCap );

// Sprawdza SAM format: kompletnosc tabeli pol, dokladnosc przebiegu tam i z
// powrotem, stabilnosc powtornego zapisu i to, ze zepsuty plik jest ODRZUCANY.
// 1 = w porzadku, 0 = opis w `err`. Wolane przez bramke, nie przez frontend.
int JozzRig_ConfigSelfTest( char* err, size_t errCap );

// Predkosc, powyzej ktorej sztywny wielokat nie utrzymuje ciaglego kontaktu:
// obtoczenie sie wokol wierzcholka wymaga przyspieszenia dosrodkowego v^2/R,
// a dostepne jest load/m. NIEZALEZNA od liczby scianek. Model odniesienia dla
// TEGO rigu (swobodne cialo, sila w srodku masy), nie ogolna granica produktu.
double JozzRig_CriticalSpeed( const JozzRigConfig* c );

// Ile scianek mija punkt kontaktu miedzy dwiema aktualizacjami manifoldu.
// b3Collide wykonuje sie RAZ na b3World_Step (src/physics_world.c), wiec to jest
// realna rozdzielczosc kontaktu, a nie liczba podkrokow.
double JozzRig_FacetsPerStep( const JozzRigConfig* c, double speed );

// --- kinematyka --------------------------------------------------------------
// Os obrotu kola to LOKALNE Y (FreezeMass stawia iSpin na inertia.cy). Pola
// `reference*` licza sie z NOMINALNEGO promienia i punktu odniesienia R pod
// srodkiem masy - to NIE predkosc rzeczywistego punktu kontaktu z manifoldu.
typedef struct
{
	b3Vec3 axleUnit;
	b3Vec3 forward;
	int degenerate;
	double omegaSpin;
	double referenceRimSpeed;
	double referenceSlipSpeed;
	double keTrans;
	double keRot;
} JozzWheelKin;

double JozzRig_Dot3( b3Vec3 a, b3Vec3 b );
b3Vec3 JozzRig_AxleWorld( b3BodyId body );
double JozzRig_OmegaSpin( b3BodyId body );
JozzWheelKin JozzRig_Kinematics( b3BodyId body );
JozzWheelKin JozzRig_KinematicsR( b3BodyId body, float radius );

// --- geometria i masa --------------------------------------------------------
int JozzRig_MakePrismPoints( b3Vec3* out, int cap, int sides, float radius, float halfWidth );

// Najwiekszy pryzmat, ktory `b3CreateHull` jeszcze przyjmuje. Wyznaczany
// pomiarem, nie zapisany na sztywno - i wyznaczany TA SAMA funkcja po obu
// stronach, zeby wariant fasetowany nie mial dwoch roznych N.
int JozzRig_ProbeMaxPrismSides( void );

// Zamrozona masa i bezwladnosc: geometria ma byc jedyna zmienna.
// Wariant bez `Ex` uzywa promienia i czynnikow z domyslnej konfiguracji, zeby
// dotychczasowi wolajacy (sekcje A-G stendu) nie musieli sie zmieniac.
void JozzRig_FreezeMass( b3BodyId body, float kg );
void JozzRig_FreezeMassEx( b3BodyId body, float kg, float radius, float spinFactor, float transFactor );

// Material JAWNY: friction = JOZZ_RIG_FRICTION, rollingResistance = 0. Nie ma
// b3Shape_SetRollingResistance, wiec material musi byc podany przy tworzeniu
// shape'u. Zwraca 0, gdy wariant jest nieprzedstawialny.
int JozzRig_BuildEnvelope( b3BodyId body, JozzRigVariant v, float density, int prismSides );
int JozzRig_BuildEnvelopeEx( b3BodyId body, JozzRigVariant v, float density, int prismSides, float radius,
							 float width, float friction );

// Droga uzywana przez OBA rigi (Q2A i Q3). Bierze cala konfiguracje, bo od
// `torus-N` obwiednia potrzebuje wiecej niz siedem luznych argumentow, a lista
// argumentow, ktora rosnie przy kazdym wariancie, jest zaproszeniem do podania
// ich w zlej kolejnosci. Zwraca 0, gdy wariant jest NIEPRZEDSTAWIALNY.
int JozzRig_BuildEnvelopeFromConfig( b3BodyId body, const JozzRigConfig* c );

// Najmniejsze N, przy ktorym pierscien kapsul jest SZCZELNY (sasiedzi zachodza
// na siebie). Ponizej tej wartosci obwiednia ma dziury i kolo wpada miedzy
// kapsuly - dlatego BuildEnvelope taka konstrukcje odrzuca, zamiast zbudowac
// cos, co wyglada jak kolo i toczy sie jak grzechotka. Zwraca 0 poza `torus-N`.
int JozzRig_MinTorusSegments( const JozzRigConfig* c );

// Tetnienie promienia obwiedni w METRACH: R_nominalne - R_najmniejsze.
// Jedyna liczba porownywalna MIEDZY wariantami wprost, bo nie zalezy od tego,
// czy obwiednia jest z hulla, czy z kapsul. sphere = 0.
double JozzRig_EnvelopeRipple( const JozzRigConfig* c );

// --- rig ---------------------------------------------------------------------

// Odczyt po kroku. Wypelniany przez JozzRig_Step; frontend go tylko formatuje.
typedef struct
{
	int step;
	double time;
	double distance;
	double targetSpeed;
	double speed;
	double error;
	double force;
	double power;
	int saturated;
	double omegaSpin;
	double refRimSpeed;
	double refSlipSpeed;
	double revolutions;
	double posY;
	double velY;
	double keTrans;
	double keRot;
	double keTotal;
	double peGravity;
} JozzRigSample;

typedef struct
{
	b3WorldId world;
	b3BodyId ground;
	b3BodyId body;

	// Z CZEGO to zbudowano. Niezmienne przez cale zycie rigu: przebudowa tworzy
	// nowy rig, a nie modyfikuje ten. `variant` i `prismSides` ponizej sa
	// lustrem cfg, zeby frontendy czytajace je wprost dalej dzialaly.
	JozzRigConfig cfg;

	JozzRigVariant variant;
	int prismSides;

	// stan regulatora - JEDYNE pola startujace z cfg, ale zmienialne na zywo
	int controllerEnabled;
	double targetSpeed;
	double loadN;
	double integral;

	// licznik krokow fizyki od utworzenia
	int step;
	double startX;

	// akumulatory bilansu pracy; zerowane przez JozzRig_ResetWork
	double wDriveSigned, wDrivePos, wDriveNeg, wDriveAbs;
	double wDownforce, wGravity;
	double vyIntegral, pathLength, absSpin;
	int satSteps, workSteps;
	double workStartX;
	b3Pos prevPos;

	// SESJA: kazda ingerencja w fizyke jest lepka. Raz podniesiona flaga nie
	// spada, bo skutek zaburzenia zostaje w stanie ciala na zawsze.
	int perturbed;
	int perturbCount;
	char lastPerturbation[96];
} JozzRig;

// Haki renderera. Silnik potrzebuje ich w definicji swiata, zeby cokolwiek dalo
// sie narysowac - bez nich okno wizualne pokazuje pusty ekran (zmierzone).
//
// Ten typ istnieje po to, zeby frontend graficzny nie mial dostepu do NICZEGO
// innego w b3WorldDef. Gdyby rig przyjmowal cala definicje albo dowolny
// "dekorator", tryb wizualny moglby po cichu zmienic grawitacje, liczbe watkow
// albo pojemnosci - i zdanie "widac to samo" przestaloby byc prawdziwe.
// Wezsze API zamyka te droge na poziomie typu, nie zaufania.
typedef struct
{
	b3CreateDebugShapeCallback* createDebugShape;
	b3DestroyDebugShapeCallback* destroyDebugShape;
	void* userDebugShapeContext;
} JozzRigRenderHooks;

// Tworzy WLASNY swiat identyczny z headless (workerCount = 1, continuous off),
// grunt 400x1x60 pod y=-1 i cialo w stanie nominalnego toczenia przy
// JOZZ_RIG_TARGET_V. Zwraca 0, gdy wariant jest nieprzedstawialny.
int JozzRig_Create( JozzRig* rig, JozzRigVariant v, int prismSides );

// To samo, plus haki renderera. Ze wzgledu na fizyke oba warianty MUSZA dawac
// bit-identyczny przebieg; sprawdza to check_visual_equivalence.py.
int JozzRig_CreateWithRenderHooks( JozzRig* rig, JozzRigVariant v, int prismSides,
								   const JozzRigRenderHooks* hooks );

// Pelna droga: rig budowany z konfiguracji. Dwie powyzsze funkcje sa jej
// nakladkami na domyslnej konfiguracji z podmienionym wariantem i N.
int JozzRig_CreateFromConfig( JozzRig* rig, const JozzRigConfig* cfg, const JozzRigRenderHooks* hooks );
void JozzRig_Destroy( JozzRig* rig );

// DOKLADNIE jeden krok o stalym dt. Kolejnosc operacji jest czescia kontraktu:
// odczyt predkosci -> regulator -> ApplyForceToCenter -> b3World_Step -> odczyt.
void JozzRig_Step( JozzRig* rig, JozzRigSample* out );

// Zeruje akumulatory pracy i ustawia nowy punkt zerowy dystansu. Stend wola to
// na wejsciu do okna pomiarowego.
void JozzRig_ResetWork( JozzRig* rig );

double JozzRig_Distance( const JozzRig* rig );
double JozzRig_Downforce( const JozzRig* rig );

// --- zaburzenia (klasa sesji EXPLORATION) ------------------------------------
// Kazde z nich ustawia `perturbed`. Nie ma cichej sciezki zmiany stanu ciala:
// frontend nie wola b3Body_* na ciele rigu bezposrednio.
// Ingerencja wykonana POZA tym modulem (framework okna: chwyt myszy, wstrzelone
// cialo). Nie dotyka ciala - tylko zapisuje, ze przebieg przestal byc czysty.
void JozzRig_MarkPerturbation( JozzRig* rig, const char* what );

void JozzRig_ApplyImpulse( JozzRig* rig, b3Vec3 impulse, const char* what );
void JozzRig_SetControllerEnabled( JozzRig* rig, int enabled );
void JozzRig_SetTargetSpeed( JozzRig* rig, double v );
void JozzRig_SetLoad( JozzRig* rig, double n );

// --- kontakt (do rysowania) --------------------------------------------------
typedef struct
{
	b3Pos point;
	b3Vec3 normal;
	float normalImpulse;
	float separation;
	int persisted;
} JozzRigContactPoint;

// Surowe punkty manifoldow ciala rigu. Bez klasyfikacji - klasyfikacja
// "nosny punkt" nalezy do eksperymentu, nie do rysowania.
int JozzRig_ContactPoints( const JozzRig* rig, JozzRigContactPoint* out, int cap );

// --- odcisk stanu (test ekwiwalencji) ----------------------------------------
// Jedna funkcja formatujaca dla obu frontendow. Gdyby kazdy mial wlasny format,
// porownanie bajtowe nie bylo by dowodem na nic.
#define JOZZ_RIG_DIGEST_HEADER                                                                                         \
	"step,px,py,pz,qx,qy,qz,qw,vx,vy,vz,wx,wy,wz,integral,w_drive_signed\n"
void JozzRig_DigestLine( const JozzRig* rig, char* out, size_t cap );

#ifdef __cplusplus
}
#endif
