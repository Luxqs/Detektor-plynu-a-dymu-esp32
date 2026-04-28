/*
 * ================================================================
 *   DETEKTOR PLYNU A DYMU – ESP32   v2.1
 *   (Kuchynský bezpečnostný systém s Telegram notifikáciami)
 * ================================================================
 *
 *  Doska  : NodeMCU-32S (ESP32 WROOM-32, 38-pin, CP2102 USB-C)
 *  Senzory: MQ-2 (dym)  |  MQ-4 (zemný plyn / metán)  |  MQ-5 (LPG)
 *  Výstup : reproduktor 8 Ω / 0.5 W (cez BC547) + červená LED
 *  Vstup  : tlačidlo (manuálne vypnutie zvuku)
 *  Sieť   : WiFi + Telegram bot (notifikácie + príkazy)
 *
 * ----------------------------------------------------------------
 *  ZMENY v2.1  (kritická oprava + kozmetické úpravy)
 * ----------------------------------------------------------------
 *   [A] KRITICKÁ OPRAVA – reproduktorový obvod:
 *       PRIDAŤ 47 Ω / 0.5 W rezistor V SÉRII s reproduktorom!
 *       Dôvod: bez neho by 8 Ω reproduktor priamo na 5 V ťahal
 *       ~600 mA. BC547 má Ic_max len 100 mA → zničil by sa,
 *       a reproduktor 0.5 W by dostal ~1.4 W (prehorenie).
 *       S odporom 47 Ω: I_peak = 87 mA (bezpečné pre BC547),
 *                       P_speaker = ~30 mW (bezpečné pre reproduktor).
 *
 *   [B] Oprava typografie: "uticany" → "umlcany" (umlčaný).
 *
 *   [C] Kontrola verzie ESP32 Arduino Core:
 *       Ak je nainštalovaný Core 3.x, kompilátor ukončí build
 *       s jasnou chybovou správou (nepôjde to nahrať).
 *       Core 3.x zmenil API pre LEDC aj watchdog — pre tento kód
 *       je POVINNÝ Core 2.x.x (odporúčaná verzia 2.0.17).
 *
 *   [D] Pridaný defenzívny #define pre max. dĺžku Telegram správy
 *       a konštanta pre počet WiFi pokusov (lepšia čitateľnosť).
 *
 *   [E] Rozšírené komentáre a bezpečnostné upozornenia.
 *
 * ----------------------------------------------------------------
 *  ZMENY v2.0 (pre historický kontext – naďalej platia)
 * ----------------------------------------------------------------
 *   [1] Delič napätia 10 kΩ / 20 kΩ → 10 kΩ / 18 kΩ
 *       Dôvod: 10k/20k dávalo 3.33 V (presah nad 3.3 V limit ADC).
 *              Nová kombinácia dáva bezpečných 3.21 V (rezerva 90 mV).
 *   [2] Explicitné nastavenie ADC útlmu 11 dB (celý rozsah 0–3.3 V).
 *   [3] Pridaný hardvérový watchdog timer (10 s timeout).
 *   [4] Odporúčaný USB zdroj zvýšený na 5 V / 2 A (3× MQ + ESP32).
 *
 * ----------------------------------------------------------------
 *  NEZAHRNUTÉ (zámerne – nie sú potrebné pre amatérsky breadboard)
 * ----------------------------------------------------------------
 *   – TIP122 namiesto BC547: s 47 Ω sériovým odporom je BC547 OK.
 *     (TIP122 je Darlington — potrebuje iný bázový odpor a je drahší.)
 *   – 1N4004 namiesto 1N4148: reproduktor má minimálnu spätnú EMF.
 *   – EEPROM kalibrácia: fixné prahy stačia pre domáce použitie.
 *   – Decoupling kondenzátory: pri 10-sample softvérovom priemere
 *     a napájaní z USB sieťového zdroja nie sú nutné.
 *   – 100 Ω sériový rezistor na ADC: delič 10k/18k dáva bezpečné 3.21 V.
 *
 * ================================================================
 *  TELEGRAM PRÍKAZY (posielajú sa do vytvoreného bota)
 * ================================================================
 *    /mute   – vypne zvukový alarm (LED zostáva svietiť)
 *    /status – aktuálne hodnoty senzorov + stav alarmu
 *    /help   – zoznam príkazov
 *    /start  – to isté ako /help (Telegram ho posiela automaticky)
 *
 * ================================================================
 *  SCHÉMA ZAPOJENIA – prehľad pinov NodeMCU-32S
 * ================================================================
 *
 *   MQ-x AO ─── [10kΩ] ───┬─── GPIO 34 / 35 / 32  (ADC1 vstupy)
 *                         │
 *                       [18kΩ]
 *                         │
 *                        GND
 *          (3× taký delič – jeden na každý senzor)
 *
 *   LED (anóda +) ─── [330Ω] ─── GPIO 26
 *   LED (katóda −) ─── GND
 *
 *   Tlačidlo pin 1 ─── GPIO 14
 *   Tlačidlo pin 2 ─── GND
 *     (INPUT_PULLUP nakonfigurovaný v kóde – externý odpor netreba)
 *
 *   ▼▼▼  NOVÉ v v2.1 — reproduktorový obvod  ▼▼▼
 *
 *   5V ───── Reproduktor(+) ── Reproduktor(−) ──── [47Ω / 0.5W] ──┐
 *                                                                  │
 *                      (kolektor BC547) ◄───────────────────────── X
 *                              │
 *                              ├──── [1N4148] ──► 5V
 *                              │     (anóda na X, katóda na 5V –
 *                              │      prúžok diódy smeruje k 5V)
 *                              │
 *          (báza BC547) ── [1kΩ] ── GPIO 25  (PWM výstup ESP32)
 *                              │
 *          (emitor BC547) ──── GND
 *
 *   PRE IDENTIFIKÁCIU NÔŽOK BC547 (plochá strana s nápisom k vám):
 *     ĽAVÁ   = Kolektor (C)
 *     STRED  = Báza       (B)
 *     PRAVÁ  = Emitor     (E)
 *
 * ================================================================
 *  NAPÁJANIE
 * ================================================================
 *   USB zdroj min. 5 V / 2 A (telefónna nabíjačka alebo powerbanka).
 *   Celkový odber: 3× MQ (~480 mA) + ESP32 (~300 mA) ≈ 780 mA.
 *   Nabíjačka 5V/1A je na hranici stability — 2A je bezpečná voľba.
 *
 * ================================================================
 *  POTREBNÉ ARDUINO KNIŽNICE (Tools → Manage Libraries)
 * ================================================================
 *   - UniversalTelegramBot  (autor: Brian Lough)     verzia 1.3.x+
 *   - ArduinoJson           (autor: Benoit Blanchon) verzia 6.x
 *   ( WiFi.h, WiFiClientSecure.h, esp_task_wdt.h sú
 *     súčasťou ESP32 Arduino Core – netreba inštalovať. )
 *
 * ================================================================
 *  POZNÁMKA K VERZII ESP32 CORE
 * ================================================================
 *   Tento kód je napísaný pre ESP32 Arduino Core 2.x.x.
 *   (odporúčaná presná verzia: 2.0.17 – posledná stabilná 2.x)
 *   Core 3.x má nekompatibilné API pre ledcSetup/ledcWriteTone
 *   aj pre esp_task_wdt_init. Ak máš nainštalovaný Core 3.x,
 *   kompilácia zlyhá so zreteľnou chybovou správou — musíš v
 *   Boards Manager preinštalovať na verziu 2.x.x.
 * ================================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include <esp_task_wdt.h>

// ----------------------------------------------------------------
//  [v2.1] Kontrola verzie ESP32 Arduino Core
//         ESP_ARDUINO_VERSION_MAJOR je definované v core >= 2.0.x.
//         Pre Core 3.x by kompilácia aj tak zlyhala na ledcSetup()
//         — tento test len dá používateľovi zrozumiteľnú správu.
// ----------------------------------------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #error "Tento kod vyzaduje ESP32 Arduino Core 2.x.x (odporucane 2.0.17). \
V Boards Manager preinstaluj esp32 na verziu 2.x.x! \
Core 3.x zmenil ledcSetup/ledcWriteTone API a kod by nefungoval."
#endif


// ================================================================
//   POVINNÉ NASTAVENIA  ← uprav PRED nahraním!
// ================================================================
const char* WIFI_SSID     = "TVOJE_WIFI_MENO";       // ← zmeň
const char* WIFI_PASSWORD = "TVOJE_WIFI_HESLO";      // ← zmeň

// Telegram (postup ako získať: pozri návod, sekcia "Telegram Bot")
const char* BOT_TOKEN   = "123456789:XXXXXXXXXXXXXXXXXXXXXXXXXXX"; // ← zmeň
const char* CHAT_ID_STR = "123456789";                             // ← zmeň


// ================================================================
//   PINY  (GPIO čísla na NodeMCU-32S)
//
//   GPIO 34, 35, 32 sú ADC1 kanály – fungujú správne aj pri zapnutej
//   WiFi. (ADC2 s WiFi nefunguje – preto ich nepoužívame.)
// ================================================================
#define PIN_MQ2      34    // MQ-2  – senzor dymu           (ADC1, len vstup)
#define PIN_MQ4      35    // MQ-4  – zemný plyn / metán    (ADC1, len vstup)
#define PIN_MQ5      32    // MQ-5  – LPG / propán-bután    (ADC1)
#define PIN_LED      26    // Červená LED  (+ 330 Ω rezistor)
#define PIN_SPEAKER  25    // PWM pre reproduktor  (cez BC547 + 47 Ω)
#define PIN_BUTTON   14    // Tlačidlo  (druhý pin tlačidla → GND)


// ================================================================
//   PRAHY CITLIVOSTI  (rozsah ADC: 0 – 4095)
//
//   Ako nastaviť:
//     1. Nahraj kód, otvor Serial Monitor (115200 baud).
//     2. Sleduj hodnoty MQ2/MQ4/MQ5 v čistom vzduchu (napr. 300).
//     3. Nastav prahy na ~ 3× hodnotu čistého vzduchu (napr. 900–1500).
//
//   FALOŠNÉ ALARMY?   → prahy ZVÝŠ
//   NEDETEKUJE plyn?  → prahy ZNÍŽ
// ================================================================
#define THRESHOLD_SMOKE   1500    // MQ-2 – dym
#define THRESHOLD_GAS4    1500    // MQ-4 – zemný plyn
#define THRESHOLD_GAS5    1500    // MQ-5 – LPG


// ================================================================
//   TÓNY REPRODUKTORA  (Hz)
//   Rôzne tóny pre rôzne typy alarmu – okamžite spoznáš typ.
//   Nízky tón = plyn (hlbší) | Vysoký tón = dym
// ================================================================
#define TONE_GAS    1000    // Hz – únik plynu
#define TONE_SMOKE  2500    // Hz – dym / spálené jedlo


// ================================================================
//   ČASOVANIE
// ================================================================
#define BLINK_INTERVAL       500    // ms – rýchlosť blikania LED
#define TELEGRAM_INTERVAL    1500   // ms – interval kontroly Telegram správ
#define DEBOUNCE_MS          200    // ms – ochrana tlačidla pred zákmitmi
#define ADC_SAMPLES          10     // počet meraní pre priemer (redukcia šumu)
#define WARMUP_SECONDS       60     // s  – zahrievanie MQ senzorov po zapnutí
#define WIFI_ATTEMPTS        40     // [v2.1] koľkokrát skúsi WiFi (×0.5s = 20s)
#define WATCHDOG_TIMEOUT_S   10     // s  – timeout hardvérového watchdogu


// ================================================================
//   NTP – Slovenský čas (automaticky prepína leto/zima CET ↔ CEST)
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
bool   prevGas        = false;   // pre záverečnú správu (typ alarmu)
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

// Priemer z ADC_SAMPLES meraní – znižuje šum senzora.
// Vracia ADC hodnotu (int 0..4095).
int readSmooth(int pin) {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / ADC_SAMPLES);
}

// Vráti aktuálny čas ako text "DD.MM.RRRR HH:MM:SS".
// Ak NTP ešte nie je zosynchronizované, vráti "--.--.---- --:--:--".
String timeNow() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "--.--.---- --:--:--";
  char buf[22];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &ti);
  return String(buf);
}

// Odošle správu do Telegram chatu. Logika: ak nie je WiFi,
// správu nepošle, len zapíše upozornenie do Serial Monitora.
void tgSend(const String& msg) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID_STR, msg, "");
    Serial.println("[TG odoslana] " + msg.substring(0, 80));
  } else {
    Serial.println("[TG] WiFi nedostupne - sprava neodoslana.");
  }
}

// Spustí tón na reproduktore. Ak je alarm umlčaný (/mute alebo
// stlačené tlačidlo), nepípne.
void buzzerPlay(int freq) {
  if (!buzzerMuted) {
    ledcWriteTone(0, freq);    // kanál 0, zvolená frekvencia v Hz
  }
}

// Zastaví reproduktor (PWM kanál 0 → ticho).
void buzzerStop() {
  ledcWriteTone(0, 0);
}

// Pripojí ESP32 k WiFi. Pri zlyhaní reštartuje celý ESP32.
// Počas čakania kŕmi watchdog, aby nevyvolal nežiaduci reštart.
void wifiConnect() {
  Serial.printf("\nWiFi: pripajam k '%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < WIFI_ATTEMPTS && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();     // kŕmenie watchdogu
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
//   SPRACOVANIE TELEGRAM SPRÁV (prichádzajúce príkazy)
// ================================================================
void handleTelegram() {
  int n = bot.getUpdates(bot.last_message_received + 1);

  while (n) {
    for (int i = 0; i < n; i++) {
      String cid  = bot.messages[i].chat_id;
      String text = bot.messages[i].text;
      String from = bot.messages[i].from_name;

      // Bezpečnosť: ignoruj správy od iných používateľov ako je CHAT_ID_STR
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
        // [v2.1] oprava typu "uticany" → "umlcany" (Slovak: umlčaný)
        s += "Zvuk:  " + String(buzzerMuted  ? "umlcany" : "aktivny")  + "\n";
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
        tgSend("Neznamy prikaz. Napis /help");
      }
    }
    n = bot.getUpdates(bot.last_message_received + 1);
  }
}


// ================================================================
//   SETUP  – vykoná sa raz pri zapnutí alebo reštarte
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== DETEKTOR PLYNU A DYMU  v2.1 ===");

  // ── Watchdog timer – reštartuje ESP32 ak sa zasekne ────────────
  //    Ak loop() nezavolá esp_task_wdt_reset() do 10 s,
  //    ESP32 vykoná hardvérový reštart. Ochrana proti WiFi deadlockom.
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);     // pridaj hlavný task (loopTask) do watchdogu
  Serial.println("Watchdog timer aktivovany (10s timeout).");

  // ── Nastavenie pinov ────────────────────────────────────────────
  pinMode(PIN_LED,    OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);   // interný pull-up → tlačidlo pripojené na GND
  digitalWrite(PIN_LED, LOW);

  // ── LEDC (ESP32 PWM) – kanál 0 pre reproduktor ─────────────────
  //    ledcSetup "rezervuje" kanál; ledcWriteTone potom dynamicky
  //    prepne frekvenciu na požadovaný Hz.
  ledcSetup(0, 2000, 8);               // kanál 0, štart 2 kHz, 8-bit rozlíšenie
  ledcAttachPin(PIN_SPEAKER, 0);       // pripojiť GPIO 25 ku kanálu 0
  buzzerStop();

  // ── ADC nastavenie ──────────────────────────────────────────────
  //    12-bit rozlíšenie → hodnoty 0–4095
  //    ADC_11db attenuation → merací rozsah 0–3.3 V
  //    DÔLEŽITÉ: bez ADC_11db by ADC meralo len do ~1 V!
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  Serial.println("ADC: 12-bit rozlisenie, 11dB attenuation (rozsah 0-3.3V).");

  // ── Self-test: 2× bliknutie LED + 2× krátke pípnutie ───────────
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

  // ── SSL certifikát pre Telegram API ─────────────────────────────
  //    TELEGRAM_CERTIFICATE_ROOT je definované v UniversalTelegramBot.h
  secureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // ── Synchronizácia času cez NTP ─────────────────────────────────
  Serial.print("NTP synchronizacia");
  configTzTime(TZ_STRING, NTP_SERVER);
  for (int i = 0; i < 20; i++) {
    struct tm t;
    if (getLocalTime(&t)) break;
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();              // kŕmenie watchdogu
  }
  Serial.println("  " + timeNow());

  // ── Telegram: správa o štarte ───────────────────────────────────
  tgSend("=== DETEKTOR SPUSTENY (v2.1) ===\n"
         "Cas: " + timeNow() + "\n\n"
         "Zahrievam senzory " + String(WARMUP_SECONDS) + " sekund...\n"
         "Prosim cakajte.");

  // ── Zahrievanie MQ senzorov ─────────────────────────────────────
  //    MQ senzory potrebujú ~60 s na stabilizáciu vyhrievacej cievky.
  //    Počas tejto doby sú hodnoty nespoľahlivé – alarm je blokovaný
  //    (v loop() sa tiež do tohto okna nedostaneme).
  //
  //    DÔLEŽITÉ: Pri ÚPLNE PRVOM zapnutí (úplne nové senzory) ich
  //    nechaj bežať 24–48 h pre "burn-in" stabilizáciu odpornej vrstvy.
  Serial.printf("Zahriatie senzorov: %d sekund\n", WARMUP_SECONDS);
  for (int i = WARMUP_SECONDS; i > 0; i--) {
    Serial.printf("  Zostava: %3ds  |  MQ2=%4d  MQ4=%4d  MQ5=%4d\r",
                  i,
                  analogRead(PIN_MQ2),
                  analogRead(PIN_MQ4),
                  analogRead(PIN_MQ5));
    delay(1000);
    esp_task_wdt_reset();              // kŕmenie watchdogu každú sekundu
  }
  Serial.println("\n\nSenzory pripravene! Zacinam monitoring...");

  tgSend("Senzory su zahriate.\n"
         "MONITORING AKTIVNY.\n\n"
         "Napiste /status alebo /help");
}


// ================================================================
//   LOOP  – opakuje sa dookola (~50 ms per iterácia)
// ================================================================
void loop() {
  unsigned long now = millis();

  // Watchdog reset – kŕmime ho každý cyklus. Ak sa niekde zaseknime
  // a tento riadok sa neodvolá do 10 s, ESP32 sa reštartuje.
  esp_task_wdt_reset();

  // ── 1. Čítanie senzorov (priemer z 10 meraní, znižuje šum) ──────
  int v2 = readSmooth(PIN_MQ2);    // dym
  int v4 = readSmooth(PIN_MQ4);    // zemný plyn
  int v5 = readSmooth(PIN_MQ5);    // LPG

  // Debug výpis do Serial Monitora – sledovanie hodnôt pre kalibráciu
  Serial.printf("[%s]  MQ2(dym)=%4d  |  MQ4(gas)=%4d  |  MQ5(LPG)=%4d",
                timeNow().c_str(), v2, v4, v5);
  if (alarmActive) Serial.print("  <<< ALARM >>>");
  Serial.println();

  // ── 2. Vyhodnotenie stavu ───────────────────────────────────────
  smokeDetected = (v2 > THRESHOLD_SMOKE);
  gasDetected   = (v4 > THRESHOLD_GAS4) || (v5 > THRESHOLD_GAS5);
  bool currAlarm = smokeDetected || gasDetected;

  // ── 3. ZAČIATOK ALARMU ──────────────────────────────────────────
  if (currAlarm && !alarmActive) {
    alarmActive    = true;
    buzzerMuted    = false;          // nový alarm vždy znovu pípne
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
    msg += "Pre vypnutie zvuku napis: /mute";

    tgSend(msg);
    Serial.println(">>> ALARM SPUSTENY: " + alarmStartTime);
  }

  // ── 4. KONIEC ALARMU ────────────────────────────────────────────
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

  // Priebežná aktualizácia typu alarmu (ak sa počas alarmu pridá ďalší)
  if (alarmActive) {
    prevGas   = prevGas   || gasDetected;
    prevSmoke = prevSmoke || smokeDetected;
  }

  // ── 5. LED + ZVUK (blikanie + pípanie počas alarmu) ─────────────
  if (alarmActive) {
    if (now - tLastBlink >= BLINK_INTERVAL) {
      tLastBlink = now;
      ledState = !ledState;
      digitalWrite(PIN_LED, ledState);

      if (!buzzerMuted) {
        if (ledState) {
          // "ON" fáza – rôzny tón podľa typu alarmu
          if (gasDetected && smokeDetected) {
            // kombinovaný alarm: striedaj oba tóny (plyn↔dym)
            static bool toneToggle = false;
            toneToggle = !toneToggle;
            buzzerPlay(toneToggle ? TONE_GAS : TONE_SMOKE);
          } else if (gasDetected) {
            buzzerPlay(TONE_GAS);
          } else {
            buzzerPlay(TONE_SMOKE);
          }
        } else {
          buzzerStop();   // "OFF" fáza → efekt beep–pauza–beep
        }
      }
    }
  }

  // ── 6. TLAČIDLO – manuálne vypnutie zvuku ──────────────────────
  bool btnNow = digitalRead(PIN_BUTTON);

  // Zostupná hrana (HIGH → LOW) = práve sa stlačilo tlačidlo,
  // plus debounce ochrana pred viacnásobným registrovaním.
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

  // ── 7. TELEGRAM – kontrola prichádzajúcich správ ────────────────
  if (now - tLastTelegram >= TELEGRAM_INTERVAL) {
    tLastTelegram = now;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi stratene! Obnovujem...");
      wifiConnect();
      secureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    }

    handleTelegram();
  }

  // Krátka pauza – cca 20 cyklov/s, dostatočne rýchle pre detekciu
  delay(50);
}
