# Stavový automat T-križovatky

Autor: Branislav Krajčovič · vetva `feature/traffic-fsm` · súbory `Core/Inc/traffic_fsm.h`, `Core/Src/traffic_fsm.c`

Automat rozhoduje o režime, svetlách a výsledku príkazov z terminálu. Nepoužíva HAL ani blokujúce čakanie. Aktuálny čas v milisekundách (`HAL_GetTick()`) a vyfiltrované vstupy dostáva ako parametre. Celý sa preto dá otestovať na PC so simulovaným časom (`tests/test_traffic_fsm.c`).

Aplikačná logika je oddelená od hardvéru. Automat nevolá ovládač GPIO priamo. Každú zmenu svetiel oznámi cez **zaregistrovanú callback funkciu**, ktorá dostane stav všetkých siedmich signálov (`TrafficSignals`). V projekte je callbackom `apply_signals` v `main.c`, ktorá stav odovzdá do `TrafficGpio_ApplyOutputs`. Vstupy číta a filtruje modul `traffic_inputs` (Miroslav). Automat dostane hotovú hodnotu snímača na C a jednorazovú udalosť stlačenia tlačidla.

Všetky funkcie automatu sa volajú z hlavnej slučky, nie z obsluhy prerušenia. Stav automatu preto nepotrebuje kvalifikátor `volatile`.

## Hlavná myšlienka: stav pre každý signál a konfliktná matica

Pôvodný automat prepínal celé fázy. Pri každej zmene zhaslo všetko a nasledovala spoločná červená. Nový automat má **vlastný stav pre každý zo siedmich signálov** (červená, zelená, žltá) a čas jeho poslednej zmeny. O bezpečnosti rozhoduje konfliktná matica:

| | `A` | `A_S` | `B_R` | `B_D` | `C` | `C_S` | `P` |
| --- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`A`** | | – | | ✕ | ✕ | ✕ | ✕ |
| **`A_S`** | – | | | ✕ | | | ✕ |
| **`B_R`** | | | | | ✕ | | |
| **`B_D`** | ✕ | ✕ | | | ✕ | | ✕ |
| **`C`** | ✕ | | ✕ | ✕ | | – | ✕ |
| **`C_S`** | ✕ | | | | – | | ✕ |
| **`P`** | ✕ | ✕ | | ✕ | ✕ | ✕ | |

✕ znamená, že dráhy sa podľa obr. 1 v README križujú alebo zlievajú do jedného pruhu:

- `A` (rovno aj dole) križuje `B_D` a `C`, zlieva sa s `C_S` a odbočenie dole prechádza cez priechod.
- `A_S` (z A dole) ide do rovnakého výjazdového pruhu ako `B_D` a cez priechod.
- `B_R` (rovno doľava) sa zlieva s odbočením z C doľava.
- `C` (doľava aj doprava) križuje alebo sa zlieva s `A`, `B_R`, `B_D` a prechádza cez priechod.
- `C_S` (doprava) sa zlieva s `A` a prechádza cez priechod.

Znak – spája šípku s jej bežným semaforom. Ide o ten istý pohyb, takže nesvietia súčasne, ale nepotrebujú medzi sebou ochranný čas.

**Pravidlo zapnutia signálu:** signál sa zapne, keď on sám aj všetky signály z jeho riadku v matici svietia načerveno aspoň `TRAFFIC_ALL_RED_MS` (1 s) a jeho dvojica (`A`/`A_S`, `C`/`C_S`) svieti načerveno. Bezpečnosť teda nezávisí od toho, ako sú zostavené fázy alebo príkazy, ale priamo od matice.

**Pravidlo ukončenia signálu:** bežný cestný semafor (`A`, `B_R`, `B_D`, `C`) prejde na 2 s na žltú a potom na červenú. Šípka zhasne a `P` prejde na červenú hneď.

## Rozhranie

