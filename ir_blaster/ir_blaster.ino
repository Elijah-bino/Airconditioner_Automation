// Mitsubishi Heavy Industries AC Controller
// Board: Wemos D1 Mini (ESP8266)
// Remote: RLA502A700B
// Library: IRremoteESP8266 by crankyoldgit
// IR LED: long leg → D2, short leg → GND
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_MitsubishiHeavy.h>

const uint16_t IR_PIN = D2;
IRMitsubishiHeavy152Ac ac(IR_PIN);

void setup() {
  Serial.begin(9600);
  ac.begin();
  ac.setMode(kMitsubishiHeavyCool);
  ac.setTemp(24);
  ac.setFan(kMitsubishiHeavy152FanAuto);
  ac.setSwingVertical(kMitsubishiHeavy152SwingVAuto);
  ac.setSwingHorizontal(kMitsubishiHeavy152SwingHOff);
  ac.setTurbo(false);
  ac.setEcono(false);
  ac.setNight(false);
  Serial.println("Mitsubishi Heavy AC ready!");
  Serial.println("Commands: POWER_ON  POWER_OFF  COOL  HEAT  FAN  AUTO");
}

void sendAC() {
  for (int i = 0; i < 3; i++) {
    ac.send();
    delay(50);
  }
  Serial.println("Sent!");
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "POWER_ON") {
      Serial.println("Sending: Power ON, Cool 24C...");
      ac.setPower(true);
      ac.setMode(kMitsubishiHeavyCool);
      ac.setTemp(24);
      sendAC();
    }
    else if (cmd == "POWER_OFF") {
      Serial.println("Sending: Power OFF...");
      ac.setPower(false);
      sendAC();
    }
    else if (cmd == "COOL") {
      Serial.println("Sending: Cool 24C...");
      ac.setPower(true);
      ac.setMode(kMitsubishiHeavyCool);
      ac.setTemp(24);
      sendAC();
    }
    else if (cmd == "HEAT") {
      Serial.println("Sending: Heat 24C...");
      ac.setPower(true);
      ac.setMode(kMitsubishiHeavyHeat);
      ac.setTemp(24);
      sendAC();
    }
    else if (cmd == "FAN") {
      Serial.println("Sending: Fan only...");
      ac.setPower(true);
      ac.setMode(kMitsubishiHeavyFan);
      sendAC();
    }
    else if (cmd == "AUTO") {
      Serial.println("Sending: Auto 24C...");
      ac.setPower(true);
      ac.setMode(kMitsubishiHeavyAuto);
      ac.setTemp(24);
      sendAC();
    }
    else {
      Serial.println("Unknown. Try: POWER_ON POWER_OFF COOL HEAT FAN AUTO");
    }
  }
}