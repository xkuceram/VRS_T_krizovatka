# Model T-križovatky so svetelnou signalizáciou

Tímový projekt z predmetu VRS <sub>· traffic-fsm · model-gpio · usart-control</sub>

**Slovenčina** · [English](EN_README.md)

Značka pri odseku odkazuje na vetvu v [rozdelení práce](#rozdelenie-práce) a určuje, kto text upravuje a udržiava. Pri spoločnej časti sa uvedení správcovia dohodnú na zmene pred odoslaním pull requestu. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Cieľ a povinný výsledok

Zostavíme model T-križovatky so štyrmi cestnými semaformi, semaforom pre chodcov a dvoma zelenými šípkami. Svetlá pripojíme priamo na 12 výstupov dosky STM32 NUCLEO-F303K8. Na vjazde C bude jeden spoločný pruh pre odbočenie doľava aj doprava a jeden IR snímač pri stopčiare. Chodci si zelenú vyžiadajú tlačidlom. <sub>· traffic-fsm · model-gpio</sub>

Na obhajobe ukážeme cyklus Demo pri voľnom aj obsadenom vjazde C. Predvedieme oba súbehy pri odbočovaní z C a odmeriame trvanie žltej a spoločnej červenej. V režime Control vyskúšame ovládanie z terminálu a odmietnutie kolízneho príkazu. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Obsah

<sub>traffic-fsm · model-gpio · usart-control</sub>

- [Rozsah projektu](#rozsah-projektu)
- [Prehľad systému](#prehľad-systému)
- [Komunikačný protokol](#komunikačný-protokol)
- [Potrebný hardvér](#potrebný-hardvér)
  - [Schéma zapojenia](#schéma-zapojenia)
  - [Zapojenie LED](#zapojenie-led)
- [Orientačný rozpočet](#orientačný-rozpočet)
- [Rozdelenie práce](#rozdelenie-práce)
- [Overenie funkčnosti](#overenie-funkčnosti)
- [Vzdelávacie ciele](#vzdelávacie-ciele)
- [Postup práce v repozitári](#postup-práce-v-repozitári)

## Rozsah projektu

Hlavná cesta vedie v pôdoryse zľava doprava. Na vjazde B sprava je pruh na jazdu rovno a pruh na odbočenie dole. Dolná vetva C má jeden spoločný vjazdový pruh pre obe odbočenia a vedľa neho samostatný výjazdový pruh. Do výjazdového pruhu odbočujú vozidlá z A a B. Pri stopčiare C snímame obsadenosť vjazdu. <sub>· traffic-fsm · model-gpio</sub>

<sub>traffic-fsm · model-gpio</sub>

| Označenie | Príjazd | Čo povoľuje zelená | Farby |
| --- | --- | --- | --- |
| `A` | Zľava | Pokračovať po hlavnej doprava alebo odbočiť dole | Červená, žltá, zelená |
| `A_S` | Zľava | Odbočiť dole pri červenej na `A` | Zelená šípka |
| `B_R` | Sprava | Pokračovať po hlavnej rovno doľava | Červená, žltá, zelená |
| `B_D` | Sprava | Odbočiť dole na vedľajšiu | Červená, žltá, zelená |
| `C` | Spoločný vjazd zdola | Odbočiť doľava z C do A | Červená, žltá, zelená |
| `C_S` | Spoločný vjazd zdola | Odbočiť doprava z C do B pri červenej na `C` | Zelená šípka |
| `P` | Priechod cez dolnú vetvu | Prejsť cez priechod | Červená, zelená |

![Pôdorys T-križovatky s jedným vjazdovým pruhom na C a IR snímačom pri stopčiare](docs/images/t-krizovatka.svg)

*Obr. 1 – Dráhy odbočenia, spoločný vjazd C, samostatný výjazd a umiestnenie IR snímača pri stopčiare.* <sub>· model-gpio</sub>

### Pravidlá signalizácie

- Po inicializácii sa rozsvieti päť červených svetiel. Obe šípky sú zhasnuté. Prvá zelená môže začať po jednej sekunde spoločnej červenej. <sub>· traffic-fsm · model-gpio</sub>
- Na každom bežnom semafore svieti v ustálenom stave práve jedna farba. Šípka `A_S` môže svietiť iba pri červenej na `A`. Pravé odbočenie z C je povolené pri rozsvietenej šípke `C_S` a červenej na `C`. Pri zelenej pre chodcov sú všetky štyri cestné semafory na červenej a obe šípky zhasnuté. <sub>· traffic-fsm</sub>
- Automat povolí iba cestné kombinácie uvedené pri Demo a jednotlivé voľby z tabuľky príkazov v Control. Napríklad `A_S` a `B_D` by viedli do toho istého výjazdového pruhu, preto sa zapínajú v rôznych fázach. <sub>· traffic-fsm · usart-control</sub>
- Pri ukončení fázy šípky zhasnú a bežné cestné semafory, ktoré mali zelenú, prejdú naraz na žltú na 2 s. Pri ukončení fázy so samotnou šípkou alebo chodcami sa prejde rovno na spoločnú červenú. Pred každou ďalšou zelenou musí spoločná červená trvať aspoň 1 s. <sub>· traffic-fsm</sub>

### Tlačidlo pre chodcov a snímanie na C

Tlačidlo dáme vedľa priechodu P. Zákmity kontaktu odfiltrujeme, aby jedno stlačenie vytvorilo jednu požiadavku aj pri podržaní. <sub>· model-gpio</sub>

Automat vybaví požiadavku zapnutím zelenej pre P. Stlačenie počas zelenej pre P patrí do prebiehajúcej fázy. Reset čakajúcu požiadavku vymaže. <sub>· traffic-fsm</sub>

Jeden IR snímač pri stopčiare na C sleduje obsadenosť spoločného vjazdu. Jeho dosah overíme so stojacim autom aj pri prejazde v susednom výjazdovom pruhu. <sub>· model-gpio</sub>

### Režim Demo

Pri zelenej na `A` môže vozidlo odbočiť z A na C cez priechod. Chodci preto dostávajú zelenú vo vlastnej fáze, keď sú cestné semafory na červenej a obe šípky zhasnuté. <sub>· traffic-fsm</sub>

V tabuľke znamená **1** zelenú alebo svietiacu šípku. **0** znamená červenú alebo zhasnutú šípku. Žltá patrí do prechodu medzi fázami. <sub>· traffic-fsm</sub>

<sub>traffic-fsm</sub>

| Fáza | `A` | `A_S` | `B_R` | `B_D` | `C` | `C_S` | `P` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| C doľava | 0 | 1 | 0 | 0 | 1 | 0 | 0 |
| B s odbočením z C doprava | 0 | 0 | 1 | 1 | 0 | 1 | 0 |
| B pri voľnom vjazde C | 0 | 0 | 1 | 1 | 0 | 0 | 0 |
| Chodci | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| Spoločná červená | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

Režim Demo začne fázou A po spoločnej červenej. Po ukončení fázy A a ochrannej sekunde vyhodnotíme obsadenosť vjazdu C. V automate ju označíme ako `CAR_C`. <sub>· traffic-fsm</sub>

- Pri `CAR_C=0` nasleduje B pri voľnom vjazde C a potom opäť A. <sub>· traffic-fsm</sub>
- Pri `CAR_C=1` nasleduje C doľava, potom B s odbočením z C doprava a nakoniec opäť A. Jeden snímač určuje obsadenosť vjazdu, preto v tomto cykle vyhradíme čas pre obe odbočenia. Ak sa vjazd uvoľní počas prvej fázy C, druhá sa aj tak vykoná. Novú obsadenosť vyhodnotíme po ďalšej fáze A. <sub>· traffic-fsm</sub>

Pri C doľava sa vozidlo z C zaradí do pruhu smerom na A. Šípka `A_S` súčasne pustí vozidlo z A do samostatného výjazdového pruhu na dolnej vetve. Pri B s odbočením z C doprava môže vozidlo z `B_R` ísť rovno doľava, vozidlo z `B_D` do výjazdového pruhu C a vozidlo z `C_S` doprava po hlavnej. Súbehy overíme podľa dráh na obr. 1 aj s modelmi áut. <sub>· traffic-fsm · model-gpio</sub>

Pre model navrhujeme 5 s zelenej v cestnej fáze, 2 s žltej a aspoň 1 s spoločnej červenej. Keď chodec čaká, po ukončení cestnej fázy a ochrannej sekunde zaradíme P. Chodci majú zelenú 5 s, po nej nasleduje spoločná červená a predtým naplánovaná cestná fáza. <sub>· traffic-fsm</sub>

Automat meria čas od vstupu do fázy podľa systémového času v milisekundách. Hlavná slučka priebežne číta vstupy a spracúva komunikáciu. Žltá a spoločná červená dostanú celý interval aj pri oneskorenej zmene predchádzajúcej fázy. <sub>· traffic-fsm</sub>

### Režim Control a zmena režimu

Po prechode do Control sú všetky semafory na červenej a obe šípky sú zhasnuté. Obsluha môže vybrať jeden semafor, jednu šípku alebo oba semafory B spolu. Zelená trvá do platného príkazu na ukončenie alebo na zmenu režimu. Nový povel na zelenú sa prijme pri `READY=1`. Pri ochrannej červenej automat vráti `ERR CONFLICT` a obsluha môže príkaz zopakovať po skončení intervalu. <sub>· traffic-fsm · usart-control</sub>

V Control ďalej čítame snímač aj tlačidlo. Príkaz `STATUS` ukáže obsadenosť vjazdu C a požiadavku chodcov. Na odbočenie z C doľava použijeme `SET C GREEN`, na odbočenie doprava `SET C_S GREEN`. Požiadavka chodcov sa vybaví pri zapnutí zelenej príkazom `SET P GREEN`. <sub>· traffic-fsm · usart-control</sub>

Pri zmene režimu ukončíme aktívnu fázu podľa rovnakých pravidiel. Rozsvietená cestná žltá dobehne, šípky zhasnú a P prejde na červenú. Cieľový režim sa aktivuje po skončení spoločnej červenej. Ak ochranná sekunda už uplynula, režim sa prepne hneď. Čakajúcu požiadavku chodcov si automat prenesie do nového režimu. Po návrate do Demo začne od fázy A. <sub>· traffic-fsm</sub>

## Prehľad systému

<sub>traffic-fsm · model-gpio · usart-control</sub>

```mermaid
flowchart TD
    PC["Sériový terminál"] <-->|"USART2 cez ST-LINK"| USART["Modul USART"]
    USART <-->|"Príkazy a stav"| FSM["Stavový automat"]
    IN["Tlačidlo P a IR snímač C"] --> GPIO
    TICK["Systémový čas v ms"] --> FSM
    FSM <-->|"Fáza a stav vstupov"| GPIO["Ovládač GPIO"]
    GPIO -->|"12 výstupov GPIO"| LED["Päť RGB LED a dve šípky"]
```

- **Stavový automat** vyberá fázy podľa obsadenosti C a požiadavky chodcov, meria intervaly a overuje príkazy z terminálu. Ovládaču GPIO odovzdá požadovanú fázu. <sub>· traffic-fsm</sub>
- **Ovládač GPIO** číta tlačidlo a IR snímač. Podľa fázy nastaví červený a zelený kanál každého RGB semafora a stav oboch šípok. <sub>· model-gpio</sub>
- **Modul USART** skladá prijaté znaky do riadkov, odovzdáva požiadavky automatu a odosiela odpovede do terminálu. <sub>· usart-control</sub>

## Komunikačný protokol

Počítač sa pripojí cez virtuálny sériový port ST-LINK. Pre USART2 aj terminál nastavíme **9600 baudov**, 8 dátových bitov, žiadnu paritu, 1 stop bit a vypnuté riadenie toku. ST-LINK na doske používa PA2 pre TX a PA15 pre RX. <sub>· usart-control</sub> [STMicroelectronics UM1956, sekcia 7.9](https://www.st.com/resource/en/user_manual/um1956-stm32-nucleo32-boards-mb1180-stmicroelectronics.pdf)

Protokol navrhujeme s textovými príkazmi v ASCII a veľkými písmenami. Jedna medzera oddeľuje susedné položky. Parameter `<cieľ>` je `A`, `A_S`, `B_R`, `B_D`, `C`, `C_S` alebo `P`. Cieľ `B` zahŕňa oba semafory na vjazde sprava. Prijatý príkaz má najviac 32 znakov pred `LF`, `CRLF` alebo samotným `CR`, ktorý niektoré terminály posielajú pri stlačení Enter. Prázdne riadky sa preskočia. Na každý neprázdny riadok zariadenie odpovie jedným riadkom s koncom `CRLF`. Terminál odošle ďalší príkaz po prijatí odpovede. <sub>· usart-control</sub>

<sub>traffic-fsm · usart-control</sub>

| Príkaz | Správanie |
| --- | --- |
| `MODE DEMO` | Vyžiada prechod do Demo. V ustálenom režime Demo potvrdí aktuálny stav. |
| `MODE CONTROL` | Vyžiada prechod do Control. V ustálenom režime Control potvrdí aktuálny stav. |
| `SET <cieľ> GREEN` | Ak už všetky požadované signály svietia nazeleno, potvrdí ich stav. Pri `READY=1` spustí novú zelenú. Pri `A_S` a `C_S` rozsvieti šípku pri červenej na príslušnom semafore, pri `B` zapne obe zelené sprava. Pri inej aktívnej fáze, žltej alebo ochrannej červenej vráti `ERR CONFLICT`. |
| `SET <cieľ> YELLOW` | Ukončí aktuálnu zelenú, ak cieľ zahŕňa práve zelený bežný cestný semafor. Na 2 s sa zapne žltá na bežných semaforoch, ktoré v tejto fáze mali zelenú. |
| `SET <cieľ> RED` | Ak cieľ zahŕňa práve zelený signál, ukončí celú aktívnu fázu. Cestná zelená dobehne cez žltú. Šípka zhasne a P prejde priamo na červenú. Pri žltej, červenej alebo zhasnutej šípke príkaz potvrdí aktuálny stav bez zmeny časovania. |
| `STATUS` | Vráti režim, stav prepínania, pripravenosť, farby semaforov, stav šípok, obsadenosť vjazdu C a čakajúcu požiadavku chodcov. |

Príkazy `SET` sa vykonávajú v režime Control po skončení prípadného prepínania. Počas prepínania je dostupný `STATUS`. Opakovanie príkazu na ten istý cieľový režim dostane `OK` a časovanie pokračuje. <sub>· traffic-fsm · usart-control</sub>

<sub>traffic-fsm · usart-control</sub>

| Odpoveď | Význam |
| --- | --- |
| `OK` | Príkaz bol prijatý. Priebeh časovaného prechodu ukáže `STATUS`. |
| `ERR SYNTAX` | Neznámy príkaz, cieľ, režim, farba alebo chybný formát riadka. |
| `ERR LENGTH` | Riadok prekročil 32 znakov. Prijímač zahodí zvyšok po koniec riadka a potom prijíma ďalší príkaz. |
| `ERR BUSY` | Počas prepínania prišiel `SET` alebo požiadavka na návrat do práve opúšťaného režimu. |
| `ERR MODE` | Používateľ poslal `SET` v režime Demo. |
| `ERR STATE` | Príkaz na žltú sa netýka žiadneho práve zeleného bežného cestného semafora. |
| `ERR CONFLICT` | Novú zelenú sme vyžiadali pri aktívnej fáze, žltej alebo počas ochrannej červenej. |

Príklad odpovede na `STATUS` v Control pri aute pred stopčiarou C a čakajúcom chodcovi <sub>· usart-control</sub>

```text
OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=1 P_REQ=1
```

`NEXT` obsahuje cieľový režim pri prepínaní alebo `-` mimo neho. `READY=1` znamená, že Control môže prijať novú zelenú. Šípky sa vypisujú ako `ON` alebo `OFF`. `CAR_C=1` znamená obsadený vjazd a `CAR_C=0` voľný vjazd. `P_REQ=1` označuje čakajúcu požiadavku chodcov. Po zapnutí zelenej pre P má `P_REQ` hodnotu `0`. <sub>· traffic-fsm · usart-control</sub>

## Potrebný hardvér

<sub>model-gpio</sub>

| Položka | Počet | Použitie |
| --- | ---: | --- |
| STM32 NUCLEO-F303K8 | 1 | Riadenie, programovanie a sériová komunikácia cez ST-LINK |
| Difúzna RGB LED 5 mm so spoločnou katódou | 5 | `A`, `B_R`, `B_D`, `C` a `P` |
| Zelená LED 5 mm | 2 | Šípky `A_S` a `C_S` |
| Rezistor 330 Ω, ¼ W | 12 | Po jednom pre každý červený a zelený kanál RGB LED a každú zelenú šípku |
| Spínacie tlačidlo (NO) | 1 | Požiadavka pri priechode P |
| Reflexný IR snímač s digitálnym výstupom a napájaním 3,3 V | 1 | Detekcia obsadenosti vjazdu pri stopčiare C |
| Kontaktné pole, 830 bodov | 1 | Osadenie dosky a skúšobné zapojenie |
| Prepojovacie vodiče M–M | 1 balenie (40 ks) | Zapojenie na kontaktnom poli |
| Počítač a dátový USB kábel s koncovkou Micro-B | 1 súprava | Napájanie z USB, nahratie programu a terminál cez ST-LINK |

### Schéma zapojenia

<sub>model-gpio</sub>

![Schéma zapojenia v KiCade](docs/schematic/schematic.svg)

Označenie `TSSP58038` a značka tlačidla v KiCade sú orientačné. Na vjazde C použijeme dostupný reflexný IR modul s digitálnym výstupom kompatibilným s 3,3 V. Zapojíme ho podľa schémy cez napájanie 3,3 V, GND a výstup `OUT`. Na vybranom module si overíme označenie vývodov aj logickú úroveň pri detekcii auta. <sub>· model-gpio</sub>

Pri priechode P použijeme dostupné spínacie tlačidlo (NO). Zapojíme jeho dva spínacie kontakty tak, aby pri stlačení spojili vstup so zemou. <sub>· model-gpio</sub>

### Zapojenie LED

Spoločnú katódu každej RGB LED pripojíme na GND. Anódu červeného a zeleného kanála pripojíme na samostatné výstupy GPIO, každú cez vlastný rezistor. Modrý vývod necháme nezapojený. Na cestnom semafore vznikne žltá súčasným rozsvietením červeného a zeleného kanála. Pre chodcov použijeme iba červenú a zelenú. <sub>· model-gpio</sub>

<sub>model-gpio</sub>

| Farba na semafore | Červený kanál R | Zelený kanál G |
| --- | ---: | ---: |
| Červená | 1 | 0 |
| Žltá (iba cestné semafory) | 1 | 1 |
| Zelená | 0 | 1 |

Použijeme 12 rovnakých sériových rezistorov po 330 Ω. Najprv na jednej RGB LED pripojenej na 3,3 V výstupy dosky skontrolujeme jas červenej a zelenej aj odtieň žltej pri súčasnom rozsvietení oboch kanálov. Zelený kanál má vyššie priepustné napätie, preto pri rovnakom odpore môže svietiť slabšie. <sub>· model-gpio</sub> [Parametre RGB LED](https://techfun.sk/produkt/led-rgb-5-mm/) · [Datasheet STM32F303x6/x8, charakteristiky GPIO](https://www.st.com/resource/en/datasheet/stm32f303k8.pdf)

<sub>model-gpio</sub>

| Svetlo | Potlač na doske | Pin STM32 | Sériový rezistor |
| --- | --- | --- | ---: |
| `A` R | A0 | PA0 | 330 Ω |
| `A` G | A1 | PA1 | 330 Ω |
| `B_R` R | A2 | PA3 | 330 Ω |
| `B_R` G | A3 | PA4 | 330 Ω |
| `B_D` R | A6 | PA7 | 330 Ω |
| `B_D` G | D9 | PA8 | 330 Ω |
| `C` R | D3 | PB0 | 330 Ω |
| `C` G | D6 | PB1 | 330 Ω |
| `P` R | D11 | PB5 | 330 Ω |
| `P` G | D12 | PB4 | 330 Ω |
| `A_S` zelená | D10 | PA11 | 330 Ω |
| `C_S` zelená | D13 | PB3 | 330 Ω |

Pri štandardnom zapojení dosky sa spolu s `C_S` rozsvieti aj používateľská LED LD3, pretože je pripojená na D13. <sub>· model-gpio</sub>

V CubeMX nastavíme 12 výstupov ako `GPIO_Output`, `Push Pull`, `Low Speed` a bez interného pull-up ani pull-down rezistora. Počiatočná úroveň piatich červených výstupov (PA0, PA3, PA7, PB0, PB5) bude `High`. Zelené výstupy a šípky začnú na `Low`. Pri ladení cez `Serial Wire` zostávajú PB3 a PB4 k dispozícii ako GPIO. <sub>· model-gpio</sub>

Potlač na doske, prepojenie LD3 s D13 a typ USB kábla vychádzajú z dokumentácie NUCLEO-F303K8. <sub>· model-gpio</sub> [STMicroelectronics UM1956, sekcie 3.1 a 7.5, tabuľka 12](https://www.st.com/resource/en/user_manual/um1956-stm32-nucleo32-boards-mb1180-stmicroelectronics.pdf)

## Orientačný rozpočet

Rozpočet počíta s jednou objednávkou z Techfunu. Ceny sú vrátane DPH a pochádzajú z ponúk overených 16. 11. 2026. Pri položkách s výberom variantu overíme skladovosť zvoleného typu pri objednávaní. Snímač a tlačidlo v tabuľke sú príklady kompatibilných súčiastok. Dosku NUCLEO, počítač a dátový USB kábel berieme ako vybavenie na zostavenie a skúšanie. <sub>· model-gpio</sub>

<sub>model-gpio</sub>

| Položka | Množstvo | Cena spolu |
| --- | ---: | ---: |
| [RGB LED 5 mm, difúzna, spoločná katóda](https://techfun.sk/produkt/led-rgb-5-mm/) | 5 ks | 0,75 € |
| [LED 5 mm, zelený variant](https://techfun.sk/produkt/led-ka-1-kus-5mm-rozne-farby/) | 2 ks | 0,06 € |
| [Rezistory 330 Ω, ¼ W](https://techfun.sk/produkt/rezistor-rozne-hodnoty-1-4w-10-kusov/) | 1 balenie (20 ks) | 0,25 € |
| [Spínacie tlačidlo 12 × 12 × 4,3 mm](https://techfun.sk/produkt/tlacidlo-1-kus-12x12x4-3-mm/) | 1 ks | 0,08 € |
| [Reflexný IR modul s digitálnym výstupom, napájanie 3,3 až 5 V](https://techfun.sk/produkt/infracerveny-senzor-pre-vyhybanie-sa-prekazkam/) | 1 ks | 0,65 € |
| [Nepájivé pole, 830 bodov](https://techfun.sk/produkt/nepajive-pole-830-bodov/) | 1 ks | 1,60 € |
| [Prepojky 10 cm, variant M–M](https://techfun.sk/produkt/kabliky-40-kusov-10-cm/) | 1 balenie (40 ks) | 1,45 € |
| **Súčiastky spolu** | | **4,84 €** |
| [Doručenie GLS ParcelShop](https://techfun.sk/sposob-platby-a-dorucenia/) | 1 zásielka | 2,75 € |
| **Spolu s dopravou** | | **7,59 €** |

Ak svetlá rozmiestnime ďalej od kontaktného poľa, [sada 20 cm prepojok](https://techfun.sk/produkt/kabliky-40-kusov-20-cm/) stojí 2,20 €. Pri tejto dĺžke vyjde objednávka s dopravou na 8,34 €. <sub>· model-gpio</sub>

Ak dokupujeme aj dátový kábel k doske, samostatná ponuka USB-A/Micro-B kábla s dĺžkou 1 m stojí 1,29 € plus doprava. <sub>· model-gpio</sub> [Solight SSC1301E](https://www.solight.sk/solight-usb-kabel-usb-2-0-a-konektor--usb-b-micro-konektor-sacek-1m-detail-6021000201.aspx)

## Rozdelenie práce

Každý člen navrhne, naprogramuje a overí vlastný modul. K modulu odovzdá zdrojové súbory a výsledky troch uvedených skúšok. Pri obhajobe svoju časť predvedie na spoločnom modeli. <sub>· traffic-fsm · model-gpio · usart-control</sub>

<sub>traffic-fsm · model-gpio · usart-control</sub>

| Člen a vetva | Návrh a implementácia | Samostatný výstup a obhajoba |
| --- | --- | --- |
| **Branislav Krajčovič** · `feature/traffic-fsm` | Pripraví stavový automat, kombinácie fáz, zaradenie oboch odbočení pri obsadenom vjazde C a ochranné intervaly. Doplní vybavenie požiadavky chodcov a vyhodnotenie povelov pre automat v Control. | Tabuľka prechodov, kód automatu a skúšky F1–F3. Predvedie oba súbehy pri odbočení z C a odmietnutie kolíznej požiadavky. |
| **Miroslav Kučera** · `feature/model-gpio` | Zhotoví model a zapojí päť RGB LED, dve šípky, tlačidlo a jeden IR snímač na C. Nakonfiguruje GPIO v CubeMX. Naprogramuje ovládač výstupov, inicializáciu a čítanie tlačidla a snímača. | Model, schéma zapojenia, kód ovládača a skúšky H1–H3. Predvedie obe šípky, samostatné semafory B a detekciu auta pri stopčiare C. |
| **Samuel Jantošovič** · `feature/usart-control` | Navrhne formát protokolu a konfiguráciu USART2. Naprogramuje príjem a odosielanie, skladanie riadkov, kontrolu syntaxe a formátovanie odpovedí. Platné požiadavky odovzdá automatu. | Popis protokolu, kód komunikácie a skúšky U1–U3 so záznamom z terminálu. Predvedie ručné ovládanie a spracovanie chybného príkazu. |

## Overenie funkčnosti

| Skúška | Postup a očakávaný výsledok | Správca |
| --- | --- | --- |
| **F1 – Obsluha vjazdu C** | Pri voľnom vjazde overíme A → B → A. S autom pred stopčiarou overíme A → C doľava → B s odbočením z C doprava → A. Auto odoberieme počas C doľava a skontrolujeme, že plánovaná fáza s `C_S` prebehne. | <sub>traffic-fsm</sub> |
| **F2 – Šípky, kolízie a chodci** | Overíme dvojicu `C` a `A_S` aj trojicu `B_R`, `B_D` a `C_S`. Počas zelenej A stlačíme tlačidlo. P dostane zelenú až po žltej a spoločnej červenej a všetky cestné pohyby ostanú zakázané. V Control pri zelenej `B_D` pošleme `SET A_S GREEN` a očakávame `ERR CONFLICT`. | <sub>traffic-fsm</sub> |
| **F3 – Prepnutie režimu** | Počas B s odbočením z C doprava stlačíme tlačidlo a vyžiadame Control. Overíme zhasnutie `C_S`, žltú na oboch B a spoločnú červenú. Keď `STATUS` ukáže `MODE=CONTROL` a `READY=1`, overíme `P_REQ=1` a zapneme P. Počas zelenej pre P vyžiadame Demo. Po ochrannej sekunde sa má spustiť A. | <sub>traffic-fsm</sub> |
| **H1 – Mapa LED** | Na stole zapneme postupne 12 výstupov, vždy jeden. Overíme priradenie kanálov a šípok. Na každom cestnom semafore zapneme R aj G a skontrolujeme, či sa žltá dá odlíšiť od červenej a zelenej. | <sub>model-gpio</sub> |
| **H2 – Inicializácia a prechod** | Resetujeme dosku počas aktívnej fázy a skontrolujeme, či prvý rozsvietený stav tvorí päť červených. Pri zmene fázy overíme žltú a spoločnú červenú pred rozsvietením ďalších zelených. | <sub>model-gpio</sub> |
| **H3 – Čítanie vstupov** | Postavíme auto pred stopčiaru C, overíme `CAR_C=1` počas státia a `CAR_C=0` po odchode. Prejazd po výjazdovom pruhu má ponechať `CAR_C=0`. Pri podržaní tlačidla sa zaznamená jedno stlačenie, ďalšie až po uvoľnení a novom stlačení. | <sub>model-gpio</sub> |
| **U1 – Ovládanie a výpis stavu** | Po `READY=1` skúsime `SET B_R GREEN`, `SET B_D GREEN`, `SET C GREEN` a `SET C_S GREEN`. Pred každým novým povolením ukončíme predchádzajúcu fázu a počkáme na `READY=1`. Pri `STATUS` s `LF` aj `CRLF` overíme hodnoty `CAR_C` a `P_REQ`. | <sub>usart-control</sub> |
| **U2 – Chybný príkaz a obnova príjmu** | V ustálenom Control pošleme `SET X GREEN`, `SET A_S YELLOW` a riadok s 33 znakmi. Očakávame `ERR SYNTAX`, `ERR STATE` a `ERR LENGTH`. Nasledujúci `STATUS` sa spracuje normálne. | <sub>usart-control</sub> |
| **U3 – Príkazy počas behu** | V Demo pošleme `SET A RED` a overíme `ERR MODE`. Počas zelenej pošleme `MODE CONTROL` a hneď po `OK` príkaz `SET P GREEN`. Počas prepínania má prísť `ERR BUSY`. | <sub>usart-control</sub> |

### Spoločná skúška na hotovom modeli

1. Po resete skontrolujeme spoločnú červenú a cyklus A → B pri voľnom C. Pristavením auta na C vyvoláme obe fázy s odbočením z C. Povolené pohyby prejdeme s modelmi áut po vyznačených dráhach a skontrolujeme odstupy. Z videa určíme trvanie zelenej, žltej a spoločnej červenej. <sub>· traffic-fsm · model-gpio</sub>
2. Počas B s odbočením z C doprava stlačíme tlačidlo. Po zhasnutí `C_S`, žltej na oboch B a spoločnej červenej dostane P zelenú. Overíme, že všetky cestné semafory svietia načerveno, obe šípky sú zhasnuté a po ukončení P sa pokračuje fázou A. <sub>· traffic-fsm · model-gpio</sub>
3. V Control po `READY=1` zadáme `SET A_S GREEN`. Na A má svietiť červená spolu so šípkou. Príkaz `SET B_D GREEN` musí vrátiť `ERR CONFLICT`. Po `SET A_S RED` a skončení spoločnej červenej zopakujeme príkaz pre `B_D` a overíme `OK` aj stav na modeli. <sub>· traffic-fsm · usart-control</sub>

K vyhodnoteniu priložíme namerané časy a záznam z terminálu s príkazmi a odpoveďami. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Vzdelávacie ciele

- Nakonfigurovať GPIO v STM32CubeMX, čítať IR snímač a odfiltrovať zákmity tlačidla. <sub>· model-gpio</sub>
- Zapájať RGB LED so spoločnou katódou a overiť mapu výstupov podľa výsledných farieb. <sub>· model-gpio</sub>
- Navrhnúť stavový automat s neblokujúcim časovaním a otestovať povolené prechody. <sub>· traffic-fsm</sub>
- Prenášať príkazy a odpovede cez USART a overiť, že komunikácia nenaruší časovanie fáz. <sub>· usart-control</sub>
- Dohodnúť rozhrania medzi modulmi v C a overiť ich spoluprácu na spoločnom modeli. <sub>· traffic-fsm · model-gpio · usart-control</sub>

## Postup práce v repozitári

1. Na vetve `main` vedieme spoločnú verziu projektu. Zmeny v konfigurácii `.ioc` a v mape GPIO pripravuje Miroslav po dohode s Branislavom a Samuelom. <sub>· traffic-fsm · model-gpio · usart-control</sub>
2. Každý člen vyvinie a odskúša svoj modul vo vlastnej vetve. Do `main` sa zmeny dostanú po kontrole kolegom cez pull request. <sub>· traffic-fsm · model-gpio · usart-control</sub>
3. Po spojení modulov overíme ich spoluprácu podľa [spoločnej skúšky na hotovom modeli](#spoločná-skúška-na-hotovom-modeli). <sub>· traffic-fsm · model-gpio · usart-control</sub>
4. Pred obhajobou zostavíme projekt z `main`, nahráme ho cez ST-LINK a vykonáme skúšky na hotovom modeli. Výsledky zaznamenáme spolu s mapou zapojenia a ukážkou komunikácie. <sub>· traffic-fsm · model-gpio · usart-control</sub>
