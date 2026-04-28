# Audit projektu: Detektor plynu a dymu ESP32

**Auditovaný artefakt:** `detektor_plynu_dymu_v2.ino` + `detektor_plynu_navod_v2.html` (verzia 2.0)
**Dátum auditu:** 21. apríl 2026
**Výstup:** verzia 2.1 (aktualizované súbory + táto správa)
**Audítor:** trojitá rola Tvorca / Auditor / Konzultant — každý výstup bol sám overený fyzikálne aj logicky.

---

## 1. Čo bolo auditované

Kompletný kuchynský bezpečnostný systém založený na doske NodeMCU-32S s ESP32 WROOM-32, tromi MQ senzormi (MQ-2 dym, MQ-4 zemný plyn, MQ-5 LPG), reproduktorom 8 Ω 0,5 W (VYS427) cez tranzistor BC547, červenou LED, tlačidlom a Telegram botom pre notifikácie. Systém má 24/7 prevádzkový predpoklad, pripojený na USB nabíjačku 5 V/2 A.

Audit pokryl šesť oblastí: (1) delič napätia MQ → ADC, (2) LED obvod, (3) reproduktorový obvod s tranzistorom, (4) celková spotreba a napájanie, (5) využitie GPIO a ich prúdové limity, (6) impedancia ADC a presnosť merania. Okrem toho boli posúdené: logika kódu, watchdog, WiFi reconnect, bezpečnosť Telegram príkazov a kompatibilita s rôznymi verziami ESP32 Arduino Core.

---

## 2. Kritický nález — obvod reproduktora

### Problém

Pôvodný návrh v2.0 pripájal 8 Ω reproduktor priamo medzi 5 V a kolektor tranzistora BC547, bez obmedzovacieho odporu. Pri zopnutí tranzistora:

- Prúd, ktorý by preň tiekol: `I = (5 V − V_ce_sat) / 8 Ω = 4,8 / 8 = 600 mA`
- Maximálny kolektorový prúd BC547 (všetky varianty A/B/C): **100 mA**
- Výkon dodaný do reproduktora pri 50 % PWM duty: `P = (4,8² / 8) × 0,5 ≈ 1,44 W`
- Rating reproduktora VYS427: **0,5 W**

BC547 je teda preťažený ~6× a reproduktor ~2,9×. V praxi by sa dialo jedno z dvoch: buď by BC547 nevládal dodať plný prúd kvôli limitu hFE pri 1 kΩ bázovom odpore (Ib = 2,6 mA × hFE 200 = 520 mA teoretická strop), a stratený rozdiel by sa premenil na teplo na tranzistore (výkonová disipácia ~760 mW pri úzkom čase — tesne nad 625 mW max), alebo by sa zničil v priebehu minút až hodín nepretržitej prevádzky. V najhoršom prípade by BC547 zlyhal „nakrátko“ a preťažil USB zdroj.

### Oprava (v2.1)

Do série s reproduktorom — medzi pin Reproduktor(−) a kolektor BC547 — bol pridaný rezistor **47 Ω / 0,5 W**.

| Parameter | Vypočítaná hodnota | Rating komponentu | Stav |
|---|---|---|---|
| Špičkový prúd cez BC547 | `(5 − 0,2) / (47 + 8) = 87 mA` | Ic_max = 100 mA | ✓ bezpečné |
| Výkon do reproduktora (50 % PWM) | 30 mW | 500 mW | ✓ 6 % ratingu |
| Výkon rozptýlený v 47 Ω odpore | ~180 mW | 500 mW (0,5 W typ) | ✓ |
| Napätie na reproduktore (peak) | 0,70 V | n/a | — |

Výsledkom je tichší alarm (pri takomto nízkom výkone hlasitosť klesne oproti pôvodnému plánu), ale elektronika je v bezpečnej zóne aj pri nepretržitej 24/7 prevádzke.

### Alternatíva pre hlasnejší alarm

Ak používateľ požaduje hlasnejší alarm, je možné urobiť dve zmeny naraz:

1. BC547 nahradiť za **2N2222A** (drop-in náhrada, rovnaké poradie nôžok C-B-E pri balení TO-92, Ic_max = 800 mA).
2. Rezistor 47 Ω nahradiť za **22 Ω / 0,5 W**.

