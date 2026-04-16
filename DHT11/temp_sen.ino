// DHT11 Temperature & Humidity Test
#include <DHT.h>

#define DHT_PIN  2
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();
  Serial.println("DHT11 ready! Reading every 2 seconds...");
}

void loop() {
  float humidity = dht.readHumidity();
  float tempC    = dht.readTemperature();
  float tempF    = dht.readTemperature(true);

  if (isnan(humidity) || isnan(tempC)) {
    Serial.println("ERROR: Failed to read sensor! Check wiring.");
    delay(2000);
    return;
  }

  Serial.println("--------------------");
  Serial.print("Temperature : ");
  Serial.print(tempC);
  Serial.print(" C  /  ");
  Serial.print(tempF);
  Serial.println(" F");

  Serial.print("Humidity    : ");
  Serial.print(humidity);
  Serial.println(" %");

  delay(2000);
}