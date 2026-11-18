# Stavový automat T-križovatky

Autor: Branislav Krajčovič · vetva `feature/traffic-fsm` · súbory `Core/Inc/traffic_fsm.h`, `Core/Src/traffic_fsm.c`

Automat rozhoduje o režime, fáze svetiel a výsledku príkazov z terminálu. Nepoužíva HAL ani blokujúce čakanie. Aktuálny čas v milisekundách (`HAL_GetTick()`) a vyfiltrované vstupy dostáva ako parametre. Celý sa preto dá otestovať na PC so simulovaným časom (`tests/test_traffic_fsm.c`).

Aplikačná logika je oddelená od hardvéru. Automat nevolá ovládač GPIO priamo. Každú zmenu svetiel oznámi cez **zaregistrovanú callback funkciu**, ktorá dostane stav všetkých siedmich signálov (`TrafficSignals`). V projekte je callbackom `apply_signals` v `main.c`, ktorá stav odovzdá do `TrafficGpio_ApplyOutputs`. Vstupy číta a filtruje modul `traffic_inputs` (Miroslav). Automat dostane už hotové hodnoty `CAR_C` a jednorazovú udalosť stlačenia tlačidla.

Všetky funkcie automatu sa volajú z hlavnej slučky, nie z obsluhy prerušenia. Stav automatu preto nepotrebuje kvalifikátor `volatile`.

## Rozhranie

| Funkcia | Použitie |
| --- | --- |
| `TrafficFsm_RegisterOutputCallback(cb)` | Funkcia volaná pri každej zmene svetiel. Registrovať pred `TrafficFsm_Init`. `NULL` = bez callbacku. |
| `TrafficFsm_Init(now)` | Spoločná červená (päť červených, šípky zhasnuté), režim Demo. Vymaže čakajúcu požiadavku chodcov. |
| `TrafficFsm_Update(now, car_c, ped_pressed)` | V každom prechode hlavnej slučky. Vyhodnotí časovanie a vstupy, pri zmene svetiel zavolá callback. |
| `TrafficFsm_RequestMode(mode)` | Príkaz `MODE DEMO` / `MODE CONTROL`. |
| `TrafficFsm_RequestSignal(target, color)` | Príkaz `SET <cieľ> <farba>`. Ciele `A`, `A_S`, `B_R`, `B_D`, `C`, `C_S`, `P` a `B` (oba B). |
| `TrafficFsm_GetStatus(&status)` | Údaje pre `STATUS`: režim, `NEXT`, `READY`, farby, šípky, `CAR_C`, `P_REQ`. |

Požiadavky používajú čas posledného volania `TrafficFsm_Update`. Rozdiel je najviac jeden prechod slučky.

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

`TRAFFIC_RESULT_ERR_PARAM` sa do protokolu nedostane, lebo parser odmietne neplatný cieľ, farbu aj režim skôr ako `ERR SYNTAX`.

## Fázy

Fáza je množina signálov, ktoré svietia nazeleno (bežné semafory) alebo svietia (šípky). Ostatné bežné semafory sú na červenej a ostatné šípky zhasnuté. Vnútorne je fáza uložená ako bitová maska.

| Fáza | `A` | `A_S` | `B_R` | `B_D` | `C` | `C_S` | `P` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `PHASE_A` | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| `PHASE_C_LEFT` | 0 | 1 | 0 | 0 | 1 | 0 | 0 |
| `PHASE_B_WITH_CS` | 0 | 0 | 1 | 1 | 0 | 1 | 0 |
| `PHASE_B_FREE` | 0 | 0 | 1 | 1 | 0 | 0 | 0 |
| `PHASE_PED` | 0 | 0 | 0 | 0 | 0 | 0 | 1 |

V Control zapína príkaz `SET <cieľ> GREEN` práve signály daného cieľa, teda jeden semafor, jednu šípku, P alebo oba B.

## Stav automatu

