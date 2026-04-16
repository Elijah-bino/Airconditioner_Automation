// AC Logger — Firebase Simulation v2
// With anonymous auth fix
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// ===== CONFIGURE THESE =====
#define WIFI_SSID     "NETGEAR61"
#define WIFI_PASSWORD "freshoctopus730"
#define API_KEY       "AIzaSyBlGQ2pO2BQPXaBqYTZUIqxiW0e5or5los"
#define DATABASE_URL  "https://ac-logger-bf8e2-default-rtdb.asia-southeast1.firebasedatabase.app/"
// ===========================

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool firebaseReady = false;
unsigned long logCount = 0;
unsigned long lastPush = 0;

String acProtocols[] = {"MITSUBISHI_HEAVY", "MITSUBISHI_HEAVY", "MITSUBISHI_HEAVY"};
String acCommands[]  = {"POWER_ON", "TEMP_UP", "MODE_COOL"};
float  acTemps[]     = {24.5, 25.0, 23.5};
float  acHumidities[]= {55.0, 57.0, 53.0};
int    simIndex      = 0;

void setup() {
  Serial.begin(9600);
  delay(500);

  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected! IP: ");
  Serial.println(WiFi.localIP());

  // Configure Firebase with anonymous auth
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  // Anonymous sign up — fixes token error
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Anonymous auth OK!");
    firebaseReady = true;
  } else {
    Serial.print("Auth failed: ");
    Serial.println(config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase ready!");
  Serial.println("Pushing simulated data every 10 seconds...");
}

void pushToFirebase(String protocol, String command, float temp, float humidity) {
  if (!firebaseReady) {
    Serial.println("Firebase not ready, skipping...");
    return;
  }

  String path = "/ac_logs/" + String(logCount);

  FirebaseJson json;
  json.set("protocol",  protocol);
  json.set("command",   command);
  json.set("temp_c",    temp);
  json.set("humidity",  humidity);
  json.set("timestamp", millis());

  Serial.println("--- Pushing to Firebase ---");
  Serial.print("Path    : "); Serial.println(path);
  Serial.print("Command : "); Serial.println(command);
  Serial.print("Temp    : "); Serial.print(temp); Serial.println(" C");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");

  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
    Serial.println("Pushed successfully!");
    logCount++;
  } else {
    Serial.print("Firebase error: ");
    Serial.println(fbdo.errorReason());
  }
}

void loop() {
  if (Firebase.ready() && (millis() - lastPush > 10000)) {
    pushToFirebase(
      acProtocols[simIndex % 3],
      acCommands[simIndex % 3],
      acTemps[simIndex % 3],
      acHumidities[simIndex % 3]
    );
    simIndex++;
    lastPush = millis();
  }
}