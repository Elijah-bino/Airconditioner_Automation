#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <IRremoteESP8266.h>
#include <ir_MitsubishiHeavy.h>
#include <DHT.h>

// ===== CONFIGURE THESE =====
#define WIFI_SSID     
#define WIFI_PASSWORD 
#define API_KEY       
#define DATABASE_URL  
// ===========================

// Pins
#define IR_PIN      D2
#define BTN_ONOFF   D1
#define BTN_MODE    D3
#define BTN_ECO     D4
#define DHT_PIN     D5
#define LED_RED     D6
#define LED_GREEN   D7
#define BUILTIN_LED LED_BUILTIN

#define DHT_TYPE    DHT11
#define LOG_INTERVAL 900000   // 15 min
#define RECONNECT_INTERVAL 1800000  // 30 minutes

IRMitsubishiHeavy152Ac ac(IR_PIN);
DHT dht(DHT_PIN, DHT_TYPE);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// AC State
bool acPower = false;
bool isCool  = true;
int  ecoMode = 0;

bool firebaseReady = false;
unsigned long logCount = 0;
unsigned long lastLogTime = 0;
unsigned long lastReconnectTime = 0;

// ===== LED FUNCTIONS =====
void blinkBuiltin(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUILTIN_LED, LOW);
    delay(150);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(150);
  }
}

void blinkBothOnce() {
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, HIGH);
  delay(200);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
  delay(200);
}

void blinkLED(int pin, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

// ===== GET LAST STATE FROM FIREBASE =====
void getLastStateFromFirebase() {
  Serial.println("Getting last state from Firebase...");
  
  // First try to get the counter
  if (Firebase.RTDB.getInt(&fbdo, "/ac_logs_counter")) {
    unsigned long lastCounter = fbdo.intData();
    Serial.printf("Last counter value: %lu\n", lastCounter);
    
    if (lastCounter > 0) {
      // Get the last log entry (counter-1 because counter points to NEXT entry)
      String lastLogPath = "/ac_logs/" + String(lastCounter - 1);
      Serial.printf("Reading last log from: %s\n", lastLogPath.c_str());
      
      if (Firebase.RTDB.getJSON(&fbdo, lastLogPath.c_str())) {
        FirebaseJson &json = fbdo.jsonObject();
        FirebaseJsonData jsonData;
        
        // Extract and restore power state
        json.get(jsonData, "power");
        if (jsonData.success) {
          acPower = jsonData.boolValue;
          Serial.printf("✅ Restored power: %s\n", acPower ? "ON" : "OFF");
        }
        
        // Extract and restore mode
        json.get(jsonData, "mode");
        if (jsonData.success) {
          String mode = jsonData.stringValue;
          isCool = (mode == "COOL");
          Serial.printf("✅ Restored mode: %s\n", isCool ? "COOL" : "HEAT");
        }
        
        // Extract and restore eco/powerful
        json.get(jsonData, "eco");
        if (jsonData.success && jsonData.boolValue) {
          ecoMode = 1;
          Serial.println("✅ Restored eco: ECO");
        } else {
          json.get(jsonData, "powerful");
          if (jsonData.success && jsonData.boolValue) {
            ecoMode = 2;
            Serial.println("✅ Restored eco: POWERFUL");
          } else {
            ecoMode = 0;
            Serial.println("✅ Restored eco: NORMAL");
          }
        }
        
        // CRITICAL: If AC was ON, sync it so buttons work
        if (acPower) {
          Serial.println("AC was ON according to database, syncing...");
          sendAC();  // This ensures the AC is in the right state
          Serial.println("✅ AC synced with last known state");
        } else {
          Serial.println("AC was OFF according to database");
        }
        
        return;
      } else {
        Serial.println("Failed to read last log entry");
      }
    } else {
      Serial.println("No previous logs found");
    }
  } else {
    Serial.println("Failed to read counter");
  }
  
  // If no previous state found, use defaults
  Serial.println("No previous state found, using defaults (AC OFF)");
  acPower = false;
  isCool = true;
  ecoMode = 0;
}

// ===== RECONNECT FUNCTION =====
void reconnectWiFiAndFirebase() {
  Serial.println("\n=== Reconnecting WiFi & Firebase ===");
  
  // Disconnect WiFi
  WiFi.disconnect();
  delay(1000);
  
  // Reconnect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Reconnecting WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Reconnected!");
    
    // Reinitialize Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;
    
    if (Firebase.signUp(&config, &auth, "", "")) {
      firebaseReady = true;
      Serial.println("Firebase Ready Again!");
      
      Firebase.begin(&config, &auth);
      Firebase.reconnectWiFi(true);
      
      delay(1000);
      
      // Reload counter
      initCounter();
      
      // Restore state from Firebase
      getLastStateFromFirebase();
    } else {
      firebaseReady = false;
      Serial.println("Firebase Re-initialization Failed!");
    }
  } else {
    Serial.println("\nWiFi Reconnection Failed!");
    firebaseReady = false;
  }
  
  lastReconnectTime = millis();
}

