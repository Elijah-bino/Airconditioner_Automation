// Wemos D1 Mini - Button / Pin Mapping Test
// Press each button and see which pin is triggered

// (WHATS WRITTEN) // (LOGICAL PIN)
// D0 //
// D1 //
// D2 //
// D3 // 1
// D4 // 2
// D5 // 5
// D6 // 6
// D7 // 7
// D8 // 3
// D9 // 4
// D11 // 7
// D13 // 5
// D14 // 2
// D15 // 1

#define LED LED_BUILTIN

// List of pins we want to test
const int testPins[] = {D1, D2, D3, D4, D5, D6, D7, D8};
const String pinNames[] = {"D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9"};

const int numPins = 8;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);   // LED off initially

  // Set all test pins as INPUT_PULLUP
  for (int i = 0; i < numPins; i++) {
    pinMode(testPins[i], INPUT_PULLUP);
  }

  Serial.println("\n=== Wemos D1 Mini Pin Test ===");
  Serial.println("Press any button connected to the pins below:");
  Serial.println("D1  D2  D3  D4  D5  D6  D7  D8");
  Serial.println("Ready...\n");
}

void loop() {
  for (int i = 0; i < numPins; i++) {
    if (digitalRead(testPins[i]) == LOW) {        // Button pressed
      Serial.print("Button Pressed → Pin: ");
      Serial.println(pinNames[i]);

      // Blink LED once
      digitalWrite(LED, LOW);   // LED ON
      delay(150);
      digitalWrite(LED, HIGH);  // LED OFF

      delay(400);               // Debounce delay
      break;                    // Exit loop after one detection
    }
  }
}