| Premenná | Hodnoty | Význam |
| --- | --- | --- |
| `mode` | `DEMO`, `CONTROL` | aktívny režim |
| `next_mode` | `NONE`, `DEMO`, `CONTROL` | cieľový režim počas prepínania |
| `stage` | `ALL_RED`, `GREEN`, `YELLOW` | etapa fázy |
| `green_mask` | maska signálov | zelené signály v etape `GREEN` |
| `yellow_mask` | maska signálov | bežné semafory so žltou v etape `YELLOW` |
| `stage_start_ms` | čas | vstup do etapy, od neho sa meria trvanie |
| `demo_next` | `A`, `DECIDE`, `C_LEFT`, `B_WITH_CS`, `B_FREE` | nasledujúca cestná fáza v Demo |
| `ped_may_follow` | 0/1 | posledná ukončená fáza Demo bola cestná, takže môžu nasledovať chodci |
| `p_req` | 0/1 | čakajúca požiadavka chodcov |
| `car_c` | 0/1 | obsadenosť vjazdu C z posledného `Update` |

Trvanie etapy sa meria od skutočného vstupu do nej. Žltá aj spoločná červená preto dostanú celý interval, aj keď sa predchádzajúca etapa skončí s oneskorením. Rozdiel časov typu `uint32_t` funguje aj pri pretečení milisekundového času.

Časovanie: zelená v Demo **5 s** (cestná aj chodci), žltá **2 s**, spoločná červená najmenej **1 s** (`TRAFFIC_GREEN_MS`, `TRAFFIC_YELLOW_MS`, `TRAFFIC_ALL_RED_MS`).

## Tabuľka prechodov

### Etapy (spoločné pre Demo aj Control)

| Z etapy | Podmienka | Do etapy | Poznámka |
| --- | --- | --- | --- |
| `GREEN` | ukončenie fázy, fáza má zelený bežný semafor | `YELLOW` | šípky zhasnú hneď, zelené bežné semafory prejdú na žltú |
| `GREEN` | ukončenie fázy, iba šípka alebo P | `ALL_RED` | bez žltej |
| `YELLOW` | uplynuli 2 s | `ALL_RED` | |
| `ALL_RED` | uplynula 1 s a čaká prepnutie režimu | `ALL_RED` | aktivuje sa cieľový režim |
| `ALL_RED` | uplynula 1 s, Demo | `GREEN` | ďalšia fáza podľa `demo_next` a `p_req` |
| `ALL_RED` | uplynula 1 s, Control, `SET … GREEN` | `GREEN` | iba pri `READY=1` |

Fázu ukončí: v Demo uplynutie 5 s zelenej, v Control príkaz `SET … YELLOW/RED` a v oboch režimoch požiadavka na zmenu režimu.

### Výber fázy v Demo

| `demo_next` pri konci ochrannej červenej | `p_req` a `ped_may_follow` | Zapne | Nový `demo_next` |
| --- | --- | --- | --- |
| ľubovoľný | oba 1 | `PHASE_PED` | nezmení sa (naplánovaná fáza príde po chodcoch) |
| `A` | inak | `PHASE_A` | `DECIDE` |
| `DECIDE` → podľa `CAR_C` | | | `C_LEFT` pri `CAR_C=1`, `B_FREE` pri `CAR_C=0`, potom podľa riadkov nižšie |
| `C_LEFT` | inak | `PHASE_C_LEFT` | `B_WITH_CS` (aj keď sa vjazd medzitým uvoľní) |
| `B_WITH_CS` | inak | `PHASE_B_WITH_CS` | `A` |
| `B_FREE` | inak | `PHASE_B_FREE` | `A` |

Obsadenosť C sa vyhodnotí raz, na konci ochrannej sekundy po fáze A. Ak čakajú chodci, rozhodnutie platí aj pre fázu, ktorá príde po nich. Po štarte aj po návrate do Demo začne automat fázou A. Čakajúca požiadavka chodcov sa vybaví až po nej.

Stlačenie tlačidla nastaví `p_req`. Stlačenie počas zelenej pre P patrí do prebiehajúcej fázy a ignoruje sa. Požiadavka sa vymaže pri zapnutí zelenej pre P (v Demo automaticky, v Control príkazom `SET P GREEN`) a pri inicializácii.

### Príkazy v Control

| Príkaz | Podmienka | Výsledok |
| --- | --- | --- |
| ľubovoľný `SET` | prebieha prepínanie | `ERR BUSY` |
| ľubovoľný `SET` | režim Demo | `ERR MODE` |
| `SET <cieľ> GREEN` | všetky signály cieľa už svietia | `OK`, bez zmeny |
| `SET <cieľ> GREEN` | `READY=1` | `OK`, zapne signály cieľa |
| `SET <cieľ> GREEN` | iná aktívna fáza, žltá alebo ochranná červená | `ERR CONFLICT` |
| `SET <cieľ> YELLOW` | cieľ obsahuje práve zelený bežný semafor | `OK`, ukončí fázu |
| `SET <cieľ> YELLOW` | inak (aj šípky a P) | `ERR STATE` |
| `SET <cieľ> RED` | cieľ obsahuje práve zelený signál | `OK`, ukončí celú fázu |
| `SET <cieľ> RED` | inak | `OK`, bez zmeny časovania |

