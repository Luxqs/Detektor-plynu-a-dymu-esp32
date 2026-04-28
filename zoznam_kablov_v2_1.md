# Zoznam káblov (wire list) — Detektor plynu a dymu ESP32 v2.1

Tento dokument je sprievodcom **k schéme** `schema_zapojenia_v2_1.svg`. Schému otvor v prehliadači (stačí dvojklik) a popri nej si prechádzaj túto tabuľku — každý riadok = jeden drôt (jeden spoj) na breadboarde.

Farby drôtov sú **odporúčané** — fyzikálne môžeš použiť hocijakú farbu, ale prehľadnosť ti neskôr zachráni život.

---

## Príprava breadboardu (pred prvým drôtom)

Tvoj nepájivý breadboard má dve strany, každá má **dva dlhé napájacie pásy** (označené `+` a `−`). Pred všetkým ostatným:

1. **Spoj oba červené pásy** (`+` na ľavej strane a `+` na pravej) krátkym červeným drôtom na okraji. Tieto dva pásy budú teraz dokopy tvoriť tvoj **+5V pás**.
2. **Spoj oba modré/čierne pásy** (`−` vľavo a `−` vpravo) krátkym čiernym drôtom. Toto je tvoj **GND pás**.

Odteraz, kedykoľvek píšem "GND" alebo "+5V", myslím tento pás.

---

## Sekcia A: Napájanie ESP32 → rozvod +5V a GND

| # | Od | Do | Farba | Poznámka |
|---|---|---|---|---|
| A1 | ESP32 pin **VIN** | +5V pás | červená | ESP32 dáva na VIN 5 V z USB-C |
| A2 | ESP32 pin **GND** (hociktorý z 3) | GND pás | čierna | dostačujú akýkoľvek GND pin |

**Pozor:** NodeMCU-32S má viac GND pinov — stačí zapojiť **jeden**, ostatné sú vnútorne spojené.

---

## Sekcia B: Senzor MQ-2 (dym) → GPIO 34

Senzor MQ-2 má štyri piny: **VCC, GND, DO, AO**. Používame len VCC, GND, AO.

| # | Od | Do | Farba | Poznámka |
|---|---|---|---|---|
| B1 | MQ-2 pin **VCC** | +5V pás | červená | napájanie senzora z 5 V |
| B2 | MQ-2 pin **GND** | GND pás | čierna | — |
| B3 | MQ-2 pin **AO** | jeden pin **rezistora 10 kΩ** | modrá (signál) | výstup senzora — surovo 0–5 V |
| B4 | druhý pin **rezistora 10 kΩ** | **spoj X** (voľný riadok breadboardu) | modrá | toto je bod deliča |
| B5 | **spoj X** | jeden pin **rezistora 18 kΩ** | — | oba rezistory sa spájajú v tomto bode |
| B6 | druhý pin **rezistora 18 kΩ** | GND pás | čierna | spodný rameno deliča |
| B7 | **spoj X** | ESP32 pin **GPIO 34** | modrá | redukovaný signál ~0–3,21 V ide do ADC |

**Prečo delič:** MQ-2 má výstup 0–5 V, ale ESP32 analógový vstup znesie max 3,3 V. Delič 10 kΩ / 18 kΩ zníži maximum na 3,21 V (bezpečná rezerva 90 mV).

---

## Sekcia C: Senzor MQ-4 (zemný plyn) → GPIO 35

Presne rovnaké ako Sekcia B, len pre druhý senzor a iný pin ESP32.

| # | Od | Do | Farba |
|---|---|---|---|
| C1 | MQ-4 pin **VCC** | +5V pás | červená |
| C2 | MQ-4 pin **GND** | GND pás | čierna |
| C3 | MQ-4 pin **AO** | pin **10 kΩ** rezistora | modrá |
| C4 | druhý pin **10 kΩ** | **spoj Y** | modrá |
| C5 | **spoj Y** | pin **18 kΩ** | — |
| C6 | druhý pin **18 kΩ** | GND pás | čierna |
| C7 | **spoj Y** | ESP32 pin **GPIO 35** | modrá |

