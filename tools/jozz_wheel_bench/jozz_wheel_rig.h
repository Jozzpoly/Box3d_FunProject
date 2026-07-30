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
	JOZZ_RIG_VARIANT_COUNT = 2
} JozzRigVariant;

const char* JozzRig_VariantName( JozzRigVariant v );

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

// --- geometria i masa --------------------------------------------------------
int JozzRig_MakePrismPoints( b3Vec3* out, int cap, int sides, float radius, float halfWidth );

// Najwiekszy pryzmat, ktory `b3CreateHull` jeszcze przyjmuje. Wyznaczany
// pomiarem, nie zapisany na sztywno - i wyznaczany TA SAMA funkcja po obu
// stronach, zeby wariant fasetowany nie mial dwoch roznych N.
int JozzRig_ProbeMaxPrismSides( void );

// Zamrozona masa i bezwladnosc: geometria ma byc jedyna zmienna.
void JozzRig_FreezeMass( b3BodyId body, float kg );

// Material JAWNY: friction = JOZZ_RIG_FRICTION, rollingResistance = 0. Nie ma
// b3Shape_SetRollingResistance, wiec material musi byc podany przy tworzeniu
// shape'u. Zwraca 0, gdy wariant jest nieprzedstawialny.
int JozzRig_BuildEnvelope( b3BodyId body, JozzRigVariant v, float density, int prismSides );

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
	JozzRigVariant variant;
	int prismSides;

	// stan regulatora
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
