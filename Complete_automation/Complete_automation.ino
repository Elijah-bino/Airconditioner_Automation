#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <IRremoteESP8266.h>
#include <ir_MitsubishiHeavy.h>
#include <DHT.h>

// ===== CONFIGURE THESE =====
#define WIFI_SSID     "NETGEAR61"
#define WIFI_PASSWORD "freshoctopus730"
#define API_KEY       "AIzaSyBlGQ2pO2BQPXaBqYTZUIqxiW0e5or5los"
#define DATABASE_URL  "https://ac-logger-bf8e2-default-rtdb.asia-southeast1.firebasedatabase.app/"
// ===========================

// Pins
#define IR_PIN      D2
#define DHT_PIN     D5
#define LED_RED     D6
#define LED_GREEN   D7
#define BUILTIN_LED LED_BUILTIN

#define DHT_TYPE DHT11

// Intervals
#define COMMAND_INTERVAL 600000     // 10 minutes (600,000 ms)
#define SENSOR_INTERVAL    300000   // 5 minutes
#define RECONNECT_INTERVAL 1800000  // 30 minutes
#define TOKEN_REFRESH_INTERVAL 3540000 // 59 minutes

// Objects
IRMitsubishiHeavy152Ac ac(IR_PIN);
DHT dht(DHT_PIN, DHT_TYPE);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// State
bool acPower = true;      // Always keep AC powered ON (user controls power manually)
bool isCool  = false;     // Default to HEAT (false = HEAT, true = COOL)
int  ecoMode = 0;         // 0 = OFF, 1 = ECO, 2 = POWERFUL

bool firebaseReady = false;
bool initialSensorSent = false;

unsigned long lastCommandFetch = 0;
unsigned long lastSensorPush   = 0;
unsigned long lastReconnect    = 0;
unsigned long lastTokenRefresh = 0;
unsigned long logIndex         = 0;


// ===== LED FUNCTIONS =====
void blinkBuiltin(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUILTIN_LED, LOW);
    delay(150);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(150);
  }
}

void blinkLED(int pin, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

void blinkBothOnce() {
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, HIGH);
  delay(200);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
}


// ===== AC CONTROL =====
void sendAC() {
  ac.setPower(acPower);  // Always true, user controls power manually via remote
  ac.setMode(isCool ? kMitsubishiHeavyCool : kMitsubishiHeavyHeat);
  ac.setTemp(24);
  ac.setFan(kMitsubishiHeavy152FanAuto);
  ac.setEcono(ecoMode == 1);
  ac.setTurbo(ecoMode == 2);
  ac.setSwingVertical(kMitsubishiHeavy152SwingVAuto);

  for (int i = 0; i < 3; i++) {
    ac.send();
    delay(80);
  }

  Serial.println("✅ IR Sent!");
  Serial.printf("   Power: %s | Mode: %s | Eco: %s | Powerful: %s\n",
                acPower ? "ON" : "OFF",
                isCool ? "COOL" : "HEAT",
                ecoMode == 1 ? "ON" : "OFF",
                ecoMode == 2 ? "ON" : "OFF");
}


// ===== REFRESH TOKEN =====
void refreshFirebaseToken() {
  Serial.println("🔄 Refreshing Firebase token...");
  
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  if (Firebase.signUp(&config, &auth, "", "")) {
    firebaseReady = true;
    Serial.println("✅ Firebase token refreshed successfully");
    blinkLED(LED_GREEN, 2);
    
    // Reinitialize Firebase with new token
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
  } else {
    Serial.println("❌ Token refresh failed");
    Serial.println(fbdo.errorReason());
    blinkLED(LED_RED, 3);
    firebaseReady = false;
  }
  
  lastTokenRefresh = millis();
}


// ===== FETCH COMMANDS AND SEND IR IMMEDIATELY =====
void fetchAndSendCommands() {
  if (!firebaseReady) {
    Serial.println("⚠️ fetchCommands: Firebase not ready, skipping");
    return;
  }

  Serial.println("\n🔍 Fetching commands from Firebase...");

  if (Firebase.RTDB.getJSON(&fbdo, "/commands")) {
    FirebaseJson &json = fbdo.jsonObject();
    FirebaseJsonData data;
    
    // Debug: Print what we received
    String jsonStr;
    json.toString(jsonStr, true);
    Serial.print("📥 Received: ");
    Serial.println(jsonStr);

    bool newIsCool = isCool;
    int  newEcoMode = ecoMode;
    bool commandReceived = false;

    // MODE (0 = HEAT, 1 = COOL)
    json.get(data, "mode");
    if (data.success) {
      Serial.printf("   mode = %d\n", data.intValue);
      newIsCool = (data.intValue == 1);
      commandReceived = true;
    } else {
      Serial.println("   mode: NOT FOUND, using default (HEAT)");
      newIsCool = false;  // Default to HEAT
    }

    // ECO
    json.get(data, "eco");
    if (data.success) {
      Serial.printf("   eco = %d\n", data.intValue);
      if (data.intValue == 1) {
        newEcoMode = 1;
        commandReceived = true;
      }
    }

    // POWERFUL overrides eco
    json.get(data, "powerful");
    if (data.success) {
      Serial.printf("   powerful = %d\n", data.intValue);
      if (data.intValue == 1) {
        newEcoMode = 2;
        commandReceived = true;
      }
    }

    if (!commandReceived) {
      Serial.println("⚠️ No valid commands found, using defaults (HEAT, no eco/powerful)");
      newIsCool = false;   // Default to HEAT
      newEcoMode = 0;      // Default to OFF
    }

    // Update state with new values (DO NOT change acPower - user controls it manually)
    isCool  = newIsCool;
    ecoMode = newEcoMode;
    // acPower remains whatever the user set - NOT changed by this code

    // Send IR regardless of whether values changed
    Serial.println("📡 Sending IR command...");
    sendAC();

    // LED FEEDBACK
    if (isCool) blinkLED(LED_GREEN, 1);
    else        blinkLED(LED_RED, 1);

    if (ecoMode == 1)      blinkLED(LED_GREEN, 2);
    else if (ecoMode == 2) blinkLED(LED_RED, 2);

  } else {
    Serial.println("❌ Firebase READ ERROR:");
    Serial.println(fbdo.errorReason());
    blinkLED(LED_RED, 3);
  }
}


