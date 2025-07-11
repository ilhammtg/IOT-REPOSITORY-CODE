#define BLYNK_TEMPLATE_ID           "TMPL6Sa42TcKy"
#define BLYNK_TEMPLATE_NAME         "PENGUSIR HAMA"
#define BLYNK_AUTH_TOKEN            "1rnEKN9nZbNHq_mv2JIcw7go61JOOJSj"

#define BLYNK_PRINT Serial

#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <SoftwareSerial.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "ham";
char pass[] = "12345678";

SoftwareSerial EspSerial(2, 3); // RX, TX
#define ESP8266_BAUD 9600
ESP8266 wifi(&EspSerial);

// PIN
const int pirPins[3] = {5, 6, 7};
const int relayPin = 8;

// Sistem dan counter
bool systemEnabled = true;
int relayCounter = 0;

// Waktu reset menggunakan millis()
unsigned long lastResetTime = 0;
const unsigned long RESET_INTERVAL = 86400000UL; // 24 jam (dalam ms)

BLYNK_WRITE(V0) {
  int value = param.asInt();
  systemEnabled = (value == 0); // 0 = aktif, 1 = nonaktif

  if (!systemEnabled) {
    Serial.println("Sistem dinonaktifkan dari Blynk.");
    digitalWrite(relayPin, HIGH);     // Relay OFF
    Blynk.virtualWrite(V1, 0);        // LED mati
  } else {
    Serial.println("Sistem diaktifkan kembali.");
  }
}

void updateCounterDisplay() {
  char counterText[32];
  sprintf(counterText, "Aktif: %dx", relayCounter);
  Blynk.virtualWrite(V2, counterText);
  Serial.print("Counter updated: ");
  Serial.println(counterText);
}

void setup() {
  Serial.begin(9600);
  EspSerial.begin(ESP8266_BAUD);
  Blynk.begin(auth, wifi, ssid, pass);

  for (int i = 0; i < 3; i++) {
    pinMode(pirPins[i], INPUT);
  }

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // Awal: relay mati
  Blynk.virtualWrite(V1, 0);     // LED mati
  updateCounterDisplay();        // Tampilkan counter awal

  lastResetTime = millis();      // Set awal waktu reset
  
  Serial.println("Sistem pengusir hama siap!");
  Serial.println("Counter akan direset otomatis setiap 24 jam");
}

void loop() {
  Blynk.run();

  // Reset counter setiap 24 jam
  if (millis() - lastResetTime >= RESET_INTERVAL) {
    relayCounter = 0;
    updateCounterDisplay();
    lastResetTime = millis();
    Serial.println("Counter direset otomatis setiap 24 jam");
  }

  // Jika sistem dinonaktifkan dari Blynk
  if (!systemEnabled) {
    return;
  }

  // Cek apakah ada gerakan dari salah satu sensor PIR
  bool motionDetected = false;
  for (int i = 0; i < 3; i++) {
    if (digitalRead(pirPins[i]) == HIGH) {
      motionDetected = true;
      break;
    }
  }

  // Jika ada gerakan
  if (motionDetected) {
    // Tambahkan counter SEBELUM aktivasi relay
    relayCounter++;
    
    Serial.print("Gerakan terdeteksi! Aktivasi ke-");
    Serial.print(relayCounter);
    Serial.println(" - Relay ON 5 detik...");
    
    // Aktifkan relay dan LED
    digitalWrite(relayPin, LOW);        // Relay ON
    Blynk.virtualWrite(V1, 255);        // LED ON
    
    // Update counter display di Blynk
    updateCounterDisplay();

    delay(5000);                        // Aktif 5 detik

    Serial.println("Relay OFF");
    digitalWrite(relayPin, HIGH);       // Relay OFF
    Blynk.virtualWrite(V1, 0);          // LED OFF

    delay(3000); // Jeda 3 detik sebelum baca sensor lagi
  }
}