---

## Sekcia D: Senzor MQ-5 (LPG) → GPIO 32

| # | Od | Do | Farba |
|---|---|---|---|
| D1 | MQ-5 pin **VCC** | +5V pás | červená |
| D2 | MQ-5 pin **GND** | GND pás | čierna |
| D3 | MQ-5 pin **AO** | pin **10 kΩ** | modrá |
| D4 | druhý pin **10 kΩ** | **spoj Z** | modrá |
| D5 | **spoj Z** | pin **18 kΩ** | — |
| D6 | druhý pin **18 kΩ** | GND pás | čierna |
| D7 | **spoj Z** | ESP32 pin **GPIO 32** | modrá |

---

## Sekcia E: Tlačidlo → GPIO 14

4-pinové tactile tlačidlo má 4 nôžky, ale elektricky sú len **dva piny** (dvojice sú interne spojené). Stačí použiť ľubovoľnú dvojicu protiľahlých nôžok.

| # | Od | Do | Farba |
|---|---|---|---|
| E1 | jedna nôžka tlačidla | GND pás | čierna |
| E2 | opačná nôžka tlačidla | ESP32 pin **GPIO 14** | žltá (voliteľne) |

**Prečo bez externého pull-up rezistora:** ESP32 má v kóde nastavené `INPUT_PULLUP`, teda **interný** pull-up (~45 kΩ). Tlačidlo sa len uzemní, keď ho stlačíš, a ESP32 to číta ako `LOW`.

---

## Sekcia F: LED indikátor → GPIO 26

Červená LED 5 mm má **dve nôžky**. Dlhšia = anóda (+), kratšia = katóda (−).

| # | Od | Do | Farba |
|---|---|---|---|
| F1 | ESP32 pin **GPIO 26** | jeden pin **rezistora 330 Ω** | zelená (výstup) |
| F2 | druhý pin **rezistora 330 Ω** | **anóda LED** (dlhšia nôžka, +) | zelená |
| F3 | **katóda LED** (kratšia nôžka, −) | GND pás | čierna |

**Prečo 330 Ω:** pri 3,3 V výstupe ESP32 a páde napätia 2 V na červenej LED dostaneme prúd 3,9 mA — LED svieti viditeľne a ESP32 sa neprehrieva.

---

## Sekcia G: Reproduktor + 1N4148 + BC547 + R47 → GPIO 25

**⚠️ POZOR:** Toto je najzložitejšia časť. Pri stavaní postupuj **presne v tomto poradí** a neodboč.

BC547 má 3 nôžky. Keď držíš tranzistor plochou stranou (s nápisom "BC547") **k sebe** a nôžkami nadol, sú v poradí **zľava doprava: C, B, E** (kolektor, báza, emitor). Alebo pre istotu: pri väčšine BC547 TO-92 je **C vľavo, B v strede, E vpravo**, ale vždy si to over na datasheete alebo multimetrom.

1N4148 dióda má **čierny pásik** na jednej strane — to je **katóda (K, −)**.

### Hlavná vetva (tranzistor)

| # | Od | Do | Farba |
|---|---|---|---|
| G1 | ESP32 pin **GPIO 25** | jeden pin **rezistora 1 kΩ** | zelená (PWM výstup) |
| G2 | druhý pin **rezistora 1 kΩ** | **báza (B)** BC547 | zelená |
| G3 | **emitor (E)** BC547 | GND pás | čierna |

### Reproduktor (paralelne s diódou)

| # | Od | Do | Farba |
|---|---|---|---|
| G4 | **(+) reproduktor** | +5V pás | červená |
| G5 | **(−) reproduktor** | **spoj W** (voľný riadok breadboardu) | oranžová |

### 1N4148 freewheel dióda (paralelne s reproduktorom)

| # | Od | Do | Farba |
|---|---|---|---|
| G6 | **katóda diódy (pásik)** | +5V pás | červená |
| G7 | **anóda diódy (opačný koniec)** | **spoj W** (ten istý, čo v G5) | oranžová |