`READY=1` platí v ustálenom Control pri spoločnej červenej, ktorá trvá aspoň 1 s.

### Prepnutie režimu

| Situácia | Výsledok |
| --- | --- |
| požadovaný režim je aktívny a neprebieha prepínanie | `OK`, potvrdenie |
| prebieha prepínanie na ten istý cieľ | `OK`, časovanie pokračuje |
| prebieha prepínanie a príde požiadavka na opúšťaný režim | `ERR BUSY` |
| nový cieľ | `OK`, aktívna zelená sa ukončí podľa pravidiel, cieľ sa aktivuje po ochrannej sekunde. Ak už uplynula, hneď. |

Po aktivácii Control sú všetky semafory na červenej a automat čaká na príkaz. Po aktivácii Demo hneď začne fáza A.

## Overenie

Testy na PC: `cd tests && make`. Testy hlásia **10 858 kontrol, 0 chýb**. Priebeh svetiel je v `tests/vysledky_F1_F3.txt`.

Pri každej zmene svetiel testy kontrolujú bezpečnostné pravidlá:

- povolené sú iba kombinácie zelených z tabuľky fáz alebo jednotlivé voľby v Control,
- `A_S` svieti iba pri červenej na A a `C_S` iba pri červenej na C,
- `A_S` nikdy nesvieti súčasne so zelenou `B_D`,
- pri zelenej pre P je všetko ostatné na červenej a P nemá žltú,
- žltá trvá celé 2 s a po nej nasleduje spoločná červená,
- každej novej zelenej predchádza aspoň 1 s spoločnej červenej.

| Skúška | Čo test overuje |
| --- | --- |
| **F1 – Obsluha vjazdu C** | Voľný vjazd A → B → A (cyklus 16 s). Obsadený vjazd A → C doľava → B s `C_S` → A. Auto odíde počas C doľava a fáza s `C_S` aj tak prebehne, v ďalšom cykle už B bez `C_S`. Auto príde počas B a vyhodnotí sa až po ďalšej fáze A. |
| **F2 – Šípky, kolízie a chodci** | Tlačidlo počas zelenej A: P až po žltej a spoločnej červenej, potom naplánovaná fáza. Stlačenie počas zelenej P sa ignoruje. V Control `SET A_S GREEN` pri zelenej `B_D` → `ERR CONFLICT`. Nová zelená počas žltej a ochrannej červenej → `ERR CONFLICT`, po 1 s → `OK`. `SET A_S YELLOW` → `ERR STATE`. Kroky spoločnej skúšky č. 3. |
| **F3 – Prepnutie režimu** | Počas B s `C_S` tlačidlo a `MODE CONTROL`: `C_S` zhasne, oba B na žltú, spoločná červená, potom `MODE=CONTROL READY=1 P_REQ=1`. `SET` počas prepínania → `ERR BUSY`, návrat do Demo → `ERR BUSY`. `SET P GREEN`, potom `MODE DEMO`: P rovno na červenú, po 1 s fáza A. Okamžité prepnutie pri uplynutej ochrannej sekunde. |
| Ďalšie | Pretečenie `uint32_t` času, beh bez callbacku a dvojhodinová náhodná simulácia so vstupmi aj príkazmi, pri ktorej sa kontrolujú pravidlá vyššie. |

## Poznámky k spoločným súborom

- `traffic_phase.h` a `TrafficGpio_Apply(TrafficPhase)` patria k pôvodnému návrhu s jednou fázou. Automat ich už nepoužíva. `traffic_fsm.h` zatiaľ `traffic_phase.h` includuje, lebo ho cez neho potrebuje `traffic_gpio.h`. Po odstránení starého rozhrania z ovládača sa môže zmazať.
- `TrafficSignals` (automat) a `TrafficGpioOutputs` (ovládač) majú rovnaké položky. `main.c` ich prevádza v `apply_signals`. Ak sa dohodneme na jednom spoločnom type, prevod odpadne.