| Funkcia | Použitie |
| --- | --- |
| `TrafficFsm_RegisterOutputCallback(cb)` | Funkcia volaná pri každej zmene svetiel. Registrovať pred `TrafficFsm_Init`. `NULL` = bez callbacku. |
| `TrafficFsm_Init(now)` | Spoločná červená (päť červených, šípky zhasnuté), režim Demo, po 1 s fáza F1. Vymaže požiadavky C aj chodcov. |
| `TrafficFsm_Update(now, car_c, ped_pressed)` | V každom prechode hlavnej slučky. Vyhodnotí časovanie a vstupy, pri zmene svetiel zavolá callback. |
| `TrafficFsm_RequestMode(mode)` | Príkaz `MODE DEMO` / `MODE CONTROL`. |
| `TrafficFsm_RequestSignal(target, color)` | Príkaz `SET <cieľ> <farba>`. Ciele `A`, `A_S`, `B_R`, `B_D`, `C`, `C_S`, `P` a `B` (oba B). |
| `TrafficFsm_GetStatus(&status)` | Údaje pre `STATUS`: režim, `NEXT`, `READY`, farby, šípky, `CAR_C`, `C_REQ`, `P_REQ`. |

Požiadavky používajú čas posledného volania `TrafficFsm_Update`. Rozdiel je najviac jeden prechod slučky.

| Konštanta | Hodnota | Význam |
| --- | ---: | --- |
| `TRAFFIC_GREEN_MS` | 5000 | trvanie fázy v Demo od chvíle, keď svietia všetky jej signály |
| `TRAFFIC_YELLOW_MS` | 2000 | cestná žltá |
| `TRAFFIC_ALL_RED_MS` | 1000 | ochranný čas pred zapnutím signálu |
| `TRAFFIC_CAR_WAIT_MS` | 5000 | čas státia auta pred snímačom na C, kým sa zapíše `C_REQ` |

Výsledok požiadavky (`TrafficResult`) zodpovedá odpovediam protokolu:

| `TrafficResult` | Odpoveď |
| --- | --- |
| `TRAFFIC_RESULT_OK` | `OK` |
| `TRAFFIC_RESULT_ERR_BUSY` | `ERR BUSY` |
| `TRAFFIC_RESULT_ERR_MODE` | `ERR MODE` |
| `TRAFFIC_RESULT_ERR_STATE` | `ERR STATE` |
| `TRAFFIC_RESULT_ERR_CONFLICT` | `ERR CONFLICT` |
| `TRAFFIC_RESULT_ERR_PARAM` | neplatný parameter, modul USART ho zachytí skôr ako `ERR SYNTAX` |

Použitie v hlavnej slučke (`main.c`):

```c
TrafficGpio_Init();
TrafficInputs_Init(HAL_GetTick());
TrafficFsm_RegisterOutputCallback(apply_signals);   /* pred spustením automatu */
TrafficFsm_Init(HAL_GetTick());                     /* oznámi spoločnú červenú */
while (1)
{
    const uint32_t now_ms = HAL_GetTick();
    TrafficInputs_Update(now_ms);
    TrafficFsm_Update(now_ms, TrafficInputs_IsCarPresent(), TrafficInputs_TakePedestrianPress());
}
```

### Napojenie modulu USART

Modul `traffic_serial` (Samuel) volá automat cez `TrafficSerialHandlers`. Enumy `SerialTarget` a `TrafficTarget` majú rovnaké poradie. `SerialColor`, `SerialMode` a `SerialResult` zodpovedajú `TrafficColor`, `TrafficMode` a `TrafficResult`. Pri spájaní vetiev stačí do `main.c` doplniť tri krátke funkcie:

```c
static SerialResult fsm_request_mode(SerialMode mode)
{
  return (SerialResult)TrafficFsm_RequestMode((TrafficMode)mode);
}

static SerialResult fsm_request_signal(SerialTarget target, SerialColor color)
{
  return (SerialResult)TrafficFsm_RequestSignal((TrafficTarget)target, (TrafficColor)color);
}

static void fsm_get_status(SerialStatus *out)
{
  TrafficFsmStatus st;
  TrafficFsm_GetStatus(&st);
  out->mode = (SerialMode)st.mode;
  out->next = (SerialMode)st.next_mode;
  out->ready = st.ready;
  out->a = (SerialColor)st.signals.a;
  out->b_r = (SerialColor)st.signals.b_r;
  out->b_d = (SerialColor)st.signals.b_d;
  out->c = (SerialColor)st.signals.c;
  out->p = (SerialColor)st.signals.p;
  out->a_s = st.signals.a_s_on;
  out->c_s = st.signals.c_s_on;
  out->car_c = st.car_c;
  out->p_req = st.p_req;
}

static const TrafficSerialHandlers serial_handlers =
{
  fsm_request_mode, fsm_request_signal, fsm_get_status
};
```

`TrafficFsmStatus` obsahuje aj `c_req`. Ak sa so Samuelom dohodneme na poli `C_REQ` vo výpise `STATUS`, stačí ho doplniť do `SerialStatus` a do tohto prevodu.

## Stav automatu

| Premenná | Hodnoty | Význam |
| --- | --- | --- |
| `mode` | `DEMO`, `CONTROL` | aktívny režim |
| `next_mode` | `NONE`, `DEMO`, `CONTROL` | cieľový režim počas prepínania |
| `sig[7]` | `RED`, `GREEN`, `YELLOW` | stav každého signálu |
| `since[7]` | čas | posledná zmena signálu, od nej sa meria žltá a ochranný čas |
| `phase` | `F1` – `F4` | aktuálna fáza v Demo |
| `target` | maska signálov | signály fázy, ktoré majú svietiť |
| `phase_on`, `phase_start_ms` | 0/1, čas | všetky signály fázy svietia, odvtedy beží 5 s |
| `before_ped` | `F1`, `F3` | fáza pred chodcami, určuje, čo nasleduje po F4 |
| `car_waiting`, `car_since_ms` | 0/1, čas | auto stojí pred snímačom pri nezelenej `C` |
| `c_req` | 0/1 | auto stálo 5 s, čaká fáza F2 |
| `p_req` | 0/1 | čakajúca požiadavka chodcov |

Rozdiel časov typu `uint32_t` funguje aj pri pretečení milisekundového času.

## Režim Demo

### Fázy

| Fáza | `A` | `A_S` | `B_R` | `B_D` | `C` | `C_S` | `P` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| F1 – Hlavná cesta | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| F2 – Vjazd C (oba smery) | 0 | 1 | 0 | 0 | 1 | 0 | 0 |
| F3 – B a šípka z C doprava | 0 | 0 | 1 | 1 | 0 | 1 | 0 |
| F4 – Chodci a B rovno | 0 | 0 | 1 | 0 | 0 | 0 | 1 |

Každá fáza je maximálna kombinácia z matice, takže sa do nej nedá pridať ďalší signál bez konfliktu. Piata maximálna kombinácia `A_S` + `B_R` + `C_S` sa v Demo nepoužíva, ale v Control sa dá zostaviť.

### Zmena fázy

1. Signály, ktoré sú aj v novej fáze, svietia ďalej.
2. Ostatné zelené signály sa ukončia (žltá alebo hneď).
3. V každom kroku sa zapnú tie signály novej fázy, ktoré už podľa matice smú.
4. Keď svietia všetky signály fázy, začne sa merať 5 s.

### Tabuľka prechodov medzi fázami

| Fáza po 5 s | Podmienka | Ďalšia fáza |
| --- | --- | --- |
| F1 | čaká chodec | F4 (zapamätá `before_ped = F1`) |
| F1 | `C_REQ=1` | F2 |
| F1 | inak | F3 |
| F4 | `before_ped = F1` a `C_REQ=1` | F2 |
| F4 | `before_ped = F1` | F3 |
| F4 | `before_ped = F3` | F1 |
| F2 | vždy | F3 |
| F3 | čaká chodec | F4 (zapamätá `before_ped = F3`) |
| F3 | inak | F1 |

Po štarte aj po návrate do Demo začne automat fázou F1.