// ===== PUSH SENSOR =====
void pushSensor() {
  if (!firebaseReady) {
    Serial.println("⚠️ pushSensor: Firebase not ready, attempting to refresh...");
    refreshFirebaseToken();
    if (!firebaseReady) return;
  }

  float humidity = dht.readHumidity();
  float tempC    = dht.readTemperature();

  if (isnan(humidity) || isnan(tempC)) {
    Serial.println("❌ DHT read failed");
    blinkLED(LED_RED, 2);
    return;
  }

  FirebaseJson json;
  json.set("temp", tempC);
  json.set("humidity", humidity);
  json.set("timestamp", millis());

  String path = "/sensor_logs/" + String(logIndex);

  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
    Serial.printf("📡 Sensor → %.1f°C | %.1f%% (Index: %d)\n", tempC, humidity, logIndex);
    logIndex++;
    Firebase.RTDB.setInt(&fbdo, "/sensor_logs_counter", logIndex);
    blinkLED(LED_GREEN, 1);
  } else {
    Serial.println("❌ Firebase WRITE ERROR:");
    Serial.println(fbdo.errorReason());
    
    // Check if error is token related
    String error = fbdo.errorReason();
    if (error.indexOf("token") >= 0 || error.indexOf("expired") >= 0) {
      Serial.println("Token expired, refreshing...");
      refreshFirebaseToken();
    }
    blinkLED(LED_RED, 3);
  }
}


// ===== RECONNECT =====
void reconnect() {
  Serial.println("\n🔄 Reconnecting WiFi & Firebase...");

  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi OK");
    
    // Refresh Firebase token after WiFi reconnect
    refreshFirebaseToken();
    
    blinkBothOnce();
  } else {
    Serial.println("\n❌ WiFi connection failed");
  }

  lastReconnect = millis();
}


// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("  AC Automation System Starting...");
  Serial.println("========================================\n");

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  ac.begin();
  dht.begin();

  // WiFi Connection
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Connection Failed!");
  }
  
  blinkBothOnce();

  // Firebase Setup
  Serial.println("\nConfiguring Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  Serial.println("Attempting Firebase sign-up...");
  if (Firebase.signUp(&config, &auth, "", "")) {
    firebaseReady = true;
    Serial.println("✅ Firebase Ready");
  } else {
    Serial.printf("❌ Firebase SignUp Failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize timers
  lastCommandFetch = millis();
  lastSensorPush   = millis();
  lastReconnect    = millis();
  lastTokenRefresh = millis();
  initialSensorSent = false;

  // Try to get current log index from Firebase
  if (firebaseReady) {
    if (Firebase.RTDB.getInt(&fbdo, "/sensor_logs_counter")) {
      logIndex = fbdo.intData();
      Serial.printf("Current log index: %d\n", logIndex);
    } else {
      logIndex = 0;
      Firebase.RTDB.setInt(&fbdo, "/sensor_logs_counter", 0);
    }
  }

  Serial.println("\n✅ System Ready!");
  Serial.println("========================================\n");
  Serial.println("NOTE: AC power is NOT controlled by this system.");
  Serial.println("      Use your remote to turn AC ON/OFF manually.");
  Serial.println("      This system only sets MODE, ECO, and POWERFUL.\n");
}


// ===== LOOP =====
void loop() {
  unsigned long currentMillis = millis();

  // Refresh token before it expires (every 59 minutes)
  if (firebaseReady && (currentMillis - lastTokenRefresh > TOKEN_REFRESH_INTERVAL)) {
    refreshFirebaseToken();
  }

  // Reconnect every 30 min if WiFi lost
  if (currentMillis - lastReconnect > RECONNECT_INTERVAL || WiFi.status() != WL_CONNECTED) {
    reconnect();
  }

  // Fetch commands and send IR every 10 minutes (ALWAYS sends IR)
  if (firebaseReady && (currentMillis - lastCommandFetch > COMMAND_INTERVAL)) {
    fetchAndSendCommands();
    lastCommandFetch = currentMillis;
  }

  // Send initial sensor reading immediately on first loop
  if (!initialSensorSent && firebaseReady) {
    delay(2000);  // Give DHT time to stabilize
    pushSensor();
    initialSensorSent = true;
    lastSensorPush = currentMillis;
  }
  
  // Push sensor every 5 minutes
  if (firebaseReady && (currentMillis - lastSensorPush > SENSOR_INTERVAL)) {
    pushSensor();
    lastSensorPush = currentMillis;
  }

  delay(100);
}