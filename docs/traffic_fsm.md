# Stavový automat T-križovatky

Autor: Branislav Krajčovič · vetva `feature/traffic-fsm` · súbory `Core/Inc/traffic_fsm.h`, `Core/Src/traffic_fsm.c`

Automat rozhoduje o režime, fáze svetiel a výsledku požiadaviek z terminálu. Nepoužíva HAL ani blokujúce čakanie. Aktuálny čas v milisekundách (`HAL_GetTick()`) dostáva ako parameter, preto sa dá otestovať aj na PC so simulovaným časom. Výstupom je jedna fáza typu `TrafficPhase`.

Podľa prednášky *Prerušenia – abstrakcia od HW* je aplikačná logika oddelená od hardvérovej vrstvy. Automat nevolá ovládač GPIO priamo. Zmenu fázy oznamuje cez **zaregistrovanú callback funkciu**, v projekte `TrafficGpio_Apply`. Ovládač GPIO sa dá vymeniť alebo nahradiť simulovaným bez zmeny automatu, čo využívajú testy na PC. Callback sa registruje pred spustením automatu a pred jeho volaním sa kontroluje, či nie je `NULL`.

Všetky funkcie automatu sa volajú z hlavnej slučky, nie z obsluhy prerušenia (modul USART v prerušení len prijíma znaky a celé riadky spracúva v slučke). Stav automatu preto nie je zdieľaný s prerušením a nepotrebuje kvalifikátor `volatile`.

## Rozhranie

| Funkcia | Použitie |
| --- | --- |
| `TrafficFsm_RegisterPhaseCallback(callback)` | Registrácia funkcie volanej pri každej zmene fázy (napr. `TrafficGpio_Apply`). Volať pred `TrafficFsm_Init`. |
| `TrafficFsm_Init(now)` | Po inicializácii spoločná červená, režim Demo začne zelenou pre A po 1 s. |
| `TrafficFsm_Update(now)` | Volá sa v každom prechode hlavnej slučky. Vyhodnotí časovanie, pri zmene fázy zavolá callback a vráti aktuálnu fázu. |
| `TrafficFsm_RequestMode(mode, now)` | Príkaz `MODE DEMO` / `MODE CONTROL`. |
| `TrafficFsm_RequestColor(light, color, now)` | Príkaz `SET <semafor> <farba>`. |
| `TrafficFsm_GetStatus(now, &status)` | Údaje pre `STATUS`: aktívny režim, cieľový režim (`NEXT`), `READY`, fáza. |
| `TrafficFsm_PhaseColor(phase, light)` | Farba semafora v danej fáze (napr. pre výpis farieb z fázy aplikovanej na GPIO). |

Výsledok požiadavky (`TrafficResult`) zodpovedá odpovediam protokolu:

| `TrafficResult` | Odpoveď |
| --- | --- |
| `TRAFFIC_RESULT_OK` | `OK` |
| `TRAFFIC_RESULT_ERR_BUSY` | `ERR BUSY` |
| `TRAFFIC_RESULT_ERR_MODE` | `ERR MODE` |
| `TRAFFIC_RESULT_ERR_STATE` | `ERR STATE` |
| `TRAFFIC_RESULT_ERR_CONFLICT` | `ERR CONFLICT` |
| `TRAFFIC_RESULT_ERR_PARAM` | neplatný parameter, modul USART ho zachytí skôr ako `ERR SYNTAX` |

Použitie v hlavnej slučke:

```c
TrafficGpio_Init();
TrafficFsm_RegisterPhaseCallback(TrafficGpio_Apply);   /* pred spustením automatu */
TrafficFsm_Init(HAL_GetTick());                        /* oznámi spoločnú červenú */
while (1)
{
    TrafficFsm_Update(HAL_GetTick());                  /* zmenu fázy oznámi callback */
    /* modul USART: príkazy -> TrafficFsm_RequestMode / TrafficFsm_RequestColor */
}
```

## Stav automatu

