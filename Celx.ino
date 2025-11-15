// === Blynk Template ===
#define BLYNK_TEMPLATE_ID "TMPL6rKTEvFKg"
#define BLYNK_TEMPLATE_NAME "Quickstart Device"
#define BLYNK_AUTH_TOKEN "AyQwCkTyempof3n56d1MzagUe1isX4bp"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include <BlynkSimpleEsp8266.h>


// === WiFi dan Telegram ===
const char* ssid = "";
const char* password = "";
const char* botToken = "";
const String chatId = "";

// === Blynk status control ===
bool sistemAktif = true; // dikontrol dari tombol Blynk

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // GMT+7

// === Pin definisi ===
#define TRIG_PIN D5
#define ECHO_PIN D0
#define BUZZER_PIN D3
#define LED_BUILTIN_PIN D4

// DFPlayer
SoftwareSerial mySerial(D6, D7);
DFRobotDFPlayerMini myDFPlayer;

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === Status & timing ===
bool alreadyPlayed = false;
bool audio2Played = false;
bool morsePlaying = false;
bool morseSymbolOn = false;

unsigned long lastTriggerTime = 0;
unsigned long audioStartTime = 0;
unsigned long lastPrintTime = 0;
unsigned long morseLastTime = 0;
unsigned long morseDelay = 0;

const unsigned long cooldownDuration = 60000;
const unsigned long delayToSecondAudio = 12000;
const unsigned long printInterval = 500;

const unsigned int unit = 60;
const unsigned int dotDuration = unit;
const unsigned int dashDuration = 3 * unit;
const unsigned int symbolSpace = unit;
const unsigned int letterSpace = 3 * unit;
const unsigned int wordSpace = 7 * unit;

String morseCode = "... . .-.. .- -- .- -   -.. .- - .- -. --.   -.. ..   - --- -.- ---   ... .- .--. - ---";
int morseIndex = 0;

// === BLYNK WRITE untuk kontrol sistem ===
BLYNK_WRITE(V0) {
  int val = param.asInt();
  sistemAktif = (val == 1);
  Serial.println(sistemAktif ? "Sistem AKTIF" : "Sistem NONAKTIF");
}

// === Fungsi kirim Telegram ===
void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage";
    String payload = "chat_id=" + chatId + "&text=" + message;

    http.begin(client, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
      Serial.println("Pesan terkirim.");
    } else {
      Serial.print("Gagal kirim: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi belum tersambung!");
  }
}

// === OLED Startup ===
void showStartupAnimation() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor((SCREEN_WIDTH - (4 * 12)) / 2, 10);
  display.println("Celx");
  display.display();
  delay(1000);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((SCREEN_WIDTH - (5 * 12)) / 2, 10);
  display.println("Alarm");
  display.display();
  delay(1000);

  for (int i = 0; i <= 100; i += 10) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor((SCREEN_WIDTH - (7 * 6)) / 2, 5);
    display.println("Loading");
    display.drawRect(14, 50, 100, 8, WHITE);
    display.fillRect(14, 50, i, 8, WHITE);
    display.display();
    delay(100);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor((SCREEN_WIDTH - (12 * 6)) / 2, 28);
  display.println("Sistem Siap!");
  display.display();
  delay(1000);
}

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_BUILTIN_PIN, OUTPUT);

  Wire.begin(D2, D1);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  showStartupAnimation();

  mySerial.begin(9600);
  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer gagal!");
    while (true);
  }
  myDFPlayer.volume(30);

  WiFi.begin(ssid, password);
  Serial.print("Menyambung WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Tersambung!");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  timeClient.begin();
}

void loop() {
  Blynk.run();
  timeClient.update();
  unsigned long currentMillis = millis();

  if (!sistemAktif) {
    display.clearDisplay();
    display.display();
    digitalWrite(LED_BUILTIN_PIN, HIGH);
    return;
  }

  // Sensor ultrasonik
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = duration * 0.034 / 2;
  bool objectDetected = (distance > 0 && distance < 50);

  if (currentMillis - lastPrintTime >= printInterval) {
    Serial.print("Jarak: ");
    Serial.print(distance);
    Serial.println(" cm");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor((SCREEN_WIDTH - (9 * 6)) / 2, 0);
    display.println("CelxAlarm");

    if (alreadyPlayed && (currentMillis / 300) % 2 == 0) {
      display.fillTriangle(118, 2, 118, 10, 124, 6, WHITE);
      display.drawLine(125, 4, 125, 8, WHITE);
      display.drawLine(126, 3, 126, 9, WHITE);
    }

    display.setTextSize(2);
    display.setCursor((SCREEN_WIDTH - (6 * 12)) / 2, 28);
    display.print(distance, 1);
    display.println("cm");

    display.display();
    lastPrintTime = currentMillis;
  }

  if (objectDetected) {
    digitalWrite(LED_BUILTIN_PIN, LOW);

    if (!alreadyPlayed && (currentMillis - lastTriggerTime > cooldownDuration)) {
      myDFPlayer.play(1);
      audioStartTime = currentMillis;
      audio2Played = false;
      alreadyPlayed = true;
      lastTriggerTime = currentMillis;

      morsePlaying = true;
      morseIndex = 0;
      morseLastTime = currentMillis;
      morseSymbolOn = false;

      Serial.println("Audio 1 diputar dan Morse dimulai.");

      // Kirim Telegram
      String t = timeClient.getFormattedTime();
      String msg = "Ada tamu terdeteksi di toko!\nJarak: " + String(distance, 1) + " cm\nJam: " + t;
      sendTelegramMessage(msg);
    }

    if (alreadyPlayed && !audio2Played && (currentMillis - audioStartTime > delayToSecondAudio)) {
      myDFPlayer.play(2);
      audio2Played = true;
      Serial.println("Audio 2 diputar.");
    }

  } else {
    digitalWrite(LED_BUILTIN_PIN, HIGH);
  }

  if (alreadyPlayed && (currentMillis - lastTriggerTime > cooldownDuration)) {
    alreadyPlayed = false;
    audio2Played = false;
  }

  // Eksekusi Morse
  if (morsePlaying && (currentMillis - morseLastTime >= morseDelay)) {
    if (morseIndex < morseCode.length()) {
      char symbol = morseCode[morseIndex];

      if (!morseSymbolOn) {
        if (symbol == '.') {
          digitalWrite(BUZZER_PIN, HIGH);
          morseDelay = dotDuration;
        } else if (symbol == '-') {
          digitalWrite(BUZZER_PIN, HIGH);
          morseDelay = dashDuration;
        } else if (symbol == ' ') {
          digitalWrite(BUZZER_PIN, LOW);
          morseDelay = wordSpace;
          morseIndex++;
          morseLastTime = currentMillis;
          return;
        } else {
          morseDelay = letterSpace;
          morseIndex++;
          morseLastTime = currentMillis;
          return;
        }
        morseSymbolOn = true;
      } else {
        digitalWrite(BUZZER_PIN, LOW);
        morseDelay = symbolSpace;
        morseSymbolOn = false;
        morseIndex++;
      }
      morseLastTime = currentMillis;
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      morsePlaying = false;
      Serial.println("Morse selesai.");
    }
  }

  delay(10);
}
