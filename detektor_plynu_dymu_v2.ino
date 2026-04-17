/*
 * ================================================================
 *   DETEKTOR PLYNU A DYMU – ESP32  v2.0
 * ================================================================
 *
 *  Senzory : MQ-2 (dym)  |  MQ-4 (zemný plyn)  |  MQ-5 (LPG)
 *  Výstupy : reproduktor 8 Ohm (cez BC547), červená LED
 *  Sieť    : Telegram – notifikácie + príkazy
 *
 * ----------------------------------------------------------------
 *  ZMENY v2.0 oproti v1.0 (na základe odbornej recenzie):
 *   [1] Delič napätia zmenený: 10kΩ/20kΩ → 6,8kΩ/10kΩ
 *       Dôvod: 10k/20k dáva 3.33V čo mierne presahuje max ESP32
 *       ADC vstupu (3.3V). Nová kombinácia dáva bezpečných 2.97V
 *       s rezervou 324mV. Rezistory bežne dostupné v SK e-shopoch.
 *   [2] Pridané explicitné nastavenie ADC útlmu (11 dB)
 *       Dôvod: zaistí správne meranie v celom rozsahu 0–3.3V
 *   [3] Pridaný hardvérový watchdog timer (10 sekúnd)
 *       Dôvod: ak kód zamrzne (napr. WiFi deadlock), ESP32 sa
 *       automaticky reštartuje – zvyšuje spoľahlivosť 24/7 prevádzky
 *   [4] Napájanie: odporúčaný zdroj zvýšený na 2A
 *       Dôvod: 3× MQ senzory (~480mA) + ESP32 (~300mA) = ~780mA
 *       Bežný USB zdroj 1A je na hranici, 2A je bezpečné riešenie
 * ----------------------------------------------------------------
 *
 *  NEZAHRNUTÉ odporúčania (zdôvodnenie):
 *   – TIP122 namiesto BC547: BC547 je pre 8Ω/0.5W reproduktor
 *     plne postačujúci. TIP122 je zbytočne silný pre tento prípad.
 *   – 1N4004 namiesto 1N4148: reproduktor má minimálnu spätnú EMF,
 *     1N4148 je dostatočná ochrana na breadboarde.
 *   – EEPROM kalibrácia: príliš komplexné pre amatéra, fixné prahy
 *     s manuálnou kalibráciou cez Serial Monitor sú postačujúce.
 *   – Dekoupling kondenzátory: na breadboarde nie sú nutné, prúdové
 *     špičky MQ senzorov sú dostatočne pomalé.
 *   – 100Ω sériový rezistor na ADC: delič 6,8k/10k dáva
 *     bezpečné napätie 2.97V (rezerva 324mV), ďalšia ochrana nie je nutná.
 *
 * ================================================================
 *
 *  TELEGRAM PRÍKAZY:
 *    /mute   – vypnúť zvukový alarm (LED zostáva svietiť)
 *    /status – aktuálne hodnoty všetkých troch senzorov
 *    /help   – zoznam príkazov
 *
 * ================================================================
 *
 *  ZAPOJENIE – prehľad pinov:
 *    MQ-x AO  →  delič (6,8kΩ/10kΩ)  →  GPIO 34 / 35 / 32
 *
 *    DELIČ pre KAŽDÝ senzor zvlášť:
 *      MQ-x AO ──── 6,8kΩ ────┬──── GPIO (34 / 35 / 32)
 *                              │
 *                             10kΩ
 *                              │
 *                             GND
 *
 *      Výpočet: 5V × (10k ÷ (6,8k+10k)) = 5 × 0.595 = 2.97V ✓
 *      (ESP32 ADC max = 3.3V → bezpečná rezerva 0.33V / 324mV)
 *
 *    LED (+)  →  330 Ohm  →  GPIO 26
 *    Tlačidlo →  GPIO 14  +  GND
 *
 *    REPRODUKTOR cez BC547:
 *      5V ──── Repr(+) ──── Repr(-) ──── [C] BC547 [E] ──── GND
 *                                              │
 *                                             [B]
 *                                              │
 *                                            1kΩ
 *                                              │
 *                                           GPIO 25
 *
 *      1N4148 dióda: anóda na Kolektor BC547, katóda na 5V
 *      (ochrana pred spätnou EMF reproduktora)
 *
 *  NAPÁJANIE:
 *    Použi USB nabíjačku min. 5V/2A!
 *    Tri MQ senzory ~480mA + ESP32 ~300mA = ~780mA celkový odber.
 *    Nabíjačky 1A môžu byť na hranici – 2A je bezpečná voľba.
 *
 * ================================================================
 *  POTREBNÉ KNIŽNICE (inštaluj cez Arduino → Manage Libraries):
 *    - UniversalTelegramBot  (autor: Brian Lough)
 *    - ArduinoJson           (autor: Benoit Blanchon, verzia 6.x)
 * ================================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include <esp_task_wdt.h>    // [v2.0] Hardvérový watchdog timer


// ================================================================
//   POVINNÉ NASTAVENIA  ← uprav pred nahraním!
// ================================================================

const char* WIFI_SSID     = "TVOJE_WIFI_MENO";       // ← zmeň
const char* WIFI_PASSWORD = "TVOJE_WIFI_HESLO";       // ← zmeň

// Telegram (postup ako získať pozri v návode)
const char* BOT_TOKEN   = "123456789:XXXXXXXXXXXXXXXXXXXXXXXXXXX"; // ← zmeň
const char* CHAT_ID_STR = "123456789";                             // ← zmeň


// ================================================================
//   PINY  (GPIO čísla na NodeMCU ESP32)
//
//   GPIO 34, 35, 32 sú ADC1 kanály – fungujú správne aj pri
//   zapnutom WiFi. ADC2 kanály s WiFi nefungujú – nepoužívame ich!
// ================================================================
#define PIN_MQ2      34    // MQ-2  – senzor dymu           (ADC1, len vstup)
#define PIN_MQ4      35    // MQ-4  – zemný plyn / metán    (ADC1, len vstup)
#define PIN_MQ5      32    // MQ-5  – LPG / propán-bután    (ADC1)
#define PIN_LED      26    // Červená LED  (+ 330 Ohm)
#define PIN_SPEAKER  25    // Reproduktor (cez BC547 tranzistor)
#define PIN_BUTTON   14    // Tlačidlo  (druhý pin tlačidla → GND)


// ================================================================
//   PRAHY CITLIVOSTI  (rozsah ADC: 0 – 4095)
//
//   Ako nastaviť:
//     1. Nahraj kód, otvor Serial Monitor (115200 baud)
//     2. Sleduj hodnoty MQ2/MQ4/MQ5 v čistom vzduchu  (napr. 300)
//     3. Nastaviť prahy na ~ 3× hodnotu čistého vzduchu (napr. 900)
//
//   Ak sú FALOŠNÉ ALARMY  → prahy ZVÝŠ
//   Ak NEDETEKUJE plyn/dym → prahy ZNÍŽ
// ================================================================
#define THRESHOLD_SMOKE   1500    // MQ-2 – dym
#define THRESHOLD_GAS4    1500    // MQ-4 – zemný plyn
#define THRESHOLD_GAS5    1500    // MQ-5 – LPG


// ================================================================
//   TÓNY REPRODUKTORA  (Hz)
//   Rôzne tóny pre rôzne typy alarmu – okamžite spoznáš typ
//   Nízky tón = plyn (pomalší, hlbší) | Vysoký tón = dym (rýchly)
// ================================================================
#define TONE_GAS    1000    // Hz – únik plynu
#define TONE_SMOKE  2500    // Hz – dym / spálené jedlo


// ================================================================
//   ČASOVANIE
// ================================================================
#define BLINK_INTERVAL    500    // ms – rýchlosť blikania LED
#define TELEGRAM_INTERVAL 1500   // ms – interval kontroly správ z Telegramu
#define DEBOUNCE_MS       200    // ms – ochrana tlačidla pred zákmitmi
#define ADC_SAMPLES       10     // počet meraní pre priemer (redukcia šumu)
#define WARMUP_SECONDS    60     // s  – zahrievanie MQ senzorov po zapnutí

// [v2.0] Watchdog – ak kód nedobehne loop() do 10s, ESP32 sa reštartuje
#define WATCHDOG_TIMEOUT_S  10


// ================================================================
//   NTP – Slovenský čas  (automaticky prepína leto/zima)
// ================================================================
#define NTP_SERVER  "pool.ntp.org"
#define TZ_STRING   "CET-1CEST,M3.5.0,M10.5.0/3"


// ================================================================
//   INTERNÉ PREMENNÉ  (nemeň)
// ================================================================
WiFiClientSecure     secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

bool   alarmActive    = false;
bool   buzzerMuted    = false;
bool   gasDetected    = false;
bool   smokeDetected  = false;
bool   prevGas        = false;   // pre správu pri ukončení alarmu
bool   prevSmoke      = false;
String alarmStartTime = "";

unsigned long tLastTelegram = 0;
unsigned long tLastBlink    = 0;
unsigned long tLastDebounce = 0;
bool   ledState     = false;
bool   lastBtnState = HIGH;


// ================================================================
//   POMOCNÉ FUNKCIE
// ================================================================

// Priemer z ADC_SAMPLES meraní – znižuje šum senzora
int readSmooth(int pin) {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / ADC_SAMPLES);
}

// Vráti aktuálny čas ako text  "DD.MM.RRRR HH:MM:SS"
String timeNow() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "--.--.---- --:--:--";
  char buf[22];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &ti);
  return String(buf);
}

// Odošle správu na Telegram
void tgSend(const String& msg) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID_STR, msg, "");
    Serial.println("[TG odoslana] " + msg.substring(0, 80));
  } else {
    Serial.println("[TG] WiFi nedostupne – sprava neodoslana.");
  }
}

// Spusti tón na reproduktore (ignoruje sa ak je alarm umlčaný)
void buzzerPlay(int freq) {
  if (!buzzerMuted) {
    ledcWriteTone(0, freq);
  }
}

// Zastaví reproduktor
void buzzerStop() {
  ledcWriteTone(0, 0);
}

// Pripojí ESP32 k WiFi, pri zlyhaní reštartuje
void wifiConnect() {
  Serial.printf("\nWiFi: pripajam k '%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();    // [v2.0] kŕmiť watchdog počas čakania na WiFi
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!  IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nCHYBA WiFi! Restartujem za 3 sekundy...");
    delay(3000);
    ESP.restart();
  }
}


// ================================================================
//   SPRACOVANIE TELEGRAM SPRÁV (príkazy)
// ================================================================
void handleTelegram() {
  int n = bot.getUpdates(bot.last_message_received + 1);

  while (n) {
    for (int i = 0; i < n; i++) {
      String cid  = bot.messages[i].chat_id;
      String text = bot.messages[i].text;
      String from = bot.messages[i].from_name;

      // Bezpečnosť: ignoruj správy od iných používateľov
      if (cid != String(CHAT_ID_STR)) {
        bot.sendMessage(cid, "Pristup zamietnuty.", "");
        continue;
      }

      Serial.println("[TG prijata] " + from + ": " + text);

      // ── /mute ──────────────────────────────────────────────────
      if (text == "/mute") {
        if (alarmActive) {
          buzzerMuted = true;
          buzzerStop();
          tgSend("Zvukovy alarm VYPNUTY.\n"
                 "LED zostava zapnuta kym problem neustane.");
        } else {
          tgSend("Momentalne nie je ziadny aktivny alarm.");
        }
      }

      // ── /status ────────────────────────────────────────────────
      else if (text == "/status") {
        int v2 = readSmooth(PIN_MQ2);
        int v4 = readSmooth(PIN_MQ4);
        int v5 = readSmooth(PIN_MQ5);

        String s = "=== STAV SENZOROV ===\n";
        s += "Cas: " + timeNow() + "\n";
        s += "---------------------\n";
        s += "MQ-2  (dym):        " + String(v2) +
             (v2 > THRESHOLD_SMOKE ? "  <!> ALARM" : "  [OK]") + "\n";
        s += "MQ-4  (zemny plyn): " + String(v4) +
             (v4 > THRESHOLD_GAS4  ? "  <!> ALARM" : "  [OK]") + "\n";
        s += "MQ-5  (LPG):        " + String(v5) +
             (v5 > THRESHOLD_GAS5  ? "  <!> ALARM" : "  [OK]") + "\n";
        s += "---------------------\n";
        s += "Alarm: " + String(alarmActive ? "AKTIVNY" : "NEAKTIVNY") + "\n";
        s += "Zvuk:  " + String(buzzerMuted  ? "uticany" : "aktivny")  + "\n";
        if (alarmActive) {
          s += "Zaciatok: " + alarmStartTime;
        }
        tgSend(s);
      }

      // ── /help  alebo  /start ────────────────────────────────────
      else if (text == "/help" || text == "/start") {
        tgSend("=== DETEKTOR PLYNU A DYMU ===\n\n"
               "Dostupne prikazy:\n"
               "/mute   - vypnut zvukovy alarm\n"
               "/status - aktualny stav senzorov\n"
               "/help   - tento zoznam\n\n"
               "Zariadenie automaticky posle spravu\n"
               "ked detekuje unik plynu alebo dym.");
      }

      // ── neznámy príkaz ─────────────────────────────────────────
      else {
        tgSend("Neznam prikaz. Napis /help");
      }
    }
    n = bot.getUpdates(bot.last_message_received + 1);
  }
}


// ================================================================
//   SETUP  – vykoná sa raz pri zapnutí
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== DETEKTOR PLYNU A DYMU  v2.0 ===");

  // ── [v2.0] Watchdog timer – reštartuje ESP32 ak zamrzne ────────
  //    Timeout: 10 sekúnd. Ak loop() nedobehne do 10s bez
  //    volania esp_task_wdt_reset(), ESP32 sa automaticky reštartuje.
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);    // pridaj hlavný task do watchdogu
  Serial.println("Watchdog timer aktivovany (10s timeout).");

  // ── Nastavenie pinov ────────────────────────────────────────────
  pinMode(PIN_LED,    OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);   // interný pull-up, tlačidlo → GND
  digitalWrite(PIN_LED, LOW);

  // ── LEDC – PWM kanál 0 pre reproduktor ─────────────────────────
  ledcSetup(0, 2000, 8);               // kanál 0, 2 kHz, 8-bit rozlíšenie
  ledcAttachPin(PIN_SPEAKER, 0);       // pripojiť GPIO 25 ku kanálu 0
  buzzerStop();

  // ── [v2.0] ADC nastavenie ───────────────────────────────────────
  //    12-bit rozlíšenie = hodnoty 0–4095
  //    ADC_11db attenuation = merací rozsah 0–3.3V
  //    DÔLEŽITÉ: bez správneho útlmu by ADC meralo len do ~1V!
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);      // ← [v2.0] explicitne pre celý rozsah
  Serial.println("ADC nastaveny: 12-bit, 11dB attenuation (0–3.3V rozsah).");

  // ── Self-test: 2× bliknutie LED + 2 pipnutia ───────────────────
  Serial.println("Self-test...");
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_LED, HIGH);
    buzzerPlay(1500);
    delay(200);
    digitalWrite(PIN_LED, LOW);
    buzzerStop();
    delay(200);
  }
  Serial.println("Self-test OK.");

  // ── WiFi pripojenie ─────────────────────────────────────────────
  wifiConnect();

  // ── SSL certifikát pre Telegram ─────────────────────────────────
  secureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // ── Synchronizácia času cez NTP ─────────────────────────────────
  Serial.print("NTP synchronizacia");
  configTzTime(TZ_STRING, NTP_SERVER);
  for (int i = 0; i < 20; i++) {
    struct tm t;
    if (getLocalTime(&t)) break;
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();              // kŕmiť watchdog počas čakania
  }
  Serial.println("  " + timeNow());

  // ── Telegram: správa o štarte ────────────────────────────────────
  tgSend("=== DETEKTOR SPUSTENY ===\n"
         "Cas: " + timeNow() + "\n\n"
         "Zahrievam senzory " + String(WARMUP_SECONDS) + " sekund...\n"
         "Prosim cakajte.");

  // ── Zahrievanie MQ senzorov ──────────────────────────────────────
  //    DÔLEŽITÉ: MQ senzory potrebujú ~60s na stabilizáciu.
  //    Počas tejto doby sú hodnoty nespoľahlivé – alarm je blokovaný.
  //    Pri ÚPLNE PRVOM zapnutí (nové senzory) nechaj bežať 24–48h
  //    pre plnú "burn-in" stabilizáciu.
  Serial.printf("Zahriatie senzorov: %d sekund\n", WARMUP_SECONDS);
  for (int i = WARMUP_SECONDS; i > 0; i--) {
    Serial.printf("  Zostava: %3ds  |  MQ2=%4d  MQ4=%4d  MQ5=%4d\r",
                  i,
                  analogRead(PIN_MQ2),
                  analogRead(PIN_MQ4),
                  analogRead(PIN_MQ5));
    delay(1000);
    esp_task_wdt_reset();              // kŕmiť watchdog každú sekundu
  }
  Serial.println("\n\nSenzory pripravene! Zacinam monitoring...");

  tgSend("Senzory su zahriate.\n"
         "MONITORING AKTIVNY.\n\n"
         "Napiste /status alebo /help");
}


// ================================================================
//   LOOP  – opakuje sa dookola
// ================================================================
void loop() {
  unsigned long now = millis();

  // [v2.0] Watchdog reset – zavolá sa každý cyklus (každých ~50ms)
  //        Ak toto prestane byť volané (kód zamrzne), ESP32 sa reštartuje
  esp_task_wdt_reset();

  // ── 1. Čítanie senzorov (priemery) ─────────────────────────────
  int v2 = readSmooth(PIN_MQ2);    // dym
  int v4 = readSmooth(PIN_MQ4);    // zemný plyn
  int v5 = readSmooth(PIN_MQ5);    // LPG

  // Výpis do Serial Monitora – sledovanie hodnôt pre kalibráciu
  Serial.printf("[%s]  MQ2(dym)=%4d  |  MQ4(gas)=%4d  |  MQ5(LPG)=%4d",
                timeNow().c_str(), v2, v4, v5);
  if (alarmActive) Serial.print("  <<< ALARM >>>");
  Serial.println();

  // ── 2. Vyhodnotenie stavu ────────────────────────────────────────
  smokeDetected = (v2 > THRESHOLD_SMOKE);
  gasDetected   = (v4 > THRESHOLD_GAS4) || (v5 > THRESHOLD_GAS5);
  bool currAlarm = smokeDetected || gasDetected;

  // ── 3. ZAČIATOK ALARMU ───────────────────────────────────────────
  if (currAlarm && !alarmActive) {
    alarmActive    = true;
    buzzerMuted    = false;          // každý nový alarm zas pípne
    alarmStartTime = timeNow();
    prevGas        = gasDetected;
    prevSmoke      = smokeDetected;

    String msg = "!!! ALARM SPUSTENY !!!\n";
    msg += "Zaciatok: " + alarmStartTime + "\n\n";

    if (gasDetected) {
      msg += ">>> UNIK PLYNU <<<\n";
      if (v4 > THRESHOLD_GAS4) msg += "  Zemny plyn MQ-4: " + String(v4) + "\n";
      if (v5 > THRESHOLD_GAS5) msg += "  LPG / Propan MQ-5: " + String(v5) + "\n";
      msg += "IHNED VETRAJTE A UZAVRITE PLYN!\n\n";
    }
    if (smokeDetected) {
      msg += ">>> DYM ZO SPALENEHO JEDLA <<<\n";
      msg += "  MQ-2 hodnota: " + String(v2) + "\n\n";
    }
    msg += "Pre vypnutie zvuku napisite: /mute";

    tgSend(msg);
    Serial.println(">>> ALARM SPUSTENY: " + alarmStartTime);
  }

  // ── 4. KONIEC ALARMU ─────────────────────────────────────────────
  if (!currAlarm && alarmActive) {
    alarmActive = false;
    String endTime = timeNow();

    String msg = "=== ALARM UKONCENY ===\n\n";
    if (prevGas)   msg += "Typ: Unik plynu\n";
    if (prevSmoke) msg += "Typ: Dym\n";
    msg += "Zaciatok: " + alarmStartTime + "\n";
    msg += "Koniec:   " + endTime + "\n\n";
    msg += "Prostredie je bezpecne.";

    tgSend(msg);
    Serial.println(">>> ALARM UKONCENY: " + endTime);

    buzzerStop();
    digitalWrite(PIN_LED, LOW);
    ledState    = false;
    buzzerMuted = false;
  }

  // Priebežná aktualizácia (ak sa počas alarmu objaví ďalší typ)
  if (alarmActive) {
    prevGas   = prevGas   || gasDetected;
    prevSmoke = prevSmoke || smokeDetected;
  }

  // ── 5. LED + ZVUK (blikanie počas alarmu) ────────────────────────
  if (alarmActive) {
    if (now - tLastBlink >= BLINK_INTERVAL) {
      tLastBlink = now;
      ledState = !ledState;
      digitalWrite(PIN_LED, ledState);

      if (!buzzerMuted) {
        if (ledState) {
          // "ON" fáza – rôzny tón podľa typu alarmu
          if (gasDetected && smokeDetected) {
            static bool toneToggle = false;
            toneToggle = !toneToggle;
            buzzerPlay(toneToggle ? TONE_GAS : TONE_SMOKE);
          } else if (gasDetected) {
            buzzerPlay(TONE_GAS);
          } else {
            buzzerPlay(TONE_SMOKE);
          }
        } else {
          buzzerStop();   // "OFF" fáza → efekt  beep–pauza–beep
        }
      }
    }
  }

  // ── 6. TLAČIDLO – vypnutie zvuku ─────────────────────────────────
  bool btnNow = digitalRead(PIN_BUTTON);

  // Zostupná hrana (HIGH→LOW = stlačenie) + debounce ochrana
  if (btnNow == LOW && lastBtnState == HIGH) {
    if (now - tLastDebounce > DEBOUNCE_MS) {
      tLastDebounce = now;

      if (alarmActive && !buzzerMuted) {
        buzzerMuted = true;
        buzzerStop();
        Serial.println("Zvuk vypnuty tlacidlom.");
        tgSend("Zvukovy alarm VYPNUTY tlacidlom.\n"
               "LED zostava zapnuta kym problem neustane.");
      }
    }
  }
  lastBtnState = btnNow;

  // ── 7. TELEGRAM – kontrola prichádzajúcich správ ─────────────────
  if (now - tLastTelegram >= TELEGRAM_INTERVAL) {
    tLastTelegram = now;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi stratene! Obnovujem...");
      wifiConnect();
      secureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    }

    handleTelegram();
  }

  // Krátka pauza – cca 20 cyklov za sekundu
  delay(50);
}