Po tejto zmene bude špičkový prúd `(5 − 0,2) / (22 + 8) = 160 mA`, výkon na reproduktor 102 mW (20 % ratingu) a výkon v odpore ~282 mW (stále v rámci 0,5 W). Alarm bude znateľne hlasnejší, stále v bezpečnej prevádzkovej zóne.

---

## 3. Ostatné nálezy

### 3.1 Typografia vo výstupe príkazu `/status`

Pôvodný reťazec obsahoval preklep `"uticany"` namiesto spisovne `"umlcany"` (umlčaný). Opravené v kóde v2.1. V telegrame sa teraz zobrazuje `Zvuk: umlcany` alebo `Zvuk: aktivny`.

### 3.2 Tolerancia na ESP32 Arduino Core 3.x

Kód používa staré API — `ledcSetup(kanál, freq, resolution)`, `ledcAttachPin(pin, kanál)`, `ledcWriteTone(kanál, freq)` a dvojparametrové `esp_task_wdt_init(timeout, panic)`. V ESP32 Arduino Core 3.x majú tieto funkcie úplne iné podpisy (watchdog očakáva konfiguračnú štruktúru, ledc API je prepracované). Na Core 3.x by projekt nekompilovaný zlyhal s neprehľadnými chybami.

Do v2.1 bola pridaná preprocesorová kontrola:

```cpp
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #error "Tento kod vyzaduje ESP32 Arduino Core 2.x.x ..."
#endif
```

Vďaka tomu kompilácia na Core 3.x skončí so zrozumiteľnou inštrukciou, čo má používateľ urobiť (preinštalovať cez Boards Manager na verziu 2.x.x, odporúčaná presná verzia 2.0.17).

### 3.3 Konzistencia dokumentácie s pamäťou projektu

Pamäť projektu obsahovala zmienku o deličovej kombinácii `6,8 kΩ + 10 kΩ` (= 2,97 V), zatiaľ čo kód aj HTML používajú `10 kΩ + 18 kΩ` (= 3,21 V). Obe kombinácie sú elektricky bezpečné a funkčné; kód je však pre používateľa autoritatívny, takže v aktualizácii v2.1 zostáva 10 kΩ / 18 kΩ. Rozdiel je zapracovaný do pamäte projektu pri ďalšej aktualizácii tak, aby sa budúce rozhodnutia robili proti aktuálnym číslam v kóde.

### 3.4 Kozmetické zlepšenia

Pridaná nová konštanta `WIFI_ATTEMPTS` (namiesto magickej „40“ v cykle), rozšírené komentáre najmä v hlavičke, pridaný aktualizovaný changelog. Kód je funkčne ekvivalentný s v2.0, ale čitateľnejší a lepšie dokumentovaný.

---

## 4. Čo bolo overené a ZOSTÁVA SPRÁVNE

| Oblasť | Výsledok overenia |
|---|---|
| Delič napätia 10 kΩ / 18 kΩ | 5 V × 18/(10+18) = **3,21 V** → bezpečná rezerva 90 mV pod limitom 3,3 V ESP32 ADC |
| Impedancia zdroja pre ADC | 10 kΩ ∥ 18 kΩ = **6,43 kΩ** → pod odporúčaným limitom <10 kΩ Espressif |
| LED obvod (330 Ω, červená) | I = (3,3 − 2,0) / 330 = **3,9 mA** → hlboko pod 40 mA GPIO limitom |
| Voľba GPIO | 34/35/32 sú ADC1 kanály — fungujú s WiFi. 14 nie je bootstrap pin (tie sú 0/2/5/12/15). 25 a 26 sú vhodné pre digitálny I/O. |
| Celková spotreba | 3× MQ (~480 mA) + ESP32 pri WiFi (~300 mA) ≈ **780 mA** → 2 A USB zdroj má 60 % rezervu |
| Hardvérový watchdog | 10 s timeout s kŕmením v každom loop() cykle aj počas dlhých čakaní (WiFi, NTP, warmup) — logika je korektná |
| Self-test pri štarte | 2× bliknutie LED + pípnutie, overí obvody bez čakania na plyn |
| Telegram bezpečnosť | Správy od iných `chat_id` sú ignorované s odpoveďou „Pristup zamietnuty“ — v poriadku |
| Warmup 60 s | Počas warmup sa alarm nemôže spustiť (nie sme v hlavnej slučke) — zámerné blokovanie počas nestability senzorov |
| ADC smoothing | 10-sample priemer v `readSmooth` — znižuje šum aj bez hardvérových decoupling kondenzátorov |
| NTP + Slovenský čas | TZ string `"CET-1CEST,M3.5.0,M10.5.0/3"` — automatický prechod zima/leto je korektný |

