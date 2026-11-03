# Model T-križovatky so svetelnou signalizáciou

Tímový projekt z predmetu VRS

## Popis projektu

Zostavime model T-križovatky s jedným priechodom pre chodcov, v ktorom STM32 riadi svetlá cestných a chodcovských semaforov. Zariadenie bude pracovať v dvoch režimoch. V režime Demo sa svetlá automaticky prepínajú podľa vopred definovanej sekvencie a časových intervalov. V režime Control môže používateľ prostredníctvom príkazov cez USART ovládať jednotlivé svetlá semaforov a prepínať prevádzkové režimy. Súčasťou projektu je návrh modelu, zapojenie LED, implementácia stavového automatu a komunikačného protokolu s potvrdením príkazov a hlásením chýb. Systém musí zabrániť súčasnému povoleniu kolíznych smerov jazdy alebo prejazdu vozidiel počas zelenej pre chodcov.

## Rozdelenie práce

| Člen tímu | Zodpovednosť | Výstup |
| --- | --- | --- |
| Branislav Krajčovič | Návrh a implementácia stavového automatu, neblokujúce časovanie Demo, prepínanie režimov a vyhodnocovanie konfliktov v oboch režimoch. | Funkčný automat a overené prechody medzi fázami. |
| Miroslav Kučera | Návrh a stavba modelu, výber GPIO, zapojenie 11 LED a implementácia ovládača výstupov vrátane inicializácie a bezpečného poradia zmeny LED. | Fyzický model, schéma zapojenia a funkčný ovládač GPIO. |
| Samuel Jantošovič | Konfigurácia USART, príjem riadkov, spracovanie príkazov, odovzdávanie požiadaviek stavovému automatu a odosielanie potvrdení, stavov a chýb. | Funkčné ovládanie zo sériového terminálu a popis protokolu. |