### Časová os (výstup testov)

Voľný vjazd C. `B_R` svieti nepretržite od 1. sekundy:

```
 1 s  A, B_R zelená                    F1
 6 s  A žltá, B_R pokračuje
 8 s  A červená
 9 s  B_D, C_S zapnú                    F3
14 s  B_D žltá, C_S zhasne, B_R pokračuje
17 s  A zelená                         F1 (cyklus 16 s)
```

Auto stojí na C od začiatku:

```
 1 s  A, B_R zelená                    F1
 5 s  C_REQ=1 (auto stojí 5 s)
 6 s  A, B_R žltá
 8 s  A, B_R červená, A_S zapne (pokračuje odbočenie z A)
 9 s  C zelená                           F2
14 s  C žltá, A_S zhasne
16 s  C červená, C_S zapne (pokračuje odbočenie z C doprava)
17 s  B_R, B_D zelená                  F3
22 s  B_D žltá, C_S zhasne, B_R pokračuje
25 s  A zelená                         F1 (cyklus 24 s)
```

Porovnanie s pôvodnou tabuľkou fáz:

| | Pôvodne | Teraz |
| --- | --- | --- |
| `B_R` pri voľnom C | 10 s zelenej zo 16 s, dve zastavenia | nepretržite zelená |
| Odbočenie z C doprava pri aute na C | 5 s z 24 s | 11 s z 24 s (5 s plná zelená + 6 s šípka) |
| `B_R` počas chodcov | červená | zelená |
| Auto, ktoré snímačom len prejde | vyvolá fázu C | nevyvolá nič |
| Odbočenie z C doprava pri voľnom C | nepovolené | šípka v každej F3 |

### Požiadavky

- **Chodci:** stlačenie nastaví `p_req`. Stlačenie počas zelenej pre P patrí do prebiehajúcej fázy. Požiadavka sa vymaže pri zapnutí zelenej pre P a pri inicializácii.
- **Vjazd C:** keď je snímač obsadený a `C` nesvieti nazeleno, beží časovač. Po 5 s nepretržitého státia sa zapíše `c_req`. Uvoľnenie snímača časovač vynuluje. Ak sa snímač uvoľní počas svietiacej šípky `C_S`, auto odbočilo doprava a `c_req` sa zruší. Pri červenej auto odísť nemôže, preto sa požiadavka nezruší. Vymaže sa pri zapnutí zelenej pre `C`.

## Režim Control

| Príkaz | Podmienka | Výsledok |
| --- | --- | --- |
| ľubovoľný `SET` | prebieha prepínanie | `ERR BUSY` |
| ľubovoľný `SET` | režim Demo | `ERR MODE` |
| `SET <cieľ> GREEN` | všetky signály cieľa už svietia | `OK`, bez zmeny |
| `SET <cieľ> GREEN` | každý nový signál cieľa smie podľa matice naskočiť | `OK`, zapne ich |
| `SET <cieľ> GREEN` | inak (konflikt so svietiacim alebo žltým signálom, ochranný čas) | `ERR CONFLICT` |
| `SET <cieľ> YELLOW` | cieľ obsahuje zelený bežný semafor | `OK`, ukončí zelené signály cieľa |
| `SET <cieľ> YELLOW` | inak (aj šípky a P) | `ERR STATE` |
| `SET <cieľ> RED` | | `OK`, ukončí zelené signály cieľa, inak bez zmeny |

Ostatné signály svietia ďalej. Fázu sa dá zostaviť postupne, napríklad `SET B_D GREEN`, `SET C_S GREEN`, `SET B GREEN` dá F3. `READY=1` platí v ustálenom Control, keď sú všetky signály na červenej aspoň 1 s.

## Prepnutie režimu

| Situácia | Výsledok |
| --- | --- |
| požadovaný režim je aktívny a neprebieha prepínanie | `OK`, potvrdenie |
| prebieha prepínanie na ten istý cieľ | `OK`, časovanie pokračuje |
| prebieha prepínanie a príde požiadavka na opúšťaný režim | `ERR BUSY` |
| nový cieľ | `OK`, všetky zelené signály sa ukončia. Cieľ sa aktivuje, keď sú všetky signály na červenej aspoň 1 s. Ak už sú, hneď. |