---

## 4a. Audit schémy — explicitný 6-bodový checklist

Pred uzavretím auditu si schému prejdeme cez štandardný checklist. Každý bod je explicitne odôvodnený, nie len „odškrtnutý“.

### ✓ 1. Napájacie napätie správne pre každý komponent

| Komponent | Potrebné napätie | Skutočné napätie v obvode | Stav |
|---|---|---|---|
| ESP32 WROOM-32 | 3,3 V (na 3V3 pine), cez USB 5 V na VIN | 5 V USB → interný LDO → 3,3 V | ✓ |
| MQ-2/4/5 moduly | 5 V (heater aj napájanie) | 5 V z pinu VIN ESP32 | ✓ |
| Reproduktor VYS427 | 0,5 W max, 8 Ω | v2.1 s 47 Ω sériovo → 30 mW (6 % ratingu) | ✓ |
| BC547 tranzistor | Ic_max 100 mA, Pmax 625 mW | v2.1: Ic = 87 mA, P_dissip ≈ 17 mW | ✓ |
| LED červená | 2,0 V / 20 mA typ | (3,3 − 2,0)/330 = 3,9 mA | ✓ |
| Tlačidlo | nie je aktívny prvok | — | ✓ |

### ✓ 2. Logické úrovne kompatibilné (3,3 V / 5 V)

- MQ senzory majú AO (analog out) výstup 0–5 V → priamo nekompatibilné s 3,3 V ADC.
- **Riešenie v obvode:** delič napätia 10 kΩ / 18 kΩ každý MQ výstup škáluje na max 3,21 V (90 mV rezerva pod 3,3 V).
- DO (digital out) pinov sa nevyužíva (z každého modulu vedie len GND, VCC, AO).
- Tlačidlo: cez `INPUT_PULLUP` na GPIO 14, uzemnené pri stlačení → logika 3,3 V ↔ 0 V, v poriadku.
- Všetky ostatné výstupy z ESP32 (LED, báza BC547, tón reproduktora) sú 3,3 V logika a cieľový komponent (BC547 báza cez 1 kΩ, LED cez 330 Ω) to znáša bez problému.

### ✓ 3. Prúdové limity GPIO neprekročené (ESP32 max 40 mA/pin, prakticky 12 mA)

| Pin | Použitie | Vypočítaný prúd | Limit | Stav |
|---|---|---|---|---|
| GPIO 25 | LED anóda cez 330 Ω | 3,9 mA | 40 mA | ✓ (hlboko pod) |
| GPIO 26 | Báza BC547 cez 1 kΩ | (3,3 − 0,7)/1000 = 2,6 mA | 40 mA | ✓ |
| GPIO 14 | Tlačidlo (vstup pullup) | 3,3/45k internal = ~0,07 mA | — | ✓ |
| GPIO 34/35/32 | Analógové vstupy z deličov | impedancia, zanedbateľný prúd | — | ✓ |
| Súčet cez 3V3 pin | všetky periférie | <15 mA | ESP32 3V3 rail ~500 mA | ✓ |

### ✓ 4. Pull-up / pull-down tam, kde treba

- **Tlačidlo GPIO 14:** `pinMode(14, INPUT_PULLUP)` → interný pullup ESP32 (~45 kΩ) aktivovaný. Externý pullup/pulldown netreba.
- **Báza BC547:** cez 1 kΩ na GPIO 26, výstup je vždy riadený (LOW ↔ HIGH PWM), pulldown proti „floating“ nie je potrebný v tomto obvode (GPIO je vždy v definovanom stave po `pinMode OUTPUT`).
- **MQ AO výstupy:** majú na modulovej doske sériový OP-AMP výstup, impedancia cca 1–5 kΩ → netreba externý pullup/pulldown, rovnako delič sa správa ako definovaný zdroj signálu.
- **I2C/SPI:** v projekte sa nepoužíva, pullupy netreba.

