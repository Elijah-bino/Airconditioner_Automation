// AC Button Controller — Wemos D1 Mini
// Button 1 (D1) → ON/OFF toggle
// Button 2 (D5) → Mode toggle (Cool ↔ Heat)
// Button 3 (D6) → Eco/Powerful toggle
// IR LED → D4 (physical) = D2 in code
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_MitsubishiHeavy.h>

// Pins
#define IR_PIN  D2  // physical D4
#define BTN1    D1  // ON/OFF
#define BTN2    D3  // Cool/Heat
#define BTN3    D4  // Eco/Powerful
#define LED     LED_BUILTIN

// AC state
bool   acPower   = false;
bool   isCool    = true;   // true=Cool, false=Heat
int    ecoMode   = 0;      // 0=Normal, 1=Eco, 2=Powerful

IRMitsubishiHeavy152Ac ac(IR_PIN);

void blink(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED, LOW);
    delay(150);
    digitalWrite(LED, HIGH);
    delay(150);
  }
}

void sendAC() {
  for (int i = 0; i < 3; i++) {
    ac.send();
    delay(50);
  }
  Serial.println("Signal sent!");
}

void printState() {
  Serial.print("Power: ");    Serial.println(acPower ? "ON" : "OFF");
  Serial.print("Mode : ");    Serial.println(isCool ? "COOL" : "HEAT");
  Serial.print("Extra: ");
  if      (ecoMode == 1) Serial.println("ECO");
  else if (ecoMode == 2) Serial.println("POWERFUL");
  else                   Serial.println("NORMAL");
  Serial.println("---");
}

void applyState() {
  ac.setPower(acPower);
  ac.setMode(isCool ? kMitsubishiHeavyCool : kMitsubishiHeavyHeat);
  ac.setTemp(24);
  ac.setFan(kMitsubishiHeavy152FanAuto);
  ac.setEcono(ecoMode == 1);
  ac.setTurbo(ecoMode == 2);
  printState();
  sendAC();
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(LED,  OUTPUT);
  digitalWrite(LED, HIGH);

  ac.begin();
  ac.setSwingVertical(kMitsubishiHeavy152SwingVAuto);
  ac.setSwingHorizontal(kMitsubishiHeavy152SwingHOff);
  ac.setNight(false);

  Serial.println("AC Controller ready!");
  Serial.println("BTN1 = ON/OFF | BTN2 = Cool/Heat | BTN3 = Normal/Eco/Powerful");
}

void loop() {
  // Button 1 — ON/OFF toggle
  if (digitalRead(BTN1) == LOW) {
    acPower = !acPower;
    Serial.print("BTN1: Power ");
    Serial.println(acPower ? "ON" : "OFF");
    blink(1);
    applyState();
    delay(500); // debounce
  }

  // Button 2 — Cool/Heat toggle (only if power is ON)
  if (digitalRead(BTN2) == LOW) {
    if (acPower) {
      isCool = !isCool;
      Serial.print("BTN2: Mode → ");
      Serial.println(isCool ? "COOL" : "HEAT");
      blink(2);
      applyState();
    } else {
      Serial.println("BTN2: AC is OFF, turn on first!");
      blink(5); // rapid blink = error
    }
    delay(500);
  }

  // Button 3 — Normal → Eco → Powerful → Normal cycle
  if (digitalRead(BTN3) == LOW) {
    if (acPower) {
      ecoMode = (ecoMode + 1) % 3;
      Serial.print("BTN3: Extra → ");
      if      (ecoMode == 0) Serial.println("NORMAL");
      else if (ecoMode == 1) Serial.println("ECO");
      else                   Serial.println("POWERFUL");
      blink(3);
      applyState();
    } else {
      Serial.println("BTN3: AC is OFF, turn on first!");
      blink(5);
    }
    delay(500);
  }
}