Po aktivácii Control sú všetky semafory na červenej a automat čaká na príkaz. Po aktivácii Demo začne fáza F1. Požiadavky `c_req` a `p_req` sa prenesú.

## Overenie

Testy na PC: `cd tests && make`. Testy hlásia **65 245 kontrol, 0 chýb**. Priebeh svetiel je v `tests/vysledky_F1_F3.txt`.

Test obsahuje vlastnú kópiu konfliktnej matice, zapísanú nezávisle od automatu. Pri každej zmene svetiel kontroluje:

- žiadne dva konfliktné signály nesvietia súčasne (zelená ani žltá),
- šípka `A_S` / `C_S` svieti iba pri červenej na `A` / `C`,
- signál sa zapne, až keď on sám aj všetky konfliktné signály svietia načerveno aspoň 1 s,
- žltá trvá celé 2 s, `P` nemá žltú a bežný semafor nezhasne zo zelenej bez žltej.

| Skúška | Čo test overuje |
| --- | --- |
| **F1 – Obsluha vjazdu C** | Voľný vjazd: F1 → F3 → F1, `B_R` nezelená iba prvú sekundu po štarte. Auto stojí: celá časová os F1 → F2 → F3 → F1 vrátane `A_S` a `C_S` hneď po žltej. Auto len prejde (2 s): bez F2. Hranica 5 s: 4,999 s nestačí. Auto po zápise požiadavky odbočí po šípke: požiadavka sa zruší. Auto ostane stáť: F2 po najbližšej F1. |
| **F2 – Šípky, kolízie a chodci** | Chodec počas F1 aj F3, `B_R` počas chodcov nezastaví. Chodec a auto: F1 → F4 → F2 → F3. Stlačenie počas zelenej P sa ignoruje. Control: `A_S` aj `P` pri `B_D` → `ERR CONFLICT`, `C_S` k `B_D` → `OK`, ručne zostavená F3 aj kombinácia `A_S` + `B_R` + `C_S`, ochranný čas presne 1 s, kroky spoločnej skúšky č. 3, `C_S` pri zelenej `C` → `ERR CONFLICT`, `A` pri chodcoch → `ERR CONFLICT`. |
| **F3 – Prepnutie režimu** | Počas F3 tlačidlo a `MODE CONTROL`: `C_S` zhasne, oba B na žltú, spoločná červená, potom `MODE=CONTROL READY=1 P_REQ=1`. `ERR BUSY` počas prepínania. `SET P GREEN`, potom `MODE DEMO`: P rovno na červenú, po 1 s F1. Okamžité prepnutie. Prenos `C_REQ` a `P_REQ` do Demo. |
| Ďalšie | Pretečenie `uint32_t` času, beh bez callbacku a dvojhodinová náhodná simulácia so vstupmi aj príkazmi. |

Testy sme overili aj opačne: v automate sme schválne vytvorili 8 chýb (chýbajúci konflikt v matici, šípka pri zelenej, bez ochranného času, A pri chodcoch, nezrušená požiadavka C, snímač 3 s, bez prenosu zelenej). Testy zachytili každú z nich.

## Poznámky k spoločným súborom

- `traffic_phase.h` a `TrafficGpio_Apply(TrafficPhase)` patria k pôvodnému návrhu s jednou fázou. Automat ich nepoužíva. `traffic_fsm.h` zatiaľ `traffic_phase.h` includuje, lebo ho cez neho potrebuje `traffic_gpio.h`. Po odstránení starého rozhrania z ovládača sa môže zmazať.
- `TrafficSignals` (automat) a `TrafficGpioOutputs` (ovládač) majú rovnaké položky. `main.c` ich prevádza v `apply_signals`. Ak sa dohodneme na jednom spoločnom type, prevod odpadne.