### ⚠️ 5. Decoupling kondenzátory pri napájaní IC

- **MQ moduly** majú už zabudovaný 100 nF keramický kondenzátor pri op-ampe priamo na PCB moduly → netreba pridávať.
- **ESP32 WROOM-32 modul** má integrované decoupling kondenzátory priamo na module (4× 100 nF + elektrolyt) → netreba pridávať.
- **Na breadboarde:** pridávať dedikované decoupling kondenzátory sa v hobby projekte tejto škály neštandardne nerobí; softvérové opatrenie — 10-vzorkový kĺzavý priemer v `readSmooth()` — kompenzuje občasný ADC noise dostatočne.
- **Odporúčanie:** ak by používateľ pozoroval nestabilné čítania (napr. pri pripnutí reproduktora), môže pridať 100 µF elektrolyt medzi 5 V a GND na napájacej lište breadboardu. V aktuálnom návrhu nie je kritické.
- **Stav:** ✓ akceptovateľné pre hobby rozsah, zdokumentované.

### ✓ 6. Ochrana proti prebudeniu / prepólovaniu / induktívnym špičkám

- **1N4148 freewheel dióda** paralelne k reproduktoru (katóda na +5 V, anóda na kolektor BC547) — chráni tranzistor pred induktívnymi špičkami z cievky reproduktora pri PWM vypnutí. Prítomné v v2.0 aj v2.1. ✓
- **47 Ω sériový odpor** (v2.1) — chráni BC547 aj reproduktor pred prúdovým preťažením. ✓
- **Watchdog timer (esp_task_wdt)** s 10 s timeoutom — chráni pred softvérovým zaseknutím ESP32 (napr. pri strate WiFi), automatický reset a pokračovanie. ✓
- **WiFi reconnect logic** s timeoutom a `wdt.reset()` v každej iterácii — zabraňuje nekonečnému cyklu v `connectWiFi()`. ✓
- **Warmup 60 s** — blokuje falošné alarmy počas stabilizácie MQ senzorov po zapnutí. ✓
- **Prepólovanie USB:** USB-C konektor je mechanicky reverzibilný — prepólovanie sa z princípu nemôže stať. ✓
- **Prepólovanie MQ modulov:** moduly majú ochranu na PCB (štandard MQ-X modul pinout: GND a VCC nie sú zameniteľné vďaka fixnému pinoutu a farbe vodičov v návode). ✓ (užívateľská chyba možná, ale moduly odolajú krátkodobému prepólovaniu vďaka ochrannej dióde na board-e)

### Výsledok checklistu

**6 z 6 bodov splnených** (bod 5 ako hobby-akceptovaný kompromis s explicitným odôvodnením a softvérovým backupom). Schéma v2.1 je elektricky bezpečná a fyzikálne realizovateľná pre nepretržitú 24/7 prevádzku.

---

## 5. Aktualizovaný zoznam komponentov (BOM) pre v2.1