// ===== GET HIGHEST KEY =====
unsigned long getHighestKey() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = String(DATABASE_URL) + "/ac_logs.json?shallow=true";
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String response = http.getString();
    http.end();
    
    unsigned long maxKey = 0;
    String numStr = "";
    
    for (int i = 0; i < response.length(); i++) {
      char c = response[i];
      if (c >= '0' && c <= '9') {
        numStr += c;
      } else if (numStr.length() > 0) {
        unsigned long num = numStr.toInt();
        if (num > maxKey) maxKey = num;
        numStr = "";
      }
    }
    if (numStr.length() > 0) {
      unsigned long num = numStr.toInt();
      if (num > maxKey) maxKey = num;
    }
    
    Serial.printf("Highest key found: %lu\n", maxKey);
    return maxKey;
  }
  
  http.end();
  return 0;
}

// ===== INITIALIZE COUNTER =====
void initCounter() {
  Serial.println("Initializing counter...");
  
  if (Firebase.RTDB.getInt(&fbdo, "/ac_logs_counter")) {
    unsigned long savedCounter = fbdo.intData();
    Serial.printf("Found counter in Firebase: %lu\n", savedCounter);
    
    String testPath = "/ac_logs/" + String(savedCounter);
    if (Firebase.RTDB.getJSON(&fbdo, testPath.c_str())) {
      logCount = savedCounter;
      Serial.printf("Using counter from Firebase: %lu\n", logCount);
      return;
    }
  }
  
  Serial.println("Searching for highest key...");
  unsigned long highestKey = getHighestKey();
  
  if (highestKey > 0) {
    logCount = highestKey + 1;
    Serial.printf("Found existing keys up to %lu, next will be: %lu\n", highestKey, logCount);
  } else {
    logCount = 0;
    Serial.println("No existing logs, starting from 0");
  }
  
  Firebase.RTDB.setInt(&fbdo, "/ac_logs_counter", logCount);
  Serial.printf("Counter saved to Firebase: %lu\n", logCount);
}

// ===== FIREBASE =====
bool pushToFirebase() {
  if (!firebaseReady) return false;

  float roomTemp = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(roomTemp)) roomTemp = -1;
  if (isnan(humidity)) humidity = -1;

  FirebaseJson json;
  json.set("power",      acPower);
  json.set("mode",       isCool ? "COOL" : "HEAT");
  json.set("eco",        ecoMode == 1);
  json.set("powerful",   ecoMode == 2);
  json.set("temp_set",   24);
  json.set("temp_room",  roomTemp);
  json.set("humidity",   humidity);
  json.set("timestamp",  millis());

  String path = "/ac_logs/" + String(logCount);

  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
    Serial.printf("✅ Firebase OK [%lu] | %s | %s | eco:%d | Temp:%.1f°C\n",
                  logCount,
                  acPower ? "ON" : "OFF",
                  isCool ? "COOL" : "HEAT",
                  ecoMode,
                  roomTemp);

    logCount++;
    Firebase.RTDB.setInt(&fbdo, "/ac_logs_counter", logCount);
    lastLogTime = millis();
    return true;
  } else {
    Serial.print("❌ Firebase Error: ");
    Serial.println(fbdo.errorReason());
    return false;
  }
}