| Premenná | Hodnoty | Význam |
| --- | --- | --- |
| `active_mode` | `DEMO`, `CONTROL` | aktívny režim |
| `next_mode` | `NONE`, `DEMO`, `CONTROL` | cieľový režim počas prepínania |
| `phase` | `ALL_RED`, `A/B/C_GREEN`, `A/B/C_YELLOW`, `P_GREEN` | aktuálna fáza (výstup pre GPIO) |
| `phase_start_ms` | čas | vstup do fázy, od neho sa meria trvanie |
| `demo_step` | 0 – 10 | krok cyklu Demo |

Trvanie fázy sa meria od skutočného vstupu do fázy. Žltá aj spoločná červená preto dostanú celý interval, aj keď predchádzajúca fáza skončí s oneskorením. Rozdiel časov typu `uint32_t` funguje aj pri pretečení milisekundového času.

Časovanie: zelená v Demo **5 s**, žltá **2 s**, spoločná červená **1 s** (`TRAFFIC_GREEN_MS`, `TRAFFIC_YELLOW_MS`, `TRAFFIC_ALL_RED_MS`).

## Tabuľka prechodov – časové (funkcia `TrafficFsm_Update`)

| Podmienka | Aktuálna fáza | Uplynulý čas | Nová fáza / akcia |
| --- | --- | --- | --- |
| Demo, bez prepínania | krok *k* cyklu | ≥ trvanie kroku *k* | krok *k+1* (po kroku 11 krok 1) |
| Control, bez prepínania | `X_YELLOW` | ≥ 2 s | `ALL_RED` |
| Control, bez prepínania | `X_GREEN`, `P_GREEN`, `ALL_RED` | – | bez zmeny (čaká na príkaz) |
| prebieha prepínanie | `X_YELLOW` | ≥ 2 s | `ALL_RED` |
| prebieha prepínanie | `ALL_RED` | ≥ 1 s | aktivácia cieľového režimu: Demo → `A_GREEN` (krok 1), Control → zostáva `ALL_RED` |

Po inicializácii: režim Demo, fáza `ALL_RED`, krok 11, takže po 1 s nasleduje krok 1 (`A_GREEN`).

Cyklus Demo (30 s):

| Krok | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Fáza | A_GREEN | A_YELLOW | ALL_RED | B_GREEN | B_YELLOW | ALL_RED | C_GREEN | C_YELLOW | ALL_RED | P_GREEN | ALL_RED |
| Trvanie | 5 s | 2 s | 1 s | 5 s | 2 s | 1 s | 5 s | 2 s | 1 s | 5 s | 1 s |

## Tabuľka prechodov – požiadavky

### `MODE <režim>` (funkcia `TrafficFsm_RequestMode`)

| Stav | Výsledok | Akcia |
| --- | --- | --- |
| prepínanie, cieľ = požadovaný režim | `OK` | prechod pokračuje podľa pôvodného časovania |
| prepínanie, požadovaný je opúšťaný režim | `ERR BUSY` | – |
| požadovaný režim je už aktívny | `OK` | len potvrdenie |
| iný režim, fáza `X_GREEN` (cestná) | `OK` | `next_mode` = cieľ, prechod na `X_YELLOW` |
| iný režim, fáza `P_GREEN` | `OK` | `next_mode` = cieľ, prechod na `ALL_RED` |
| iný režim, fáza `X_YELLOW` | `OK` | `next_mode` = cieľ, žltá dobehne |
| iný režim, `ALL_RED` kratšie ako 1 s | `OK` | `next_mode` = cieľ, interval dobehne |
| iný režim, `ALL_RED` aspoň 1 s | `OK` | cieľový režim sa aktivuje ihneď |

### `SET <semafor> <farba>` (funkcia `TrafficFsm_RequestColor`)

Kontroly sa vyhodnocujú v tomto poradí:

| Stav | Výsledok |
| --- | --- |
| prebieha prepínanie režimu | `ERR BUSY` |
| aktívny režim Demo | `ERR MODE` |

V aktívnom režime Control:

| Príkaz | Aktuálna fáza | Výsledok | Akcia |
| --- | --- | --- | --- |
| `SET X GREEN` | `X_GREEN` (tento semafor už svieti nazeleno) | `OK` | bez zmeny |
| `SET X GREEN` | iná zelená, akákoľvek žltá, `ALL_RED` kratšie ako 1 s | `ERR CONFLICT` | bez zmeny |
| `SET X GREEN` | `ALL_RED` aspoň 1 s (`READY=1`) | `OK` | `X_GREEN` |
| `SET X YELLOW` (X = A, B, C) | `X_GREEN` | `OK` | `X_YELLOW`, po 2 s `ALL_RED` |
| `SET X YELLOW` (X = A, B, C) | iná fáza (X nemá zelenú) | `ERR STATE` | bez zmeny |
| `SET P YELLOW` | ľubovoľná | `ERR STATE` | bez zmeny |
| `SET X RED` (X = A, B, C) | `X_GREEN` | `OK` | `X_YELLOW`, po 2 s `ALL_RED` |
| `SET P RED` | `P_GREEN` | `OK` | `ALL_RED` (začína interval spoločnej červenej) |
| `SET X RED` | `X_YELLOW` | `OK` | žltá dobehne |
| `SET X RED` | X svieti načerveno | `OK` | potvrdenie stavu |

Pri odmietnutí príkazu sa stav nemení a pokračuje naplánované časovanie aktuálnej fázy.

`READY = 1` platí, keď je aktívny Control, neprebieha prepínanie a spoločná červená trvá aspoň 1 s.

## Skúšky F1 – F3

Testy sú v `tests/test_traffic_fsm.c`. Automat dostáva simulovaný čas, takže sa dajú spustiť na PC bez dosky:

```
cd tests
make
```

(alebo `gcc -std=c99 -Wall -I../Core/Inc test_traffic_fsm.c ../Core/Src/traffic_fsm.c -o test_traffic_fsm`)

Každý krok simulácie navyše kontroluje bezpečnostné pravidlo, že najviac jeden semafor nesvieti načerveno. Úplný výpis je v `tests/vysledky_F1_F3.txt`.

| Skúška | Overené | Výsledok |
| --- | --- | --- |
| **Callback** | Bez registrácie (`NULL`) automat beží a nič nevolá. Po registrácii Init oznámi spoločnú červenú, každá zmena fázy (časová aj vyvolaná príkazom) zavolá callback práve raz a simulovaný ovládač GPIO zobrazuje vždy aktuálnu fázu. | prešla |
| **F1 – Časovanie Demo** | 1 s štartovacej červenej, dva celé cykly A → B → C → P, presné časy všetkých zmien podľa tabuľky (žiadna fáza nezačne skôr). Pri volaní `Update` s oneskorením (krok 7 ms) trvá žltá celé 2 s a spoločná červená celú 1 s. | prešla |
| **F2 – Kolízie** | Pri zelenej A: `SET B GREEN` aj `SET P GREEN` → `ERR CONFLICT`. Pri zelenej P: `SET C GREEN` → `ERR CONFLICT`. Zamietnutie počas žltej aj počas spoločnej červenej, prijatie po uplynutí intervalu. `SET P YELLOW` a žltá pre červený semafor → `ERR STATE`, `SET` v Demo → `ERR MODE`. | prešla |
| **F3 – Prepnutie režimu** | `MODE CONTROL` počas zelenej A: celá žltá 2 s a spoločná červená 1 s, potom Control čaká (`READY=1`). Počas prepínania `SET P GREEN` a návrat do Demo → `ERR BUSY`, opakovaný cieľ → `OK`. `MODE DEMO` počas zelenej P: spoločná červená 1 s, Demo začne fázou A. Prepnutie po uplynutí spoločnej červenej prebehne ihneď. Pretečenie času `uint32_t`. | prešla |

Výsledok: **131 815 kontrol, 0 chýb**.

Na hotovom modeli sa skúšky zopakujú s ovládačom GPIO a modulom USART (časovanie F1 stopkami, F2 a F3 príkazmi z terminálu).
