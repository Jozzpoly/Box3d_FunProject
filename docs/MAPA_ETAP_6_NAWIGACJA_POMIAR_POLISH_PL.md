# Mapa — Etap 6: nawigacja od centrum, telemetria i finalny sign-off

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Status: **ZABLOKOWANY do zamknięcia Etapów 2R–5.**

## 1. Cel

Spinać mapę jako jeden system: centralny kampus, satelity kaflowe i offroad.
Nawigacja ma wynikać z relacji do centrum, telemetria ma nazywać nawierzchnię
i wynik próby, a finalne rendery mają wykrywać utratę fokusu — błąd, którego
poprzednia walidacja Etapu 2 nie wykryła.

## 2. Nawigacja

### 2.1 Rejestr kotwic

Każda kotwica ma:

- stabilne ID i polską nazwę;
- tile/strefę;
- pozycję, yaw i bezpieczną wysokość;
- kierunek „do atrakcji”;
- exclusion check po zbudowaniu świata.

Minimalny rejestr:

- Centrum — rdzeń;
- Centrum — Komfort N;
- Centrum — Artykulacja W;
- Centrum — Teren punktowy E;
- Centrum — Impact S;
- Drift W;
- Tor — start N;
- Tor — hairpin NE;
- Duże lądowania SE;
- Plac fizyki SW;
- Stress yard S;
- Offroad — brama E;
- Offroad — góra;
- Offroad — głęboko.

### 2.2 UI

- combo grupowane: Centrum / Tor i drift / Fizyka i stress / Offroad;
- przycisk Teleportuj;
- opis kierunku i zastosowania kotwicy;
- hotkeye dopiero po audycie `HOTKEY_AUDIT_PL.md`;
- teleport resetuje bieżący pomiar stanowiska/lap, ale nie usuwa bestów, jeśli
  dany moduł jawnie je zachowuje.

### 2.3 Wayfinding w świecie

- cztery bramy centralnego kafla N/W/E/S;
- w satelitach małe znaczniki powrotu „CENTRUM”;
- kolor identyfikuje rodzinę strefy tylko jako akcent;
- etykiety z distance cullingiem, bez ściany tekstu widocznej z rdzenia;
- brak pełnej minimapy w tym tracku.

## 3. Telemetria

### Nawierzchnia

`userMaterialId` jest stałym kontraktem, nie lokalną liczbą modułu:

| ID | Nawierzchnia |
|---:|---|
| 0 | nieznana |
| 1 | grid/płyta |
| 2 | offroad |
| 3 | obstacle techniczny |
| 4 | tor |
| 5 | drift/lód |
| 6 | platforma ruchoma |
| 7 | most |

HUD pokazuje materiał większości kół i stan mieszany na szwie.

### Wynik próby

- airtime i dystans skoku;
- maksymalna kompresja 0.5 s po lądowaniu;
- min/max travel na stanowisku;
- utrata kontaktu per koło;
- czas przejazdu stanowiska, jeśli ma sensory;
- lap/splity na torze;
- aktywny station ID, żeby wynik miał kontekst.

Telemetria korzysta z istniejących kontaktów i danych rigu. Nie tworzy
równoległego modelu fizyki.

## 4. Język wizualny finalny

| Rodzina | Baza | Akcent |
|---|---|---|
| Centralny kampus | proceduralny techniczny grid | biały/cyjan + mały green/amber/red difficulty |
| Tor | neutralna ciemniejsza nawierzchnia | niebieskie bramy, czerwono-białe krawężniki |
| Drift/lód | neutralna płyta | lodowy błękit |
| Plac fizyki | stal/szarość | fiolet na bramie/aktywnym stanowisku |
| Stress | neutralny tile | pomarańczowe ostrzeżenia |
| Offroad | kolor materiału/heightfielda | brązowe tyczki |

Pełne nasycone kolory nie mogą przykrywać geometrii kontaktu ani gridu.
Paleta jest zatwierdzana na renderach, nie w tabeli.

## 5. Finalna suita porównawcza

Wszystkie polecenia, env i kamery zapisujemy obok listy dowodów.

1. `mapa_final_plate_top` — cała płyta 3×3 + brama offroadu;
2. `mapa_final_center_top` — dokładnie ten sam kadr co Etap 2R;
3. `mapa_final_center_driver` — z rdzenia ku trzem wejściom;
4. `mapa_final_track`;
5. `mapa_final_drift`;
6. `mapa_final_landings`;
7. `mapa_final_physics`;
8. `mapa_final_stress_clean` i `stress_loaded`;
9. `mapa_final_offroad_seam`;
10. `mapa_final_offroad_mountain`.

Do sign-offu trafia też zestaw trzech kadrów środka:

- stan przed pierwszym Etapem 2;
- odrzucony `b8afab9`;
- finalna Mapa 2.0.

## 6. Bramka końcowa

### Techniczna

- pełny gate;
- przejście po wszystkich kotwicach: poprawna pozycja/yaw, brak overlapu;
- materiały raportowane poprawnie na szwach i platformach;
- lap timer, station timer, airtime i landing compression mają sanity values;
- reset świata czyści dynamiczne batch'e i odtwarza stałe stanowiska;
- brak wzrostu body/shape po cyklu teleport/reset/regenerate;
- dokumentacja README/INDEX/TECH_DEBT/CHECKPOINTS zsynchronizowana.

### Produktowa

- centrum jest natychmiast rozpoznawalne jako serce mapy;
- z centrum da się wybrać kierunek bez teleportu i bez zgadywania;
- satelity wyglądają jak część jednej mapy;
- żadna późniejsza strefa nie zdegradowała czytelności centralnego gridu;
- Jozz przejeżdża wybrany ciąg:
  Centrum → test podstrefy → tor/drift → plac fizyki → offroad → powrót;
- finalna akceptacja Jozza zamyka status **Mapa 2.0**.

## 7. Dokumentacja zamknięcia

W tym samym commicie:

- `README_FOR_AGENTS.md`: nowy zwalidowany stan i reguły środka;
- `CURRENT_STATE_INDEX_PL.md`: kamień milowy Mapa 2.0;
- `TECH_DEBT_PL.md`: jawne ograniczenia i odłożone rzeczy;
- `CHECKPOINTS_PL.md`: oddzielnie wynik techniczny i sign-off produktowy;
- stare dokumenty Etapu 2 nie są kopiowane do nowych „raportów”; historia
  pozostaje w git.

## 8. Ryzyka

- telemetria może stać się przeładowanym HUD-em — domyślnie jedna linia
  kontekstowa, szczegóły w rozwijanym panelu;
- teleporty mogą ukryć zły wayfinding — sign-off zawiera pełny przejazd bez
  teleportowania;
- polish może stać się pretekstem do nowej strefy — nowe strefy są poza
  zakresem;
- zielone testy mogą ponownie przykryć słaby layout — produktowa bramka jest
  równorzędna i wymaga Jozza.