// ===== AC CONTROL =====
void sendAC() {
  ac.setPower(acPower);
  ac.setMode(isCool ? kMitsubishiHeavyCool : kMitsubishiHeavyHeat);
  ac.setTemp(24);
  ac.setFan(kMitsubishiHeavy152FanAuto);
  ac.setEcono(ecoMode == 1);
  ac.setTurbo(ecoMode == 2);
  ac.setSwingVertical(kMitsubishiHeavy152SwingVAuto);
  ac.setSwingHorizontal(kMitsubishiHeavy152SwingHOff);

  for (int i = 0; i < 3; i++) {
    ac.send();
    delay(80);
  }

  Serial.println("IR Command Sent");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(BTN_ONOFF, INPUT_PULLUP);
  pinMode(BTN_MODE,  INPUT_PULLUP);
  pinMode(BTN_ECO,   INPUT_PULLUP);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(BUILTIN_LED, HIGH);

  ac.begin();
  dht.begin();

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  blinkBothOnce();

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    firebaseReady = true;
    Serial.println("Firebase Ready!");
  } else {
    Serial.println("Firebase SignUp Failed!");
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  delay(1000);
  
  if (firebaseReady) {
    initCounter();
    getLastStateFromFirebase();  // Restore state so buttons work!
  }

  lastLogTime = millis();
  lastReconnectTime = millis();
  Serial.printf("Starting with counter: %lu\n", logCount);
  Serial.println("=== AC Controller Ready ===\n");
}

// ===== LOOP =====
void loop() {

  // Check and reconnect every 30 minutes
  if (millis() - lastReconnectTime >= RECONNECT_INTERVAL) {
    reconnectWiFiAndFirebase();
  }

  // ===== ON/OFF =====
  if (digitalRead(BTN_ONOFF) == LOW) {
    acPower = !acPower;

    Serial.println(acPower ? "→ Power ON" : "→ Power OFF");

    blinkBuiltin(2);

    sendAC();

    bool success = pushToFirebase();

    if (!success) blinkLED(LED_RED, 3);

    delay(600);
  }

  // ===== MODE =====
  if (digitalRead(BTN_MODE) == LOW) {
    if (acPower) {  // Now this will work because acPower was restored!
      isCool = !isCool;

      Serial.println(isCool ? "→ COOL" : "→ HEAT");

      blinkBuiltin(1);

      sendAC();

      bool success = pushToFirebase();

      if (success) {
        if (isCool) blinkLED(LED_GREEN, 1);
        else        blinkLED(LED_RED, 1);
      } else {
        blinkLED(LED_RED, 3);
      }
    } else {
      Serial.println("→ AC is OFF, press ON/OFF first");
    }
    delay(600);
  }

  // ===== ECO / POWERFUL =====
  if (digitalRead(BTN_ECO) == LOW) {
    if (acPower) {  // Now this will work because acPower was restored!
      ecoMode = (ecoMode + 1) % 3;

      Serial.print("→ ");
      Serial.println(ecoMode == 0 ? "NORMAL" :
                     ecoMode == 1 ? "ECO" : "POWERFUL");

      blinkBuiltin(1);

      sendAC();

      bool success = pushToFirebase();

      if (success) {
        if (ecoMode == 1) {
          blinkLED(LED_GREEN, 2);
        } else if (ecoMode == 2) {
          blinkLED(LED_RED, 2);
        }
      } else {
        blinkLED(LED_RED, 3);
      }
    } else {
      Serial.println("→ AC is OFF, press ON/OFF first");
    }
    delay(600);
  }

  // ===== AUTO LOG =====
  if (firebaseReady && (millis() - lastLogTime >= LOG_INTERVAL)) {
    Serial.println("→ 15 min auto log");

    bool success = pushToFirebase();

    if (!success) blinkLED(LED_RED, 3);
  }

  delay(50);
}