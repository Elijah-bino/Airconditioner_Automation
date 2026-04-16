// AC IR Auto Detector using Known Examples
// Uses fingerprint matching for POWER_ON / POWER_OFF

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

#define IR_PIN D6

IRrecv irrecv(IR_PIN);
decode_results results;

unsigned long lastPressTime = 0;

// Known good raw examples (add more as you capture)
const uint16_t powerOnRaw1[100] PROGMEM = {3488,24,12,14,22,1232,692,22,28,52,698,24,40,14,828,692,22,28,54,694,22,26,24,28,698,20,26,24,834,690,22,26,60,690,22,12,12,28,834,688,24,12,12,26,30,692,28,12,24,12,30,690,24,14,12,24,838,688,22,16,16,20,836,686,28,16,22,16,834,684,24,16,16,18,38,660,22,16,16,20,24,844,682,32,16,16,16,32,684,24,16,16,18,844,680,24,12,14,22};
const uint16_t powerOffRaw1[100] PROGMEM = {3478,24,14,12,24,1240,658,24,14,12,24,60,688,24,14,12,24,840,684,22,14,12,24,24,12,688,22,24,24,38,686,24,12,12,26,842,680,24,14,12,22,40,684,24,14,12,22,846,680,24,12,14,22,42,658,22,14,12,22,68,654,28,14,12,896,650,24,14,12,22,874,650,24,16,16,18,876,698,16,16,18,46,654,24,16,16,18,878,646,24,16,16,18,74,674,16,16,18,880};

String getButtonLabel(const decode_results &res) {
  if (res.rawlen < 90) return "UNKNOWN";

  uint32_t bestScore = 999999;
  String bestLabel = "UNKNOWN";

  // Compare with POWER_ON example
  uint32_t scoreOn = 0;
  for (uint16_t i = 1; i < res.rawlen && i < 100; i++) {
    uint16_t expected = pgm_read_word(&powerOnRaw1[i-1]);
    scoreOn += abs((int)res.rawbuf[i] * RAWTICK - (int)expected);
  }
  if (scoreOn < bestScore) {
    bestScore = scoreOn;
    bestLabel = "POWER_ON";
  }

  // Compare with POWER_OFF example
  uint32_t scoreOff = 0;
  for (uint16_t i = 1; i < res.rawlen && i < 100; i++) {
    uint16_t expected = pgm_read_word(&powerOffRaw1[i-1]);
    scoreOff += abs((int)res.rawbuf[i] * RAWTICK - (int)expected);
  }
  if (scoreOff < bestScore) {
    bestScore = scoreOff;
    bestLabel = "POWER_OFF";
  }

  // Simple threshold - adjust if needed
  if (bestScore > 8000) bestLabel = "OTHER_BUTTON";

  return bestLabel;
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  irrecv.enableIRIn();

  Serial.println(F("\n=== AC Auto Detector Started ==="));
  Serial.println(F("Press Power button or any button..."));
  Serial.println(F("It will try to auto-detect POWER_ON / POWER_OFF\n"));
}

void loop() {
  if (irrecv.decode(&results)) {
    if (millis() - lastPressTime < 400) {
      irrecv.resume();
      return;
    }
    lastPressTime = millis();

    String label = getButtonLabel(results);

    Serial.println(F("\n=== SIGNAL RECEIVED ==="));
    Serial.print(F("Auto Label   : "));
    Serial.println(label);
    Serial.print(F("Signal Hash  : 0x"));
    Serial.println(results.value, HEX);   // for reference

    // Show raw (for debugging)
    Serial.print(F("Raw["));
    Serial.print(results.rawlen - 1);
    Serial.print(F("]: "));
    for (uint16_t i = 1; i < results.rawlen; i++) {
      Serial.print(results.rawbuf[i] * RAWTICK);
      if (i < results.rawlen - 1) Serial.print(", ");
    }
    Serial.println(F("\n======================================\n"));

    irrecv.resume();
  }
}