### R47 rezistor (NOVÉ v v2.1 — chráni tranzistor)

| # | Od | Do | Farba |
|---|---|---|---|
| G8 | **spoj W** | jeden pin **rezistora 47 Ω 0,5 W** | čierna |
| G9 | druhý pin **rezistora 47 Ω** | **kolektor (C)** BC547 | čierna |

**Prečo takto to ide dokopy:**
- Keď ESP32 cez GPIO 25 pustí PWM signál na bázu cez R1k, BC547 sa otvorí.
- Prúd potečie: **+5V → reproduktor → R47 → kolektor BC547 → emitor → GND**.
- Reproduktor "buble" podľa frekvencie PWM (nastavené na 1500 Hz alebo 2000 Hz v kóde).
- R47 **obmedzuje** prúd na bezpečných 87 mA (bez neho by to bolo 600 mA a zničilo by BC547).
- Keď PWM signál ide dole a tranzistor sa zatvára, cievka reproduktora by urobila vysoký napäťový špic (kickback). **Dióda 1N4148 tento špic zachytí** — pustí ho späť do +5V.

---

## Celkový kontrolný prehľad

Po zapojení by si mal mať:

- [ ] ESP32 pripojené k USB-C nabíjačke 5 V / 2 A (nezapínaj ešte — prvé zapnutie až po dokončení)
- [ ] +5V pás breadboardu má 7 pripojení: ESP32 VIN + 3× MQ senzor VCC + reproduktor (+) + dióda katóda + ESP32 3V3 (NIE, 3V3 nepoužívame)
- [ ] GND pás má 9 pripojení: ESP32 GND + 3× MQ senzor GND + 3× 18 kΩ rezistor dolný pin + LED katóda + tlačidlo + BC547 emitor
- [ ] 3× delič napätia (10 kΩ + 18 kΩ) postavené, ich stredové body idú do GPIO 34/35/32
- [ ] LED v sérii s 330 Ω rezistorom medzi GPIO 26 a GND
- [ ] Tlačidlo medzi GPIO 14 a GND
- [ ] Tranzistor BC547 s bázou cez 1 kΩ na GPIO 25, emitorom na GND
- [ ] Reproduktor paralelne s diódou 1N4148 medzi +5V a kolektorom BC547, cez 47 Ω do série

**Ak máš všetky body odškrtnuté, zapoj USB-C kábel** a sleduj Serial Monitor v Arduino IDE (115200 baud). V priebehu 60 sekúnd od zapnutia má ESP32 urobiť self-test (2× blikne LED + dvakrát pípne), potom hlási "Warmup..." a po 60 s "Ready — detection active".

---

## Časté chyby pri zapojení (checklist pred prvým zapnutím)

| Symptóm | Pravdepodobná príčina | Oprava |
|---|---|---|
| Po zapnutí sa nič nedeje | ESP32 nie je napájaný | Over VIN → +5V pás, GND → GND pás |
| ESP32 sa reštartuje stále dokola | Nestabilné napájanie, slabé USB | Použi kvalitnú 2A nabíjačku |
| LED nesvieti ani pri self-teste | Prehodená polarita LED, alebo chýba 330 Ω | Prehoď nôžky LED, over rezistor |
| Reproduktor tichý/nefunguje | Chýba R47 alebo prehodený BC547 (C↔E) | Over orientáciu BC547 podľa datasheetu |
| BC547 sa po minúte silno zahreje | Chýba 47 Ω v sérii s reproduktorom | ⚠️ Okamžite odpoj napájanie, pridaj R47! |
| Senzory hlásia 0 stále | AO pin na nesprávnej nôžke modulu | Over pinout modulu (býva to pin úplne vpravo/vľavo) |
| Alarm sa spustí hneď po zapnutí | Warmup ešte nebeží, alebo tresh príliš nízky | Počkaj 60 s; potom v kóde skontroluj `GAS_THRESHOLD` |

---

*Verzia dokumentu: 1.0 (sprievodca pre schému v2.1)*
*Dátum: apríl 2026*