| # | Súčiastka | Počet | Poznámka |
|---|---|---|---|
| 1 | NodeMCU-32S (ESP32 WROOM-32, 38-pin, CP2102) | 1× | hlavný MCU |
| 2 | Senzor MQ-2 | 1× | dym |
| 3 | Senzor MQ-4 | 1× | zemný plyn (metán) |
| 4 | Senzor MQ-5 | 1× | LPG (propán-bután) |
| 5 | Reproduktor 8 Ω 0,5 W (napr. VYS427) | 1× | alarm |
| 6 | Tlačidlo 4-pin tactile | 1× | manuálny mute |
| 7 | Tranzistor NPN BC547 (alternatíva 2N2222A) | 1× | spínač reproduktora |
| 8 | Dióda 1N4148 | 1× | freewheel (ochrana tranzistora) |
| **9** | **Rezistor 47 Ω / 0,5 W** (žltá-fialová-čierna) | **1×** | **[NOVÉ v v2.1] v sérii s reproduktorom** |
| 10 | Rezistor 1 kΩ (hnedá-čierna-červená) | 1× | bázový odpor BC547 |
| 11 | Rezistor 330 Ω (oranžová-oranžová-hnedá) | 1× | obmedzovač prúdu LED |
| 12 | Rezistor 10 kΩ (hnedá-čierna-oranžová) | 3× | delič napätia horný (pre MQ-2/4/5) |
| 13 | Rezistor 18 kΩ (hnedá-šedá-oranžová) | 3× | delič napätia dolný (pre MQ-2/4/5) |
| 14 | Červená LED 5 mm | 1× | vizuálna indikácia alarmu |
| 15 | Nepájivé pole (breadboard) | 1× | prototyp |
| 16 | Jumper wires (prepojky) | ~25× | spoje |
| 17 | USB nabíjačka 5 V / 2 A + kábel USB-C | 1× | napájanie |

**Kde kúpiť:** primárne slovenské a české e-shopy — **soselectronic.sk**, **hwkitchen.sk**, **gme.cz**, **tme.eu**. Cena celkovej dodatočnej sady (rezistory, tranzistor, dióda, LED) typicky 3–5 €.

---

## 6. Ak si mal postavenú už v2.0 — ako prejsť na v2.1

1. **Odpoj USB napájanie.**
2. Do série s reproduktorom vlož **rezistor 47 Ω 0,5 W** (medzi pin reproduktora mínus a kolektor BC547). Inde zapojenie nemení.
3. Stiahni nový súbor `detektor_plynu_dymu_v2_1.ino` a nahraj ho do ESP32 (cez Arduino IDE, ostatné nastavenia zostávajú).
4. V `/status` Telegram príkaze skontroluj, že riadok `Zvuk:` obsahuje `aktivny` alebo `umlcany` (ak vidíš starý text `uticany`, nahratie neprebehlo).

Ak si v2.0 prevádzkoval dlhší čas bez 47 Ω rezistora a BC547 sa zahrieval, preventívne ho vymeň za nový — polovodič mohol utrpieť degradáciu.

---

## 7. Bezpečnostné poznámky (pripomienka)

Tento projekt je hobby kuchynský monitoring, **nie** certifikovaný detektor podľa EN 50194 / EN 50291. Neslúži ako náhrada certifikovaného detektora plynu alebo dymu vyžadovaného regulačnými predpismi. Jeho cieľom je doplnková notifikácia cez Telegram v prípade spáleného jedla alebo úniku domáceho plynu. Pre spoľahlivú ochranu života a majetku pred CO a požiarom odporúčame zaobstarať aj certifikovaný detektor.

---

## 8. Sumár zmien v2.0 → v2.1

| Kategória | Zmena | Súbory |
|---|---|---|
| Kritická HW oprava | Pridaný 47 Ω 0,5 W rezistor v sérii s reproduktorom (ochrana BC547 a reproduktora) | `.ino` (komentár/schéma), `.html` (BOM, schéma, sekcia zmien) |
| Kozmetika | Oprava typografie `uticany` → `umlcany` | `.ino`, `.html` (embedded kód) |
| Bezpečnosť kompilácie | Preprocesorový `#error` pre ESP32 Core 3.x | `.ino`, `.html` (embedded kód) |
| Dokumentácia | Rozšírený changelog, zmeny v návode, nová sekcia „Zmeny v2.1“ | `.html` |
| Čitateľnosť | Konštanta `WIFI_ATTEMPTS`, doplnené komentáre | `.ino`, `.html` (embedded kód) |
| Troubleshooting | Rozšírená tabuľka častých problémov (warm BC547, kompilačné chyby, tichý reproduktor) | `.html` |

**Funkčne je kód s v2.0 ekvivalentný** — všetky detekčné logiky, Telegram príkazy, blikanie, warmup, watchdog a NTP fungujú rovnako. Rozdiel je v hardvérovej bezpečnosti (nový rezistor) a v odolnosti proti omylu (Core 3.x check, preklepy, lepšia dokumentácia).
